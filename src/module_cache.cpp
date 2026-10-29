#include "module_cache.hpp"
#include <unordered_map>

namespace mre {
namespace {
std::string moduleFromPath(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.size() < 6 || path.substr(path.size()-5) != ".lean") return {};
    path.resize(path.size()-5);
    if (path.rfind("./",0)==0) path.erase(0,2);
    if (path.empty() || path[0]=='/' || path.find(':')!=std::string::npos) return {};
    std::replace(path.begin(), path.end(), '/', '.');
    return path;
}
}
ModuleInventory scanOlean(const fs::path& root) {
    check(fs::is_directory(root), "OLEAn root is not a directory");
    ModuleInventory inventory;
    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
    for (; it!=end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec) || it->path().extension() != ".olean") continue;
        ++inventory.oleanFiles;
        auto relative = fs::relative(it->path(), root, ec);
        if (ec) { ec.clear(); continue; }
        relative.replace_extension(".lean");
        auto module = moduleFromPath(relative.generic_string());
        if (!module.empty()) inventory.available.insert(std::move(module));
    }
    return inventory;
}
int runModuleInventory(const fs::path& source, const fs::path& output,
                       const fs::path& oleanRoot, const std::string& prefix) {
    check(fs::is_regular_file(source), "Research database not found");
    check(fs::absolute(source) != fs::absolute(output), "Cannot modify research source database");
    const ModuleInventory inventory=scanOlean(oleanRoot);
    Db src(source,true), dst(output);
    dst.exec("CREATE TABLE IF NOT EXISTS module_inventory(module TEXT PRIMARY KEY, available INTEGER NOT NULL, source_file TEXT NOT NULL, checked_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)");
    Stmt query(src,"SELECT DISTINCT file FROM declarations");
    Stmt put(dst,"INSERT INTO module_inventory(module,available,source_file,checked_at) VALUES(?1,?2,?3,CURRENT_TIMESTAMP) ON CONFLICT(module) DO UPDATE SET available=excluded.available,source_file=excluded.source_file,checked_at=CURRENT_TIMESTAMP");
    uint64_t modules=0, ready=0;
    dst.beginTransaction();
    try {
        while(query.step()) {
            const std::string f=query.getText(0), module=moduleFromPath(f);
            if(module.empty() || (!prefix.empty() && module.rfind(prefix,0)!=0)) continue;
            const bool present=inventory.available.contains(module);
            put.bind(1,module); put.bind(2,present?1:0); put.bind(3,f);
            put.step(); put.reset();
            ++modules; if(present) ++ready;
        }
        dst.commit();
    } catch (...) { dst.rollback(); throw; }
    std::cout << "MODULE_INVENTORY_DONE source_modules=" << modules << " available=" << ready
              << " unavailable=" << (modules-ready) << " olean_files=" << inventory.oleanFiles
              << " output=" << utf8(output) << '\n';
    std::cout << "MODULE_INVENTORY_NOTE: .olean presence is only a prerequisite; transitive imports and Lean version must also match.\n";
    return 0;
}
int runSemanticReport(const fs::path& output) {
    check(fs::is_regular_file(output),"Result database not found");
    Db db(output,true);
    std::cout << "SEMANTIC_REPORT " << utf8(output) << '\n';
    if (db.count("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='module_inventory'") != 0) {
        Stmt modules(db,"SELECT available,COUNT(*) FROM module_inventory GROUP BY available ORDER BY available DESC");
        while(modules.step()) std::cout << "modules_available=" << modules.getInt64(0) << " count=" << modules.getInt64(1) << '\n';
    }
    if (db.count("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='semantic_probe_results'") != 0) {
        Stmt probes(db,"SELECT status,COUNT(*) FROM semantic_probe_results GROUP BY status ORDER BY status");
        while(probes.step()) std::cout << "probe_status=" << probes.getText(0) << " count=" << probes.getInt64(1) << '\n';
    }
    return 0;
}
}

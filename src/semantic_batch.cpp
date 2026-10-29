#include "semantic_batch.hpp"
#include <random>
#include <set>
#include <tuple>
#include <cctype>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace mre {
namespace {
struct Decl { std::string module, name, kind, path; int64_t line = 0; };

bool validIdentifier(const std::string& s) {
    if (s.empty() || s.size() > 240) return false;
    for (unsigned char c : s)
        if (!(std::isalnum(c) || c == '_' || c == '.' || c == '\'')) return false;
    return s.front() != '.' && s.back() != '.';
}
std::string moduleFromFile(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    if (s.size() < 6 || s.substr(s.size() - 5) != ".lean") return {};
    s.resize(s.size() - 5);
    if (s.rfind("./", 0) == 0) s.erase(0, 2);
    if (s.find(":") != std::string::npos || s[0] == '/') return {};
    std::replace(s.begin(), s.end(), '/', '.');
    return validIdentifier(s) ? s : std::string{};
}
std::vector<Decl> sample(Db& db, int count, uint64_t seed, const std::string& prefix, const std::unordered_set<std::string>* ready) {
    std::vector<Decl> result;
    result.reserve(static_cast<size_t>(count));
    std::mt19937_64 rng(seed);
    Stmt read(db, "SELECT file,line,kind,name FROM declarations WHERE kind IN ('def','theorem','lemma')");
    uint64_t eligible = 0;
    while (read.step()) {
        Decl d;
        d.path = read.getText(0); d.line = read.getInt64(1);
        d.kind = read.getText(2); d.name = read.getText(3);
        d.module = moduleFromFile(d.path);
        if (!validIdentifier(d.name) || d.module.empty()) continue;
        if (!prefix.empty() && d.module.rfind(prefix, 0) != 0) continue;
        if (ready != nullptr && !ready->contains(d.module)) continue;
        // Avoid certificate-heavy families; selection remains auditable and reproducible.
        if (d.module.find("Certificate") != std::string::npos ||
            d.module.find("Construction.Certificates") != std::string::npos ||
            d.name.find("decode_entry") != std::string::npos) continue;
        ++eligible;
        if (result.size() < static_cast<size_t>(count)) result.push_back(std::move(d));
        else {
            std::uniform_int_distribution<uint64_t> dist(0, eligible - 1);
            const uint64_t j = dist(rng);
            if (j < result.size()) result[static_cast<size_t>(j)] = std::move(d);
        }
    }
    return result;
}
// Resolve the source declaration in its actual Lean namespace, never the import path.
// This is a conservative source-location resolver: ambiguous or unverified records
// remain diagnostic failures rather than being labelled mathematically incompatible.
struct ResolvedName { std::string full; std::string diagnostic; };
ResolvedName resolveSourceName(const Decl& d, const fs::path& sourceRoot) {
    if (sourceRoot.empty()) return {d.name, "source_root_not_configured"};
    fs::path file = sourceRoot / fs::path(d.path);
    if (!fs::is_regular_file(file)) return {d.name, "source_file_missing"};
    std::ifstream in(file, std::ios::binary);
    if (!in) return {d.name, "source_file_unreadable"};
    std::vector<std::string> spaces;
    std::string line;
    int64_t number = 0;
    bool found = false;
    std::string full;
    auto trim = [](std::string s) {
        const auto first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return std::string{};
        const auto last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, last - first + 1);
    };
    while (std::getline(in, line) && number < d.line) {
        ++number;
        const std::string t = trim(line);
        if (t.rfind("namespace ", 0) == 0) {
            std::string name = trim(t.substr(10));
            const auto end = name.find_first_of(" \t/-");
            if (end != std::string::npos) name.resize(end);
            if (validIdentifier(name)) spaces.push_back(name);
        } else if (t == "end" || t.rfind("end ", 0) == 0) {
            if (!spaces.empty()) spaces.pop_back();
        }
        if (number == d.line) {
            // Confirm that the indexed line actually defines the indexed name.
            // An index mismatch must not silently produce a fabricated declaration.
            const std::string needle = d.name;
            if (t.find(needle) == std::string::npos) return {d.name, "source_line_name_mismatch"};
            std::string prefix;
            for (const auto& ns : spaces) prefix += ns + ".";
            full = d.name.find('.') != std::string::npos ? d.name : prefix + d.name;
            found = true;
        }
    }
    if (!found || !validIdentifier(full)) return {d.name, "source_location_unresolved"};
    return {full, "source_namespace_resolved"};
}

// Two JSON records are emitted by Lean (a -> b, b -> a).
// Do not mistake missing declarations or malformed stdout for a negative type judgement.
std::string classify(const ProcessResult& p) {
    if (p.timed_out) return "RESOURCE_LIMIT";
    if (p.exit_code != 0) return "INFRASTRUCTURE_ERROR";
    if (p.output.find("source_a_not_found") != std::string::npos ||
        p.output.find("source_b_not_found") != std::string::npos ||
        p.output.find("lean_meta_exception_during_application") != std::string::npos)
        return "PROBE_ERROR";
    const bool positive = p.output.find("\"type_correct\":true") != std::string::npos;
    const bool negative = p.output.find("\"type_correct\":false") != std::string::npos;
    if (positive) return "TYPE_CORRECT";
    if (negative) return "TYPE_INCOMPATIBLE";
    return "MALFORMED_OUTPUT";
}

std::vector<std::string> nameTokens(const std::string& name) {
    std::vector<std::string> tokens;
    std::string current;
    for (unsigned char c : name) {
        if (std::isalnum(c)) current += static_cast<char>(std::tolower(c));
        else if (current.size() > 2) { tokens.push_back(current); current.clear(); }
        else current.clear();
    }
    if (current.size() > 2) tokens.push_back(current);
    return tokens;
}
// Reproducible HEURISTIC ranking only. Real type compatibility is decided by Lean MetaM.
int candidateScore(const Decl& a, const Decl& b) {
    int score = (a.module == b.module ? 12 : 0);
    const auto posA = a.module.rfind('.');
    const auto posB = b.module.rfind('.');
    if (posA != std::string::npos && posB != std::string::npos &&
        a.module.substr(0, posA) == b.module.substr(0, posB)) score += 3;
    const auto ta = nameTokens(a.name);
    const auto tb = nameTokens(b.name);
    for (const auto& x : ta)
        for (const auto& y : tb)
            if (x == y) score += 4;
    if ((a.kind == "lemma" || a.kind == "theorem") && b.kind == "def") score += 2;
    if ((b.kind == "lemma" || b.kind == "theorem") && a.kind == "def") score += 2;
    return score;
}
}

int runSemanticBatch(const fs::path& source, const fs::path& output,
                     const fs::path& script, const fs::path& lean,
                     int limit, uint64_t seed, const std::string& prefix,
                     uint32_t timeoutMs) {
    check(limit > 0 && limit <= 100000, "Pair count must be in range 1..100000");
    check(timeoutMs >= 1000 && timeoutMs <= 3600000, "Timeout must be 1000..3600000 ms");
    check(fs::is_regular_file(source), "Source database not found");
    check(fs::is_regular_file(script), "Lean synthesis script not found");
    check(fs::is_regular_file(lean), "Lean executable not found");
    check(fs::absolute(source) != fs::absolute(output), "Source and output databases must be separate");
    Db src(source, true);
    Db dst(output);
    dst.exec("CREATE TABLE IF NOT EXISTS semantic_probe_results("
             "id INTEGER PRIMARY KEY, seed INTEGER NOT NULL, module_a TEXT NOT NULL,"
             "name_a TEXT NOT NULL, module_b TEXT NOT NULL, name_b TEXT NOT NULL,"
             "source_a TEXT NOT NULL, source_b TEXT NOT NULL,"
             "status TEXT NOT NULL, lean_output TEXT NOT NULL,"
             "lean_exit INTEGER NOT NULL, elapsed_ms INTEGER NOT NULL,"
             "created_at TEXT DEFAULT CURRENT_TIMESTAMP)");
    dst.exec("CREATE INDEX IF NOT EXISTS idx_semantic_probe_status ON semantic_probe_results(status)");
    dst.exec("CREATE TABLE IF NOT EXISTS semantic_probe_ranking("
             "probe_id INTEGER PRIMARY KEY, strategy TEXT NOT NULL, score INTEGER NOT NULL,"
             "run_tag TEXT NOT NULL, FOREIGN KEY(probe_id) REFERENCES semantic_probe_results(id))");
    // Once a module inventory exists, it is authoritative for selection.
    // Missing Lean modules must not cause thousands of redundant subprocess failures.
    std::unordered_set<std::string> ready;
    const bool hasInventory = dst.count("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='module_inventory'") != 0;
    if (hasInventory) {
        Stmt modules(dst, "SELECT module FROM module_inventory WHERE available=1");
        while (modules.step()) ready.insert(modules.getText(0));
        if (ready.empty()) {
            std::cerr << "SEMANTIC_BATCH_BLOCKED: no compiled source modules in cached inventory; "
                         "prepare Lean dependencies before running probes. Database untouched.\n";
            return 6;
        }
    }
    auto decls = sample(src, std::max(2, std::min(512, limit * 16)), seed, prefix, hasInventory ? &ready : nullptr);
    if (decls.size() < 2) {
        std::cerr << "SEMANTIC_BATCH_NO_SOURCES: insufficient matching declarations\n";
        return 4;
    }
    std::mt19937_64 rng(seed);
    std::shuffle(decls.begin(), decls.end(), rng);
    fs::path sourceRoot;
#ifdef _WIN32
    char* envValue = nullptr;
    size_t envLength = 0;
    const errno_t envResult = _dupenv_s(&envValue, &envLength, "MRE_OAI_SOURCE_ROOT");
    if (envResult == 0 && envValue != nullptr) sourceRoot = fs::path(envValue);
    free(envValue);
#else
    if (const char* value = std::getenv("MRE_OAI_SOURCE_ROOT")) sourceRoot = fs::path(value);
#endif
    // Rank candidate pairs by lexical/domain affinity, preserving seeded tie-breaking.
    struct Pair { size_t a, b; int score; };
    std::vector<Pair> ranked;
    const size_t pool = decls.size();
    for (size_t i = 0; i < pool; ++i)
        for (size_t j = i + 1; j < pool; ++j) {
            ranked.push_back({i, j, candidateScore(decls[i], decls[j])});
            ranked.push_back({j, i, candidateScore(decls[j], decls[i])});
        }
    std::stable_sort(ranked.begin(), ranked.end(), [](const Pair& a, const Pair& b) {
        return a.score > b.score;
    });
    Stmt insert(dst, "INSERT INTO semantic_probe_results(seed,module_a,name_a,module_b,name_b,"
                     "source_a,source_b,status,lean_output,lean_exit,elapsed_ms)"
                     " VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)");
    int completed = 0, positive = 0, failed = 0;
    std::set<std::pair<std::string, std::string>> done;
    dst.exec("CREATE TABLE IF NOT EXISTS semantic_probe_identity("
             "probe_id INTEGER PRIMARY KEY, qualified_a TEXT NOT NULL, qualified_b TEXT NOT NULL,"
             "resolution_a TEXT NOT NULL, resolution_b TEXT NOT NULL)");
    Stmt addIdentity(dst, "INSERT INTO semantic_probe_identity(probe_id,qualified_a,qualified_b,resolution_a,resolution_b)"
                          " VALUES(last_insert_rowid(),?1,?2,?3,?4)");
    Stmt addRank(dst, "INSERT INTO semantic_probe_ranking(probe_id,strategy,score,run_tag)"
                      " VALUES(last_insert_rowid(),'affinity_v2',?1,?2)");
    const std::string runTag = std::to_string(seed) + ":" + std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());
    const auto begin = std::chrono::steady_clock::now();
    // Rotating anchors prevent a single declaration from dominating the entire batch.
    for (const Pair& pair : ranked) {
            if (completed >= limit) break;
            const Decl& a = decls[pair.a]; const Decl& b = decls[pair.b];
            if (a.module == b.module && a.name == b.name) continue;
            if (!done.insert({a.module + ':' + a.name, b.module + ':' + b.name}).second) continue;
            const ResolvedName qa = resolveSourceName(a, sourceRoot);
            const ResolvedName qb = resolveSourceName(b, sourceRoot);
            const auto started = std::chrono::steady_clock::now();
            const std::string command = shellQuote(utf8(lean)) + " --run " + shellQuote(utf8(script)) +
                " " + shellQuote(a.module) + " " + shellQuote(qa.full) +
                " " + shellQuote(b.module) + " " + shellQuote(qb.full);
            ProcessResult p = runProcess(command, timeoutMs);
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count();
            const std::string status = (sourceRoot.empty() ||
                (qa.diagnostic == "source_namespace_resolved" && qb.diagnostic == "source_namespace_resolved"))
                ? classify(p) : "NAME_RESOLUTION_ERROR";
            insert.bind(1, static_cast<uint64_t>(seed));
            insert.bind(2, a.module); insert.bind(3, a.name);
            insert.bind(4, b.module); insert.bind(5, b.name);
            const std::string sourceA = a.path + ":" + std::to_string(a.line);
            const std::string sourceB = b.path + ":" + std::to_string(b.line);
            insert.bind(6, sourceA);
            insert.bind(7, sourceB);
            insert.bind(8, status); insert.bind(9, p.output);
            insert.bind(10, p.exit_code); insert.bind(11, static_cast<int64_t>(ms));
            insert.step(); insert.reset();
            addIdentity.bind(1, qa.full); addIdentity.bind(2, qb.full);
            addIdentity.bind(3, qa.diagnostic); addIdentity.bind(4, qb.diagnostic);
            addIdentity.step(); addIdentity.reset();
            addRank.bind(1, pair.score);
            addRank.bind(2, runTag);
            addRank.step(); addRank.reset();
            ++completed;
            if (status == "TYPE_CORRECT") ++positive;
            if (status == "INFRASTRUCTURE_ERROR") ++failed;
            std::cout << "PROBE " << completed << '/' << limit << " " << status
                      << " " << a.module << ':' << a.name << " -> " << b.module << ':' << b.name << '\n';
    }
    const auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - begin).count();
    std::cout << "SEMANTIC_BATCH_DONE completed=" << completed << " type_correct=" << positive
              << " infrastructure_errors=" << failed << " selection=affinity_v2 elapsed_ms=" << totalMs
              << " formal_proofs=0\n";
    return completed == limit ? 0 : 5;
}
} // namespace mre

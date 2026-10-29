#include "type_graph.hpp"
#include <random>
#include <cstdlib>
#include <filesystem>
#include <unordered_set>
#include <iostream>

namespace mre {

TypeGraph::TypeGraph() {
    domain_weights_["Algebra"] = 2.0;
    domain_weights_["Analysis"] = 2.0;
    domain_weights_["Topology"] = 1.5;
    domain_weights_["NumberTheory"] = 1.5;
    domain_weights_["LinearAlgebra"] = 1.5;
    domain_weights_["CategoryTheory"] = 1.2;
    domain_weights_["GroupTheory"] = 1.5;
    domain_weights_["RingTheory"] = 1.5;
    domain_weights_["Geometry"] = 1.2;
    domain_weights_["Computability"] = 1.0;
    domain_weights_["Combinatorics"] = 0.5; // prevent Ramsey dominating
    domain_weights_["General"] = 1.0;
}

void TypeGraph::setDomainWeight(const std::string& domain, double weight) {
    domain_weights_[domain] = weight;
}

double TypeGraph::getDomainWeight(const std::string& domain) const {
    auto it = domain_weights_.find(domain);
    if (it != domain_weights_.end()) return it->second;
    return 1.0;
}

static std::string moduleOfPath(const std::string& path) {
    std::string s = path;
    for (char& c : s) if (c == '\\' || c == '/') c = '.';
    if (s.size() > 5 && s.substr(s.size() - 5) == ".lean") {
        s.resize(s.size() - 5);
    }
    return s;
}

void TypeGraph::buildImportGraph(Db& source_db, Db& discovery_db, bool verbose) {
    initializeOrMigrateDiscoveryDb(discovery_db);

    Stmt read(source_db, "SELECT file, module FROM imports");
    Stmt write_legacy(discovery_db, "INSERT INTO graph_edges(source, target, weight) VALUES(?1, ?2, 1) "
                                    "ON CONFLICT(source, target) DO UPDATE SET weight = weight + 1");
    Stmt write_tge(discovery_db, "INSERT INTO type_graph_edges(source, target, kind, weight, metadata) "
                                 "VALUES(?1, ?2, 'import', 1, '') "
                                 "ON CONFLICT(source, target, kind) DO UPDATE SET weight = weight + 1");

    discovery_db.beginTransaction();
    try {
        discovery_db.exec("DELETE FROM graph_edges;");
        discovery_db.exec("DELETE FROM type_graph_edges WHERE kind = 'import';");

        int64_t count = 0;
        while (read.step()) {
            std::string from = moduleOfPath(read.getText(0));
            std::string to = read.getText(1);

            write_legacy.bind(1, from);
            write_legacy.bind(2, to);
            write_legacy.step();
            write_legacy.reset();

            write_tge.bind(1, from);
            write_tge.bind(2, to);
            write_tge.step();
            write_tge.reset();

            ++count;
            if (verbose && count % 25000 == 0) {
                std::cout << "Import graph edges processed: " << count << "\n";
            }
        }
        discovery_db.commit();
        if (verbose) {
            std::cout << "Import graph built successfully. Total imports: " << count << "\n";
        }
    } catch (...) {
        discovery_db.rollback();
        throw;
    }
}

void TypeGraph::buildSemanticTypeGraph(const std::vector<SemanticDeclaration>& decls, Db& discovery_db, bool verbose) {
    initializeOrMigrateDiscoveryDb(discovery_db);

    Stmt write(discovery_db, "INSERT INTO type_graph_edges(source, target, kind, weight, metadata) "
                             "VALUES(?1, ?2, ?3, 1, ?4) "
                             "ON CONFLICT(source, target, kind) DO UPDATE SET weight = weight + 1");

    discovery_db.beginTransaction();
    try {
        int64_t count = 0;
        for (size_t i = 0; i < decls.size(); ++i) {
            for (size_t j = 0; j < decls.size(); ++j) {
                if (i == j) continue;
                const auto& d1 = decls[i];
                const auto& d2 = decls[j];

                if (areTypesCompatibleForComposition(d1, d2)) {
                    write.bind(1, d1.name);
                    write.bind(2, d2.name);
                    write.bind(3, std::string("comp_compatible"));
                    write.bind(4, d1.getCodomainType() + " -> " + d2.getDomainType());
                    write.step();
                    write.reset();
                    ++count;
                }

                if (d1.domain != d2.domain && d1.domain != "General" && d2.domain != "General") {
                    write.bind(1, d1.domain);
                    write.bind(2, d2.domain);
                    write.bind(3, std::string("domain_bridge"));
                    write.bind(4, d1.name + " <-> " + d2.name);
                    write.step();
                    write.reset();
                    ++count;
                }
            }
        }
        discovery_db.commit();
        if (verbose) {
            std::cout << "Semantic type graph edges inserted: " << count << "\n";
        }
    } catch (...) {
        discovery_db.rollback();
        throw;
    }
}

// Inventory only existing compiled modules. This does not invoke Lean or build anything.
// Restricting the SQL query before sampling prevents 48 impossible candidates.
static const std::vector<std::string>& importableFiles() {
    static const std::vector<std::string> files = [] {
        std::vector<std::string> out;
        const char* env = std::getenv("LEAN_PATH");
        if (!env || !*env) return out;
        std::unordered_set<std::string> unique;
#ifdef _WIN32
        constexpr char separator = ';';
#else
        constexpr char separator = ':';
#endif
        std::istringstream paths(env);
        std::string root;
        while (std::getline(paths, root, separator)) {
            if (root.empty()) continue;
            std::error_code ec;
            if (!std::filesystem::is_directory(root, ec)) continue;
            std::filesystem::recursive_directory_iterator it(root,
                std::filesystem::directory_options::skip_permission_denied, ec), end;
            while (it != end) {
                const auto path = it->path();
                if (it->is_regular_file(ec) && path.extension() == ".olean") {
                    auto relative = path.lexically_relative(root);
                    relative.replace_extension(".lean");
                    const auto name_fwd = relative.generic_string();
                    auto name_back = name_fwd;
                    std::replace(name_back.begin(), name_back.end(), '/', '\\');
                    if (unique.insert(name_fwd).second) out.push_back(name_fwd);
                    if (unique.insert(name_back).second) out.push_back(name_back);
                }
                it.increment(ec);
                if (ec) ec.clear();
            }
        }
        return out;
    }();
    return files;
}

std::vector<SemanticDeclaration> TypeGraph::sampleDeclarations(
    Db& source_db,
    int count,
    uint64_t seed,
    const std::string& domain_filter,
    bool exclude_certs
) {
    std::vector<SemanticDeclaration> reservoir;
    reservoir.reserve(count);
    std::mt19937_64 rng(seed);

    const bool importable_only = std::getenv("MRE_IMPORTABLE_ONLY") != nullptr;
    std::string sql = "SELECT file, line, kind, name, statement FROM declarations "
                      "WHERE kind IN ('theorem', 'lemma', 'def')";
    if (importable_only) {
        const auto& files = importableFiles();
        if (files.empty()) throw std::runtime_error("MRE_IMPORTABLE_ONLY: no compiled .olean modules found in LEAN_PATH");
        // The source database is opened SQLITE_OPEN_READONLY. Even CREATE TEMP TABLE
        // may fail on that connection; construct a read-only membership predicate.
        // Module names come from filesystem paths, not from the research database.
        sql += " AND file IN (";
        bool first = true;
        for (const auto& file : files) {
            if (!first) sql += ",";
            first = false;
            sql += "'";
            for (const char ch : file) {
                if (ch == '\'') sql += "''";
                else sql += ch;
            }
            sql += "'";
        }
        sql += ")";
        std::cout << "MRE importable module inventory=" << files.size()
                  << " (read-only source; existing .olean only)\n";

    }

    Stmt s(source_db, sql);
    uint64_t seen = 0;
    uint64_t max_scan = std::max<uint64_t>(20000, static_cast<uint64_t>(count * 250));

    while (s.step()) {
        std::string file = s.getText(0);
        int64_t line = s.getInt64(1);
        std::string kind = s.getText(2);
        std::string name = s.getText(3);
        std::string statement = s.getText(4);

        if (name.empty()) continue;

        // Certificate filter
        if (exclude_certs && isCertificateFile(file, name)) {
            continue;
        }

        std::string domain = extractDomain(file);
        if (!domain_filter.empty() && domain != domain_filter) {
            continue;
        }

        double weight = getDomainWeight(domain);
        if (weight <= 0.0) continue;

        SemanticDeclaration decl = extractSemanticFromRawStatement(file, line, kind, name, statement);

        ++seen;
        if (static_cast<int>(reservoir.size()) < count) {
            reservoir.push_back(std::move(decl));
        } else {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            if (dist(rng) < (static_cast<double>(count) / static_cast<double>(seen)) * weight) {
                std::uniform_int_distribution<size_t> idx_dist(0, reservoir.size() - 1);
                reservoir[idx_dist(rng)] = std::move(decl);
            }
        }

        if (seen >= max_scan) {
            break;
        }
    }

    return reservoir;
}

std::vector<std::pair<SemanticDeclaration, SemanticDeclaration>> TypeGraph::sampleCrossDomainPairs(
    Db& source_db,
    int count,
    uint64_t seed,
    const std::string& domain_a,
    const std::string& domain_b
) {
    auto list_a = sampleDeclarations(source_db, count * 2, seed, domain_a, true);
    auto list_b = sampleDeclarations(source_db, count * 2, seed + 9999, domain_b, true);

    std::vector<std::pair<SemanticDeclaration, SemanticDeclaration>> pairs;
    if (list_a.empty() || list_b.empty()) return pairs;

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<size_t> dist_a(0, list_a.size() - 1);
    std::uniform_int_distribution<size_t> dist_b(0, list_b.size() - 1);

    for (int i = 0; i < count; ++i) {
        pairs.emplace_back(list_a[dist_a(rng)], list_b[dist_b(rng)]);
    }
    return pairs;
}

} // namespace mre

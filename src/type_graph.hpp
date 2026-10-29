#pragma once

#include "database.hpp"
#include "semantic_types.hpp"
#include <map>
#include <unordered_map>
#include <set>

namespace mre {

struct GraphEdge {
    std::string source;
    std::string target;
    std::string kind; // "import", "comp_compatible", "domain_bridge"
    int64_t weight = 1;
    std::string metadata;
};

class TypeGraph {
    std::map<std::string, double> domain_weights_;
    bool exclude_certificates_ = true;

public:
    TypeGraph();

    void setDomainWeight(const std::string& domain, double weight);
    double getDomainWeight(const std::string& domain) const;
    void setExcludeCertificates(bool exclude) { exclude_certificates_ = exclude; }
    bool isExcludeCertificates() const { return exclude_certificates_; }

    // Build module import graph from source DB into discovery DB
    void buildImportGraph(Db& source_db, Db& discovery_db, bool verbose = true);

    // Build mathematical type compatibility edges between declarations
    void buildSemanticTypeGraph(const std::vector<SemanticDeclaration>& decls, Db& discovery_db, bool verbose = true);

    // Weighted sampling of declarations respecting domain weights & certificate exclusion
    std::vector<SemanticDeclaration> sampleDeclarations(
        Db& source_db,
        int count,
        uint64_t seed,
        const std::string& domain_filter = "",
        bool exclude_certs = true
    );

    // Cross-domain paired sampling: pairs declaration from domain A with domain B
    std::vector<std::pair<SemanticDeclaration, SemanticDeclaration>> sampleCrossDomainPairs(
        Db& source_db,
        int count,
        uint64_t seed,
        const std::string& domain_a,
        const std::string& domain_b
    );
};

} // namespace mre

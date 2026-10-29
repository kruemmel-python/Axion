#pragma once

#include "semantic_types.hpp"
#include "type_graph.hpp"
#include <string>
#include <vector>

namespace mre {

enum class GenerationStrategy {
    TypeSynthesis,
    Systematic,
    Random,
    CrossDomain,
    Mutation,
    Iterative
};

inline std::string strategyToString(GenerationStrategy s) {
    switch (s) {
        case GenerationStrategy::TypeSynthesis: return "type_synthesis";
        case GenerationStrategy::Systematic: return "systematic";
        case GenerationStrategy::Random: return "random";
        case GenerationStrategy::CrossDomain: return "cross_domain";
        case GenerationStrategy::Mutation: return "mutation";
        case GenerationStrategy::Iterative: return "iterative";
    }
    return "unknown";
}

inline GenerationStrategy stringToStrategy(const std::string& str) {
    if (str == "type_synthesis") return GenerationStrategy::TypeSynthesis;
    if (str == "systematic") return GenerationStrategy::Systematic;
    if (str == "random") return GenerationStrategy::Random;
    if (str == "cross_domain") return GenerationStrategy::CrossDomain;
    if (str == "mutation") return GenerationStrategy::Mutation;
    if (str == "iterative") return GenerationStrategy::Iterative;
    return GenerationStrategy::Random;
}

struct Candidate {
    int64_t id = 0;
    uint64_t seed = 0;
    int ordinal = 0;
    std::string strategy;
    std::string mode;
    std::string status = "GENERATED";
    std::string source_a;
    std::string source_b;
    std::string file_a;
    std::string file_b;
    std::string type_sig_a;
    std::string type_sig_b;
    std::string hypothesis_code;
    std::string proof_code;
    std::string premises_status = "NONE";
    std::string unresolved_premises;
    std::string counterexample;
    std::string details;
    std::string error_log;
    int verification_exit_code = -1;
    uint32_t verification_duration_ms = 0;
    int64_t parent_id = 0;
    int chain_depth = 0;
    std::vector<std::string> imports;
};

class HypothesisGenerator {
    TypeGraph& type_graph_;

public:
    explicit HypothesisGenerator(TypeGraph& tg) : type_graph_(tg) {}

    // Generate candidates using the specified strategy
    std::vector<Candidate> generate(
        Db& source_db,
        int count,
        uint64_t seed,
        GenerationStrategy strategy,
        const std::string& domain_filter = "",
        bool exclude_certs = true,
        bool k4d_guided = false
    );

    // Save candidates to discovery database
    void saveCandidates(Db& discovery_db, const std::vector<Candidate>& candidates);
};

} // namespace mre

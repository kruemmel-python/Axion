#pragma once

#include "database.hpp"
#include "type_graph.hpp"
#include "generator.hpp"
#include "premise_resolver.hpp"
#include "numeric_evaluator.hpp"
#include "verifier.hpp"

namespace mre {

struct ExperimentSummary {
    int64_t session_id = 0;
    std::string session_name;
    int total_candidates = 0;
    int formally_proven = 0;
    int numeric_counterexamples = 0;
    int numeric_survived = 0;
    int proof_failed = 0;
    int premises_unresolved = 0;
    int elaboration_failed = 0;
    int already_known = 0;
    uint32_t duration_ms = 0;
};

struct ChainProgressStep {
    int depth = 0;
    int64_t candidate_id = 0;
    int64_t parent_id = 0;
    std::string theorem_name;
    std::string status;
    double stability_score = 0.0;
    uint32_t duration_ms = 0;
};

struct ResearchChainSummary {
    int target_depth = 0;
    int achieved_depth = 0;
    int total_attempts = 0;
    int total_proven = 0;
    std::vector<ChainProgressStep> steps;
    uint32_t total_duration_ms = 0;
};

struct ResearchNetworkSummary {
    int target_depth = 0;
    int max_depth_achieved = 0;
    int branching_factor = 2;
    int total_nodes_explored = 0;
    int total_proven_nodes = 0;
    int active_frontier_size = 0;
    std::vector<ChainProgressStep> steps;
    uint32_t total_duration_ms = 0;
};

class ExperimentRunner {
    TypeGraph type_graph_;
    HypothesisGenerator generator_;
    NumericEvaluator numeric_evaluator_;
    Verifier verifier_;

public:
    explicit ExperimentRunner(const fs::path& lean_bin = "");

    // Runs a full 12-stage automated research cycle
    ExperimentSummary runCycle(
        Db& source_db,
        Db& discovery_db,
        const fs::path& lean_output_dir,
        int count,
        uint64_t seed,
        GenerationStrategy strategy,
        const std::string& domain_filter = "",
        bool exclude_certs = true,
        bool k4d_guided = false
    );

    // Runs a continuous research chain: proven theorems become the foundation for deeper hypotheses
    ResearchChainSummary runResearchChain(
        Db& source_db,
        Db& discovery_db,
        const fs::path& lean_output_dir,
        int max_depth,
        uint64_t seed,
        GenerationStrategy strategy,
        int64_t start_from_candidate_id = 0,
        bool k4d_guided = true
    );

    // Runs a branching research network: proven theorems spawn multiple children forming an evolving proof tree/network
    ResearchNetworkSummary runResearchNetwork(
        Db& source_db,
        Db& discovery_db,
        const fs::path& lean_output_dir,
        int max_depth,
        int branching_factor,
        uint64_t seed,
        GenerationStrategy strategy,
        int64_t start_from_candidate_id = 0,
        bool k4d_guided = true
    );
};

} // namespace mre

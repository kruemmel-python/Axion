#pragma once

#include "semantic_types.hpp"
#include "../k4d/k4d.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace mre {

enum class HypothesisRelation {
    MorphismComposition,     // f ∘ g (Transitive composition of homomorphisms)
    EndomorphismCommutation, // f(g(x)) = g(f(x)) (Commutativity of endomorphisms)
    InvariantKernel,         // D(v) = 0 (Vanishing in invariant kernel)
    PropositionImplication,  // P → Q (Logical deduction)
    Equivalence,             // P ↔ Q (Logical equivalence)
    DirectCheck              // Component type check
};

inline std::string hypothesisRelationToString(HypothesisRelation r) {
    switch (r) {
        case HypothesisRelation::MorphismComposition: return "MorphismComposition";
        case HypothesisRelation::EndomorphismCommutation: return "EndomorphismCommutation";
        case HypothesisRelation::InvariantKernel: return "InvariantKernel";
        case HypothesisRelation::PropositionImplication: return "PropositionImplication";
        case HypothesisRelation::Equivalence: return "Equivalence";
        case HypothesisRelation::DirectCheck: return "DirectCheck";
    }
    return "Unknown";
}

struct EvolvedHypothesis {
    HypothesisRelation relation = HypothesisRelation::DirectCheck;
    SemanticDeclaration root_a;
    SemanticDeclaration root_b;
    std::vector<SemanticDeclaration> chain;
    std::string theorem_name;
    std::string theorem_statement;
    std::string proof_script;
    std::string check_script;
    int evolution_steps = 0;
    double stability_score = 1.0;
    bool is_k4d_guided = false;
    std::string provenance;
    std::vector<std::string> imports;
    int64_t parent_id = 0;
    int chain_depth = 0;
    std::string parent_theorem_name;
};

class HypothesisEvolutionEngine {
public:
    static EvolvedHypothesis evolve(
        const std::vector<SemanticDeclaration>& eligible_pool,
        size_t index_a,
        size_t index_b,
        uint64_t seed,
        int max_steps,
        bool use_k4d_dynamics
    );

    static EvolvedHypothesis evolveChained(
        const std::vector<SemanticDeclaration>& eligible_pool,
        int64_t parent_id,
        int parent_depth,
        const std::string& parent_theorem_code,
        const std::string& parent_theorem_name,
        const std::vector<std::string>& parent_imports,
        size_t partner_index,
        uint64_t seed,
        int max_steps,
        bool use_k4d_dynamics
    );
};

} // namespace mre

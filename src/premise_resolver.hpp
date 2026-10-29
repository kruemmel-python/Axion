#pragma once

#include "semantic_types.hpp"

namespace mre {

enum class PremiseStatus {
    None,
    ResolvedTrivial,
    ResolvedArithmetic,
    ResolvedHypothesis,
    Unresolved
};

inline std::string premiseStatusToString(PremiseStatus status) {
    switch (status) {
        case PremiseStatus::None: return "NONE";
        case PremiseStatus::ResolvedTrivial: return "RESOLVED_TRIVIAL";
        case PremiseStatus::ResolvedArithmetic: return "RESOLVED_ARITHMETIC";
        case PremiseStatus::ResolvedHypothesis: return "RESOLVED_HYPOTHESIS";
        case PremiseStatus::Unresolved: return "PREMISES_UNRESOLVED";
    }
    return "UNKNOWN";
}

struct PremiseResolutionResult {
    PremiseStatus status = PremiseStatus::None;
    std::string discharge_tactic;
    std::vector<std::string> unresolved_premises;
    std::string explanation;
};

class PremiseResolver {
public:
    PremiseResolver() = default;

    // Check whether required premises can be discharged or need explicit tracking
    PremiseResolutionResult resolve(const std::vector<std::string>& premises, const std::vector<DeclParam>& context_params);
};

} // namespace mre

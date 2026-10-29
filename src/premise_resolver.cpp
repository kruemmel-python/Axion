#include "premise_resolver.hpp"

namespace mre {

PremiseResolutionResult PremiseResolver::resolve(
    const std::vector<std::string>& premises,
    const std::vector<DeclParam>& context_params
) {
    PremiseResolutionResult result;

    if (premises.empty()) {
        result.status = PremiseStatus::None;
        result.discharge_tactic = "rfl";
        result.explanation = "No premises required";
        return result;
    }

    std::vector<std::string> tactics;
    bool all_resolved = true;

    for (const auto& p : premises) {
        std::string premise = trim(p);

        // Check if premise is trivial equality: a = a
        size_t eqPos = premise.find('=');
        if (eqPos != std::string::npos) {
            std::string lhs = trim(premise.substr(0, eqPos));
            std::string rhs = trim(premise.substr(eqPos + 1));
            if (lhs == rhs) {
                tactics.push_back("rfl");
                continue;
            }
        }

        // Check trivial reflexivity or zero bound
        if (premise.find("0 ≤") != std::string::npos || premise.find("0 <= ") != std::string::npos) {
            tactics.push_back("exact Nat.zero_le _");
            continue;
        }

        if (premise == "True") {
            tactics.push_back("trivial");
            continue;
        }

        // Check against context hypotheses
        bool found_in_context = false;
        for (const auto& cp : context_params) {
            if (cp.is_prop && cp.type == premise) {
                tactics.push_back("exact " + cp.name);
                found_in_context = true;
                break;
            }
        }
        if (found_in_context) continue;

        // Otherwise unresolved
        all_resolved = false;
        result.unresolved_premises.push_back(premise);
    }

    if (all_resolved) {
        result.status = PremiseStatus::ResolvedArithmetic;
        result.discharge_tactic = "by ";
        for (size_t i = 0; i < tactics.size(); ++i) {
            if (i > 0) result.discharge_tactic += "; ";
            result.discharge_tactic += tactics[i];
        }
        result.explanation = "All premises discharged from context or elementary axioms";
    } else {
        result.status = PremiseStatus::Unresolved;
        result.discharge_tactic = "";
        result.explanation = "Unresolved premises: " + std::to_string(result.unresolved_premises.size());
    }

    return result;
}

} // namespace mre

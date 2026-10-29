#pragma once

#include "common.hpp"

namespace mre {

enum class NumericVerdict {
    NotApplicable,
    Counterexample,
    Survived,
    DomainError
};

inline std::string numericVerdictToString(NumericVerdict verdict) {
    switch (verdict) {
        case NumericVerdict::NotApplicable: return "NOT_NUMERIC";
        case NumericVerdict::Counterexample: return "NUMERIC_COUNTEREXAMPLE";
        case NumericVerdict::Survived: return "NUMERIC_SURVIVED";
        case NumericVerdict::DomainError: return "DOMAIN_ERROR";
    }
    return "UNKNOWN";
}

struct NumericResult {
    NumericVerdict verdict = NumericVerdict::NotApplicable;
    std::string counterexample_point;
    int points_tested = 0;
    std::string details;
};

class NumericEvaluator {
public:
    NumericEvaluator() = default;

    // Evaluates an algebraic equation hypothesis: "LHS = RHS"
    NumericResult testHypothesis(
        const std::string& lhs_expr,
        const std::string& rhs_expr,
        uint64_t seed = 42
    );

    // Parses a Lean algebraic statement like "∀ (x y : Nat), x + y = y + x"
    NumericResult testStatement(const std::string& statement, uint64_t seed = 42);
};

} // namespace mre

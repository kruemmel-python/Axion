#include "numeric_evaluator.hpp"
#include <cmath>
#include <random>

namespace mre {

struct EvalValue {
    double val = 0.0;
    bool valid = true;
    std::string err;
};

class ExprParser {
    std::string src_;
    size_t pos_ = 0;

    void skipSpace() {
        while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) {
            pos_++;
        }
    }

    char peek() {
        skipSpace();
        return pos_ < src_.size() ? src_[pos_] : '\0';
    }

    char get() {
        skipSpace();
        return pos_ < src_.size() ? src_[pos_++] : '\0';
    }

public:
    explicit ExprParser(const std::string& s) : src_(s) {}

    EvalValue parseExpr(double x, double y) {
        auto left = parseTerm(x, y);
        if (!left.valid) return left;

        while (true) {
            char op = peek();
            if (op == '+' || op == '-') {
                get();
                auto right = parseTerm(x, y);
                if (!right.valid) return right;
                if (op == '+') left.val += right.val;
                else left.val -= right.val;
            } else {
                break;
            }
        }
        return left;
    }

    EvalValue parseTerm(double x, double y) {
        auto left = parseFactor(x, y);
        if (!left.valid) return left;

        while (true) {
            char op = peek();
            if (op == '*' || op == '/') {
                get();
                auto right = parseFactor(x, y);
                if (!right.valid) return right;
                if (op == '*') {
                    left.val *= right.val;
                } else {
                    if (std::abs(right.val) < 1e-12) {
                        return {0.0, false, "Division by zero"};
                    }
                    left.val /= right.val;
                }
            } else {
                break;
            }
        }
        return left;
    }

    EvalValue parseFactor(double x, double y) {
        auto base = parsePrimary(x, y);
        if (!base.valid) return base;

        if (peek() == '^') {
            get();
            auto exp = parsePrimary(x, y);
            if (!exp.valid) return exp;
            if (base.val < 0.0 && std::floor(exp.val) != exp.val) {
                return {0.0, false, "Negative base with non-integer exponent"};
            }
            base.val = std::pow(base.val, exp.val);
            if (std::isnan(base.val) || std::isinf(base.val)) {
                return {0.0, false, "Numeric overflow or NaN in pow"};
            }
        }
        return base;
    }

    EvalValue parsePrimary(double x, double y) {
        skipSpace();
        char c = peek();
        if (c == '-') {
            get();
            auto res = parsePrimary(x, y);
            if (res.valid) res.val = -res.val;
            return res;
        }
        if (c == '(') {
            get();
            auto res = parseExpr(x, y);
            if (peek() == ')') get();
            else return {0.0, false, "Mismatched parentheses"};
            return res;
        }
        if (c == 'x' || c == 'X') {
            get();
            return {x, true, ""};
        }
        if (c == 'y' || c == 'Y') {
            get();
            return {y, true, ""};
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            size_t start = pos_;
            while (pos_ < src_.size() && (std::isdigit(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '.')) {
                pos_++;
            }
            try {
                double v = std::stod(src_.substr(start, pos_ - start));
                return {v, true, ""};
            } catch (...) {
                return {0.0, false, "Invalid number format"};
            }
        }
        return {0.0, false, "Unexpected character: " + std::string(1, c)};
    }
};

NumericResult NumericEvaluator::testHypothesis(
    const std::string& lhs_expr,
    const std::string& rhs_expr,
    uint64_t seed
) {
    NumericResult result;

    // Probe points
    const std::vector<double> probes = {-10.0, -5.0, -2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0, 5.0, 10.0};
    int tested = 0;

    // 1. Grid testing on probes
    for (double x : probes) {
        for (double y : probes) {
            ExprParser p1(lhs_expr);
            ExprParser p2(rhs_expr);

            auto v1 = p1.parseExpr(x, y);
            auto v2 = p2.parseExpr(x, y);

            if (!v1.valid || !v2.valid) {
                // If parsing fundamentally fails on simple valid input, it's not a numeric expression
                if (tested == 0 && (v1.err.find("Unexpected character") != std::string::npos ||
                                   v2.err.find("Unexpected character") != std::string::npos)) {
                    result.verdict = NumericVerdict::NotApplicable;
                    result.details = "Expression not supported by numeric evaluator: " + v1.err + " / " + v2.err;
                    return result;
                }
                // Otherwise it's a domain boundary / singularity (e.g. division by zero at x=0)
                continue;
            }

            tested++;
            if (std::abs(v1.val - v2.val) > 1e-9) {
                result.verdict = NumericVerdict::Counterexample;
                std::ostringstream ss;
                ss << "x=" << x << ", y=" << y << " (LHS=" << v1.val << " != RHS=" << v2.val << ")";
                result.counterexample_point = ss.str();
                result.points_tested = tested;
                result.details = "Counterexample found at probe grid point";
                return result;
            }
        }
    }

    // 2. Randomized testing from seed
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist(-50.0, 50.0);

    for (int i = 0; i < 40; ++i) {
        double x = dist(rng);
        double y = dist(rng);

        ExprParser p1(lhs_expr);
        ExprParser p2(rhs_expr);

        auto v1 = p1.parseExpr(x, y);
        auto v2 = p2.parseExpr(x, y);

        if (!v1.valid || !v2.valid) continue;

        tested++;
        if (std::abs(v1.val - v2.val) > 1e-9) {
            result.verdict = NumericVerdict::Counterexample;
            std::ostringstream ss;
            ss << "x=" << x << ", y=" << y << " (LHS=" << v1.val << " != RHS=" << v2.val << ")";
            result.counterexample_point = ss.str();
            result.points_tested = tested;
            result.details = "Counterexample found during randomized testing";
            return result;
        }
    }

    if (tested > 0) {
        result.verdict = NumericVerdict::Survived;
        result.points_tested = tested;
        result.details = "Survived " + std::to_string(tested) + " numerical probe points (NOT a formal proof)";
    } else {
        result.verdict = NumericVerdict::NotApplicable;
        result.details = "Could not evaluate numeric probes";
    }

    return result;
}

NumericResult NumericEvaluator::testStatement(const std::string& statement, uint64_t seed) {
    // Look for equality sign '='
    size_t eqPos = statement.find('=');
    if (eqPos == std::string::npos) {
        NumericResult res;
        res.verdict = NumericVerdict::NotApplicable;
        res.details = "No equality relation found in statement";
        return res;
    }

    // Find colon if quantified: ∀ (x y : Nat), lhs = rhs
    size_t startLhs = 0;
    size_t colonPos = statement.find(':');
    if (colonPos != std::string::npos && colonPos < eqPos) {
        startLhs = colonPos + 1;
    }

    std::string lhs = trim(statement.substr(startLhs, eqPos - startLhs));
    std::string rhs = trim(statement.substr(eqPos + 1));

    // Remove any trailing ":= by ..." or ":="
    size_t byPos = rhs.find(":=");
    if (byPos != std::string::npos) {
        rhs = trim(rhs.substr(0, byPos));
    }

    return testHypothesis(lhs, rhs, seed);
}

} // namespace mre

#pragma once

#include "common.hpp"
#include <string>
#include <vector>

namespace mre {

enum class VerificationStatus {
    Generated,
    ElaborationFailed,
    TypeCorrect,
    PremisesUnresolved,
    NumericCounterexample,
    NumericSurvived,
    ProofFailed,
    FormallyProven,
    AlreadyKnown,
    ResourceLimit,
    InfrastructureError
};

inline std::string verificationStatusToString(VerificationStatus status) {
    switch (status) {
        case VerificationStatus::Generated: return "GENERATED";
        case VerificationStatus::ElaborationFailed: return "ELABORATION_FAILED";
        case VerificationStatus::TypeCorrect: return "TYPE_CORRECT";
        case VerificationStatus::PremisesUnresolved: return "PREMISES_UNRESOLVED";
        case VerificationStatus::NumericCounterexample: return "NUMERIC_COUNTEREXAMPLE";
        case VerificationStatus::NumericSurvived: return "NUMERIC_SURVIVED";
        case VerificationStatus::ProofFailed: return "PROOF_FAILED";
        case VerificationStatus::FormallyProven: return "FORMALLY_PROVEN";
        case VerificationStatus::AlreadyKnown: return "ALREADY_KNOWN";
        case VerificationStatus::ResourceLimit: return "RESOURCE_LIMIT";
        case VerificationStatus::InfrastructureError: return "INFRASTRUCTURE_ERROR";
    }
    return "UNKNOWN";
}

struct VerificationResult {
    VerificationStatus status = VerificationStatus::Generated;
    int exit_code = -1;
    uint32_t duration_ms = 0;
    std::string log_file;
    std::string stdout_output;
    std::string error_message;
    std::vector<std::string> axioms_used;
    bool uses_sorry = false;
};

class Verifier {
    fs::path lean_bin_;
    uint32_t timeout_ms_ = 10000;

public:
    explicit Verifier(const fs::path& lean_bin = "");

    void setTimeoutMs(uint32_t ms) { timeout_ms_ = ms; }
    fs::path getLeanBin() const { return lean_bin_; }

    // Verifies a Lean 4 source file on disk
    VerificationResult verifyFile(const fs::path& lean_file, const fs::path& log_file);

    // Writes a candidate to a .lean file and verifies it
    VerificationResult verifyCandidate(
        const fs::path& output_dir,
        int64_t candidate_id,
        const std::string& lean_code,
        const std::vector<std::string>& imports = {}
    );
};

// Locate native lean.exe on the system
fs::path findLeanExecutable();

} // namespace mre

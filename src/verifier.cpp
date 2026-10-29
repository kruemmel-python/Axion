#include "verifier.hpp"
#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>

namespace mre {

fs::path findLeanExecutable() {
#ifdef _WIN32
    // Check user elan directory
    char* userprofile = nullptr;
    size_t len = 0;
    if (_dupenv_s(&userprofile, &len, "USERPROFILE") == 0 && userprofile) {
        fs::path p = fs::path(userprofile) / ".elan" / "bin" / "lean.exe";
        free(userprofile);
        if (fs::exists(p)) return p;
    }
    // Hardcoded fallback for ralfk environment
    if (fs::exists("C:\\Users\\ralfk\\.elan\\bin\\lean.exe")) {
        return "C:\\Users\\ralfk\\.elan\\bin\\lean.exe";
    }
#endif
    return "lean";
}

static const std::unordered_map<std::string, std::vector<std::string>>& oaiModuleNamespaces() {
    static const std::unordered_map<std::string, std::vector<std::string>> m = {
        {"BezoutFrame", {"ComplexCancellation.BezoutFrame"}},
        {"Bundle", {"ComplexCancellation.Bundle"}},
        {"BundleGrading", {"ComplexCancellation.Bundle"}},
        {"Clearing", {"ComplexCancellation.Clearing"}},
        {"Cylinder", {"ComplexCancellation.Cylinder", "Point"}},
        {"CylinderMaps", {"ComplexCancellation.Cylinder", "Frame", "Point"}},
        {"CylinderShift", {"ComplexCancellation.Cylinder", "Frame", "Point"}},
        {"Degeneration", {"ComplexCancellation.Degeneration"}},
        {"DegenerationGrading", {"ComplexCancellation.Degeneration"}},
        {"Determinant", {"ComplexCancellation.Determinant"}},
        {"DeterminantRigidity", {"ComplexCancellation.Determinant"}},
        {"DifferentialSlice", {"ComplexCancellation.DifferentialSlice"}},
        {"Dimension", {"ComplexCancellation"}},
        {"EquivariantLift", {"ComplexCancellation.PolynomialTorsor"}},
        {"Extraction", {"ComplexCancellation.AffineModification"}},
        {"FactorDerivation", {"ComplexCancellation.FactorDerivation"}},
        {"FractionDerivation", {"ComplexCancellation.FractionDerivation"}},
        {"GradedCommutation", {"ComplexCancellation.GradedLND"}},
        {"GradedLND", {"ComplexCancellation.GradedLND"}},
        {"GradedMap", {"ComplexCancellation.GradedMap"}},
        {"HighestWeight", {"ComplexCancellation.SL2"}},
        {"HomogeneousExtraction", {"ComplexCancellation.AffineModification"}},
        {"Intertwining", {"ComplexCancellation.DerivationIntertwining"}},
        {"InvariantRestriction", {"ComplexCancellation.InvariantRestriction"}},
        {"Kernel", {"ComplexCancellation.LND"}},
        {"LaurentDerivation", {"ComplexCancellation.LaurentDerivation"}},
        {"LaurentEuler", {"ComplexCancellation.LaurentEuler"}},
        {"Main", {"ComplexCancellation"}},
        {"Model", {"ComplexCancellation"}},
        {"Modification", {"ComplexCancellation.AffineModification"}},
        {"ModificationGrading", {"ComplexCancellation.AffineModification"}},
        {"Orbit", {"ComplexCancellation.LND"}},
        {"OrbitRigidity", {"ComplexCancellation.LND"}},
        {"Order", {"ComplexCancellation.LND"}},
        {"PolynomialDerivation", {"ComplexCancellation.PolynomialDerivation"}},
        {"PolynomialSlice", {"ComplexCancellation.DifferentialSlice", "ComplexCancellation.InvariantClearing", "ComplexCancellation.PolynomialModel"}},
        {"PositiveInvariant", {"ComplexCancellation.LND", "ComplexCancellation.Rees"}},
        {"PositiveObstruction", {"ComplexCancellation.Bundle", "ComplexCancellation.Determinant"}},
        {"Primitive", {"ComplexCancellation.LND"}},
        {"Quadric", {"ComplexCancellation.Quadric"}},
        {"QuotientDerivation", {"ComplexCancellation.QuotientDerivation"}},
        {"QuotientGrading", {"ComplexCancellation.QuotientGrading"}},
        {"RationalSlice", {"ComplexCancellation.RationalSlice"}},
        {"Rees", {"ComplexCancellation.Rees"}},
        {"ReesClearing", {"ComplexCancellation.Rees"}},
        {"ReesDerivation", {"ComplexCancellation.Rees"}},
        {"ReesGrading", {"ComplexCancellation.Rees"}},
        {"ReesReduction", {"ComplexCancellation.Rees"}},
        {"RelativeClearing", {"ComplexCancellation.DerivationRange", "ComplexCancellation.RelativeClearing"}},
        {"ReplicaLift", {"ComplexCancellation.Bundle", "ComplexCancellation.PolynomialTorsor", "ComplexCancellation.SliceDerivative", "ComplexCancellation.Torsor"}},
        {"Rigidity", {"ComplexCancellation.Rigidity"}},
        {"TorsorClearing", {"ComplexCancellation.Torsor"}},
        {"TorsorEuler", {"ComplexCancellation.Torsor"}},
        {"TorsorFrame", {"ComplexCancellation.Torsor"}},
        {"TorsorInverse", {"ComplexCancellation.Torsor"}}
    };
    return m;
}

Verifier::Verifier(const fs::path& lean_bin) {
    if (!lean_bin.empty() && fs::exists(lean_bin)) {
        lean_bin_ = lean_bin;
    } else {
        lean_bin_ = findLeanExecutable();
    }
    const char* timeout_env = std::getenv("MRE_LEAN_TIMEOUT_MS");
    if (timeout_env && *timeout_env) {
        try {
            timeout_ms_ = static_cast<uint32_t>(std::stoul(timeout_env));
        } catch (...) {}
    } else {
        timeout_ms_ = 500000; // 500 seconds default
    }
}

VerificationResult Verifier::verifyFile(const fs::path& lean_file, const fs::path& log_file) {
    VerificationResult result;
    result.log_file = utf8(log_file);

    if (!fs::exists(lean_bin_)) {
        result.status = VerificationStatus::InfrastructureError;
        result.error_message = "Lean binary not found: " + utf8(lean_bin_);
        return result;
    }

    if (!fs::exists(lean_file)) {
        result.status = VerificationStatus::InfrastructureError;
        result.error_message = "Lean source file not found: " + utf8(lean_file);
        return result;
    }

    // Fast offline preflight: do not spawn Lean for modules missing from every
    // configured LEAN_PATH root. No database scan or module build is performed.
    // Only applies to the source-bound exploratory candidates generated by MRE.
    {
        std::ifstream input(lean_file, std::ios::binary);
        const std::string source((std::istreambuf_iterator<char>(input)),
                                 std::istreambuf_iterator<char>());
        if (source.find("MRE_EXPLORATORY_ONLY") != std::string::npos) {
            const char* path_env = std::getenv("LEAN_PATH");
            if (path_env != nullptr && *path_env != '\0') {
                std::vector<fs::path> roots;
                std::istringstream paths(path_env);
#ifdef _WIN32
                constexpr char separator = ';';
#else
                constexpr char separator = ':';
#endif
                std::string root;
                while (std::getline(paths, root, separator)) {
                    if (!root.empty()) roots.emplace_back(root);
                }
                std::istringstream lines(source);
                std::string line;
                std::vector<std::string> missing;
                while (std::getline(lines, line)) {
                    if (line.rfind("import ", 0) != 0) continue;
                    std::istringstream modules(line.substr(7));
                    std::string module;
                    while (modules >> module) {
                        std::replace(module.begin(), module.end(), '.', '/');
                        const fs::path relative = fs::path(module + ".olean");
                        bool found = false;
                        for (const auto& base : roots) {
                            if (fs::is_regular_file(base / relative)) { found = true; break; }
                        }
                        if (!found) missing.push_back(module);
                    }
                }
                if (!missing.empty()) {
                    result.status = VerificationStatus::InfrastructureError;
                    result.exit_code = -1;
                    result.error_message = "MODULE_UNAVAILABLE: ";
                    for (size_t i = 0; i < missing.size(); ++i) {
                        if (i) result.error_message += ", ";
                        result.error_message += missing[i];
                    }
                    std::ofstream logOut(log_file, std::ios::binary);
                    if (logOut) logOut << "Preflight: no Lean process started\n"
                                       << result.error_message << "\n";
                    return result;
                }
            }
        }
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    std::string cmd = shellQuote(utf8(lean_bin_)) + " " + shellQuote(utf8(lean_file));
    ProcessResult proc = runProcess(cmd, timeout_ms_);

    auto end_time = std::chrono::high_resolution_clock::now();
    result.duration_ms = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count()
    );

    result.exit_code = proc.exit_code;
    result.stdout_output = proc.output;

    // Save log file
    {
        std::ofstream logOut(log_file, std::ios::binary);
        if (logOut) {
            logOut << "Command: " << cmd << "\n";
            logOut << "ExitCode: " << proc.exit_code << "\n";
            logOut << "DurationMs: " << result.duration_ms << "\n\n";
            logOut << "=== OUTPUT ===\n" << proc.output << "\n";
        }
    }

    if (proc.timed_out) {
        result.status = VerificationStatus::ResourceLimit;
        result.error_message = "Lean process timed out after " + std::to_string(timeout_ms_) + " ms";
        return result;
    }

    // Check for sorry or admit
    if (proc.output.find("declaration uses 'sorry'") != std::string::npos ||
        proc.output.find("declaration uses `sorry`") != std::string::npos ||
        proc.output.find("declaration uses 'admit'") != std::string::npos ||
        proc.output.find("declaration uses `admit`") != std::string::npos ||
        proc.output.find("`sorry`") != std::string::npos) {
        result.uses_sorry = true;
    }

    // Check for missing module / environment infrastructure error
    if (proc.output.find("unknown module prefix") != std::string::npos ||
        proc.output.find("no such file or directory") != std::string::npos ||
        (proc.output.find("object file ") != std::string::npos &&
         proc.output.find("does not exist") != std::string::npos)) {
        result.status = VerificationStatus::InfrastructureError;
        result.error_message = "Missing module or unresolved Lean environment path";
        return result;
    }

    if (proc.exit_code == 0) {
        // Checking existing constants, or defining a pair, is NOT a new proof.
        if (lean_file.extension() == ".lean") {
            std::ifstream source(lean_file, std::ios::binary);
            std::string contents((std::istreambuf_iterator<char>(source)),
                                  std::istreambuf_iterator<char>());
            if (contents.find("MRE_EXPLORATORY_ONLY") != std::string::npos &&
                contents.find("theorem ") == std::string::npos) {
                result.status = VerificationStatus::TypeCorrect;
                result.error_message = "Selected sources elaborated; no new theorem proved";
                return result;
            }
        }
        if (result.uses_sorry) {
            result.status = VerificationStatus::TypeCorrect;
            result.error_message = "Candidate is type-correct but contains 'sorry' or unproven obligations";
        } else {
            result.status = VerificationStatus::FormallyProven;

            // Parse axioms from output if present
            auto axPos = proc.output.find("does not depend on any axioms");
            if (axPos != std::string::npos) {
                result.axioms_used.push_back("none");
            } else {
                auto dependsPos = proc.output.find("depends on axioms:");
                if (dependsPos != std::string::npos) {
                    std::string axSub = proc.output.substr(dependsPos + 18);
                    std::istringstream ss(axSub);
                    std::string ax;
                    while (ss >> ax) {
                        result.axioms_used.push_back(ax);
                    }
                }
            }
        }
    } else {
        // Distinguish proof failure from syntax/elaboration failure
        if (proc.output.find("unsolved goals") != std::string::npos ||
            proc.output.find("tactic '") != std::string::npos ||
            proc.output.find("failed") != std::string::npos) {
            result.status = VerificationStatus::ProofFailed;
            result.error_message = "Proof tactic failed or goals unsolved";
        } else {
            result.status = VerificationStatus::ElaborationFailed;
            result.error_message = "Elaboration error: syntax or type mismatch";
        }
    }

    return result;
}

VerificationResult Verifier::verifyCandidate(
    const fs::path& output_dir,
    int64_t candidate_id,
    const std::string& lean_code,
    const std::vector<std::string>& imports
) {
    fs::create_directories(output_dir);

    fs::path lean_path = output_dir / ("Candidate_" + std::to_string(candidate_id) + ".lean");
    fs::path log_path = output_dir / ("Candidate_" + std::to_string(candidate_id) + ".log");

    {
        std::ofstream out(lean_path, std::ios::binary);
        check(bool(out), "Cannot write candidate Lean file: " + utf8(lean_path));

        out << "-- Math Research Engine v0.3 – Generated Formal Candidate\n";
        out << "-- Candidate ID: " << candidate_id << "\n";
        out << "-- Timestamp: " << currentTimestamp() << "\n\n";

        for (const auto& imp : imports) {
            if (!imp.empty()) {
                out << "import " << imp << "\n";
            }
        }

        // Open namespaces for imported modules so lexical declaration names can be resolved
        bool opened_oai = false;
        std::unordered_set<std::string> opened;
        for (const auto& imp : imports) {
            if (imp.rfind("OAI.", 0) == 0 && !opened_oai) {
                out << "open OAI\n";
                out << "open ComplexCancellation\n";
                opened.insert("OAI");
                opened.insert("ComplexCancellation");
                opened_oai = true;
            }
            if (imp.rfind("OAI.Algebra.AffineCancellation.", 0) == 0) {
                std::string leaf = imp.substr(std::string("OAI.Algebra.AffineCancellation.").size());
                const auto& ns_map = oaiModuleNamespaces();
                auto it = ns_map.find(leaf);
                if (it != ns_map.end()) {
                    for (const auto& ns : it->second) {
                        if (opened.insert(ns).second) {
                            out << "open " << ns << "\n";
                        }
                    }
                }
            }
        }

        out << "\n" << lean_code << "\n";
    }

    return verifyFile(lean_path, log_path);
}

} // namespace mre

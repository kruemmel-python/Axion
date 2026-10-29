#pragma once

#include "common.hpp"
#include "database.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>

namespace mre {

struct ExtractedComponent {
    std::string name;
    std::string kind; // "function", "class", "loop_recurrence", "state_machine", "data_structure"
    std::string file;
    int start_line = 0;
    int end_line = 0;
    std::string signature;
    std::string body;
    std::vector<std::string> mathematical_invariants;
    std::vector<std::string> state_variables;
};

struct CodeDumpAnalysis {
    fs::path source_path;
    std::vector<fs::path> scanned_files;
    std::vector<ExtractedComponent> components;
    std::vector<std::string> mathematical_patterns;
    size_t total_lines_of_code = 0;
    std::string summary;
};

struct OracleTestCase {
    std::string name;
    std::vector<std::string> arguments;
    std::string stdin_data;
    std::string expected_stdout;
    int expected_exit_code = 0;
    double tolerance = 1e-9;
};

struct OracleEvaluation {
    bool compiled = false;
    bool selftest_passed = false;
    bool equivalence_passed = false;
    int cases_tested = 0;
    int cases_matched = 0;
    int cases_diverged = 0;
    double baseline_avg_ns = 0.0;
    double candidate_avg_ns = 0.0;
    double speedup_ratio = 1.0; // candidate_time / baseline_time (< 1.0 means faster)
    double max_abs_delta = 0.0;
    std::string divergence_detail;
    std::string audit_log;
};

// Vollständiger Herkunftsnachweis:
// Originalcode -> extrahierte mathematische Struktur -> verwendete Datenbankbeziehungen ->
// neue Hypothese -> Codeänderung -> Prüfergebnisse -> neue Generation
struct ResearchFamily {
    std::string family_id;
    std::string name;
    std::string source_dump_path;
    std::string content_hash;
    int active_gen_index = 0;
    std::string created_at;
};

// Algebraische Struktur eines importierten HPP-Recurrence-Kernels
struct KernelAlgebraicStructure {
    bool is_detected = false;
    std::string header_filename = "candidate_kernel.hpp";
    uint32_t modulus = 65521;
    uint64_t c_x = 0;   // Koeffizient vor x
    uint64_t c_y = 0;   // Koeffizient vor y
    uint64_t c_xy = 0;  // Koeffizient vor x*y (bilineare Kopplung)
    uint64_t c_const = 0; // additiver konstanter Term
    int total_polynomial_degree = 2;
    int state_degree = 1; // Polynomgrad bezüglich Zustand x (1 = affin in x für festes y)
    bool has_bilinear_coupling = true;
    bool is_state_affine = true; // f_y(x) = (a + c*y)*x + (b*y + d) mod p
    uint64_t barrett_reciprocal = 65551; // floor(2^32 / modulus)
    uint64_t a_tot = 1; // 1000-Schritt komponierter Monoid-Faktor A
    uint64_t b_tot = 0; // 1000-Schritt komponierter Monoid-Summand B
    std::string math_description;
};

// Mathematische Domänenkategorie für elterngeleitete Synthese
enum class DomainCategory {
    MODULAR_RECURRENCE,
    TENSOR_CONTRACTION,
    UNKNOWN_DOMAIN
};

// Extrahierte Signatur einer bestehenden Eltern-Forschungsfamilie
struct FamilyDomainSignature {
    std::string family_id;
    fs::path baseline_dir;
    DomainCategory category = DomainCategory::UNKNOWN_DOMAIN;
    uint32_t modulus = 0;
    std::string header_filename;
    fs::path header_path;
    std::string step_definition_code;
    std::string description;
};

// Wissenschaftlich begründeter Fusionsplan für zwei Elternfamilien
struct MetaFusionPlan {
    bool viable = false;
    std::string non_viability_reason;
    std::string plan_title;
    std::string math_structure_provenance;
    std::string theorems_used_provenance;
    std::string synthesized_hypothesis;
    std::string main_cpp_code;
    std::vector<fs::path> headers_to_copy;
};

// Vollständiger Herkunftsnachweis:
// Originalcode -> extrahierte mathematische Struktur -> verwendete Datenbankbeziehungen ->
// neue Hypothese -> Codeänderung -> Prüfergebnisse -> neue Generation
struct MathematicalProvenance {
    std::string family_id = "AFFINE_RECURRENCE"; // Forschungsfamilie (z.B. AFFINE_RECURRENCE, TENSOR_ATTENTION)
    int gen_index = 0;
    std::string gen_id;
    std::string parent_gen_id;
    std::string algo_fingerprint; // Normalisierter struktureller Algorithmus-Fingerabdruck
    std::string complexity_class; // e.g. "O(N) Iterative", "O(1) Closed-Form", "O(log N) Matrix-Power"
    std::string original_code_snippet;
    std::string extracted_math_structure;
    std::string database_theorems_used;
    std::string synthesized_hypothesis;
    std::string code_mutation_diff;
    std::string oracle_validation_evidence;
    std::string scientific_summary;
    std::string created_at;
};

// Wissenschaftliche Bewertung der Zielerreichung (Regel 3)
struct GoalEvaluation {
    bool goal_achieved = false;
    std::string goal_summary;
    std::string scientific_evidence;
};

struct EvolutionConfig {
    fs::path dump_path;
    fs::path workspace_path;
    fs::path knowledge_db; // e.g. k4d_six_results.sqlite
    std::string family_id = "auto"; // "auto", "AFFINE_RECURRENCE", "TENSOR_ATTENTION", etc.
    std::string application_type = "auto"; // "auto", "console", "win32_gui"
    std::string fusion_family_a;
    std::string fusion_family_b;
    std::string target_fusion_family;
    std::string lm_studio_url;
    std::string model = "auto";
    uint64_t seed = 42;
    int cycle_depth = 10; // Generationen pro Forschungszyklus (Einstellung "Evolutionstiefe", Default 10)
    int max_cycles = 10;  // Maximale Zyklen vor sicherer Pause (Default 10 Zyklen à 10 = 100 Generationen)
    int max_rounds = 1;   // Legacy Kompatibilität
    uint32_t build_timeout_ms = 180000;
    uint32_t oracle_timeout_ms = 60000;
    bool require_lm = false;
};

class EvolutionEngine {
public:
    explicit EvolutionEngine(const EvolutionConfig& config);

    // Phase 1: Ingestion & Algorithm Extraction from arbitrary code-dump
    CodeDumpAnalysis analyzeAndIngestDump(const fs::path& dump_path);

    // Phase 2 & 3: Mathematical Meta-Fusion & Isolated Generation
    // Phase 4 & 5: Independent Oracle Validation & Promotion / Failure Memory
    int runEvolutionCycle(GoalEvaluation* outGoal = nullptr);

    // Meta-Fusion: Synthesize a new independent research family by fusing two existing families
    int runMetaFusionCycle(const std::string& family_a, const std::string& family_b, const std::string& target_fusion_family = "");

    // Autonomer Forschungszyklus über Blöcke von Generationen bis zum Zielerfolg (Regel 1, 2, 3)
    int runAutonomousContinuousEvolution(int cycle_depth = 10, int max_cycles = 10);
    int runAutonomousMetaFusionResearch(
        const std::string& family_a,
        const std::string& family_b,
        const std::string& target_fusion_family = "",
        int cycle_depth = 10,
        int max_cycles = 10
    );

    // Legacy Wrapper
    int runContinuousEvolution(int rounds);

    // Deterministische Teil-Seed Ableitung (Regel 1)
    static uint64_t deriveSubSeed(uint64_t baseSeed, int cycleIndex, int attemptIndex);

    // Zielerreichungs-Prüfung (Regel 3)
    GoalEvaluation evaluateResearchGoal(
        const std::string& family_id,
        const std::string& algo_fingerprint,
        const std::string& complexity_class,
        const OracleEvaluation& eval,
        const std::map<std::string, std::string>& files
    );

    // Rollback production baseline to previous generation (or specific gen_id)
    bool rollbackGeneration(int target_gen_index = -1);

    // Human-readable status overview (Generations, Baseline, Failure Memory)
    std::string generateStatusReport();

    // Retrieve full mathematical provenance for a given generation
    std::optional<MathematicalProvenance> getProvenance(int gen_index);

    // Render publication-ready markdown provenance document
    static std::string formatProvenanceMarkdown(const MathematicalProvenance& prov);

    // Normalisierter Algorithmus-Fingerabdruck (entfernt Syntaxrauschen & erkennt echte Transformationen)
    static std::string computeAlgorithmFingerprint(const std::string& code);
    static std::string classifyComplexity(const std::string& code);

    // Mathematische Strukturanalyse & Hypothesengenerierung für HPP-Kernel
    static KernelAlgebraicStructure analyzeKernelAlgebraicStructure(const fs::path& baseline_dir);

    // Elterngeleitete Signatur-Inspektion & Fusionssynthese
    static FamilyDomainSignature inspectFamilySignature(const fs::path& workspace_path, const std::string& family_id);
    static MetaFusionPlan formulateMetaFusionPlan(const fs::path& workspace_path, const std::string& famA, const std::string& famB, const std::string& fusedId);

    // Liste aller registrierten Forschungsfamilien
    std::vector<ResearchFamily> listFamilies();
    std::string getActiveFamilyId() const { return active_family_id_; }

    // Selftest verifying all 5 phases in an isolated temp environment
    static int runSelftest(const fs::path& temp_dir);

private:
    EvolutionConfig config_;
    std::string active_family_id_ = "AFFINE_RECURRENCE";
    fs::path baseline_dir_;
    fs::path generations_dir_;
    fs::path mutation_lab_dir_;
    fs::path archive_dir_;
    fs::path lineage_db_path_;
    fs::path validator_vault_dir_; // Unantastbare Validierungsinstanz (außerhalb der Mutation-Sandbox)

    void initDirectoriesAndDb();
    std::string detectOrRegisterFamily(const fs::path& dump_path, Db& db);
    int getActiveGenerationIndex(Db& db);
    std::string getActiveGenerationId(Db& db);
    void setupInitialBaseline(const CodeDumpAnalysis& analysis, Db& db);
    std::vector<OracleTestCase> loadOrCreateImmutableOracles(const fs::path& baseline_exe);
    OracleEvaluation evaluateCandidateAgainstOracle(const fs::path& candidate_exe, const fs::path& baseline_exe, const std::vector<OracleTestCase>& cases);
    void recordFailure(Db& db, int gen_index, const std::string& hypothesis_sig, const std::string& hypothesis_text, const std::string& failure_type, const std::string& details, const std::string& counterexample);
    void recordPromotion(Db& db, int new_gen_index, const std::string& new_gen_id, const std::string& parent_gen_id, const std::string& description, const std::string& hypothesis, const OracleEvaluation& eval, const MathematicalProvenance& prov);
    bool isFingerprintKnown(Db& db, const std::string& fingerprint);
};

} // namespace mre

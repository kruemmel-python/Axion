#include "experiment_runner.hpp"
#include "hypothesis_evolution.hpp"

namespace mre {

static std::string moduleOf(const std::string& path) {
    std::string s = path;
    for (char& c : s) if (c == '\\' || c == '/') c = '.';
    if (s.size() > 5 && s.substr(s.size() - 5) == ".lean") {
        s.resize(s.size() - 5);
    }
    return s;
}

ExperimentRunner::ExperimentRunner(const fs::path& lean_bin)
    : type_graph_(),
      generator_(type_graph_),
      numeric_evaluator_(),
      verifier_(lean_bin) {}

ExperimentSummary ExperimentRunner::runCycle(
    Db& source_db,
    Db& discovery_db,
    const fs::path& lean_output_dir,
    int count,
    uint64_t seed,
    GenerationStrategy strategy,
    const std::string& domain_filter,
    bool exclude_certs,
    bool k4d_guided
) {
    initializeOrMigrateDiscoveryDb(discovery_db);

    auto start_time = std::chrono::high_resolution_clock::now();
    ExperimentSummary summary;
    summary.total_candidates = count;
    summary.session_name = "Session_" + strategyToString(strategy) + (k4d_guided ? "_K4D_" : "_CONTROL_" ) + std::to_string(seed);

    // 1. Register experiment session in DB
    {
        Stmt insExp(discovery_db,
            "INSERT INTO experiments(session_name, strategy, seed, total_candidates, status) "
            "VALUES(?1, ?2, ?3, ?4, 'RUNNING');"
        );
        insExp.bind(1, summary.session_name);
        insExp.bind(2, strategyToString(strategy));
        insExp.bind(3, static_cast<int64_t>(seed));
        insExp.bind(4, count);
        insExp.step();
        summary.session_id = discovery_db.count("SELECT last_insert_rowid()");
    }

    // 2. Stages 1-4: Generate candidates
    auto candidates = generator_.generate(source_db, count, seed, strategy, domain_filter, exclude_certs, k4d_guided);

    const auto previous_max_id = discovery_db.count("SELECT COALESCE(MAX(id),0) FROM candidates");
    // Initial save to establish IDs
    generator_.saveCandidates(discovery_db, candidates);

    // Fetch assigned IDs
    Stmt getIds(discovery_db,
        "SELECT id FROM candidates WHERE id > " + std::to_string(previous_max_id) +
        " ORDER BY id LIMIT " + std::to_string(count)
    );
    int idx = 0;
    while (getIds.step() && idx < static_cast<int>(candidates.size())) {
        candidates[idx].id = getIds.getInt64(0);
        idx++;
    }

    // 3. Stages 5-8: Premise resolution, Numeric Evaluation, Lean Verification
    Stmt updateCand(discovery_db,
        "UPDATE candidates SET "
        "status = ?1, details = ?2, premises_status = ?3, unresolved_premises = ?4, "
        "counterexample = ?5, verification_exit_code = ?6, verification_duration_ms = ?7, "
        "error_log = ?8, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ?9"
    );

    Stmt insVerif(discovery_db,
        "INSERT INTO verifications(candidate_id, status, exit_code, log_path, duration_ms) "
        "VALUES(?1, ?2, ?3, ?4, ?5)"
    );

    Stmt insNum(discovery_db,
        "INSERT INTO numeric_evaluations(candidate_id, strategy, status, counterexample_val, test_points_count) "
        "VALUES(?1, ?2, ?3, ?4, ?5)"
    );

    for (auto& c : candidates) {
        // Stage 5: Premise resolution
        // Raw source indexing does not supply elaborated premise types.
        // Do not silently resolve an empty fabricated premise set.
        c.premises_status = "NOT_EXTRACTED";
        c.unresolved_premises = "Lean-native semantic extraction required";

        // Stage 7: Numeric counterexample evaluation
        auto numRes = numeric_evaluator_.testStatement(c.hypothesis_code, seed + c.ordinal);
        if (numRes.verdict != NumericVerdict::NotApplicable) {
            insNum.bind(1, c.id);
            insNum.bind(2, c.strategy);
            insNum.bind(3, numericVerdictToString(numRes.verdict));
            insNum.bind(4, numRes.counterexample_point);
            insNum.bind(5, numRes.points_tested);
            insNum.step();
            insNum.reset();
        }

        if (numRes.verdict == NumericVerdict::Counterexample) {
            c.status = "NUMERIC_COUNTEREXAMPLE";
            c.counterexample = numRes.counterexample_point;
            c.details = "Numeric counterexample detected: " + numRes.counterexample_point;
            summary.numeric_counterexamples++;
        } else {
            if (numRes.verdict == NumericVerdict::Survived) {
                summary.numeric_survived++;
            }

            // Stage 8: Formal Lean verification
            auto verifRes = verifier_.verifyCandidate(lean_output_dir, c.id, c.hypothesis_code, c.imports);
            c.status = verificationStatusToString(verifRes.status);
            c.verification_exit_code = verifRes.exit_code;
            c.verification_duration_ms = verifRes.duration_ms;
            c.error_log = verifRes.error_message;

            insVerif.bind(1, c.id);
            insVerif.bind(2, c.status);
            insVerif.bind(3, verifRes.exit_code);
            insVerif.bind(4, verifRes.log_file);
            insVerif.bind(5, verifRes.duration_ms);
            insVerif.step();
            insVerif.reset();

            if (verifRes.status == VerificationStatus::FormallyProven) {
                summary.formally_proven++;
            } else if (verifRes.status == VerificationStatus::ProofFailed) {
                summary.proof_failed++;
            } else if (verifRes.status == VerificationStatus::ElaborationFailed) {
                summary.elaboration_failed++;
            } else if (verifRes.status == VerificationStatus::AlreadyKnown) {
                summary.already_known++;
            }
        }

        // Stage 10: Persist updated candidate state
        updateCand.bind(1, c.status);
        updateCand.bind(2, c.details);
        updateCand.bind(3, c.premises_status);
        updateCand.bind(4, c.unresolved_premises);
        updateCand.bind(5, c.counterexample);
        updateCand.bind(6, c.verification_exit_code);
        updateCand.bind(7, c.verification_duration_ms);
        updateCand.bind(8, c.error_log);
        updateCand.bind(9, c.id);
        updateCand.step();
        updateCand.reset();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    summary.duration_ms = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count()
    );

    // Stage 12: Finalize experiment session
    {
        Stmt finExp(discovery_db,
            "UPDATE experiments SET "
            "proven_count = ?1, counterexample_count = ?2, status = 'COMPLETED' "
            "WHERE id = ?3"
        );
        finExp.bind(1, summary.formally_proven);
        finExp.bind(2, summary.numeric_counterexamples);
        finExp.bind(3, summary.session_id);
        finExp.step();
    }

    return summary;
}

ResearchChainSummary ExperimentRunner::runResearchChain(
    Db& source_db,
    Db& discovery_db,
    const fs::path& lean_output_dir,
    int max_depth,
    uint64_t seed,
    GenerationStrategy strategy,
    int64_t start_from_candidate_id,
    bool k4d_guided
) {
    initializeOrMigrateDiscoveryDb(discovery_db);
    auto start_time = std::chrono::high_resolution_clock::now();

    ResearchChainSummary chain_summary;
    chain_summary.target_depth = max_depth;

    auto pool = type_graph_.sampleDeclarations(source_db, 256, seed, "", true);
    if (pool.size() < 2) {
        throw std::runtime_error("Not enough eligible declarations in database for research chain.");
    }

    int64_t base_id = 0;
    int base_depth = 0;
    std::string base_code;
    std::string base_thm_name;
    std::vector<std::string> base_imports;

    // Check if continuing from an existing proven candidate
    if (start_from_candidate_id > 0) {
        Stmt q(discovery_db, "SELECT id, chain_depth, hypothesis_code, details, status FROM candidates WHERE id = " + std::to_string(start_from_candidate_id));
        if (q.step()) {
            base_id = q.getInt64(0);
            base_depth = static_cast<int>(discovery_db.count("SELECT chain_depth FROM candidates WHERE id = " + std::to_string(base_id)));
            if (base_depth <= 0) base_depth = 1;
            base_code = q.getText(2);
            base_thm_name = "candidate_" + std::to_string(base_id);
            chain_summary.achieved_depth = base_depth;
            std::cout << "[CHAIN] Resuming continuous research chain from Candidate #" << base_id
                      << " at depth " << base_depth << "...\n";
        }
    }

    // If starting fresh (no base), first produce Depth 1 root
    if (base_id == 0) {
        std::cout << "[CHAIN] Initializing Depth 1 root theorem using strategy: " << strategyToString(strategy) << "...\n";
        auto root_candidates = generator_.generate(source_db, 1, seed, strategy, "", true, k4d_guided);
        if (root_candidates.empty()) throw std::runtime_error("Failed to generate root candidate.");
        
        auto& c = root_candidates[0];
        c.chain_depth = 1;
        c.parent_id = 0;
        const auto prev_max = discovery_db.count("SELECT COALESCE(MAX(id),0) FROM candidates");
        generator_.saveCandidates(discovery_db, root_candidates);
        c.id = prev_max + 1;

        auto verif = verifier_.verifyCandidate(lean_output_dir / "depth_1", c.id, c.hypothesis_code, c.imports);
        c.status = verificationStatusToString(verif.status);
        c.verification_exit_code = verif.exit_code;
        c.verification_duration_ms = verif.duration_ms;
        c.error_log = verif.error_message;

        Stmt upd(discovery_db, "UPDATE candidates SET status = ?1, verification_exit_code = ?2, "
                               "verification_duration_ms = ?3, error_log = ?4, parent_id = ?5, chain_depth = ?6 WHERE id = ?7");
        upd.bind(1, c.status);
        upd.bind(2, c.verification_exit_code);
        upd.bind(3, c.verification_duration_ms);
        upd.bind(4, c.error_log);
        upd.bind(5, c.parent_id);
        upd.bind(6, static_cast<int64_t>(c.chain_depth));
        upd.bind(7, c.id);
        upd.step();

        ChainProgressStep step;
        step.depth = 1;
        step.candidate_id = c.id;
        step.parent_id = 0;
        step.status = c.status;
        step.theorem_name = "candidate_" + std::to_string(c.id);
        step.duration_ms = c.verification_duration_ms;
        chain_summary.steps.push_back(step);
        chain_summary.total_attempts++;

        if (verif.status == VerificationStatus::FormallyProven) {
            chain_summary.total_proven++;
            chain_summary.achieved_depth = 1;
            base_id = c.id;
            base_depth = 1;
            base_code = c.hypothesis_code;
            base_thm_name = "candidate_" + std::to_string(c.id);
            base_imports = c.imports;
            std::cout << "[CHAIN] Depth 1 PROVEN in Lean (Candidate #" << base_id << "). Moving to Depth 2...\n";
        } else {
            std::cout << "[CHAIN] Root candidate not proven (status: " << c.status << "). Chain stopped at Depth 1.\n";
            auto end_time = std::chrono::high_resolution_clock::now();
            chain_summary.total_duration_ms = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count()
            );
            return chain_summary;
        }
    }

    // Iterate continuous research chain: Depth d -> Depth d+1
    while (chain_summary.achieved_depth < max_depth) {
        int target_d = chain_summary.achieved_depth + 1;
        std::cout << "\n[CHAIN] === Advancing Research Chain to Depth " << target_d
                  << " (Building upon Candidate #" << base_id << ") ===\n";

        bool depth_proven = false;
        int max_attempts_per_depth = 3;

        for (int attempt = 0; attempt < max_attempts_per_depth && !depth_proven; ++attempt) {
            size_t partner_idx = (seed + target_d * 17 + attempt * 31) % pool.size();
            auto evolved = HypothesisEvolutionEngine::evolveChained(
                pool, base_id, base_depth, base_code, base_thm_name, base_imports,
                partner_idx, seed + target_d * 101 + attempt, 12, k4d_guided
            );

            Candidate c;
            c.seed = seed + target_d * 100 + attempt;
            c.ordinal = attempt;
            c.strategy = strategyToString(strategy);
            c.mode = k4d_guided ? "K4D_GUIDED" : "CONTROL_NO_K";
            c.parent_id = base_id;
            c.chain_depth = target_d;
            c.source_a = base_thm_name;
            c.source_b = evolved.root_b.name;
            c.file_a = "chain_parent_" + std::to_string(base_id);
            c.file_b = evolved.root_b.file;
            c.status = "GENERATED";
            c.details = "ChainDepth=" + std::to_string(target_d) +
                        " | Parent=" + std::to_string(base_id) +
                        " | Rel=" + hypothesisRelationToString(evolved.relation) +
                        " | Stability=" + std::to_string(evolved.stability_score) +
                        " | " + evolved.provenance;
            c.hypothesis_code = evolved.theorem_statement + "\n" + evolved.check_script;
            c.imports = evolved.imports;

            const auto prev_max = discovery_db.count("SELECT COALESCE(MAX(id),0) FROM candidates");
            std::vector<Candidate> to_save{c};
            generator_.saveCandidates(discovery_db, to_save);
            c.id = prev_max + 1;

            fs::path depth_dir = lean_output_dir / ("depth_" + std::to_string(target_d));
            auto verif = verifier_.verifyCandidate(depth_dir, c.id, c.hypothesis_code, c.imports);
            c.status = verificationStatusToString(verif.status);
            c.verification_exit_code = verif.exit_code;
            c.verification_duration_ms = verif.duration_ms;
            c.error_log = verif.error_message;

            Stmt upd(discovery_db, "UPDATE candidates SET status = ?1, verification_exit_code = ?2, "
                                   "verification_duration_ms = ?3, error_log = ?4, parent_id = ?5, chain_depth = ?6 WHERE id = ?7");
            upd.bind(1, c.status);
            upd.bind(2, c.verification_exit_code);
            upd.bind(3, c.verification_duration_ms);
            upd.bind(4, c.error_log);
            upd.bind(5, c.parent_id);
            upd.bind(6, static_cast<int64_t>(c.chain_depth));
            upd.bind(7, c.id);
            upd.step();

            ChainProgressStep step;
            step.depth = target_d;
            step.candidate_id = c.id;
            step.parent_id = base_id;
            step.status = c.status;
            step.theorem_name = evolved.theorem_name;
            step.stability_score = evolved.stability_score;
            step.duration_ms = c.verification_duration_ms;
            chain_summary.steps.push_back(step);
            chain_summary.total_attempts++;

            std::cout << "  [Attempt " << (attempt + 1) << "] Candidate #" << c.id
                      << " -> " << c.status << " (exit=" << verif.exit_code
                      << ", duration=" << verif.duration_ms << "ms)\n";

            if (verif.status == VerificationStatus::FormallyProven) {
                depth_proven = true;
                chain_summary.total_proven++;
                chain_summary.achieved_depth = target_d;

                // Promote the proven candidate to the new foundation for the subsequent level!
                base_id = c.id;
                base_depth = target_d;
                base_code = evolved.theorem_statement;
                base_thm_name = evolved.theorem_name;
                base_imports = evolved.imports;

                std::cout << "  >>> SUCCESS: Theorem at Depth " << target_d
                          << " formally verified! Promoted as foundation for Depth "
                          << (target_d + 1) << ".\n";
            }
        }

        if (!depth_proven) {
            std::cout << "[CHAIN] Could not prove hypothesis at Depth " << target_d
                      << " within " << max_attempts_per_depth << " attempts. Chain halted at Depth "
                      << chain_summary.achieved_depth << ".\n";
            break;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    chain_summary.total_duration_ms = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count()
    );

    std::cout << "\n=== CONTINUOUS RESEARCH CHAIN COMPLETED ===\n"
              << "Target Depth:   " << chain_summary.target_depth << "\n"
              << "Achieved Depth: " << chain_summary.achieved_depth << "\n"
              << "Total Attempts: " << chain_summary.total_attempts << "\n"
              << "Theorems Proven:" << chain_summary.total_proven << "\n"
              << "Total Duration: " << chain_summary.total_duration_ms << " ms\n"
              << "===========================================\n";

    return chain_summary;
}

ResearchNetworkSummary ExperimentRunner::runResearchNetwork(
    Db& source_db,
    Db& discovery_db,
    const fs::path& lean_output_dir,
    int max_depth,
    int branching_factor,
    uint64_t seed,
    GenerationStrategy strategy,
    int64_t start_from_candidate_id,
    bool k4d_guided
) {
    initializeOrMigrateDiscoveryDb(discovery_db);
    auto start_time = std::chrono::high_resolution_clock::now();

    ResearchNetworkSummary net_summary;
    net_summary.target_depth = max_depth;
    net_summary.branching_factor = std::max(1, std::min(8, branching_factor));

    auto pool = type_graph_.sampleDeclarations(source_db, 256, seed, "", true);
    if (pool.size() < 2) {
        throw std::runtime_error("Not enough eligible declarations in database for research network.");
    }

    struct NetworkNode {
        int64_t id = 0;
        int depth = 0;
        std::string code;
        std::string thm_name;
        std::vector<std::string> imports;
    };

    std::vector<NetworkNode> frontier;

    // Check if continuing from an existing proven candidate
    if (start_from_candidate_id > 0) {
        Stmt q(discovery_db, "SELECT id, chain_depth, hypothesis_code, details, status, file_a, file_b FROM candidates WHERE id = " + std::to_string(start_from_candidate_id));
        if (q.step()) {
            NetworkNode root;
            root.id = q.getInt64(0);
            root.depth = static_cast<int>(discovery_db.count("SELECT chain_depth FROM candidates WHERE id = " + std::to_string(root.id)));
            if (root.depth <= 0) root.depth = 1;
            root.code = q.getText(2);
            root.thm_name = "candidate_" + std::to_string(root.id);
            std::string fa = q.getText(5);
            std::string fb = q.getText(6);
            if (!fa.empty() && fa.rfind("chain_", 0) != 0) root.imports.push_back(moduleOf(fa));
            if (!fb.empty() && fb.rfind("chain_", 0) != 0) root.imports.push_back(moduleOf(fb));

            frontier.push_back(root);
            net_summary.max_depth_achieved = root.depth;
            std::cout << "[NETWORK] Resuming research network from Candidate #" << root.id
                      << " at depth " << root.depth << " (Branching factor: " << net_summary.branching_factor << ")...\n";
        }
    }

    // If starting fresh (no base), first produce Depth 1 root
    if (frontier.empty()) {
        std::cout << "[NETWORK] Initializing Depth 1 root theorem using strategy: " << strategyToString(strategy) << "...\n";
        auto root_candidates = generator_.generate(source_db, 1, seed, strategy, "", true, k4d_guided);
        if (root_candidates.empty()) throw std::runtime_error("Failed to generate root candidate.");

        auto& c = root_candidates[0];
        c.chain_depth = 1;
        c.parent_id = 0;
        const auto prev_max = discovery_db.count("SELECT COALESCE(MAX(id),0) FROM candidates");
        generator_.saveCandidates(discovery_db, root_candidates);
        c.id = prev_max + 1;

        auto verif = verifier_.verifyCandidate(lean_output_dir / "depth_1", c.id, c.hypothesis_code, c.imports);
        c.status = verificationStatusToString(verif.status);
        c.verification_exit_code = verif.exit_code;
        c.verification_duration_ms = verif.duration_ms;
        c.error_log = verif.error_message;

        Stmt upd(discovery_db, "UPDATE candidates SET status = ?1, verification_exit_code = ?2, "
                               "verification_duration_ms = ?3, error_log = ?4, parent_id = ?5, chain_depth = ?6 WHERE id = ?7");
        upd.bind(1, c.status);
        upd.bind(2, c.verification_exit_code);
        upd.bind(3, c.verification_duration_ms);
        upd.bind(4, c.error_log);
        upd.bind(5, c.parent_id);
        upd.bind(6, static_cast<int64_t>(c.chain_depth));
        upd.bind(7, c.id);
        upd.step();

        ChainProgressStep step;
        step.depth = 1;
        step.candidate_id = c.id;
        step.parent_id = 0;
        step.status = c.status;
        step.theorem_name = "candidate_" + std::to_string(c.id);
        step.duration_ms = c.verification_duration_ms;
        net_summary.steps.push_back(step);
        net_summary.total_nodes_explored++;

        if (verif.status == VerificationStatus::FormallyProven) {
            net_summary.total_proven_nodes++;
            net_summary.max_depth_achieved = 1;
            NetworkNode root;
            root.id = c.id;
            root.depth = 1;
            root.code = c.hypothesis_code;
            root.thm_name = "candidate_" + std::to_string(c.id);
            root.imports = c.imports;
            frontier.push_back(root);
            std::cout << "[NETWORK] Root PROVEN in Lean (Candidate #" << c.id << "). Spawning initial network branches...\n";
        } else {
            std::cout << "[NETWORK] Root candidate not proven (status: " << c.status << "). Network terminated at Depth 1.\n";
            auto end_time = std::chrono::high_resolution_clock::now();
            net_summary.total_duration_ms = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count()
            );
            return net_summary;
        }
    }

    // Branching frontier expansion: Breadth-first generation of proof network
    int current_depth = frontier.empty() ? 1 : frontier.front().depth;
    while (!frontier.empty() && current_depth < max_depth) {
        int next_depth = current_depth + 1;
        std::cout << "\n[NETWORK] === Expanding Frontier: Depth " << current_depth
                  << " -> Depth " << next_depth << " (" << frontier.size()
                  << " parent node" << (frontier.size() > 1 ? "s" : "")
                  << ", branching factor " << net_summary.branching_factor << ") ===\n";

        std::vector<NetworkNode> next_frontier;

        for (size_t p_idx = 0; p_idx < frontier.size(); ++p_idx) {
            const auto& parent = frontier[p_idx];
            std::cout << "  [Parent Node #" << parent.id << " (Depth " << parent.depth << ")] Spawning "
                      << net_summary.branching_factor << " parallel branches...\n";

            for (int branch = 0; branch < net_summary.branching_factor; ++branch) {
                uint64_t branch_seed = seed + next_depth * 1000 + p_idx * 100 + branch * 19;
                size_t partner_idx = (branch_seed + branch * 37) % pool.size();

                auto evolved = HypothesisEvolutionEngine::evolveChained(
                    pool, parent.id, parent.depth, parent.code, parent.thm_name, parent.imports,
                    partner_idx, branch_seed, 12, k4d_guided
                );

                Candidate c;
                c.seed = branch_seed;
                c.ordinal = branch;
                c.strategy = strategyToString(strategy);
                c.mode = k4d_guided ? "K4D_GUIDED" : "CONTROL_NO_K";
                c.parent_id = parent.id;
                c.chain_depth = next_depth;
                c.source_a = parent.thm_name;
                c.source_b = evolved.root_b.name;
                c.file_a = "chain_parent_" + std::to_string(parent.id);
                c.file_b = evolved.root_b.file;
                c.status = "GENERATED";
                c.details = "NetworkDepth=" + std::to_string(next_depth) +
                            " | Parent=" + std::to_string(parent.id) +
                            " | Branch=" + std::to_string(branch + 1) + "/" + std::to_string(net_summary.branching_factor) +
                            " | Rel=" + hypothesisRelationToString(evolved.relation) +
                            " | Stability=" + std::to_string(evolved.stability_score) +
                            " | " + evolved.provenance;
                c.hypothesis_code = evolved.theorem_statement + "\n" + evolved.check_script;
                c.imports = evolved.imports;

                const auto prev_max = discovery_db.count("SELECT COALESCE(MAX(id),0) FROM candidates");
                std::vector<Candidate> to_save{c};
                generator_.saveCandidates(discovery_db, to_save);
                c.id = prev_max + 1;

                fs::path depth_dir = lean_output_dir / ("depth_" + std::to_string(next_depth)) / ("parent_" + std::to_string(parent.id));
                auto verif = verifier_.verifyCandidate(depth_dir, c.id, c.hypothesis_code, c.imports);
                c.status = verificationStatusToString(verif.status);
                c.verification_exit_code = verif.exit_code;
                c.verification_duration_ms = verif.duration_ms;
                c.error_log = verif.error_message;

                Stmt upd(discovery_db, "UPDATE candidates SET status = ?1, verification_exit_code = ?2, "
                                       "verification_duration_ms = ?3, error_log = ?4, parent_id = ?5, chain_depth = ?6 WHERE id = ?7");
                upd.bind(1, c.status);
                upd.bind(2, c.verification_exit_code);
                upd.bind(3, c.verification_duration_ms);
                upd.bind(4, c.error_log);
                upd.bind(5, c.parent_id);
                upd.bind(6, static_cast<int64_t>(c.chain_depth));
                upd.bind(7, c.id);
                upd.step();

                ChainProgressStep step;
                step.depth = next_depth;
                step.candidate_id = c.id;
                step.parent_id = parent.id;
                step.status = c.status;
                step.theorem_name = evolved.theorem_name;
                step.stability_score = evolved.stability_score;
                step.duration_ms = c.verification_duration_ms;
                net_summary.steps.push_back(step);
                net_summary.total_nodes_explored++;

                std::cout << "    [Branch " << (branch + 1) << "] Candidate #" << c.id
                          << " -> " << c.status << " (exit=" << verif.exit_code
                          << ", duration=" << verif.duration_ms << "ms)\n";

                if (verif.status == VerificationStatus::FormallyProven) {
                    net_summary.total_proven_nodes++;
                    net_summary.max_depth_achieved = std::max(net_summary.max_depth_achieved, next_depth);

                    NetworkNode child;
                    child.id = c.id;
                    child.depth = next_depth;
                    child.code = evolved.theorem_statement;
                    child.thm_name = evolved.theorem_name;
                    child.imports = evolved.imports;
                    next_frontier.push_back(child);

                    std::cout << "    >>> PROVEN IN LEAN: Promoted Candidate #" << c.id
                              << " to active network frontier for Depth " << (next_depth + 1) << "!\n";
                }
            }
        }

        frontier = std::move(next_frontier);
        current_depth = next_depth;

        if (frontier.empty()) {
            std::cout << "[NETWORK] All branches terminated at Depth " << current_depth
                      << ". No candidates survived to expand the frontier further.\n";
            break;
        }
    }

    net_summary.active_frontier_size = static_cast<int>(frontier.size());
    auto end_time = std::chrono::high_resolution_clock::now();
    net_summary.total_duration_ms = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count()
    );

    std::cout << "\n=== CONTINUOUS RESEARCH NETWORK COMPLETED ===\n"
              << "Target Depth:        " << net_summary.target_depth << "\n"
              << "Max Depth Achieved:  " << net_summary.max_depth_achieved << "\n"
              << "Branching Factor:    " << net_summary.branching_factor << "\n"
              << "Total Nodes Explored:" << net_summary.total_nodes_explored << "\n"
              << "Theorems Proven:     " << net_summary.total_proven_nodes << "\n"
              << "Active Frontier:     " << net_summary.active_frontier_size << " nodes\n"
              << "Total Duration:      " << net_summary.total_duration_ms << " ms\n"
              << "=============================================\n";

    return net_summary;
}

} // namespace mre

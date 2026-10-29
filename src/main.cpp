#include "common.hpp"
#include "k4d.hpp"
#include "database.hpp"
#include "semantic_types.hpp"
#include "type_graph.hpp"
#include "generator.hpp"
#include "premise_resolver.hpp"
#include "numeric_evaluator.hpp"
#include "verifier.hpp"
#include "experiment_runner.hpp"
#include "cli_monitor.hpp"
#include "selftest.hpp"
#include "semantic_batch.hpp"
#include "module_cache.hpp"
#include "open_research.hpp"
#include "open_structures.hpp"
#include "algorithm_lab.hpp"
#include "autonomous_software.hpp"
#include "database_first_research.hpp"
#include "evolution_engine.hpp"

using namespace mre;

static void printUsage() {
    std::cout << "Axion Research Engine – Autonomous Mathematical Research & Synthesis\n"
              << "Entwickler: Ralf Krümmel\n\n"
              << "CLI Usage:\n\n"
              << "  axion_engine k4d-research <output.json> [side] [steps] [seed]\n"
              << "  axion_engine k4d-selftest\n\n"
              << "  math_research_v03 stats <db-file>\n"
              << "      Show comprehensive statistics of a database\n\n"
              << "  math_research_v03 graph <source-db> <discovery-db>\n"
              << "      Build module import graph and structural relation edges\n\n"
              << "  math_research_v03 discover <source-db> <discovery-db> <count> <seed> <strategy> [domain]\n"
              << "      Synthesize mathematical hypotheses using specified strategy:\n"
              << "      (type_synthesis | systematic | random | cross_domain | mutation | iterative)\n\n"
              << "  math_research_v03 numtest <discovery-db> <first-id> <count>\n"
              << "      Execute numeric counterexample probes against algebraic statements\n\n"
              << "  math_research_v03 export <discovery-db> <output-dir> <first-id> <count>\n"
              << "      Export candidate formal proofs into Lean source files\n\n"
              << "  math_research_v03 verify <discovery-db> <output-dir> <first-id> <count> [lean-path]\n"
              << "      Execute formal Lean 4 verification on exported candidates\n\n"
              << "  math_research_v03 cycle-k4d-six <source-db> <discovery-db> <output-dir> <count-per-line> <seed> [lean-exe]\n"
              << "      Six Lean strategies x K4D_GUIDED and CONTROL_NO_K, same budget, audited candidates\n\n"
              << "  math_research_v03 cycle <source-db> <discovery-db> <output-dir> <count> <seed> <strategy>\n"
              << "      Run full 12-stage autonomous research discovery cycle\n\n"
              << "  math_research_v03 monitor <discovery-db>\n"
              << "      Display live console dashboard and candidate breakdown\n\n"
              << "  math_research_v03 inspect <discovery-db> <candidate-id>\n"
              << "      View detailed code, counterexample, and proof log for candidate\n\n"
              << "  math_research_v03 export-data <discovery-db> <output-file> <json|csv>\n"
              << "      Export candidates to structured JSON or CSV format\n\n"
              << "  math_research_v03 semantic-probe <script> <moduleA> <nameA> <moduleB> <nameB> <lean-exe>\n"
              << "      Lean-native type and application analysis (JSON, no proof claim)\n\n"
              << "  math_research_v03 module-inventory <source-db> <result-db> <olean-root> [module-prefix]\n"
              << "      Cache module availability in result database without touching original sources\n\n"
              << "  math_research_v03 semantic-report <result-db>\n"
              << "      Show cached module availability and semantic probe statuses\n\n"
              << "  math_research_v03 semantic-batch <source-db> <result-db> <LeanSynthesizer.lean> <lean-exe> <pairs> <seed> [module-prefix] [timeout-ms]\n"
              << "      Run source-bound Lean-native research probes; persist every result separately\n\n"
              << "  math_research_v03 open-explore <result-db> <count> <seed> [lean-exe]\n"
              << "      Independent algebraic hypothesis generation, numeric tests, optional Lean proof\n\n"
              << "  math_research_v03 open-structures <result-db> <count> <seed>\n"
              << "      Experimental operators on finite modular, integer-pair and Boolean-pair structures\n\n"
              << "  math_research_v03 algorithm-lab <result-db> <cpp-output-dir> <count> <seed>\n"
              << "      Evaluate generated native operator kernels against a fixed streaming baseline\n\n"
              << "  math_research_v03 software-research <memory-db> <workspace> <iterations> <seed> [lm-studio-url] [model] [--require-lm]\n"
              << "      Research software purposes, ask a coding model for implementations, compile and test candidates\n\n"
              << "  math_research_v03 database-first-software-research <source-db> <research-db> <workspace> <iterations> <seed> <api-url> <model> [lean-exe] [lean-script] [lean-root] [--provider=gemini|lm-studio] [--lm-studio-url=<url>] [--require-gemini|--require-model]\n"
              << "      Index the complete read-only mathematical DB, create an auditable dossier, optionally run Lean, and call Gemini or LM Studio\n\n"
              << "  math_research_v03 software-selftest <temp-dir>\n"
              << "      Validate the open-ended software laboratory and persistent research memory\n\n"
              << "  math_research_v03 selftest <temp-dir>\n"
              << "      Execute complete 18-point automated validation test suite\n\n";
}

int main(int argc, char** argv) {
    try {
        if (argc >= 2 && std::string(argv[1]) == "k4d-selftest") return mre::k4d::selftest();
        if (argc >= 3 && std::string(argv[1]) == "k4d-research") {
            return mre::k4d::run(argv[2], argc >= 4 ? std::stoi(argv[3]) : 8,
                argc >= 5 ? std::stoi(argv[4]) : 100,
                argc >= 6 ? std::stoull(argv[5]) : 314159ULL);
        }
        if (argc < 2) {
            printUsage();
            return 2;
        }

        std::string cmd = argv[1];

        if (cmd == "semantic-probe" && argc >= 8) {
            // Lean itself decides compatibility; textual C++ type guesses are not trusted.
            // Syntax: semantic-probe <LeanSynthesizer.lean> <ModuleA> <NameA> <ModuleB> <NameB> <lean-exe>
            auto validName = [](const std::string& v) {
                if (v.empty()) return false;
                for (unsigned char ch : v) {
                    if (!(std::isalnum(ch) || ch == '_' || ch == '.' || ch == '\'')) return false;
                }
                return true;
            };
            const fs::path script = argv[2];
            const std::string ma = argv[3], a = argv[4], mb = argv[5], b = argv[6];
            const fs::path leanBin = argv[7];
            check(fs::is_regular_file(script), "LeanSynthesizer file missing");
            check(fs::is_regular_file(leanBin), "Lean executable missing");
            check(validName(ma) && validName(a) && validName(mb) && validName(b),
                  "Invalid module/declaration name; expected fully qualified Lean identifier");
            const std::string command = shellQuote(utf8(leanBin)) + " --run " + shellQuote(utf8(script)) +
                " " + shellQuote(ma) + " " + shellQuote(a) + " " + shellQuote(mb) + " " + shellQuote(b);
            const ProcessResult result = runProcess(command, 120000);
            std::cout << result.output << "\n";
            if (result.timed_out) { std::cerr << "RESOURCE_LIMIT: Lean probe timed out\n"; return 3; }
            return result.exit_code == 0 ? 0 : 1;
        }

        if (cmd == "module-inventory" && argc >= 5) {
            return runModuleInventory(argv[2], argv[3], argv[4], argc >= 6 ? argv[5] : "OAI.");
        }
        if (cmd == "semantic-report" && argc >= 3) {
            return runSemanticReport(argv[2]);
        }
        if (cmd == "semantic-batch" && argc >= 8) {
            const std::string prefix = argc >= 9 ? argv[8] : "OAI.";
            const uint32_t timeout = argc >= 10 ? static_cast<uint32_t>(std::stoul(argv[9])) : 30000;
            return runSemanticBatch(argv[2], argv[3], argv[4], argv[5],
                                    std::stoi(argv[6]), std::stoull(argv[7]), prefix, timeout);
        }

        if (cmd == "open-explore" && argc >= 5) {
            return runOpenResearch(argv[2], std::stoi(argv[3]), std::stoull(argv[4]),
                                   argc >= 6 ? fs::path(argv[5]) : fs::path{});
        }
        if (cmd == "open-structures" && argc >= 5) {
            return runOpenStructures(argv[2], std::stoi(argv[3]), std::stoull(argv[4]));
        }
        if (cmd == "algorithm-lab" && argc >= 6) {
            return runAlgorithmLab(argv[2], argv[3], std::stoi(argv[4]), std::stoull(argv[5]));
        }
        if (cmd == "software-selftest" && argc >= 3) {
            return runSoftwareLabSelftest(argv[2]);
        }
        if (cmd == "evolution-run" && argc >= 4) {
            EvolutionConfig cfg;
            cfg.dump_path = argv[2];
            cfg.workspace_path = argv[3];
            if (argc >= 5) cfg.seed = std::stoull(argv[4]);
            int rounds = 1;
            int positional = 0;
            for (int i = 5; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg == "--require-lm") cfg.require_lm = true;
                else if (arg.rfind("--rounds=", 0) == 0) rounds = std::stoi(arg.substr(9));
                else if (arg == "--rounds" && i + 1 < argc) rounds = std::stoi(argv[++i]);
                else if (arg.rfind("--depth=", 0) == 0) cfg.cycle_depth = std::stoi(arg.substr(8));
                else if (arg == "--depth" && i + 1 < argc) cfg.cycle_depth = std::stoi(argv[++i]);
                else if (arg.rfind("--max-cycles=", 0) == 0) cfg.max_cycles = std::stoi(arg.substr(13));
                else if (arg == "--max-cycles" && i + 1 < argc) cfg.max_cycles = std::stoi(argv[++i]);
                else if (arg.rfind("--seed=", 0) == 0) cfg.seed = std::stoull(arg.substr(7));
                else if (arg == "--seed" && i + 1 < argc) cfg.seed = std::stoull(argv[++i]);
                else if (arg.rfind("--family=", 0) == 0) cfg.family_id = arg.substr(9);
                else if (arg == "--family" && i + 1 < argc) cfg.family_id = argv[++i];
                else if (arg.rfind("--app-type=", 0) == 0) cfg.application_type = arg.substr(11);
                else if (arg == "--app-type" && i + 1 < argc) cfg.application_type = argv[++i];
                else if (arg.rfind("--application-type=", 0) == 0) cfg.application_type = arg.substr(19);
                else if (arg == "--application-type" && i + 1 < argc) cfg.application_type = argv[++i];
                else if (positional == 0) { cfg.lm_studio_url = arg; positional++; }
                else if (positional == 1) { cfg.model = arg; positional++; }
            }
            EvolutionEngine engine(cfg);
            if (rounds > 1 || cfg.max_cycles > 1) {
                int depth = (cfg.cycle_depth > 0) ? cfg.cycle_depth : rounds;
                int maxC = (cfg.max_cycles > 0) ? cfg.max_cycles : 10;
                return engine.runAutonomousContinuousEvolution(depth, maxC);
            }
            return engine.runEvolutionCycle();
        }
        if (cmd == "evolution-fusion" && argc >= 5) {
            EvolutionConfig cfg;
            std::string famA = argv[2];
            std::string famB = argv[3];
            cfg.workspace_path = argv[4];
            std::string targetFam = (argc >= 6 && argv[5][0] != '-') ? argv[5] : "";
            int startArg = (argc >= 6 && argv[5][0] != '-') ? 6 : 5;
            for (int i = startArg; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg.rfind("--target=", 0) == 0) targetFam = arg.substr(9);
                else if (arg.rfind("--app-type=", 0) == 0) cfg.application_type = arg.substr(11);
                else if (arg.rfind("--application-type=", 0) == 0) cfg.application_type = arg.substr(19);
                else if (arg.rfind("--seed=", 0) == 0) cfg.seed = std::stoull(arg.substr(7));
                else if (arg == "--seed" && i + 1 < argc) cfg.seed = std::stoull(argv[++i]);
                else if (arg.rfind("--depth=", 0) == 0) cfg.cycle_depth = std::stoi(arg.substr(8));
                else if (arg == "--depth" && i + 1 < argc) cfg.cycle_depth = std::stoi(argv[++i]);
                else if (arg.rfind("--max-cycles=", 0) == 0) cfg.max_cycles = std::stoi(arg.substr(13));
                else if (arg == "--max-cycles" && i + 1 < argc) cfg.max_cycles = std::stoi(argv[++i]);
                else if (arg == "--require-lm") cfg.require_lm = true;
            }
            EvolutionEngine engine(cfg);
            return engine.runAutonomousMetaFusionResearch(famA, famB, targetFam, cfg.cycle_depth, cfg.max_cycles);
        }
        if (cmd == "evolution-status" && argc >= 3) {
            EvolutionConfig cfg;
            cfg.workspace_path = argv[2];
            for (int i = 3; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg.rfind("--family=", 0) == 0) cfg.family_id = arg.substr(9);
                else if (arg == "--family" && i + 1 < argc) cfg.family_id = argv[++i];
            }
            EvolutionEngine engine(cfg);
            std::cout << engine.generateStatusReport() << "\n";
            return 0;
        }
        if (cmd == "evolution-rollback" && argc >= 3) {
            EvolutionConfig cfg;
            cfg.workspace_path = argv[2];
            int targetGen = -1;
            for (int i = 3; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg.rfind("--family=", 0) == 0) cfg.family_id = arg.substr(9);
                else if (arg == "--family" && i + 1 < argc) cfg.family_id = argv[++i];
                else {
                    try { targetGen = std::stoi(arg); } catch (...) {}
                }
            }
            EvolutionEngine engine(cfg);
            return engine.rollbackGeneration(targetGen) ? 0 : 1;
        }
        if (cmd == "evolution-selftest" && argc >= 3) {
            return EvolutionEngine::runSelftest(argv[2]);
        }
        if ((cmd == "software-research" || cmd == "software-meta-synthesis") && argc >= 6) {
            SoftwareResearchConfig config;
            config.memory_db = argv[2];
            config.workspace = argv[3];
            config.iterations = std::stoi(argv[4]);
            config.seed = std::stoull(argv[5]);
            bool metaSynthesis = (cmd == "software-meta-synthesis");
            for (int i = 6; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg == "--require-lm") config.require_lm_studio = true;
                else if (arg == "--meta-synthesis") metaSynthesis = true;
                else if (arg.rfind("--candidates-db=", 0) == 0) config.candidates_db = arg.substr(16);
                else if (arg == "--candidates-db" && i + 1 < argc) config.candidates_db = argv[++i];
                else if (arg.rfind("--candidate-id=", 0) == 0) config.candidate_id = std::stoll(arg.substr(15));
                else if (arg == "--candidate-id" && i + 1 < argc) config.candidate_id = std::stoll(argv[++i]);
                else if (arg.rfind("--app-type=", 0) == 0) config.application_type = arg.substr(11);
                else if (arg == "--app-type" && i + 1 < argc) config.application_type = argv[++i];
                else if (arg.rfind("--application-type=", 0) == 0) config.application_type = arg.substr(19);
                else if (arg == "--application-type" && i + 1 < argc) config.application_type = argv[++i];
                else if (arg.rfind("--model=", 0) == 0) config.model = arg.substr(8);
                else if (arg == "--model" && i + 1 < argc) config.model = argv[++i];
                else if (config.lm_studio_url.empty()) config.lm_studio_url = arg;
                else if (config.model == "local-coding-model") config.model = arg;
            }
            if (metaSynthesis) {
                return runAutonomousSoftwareSynthesis(config);
            }
            return runAutonomousSoftwareResearch(config);
        }
        if (cmd == "database-first-software-research" && argc >= 9) {
            DatabaseFirstConfig c; c.source_db=argv[2]; c.research_db=argv[3]; c.workspace=argv[4]; c.iterations=std::stoi(argv[5]); c.seed=std::stoull(argv[6]); c.gemini_url=argv[7]; c.model=argv[8];
            int positional=0;
            for(int i=9;i<argc;++i){
                const std::string arg=argv[i];
                if(arg=="--require-gemini"){c.require_gemini=true;c.require_model=true;continue;}
                if(arg=="--require-model"){c.require_model=true;continue;}
                if(arg.rfind("--llm-timeout-ms=",0)==0){c.llm_timeout_ms=static_cast<uint32_t>(std::stoul(arg.substr(17)));continue;}
                if(arg=="--llm-timeout-ms"&&i+1<argc){c.llm_timeout_ms=static_cast<uint32_t>(std::stoul(argv[++i]));continue;}
                if(arg.rfind("--provider=",0)==0){c.provider=arg.substr(11);continue;}
                if(arg=="--provider"&&i+1<argc){c.provider=argv[++i];continue;}
                if(arg.rfind("--lm-studio-url=",0)==0){c.gemini_url=arg.substr(16);continue;}
                if(arg=="--lm-studio-url"&&i+1<argc){c.gemini_url=argv[++i];continue;}
                if(positional==0)c.lean_exe=arg;
                else if(positional==1)c.lean_script=arg;
                else if(positional==2)c.lean_root=arg;
                ++positional;
            }
            return completeDatabaseFirstSoftwareResearch(c);
        }
        if (cmd == "selftest" && argc >= 3) {
            return runSelftest(argv[2]);
        }

        if (cmd == "stats" && argc >= 3) {
            Db db(argv[2], true);
            CliMonitor::printDbStats(db, argv[2]);
            return 0;
        }

        if (cmd == "graph" && argc >= 4) {
            Db src(argv[2], true);
            Db dst(argv[3]);
            TypeGraph tg;
            tg.buildImportGraph(src, dst, true);
            return 0;
        }

        if (cmd == "discover" && argc >= 7) {
            fs::path srcPath = argv[2];
            fs::path dstPath = argv[3];
            int count = std::stoi(argv[4]);
            uint64_t seed = std::stoull(argv[5]);
            GenerationStrategy strat = stringToStrategy(argv[6]);
            std::string domain = (argc >= 8) ? argv[7] : "";

            Db src(srcPath, true);
            Db dst(dstPath);
            TypeGraph tg;
            HypothesisGenerator gen(tg);

            std::cout << "Generating " << count << " candidates using strategy: " << strategyToString(strat) << "...\n";
            auto candidates = gen.generate(src, count, seed, strat, domain, true);
            gen.saveCandidates(dst, candidates);
            std::cout << "Successfully saved " << candidates.size() << " candidates into: " << utf8(dstPath) << "\n";
            return 0;
        }

        if (cmd == "numtest" && argc >= 5) {
            Db db(argv[2]);
            int64_t first_id = std::stoll(argv[3]);
            int count = std::stoi(argv[4]);

            NumericEvaluator evaluator;
            Stmt q(db, "SELECT id, hypothesis_code FROM candidates WHERE id >= " + std::to_string(first_id) +
                       " ORDER BY id LIMIT " + std::to_string(count));

            Stmt upd(db, "UPDATE candidates SET status = ?1, counterexample = ?2, details = ?3 WHERE id = ?4");

            int tested = 0;
            while (q.step()) {
                int64_t id = q.getInt64(0);
                std::string code = q.getText(1);

                auto res = evaluator.testStatement(code, static_cast<uint64_t>(id));
                if (res.verdict == NumericVerdict::Counterexample) {
                    upd.bind(1, std::string("NUMERIC_COUNTEREXAMPLE"));
                    upd.bind(2, res.counterexample_point);
                    upd.bind(3, res.details);
                    upd.bind(4, id);
                    upd.step();
                    upd.reset();
                    std::cout << "Candidate " << id << ": COUNTEREXAMPLE at " << res.counterexample_point << "\n";
                } else if (res.verdict == NumericVerdict::Survived) {
                    upd.bind(1, std::string("NUMERIC_SURVIVED"));
                    upd.bind(2, std::string(""));
                    upd.bind(3, res.details);
                    upd.bind(4, id);
                    upd.step();
                    upd.reset();
                    std::cout << "Candidate " << id << ": SURVIVED " << res.points_tested << " probes\n";
                }
                tested++;
            }
            std::cout << "Tested " << tested << " candidates numerically.\n";
            return 0;
        }

        if (cmd == "export" && argc >= 6) {
            Db db(argv[2], true);
            fs::path dir = argv[3];
            int64_t first_id = std::stoll(argv[4]);
            int count = std::stoi(argv[5]);

            fs::create_directories(dir);
            Stmt q(db, "SELECT id, hypothesis_code, file_a, file_b FROM candidates WHERE id >= " +
                       std::to_string(first_id) + " ORDER BY id LIMIT " + std::to_string(count));

            int exported = 0;
            while (q.step()) {
                int64_t id = q.getInt64(0);
                std::string code = q.getText(1);
                std::string fa = q.getText(2);
                std::string fb = q.getText(3);

                fs::path outPath = dir / ("Candidate_" + std::to_string(id) + ".lean");
                std::ofstream out(outPath, std::ios::binary);
                out << "-- Candidate ID: " << id << "\n";
                if (!fa.empty() && fa.find("OAI") != std::string::npos) out << "import " << fa << "\n";
                if (!fb.empty() && fb != fa && fb.find("OAI") != std::string::npos) out << "import " << fb << "\n";
                out << "\n" << code << "\n";
                exported++;
            }
            std::cout << "Exported " << exported << " Lean candidate files to: " << utf8(dir) << "\n";
            return 0;
        }

        if (cmd == "verify" && argc >= 6) {
            Db db(argv[2]);
            fs::path dir = argv[3];
            int64_t first_id = std::stoll(argv[4]);
            int count = std::stoi(argv[5]);
            fs::path leanPath = (argc >= 7) ? argv[6] : "";

            Verifier verifier(leanPath);
            Stmt q(db, "SELECT id FROM candidates WHERE id >= " + std::to_string(first_id) +
                       " ORDER BY id LIMIT " + std::to_string(count));

            Stmt upd(db, "UPDATE candidates SET status = ?1, verification_exit_code = ?2, "
                         "verification_duration_ms = ?3, error_log = ?4, details = ?5 WHERE id = ?6");

            int verified = 0;
            while (q.step()) {
                int64_t id = q.getInt64(0);
                fs::path leanFile = dir / ("Candidate_" + std::to_string(id) + ".lean");
                fs::path logFile = dir / ("Candidate_" + std::to_string(id) + ".log");

                if (!fs::exists(leanFile)) {
                    std::cout << "Skipping ID " << id << ": source file missing\n";
                    continue;
                }

                auto res = verifier.verifyFile(leanFile, logFile);
                std::string statusStr = verificationStatusToString(res.status);

                upd.bind(1, statusStr);
                upd.bind(2, res.exit_code);
                upd.bind(3, res.duration_ms);
                upd.bind(4, res.error_message);
                upd.bind(5, res.stdout_output.substr(0, 200));
                upd.bind(6, id);
                upd.step();
                upd.reset();

                std::cout << "Candidate " << id << " -> " << statusStr << " (exit=" << res.exit_code
                          << ", duration=" << res.duration_ms << "ms)\n";
                verified++;
            }
            std::cout << "Verified " << verified << " candidates.\n";
            return 0;
        }

        if (cmd == "cycle-k4d-six" && argc >= 7) {
            const int count = std::stoi(argv[5]);
            check(count > 0 && count <= 10000, "count-per-line must be 1..10000");
            const uint64_t seed = std::stoull(argv[6]);
            Db srcDb(argv[2], true);
            Db dstDb(argv[3]);
            const fs::path outDir = argv[4];
            fs::path leanBin = argc >= 8 ? fs::path(argv[7]) : fs::path{};
            std::string mode_filter = argc >= 9 ? argv[8] : "both";
            std::string strat_filter = argc >= 10 ? argv[9] : "all";
            ExperimentRunner runner(leanBin);
            const GenerationStrategy strategies[] = {
                GenerationStrategy::TypeSynthesis, GenerationStrategy::Systematic,
                GenerationStrategy::Random, GenerationStrategy::CrossDomain,
                GenerationStrategy::Mutation, GenerationStrategy::Iterative
            };
            for (const auto strategy : strategies) {
                const auto name = strategyToString(strategy);
                if (strat_filter != "all" && strat_filter != name) continue;
                for (const bool guided : {false, true}) {
                    if ((mode_filter == "k4d" || mode_filter == "1") && !guided) continue;
                    if ((mode_filter == "control" || mode_filter == "0") && guided) continue;
                    const auto line = guided ? "K4D_GUIDED" : "CONTROL_NO_K";
                    const auto result = runner.runCycle(srcDb, dstDb, outDir / name / line,
                        count, seed, strategy, "", true, guided);
                    std::cout << name << " " << line << " generated=" << result.total_candidates
                              << " formally_proven=" << result.formally_proven
                              << " numeric_counterexamples=" << result.numeric_counterexamples << "\n";
                }
            }
            return 0;
        }

        if (cmd == "cycle" && argc >= 8) {
            fs::path src = argv[2];
            fs::path dst = argv[3];
            fs::path outDir = argv[4];
            int count = std::stoi(argv[5]);
            uint64_t seed = std::stoull(argv[6]);
            GenerationStrategy strat = stringToStrategy(argv[7]);

            Db srcDb(src, true);
            Db dstDb(dst);

            fs::path leanBin = argc >= 9 ? fs::path(argv[8]) : fs::path{};
            bool guided = argc >= 10 ? (std::string(argv[9]) == "1" || std::string(argv[9]) == "k4d" || std::string(argv[9]) == "true") : false;

            ExperimentRunner runner(leanBin);
            std::cout << "Starting 12-stage automated research cycle: " << count << " candidates, strategy: "
                      << strategyToString(strat) << ", seed: " << seed
                      << ", line: " << (guided ? "K4D_GUIDED" : "CONTROL_NO_K") << "...\n";

            auto summary = runner.runCycle(srcDb, dstDb, outDir, count, seed, strat, "", true, guided);

            std::cout << "\n=== RESEARCH CYCLE COMPLETED ===\n"
                      << "Total Generated:         " << summary.total_candidates << "\n"
                      << "Formally Proven (Lean): " << summary.formally_proven << "\n"
                      << "Numeric Counterexamples: " << summary.numeric_counterexamples << "\n"
                      << "Numeric Survived:        " << summary.numeric_survived << "\n"
                      << "Proof Failed:            " << summary.proof_failed << "\n"
                      << "Premises Unresolved:     " << summary.premises_unresolved << "\n"
                      << "Elaboration Failed:      " << summary.elaboration_failed << "\n"
                      << "Total Duration:          " << summary.duration_ms << " ms\n"
                      << "================================\n";
            return 0;
        }

        if (cmd == "chain" && argc >= 8) {
            fs::path src = argv[2];
            fs::path dst = argv[3];
            fs::path outDir = argv[4];
            int maxDepth = std::stoi(argv[5]);
            uint64_t seed = std::stoull(argv[6]);
            GenerationStrategy strat = stringToStrategy(argv[7]);
            int64_t startFromId = (argc >= 9 && std::string(argv[8]) != "0" && !std::string(argv[8]).empty()) ? std::stoll(argv[8]) : 0;
            fs::path leanBin = (argc >= 10 && !std::string(argv[9]).empty()) ? fs::path(argv[9]) : fs::path{};
            bool guided = (argc >= 11) ? (std::string(argv[10]) == "1" || std::string(argv[10]) == "k4d" || std::string(argv[10]) == "true") : true;

            Db srcDb(src, true);
            Db dstDb(dst);

            ExperimentRunner runner(leanBin);
            std::cout << "Starting Continuous Research Chain (Target Depth: " << maxDepth
                      << ", Strategy: " << strategyToString(strat) << ", StartFromId: " << startFromId
                      << ", Mode: " << (guided ? "K4D_GUIDED" : "CONTROL_NO_K") << ")...\n";

            auto summary = runner.runResearchChain(srcDb, dstDb, outDir, maxDepth, seed, strat, startFromId, guided);
            return 0;
        }

        if (cmd == "network" && argc >= 8) {
            fs::path src = argv[2];
            fs::path dst = argv[3];
            fs::path outDir = argv[4];
            int maxDepth = std::stoi(argv[5]);
            uint64_t seed = std::stoull(argv[6]);
            GenerationStrategy strat = stringToStrategy(argv[7]);
            int branching = (argc >= 9 && std::string(argv[8]) != "0" && !std::string(argv[8]).empty()) ? std::stoi(argv[8]) : 2;
            int64_t startFromId = (argc >= 10 && std::string(argv[9]) != "0" && !std::string(argv[9]).empty()) ? std::stoll(argv[9]) : 0;
            fs::path leanBin = (argc >= 11 && !std::string(argv[10]).empty()) ? fs::path(argv[10]) : fs::path{};
            bool guided = (argc >= 12) ? (std::string(argv[11]) == "1" || std::string(argv[11]) == "k4d" || std::string(argv[11]) == "true") : true;

            Db srcDb(src, true);
            Db dstDb(dst);

            ExperimentRunner runner(leanBin);
            std::cout << "Starting Branching Research Network (Target Depth: " << maxDepth
                      << ", Branching Factor: " << branching
                      << ", Strategy: " << strategyToString(strat)
                      << ", StartFromId: " << startFromId
                      << ", Mode: " << (guided ? "K4D_GUIDED" : "CONTROL_NO_K") << ")...\n";

            auto summary = runner.runResearchNetwork(srcDb, dstDb, outDir, maxDepth, branching, seed, strat, startFromId, guided);
            return 0;
        }

        if (cmd == "monitor" && argc >= 3) {
            Db db(argv[2], true);
            CliMonitor::renderDashboard(db);
            return 0;
        }

        if (cmd == "inspect" && argc >= 4) {
            Db db(argv[2], true);
            CliMonitor::inspectCandidate(db, std::stoll(argv[3]));
            return 0;
        }

        if (cmd == "export-data" && argc >= 5) {
            Db db(argv[2], true);
            CliMonitor::exportCandidates(db, argv[3], argv[4]);
            return 0;
        }

        printUsage();
        return 2;

    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << "\n";
        return 1;
    }
}

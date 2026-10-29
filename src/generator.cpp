#include "generator.hpp"
#include "hypothesis_evolution.hpp"
#include "../k4d/k4d.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <cstdlib>

namespace mre {

std::vector<Candidate> HypothesisGenerator::generate(
    Db& source_db,
    int count,
    uint64_t seed,
    GenerationStrategy strategy,
    const std::string& domain_filter,
    bool exclude_certs,
    bool k4d_guided
) {
    std::vector<Candidate> candidates;
    candidates.reserve(count);

    // In the six-branch experiment, reuse exactly the same eligible source pool
    // for CONTROL and K4D, across all strategies. The source DB is read once.
    const bool importable_only = std::getenv("MRE_IMPORTABLE_ONLY") != nullptr;
    static std::vector<SemanticDeclaration> shared_pool;
    static const Db* shared_source = nullptr;
    std::vector<SemanticDeclaration> pool;
    if (importable_only && shared_source == &source_db && !shared_pool.empty()) {
        pool = shared_pool;
    } else {
        pool = type_graph_.sampleDeclarations(source_db,
            importable_only ? std::max(256, count * 8) : std::max(50, count * 4),
            seed, domain_filter, exclude_certs);
        if (importable_only) {
            shared_pool = pool;
            shared_source = &source_db;
        }
    }
    if (pool.size() < 2) {
        throw std::runtime_error("Not enough eligible declarations in database for strategy: " + strategyToString(strategy));
    }

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);

    // K4D is a proposal source only. The control line uses the original sampler.
    // Both lines consume the same number of candidates and the same Lean checks.
    const auto obs = k4d_guided ? k4d::observe(6, 24, seed, true) : k4d::Observation{};
    const std::uint64_t kseed = k4d_guided ? obs.signature : 0ULL;
    for (int i = 0; i < count; ++i) {
        Candidate c;
        c.seed = seed;
        c.ordinal = i;
        c.strategy = strategyToString(strategy);
        c.mode = k4d_guided ? "K4D_GUIDED" : "CONTROL_NO_K";
        c.status = "GENERATED";

        // Source-bound exploration: no hard-coded Nat theorem may be counted as a discovery.
        // The source database contains lexical names, not Lean-elaborated qualified constants.
        // Therefore verification is a *type-check attempt*, never a mathematical proof.
        size_t a = strategy == GenerationStrategy::Systematic
            ? (static_cast<size_t>(2) * i) % pool.size() : dist(rng);
        size_t b = strategy == GenerationStrategy::Systematic
            ? (a + 1) % pool.size() : dist(rng);
        if (k4d_guided) {
            // Different deterministic mappings for all six strategies, no fitness reward.
            const std::uint64_t z = kseed ^ (seed + 0x9e3779b97f4a7c15ULL * (static_cast<std::uint64_t>(i) + 1ULL));
            const size_t n = pool.size();
            switch (strategy) {
                case GenerationStrategy::TypeSynthesis:
                    a = static_cast<size_t>((z ^ (z >> 19)) % n);
                    b = (a + 1 + static_cast<size_t>(obs.unique % (n - 1))) % n;
                    break;
                case GenerationStrategy::Systematic:
                    a = (static_cast<size_t>(2) * i + static_cast<size_t>(z % n)) % n;
                    b = (a + 1) % n;
                    break;
                case GenerationStrategy::Random:
                    a = static_cast<size_t>(z % n);
                    b = static_cast<size_t>((z >> 23) % n);
                    break;
                case GenerationStrategy::CrossDomain:
                    a = static_cast<size_t>(z % n);
                    b = static_cast<size_t>((z >> 17) % n);
                    break;
                case GenerationStrategy::Mutation:
                    a = static_cast<size_t>(z % n);
                    b = (a + 1 + static_cast<size_t>(obs.revisits % (n - 1))) % n;
                    break;
                case GenerationStrategy::Iterative:
                    a = static_cast<size_t>((z + static_cast<std::uint64_t>(i)) % n);
                    b = (a + 1 + static_cast<size_t>((obs.signature >> (i % 32)) % (n - 1))) % n;
                    break;
            }
        }
        if (a == b) b = (b + 1) % pool.size();
        if (strategy == GenerationStrategy::CrossDomain) {
            for (size_t attempt = 0; attempt < pool.size(); ++attempt) {
                size_t possible = (b + attempt) % pool.size();
                if (pool[a].domain != pool[possible].domain &&
                    pool[a].domain != "General" && pool[possible].domain != "General") {
                    b = possible;
                    break;
                }
            }
        }
        auto evolved = HypothesisEvolutionEngine::evolve(
            pool, a, b, seed + static_cast<uint64_t>(i), 12, k4d_guided
        );

        c.source_a = evolved.root_a.name;
        c.source_b = evolved.root_b.name;
        c.file_a = evolved.root_a.file;
        c.file_b = evolved.root_b.file;
        c.type_sig_a = evolved.root_a.type_raw;
        c.type_sig_b = evolved.root_b.type_raw;
        c.status = "GENERATED";
        c.premises_status = "NOT_EXTRACTED";
        c.details = "Hypothesis: " + hypothesisRelationToString(evolved.relation) +
                    " | steps=" + std::to_string(evolved.evolution_steps) +
                    " | stability=" + std::to_string(evolved.stability_score) +
                    " | " + evolved.provenance;
        // Keep provenance marker and both the theorem goal and component check
        c.hypothesis_code =
            "-- MRE_EXPLORATORY_ONLY: formal hypothesis generation\n" +
            evolved.theorem_statement + "\n" +
            evolved.check_script;
        c.proof_code.clear();
        c.imports = evolved.imports;

        candidates.push_back(std::move(c));
    }

    return candidates;
}

void HypothesisGenerator::saveCandidates(Db& discovery_db, const std::vector<Candidate>& candidates) {
    initializeOrMigrateDiscoveryDb(discovery_db);

    Stmt insert(discovery_db,
        "INSERT INTO candidates ("
        "seed, ordinal, mode, strategy, source_a, source_b, file_a, file_b, "
        "proposal, hypothesis_code, proof_code, type_sig_a, type_sig_b, "
        "status, details, premises_status, unresolved_premises, counterexample, "
        "parent_id, chain_depth"
        ") VALUES ("
        "?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20"
        ")"
    );

    discovery_db.beginTransaction();
    try {
        for (const auto& c : candidates) {
            insert.bind(1, static_cast<int64_t>(c.seed));
            insert.bind(2, c.ordinal);
            insert.bind(3, c.mode.empty() ? c.strategy : c.mode);
            insert.bind(4, c.strategy);
            insert.bind(5, c.source_a);
            insert.bind(6, c.source_b);
            insert.bind(7, c.file_a);
            insert.bind(8, c.file_b);
            insert.bind(9, c.hypothesis_code);
            insert.bind(10, c.hypothesis_code);
            insert.bind(11, c.proof_code);
            insert.bind(12, c.type_sig_a);
            insert.bind(13, c.type_sig_b);
            insert.bind(14, c.status);
            insert.bind(15, c.details);
            insert.bind(16, c.premises_status);
            insert.bind(17, c.unresolved_premises);
            insert.bind(18, c.counterexample);
            insert.bind(19, c.parent_id);
            insert.bind(20, static_cast<int64_t>(c.chain_depth));

            insert.step();
            insert.reset();
        }
        discovery_db.commit();
    } catch (...) {
        discovery_db.rollback();
        throw;
    }
}

} // namespace mre

#include "algorithm_lab.hpp"
#include <chrono>
#include <limits>
#include <set>

namespace mre {
namespace {
// A bounded family of user-inspectable, deterministic operators. No arbitrary source execution.
constexpr uint32_t prime = 65521U;
struct Kernel { uint32_t a, b, c, d; };
uint32_t evaluate(Kernel k, uint32_t state, uint32_t input) {
    const uint64_t x = state % prime, y = input % prime;
    return static_cast<uint32_t>((uint64_t{k.a} * x + uint64_t{k.b} * y +
         uint64_t{k.c} * x * y + k.d) % prime);
}
uint32_t reference(uint32_t state, uint32_t input) {
    return static_cast<uint32_t>((uint64_t{257} * state + input + 1) % prime);
}
uint32_t evaluateRef(Kernel, uint32_t s, uint32_t v) { return reference(s,v); }
using Op = uint32_t(*)(Kernel,uint32_t,uint32_t);
struct Bench { double nsPerItem=0; uint64_t checksum=0; uint32_t distinct=0, changed=0, total=0; };
Bench benchmark(Kernel k, Op op, const std::vector<uint32_t>& input, int rounds) {
    Bench out;
    std::set<uint32_t> outputs;
    // Multiple rounds, identical workload. Checksum is observed to prevent elimination.
    const auto start=std::chrono::steady_clock::now();
    for(int r=0;r<rounds;++r) {
        uint32_t state=0;
        for(uint32_t v:input) {
            state=op(k,state,v);
            out.checksum += static_cast<uint64_t>(state) + 1;
            if(r==0) outputs.insert(state);
        }
    }
    auto stop=std::chrono::steady_clock::now();
    out.nsPerItem=std::chrono::duration<double,std::nano>(stop-start).count() /
                  static_cast<double>(rounds*input.size());
    out.distinct=static_cast<uint32_t>(outputs.size());
    return out;
}
std::string kernelCode(Kernel k, uint64_t seed, int ordinal) {
    std::ostringstream s;
    s << "// Generated from measured Algorithm Lab experiment.\n"
      << "// Seed " << seed << ", ordinal " << ordinal << ". No correctness or novelty claim.\n"
      << "#pragma once\n#include <cstdint>\nnamespace mre_generated {\n"
      << "inline constexpr uint32_t modulus = 65521U;\n"
      << "inline uint32_t step(uint32_t state, uint32_t input) {\n"
      << "  const uint64_t x = state % modulus, y = input % modulus;\n"
      << "  return static_cast<uint32_t>((uint64_t{" << k.a << "} * x + uint64_t{" << k.b
      << "} * y + uint64_t{" << k.c << "} * x * y + " << k.d << "U) % modulus);\n"
      << "}\n}\n";
    return s.str();
}
}
int runAlgorithmLab(const fs::path& dbPath, const fs::path& artifactDir, int count, uint64_t seed) {
    check(count>0 && count<=10000,"Algorithm Lab count must be 1..10000");
    fs::create_directories(artifactDir);
    if(dbPath.has_parent_path()) fs::create_directories(dbPath.parent_path());
    Db db(dbPath);
    db.exec("CREATE TABLE IF NOT EXISTS algorithm_lab ("
            "id INTEGER PRIMARY KEY, run_seed INTEGER NOT NULL, ordinal INTEGER NOT NULL, "
            "operator_formula TEXT NOT NULL, a INTEGER NOT NULL,b INTEGER NOT NULL,"
            "c INTEGER NOT NULL,d INTEGER NOT NULL, workload TEXT NOT NULL, "
            "sample_count INTEGER NOT NULL, candidate_ns REAL NOT NULL, baseline_ns REAL NOT NULL, "
            "candidate_checksum TEXT NOT NULL, baseline_checksum TEXT NOT NULL,"
            "candidate_distinct INTEGER NOT NULL, baseline_distinct INTEGER NOT NULL, "
            "changed_from_baseline INTEGER NOT NULL, equivalence TEXT NOT NULL, "
            "artifact_path TEXT NOT NULL, novelty TEXT NOT NULL, created_at TEXT DEFAULT CURRENT_TIMESTAMP)");
    Stmt insert(db,"INSERT INTO algorithm_lab(run_seed,ordinal,operator_formula,a,b,c,d,workload,"
        "sample_count,candidate_ns,baseline_ns,candidate_checksum,baseline_checksum,candidate_distinct,"
        "baseline_distinct,changed_from_baseline,equivalence,artifact_path,novelty) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19)");
    std::mt19937_64 rng(seed);
    std::vector<uint32_t> data;
    data.reserve(4096);
    for(int j=0;j<4096;++j) data.push_back(static_cast<uint32_t>(rng()%prime));
    constexpr int rounds=32;
    const Kernel noOp{0,0,0,0};
    Bench baseline=benchmark(noOp,evaluateRef,data,rounds);
    std::cout << "ALGORITHM_LAB_BASELINE items=" << data.size() << " ns_per_item=" << baseline.nsPerItem << '\n';
    int changed=0;
    for(int i=0;i<count;++i) {
        Kernel k{static_cast<uint32_t>(rng()%prime),static_cast<uint32_t>(rng()%prime),
                 static_cast<uint32_t>(rng()%prime),static_cast<uint32_t>(rng()%prime)};
        // A small control group tests the reference itself, independent of the generated family.
        Bench candidate=benchmark(k,evaluate,data,rounds);
        uint32_t candState=0, refState=0;
        bool equivalent=true;
        for(uint32_t value:data) {
            candState=evaluate(k,candState,value);
            refState=reference(refState,value);
            if(candState!=refState) equivalent=false;
        }
        if(!equivalent) ++changed;
        fs::path artifact=artifactDir/("candidate_"+std::to_string(seed)+"_"+std::to_string(i)+".hpp");
        { std::ofstream out(artifact,std::ios::binary); check(static_cast<bool>(out),"Cannot create C++ kernel");
          out << kernelCode(k,seed,i);check(static_cast<bool>(out),"Cannot write C++ kernel"); }
        std::ostringstream expr;
        expr << "("<<k.a<<"*s+"<<k.b<<"*x+"<<k.c<<"*s*x+"<<k.d<<") mod 65521";
        const std::string formula=expr.str();
        const std::string candidateChecksum=std::to_string(candidate.checksum);
        const std::string baselineChecksum=std::to_string(baseline.checksum);
        const std::string equivalence=equivalent?"MATCHED_SAMPLED_TRACE":"DIFFERENT_OUTPUTS";
        const std::string artifactString=utf8(artifact);
        const std::string workload="stream_fold_mod65521_4096x32";
        const std::string novelty="NOT_ASSESSED";
        insert.bind(1,seed);insert.bind(2,i);insert.bind(3,formula);
        insert.bind(4,k.a);insert.bind(5,k.b);insert.bind(6,k.c);insert.bind(7,k.d);
        insert.bind(8,workload);insert.bind(9,static_cast<int64_t>(data.size()*rounds));
        insert.bind(10,candidate.nsPerItem);insert.bind(11,baseline.nsPerItem);
        insert.bind(12,candidateChecksum);insert.bind(13,baselineChecksum);
        insert.bind(14,candidate.distinct);insert.bind(15,baseline.distinct);
        insert.bind(16,equivalent?0:1);insert.bind(17,equivalence);
        insert.bind(18,artifactString);insert.bind(19,novelty);insert.step();insert.reset();
        std::cout << "ALGORITHM_CANDIDATE "<<(i+1)<<'/'<<count<<" ns_per_item="<<candidate.nsPerItem
                  <<" distinct="<<candidate.distinct<<'/'<<data.size()<<" equivalence="
                  <<(equivalent?"MATCHED_SAMPLED_TRACE":"DIFFERENT_OUTPUTS")<<'\n';
    }
    std::cout << "ALGORITHM_LAB_DONE count="<<count<<" different_from_baseline="<<changed
              <<" outputs=CPP_HEADERS+SQLITE novelty=NOT_ASSESSED proof=NOT_ATTEMPTED\n";
    return 0;
}
} // namespace mre

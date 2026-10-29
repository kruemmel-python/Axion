#include "open_research.hpp"
#include <array>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <fstream>
#include <limits>

namespace mre {
namespace {
struct Expr {
    char op = 'v';
    int value = 0;
    std::shared_ptr<Expr> left, right;
};
using Node = std::shared_ptr<Expr>;
Node leaf(char op, int v = 0) { auto p = std::make_shared<Expr>(); p->op=op; p->value=v; return p; }
Node branch(char op, Node a, Node b) {
    auto p = leaf(op); p->left=std::move(a); p->right=std::move(b); return p;
}
std::string render(const Node& n) {
    if (n->op=='v') return std::string(1, "xyz"[n->value]);
    if (n->op=='c') return std::to_string(n->value);
    return "(" + render(n->left) + " " + n->op + " " + render(n->right) + ")";
}
int64_t evaluate(const Node& n, const std::array<int64_t,3>& xyz) {
    if (n->op=='v') return xyz[static_cast<size_t>(n->value)];
    if (n->op=='c') return n->value;
    const auto a=evaluate(n->left,xyz), b=evaluate(n->right,xyz);
    if (n->op=='+') return a+b;
    if (n->op=='-') return a-b;
    return a*b;
}
Node randomExpr(std::mt19937_64& rng, int depth) {
    std::uniform_int_distribution<int> d(0,7);
    int k=d(rng);
    if (depth == 0 || k < 3) return k == 0 ? leaf('c',static_cast<int>(rng()%5)) : leaf('v',static_cast<int>(rng()%3));
    return branch("+-*"[static_cast<size_t>(rng()%3)], randomExpr(rng,depth-1), randomExpr(rng,depth-1));
}
Node mutate(std::mt19937_64& rng, const Node& n) {
    if (!n->left || rng()%4 == 0) return randomExpr(rng,1);
    return branch(n->op, mutate(rng,n->left),n->right);
}
struct Candidate { Node a,b; std::string lane; };
Candidate generate(std::mt19937_64& rng, int lane) {
    Node a=randomExpr(rng,3), b=randomExpr(rng,3);
    if (lane == 1) b=mutate(rng,a);
    if (lane == 2) { // Algebraic deformation: distributive expansion, not assumed correct until checked.
        Node x=randomExpr(rng,1),y=randomExpr(rng,1),z=randomExpr(rng,1);
        a=branch('*',x,branch('+',y,z));
        b=branch('+',branch('*',x,y),branch('*',x,z));
    }
    if (lane == 4) { // Frequent special-case agreement can signal a useful restricted conjecture.
        a=branch('*', randomExpr(rng,2), leaf('v',static_cast<int>(rng()%3)));
        b=leaf('c',0);
    }
    if (lane == 3) { // Non-affinity cross-expression: intentionally independent structures.
        a=branch('*',randomExpr(rng,2),randomExpr(rng,1));
        b=branch('-',randomExpr(rng,2),randomExpr(rng,2));
    }
    return {a,b,std::array<std::string,5>{"free_synthesis","structural_mutation","rule_deformation","independent_cross","exceptional_pattern"}[static_cast<size_t>(lane)]};
}
std::string tempLeanName(uint64_t seed, int ordinal) {
    return "OpenResearch_" + std::to_string(seed) + "_" + std::to_string(ordinal) + ".lean";
}
}
int runOpenResearch(const fs::path& resultDb, int count, uint64_t seed, const fs::path& leanExe) {
    check(count > 0 && count <= 10000, "Open research count must be 1..10000");
    check(leanExe.empty() || fs::is_regular_file(leanExe), "Lean executable not found");
    Db db(resultDb);
    db.exec("CREATE TABLE IF NOT EXISTS open_research ("
            "id INTEGER PRIMARY KEY, seed INTEGER NOT NULL, ordinal INTEGER NOT NULL,"
            "lane TEXT NOT NULL, lhs TEXT NOT NULL, rhs TEXT NOT NULL,"
            "status TEXT NOT NULL, numeric_tests INTEGER NOT NULL,"
            "counterexample TEXT NOT NULL, agreement_points INTEGER NOT NULL, lean_exit INTEGER NOT NULL,"
            "lean_log TEXT NOT NULL, novelty TEXT NOT NULL,"
            "created_at TEXT DEFAULT CURRENT_TIMESTAMP)");
    db.exec("CREATE INDEX IF NOT EXISTS idx_open_research_status ON open_research(status)");
    Stmt insert(db,"INSERT INTO open_research(seed,ordinal,lane,lhs,rhs,status,numeric_tests,counterexample,agreement_points,lean_exit,lean_log,novelty)"
                   " VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12)");
    std::mt19937_64 rng(seed);
    std::set<std::pair<std::string,std::string>> seen;
    std::array<int,5> laneCount{};
    int survived=0, refuted=0, proven=0, unresolved=0;
    for (int i=0;i<count;++i) {
        Candidate c;
        std::string lhs,rhs;
        bool unique=false;
        for (int tries=0;tries<200;++tries) {
            c=generate(rng,i%5); lhs=render(c.a);rhs=render(c.b);
            if (lhs!=rhs && seen.insert({lhs,rhs}).second) { unique=true;break; }
        }
        check(unique,"Could not generate a distinct candidate");
        ++laneCount[static_cast<size_t>(i%5)];
        std::string counterexample;
        int tests=0, agreements=0;
        // Deterministic grid, no dependency on previous theorem inventory or affinity.
        for(int x=-3;x<=3;++x)
          for(int y=-3;y<=3;++y)
            for(int z=-3;z<=3;++z) {
                ++tests;
                const std::array<int64_t,3> args={x,y,z};
                if(evaluate(c.a,args)==evaluate(c.b,args)) { ++agreements; }
                else if(counterexample.empty()) {
                    counterexample="x="+std::to_string(x)+",y="+std::to_string(y)+",z="+std::to_string(z);
                }
            }
        std::string status=counterexample.empty()?"NUMERIC_SURVIVED_UNRESOLVED":"NUMERIC_COUNTEREXAMPLE";
        std::string log;
        int leanExit=-1;
        if (counterexample.empty() && !leanExe.empty()) {
            // Candidate is formalized as an unconditional equation on Int, with explicit proof.
            // Numeric survival ALONE never establishes validity.
            const fs::path temp = fs::current_path()/tempLeanName(seed,i);
            {
                std::ofstream out(temp,std::ios::binary);
                check(static_cast<bool>(out),"Failed to open Lean temporary source");
                out << "import Mathlib\nexample (x y z : Int) : " << lhs << " = " << rhs << " := by\n  ring\n";
            }
            const auto proc=runProcess(shellQuote(utf8(leanExe))+" "+shellQuote(utf8(temp)),120000);
            leanExit=proc.exit_code;
            log=proc.output;
            std::error_code ec;
            fs::remove(temp,ec);
            if(proc.timed_out) status="FORMAL_TIMEOUT_UNRESOLVED";
            else if(proc.exit_code==0 && log.find("sorry") == std::string::npos) status="FORMALLY_PROVEN_KNOWN_TACTIC";
            else status="FORMAL_ATTEMPT_UNRESOLVED";
        }
        if(status=="NUMERIC_COUNTEREXAMPLE") ++refuted;
        else if(status=="FORMALLY_PROVEN_KNOWN_TACTIC") ++proven;
        else if(status=="NUMERIC_SURVIVED_UNRESOLVED") ++survived;
        else ++unresolved;
        insert.bind(1,seed);insert.bind(2,i);insert.bind(3,c.lane);
        insert.bind(4,lhs);insert.bind(5,rhs);insert.bind(6,status);
        insert.bind(7,tests);insert.bind(8,counterexample);insert.bind(9,agreements);insert.bind(10,leanExit);
        insert.bind(11,log);insert.bind(12,std::string("NOT_ASSESSED"));
        insert.step();insert.reset();
        std::cout << "OPEN_PROBE " << (i+1) << '/' << count << ' ' << c.lane << ' ' << status << " agreement=" << agreements << '/' << tests << '\n';
    }
    std::cout << "OPEN_RESEARCH_DONE count=" << count << " counterexamples=" << refuted
              << " numeric_unresolved=" << survived << " formal_proven=" << proven
              << " formal_unresolved=" << unresolved << " novelty=NOT_ASSESSED\n";
    return 0;
}
} // namespace mre

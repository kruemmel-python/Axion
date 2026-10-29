#include "open_structures.hpp"
#include <array>
#include <random>
#include <set>
#include <sstream>

namespace mre {
namespace {
struct Elem { int64_t a = 0, b = 0; };
struct Operator {
    int domain = 0; // 0 finite modular ring, 1 integer vector pair, 2 finite Boolean pair
    int modulus = 5;
    int alpha = 0, beta = 0, gamma = 0, delta = 0;
    int law = 0;
};
int64_t mod(int64_t a, int p) { auto r = a % p; return r < 0 ? r + p : r; }
Elem apply(const Operator& o, Elem x, Elem y) {
    const int64_t a = x.a + y.a + static_cast<int64_t>(o.alpha)*x.a*y.a + o.beta;
    const int64_t b = x.b + y.b + static_cast<int64_t>(o.gamma)*x.b*y.b + static_cast<int64_t>(o.delta)*x.a*y.a;
    if (o.domain == 0) return {mod(a,o.modulus),0};
    if (o.domain == 1) return {a,b};
    // F2 x F2; nonstandard coupled operator depending on the first coordinates
    return {mod(a,2),mod(b,2)};
}
std::string show(Elem x, int domain) {
    if(domain == 0) return std::to_string(x.a);
    return "("+std::to_string(x.a)+","+std::to_string(x.b)+")";
}
std::string rule(const Operator& o) {
    std::ostringstream s;
    s << "a'=x.a+y.a+" << o.alpha << "*x.a*y.a+" << o.beta;
    if (o.domain != 0) s << "; b'=x.b+y.b+" << o.gamma << "*x.b*y.b+" << o.delta << "*x.a*y.a";
    if (o.domain == 0) s << " (mod " << o.modulus << ")";
    if (o.domain == 2) s << " (mod 2 on each coordinate)";
    return s.str();
}
std::string lawName(int law) {
    if (law == 0) return "associativity";
    if (law == 1) return "commutativity";
    return "zero_is_identity";
}
std::string domainName(int d) {
    if(d==0) return "ZMod";
    if(d==1) return "IntegerPair";
    return "BooleanPair";
}
std::string parameters(const Operator& o) {
    std::ostringstream s;
    s << "alpha="<<o.alpha<<",beta="<<o.beta<<",gamma="<<o.gamma<<",delta="<<o.delta<<",modulus="<<o.modulus;
    return s.str();
}
std::vector<Elem> samples(const Operator& o) {
    std::vector<Elem> out;
    if (o.domain == 0) { for (int a=0;a<o.modulus;++a) out.push_back({a,0}); }
    if (o.domain == 1) { for (int a=-2;a<=2;++a) for(int b=-2;b<=2;++b) out.push_back({a,b}); }
    if (o.domain == 2) { for (int a=0;a<2;++a) for(int b=0;b<2;++b) out.push_back({a,b}); }
    return out;
}
struct TestResult { int64_t tests=0, matches=0; std::string counterexample; };
TestResult test(const Operator& op) {
    TestResult r;
    auto elems=samples(op);
    for(const auto& x:elems) for(const auto& y:elems) {
        const size_t n = op.law == 0 ? elems.size() : 1;
        for(size_t j=0;j<n;++j) {
            const Elem z = op.law == 0 ? elems[j] : Elem{};
            Elem lhs, rhs;
            if (op.law == 0) { lhs=apply(op,apply(op,x,y),z); rhs=apply(op,x,apply(op,y,z)); }
            else if(op.law==1) { lhs=apply(op,x,y);rhs=apply(op,y,x); }
            else { lhs=apply(op,x,Elem{});rhs=x; }
            ++r.tests;
            if(lhs.a==rhs.a && lhs.b==rhs.b) ++r.matches;
            else if(r.counterexample.empty()) {
                r.counterexample="x="+show(x,op.domain)+",y="+show(y,op.domain);
                if(op.law==0) r.counterexample+=",z="+show(z,op.domain);
                r.counterexample+=",lhs="+show(lhs,op.domain)+",rhs="+show(rhs,op.domain);
            }
        }
    }
    return r;
}
}
int runOpenStructures(const fs::path& resultDb, int count, uint64_t seed) {
    check(count>0 && count<=10000,"Open structures count must be 1..10000");
    Db db(resultDb);
    db.exec("CREATE TABLE IF NOT EXISTS open_structures ("
            "id INTEGER PRIMARY KEY, seed INTEGER NOT NULL, ordinal INTEGER NOT NULL,"
            "domain TEXT NOT NULL, law TEXT NOT NULL, operator_rule TEXT NOT NULL,"
            "parameters TEXT NOT NULL, status TEXT NOT NULL, trials INTEGER NOT NULL,"
            "matches INTEGER NOT NULL, counterexample TEXT NOT NULL, novelty TEXT NOT NULL,"
            "proof_status TEXT NOT NULL, created_at TEXT DEFAULT CURRENT_TIMESTAMP)");
    db.exec("CREATE INDEX IF NOT EXISTS idx_open_structures_status ON open_structures(status)");
    Stmt add(db,"INSERT INTO open_structures(seed,ordinal,domain,law,operator_rule,parameters,status,trials,matches,counterexample,novelty,proof_status) "
                "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12)");
    std::mt19937_64 rng(seed);
    std::set<std::string> seen;
    int refuted=0, survived=0;
    for(int i=0;i<count;++i) {
        Operator op;
        bool unique=false;
        for(int attempt=0;attempt<1000;++attempt) {
            op.domain=i%3;
            op.modulus=std::array<int,3>{3,5,7}[static_cast<size_t>(rng()%3)];
            op.alpha=static_cast<int>(rng()%5)-2;
            op.beta=static_cast<int>(rng()%5)-2;
            op.gamma=static_cast<int>(rng()%5)-2;
            op.delta=static_cast<int>(rng()%5)-2;
            op.law=(i/3)%3;
            std::string identity=domainName(op.domain)+":"+lawName(op.law)+":"+parameters(op);
            if(seen.insert(identity).second) {unique=true;break;}
        }
        check(unique,"Unique structure generation exhausted");
        auto result=test(op);
        const std::string status=result.counterexample.empty()?"FINITE_TESTS_SURVIVED_UNRESOLVED":"NUMERIC_COUNTEREXAMPLE";
        if(result.counterexample.empty()) ++survived; else ++refuted;
        const std::string domainLabel = domainName(op.domain);
        const std::string lawLabel = lawName(op.law);
        const std::string operatorRule = rule(op);
        const std::string operatorParameters = parameters(op);
        const std::string novelty = "NOT_ASSESSED";
        const std::string proof = "NOT_ATTEMPTED";
        add.bind(1,seed);add.bind(2,i);add.bind(3,domainLabel);
        add.bind(4,lawLabel);add.bind(5,operatorRule);add.bind(6,operatorParameters);
        add.bind(7,status);add.bind(8,result.tests);add.bind(9,result.matches);
        add.bind(10,result.counterexample);add.bind(11,novelty);
        add.bind(12,proof);add.step();add.reset();
        std::cout<<"STRUCTURE_PROBE "<<(i+1)<<'/'<<count<<' '<<domainName(op.domain)<<' '<<lawName(op.law)
                 <<' '<<status<<" matches="<<result.matches<<'/'<<result.tests<<'\n';
    }
    std::cout<<"OPEN_STRUCTURES_DONE count="<<count<<" counterexamples="<<refuted
             <<" unresolved="<<survived<<" novelty=NOT_ASSESSED formal_proofs=0\n";
    return 0;
}
} // namespace mre

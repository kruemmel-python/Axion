#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
namespace {
constexpr uint32_t modulus=65521U;
struct Kernel {uint32_t a{},b{},c{},d{};};
struct Sample {uint32_t input{},state{};bool event{};};
struct Counts {unsigned tp{},fp{},fn{};};
struct Trial {std::vector<Sample> rows;};
struct Evaluated {std::string name,kind,verdict;double candidateF1{},baselineF1{},uplift{};unsigned trainCount{},testCount{};};
void demand(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
std::string load(const fs::path& p){std::ifstream f(p,std::ios::binary);demand(bool(f),"Cannot open "+p.string());return {std::istreambuf_iterator<char>(f),{}};}
void save(const fs::path& p,const std::string& value){std::ofstream f(p,std::ios::binary);demand(bool(f),"Cannot write "+p.string());f<<value;demand(bool(f),"Write failed "+p.string());}
uint32_t advance(const Kernel& k,uint32_t s,uint32_t x){return static_cast<uint32_t>((uint64_t{k.a}*(s%modulus)+uint64_t{k.b}*(x%modulus)+uint64_t{k.c}*(s%modulus)*(x%modulus)+k.d)%modulus);}
Kernel parse(const fs::path& p){
 const auto s=load(p);
 const std::regex r(R"(uint64_t\{([0-9]+)\}\s*\*\s*x\s*\+\s*uint64_t\{([0-9]+)\}\s*\*\s*y\s*\+\s*uint64_t\{([0-9]+)\}\s*\*\s*x\s*\*\s*y\s*\+\s*([0-9]+)U)");
 std::smatch m;demand(std::regex_search(s,m,r),"Unsupported generated kernel in "+p.string());
 std::array<uint32_t,4> a{};for(size_t i=0;i<a.size();++i){const auto v=std::stoull(m[i+1].str());demand(v<modulus,"Invalid kernel coefficient");a[i]=static_cast<uint32_t>(v);}return {a[0],a[1],a[2],a[3]};
}
uint32_t next(uint32_t& state){state=state*1664525U+1013904223U;return state;}
uint32_t distance(uint32_t a,uint32_t b){return a>=b?a-b:b-a;}
Trial generate(const Kernel& k,unsigned seed,unsigned length){
 uint32_t rng=seed*747796405U+2891336453U;
 uint32_t s=0;
 Trial trial;trial.rows.reserve(length);
 // Fixed periodic signal with moderate measurement jitter; sparse injected large changes.
 // Ground truth is generated independently of both algorithms.
 const uint32_t period=4U+(next(rng)%9U);
 std::vector<uint32_t> pattern(period);
 for(uint32_t& value:pattern)value=10000U+(next(rng)%25000U);
 for(unsigned i=0;i<length;++i){
  const bool event=(i>=16U && next(rng)%19U==0U);
  const int jitter=static_cast<int>(next(rng)%201U)-100;
  int v=static_cast<int>(pattern[i%period])+jitter;
  if(event)v+=((next(rng)&1U)==0U?1:-1)*static_cast<int>(6000U+next(rng)%11000U);
  v=std::max(0,std::min(65520,v));const uint32_t x=static_cast<uint32_t>(v);
  s=advance(k,s,x);trial.rows.push_back({x,s,event});
 }
 return trial;
}
// Fixed, simple feature families. Training picks a threshold without touching the holdout.
uint32_t feature(const Trial& t,size_t i,bool candidate,bool periodic){
 if(i==0)return 0;
 if(periodic){
  if(i<12)return 65520U;
  return distance(candidate?t.rows[i].state:t.rows[i].input,candidate?t.rows[i-12].state:t.rows[i-12].input);
 }
 return distance(candidate?t.rows[i].state:t.rows[i].input,candidate?t.rows[i-1].state:t.rows[i-1].input);
}
Counts count(const std::vector<Trial>& trials,bool candidate,bool periodic,uint32_t threshold){
 Counts c;
 for(const auto& t:trials)for(size_t i=1;i<t.rows.size();++i){
  const bool flagged=feature(t,i,candidate,periodic)>threshold;
  if(flagged&&t.rows[i].event)++c.tp;
  if(flagged&&!t.rows[i].event)++c.fp;
  if(!flagged&&t.rows[i].event)++c.fn;
 }
 return c;
}
double f1(Counts c){const double den=2.0*c.tp+c.fp+c.fn;return den==0.0?0.0:2.0*c.tp/den;}
uint32_t tune(const std::vector<Trial>& trials,bool candidate,bool periodic){
 std::vector<uint32_t> values;for(const auto& t:trials)for(size_t i=1;i<t.rows.size();++i)values.push_back(feature(t,i,candidate,periodic));
 std::sort(values.begin(),values.end());values.erase(std::unique(values.begin(),values.end()),values.end());
 uint32_t best=0;double score=-1.0;
 for(uint32_t threshold:values){double metric=f1(count(trials,candidate,periodic,threshold));if(metric>score+1e-12){best=threshold;score=metric;}}
 return best;
}
std::string decimal(double v){std::ostringstream s;s<<std::fixed<<std::setprecision(6)<<v;return s.str();}
Evaluated evaluate(const fs::path& dir){
 const auto name=dir.filename().string();
 const bool periodic=name.find("_pattern_explorer")!=std::string::npos;
 const std::string kind=periodic?"pattern_explorer":name.find("_change_detector")!=std::string::npos?"change_detector":"stream_analyzer";
 const Kernel k=parse(dir/"kernel.hpp");
 std::vector<Trial> train,test;
 for(unsigned i=0;i<12;++i)train.push_back(generate(k,100U+i,256));
 for(unsigned i=0;i<12;++i)test.push_back(generate(k,10000U+i,256));
 const auto cThreshold=tune(train,true,periodic),bThreshold=tune(train,false,periodic);
 const auto cTrain=f1(count(train,true,periodic,cThreshold));
 const auto bTrain=f1(count(train,false,periodic,bThreshold));
 const auto cTest=f1(count(test,true,periodic,cThreshold));
 const auto bTest=f1(count(test,false,periodic,bThreshold));
 const double uplift=cTest-bTest;
 // Conservatively require a material improvement on the holdout.
 const std::string verdict=uplift>=0.05&&cTest>=0.35?"PROMISING_ON_SYNTHETIC_HOLDOUT":"NO_VERIFIED_GAIN";
 std::ostringstream report;
 report<<"{\n  \"schema\": 2,\n  \"benchmark\": \"seeded_synthetic_anomaly_detection_v1\",\n"
   <<"  \"application\": \""<<name<<"\",\n  \"kind\": \""<<kind<<"\",\n"
   <<"  \"train_sequences\": 12,\n  \"holdout_sequences\": 12,\n  \"length_per_sequence\": 256,\n"
   <<"  \"candidate_training_f1\": "<<decimal(cTrain)<<",\n  \"baseline_training_f1\": "<<decimal(bTrain)<<",\n"
   <<"  \"candidate_holdout_f1\": "<<decimal(cTest)<<",\n  \"baseline_holdout_f1\": "<<decimal(bTest)<<",\n"
   <<"  \"f1_uplift\": "<<decimal(uplift)<<",\n  \"candidate_threshold\": "<<cThreshold<<",\n  \"baseline_threshold\": "<<bThreshold<<",\n"
   <<"  \"decision\": \""<<verdict<<"\",\n  \"caveat\": \"Artificial anomaly labels; no real-world superiority, novelty or correctness implied.\"\n}\n";
 save(dir/"evaluation.json",report.str());
 return {name,kind,verdict,cTest,bTest,uplift,12,12};
}
int selftest(){
 const Kernel k{12,34,56,78};demand(advance(k,0,0)==78,"mod arithmetic");
 auto a=generate(k,100,256),b=generate(k,100,256);demand(a.rows.size()==256,"trial length");
 for(size_t i=0;i<a.rows.size();++i)demand(a.rows[i].input==b.rows[i].input&&a.rows[i].state==b.rows[i].state&&a.rows[i].event==b.rows[i].event,"deterministic experiment");
 demand(f1({1,0,0})==1.0,"f1 perfect");demand(f1({0,1,1})==0.0,"f1 absent");
 demand(count({a},false,false,65520).tp==0,"high threshold");
 std::cout<<"APPLICATION_EVALUATOR_SELFTEST PASS\n";return 0;
}
int run(const fs::path& root){
 demand(fs::is_directory(root),"Discovery run directory not found");
 std::vector<Evaluated> results;
 for(const auto& entry:fs::directory_iterator(root))if(entry.is_directory()&&fs::exists(entry.path()/"kernel.hpp"))results.push_back(evaluate(entry.path()));
 demand(!results.empty(),"No generated programs found");
 std::sort(results.begin(),results.end(),[](const auto& a,const auto& b){if(a.uplift!=b.uplift)return a.uplift>b.uplift;return a.name<b.name;});
 std::ostringstream out;out<<"rank,application,kind,candidate_holdout_f1,baseline_holdout_f1,f1_uplift,decision\n";
 unsigned rank=0,promising=0;
 for(const auto& r:results){if(r.verdict=="PROMISING_ON_SYNTHETIC_HOLDOUT")++promising;
  out<<++rank<<','<<r.name<<','<<r.kind<<','<<decimal(r.candidateF1)<<','<<decimal(r.baselineF1)<<','<<decimal(r.uplift)<<','<<r.verdict<<'\n';
  std::cout<<"EVALUATED "<<r.name<<" candidate_f1="<<decimal(r.candidateF1)<<" baseline_f1="<<decimal(r.baselineF1)<<" verdict="<<r.verdict<<'\n';
 }
 save(root/"evaluation_ranking.csv",out.str());
 std::ostringstream summary;summary<<"# Application Discovery Evaluation\n\n"
   <<"Evaluated programs: "<<results.size()<<". Promising on synthetic holdout: "<<promising<<".\n\n"
   <<"Benchmark: 12 independent deterministic training streams and 12 disjoint seeded test streams per program, each 256 samples.\n"
   <<"Parameters were selected exclusively on training streams. Baselines use input difference; candidates use generated state difference.\n"
   <<"For pattern explorers both are compared at lag 12. This is a deliberately narrow proxy task and NOT proof of useful pattern discovery.\n"
   <<"The benchmark measures injected event discrimination only, and does not establish real-world usefulness or mathematical novelty.\n\n"
   <<"See evaluation_ranking.csv and each application/evaluation.json.\n";
 save(root/"EVALUATION_REPORT.md",summary.str());
 std::cout<<"EVALUATION_COMPLETE applications="<<results.size()<<" promising="<<promising<<" destination="<<root.string()<<'\n';return 0;
}
}
int main(int argc,char**argv){try{
 if(argc==2&&std::string(argv[1])=="selftest")return selftest();
 if(argc==3&&std::string(argv[1])=="evaluate")return run(argv[2]);
 std::cerr<<"Usage: math_application_evaluator selftest | evaluate <generated_run_dir>\n";return 2;
}catch(const std::exception& e){std::cerr<<"EVALUATION_ERROR "<<e.what()<<'\n';return 1;}}

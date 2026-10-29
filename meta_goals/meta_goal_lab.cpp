#ifdef _WIN32
#include "../memory/win_sqlite_memory_api.hpp"
#else
#include <sqlite3.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
namespace {
using V=std::array<double,8>;
void check(bool b,const std::string& m){if(!b)throw std::runtime_error(m);}
void save(const fs::path& f,const std::string& s){fs::create_directories(f.parent_path());std::ofstream o(f,std::ios::binary);check(bool(o),"Cannot write "+f.string());o<<s;check(bool(o),"Write failure "+f.string());}
struct Rule {unsigned lhs=0,rhs=1,transform=0,weight=1;};
struct Candidate{Rule rule{};double selection=-1e9,holdout=0,reference=0;unsigned generation=0;};
// Grammar: a new research target is a measurable relationship between independently observed data features.
// Meta-evolution selects WHICH relationship/complexity/robustness criterion should be pursued.
double feature(const V& v,unsigned id){double s=0,mx=v[0],mn=v[0];for(double x:v){s+=x;mx=std::max(mx,x);mn=std::min(mn,x);}switch(id){case 0:return v[0];case 1:return v[7];case 2:return s/8.0;case 3:return mx;case 4:return mn;case 5:return v[7]-v[0];case 6:return (v[0]+v[1]+v[2]+v[3])/4.0;default:return (v[4]+v[5]+v[6]+v[7])/4.0;}}
double value(const V& v,const Rule& r){const double a=feature(v,r.lhs),b=feature(v,r.rhs);switch(r.transform){case 0:return (a+b)*0.5;case 1:return std::abs(a-b);case 2:return std::max(a,b);case 3:return std::min(a,b);default:return (3*a+b)/4.0;}}
V sample(std::mt19937& rng,unsigned domain,bool shift){std::uniform_real_distribution<double> u(0,1);V r{};double last=u(rng);for(unsigned i=0;i<r.size();++i){double z=u(rng);switch(domain){case 0:r[i]=z;break;case 1:r[i]=std::clamp(last+(z-0.5)*(shift?0.9:0.18),0.0,1.0);break;case 2:r[i]=((i%2)==0?0.12:0.85)+0.1*(z-0.5);break;default:r[i]=std::clamp((shift?0.2:0.65)*z+0.3*static_cast<double>(i)/7.0,0.0,1.0);break;}last=r[i];}return r;}
// Operational utility independent of an invented 'ground truth': informative variance,
// nonredundancy against a naive reference, stability across domain shift and cost penalty.
// These are explicit experimental preferences, NOT a proof that a tool is useful to humans.
struct Metrics{double diversity=0,nonredundancy=0,stability=0,objective=0;};
Metrics measure(const Rule& rule,unsigned domain,unsigned seed,bool shift){std::mt19937 rng(seed);const unsigned n=320;double mean=0,m2=0,md=0,m2d=0,cross=0,err=0;for(unsigned i=0;i<n;++i){auto v=sample(rng,domain,shift);double y=value(v,rule),ref=feature(v,0);double d=y-mean;mean+=d/static_cast<double>(i+1);m2+=d*(y-mean);double dr=ref-md;md+=dr/static_cast<double>(i+1);m2d+=dr*(ref-md);cross+=(y-ref)*(y-ref);err+=std::abs(y-ref);}double variance=m2/n;double varianceRef=m2d/n;Metrics m;m.diversity=std::min(1.0,variance*20.0);m.nonredundancy=std::min(1.0,(err/n)*4.0);m.stability=1.0-std::min(1.0,std::abs(variance-varianceRef));m.objective=0.42*m.diversity+0.38*m.nonredundancy+0.20*m.stability-0.01*static_cast<double>(rule.weight);(void)cross;return m;}
double criterion(const Rule& r,unsigned seed,bool shifted){double sum=0;for(unsigned domain=0;domain<4;++domain)sum+=measure(r,domain,seed+domain*9931U,shifted).objective;return sum/4.0;}
Rule mutate(Rule r,std::mt19937& rng){switch(rng()%4U){case 0:r.lhs=rng()%8U;break;case 1:r.rhs=rng()%8U;break;case 2:r.transform=rng()%5U;break;default:r.weight=1U+rng()%5U;break;}return r;}
std::string signature(const Rule& r){return std::to_string(r.lhs)+"_"+std::to_string(r.rhs)+"_"+std::to_string(r.transform)+"_"+std::to_string(r.weight);}
void generate(const fs::path& dir,const Candidate& candidate){fs::create_directories(dir);const Rule r=candidate.rule;std::ostringstream code;code<<"#pragma once\n#include <algorithm>\n#include <array>\n#include <cmath>\nnamespace generated {\nusing Row=std::array<double,8>;\ninline double feature(const Row& v,unsigned id){double sum=0,mx=v[0],mn=v[0];for(double x:v){sum+=x;mx=std::max(mx,x);mn=std::min(mn,x);}switch(id){case 0:return v[0];case 1:return v[7];case 2:return sum/8.0;case 3:return mx;case 4:return mn;case 5:return v[7]-v[0];case 6:return (v[0]+v[1]+v[2]+v[3])/4.0;default:return (v[4]+v[5]+v[6]+v[7])/4.0;}}\ninline double evaluate(const Row& v){const double a=feature(v,"<<r.lhs<<"U),b=feature(v,"<<r.rhs<<"U);";
switch(r.transform){case 0:code<<"return (a+b)*0.5;";break;case 1:code<<"return std::abs(a-b);";break;case 2:code<<"return std::max(a,b);";break;case 3:code<<"return std::min(a,b);";break;default:code<<"return (3*a+b)/4.0;";}code<<"}\n}\n";save(dir/"generated.hpp",code.str());
std::mt19937 rng(2941);std::ostringstream csv;csv<<std::setprecision(17);for(unsigned i=0;i<256;++i){auto v=sample(rng,i%4,i%3==0);for(unsigned j=0;j<v.size();++j)csv<<(j?",":"")<<v[j];csv<<'\n';}save(dir/"example_input.csv",csv.str());
std::ostringstream main;main<<R"CPP(#include "generated.hpp"
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
int main(int argc,char** argv){try{
 if(argc==2&&std::string(argv[1])=="--selftest"){
  const generated::Row r={0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8};
  const double a=generated::evaluate(r);
  if(!std::isfinite(a)||std::abs(a-)CPP"<<std::setprecision(17)<<value(V{0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8},r)<<R"CPP()>1e-10)return 3;
  std::cout<<"SELFTEST PASS\n";return 0;
 }
 if(argc!=3){std::cerr<<"Usage: discovered_app input.csv output.csv\n";return 2;}
 std::ifstream in(argv[1]);if(!in)throw std::runtime_error("Cannot open input");
 std::ofstream out(argv[2]);if(!out)throw std::runtime_error("Cannot open output");
 out.precision(17);out<<"row,value\n";std::string line;unsigned count=0;
 while(std::getline(in,line)){
  if(line.empty())continue;
  generated::Row v{};std::istringstream ss(line);std::string token;
  for(double& x:v){if(!std::getline(ss,token,','))throw std::runtime_error("Missing column");
   std::size_t used=0;x=std::stod(token,&used);if(used!=token.size()||!std::isfinite(x))throw std::runtime_error("Bad number");}
  if(std::getline(ss,token,','))throw std::runtime_error("Extra column");
  out<<count<<','<<generated::evaluate(v)<<'\n';if(++count>1000000U)throw std::runtime_error("Row limit");
 }
 if(!out)throw std::runtime_error("Write failure");
 std::cout<<"PROCESSED "<<count<<" rows\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
)CPP";save(dir/"main.cpp",main.str());
save(dir/"CMakeLists.txt","cmake_minimum_required(VERSION 3.21)\nproject(meta_discovered_app LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\nset(CMAKE_CXX_STANDARD_REQUIRED ON)\nadd_executable(discovered_app main.cpp)\nif(MSVC)\n target_compile_options(discovered_app PRIVATE /W4 /WX /permissive- /EHsc)\nelse()\n target_compile_options(discovered_app PRIVATE -Wall -Wextra -Wpedantic -Werror)\nendif()\nenable_testing()\nadd_test(NAME generated_selftest COMMAND discovered_app --selftest)\n");
std::ostringstream spec;spec<<"language MRE_META_1\nvector f64 length=8\ninput csv\nrule lhs="<<r.lhs<<" rhs="<<r.rhs<<" operator="<<r.transform<<" cost="<<r.weight<<"\nmetric informative_variance=0.42 nonredundancy=0.38 stability=0.20\noutput csv\n";save(dir/"application.mremeta",spec.str());}
void selftest(){Rule a{1,2,0,1};check(signature(a)=="1_2_0_1","signature");check(std::abs(value(V{0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8},a)-0.625)<1e-10,"interpretation");check(std::abs(criterion(a,400,false)-criterion(a,400,false))<1e-12,"determinism");std::cout<<"META_GOAL_SELFTEST PASS\n";}
struct MetaMemory {
 sqlite3* db=nullptr;
 explicit MetaMemory(const fs::path& path){
  fs::create_directories(path.parent_path());
  check(sqlite3_open(path.string().c_str(),&db)==SQLITE_OK,"Cannot open meta memory");
  const char* schema="PRAGMA busy_timeout=5000; CREATE TABLE IF NOT EXISTS meta_trials(signature TEXT PRIMARY KEY,lhs INTEGER,rhs INTEGER,transform INTEGER,cost INTEGER,score REAL NOT NULL,visits INTEGER NOT NULL DEFAULT 1);";
  check(sqlite3_exec(db,schema,nullptr,nullptr,nullptr)==SQLITE_OK,"Meta memory schema failed");
 }
 ~MetaMemory(){if(db)sqlite3_close(db);}
 MetaMemory(const MetaMemory&)=delete;MetaMemory& operator=(const MetaMemory&)=delete;
 std::vector<Rule> bestRules(unsigned limit){
  sqlite3_stmt* st=nullptr;std::vector<Rule> rules;
  check(sqlite3_prepare_v2(db,"SELECT lhs,rhs,transform,cost FROM meta_trials ORDER BY score DESC,signature LIMIT ?1",-1,&st,nullptr)==SQLITE_OK,"Meta memory query");
  sqlite3_bind_int(st,1,static_cast<int>(limit));
  while(sqlite3_step(st)==SQLITE_ROW){rules.push_back(Rule{static_cast<unsigned>(sqlite3_column_int(st,0)),static_cast<unsigned>(sqlite3_column_int(st,1)),static_cast<unsigned>(sqlite3_column_int(st,2)),static_cast<unsigned>(sqlite3_column_int(st,3))});}
  sqlite3_finalize(st);return rules;
 }
 void record(const Rule& r,double score){
  sqlite3_stmt* st=nullptr;
  const char* sql="INSERT INTO meta_trials(signature,lhs,rhs,transform,cost,score) VALUES(?1,?2,?3,?4,?5,?6) ON CONFLICT(signature) DO UPDATE SET score=MAX(score,excluded.score),visits=visits+1";
  check(sqlite3_prepare_v2(db,sql,-1,&st,nullptr)==SQLITE_OK,"Meta memory insert");
  const auto sig=signature(r);sqlite3_bind_text(st,1,sig.c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_int(st,2,static_cast<int>(r.lhs));sqlite3_bind_int(st,3,static_cast<int>(r.rhs));sqlite3_bind_int(st,4,static_cast<int>(r.transform));sqlite3_bind_int(st,5,static_cast<int>(r.weight));sqlite3_bind_double(st,6,score);
  const int rc=sqlite3_step(st);sqlite3_finalize(st);check(rc==SQLITE_DONE,"Meta memory write");
 }
};
void search(const fs::path& root,unsigned budget,MetaMemory* memory=nullptr,unsigned explorePercent=30){check(budget>=8&&budget<=100000,"Budget must be 8..100000");fs::create_directories(root);std::mt19937 rng(314159);std::vector<Candidate> population;
 std::vector<Rule> remembered=memory?memory->bestRules(24):std::vector<Rule>{};
 unsigned rememberedUsed=0,explored=0;
 std::ostringstream history;history<<"trial,generation,lhs,rhs,transform,cost,selection_score\n";Candidate best;unsigned stagnation=0;for(unsigned i=0;i<budget;++i){Rule r;
 // Fixed exploration quota prevents the experience prior from monopolizing the search.
 const bool choosePrior=memory&&!remembered.empty()&&(i%100U >= explorePercent)&&(i>=8U);
 if(choosePrior){r=mutate(remembered[rememberedUsed%remembered.size()],rng);++rememberedUsed;}
 else if(i<8){r={i,i==7?0U:i+1U,i%5U,1U};}else if(i%5==0||population.empty()){r={static_cast<unsigned>(rng()%8U),static_cast<unsigned>(rng()%8U),static_cast<unsigned>(rng()%5U),static_cast<unsigned>(1U+rng()%5U)};}else{r=mutate(population[rng()%population.size()].rule,rng);}
 if(!choosePrior){++explored;}
 if(r.lhs==r.rhs){r.rhs=(r.rhs+1U)%8U;}
 bool duplicate=false;for(const auto& p:population){if(signature(p.rule)==signature(r)){duplicate=true;break;}}if(duplicate)continue;Candidate c;c.rule=r;c.generation=i;c.selection=criterion(r,102001,false);history<<i<<','<<i/16<<','<<r.lhs<<','<<r.rhs<<','<<r.transform<<','<<r.weight<<','<<c.selection<<'\n';population.push_back(c);if(memory)memory->record(r,c.selection);std::sort(population.begin(),population.end(),[](const Candidate& a,const Candidate& b){return a.selection>b.selection;});if(population.size()>24)population.resize(24);if(c.selection>best.selection+1e-9){best=c;stagnation=0;}else ++stagnation;if(stagnation>=std::min(250U,std::max(32U,budget/2)))break;}
check(!population.empty(),"No candidates");save(root/"search_history.csv",history.str());std::ostringstream ranking;ranking<<"rank,lhs,rhs,transform,cost,validation_score,holdout_shift_score,reference_score,verdict\n";std::vector<Candidate> chosen;for(unsigned i=0;i<std::min<size_t>(3,population.size());++i){auto c=population[i];c.holdout=criterion(c.rule,900017,true);Rule reference{0,1,0,1};c.reference=criterion(reference,900017,true);const bool gain=c.holdout>c.reference+0.03;ranking<<i+1<<','<<c.rule.lhs<<','<<c.rule.rhs<<','<<c.rule.transform<<','<<c.rule.weight<<','<<c.selection<<','<<c.holdout<<','<<c.reference<<','<<(gain?"GAIN_VS_NAIVE_REFERENCE":"NO_VERIFIED_GAIN")<<'\n';chosen.push_back(c);generate(root/("application_"+std::to_string(i+1)),c);}save(root/"ranking.csv",ranking.str());std::ostringstream report;report<<"# Meta-goal evolution v6\n\nTrials evaluated: "<<population.size()<<" surviving in elite pool (search_history.csv contains all evaluated proposals).\nStop condition: budget or stagnation.\nThe search evolves RULES for what to measure (feature relationships), not just kernels. Each experiment selects a feature pair and mathematical relationship, then uses fixed explicit utility weights (variance, nonredundancy, shift stability, cost).\nTraining/selection seed=102001, final holdout seed=900017 with changed data distributions; holdout is not used for mutation.\nThree applications are generated even if none show verified gain. Experience-guided proposals used: "<<rememberedUsed<<"; exploratory proposals: "<<explored<<". Memory stores SELECTION scores only, never final holdout scores.\n\n**Limits:** These are machine-defined *proxy goals*, not independently established human needs or evidence of mathematical novelty. The outer utility weights and primitive grammar are still engineered. Repeated runs on the same holdout would invalidate its independence. Generated applications must be sandboxed when extended to untrusted inputs.\n";save(root/"RESEARCH_REPORT.md",report.str());std::cout<<"META_GOAL_COMPLETE candidates="<<population.size()<<" applications="<<chosen.size()<<" destination="<<root.string()<<'\n';}
}
int main(int argc,char** argv){try{if(argc==2&&std::string(argv[1])=="selftest"){selftest();return 0;}if(argc==4&&std::string(argv[1])=="discover"){search(argv[2],static_cast<unsigned>(std::stoul(argv[3])));return 0;}
 if(argc==6&&std::string(argv[1])=="discover_memory"){
  MetaMemory memory(argv[4]);const unsigned explore=static_cast<unsigned>(std::stoul(argv[5]));check(explore>=10&&explore<=90,"Explore percentage 10..90");
  search(argv[2],static_cast<unsigned>(std::stoul(argv[3])),&memory,explore);return 0;
 }std::cerr<<"Usage: math_meta_goal_lab selftest | discover destination budget | discover_memory destination budget memory.sqlite explorePercent\n";return 2;}catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}

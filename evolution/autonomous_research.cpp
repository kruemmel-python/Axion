#include "k4d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
namespace {
constexpr uint32_t mod=65521U;
struct K {uint32_t a{},b{},c{},d{};};
struct Row {uint32_t x{},state{};bool event{};bool operator==(const Row&) const = default;};
struct Job {std::string id;uint32_t lag{};bool useState{};bool circular{};};
struct Score {double train{},validation{},baseline{},finalCandidate{},finalBaseline{};uint32_t threshold{};};
void ensure(bool ok,const std::string& s){if(!ok)throw std::runtime_error(s);}
uint32_t rngStep(uint32_t& r){r=r*1664525U+1013904223U;return r;}
uint32_t advance(const K& k,uint32_t s,uint32_t x){uint64_t a=s%mod,b=x%mod;return static_cast<uint32_t>((uint64_t{k.a}*a+uint64_t{k.b}*b+uint64_t{k.c}*a*b+k.d)%mod);}
uint32_t diff(uint32_t a,uint32_t b,bool circular){uint32_t d=a>b?a-b:b-a;return circular?std::min(d,mod-d):d;}
std::string load(const fs::path& p){std::ifstream f(p,std::ios::binary);ensure(bool(f),"Cannot read "+p.string());return {std::istreambuf_iterator<char>(f),{}};}
void write(const fs::path& p,const std::string& s){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);ensure(bool(f),"Cannot write "+p.string());f<<s;ensure(bool(f),"Write failure");}
K parse(const fs::path& p){auto s=load(p);const std::regex r(R"(uint64_t\{([0-9]+)\}\s*\*\s*x\s*\+\s*uint64_t\{([0-9]+)\}\s*\*\s*y\s*\+\s*uint64_t\{([0-9]+)\}\s*\*\s*x\s*\*\s*y\s*\+\s*([0-9]+)U)");std::smatch m;ensure(std::regex_search(s,m,r),"Unsupported kernel: "+p.string());std::array<uint32_t,4> v{};for(size_t i=0;i<4;++i){auto n=std::stoull(m[i+1].str());ensure(n<mod,"Bad coefficient");v[i]=static_cast<uint32_t>(n);}return {v[0],v[1],v[2],v[3]};}
std::string header(K k){std::ostringstream o;o<<"#pragma once\n#include <cstdint>\nnamespace mre_generated {\ninline uint32_t step(uint32_t state,uint32_t input){const uint64_t x=state%65521U,y=input%65521U;return static_cast<uint32_t>((uint64_t{"<<k.a<<"}*x+uint64_t{"<<k.b<<"}*y+uint64_t{"<<k.c<<"}*x*y+"<<k.d<<"U)%65521U);}\n}\n";return o.str();}
std::vector<Row> data(K k,uint32_t seed,unsigned n){uint32_t r=seed*747796405U+2891336453U,s=0;uint32_t period=4U+rngStep(r)%9U;std::vector<uint32_t> pattern(period);for(auto& v:pattern)v=10000U+rngStep(r)%25000U;std::vector<Row> out;out.reserve(n);for(unsigned i=0;i<n;++i){bool e=i>=16U&&rngStep(r)%19U==0U;int x=static_cast<int>(pattern[i%period])+static_cast<int>(rngStep(r)%201U)-100;if(e)x+=((rngStep(r)&1U)==0U?1:-1)*static_cast<int>(6000U+rngStep(r)%11000U);x=std::clamp(x,0,65520);s=advance(k,s,static_cast<uint32_t>(x));out.push_back({static_cast<uint32_t>(x),s,e});}return out;}
uint32_t feature(const std::vector<Row>& v,size_t i,const Job& j){if(i<j.lag)return 0;auto x=j.useState?v[i].state:v[i].x;auto y=j.useState?v[i-j.lag].state:v[i-j.lag].x;return diff(x,y,j.circular);}
struct Counts{uint32_t tp{},fp{},fn{};};
double metric(const std::vector<std::vector<Row>>& all,const Job& j,uint32_t threshold){Counts c;for(auto& seq:all)for(size_t i=j.lag;i<seq.size();++i){bool hit=feature(seq,i,j)>threshold;bool e=seq[i].event;if(hit&&e)++c.tp;else if(hit)++c.fp;else if(e)++c.fn;}double n=2.0*c.tp+c.fp+c.fn;return n?2.0*c.tp/n:0.0;}
uint32_t optimizeThreshold(const std::vector<std::vector<Row>>& all,const Job& j){std::vector<uint32_t> values;for(auto& s:all)for(size_t i=j.lag;i<s.size();++i)values.push_back(feature(s,i,j));std::sort(values.begin(),values.end());values.erase(std::unique(values.begin(),values.end()),values.end());uint32_t best=0;double bestV=-1;for(auto v:values){double q=metric(all,j,v);if(q>bestV+1e-12){bestV=q;best=v;}}return best;}
std::vector<std::vector<Row>> dataset(K k,uint32_t seed){std::vector<std::vector<Row>> out;for(unsigned i=0;i<8;++i)out.push_back(data(k,seed+i,256));return out;}
std::string fmt(double v){std::ostringstream o;o<<std::fixed<<std::setprecision(6)<<v;return o.str();}
// Each candidate application type is constructed from an operator (lag, metric, signal-space) tuple.
// No fixed whitelist of application names determines the search space.
std::vector<Job> jobs(){std::vector<Job> out;for(uint32_t lag:{1U,2U,3U,4U,6U,8U,12U,16U})for(bool circular:{false,true})for(bool useState:{true}){Job j;j.lag=lag;j.circular=circular;j.useState=useState;j.id=std::string(useState?"state":"input")+"_lag"+std::to_string(lag)+(circular?"_ring":"_absolute");out.push_back(j);}return out;}
struct Item{K k;Job j;uint32_t threshold{};double train{},valid{},baseline{};std::string lineage;};
Item evaluate(K k,Job j,const std::string& lineage){auto train=dataset(k,100U),valid=dataset(k,10000U);uint32_t t=optimizeThreshold(train,j);double a=metric(train,j,t),b=metric(valid,j,t);Job ref=j;ref.useState=false;uint32_t rt=optimizeThreshold(train,ref);double c=metric(valid,ref,rt);return {k,j,t,a,b,c,lineage};}
bool better(const Item& a,const Item& b){double av=a.valid-a.baseline,bv=b.valid-b.baseline;if(std::abs(av-bv)>1e-12)return av>bv;if(std::abs(a.valid-b.valid)>1e-12)return a.valid>b.valid;return a.j.id<b.j.id;}
K mutate(K p,uint32_t& rng){std::array<uint32_t*,4> a{&p.a,&p.b,&p.c,&p.d};uint32_t idx=rngStep(rng)%4U;uint32_t m=rngStep(rng)%5U;uint32_t n=m==0?1U:m==1?17U:m==2?257U:m==3?4093U:(rngStep(rng)%mod);uint32_t old=*a[idx];*a[idx]=(m==4?n:(rngStep(rng)&1U)?(old+n)%mod:(old+mod-n)%mod);return p;}
std::string description(const Job& j){std::ostringstream o;o<<"Automatically composed signal feature: "<<(j.useState?"generated recurrent state":"original input")<<", lag="<<j.lag<<", distance="<<(j.circular?"modular circular":"absolute")<<". Experimental event-detection application, not a verified novel use case.";return o.str();}
void application(const fs::path& root,const Item& best){fs::path dst=root/"selected_application";fs::create_directories(dst);write(dst/"kernel.hpp",header(best.k));std::ostringstream src;src<<R"CPP(#include "kernel.hpp"
#include "k4d.hpp"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
int main(int argc,char** argv){try{
 if(argc==2&&std::string(argv[1])=="--selftest"){
   if(mre_generated::step(1,2)>=65521U)return 3;
   std::cout<<"SELFTEST PASS\n";return 0;
 }
 if(argc!=3){std::cerr<<"Usage: discovered_app input.txt output.csv\n";return 2;}
 std::ifstream in(argv[1]);if(!in)throw std::runtime_error("Input missing");
 std::vector<uint32_t> input;std::string line;
 while(std::getline(in,line)){if(line.empty())continue;size_t pos=0;auto n=std::stoul(line,&pos);if(pos!=line.size()||n>=65521U)throw std::runtime_error("Invalid integer");if(input.size()>=1000000U)throw std::runtime_error("Too many values");input.push_back(static_cast<uint32_t>(n));}
 if(input.empty())throw std::runtime_error("Empty input");
 std::ofstream out(argv[2]);if(!out)throw std::runtime_error("Output unavailable");
 std::vector<uint32_t> values;values.reserve(input.size());uint32_t state=0;
 for(uint32_t x:input){state=mre_generated::step(state,x);values.push_back()CPP";
 src<<(best.j.useState?"state":"x")<<R"CPP();}
 out<<"index,input,feature,flagged\n";
 for(size_t i=0;i<input.size();++i){uint32_t f=0;
)CPP";
 src<<"  if(i>="<<best.j.lag<<"U){uint32_t x=values[i],y=values[i-"<<best.j.lag<<"U];uint32_t d=x>=y?x-y:y-x;f="<<(best.j.circular?"std::min(d,65521U-d)":"d")<<";}\n";
 src<<"  out<<i<<','<<input[i]<<','<<f<<','<<(f>"<<best.threshold<<"U?1:0)<<'\\n';}\n";
 src<<R"CPP( if(!out)throw std::runtime_error("Write failure");
 std::cout<<"PROCESSED "<<input.size()<<" samples\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
)CPP";
 write(dst/"main.cpp",src.str());write(dst/"CMakeLists.txt",R"CM(cmake_minimum_required(VERSION 3.21)
project(DiscoveredEvolutionApp LANGUAGES CXX)
add_executable(discovered_app main.cpp)
set_target_properties(discovered_app PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
if(MSVC)
 target_compile_options(discovered_app PRIVATE /W4 /WX /permissive- /EHsc)
else()
 target_compile_options(discovered_app PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
enable_testing()
add_test(NAME generated_selftest COMMAND discovered_app --selftest)
)CM");
 std::ostringstream input;for(unsigned i=0;i<256;++i)input<<(i*137U+53U)%mod<<'\n';write(dst/"example_input.txt",input.str());write(dst/"APPLICATION.md","# Generated application\n\n"+description(best.j)+"\n\nThreshold optimized on training data. Candidate selected on validation data. Final audit never used for selection.\n");}
int run(fs::path source,fs::path output,unsigned rounds,unsigned population,bool useK4D=false){ensure(rounds>0&&rounds<=100&&population>0&&population<=256,"Budget out of bounds");fs::create_directories(output);std::vector<fs::path> inputs;for(auto& e:fs::directory_iterator(source))if(e.is_regular_file()&&e.path().extension()==".hpp")inputs.push_back(e.path());std::sort(inputs.begin(),inputs.end());ensure(!inputs.empty(),"No source kernels");
 std::vector<Job> all=jobs();std::vector<Item> leaders;std::set<std::array<uint32_t,4>> visited;uint32_t rng=0x6172C1A5U;std::ostringstream history;history<<"round,parent,feature,train_f1,validation_f1,baseline_validation_f1,uplift,a,b,c,d\n";
 std::ostringstream k4dEvidence;k4dEvidence<<"round,slot,branch,seed,signature,activity,intervention_effect,unique,revisits,proposal_a,proposal_b,proposal_c,proposal_d\n";
 auto push=[&](K k,const std::string& parent,unsigned round){auto identity=std::array<uint32_t,4>{k.a,k.b,k.c,k.d};if(!visited.insert(identity).second)return;Item local{};bool has=false;for(const auto& j:all){Item t=evaluate(k,j,parent);if(!has||better(t,local)){local=t;has=true;}}ensure(has,"No hypotheses");leaders.push_back(local);history<<round<<','<<parent<<','<<local.j.id<<','<<fmt(local.train)<<','<<fmt(local.valid)<<','<<fmt(local.baseline)<<','<<fmt(local.valid-local.baseline)<<','<<k.a<<','<<k.b<<','<<k.c<<','<<k.d<<'\n';};
 for(size_t i=0;i<std::min<size_t>(inputs.size(),4);++i)push(parse(inputs[i]),inputs[i].stem().string(),0);
 ensure(!leaders.empty(),"No evaluated seeds");std::sort(leaders.begin(),leaders.end(),better);
 unsigned stagnation=0;double bestGain=leaders.front().valid-leaders.front().baseline;
 for(unsigned r=1;r<=rounds;++r){std::vector<Item> elite=leaders;std::sort(elite.begin(),elite.end(),better);size_t pool=std::min<size_t>(elite.size(),4);for(unsigned i=0;i<population;++i){
   const Item& parent=elite[rngStep(rng)%pool];
   if(useK4D){
    // K4D supplies measured 4D dynamics, not a claimed theorem or fitness score.
    // Paired control receives the same parent, budget and evaluation pipeline.
    const std::uint64_t observationSeed=static_cast<std::uint64_t>(r)*1000003ULL+i+314159ULL;
    for(bool coupled:{true,false}){
     const auto observation=mre::k4d::observe(4,12,observationSeed,coupled);
     uint32_t local=static_cast<uint32_t>(observation.signature ^ (observation.signature>>32));
     local^=static_cast<uint32_t>(observation.unique*2654435761ULL);
     local^=static_cast<uint32_t>(observation.revisits*2246822519ULL);
     // Proposal depends on the observed K4D state, but is independently validated.
     const K proposed=mutate(parent.k,local);
     k4dEvidence<<r<<','<<i<<','<<(coupled?"K4D":"CONTROL_NO_K")<<','<<observationSeed<<','<<observation.signature<<','<<fmt(observation.activity)<<','<<fmt(observation.intervention_effect)<<','<<observation.unique<<','<<observation.revisits<<','<<proposed.a<<','<<proposed.b<<','<<proposed.c<<','<<proposed.d<<'\n';
     push(proposed,coupled?"K4D_OBSERVED":"CONTROL_NO_K",r);
    }
   }else push(mutate(parent.k,rng),parent.j.id,r);
  }
  std::sort(leaders.begin(),leaders.end(),better);double nextGain=leaders.front().valid-leaders.front().baseline;if(nextGain>bestGain+1e-8){bestGain=nextGain;stagnation=0;}else ++stagnation;
  std::cout<<"ROUND "<<r<<" candidates="<<leaders.size()<<" best_validation_uplift="<<fmt(bestGain)<<" stagnation="<<stagnation<<'\n';if(stagnation>=5){std::cout<<"STOP validation plateau\n";break;}}
 const Item& win=leaders.front(); // Final audit is held back until selection is frozen.
 auto finalData=dataset(win.k,60000U);auto last=metric(finalData,win.j,win.threshold);
 Job bj=win.j;bj.useState=false;auto baselineTrain=dataset(win.k,100U);uint32_t bt=optimizeThreshold(baselineTrain,bj);double bfinal=metric(finalData,bj,bt);
 bool verified=last-bfinal>=0.05&&last>=0.35;write(output/"search_history.csv",history.str());if(useK4D)write(output/"k4d_evidence.csv",k4dEvidence.str());application(output,win);
 std::ostringstream rep;rep<<"# Autonomous Research Report\n\nSeeds: "<<std::min<size_t>(4,inputs.size())<<"; evaluated unique kernels: "<<leaders.size()<<"; max rounds: "<<rounds<<"; population/round: "<<population<<".\n\n";
 if(useK4D)rep<<"K4D observed-state proposals and paired CONTROL_NO_K proposals were both evaluated with the same validation pipeline. K4D is a proposal source, not a fitness bonus or proof. See k4d_evidence.csv.\n\n";
 rep<<"Selected application: "<<win.j.id<<"\n\n"<<description(win.j)<<"\n\nTraining F1: "<<fmt(win.train)<<"; validation F1: "<<fmt(win.valid)<<"; validation baseline F1: "<<fmt(win.baseline)<<".\n\n";
 rep<<"**Untouched final audit**: candidate F1="<<fmt(last)<<"; baseline F1="<<fmt(bfinal)<<"; uplift="<<fmt(last-bfinal)<<".\n\n";
 rep<<"Decision: "<<(verified?"PROMISING_ON_SYNTHETIC_FINAL_AUDIT":"NO_VERIFIED_GAIN")<<"\n\nOnly fixed synthetic event detection was evaluated; composed feature descriptions are not unconstrained application invention. Finite search, budget exhaustion and plateau are reported, not mathematical impossibility.\n";
 write(output/"AUTONOMOUS_RESEARCH_REPORT.md",rep.str());std::cout<<"FINAL_AUDIT candidate="<<fmt(last)<<" baseline="<<fmt(bfinal)<<" verdict="<<(verified?"PROMISING_ON_SYNTHETIC_FINAL_AUDIT":"NO_VERIFIED_GAIN")<<"\nOUTPUT "<<output.string()<<'\n';return 0;}
int selftest(){K k{12,34,56,78};ensure(advance(k,0,0)==78,"Arithmetic");auto x=data(k,100,256),y=data(k,100,256);ensure(x.size()==256&&x==y,"Determinism");ensure(jobs().size()==16,"Compositional space");auto t=dataset(k,100);auto j=jobs().front();auto threshold=optimizeThreshold(t,j);ensure(threshold<mod,"Threshold");std::cout<<"AUTONOMOUS_RESEARCH_SELFTEST PASS\n";return 0;}
}
int main(int argc,char**argv){try{if(argc==2&&std::string(argv[1])=="selftest")return selftest();if(argc==6&&(std::string(argv[1])=="search"||std::string(argv[1])=="search-k4d"))return run(argv[2],argv[3],static_cast<unsigned>(std::stoul(argv[4])),static_cast<unsigned>(std::stoul(argv[5])),std::string(argv[1])=="search-k4d");std::cerr<<"Usage: math_autonomous_research selftest | search|search-k4d <kernels_dir> <new_output_dir> <rounds> <population>\n";return 2;}catch(const std::exception& e){std::cerr<<"RESEARCH_ERROR "<<e.what()<<'\n';return 1;}}

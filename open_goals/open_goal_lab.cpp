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
using Row=std::vector<std::uint32_t>;
struct Idea{unsigned length{},lag{},operation{},target{},seed{};double train{},validation{},holdout{},baseline{};};
void require(bool yes,const std::string& message){if(!yes)throw std::runtime_error(message);}
void write(const fs::path& p,const std::string& data){fs::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary);require(bool(out),"Cannot write "+p.string());out<<data;require(bool(out),"Write failed: "+p.string());}
std::string targetName(unsigned n){static const std::array<const char*,6> labels={"neighbor_delta","local_mean","local_max","threshold_count","neighbor_xor","rolling_energy"};return labels.at(n);}
std::uint32_t clamp16(std::uint64_t v){return static_cast<std::uint32_t>(std::min<std::uint64_t>(65535U,v));}
std::uint32_t truth(const Row& a,unsigned target){require(!a.empty(),"Empty row");std::uint64_t sum=0;std::uint32_t maxv=0,count=0,mix=0;for(std::size_t i=0;i<a.size();++i){sum+=a[i];maxv=std::max(maxv,a[i]);count+=a[i]>32767U?1U:0U;mix^=a[i];}switch(target){case 0:return a.size()>1?(a.back()>a.front()?a.back()-a.front():a.front()-a.back()):0U;case 1:return static_cast<std::uint32_t>(sum/a.size());case 2:return maxv;case 3:return count;case 4:return mix;default:{std::uint64_t energy=0;for(std::size_t i=1;i<a.size();++i){auto d=a[i]>a[i-1]?a[i]-a[i-1]:a[i-1]-a[i];energy+=d;}return clamp16(energy/a.size());}}}
std::uint32_t estimate(const Row& a,unsigned op,unsigned lag){auto index=a.size()-1;auto p=a[index>=lag?index-lag:0];auto x=a.back();switch(op){case 0:return x;case 1:return (x+p)/2U;case 2:return x>p?x-p:p-x;case 3:return x^p;case 4:return std::max(x,p);case 5:return std::min(x,p);case 6:return (x+p)&65535U;case 7:return (x*p)&65535U;case 8:return x>32767U?1U:0U;default:return (x+2U*p)/3U;}}
std::vector<Row> dataset(unsigned n,unsigned length,std::uint32_t seed){std::mt19937 rng(seed);std::vector<Row> rows;rows.reserve(n);for(unsigned k=0;k<n;++k){Row a;std::uint32_t prev=rng()&65535U;for(unsigned j=0;j<length;++j){std::uint32_t value=rng()&65535U;if(k%4==0)value=(prev+(rng()%4000U))&65535U;else if(k%4==1)value=((j%3==0)?60000U:1000U)+(rng()%300U);else if(k%4==2)value=(j*4500U+rng()%1000U)&65535U;value&=65535U;a.push_back(value);prev=value;}rows.push_back(std::move(a));}return rows;}
double loss(const std::vector<Row>& rows,unsigned target,unsigned op,unsigned lag){double sum=0;for(const auto& r:rows){auto x=truth(r,target),y=estimate(r,op,lag);sum+=std::abs(static_cast<double>(x)-static_cast<double>(y))/65535.0;}return sum/static_cast<double>(rows.size());}
double baseline(const std::vector<Row>& rows,unsigned target){return loss(rows,target,0,1);}
std::string ideaName(const Idea& x){return "goal_"+targetName(x.target)+"_vector"+std::to_string(x.length)+"_op"+std::to_string(x.operation)+"_lag"+std::to_string(x.lag);}
void generate(const fs::path& root,const Idea& idea){fs::create_directories(root);std::ostringstream spec;spec<<"language MRE_GOAL_2\nname "<<ideaName(idea)<<"\ndata vector<u16> length="<<idea.length<<"\ninput csv_rows\noperator "<<idea.operation<<" lag="<<idea.lag<<"\ngoal "<<targetName(idea.target)<<"\nmetric normalized_mae\noutput csv_rows\n";write(root/"application.mre2",spec.str());std::ostringstream hdr;hdr<<"#pragma once\n#include <algorithm>\n#include <cstdint>\n#include <vector>\nnamespace discovered {\ninline std::uint32_t compute(const std::vector<std::uint32_t>& a){const auto x=a.back();const auto p=a[a.size()-1U-std::min<std::size_t>("<<idea.lag<<"U,a.size()-1U)];\n";
 switch(idea.operation){case 0:hdr<<"(void)p;return x;";break;case 1:hdr<<"return (x+p)/2U;";break;case 2:hdr<<"return x>p?x-p:p-x;";break;case 3:hdr<<"return x^p;";break;case 4:hdr<<"return std::max(x,p);";break;case 5:hdr<<"return std::min(x,p);";break;case 6:hdr<<"return (x+p)&65535U;";break;case 7:hdr<<"return (x*p)&65535U;";break;case 8:hdr<<"(void)p;return x>32767U?1U:0U;";break;default:hdr<<"return (x+2U*p)/3U;";break;}hdr<<"\n}\n}\n";write(root/"generated.hpp",hdr.str());
 std::ostringstream main;main<<R"CPP(#include "generated.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
int main(int argc,char** argv){try{
 if(argc==2&&std::string(argv[1])=="--selftest"){
  if(discovered::compute(std::vector<std::uint32_t>{100U,300U})!=)CPP"<<estimate(Row{100U,300U},idea.operation,idea.lag)<<"U) return 3;\n  std::cout<<\"SELFTEST PASS\\n\";return 0;\n }\n"<<R"CPP( if(argc!=3){std::cerr<<"Usage: discovered_app input.csv output.csv\n";return 2;}
 std::ifstream in(argv[1]);if(!in)throw std::runtime_error("Input missing");
 std::ofstream out(argv[2]);if(!out)throw std::runtime_error("Output inaccessible");
 out<<"row,value\n";std::string line;std::size_t count=0;
 while(std::getline(in,line)){
  if(line.empty())continue;
  std::istringstream ss(line);std::string token;std::vector<std::uint32_t> values;
  while(std::getline(ss,token,',')){
   if(token.empty())throw std::runtime_error("Empty item");
   std::size_t used=0;
   const auto n=std::stoull(token,&used);if(used!=token.size()||n>65535U)throw std::runtime_error("Invalid u16");
   values.push_back(static_cast<std::uint32_t>(n));
  }
  if(values.size()!=)CPP"<<idea.length<<"U)throw std::runtime_error(\"Wrong vector length\");\n"<<R"CPP(  out<<count<<','<<discovered::compute(values)<<'\n';
  if(++count>1000000U)throw std::runtime_error("Row limit");
 }
 if(!out)throw std::runtime_error("Output write failed");
 std::cout<<"PROCESSED "<<count<<" rows\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
)CPP";write(root/"main.cpp",main.str());write(root/"CMakeLists.txt","cmake_minimum_required(VERSION 3.20)\nproject(discovered_app LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\nset(CMAKE_CXX_STANDARD_REQUIRED ON)\nadd_executable(discovered_app main.cpp)\nif(MSVC)\n target_compile_options(discovered_app PRIVATE /W4 /WX /permissive- /EHsc)\nelse()\n target_compile_options(discovered_app PRIVATE -Wall -Wextra -Wpedantic -Werror)\nendif()\nenable_testing()\nadd_test(NAME generated_selftest COMMAND discovered_app --selftest)\n");auto data=dataset(256,idea.length,9901U);std::ostringstream csv;for(auto& row:data){for(std::size_t j=0;j<row.size();++j)csv<<(j?",":"")<<row[j];csv<<'\n';}write(root/"example_input.csv",csv.str());std::ostringstream report;report<<"# Generated application\n\nDerived target hypothesis: "<<targetName(idea.target)<<"\nVector length: "<<idea.length<<"\nOperation: "<<idea.operation<<"; lag: "<<idea.lag<<"\nValidation normalized MAE: "<<idea.validation<<"\nFinal holdout normalized MAE: "<<idea.holdout<<"\nFinal baseline MAE: "<<idea.baseline<<"\nVerdict: "<<(idea.holdout+0.001<idea.baseline?"GAIN_VS_NAIVE_BASELINE":"NO_VERIFIED_GAIN")<<"\n\nThis is an automatically generated *synthetic objective*, not an autonomously validated real-world need.\n";write(root/"RESULT.md",report.str());}
void selftest(){require(truth(Row{5,7,8},0)==3,"delta");require(truth(Row{2,4,6},1)==4,"mean");require(truth(Row{2,4,6},2)==6,"max");require(truth(Row{2,50000,65535},3)==2,"count");require(estimate(Row{2,7},2,1)==5,"absdiff");require(dataset(12,4,17)==dataset(12,4,17),"determinism");std::cout<<"OPEN_GOAL_SELFTEST PASS\n";}
void search(const fs::path& root,unsigned budget){require(budget>=1&&budget<=100000,"Budget out of range");fs::create_directories(root);std::ostringstream history;history<<"goal,vector_length,operator,lag,validation_mae,baseline_mae\n";std::ostringstream ranking;ranking<<"goal,vector_length,operator,lag,validation_mae,holdout_mae,baseline_holdout_mae,verdict\n";unsigned evaluated=0;std::vector<Idea> winners;for(unsigned goal=0;goal<6;++goal){for(unsigned length:{2U,4U,8U,16U}){auto valid=dataset(384,length,6001U+goal*23U+length);double best=std::numeric_limits<double>::infinity();Idea chosen{};for(unsigned k=0;k<budget;++k){unsigned op=k%10U,lag=1U+(k/10U)%length;auto score=loss(valid,goal,op,lag);auto ref=baseline(valid,goal);history<<targetName(goal)<<','<<length<<','<<op<<','<<lag<<','<<score<<','<<ref<<'\n';++evaluated;if(score<best){best=score;chosen={length,lag,op,goal,k,0,score,0,0};}}auto held=dataset(512,length,900001U+goal*47U+length);chosen.holdout=loss(held,goal,chosen.operation,chosen.lag);chosen.baseline=baseline(held,goal);ranking<<targetName(goal)<<','<<length<<','<<chosen.operation<<','<<chosen.lag<<','<<chosen.validation<<','<<chosen.holdout<<','<<chosen.baseline<<','<<(chosen.holdout+0.001<chosen.baseline?"GAIN_VS_NAIVE_BASELINE":"NO_VERIFIED_GAIN")<<'\n';winners.push_back(chosen);}}write(root/"search_history.csv",history.str());write(root/"ranking.csv",ranking.str());for(const auto& winner:winners)generate(root/ideaName(winner),winner);std::ostringstream info;info<<"# Open Goal Lab v5 – experiment\n\n"<<evaluated<<" evaluated graph configurations, "<<winners.size()<<" generated vector applications across 6 inferred objective families and 4 vector sizes.\n\nGoals are derived from measurable properties of the data (neighbor distance, mean, max, threshold count, xor and rolling energy), not chosen as real-world needs. Validation data controls selection; untouched holdout data only scores the selected proposal. Baseline: last element of vector. The generated goals are constrained by the evaluator grammar and cannot claim unconstrained invention.\n\nSee ranking.csv, search_history.csv and individual RESULT.md files.\n";write(root/"RESEARCH_REPORT.md",info.str());std::cout<<"OPEN_GOAL_COMPLETE tested="<<evaluated<<" applications="<<winners.size()<<" destination="<<root.string()<<'\n';}
}
int main(int argc,char** argv){try{if(argc==2&&std::string(argv[1])=="selftest"){selftest();return 0;}if(argc==4&&std::string(argv[1])=="discover"){search(argv[2],static_cast<unsigned>(std::stoul(argv[3])));return 0;}std::cerr<<"Usage: math_open_goal_lab selftest | discover output_folder budget_per_goal\n";return 2;}catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}

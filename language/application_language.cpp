#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
namespace {
using Word=std::uint32_t;
struct Op {std::string name,kind,a,b;};
struct Spec {std::string name="untitled",goal="identity",output="result";std::vector<Op> ops;};
struct Sample {Word input{},previous{},state{};};
void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
void put(const fs::path& p,const std::string& body){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);check(bool(f),"Cannot create "+p.string());f<<body;check(bool(f),"Cannot write "+p.string());}
std::string get(const fs::path& p){std::ifstream f(p,std::ios::binary);check(bool(f),"Cannot open "+p.string());return {std::istreambuf_iterator<char>(f),{}};}
std::string trim(const std::string& s){auto a=s.find_first_not_of(" \t\r\n");if(a==std::string::npos)return {};auto b=s.find_last_not_of(" \t\r\n");return s.substr(a,b-a+1);}
const std::set<std::string>& opnames(){static const std::set<std::string> names={"add","sub","xor","min","max","absdiff","avg","and","or","mul","mod","shiftr"};return names;}
const std::set<std::string>& goals(){static const std::set<std::string> names={"identity","delta","midpoint","bounded257","smooth3","state_mix"};return names;}
Word readval(const std::string& arg,const std::map<std::string,Word>& values){auto it=values.find(arg);if(it!=values.end())return it->second;if(arg.rfind("const:",0)==0){auto str=arg.substr(6);check(!str.empty(),"Empty constant");size_t used=0;auto val=std::stoull(str,&used);check(used==str.size()&&val<=65535,"Invalid constant");return static_cast<Word>(val);}throw std::runtime_error("Unknown value: "+arg);}
Word apply(const std::string& op,Word x,Word y){if(op=="add")return (x+y)&65535U;if(op=="sub")return (x-y)&65535U;if(op=="xor")return x^y;if(op=="min")return std::min(x,y);if(op=="max")return std::max(x,y);if(op=="absdiff")return x>=y?x-y:y-x;if(op=="avg")return (x+y)/2U;if(op=="and")return x&y;if(op=="or")return x|y;if(op=="mul")return (x*y)&65535U;if(op=="mod")return y?x%y:0U;if(op=="shiftr")return x>>(y&15U);throw std::runtime_error("Unknown operation: "+op);}
Spec parse(const std::string& content){Spec s;std::set<std::string> known={"input","previous","state"};bool version=false,seenOut=false,seenGoal=false;std::istringstream f(content);std::string line;unsigned lineNum=0;
 while(std::getline(f,line)){++lineNum;auto comment=line.find('#');if(comment!=std::string::npos)line.erase(comment);line=trim(line);if(line.empty())continue;std::istringstream r(line);std::string key;r>>key;
 if(key=="language"){std::string value,extra;r>>value;check(value=="MRE_APP_1"&&!(r>>extra),"Unsupported language at line "+std::to_string(lineNum));version=true;}
 else if(key=="name"){std::string v,extra;r>>v;check(!v.empty()&&!(r>>extra),"Bad name");s.name=v;}
 else if(key=="input"){std::string v,t,extra;r>>v>>t;check(v=="input"&&t=="u16"&&!(r>>extra),"Only input u16 supported");}
 else if(key=="state"){std::string v,t,extra;r>>v>>t;check(v=="state"&&t=="u16"&&!(r>>extra),"Only state u16 supported");}
 else if(key=="op"){Op o;std::string extra;r>>o.name>>o.kind>>o.a>>o.b;check(!o.name.empty()&&!o.kind.empty()&&!o.a.empty()&&!o.b.empty()&&!(r>>extra),"Invalid op arity");check(opnames().count(o.kind)>0,"Unknown operator");check(!known.count(o.name),"Duplicate identifier");check(o.name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")==std::string::npos,"Unsafe identifier");check(o.name.size()<=40,"Identifier too long");static const std::set<std::string> reserved={"auto","class","const","constexpr","default","delete","double","else","enum","false","float","for","if","int","long","namespace","new","operator","private","protected","public","return","short","signed","sizeof","static","struct","switch","template","this","throw","true","try","typedef","union","unsigned","using","virtual","void","volatile","while"};check(!reserved.count(o.name),"Reserved C++ identifier");auto valid=[&](const std::string& a){if(known.count(a))return true;if(a.rfind("const:",0)==0){(void)readval(a,{});return true;}return false;};check(valid(o.a)&&valid(o.b),"Unknown operand in "+o.name);known.insert(o.name);s.ops.push_back(o);check(s.ops.size()<=64,"Too many operations");}
 else if(key=="goal"){std::string v,extra;r>>v;check(goals().count(v)>0&&!(r>>extra),"Unknown goal");s.goal=v;seenGoal=true;}
 else if(key=="output"){std::string v,t,extra;r>>v>>t;check(known.count(v)>0&&t=="u16"&&!(r>>extra),"Invalid output");s.output=v;seenOut=true;}
 else throw std::runtime_error("Unknown directive at line "+std::to_string(lineNum));}
 check(version&&seenOut&&seenGoal,"Missing required language, goal or output declaration");return s;}
Word evaluate(const Spec& s,Sample sample){std::map<std::string,Word> v={{"input",sample.input},{"previous",sample.previous},{"state",sample.state}};for(const auto& o:s.ops)v[o.name]=apply(o.kind,readval(o.a,v),readval(o.b,v));return v.at(s.output);}
Word truth(const std::string& task,Sample s){if(task=="identity")return s.input;if(task=="delta")return s.input>=s.previous?s.input-s.previous:s.previous-s.input;if(task=="midpoint")return (s.input+s.previous)/2U;if(task=="bounded257")return s.input%257U;if(task=="smooth3")return (s.input+s.previous+s.state)/3U;if(task=="state_mix")return s.input^s.state;throw std::runtime_error("Unknown task");}
std::vector<Sample> examples(Word seed){Word r=seed;std::vector<Sample> rows;for(unsigned i=0;i<512;++i){r=r*1664525U+1013904223U;Word x=(r>>8U)&65535U;r=r*1664525U+1013904223U;Word p=(r>>8U)&65535U;r=r*1664525U+1013904223U;Word z=(r>>8U)&65535U;rows.push_back({x,p,z});}return rows;}
double error(const Spec& s,const std::vector<Sample>& rows){double total=0;for(const auto& row:rows){auto a=evaluate(s,row),b=truth(s.goal,row);total+=a>=b?a-b:b-a;}return total/static_cast<double>(rows.size());}
double baseline(const std::string& goal,const std::vector<Sample>& rows){double total=0;for(auto r:rows){auto target=truth(goal,r);total+=r.input>=target?r.input-target:target-r.input;}return total/static_cast<double>(rows.size());}
std::string render(const Spec& s){std::ostringstream out;out<<"language MRE_APP_1\nname "<<s.name<<"\ninput input u16\nstate state u16\n";for(auto& o:s.ops)out<<"op "<<o.name<<' '<<o.kind<<' '<<o.a<<' '<<o.b<<'\n';out<<"goal "<<s.goal<<"\noutput "<<s.output<<" u16\n";return out.str();}
std::string emitted(const Spec& s){std::ostringstream o;o<<"#pragma once\n#include <algorithm>\n#include <cstdint>\nnamespace mre_program {\ninline std::uint32_t compute(std::uint32_t input,std::uint32_t previous,std::uint32_t state){\n";
 for(const auto& op:s.ops){auto v=[&](const std::string& a){return a.rfind("const:",0)==0?(a.substr(6)+"U"):a;};auto a=v(op.a),b=v(op.b);std::string exp;
 if(op.kind=="add")exp="(("+a+"+"+b+")&65535U)";else if(op.kind=="sub")exp="(("+a+"-"+b+")&65535U)";else if(op.kind=="xor")exp="("+a+"^"+b+")";else if(op.kind=="and")exp="("+a+"&"+b+")";else if(op.kind=="or")exp="("+a+"|"+b+")";else if(op.kind=="mul")exp="(("+a+"*"+b+")&65535U)";else if(op.kind=="min")exp="std::min("+a+","+b+")";else if(op.kind=="max")exp="std::max("+a+","+b+")";else if(op.kind=="absdiff")exp="("+a+">="+b+"?"+a+"-"+b+":"+b+"-"+a+")";else if(op.kind=="avg")exp="(("+a+"+"+b+")/2U)";else if(op.kind=="mod")exp="("+b+"?"+a+"%"+b+":0U)";else if(op.kind=="shiftr")exp="("+a+">>("+b+"&15U))";
 o<<" const std::uint32_t "<<op.name<<"="<<exp<<";\n (void)"<<op.name<<";\n";}
 // Avoid /W4 C4100 for unused standard arguments.
 std::string references=s.output;for(auto& op:s.ops)references+=" "+op.a+" "+op.b;
 for(auto& field:{"input","previous","state"})if(references.find(field)==std::string::npos)o<<" (void)"<<field<<";\n";
 o<<" return "<<s.output<<";\n}\n}\n";return o.str();}
void application(const fs::path& root,const Spec& s,double valid,double test,double ref){fs::create_directories(root);put(root/"program.mre",render(s));put(root/"generated.hpp",emitted(s));std::ostringstream main;main<<R"CPP(#include "generated.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
int main(int argc,char** argv){try{
 if(argc==2&&std::string(argv[1])=="--selftest"){
  const auto v=mre_program::compute(100U,80U,20U);
  if(v>65535U)return 3;
  std::cout<<"SELFTEST PASS\n";return 0;
 }
 if(argc!=3){std::cerr<<"Usage: discovered_app input.txt output.csv\n";return 2;}
 std::ifstream in(argv[1]);if(!in)throw std::runtime_error("Cannot open input");
 std::ofstream out(argv[2]);if(!out)throw std::runtime_error("Cannot create output");
 out<<"index,input,previous,state,output\n";
 std::uint32_t previous=0,state=0;std::size_t index=0;std::string line;
 while(std::getline(in,line)){
  if(line.empty())continue;
  std::size_t used=0;auto n=std::stoull(line,&used);
  if(used!=line.size()||n>65535U)throw std::runtime_error("Invalid input value");
  if(index>=1000000U)throw std::runtime_error("Input too long");
  const std::uint32_t x=static_cast<std::uint32_t>(n);
  const std::uint32_t result=mre_program::compute(x,previous,state);
  out<<index<<','<<x<<','<<previous<<','<<state<<','<<result<<'\n';
  previous=x;state=result;++index;
 }
 if(!out)throw std::runtime_error("Output failure");
 std::cout<<"PROCESSED "<<index<<" records\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
)CPP";
 std::string mainCode=main.str();
 std::ostringstream oracle;oracle<<"  if(mre_program::compute(100U,80U,20U)!="<<evaluate(s,{100,80,20})<<"U)return 4;\n";
 oracle<<"  if(mre_program::compute(65535U,2U,7U)!="<<evaluate(s,{65535,2,7})<<"U)return 5;\n";
 oracle<<"  if(mre_program::compute(0U,65535U,256U)!="<<evaluate(s,{0,65535,256})<<"U)return 6;\n";
 auto anchor=mainCode.find("  std::cout<<\"SELFTEST PASS\\n\";return 0;");
 check(anchor!=std::string::npos,"Generated selftest hook missing");mainCode.insert(anchor,oracle.str());
 put(root/"main.cpp",mainCode);put(root/"CMakeLists.txt",R"CM(cmake_minimum_required(VERSION 3.21)
project(MRELanguageGeneratedApp LANGUAGES CXX)
add_executable(discovered_app main.cpp)
set_target_properties(discovered_app PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
if(MSVC)
 target_compile_options(discovered_app PRIVATE /W4 /WX /permissive- /EHsc)
else()
 target_compile_options(discovered_app PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
enable_testing()
add_test(NAME generated_selftest COMMAND discovered_app --selftest)
)CM");std::ostringstream ex;for(unsigned i=0;i<256;++i)ex<<(i*137U+53U)%65536U<<'\n';put(root/"example_input.txt",ex.str());std::ostringstream report;report<<"# Generated MRE application\n\nGoal: "<<s.goal<<"\n\nValidation MAE: "<<valid<<"\nIndependent final MAE: "<<test<<"\nIndependent baseline MAE: "<<ref<<"\n\n"<<(test+1e-9<ref?"ADVANTAGE_VS_INPUT_BASELINE":"NO_VERIFIED_GAIN")<<"\n\nThe only demonstrated property is performance on synthetic data against the explicit input-as-answer baseline. This is not proof of practical benefit or novelty.\n";put(root/"RESULT.md",report.str());}
Spec candidate(const std::string& goal,unsigned a,unsigned b,unsigned c){Spec s;s.name="generated_"+goal;s.goal=goal;static const std::array<std::string,3> fields={"input","previous","state"};static const std::array<std::string,11> operators={"add","sub","xor","min","max","absdiff","avg","and","or","mod","shiftr"};auto rhs=fields[b%3];if(c%5==0)rhs="const:257";else if(c%7==0)rhs="const:3";s.ops.push_back({"n0",operators[a%operators.size()],fields[c%3],rhs});if(a%4==0)s.ops.push_back({"n1",operators[(a/4+b)%operators.size()],"n0",fields[(c+1)%3]});s.output=s.ops.back().name;return s;}
void selftest(){auto s=parse("language MRE_APP_1\nname test\ninput input u16\nstate state u16\nop d absdiff input previous\ngoal delta\noutput d u16\n");check(evaluate(s,{8,3,1})==5,"Interpreter");check(error(s,examples(9))==0,"Exact delta");check(parse(render(s)).ops.size()==1,"Roundtrip");for(auto& n:opnames())check(apply(n,500,3)<=65535,"Operator bound");for(auto bad:{"language MRE_APP_1\ngoal delta\noutput missing u16\n","language MRE_APP_1\nop a add nonexistent input\ngoal delta\noutput a u16\n"}){bool threw=false;try{(void)parse(bad);}catch(const std::exception&){threw=true;}check(threw,"Malformed accepted");}std::cout<<"APPLICATION_LANGUAGE_SELFTEST PASS\n";}
int search(const fs::path& output,unsigned budget){check(budget>=1&&budget<=100000,"Budget out of range");fs::create_directories(output);auto train=examples(17),validation=examples(7001),test=examples(93019);(void)train;std::ostringstream history;history<<"goal,candidate,validation_mae,baseline_validation_mae,accepted\n";unsigned generated=0;std::map<std::string,Spec> winners;std::map<std::string,double> scores;for(auto& goal:goals()){double best=std::numeric_limits<double>::infinity();Spec pick;auto ref=baseline(goal,validation);for(unsigned i=0;i<budget;++i){auto s=candidate(goal,i,i/11,i/7+1);auto value=error(s,validation);if(value<best){best=value;pick=s;}history<<goal<<','<<i<<','<<value<<','<<ref<<','<<(value<ref?1:0)<<'\n';++generated;}winners[goal]=pick;scores[goal]=best;}
 put(output/"search_history.csv",history.str());std::ostringstream ranking;ranking<<"goal,validation_mae,final_mae,baseline_final_mae,verdict\n";for(auto& [goal,s]:winners){auto valid=scores.at(goal),final=error(s,test),ref=baseline(goal,test);ranking<<goal<<','<<valid<<','<<final<<','<<ref<<','<<(final+1e-9<ref?"ADVANTAGE_VS_INPUT_BASELINE":"NO_VERIFIED_GAIN")<<'\n';application(output/("application_"+goal),s,valid,final,ref);}put(output/"ranking.csv",ranking.str());std::ostringstream report;report<<"# Autonomous application language experiment\n\n"<<generated<<" generated operator graphs; "<<winners.size()<<" separately evaluated goals.\n\nAll goal families are explicit and synthetic. New goals and operators may be registered in the C++ source. Unrestricted human-level category invention is not implemented. Training/validation/final split: candidate graphs selected on validation only; final test held out. Strictly limited to unsigned 16-bit scalar streams.\n\nSee ranking.csv and search_history.csv.\n";put(output/"RESEARCH_REPORT.md",report.str());std::cout<<"LANGUAGE_DISCOVERY_COMPLETE graphs="<<generated<<" applications="<<winners.size()<<" destination="<<output.string()<<'\n';return 0;}
}
int main(int argc,char** argv){try{if(argc==2&&std::string(argv[1])=="selftest"){selftest();return 0;}if(argc==4&&std::string(argv[1])=="compile"){auto s=parse(get(argv[2]));application(argv[3],s,0,0,0);std::cout<<"PROGRAM_COMPILED source="<<argv[2]<<'\n';return 0;}if(argc==4&&std::string(argv[1])=="discover")return search(argv[2],static_cast<unsigned>(std::stoul(argv[3])));std::cerr<<"Usage: math_application_language selftest | compile program.mre output | discover output budget\n";return 2;}catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}

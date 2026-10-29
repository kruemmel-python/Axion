#include <algorithm>
#include <array>
#include <cctype>
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
namespace fs = std::filesystem;
namespace {
constexpr uint32_t mod = 65521U;
struct Kernel { uint32_t a{},b{},c{},d{}; fs::path source; };
struct Profile { double diversity{}, collision{}, delta{}, avalanche{}, sensitivity{}; unsigned distinct{}; };
void require(bool ok,const std::string& msg){if(!ok)throw std::runtime_error(msg);}
std::string read(const fs::path& p){std::ifstream in(p,std::ios::binary);require(static_cast<bool>(in),"Cannot open: "+p.string());return {std::istreambuf_iterator<char>(in),{}};}
void write(const fs::path& p,const std::string& value){fs::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary);require(static_cast<bool>(out),"Cannot write: "+p.string());out<<value;require(static_cast<bool>(out),"Write failed: "+p.string());}
uint32_t step(const Kernel& k,uint32_t s,uint32_t x){uint64_t a=s%mod,b=x%mod;return static_cast<uint32_t>((uint64_t{k.a}*a+uint64_t{k.b}*b+uint64_t{k.c}*a*b+k.d)%mod);}
uint32_t ref(uint32_t s,uint32_t x){return static_cast<uint32_t>((uint64_t{257}*s+x+1)%mod);}
std::string basename(const fs::path& p){return p.stem().string();}
std::string cleanName(std::string n){for(char& c:n)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='_')c='_';if(n.empty())n="kernel";return n;}
Kernel parse(const fs::path& file){
    const std::string s=read(file);
    // Intentionally restrict discovery to original generated Algorithm Lab kernels.
    require(s.find("namespace mre_generated")!=std::string::npos&&s.find("inline uint32_t step(")!=std::string::npos,"Unsupported kernel interface: "+file.string());
    const std::regex expression(R"(uint64_t\{([0-9]+)\}\s*\*\s*x\s*\+\s*uint64_t\{([0-9]+)\}\s*\*\s*y\s*\+\s*uint64_t\{([0-9]+)\}\s*\*\s*x\s*\*\s*y\s*\+\s*([0-9]+)U)");
    std::smatch m;require(std::regex_search(s,m,expression),"Unknown operator format: "+file.string());
    std::array<uint32_t,4> a{};for(size_t i=0;i<4;++i){unsigned long long n=std::stoull(m[i+1].str());require(n<mod,"Coefficient out of range");a[i]=static_cast<uint32_t>(n);}
    return {a[0],a[1],a[2],a[3],file};
}
Profile measure(const Kernel& k){
    std::set<uint32_t> outputs;
    uint32_t state=0,control=0,perturbed=0;uint64_t totalDelta=0,avalanche=0,sensitive=0;
    for(uint32_t i=0;i<4096;++i){
        const uint32_t x=(i*137U+53U)%mod;
        state=step(k,state,x);control=ref(control,x);
        outputs.insert(state);
        totalDelta+=state>=control?state-control:control-state;
        // Matched length, single changed input at start; measures state dependence, not cryptographic security.
        perturbed=step(k,perturbed,i==0?(x+1)%mod:x);
        avalanche+=state>=perturbed?state-perturbed:perturbed-state;
        if(step(k,state,x)!=step(k,state,(x+1)%mod))++sensitive;
    }
    Profile p;p.distinct=static_cast<unsigned>(outputs.size());p.diversity=static_cast<double>(outputs.size())/4096.0;
    p.collision=1.0-p.diversity;p.delta=static_cast<double>(totalDelta)/4096.0;
    p.avalanche=static_cast<double>(avalanche)/4096.0;
    p.sensitivity=static_cast<double>(sensitive)/4096.0;return p;
}
struct Choice {std::string kind,reason;double score{};};
std::vector<Choice> hypotheses(const Profile& p){
    // Hypotheses derived from observed properties, not correctness claims.
    std::vector<Choice> out;
    if(p.diversity>=0.60&&p.sensitivity>=0.95)out.push_back({"stream_analyzer","high state diversity and input responsiveness",p.diversity*0.7+p.sensitivity*0.3});
    if(p.avalanche>=1000.0&&p.sensitivity>=0.95)out.push_back({"change_detector","perturbation persists in recurrent stream",std::min(1.0,p.avalanche/32760.0)*0.5+p.sensitivity*0.5});
    if(p.diversity>=0.30)out.push_back({"pattern_explorer","nontrivial recurrent state space",p.diversity});
    std::sort(out.begin(),out.end(),[](const Choice& a,const Choice& b){return a.score>b.score;});
    if(out.size()>2)out.resize(2);
    return out;
}
std::string kernelHeader(const Kernel& k){
    std::ostringstream s;s<<"#pragma once\n#include <cstdint>\nnamespace discovered_kernel {\n"
        <<"inline constexpr uint32_t modulus=65521U;\ninline uint32_t step(uint32_t state,uint32_t input) {\n"
        <<" const uint64_t x=state%modulus,y=input%modulus;\n"
        <<" return static_cast<uint32_t>((uint64_t{"<<k.a<<"}*x+uint64_t{"<<k.b<<"}*y+uint64_t{"<<k.c<<"}*x*y+"<<k.d<<"U)%modulus);\n}\n}\n";
    return s.str();
}
std::string program(const std::string& kind){
    const std::string common=R"CPP(#include "kernel.hpp"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <stdexcept>
using namespace std;
static vector<uint32_t> values(const string& path){
 ifstream in(path);if(!in)throw runtime_error("Cannot read input file");
 vector<uint32_t> out;string line;
 while(getline(in,line)){
  if(line.empty()||line[0]=='#')continue;
  size_t pos=0;unsigned long n=stoul(line,&pos,10);
  if(pos!=line.size()||n>65520UL)throw runtime_error("Input must be one integer from 0 to 65520 per line");
  if(out.size()>=1000000U)throw runtime_error("Input too long");
  out.push_back(static_cast<uint32_t>(n));
 }
 if(out.empty())throw runtime_error("Input file is empty");
 return out;
}
static int selftest(){
 uint32_t s=0;for(int i=0;i<1000;++i){s=discovered_kernel::step(s,static_cast<uint32_t>(i));if(s>=65521U)return 3;}
 if(discovered_kernel::step(0,1)!=discovered_kernel::step(0,1))return 4;
 cout<<"SELFTEST PASS modulus_bound=1000 deterministic=YES\n";return 0;
}
int main(int argc,char** argv){
 try{
 if(argc==2&&string(argv[1])=="--selftest")return selftest();
 if(argc!=3){cerr<<"Usage: "<<argv[0]<<" input.txt output.csv (or --selftest)\n";return 2;}
 auto input=values(argv[1]);ofstream out(argv[2]);if(!out)throw runtime_error("Cannot create output");
 uint32_t state=0;
)CPP";
    std::string body;
    if(kind=="stream_analyzer")body=R"CPP(
 out<<"index,input,state,step_delta\n";
 uint32_t prev=0;
 for(size_t i=0;i<input.size();++i){state=discovered_kernel::step(state,input[i]);
  const uint32_t delta=state>=prev?state-prev:prev-state;
  out<<i<<","<<input[i]<<","<<state<<","<<delta<<"\n";prev=state;
 }
)CPP";
    else if(kind=="change_detector")body=R"CPP(
 out<<"index,input,state,perturbation_gap\n";
 uint32_t altered=0;
 for(size_t i=0;i<input.size();++i){
  state=discovered_kernel::step(state,input[i]);
  altered=discovered_kernel::step(altered,i==0?(input[i]+1U)%65521U:input[i]);
  const uint32_t gap=state>=altered?state-altered:altered-state;
  out<<i<<","<<input[i]<<","<<state<<","<<gap<<"\n";
 }
)CPP";
    else body=R"CPP(
 out<<"index,input,state,seen_before\n";
 set<uint32_t> seen;
 for(size_t i=0;i<input.size();++i){
  state=discovered_kernel::step(state,input[i]);
  bool repeated=!seen.insert(state).second;
  out<<i<<","<<input[i]<<","<<state<<","<<(repeated?1:0)<<"\n";
 }
)CPP";
    return common+body+R"CPP(
 if(!out)throw runtime_error("Write failed");
 cout<<"PROCESSED "<<input.size()<<" samples; result="<<argv[2]<<"\n";return 0;
 }catch(const exception& e){cerr<<"ERROR "<<e.what()<<"\n";return 1;}
}
)CPP";
}
std::string jsonEscape(const std::string& s){std::string out;for(char c:s){if(c=='"'||c=='\\')out+='\\';if(c=='\n')out+="\\n";else out+=c;}return out;}
std::string decimal(double v){std::ostringstream out;out<<std::fixed<<std::setprecision(6)<<v;return out.str();}
void emit(const fs::path& root,const Kernel& k,const Choice& choice,const Profile& profile){
    const std::string id=cleanName(basename(k.source))+"_"+choice.kind;
    const fs::path dir=root/id;
    require(!fs::exists(dir),"Refusing to overwrite existing program: "+dir.string());
    fs::create_directories(dir);
    write(dir/"kernel.hpp",kernelHeader(k));write(dir/"main.cpp",program(choice.kind));
    write(dir/"CMakeLists.txt",R"(cmake_minimum_required(VERSION 3.21)
project(DiscoveredApplication LANGUAGES CXX)
add_executable(discovered_app main.cpp)
set_target_properties(discovered_app PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(MSVC)
 target_compile_options(discovered_app PRIVATE /W4 /WX /permissive- /EHsc)
else()
 target_compile_options(discovered_app PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
enable_testing()
add_test(NAME generated_selftest COMMAND discovered_app --selftest)
)");
    std::ostringstream samples;for(unsigned i=0;i<256;++i)samples<<(i*137U+53U)%mod<<'\n';write(dir/"example_input.txt",samples.str());
    std::ostringstream report;report<<"{\n  \"schema\": 1,\n  \"source_header\": \""<<jsonEscape(k.source.generic_string())<<"\",\n"
      <<"  \"formula\": \"("<<k.a<<"*s+"<<k.b<<"*x+"<<k.c<<"*s*x+"<<k.d<<") mod 65521\",\n"
      <<"  \"application\": \""<<choice.kind<<"\",\n  \"reason\": \""<<choice.reason<<"\",\n"
      <<"  \"hypothesis_score\": "<<decimal(choice.score)<<",\n  \"diversity\": "<<decimal(profile.diversity)<<",\n"
      <<"  \"input_sensitivity\": "<<decimal(profile.sensitivity)<<",\n  \"perturbation_mean_gap\": "<<decimal(profile.avalanche)<<",\n"
      <<"  \"sample_size\": 4096,\n  \"validation\": \"HYPOTHESIS_ONLY_UNTIL_COMPILED_AND_TESTED\",\n"
      <<"  \"novelty\": \"NOT_ASSESSED\"\n}\n";
    write(dir/"discovery.json",report.str());
    write(dir/"README.md","# "+id+"\n\nAutomatically proposed from the measured recurrence properties of an Algorithm Lab kernel.\n\n"
      "This program is an experimental data-analysis CLI, not a proven detector, compressor, or cryptographic primitive.\n\n"
      "## Build\n\n`cmake -S . -B build` then `cmake --build build --config Release`.\n\n"
      "## Run\n\n`discovered_app example_input.txt output.csv` (Windows: `build\\Release\\discovered_app.exe`).\n\n"
      "## Test\n\n`ctest --test-dir build -C Release --output-on-failure`\n\n"
      "The file `discovery.json` contains the hypothesis and provenance.\n");
}
int discover(const fs::path& kernelFolder,const fs::path& output,int limit){
    require(fs::exists(kernelFolder)&&fs::is_directory(kernelFolder),"Kernel folder does not exist");
    require(limit>=1&&limit<=128,"Limit must be between 1 and 128");
    fs::create_directories(output);
    std::vector<fs::path> headers;
    for(const auto& p:fs::directory_iterator(kernelFolder))if(p.is_regular_file()&&p.path().extension()==".hpp"&&p.path().filename().string().rfind("candidate_",0)==0)headers.push_back(p.path());
    std::sort(headers.begin(),headers.end());
    require(!headers.empty(),"No generated candidate_*.hpp headers found");
    std::ostringstream catalogue;catalogue<<"source,kernel,kind,score,diversity,mean_perturbation_gap\n";
    int done=0,proposals=0;
    for(const fs::path& file:headers){if(done>=limit)break;Kernel k=parse(file);Profile p=measure(k);
        const std::vector<Choice> options=hypotheses(p);
        for(const Choice& choice:options){emit(output,k,choice,p);catalogue<<'"'<<file.generic_string()<<"\","<<basename(file)<<','<<choice.kind<<','<<decimal(choice.score)<<','<<decimal(p.diversity)<<','<<decimal(p.avalanche)<<'\n';++proposals;}
        std::cout<<"ANALYZED "<<file.filename().string()<<" distinct="<<p.distinct<<" proposals="<<options.size()<<'\n';++done;
    }
    write(output/"discovery_catalogue.csv",catalogue.str());
    std::cout<<"DISCOVERY_COMPLETE kernels="<<done<<" generated_programs="<<proposals<<" destination="<<output.string()<<'\n';return 0;
}
int selftest(){
    const Kernel k{12,34,56,78,{}};require(step(k,0,0)==78,"Zero input result");
    require(step(k,5,7)==static_cast<uint32_t>((uint64_t{12}*5+uint64_t{34}*7+uint64_t{56}*35+78)%mod),"Formula integrity");
    require(step(k,23,77)==step(k,23,77),"Deterministic kernel");
    Profile p=measure(k);require(p.distinct>0&&p.distinct<=4096,"Sample count");
    require(hypotheses(p).size()<=2,"Maximum proposals");
    std::cout<<"APPLICATION_DISCOVERY_SELFTEST PASS\n";return 0;
}
}
int main(int argc,char** argv){try{
    if(argc==2&&std::string(argv[1])=="selftest")return selftest();
    if(argc!=5||std::string(argv[1])!="discover"){
        std::cerr<<"Usage: math_application_discovery discover <kernel_folder> <output_folder> <max_kernels_1_to_128>\n"
                 <<"       math_application_discovery selftest\n";return 2;
    }
    return discover(fs::path(argv[2]),fs::path(argv[3]),std::stoi(argv[4]));
}catch(const std::exception& e){std::cerr<<"DISCOVERY_ERROR "<<e.what()<<'\n';return 1;}}

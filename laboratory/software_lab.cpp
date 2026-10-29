#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif
namespace fs=std::filesystem;
namespace {
using Row=std::array<double,8>;
void require(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
void write(const fs::path& p,const std::string& s){fs::create_directories(p.parent_path());std::ofstream o(p,std::ios::binary);require(bool(o),"open output: "+p.string());o<<s;require(bool(o),"write output: "+p.string());}
struct Rule{unsigned a=0,b=7,op=1;};
enum class Purpose{delta,mean,maximum,minimum,spread};
Purpose parsePurpose(const std::string& s){if(s=="delta")return Purpose::delta;if(s=="mean")return Purpose::mean;if(s=="maximum")return Purpose::maximum;if(s=="minimum")return Purpose::minimum;if(s=="spread")return Purpose::spread;throw std::runtime_error("unsupported purpose: "+s);}
std::string purposeName(Purpose p){switch(p){case Purpose::delta:return "delta";case Purpose::mean:return "mean";case Purpose::maximum:return "maximum";case Purpose::minimum:return "minimum";case Purpose::spread:return "spread";}return "invalid";}
double feat(const Row& v,unsigned id){double sum=std::accumulate(v.begin(),v.end(),0.0);switch(id){case 0:return v[0];case 1:return v[7];case 2:return sum/8.;case 3:return *std::max_element(v.begin(),v.end());case 4:return *std::min_element(v.begin(),v.end());case 5:return v[7]-v[0];case 6:return (v[0]+v[1]+v[2]+v[3])/4.;default:return (v[4]+v[5]+v[6]+v[7])/4.;}}
double evaluateRule(const Row& v,Rule r){double a=feat(v,r.a),b=feat(v,r.b);switch(r.op){case 0:return (a+b)/2.;case 1:return std::abs(a-b);case 2:return std::max(a,b);case 3:return std::min(a,b);default:return (3*a+b)/4.;}}
std::vector<Row> data(unsigned seed,unsigned n,unsigned domain){std::mt19937 gen(seed);std::uniform_real_distribution<double> noise(-0.09,0.09);std::vector<Row> rows;rows.reserve(n);for(unsigned i=0;i<n;++i){Row r{};double phase=double(i)*0.036;for(unsigned j=0;j<8;++j){double x=0.5+0.22*std::sin(phase+j*0.22)+noise(gen);if(domain==1)x+=(i%97==0?0.35:0.0);if(domain==2)x=0.3+0.4*(double(i%41)/41.)+noise(gen);if(domain==3)x=0.5+0.24*std::sin(phase*2+j*0.41)+noise(gen);r[j]=std::clamp(x,0.,1.);}rows.push_back(r);}return rows;}
double target(const Row& r,Purpose p){switch(p){case Purpose::delta:return std::abs(r[7]-r[0]);case Purpose::mean:return std::accumulate(r.begin(),r.end(),0.)/8.;case Purpose::maximum:return *std::max_element(r.begin(),r.end());case Purpose::minimum:return *std::min_element(r.begin(),r.end());case Purpose::spread:return *std::max_element(r.begin(),r.end())-*std::min_element(r.begin(),r.end());}return 0.;} // Explicit benchmark tasks, not invented human utility.
struct Score{double mae=0;double throughput=0;double peakKiB=0;double variance=0;};
double memoryKiB(){
#ifdef _WIN32
 PROCESS_MEMORY_COUNTERS_EX info{};if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&info),sizeof(info)))return double(info.PeakWorkingSetSize)/1024.;return 0.;
#else
 struct rusage r{};if(getrusage(RUSAGE_SELF,&r)==0)return double(r.ru_maxrss);return 0.;
#endif
}
Score score(const std::vector<Row>& rows,Rule rule,unsigned iterations,Purpose purpose=Purpose::delta){volatile double sink=0.;double err=0.,sum=0.,sumsq=0.;auto t0=std::chrono::steady_clock::now();for(unsigned k=0;k<iterations;++k){for(const auto& row:rows){double y=evaluateRule(row,rule);sink=sink+y; if(k==0){err+=std::abs(y-target(row,purpose));sum+=y;sumsq+=y*y;}}}auto t1=std::chrono::steady_clock::now();(void)sink;Score s;s.mae=err/double(rows.size());double secs=std::chrono::duration<double>(t1-t0).count();s.throughput=double(rows.size())*iterations/std::max(secs,1e-9);s.variance=std::max(0.,sumsq/double(rows.size())-std::pow(sum/double(rows.size()),2));s.peakKiB=memoryKiB();return s;}
std::string guiSource(){return R"GUI(#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include "generated.hpp"
static HWND inputBox=nullptr, outputBox=nullptr, status=nullptr;
static std::wstring browse(HWND owner,bool save){wchar_t path[MAX_PATH]{};OPENFILENAMEW of{};of.lStructSize=sizeof(of);of.hwndOwner=owner;of.lpstrFile=path;of.nMaxFile=MAX_PATH;of.lpstrFilter=L"CSV files\0*.csv\0All files\0*.*\0";of.Flags=OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);if(save?GetSaveFileNameW(&of):GetOpenFileNameW(&of))return path;return {};}
static void run(){wchar_t a[32768]{},b[32768]{};GetWindowTextW(inputBox,a,32768);GetWindowTextW(outputBox,b,32768);if(!*a||!*b){SetWindowTextW(status,L"Select input and output CSV");return;}
 std::ifstream in{std::filesystem::path(a)};std::ofstream out{std::filesystem::path(b)};if(!in||!out){SetWindowTextW(status,L"Could not open file");return;}std::string line;unsigned count=0;out<<"row,value,reference,delta\n";out<<std::setprecision(17);try{while(std::getline(in,line)){if(line.empty())continue;generated::Row v{};std::istringstream ss(line);std::string cell;for(double& x:v){if(!std::getline(ss,cell,','))throw 1;x=std::stod(cell);if(!std::isfinite(x))throw 1;}double y=generated::evaluate(v),ref=generated::reference(v);out<<count<<','<<y<<','<<ref<<','<<std::abs(y-ref)<<'\n';++count;}}catch(...){SetWindowTextW(status,L"Invalid CSV data");return;}std::wstring msg=L"Completed rows: "+std::to_wstring(count);SetWindowTextW(status,msg.c_str());}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM wp,LPARAM lp){switch(m){case WM_CREATE:{CreateWindowW(L"STATIC",L"Input CSV",WS_CHILD|WS_VISIBLE,16,18,200,24,w,nullptr,nullptr,nullptr);inputBox=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,16,45,390,26,w,nullptr,nullptr,nullptr);CreateWindowW(L"BUTTON",L"Browse",WS_CHILD|WS_VISIBLE,414,45,96,26,w,reinterpret_cast<HMENU>(1),nullptr,nullptr);CreateWindowW(L"STATIC",L"Output CSV",WS_CHILD|WS_VISIBLE,16,85,200,24,w,nullptr,nullptr,nullptr);outputBox=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,16,112,390,26,w,nullptr,nullptr,nullptr);CreateWindowW(L"BUTTON",L"Browse",WS_CHILD|WS_VISIBLE,414,112,96,26,w,reinterpret_cast<HMENU>(2),nullptr,nullptr);CreateWindowW(L"BUTTON",L"Process data",WS_CHILD|WS_VISIBLE,16,163,180,32,w,reinterpret_cast<HMENU>(3),nullptr,nullptr);status=CreateWindowW(L"STATIC",L"Ready - 8 numeric columns per input row",WS_CHILD|WS_VISIBLE,16,215,490,48,w,nullptr,nullptr,nullptr);return 0;}case WM_COMMAND:{int id=LOWORD(wp);if(id==1||id==2){auto s=browse(w,id==2);if(!s.empty())SetWindowTextW(id==1?inputBox:outputBox,s.c_str());}else if(id==3)run();return 0;}case WM_DESTROY:PostQuitMessage(0);return 0;}return DefWindowProcW(w,m,wp,lp);}
int WINAPI wWinMain/*_entry*/(HINSTANCE h,HINSTANCE,PWSTR,int show){WNDCLASSW wc{};wc.lpfnWndProc=wnd;wc.hInstance=h;wc.lpszClassName=L"MRELabApp";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);if(!RegisterClassW(&wc))return 2;HWND w=CreateWindowW(wc.lpszClassName,L"MathResearchEngine - Discovered Application",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,550,330,nullptr,nullptr,h,nullptr);if(!w)return 3;ShowWindow(w,show);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return static_cast<int>(msg.wParam);}
)GUI";}
void generate(const fs::path& dir,Rule r,Purpose purpose){fs::create_directories(dir);std::ostringstream h;h<<"#pragma once\n#include <array>\n#include <algorithm>\n#include <cmath>\nnamespace generated {using Row=std::array<double,8>;\ninline double feature(const Row& v,unsigned id){double sum=0,mx=v[0],mn=v[0];for(double x:v){sum+=x;mx=std::max(mx,x);mn=std::min(mn,x);}switch(id){case 0:return v[0];case 1:return v[7];case 2:return sum/8.;case 3:return mx;case 4:return mn;case 5:return v[7]-v[0];case 6:return (v[0]+v[1]+v[2]+v[3])/4.;default:return (v[4]+v[5]+v[6]+v[7])/4.;}}\ninline double evaluate(const Row& v){double a=feature(v,"<<r.a<<"),b=feature(v,"<<r.b<<");switch("<<r.op<<"){case 0:return (a+b)/2.;case 1:return std::abs(a-b);case 2:return std::max(a,b);case 3:return std::min(a,b);default:return (3*a+b)/4.;}}}\n";h<<"namespace generated {inline double reference(const Row& r){switch("<<static_cast<int>(purpose)<<"){case 0:return std::abs(r[7]-r[0]);case 1:{double s=0;for(double x:r)s+=x;return s/8.;}case 2:return *std::max_element(r.begin(),r.end());case 3:return *std::min_element(r.begin(),r.end());default:return *std::max_element(r.begin(),r.end())-*std::min_element(r.begin(),r.end());}}}\n";write(dir/"generated.hpp",h.str());
std::ostringstream cm;cm<<"cmake_minimum_required(VERSION 3.21)\nproject(discovered_software LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\nadd_executable(discovered_app main.cpp)\n";cm<<"if(MSVC)\n target_compile_options(discovered_app PRIVATE /W4 /WX /permissive- /EHsc)\nelse()\n target_compile_options(discovered_app PRIVATE -Wall -Wextra -Wpedantic -Werror)\nendif()\n";cm<<"if(WIN32)\n add_executable(discovered_gui WIN32 gui.cpp)\n target_compile_options(discovered_gui PRIVATE /W4 /WX /permissive- /EHsc)\n target_link_libraries(discovered_gui PRIVATE comdlg32)\nendif()\n";write(dir/"CMakeLists.txt",cm.str());
std::ostringstream app;app<<R"CPP(#include "generated.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
int main(int argc,char** argv){try{
 if(argc==2&&std::string(argv[1])=="--selftest"){
  generated::Row r{0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8};
  if(std::abs(generated::evaluate(r)-)CPP"<<std::setprecision(17)<<evaluateRule(Row{.1,.2,.3,.4,.5,.6,.7,.8},r)<<R"CPP()>1e-10)return 4;
  std::cout<<"SELFTEST PASS\n";return 0;
 }
 if(argc!=3){std::cerr<<"Usage: discovered_app input.csv output.csv\n";return 2;}
 std::ifstream in(argv[1]);std::ofstream out(argv[2]);if(!in||!out)throw std::runtime_error("Cannot open files");
 out<<std::setprecision(17)<<"row,value,reference,delta\n";
 unsigned n=0;std::string line;while(std::getline(in,line)){
  if(line.empty())continue;
  std::istringstream ss(line);std::string field;generated::Row row{};
  for(double& x:row){if(!std::getline(ss,field,','))throw std::runtime_error("CSV missing columns");x=std::stod(field);if(!std::isfinite(x))throw std::runtime_error("Nonfinite input");}
  double v=generated::evaluate(row),ref=generated::reference(row);out<<n++<<','<<v<<','<<ref<<','<<std::abs(v-ref)<<'\n';
 }
 std::cout<<"PROCESSED "<<n<<" rows\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
)CPP";write(dir/"main.cpp",app.str());write(dir/"gui.cpp",guiSource());
std::ostringstream csv;csv<<std::setprecision(17);for(const auto& row:data(1201,256,1)){for(unsigned j=0;j<8;++j)csv<<(j?",":"")<<row[j];csv<<'\n';}write(dir/"example_input.csv",csv.str());
write(dir/"APP_SPEC.md","# Generated software\n\nInput: CSV with eight numeric columns.\nOutput: CSV with row, candidate, reference and absolute difference.\nReference: declared objective (see research report).\nGUI: Win32 file selection and processing on Windows.\nThis is an experimental program, not a verified new scientific discovery.\n");}
void selftest(){Row row{0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8};require(std::abs(evaluateRule(row,Rule{0,1,1})-.7)<1e-10,"operator selftest");auto a=data(42,40,1),b=data(42,40,1);require(a==b,"reproducibility");auto s=score(a,Rule{0,1,1},1);require(s.mae<1e-12,"reference score");require(std::abs(target(row,Purpose::mean)-.45)<1e-12,"mean objective");require(std::abs(target(row,Purpose::maximum)-.8)<1e-12,"max objective");std::cout<<"SOFTWARE_LAB_SELFTEST PASS\n";}
void discover(const fs::path& root,unsigned budget,Purpose purpose=Purpose::delta,const fs::path& feedbackPath={}){require(budget>=8&&budget<=200000,"invalid budget");fs::create_directories(root);std::mt19937 rng(8044);const auto train=data(100,512,0),validation=data(200,512,1),holdout=data(300,1024,2),stress=data(400,1024,3);Rule best{};double lowest=1e9;int preferred=-1;double preferenceWeight=0.;if(!feedbackPath.empty()){std::ifstream f(feedbackPath);require(bool(f),"feedback file missing");std::string line;while(std::getline(f,line)){if(line.empty()||line[0]=='#'||line=="metric,value")continue;auto pos=line.find(',');require(pos!=std::string::npos,"invalid feedback row");const auto key=line.substr(0,pos),value=line.substr(pos+1);if(key=="preferred_operation"){preferred=std::stoi(value);require(preferred>=0&&preferred<=4,"preferred_operation must be 0..4");}else if(key=="preference_weight"){preferenceWeight=std::stod(value);require(std::isfinite(preferenceWeight)&&preferenceWeight>=0.&&preferenceWeight<=0.25,"preference_weight must be 0..0.25");}}} std::ostringstream history;history<<"candidate,a,b,operation,training_mae,validation_mae\n";for(unsigned i=0;i<budget;++i){Rule r{i==0?0U:static_cast<unsigned>(rng()%8U),i==0?1U:static_cast<unsigned>(rng()%8U),i==0?1U:static_cast<unsigned>(rng()%5U)};auto t=score(train,r,1,purpose),v=score(validation,r,1,purpose);history<<i<<','<<r.a<<','<<r.b<<','<<r.op<<','<<t.mae<<','<<v.mae<<'\n';const double adjusted=v.mae+((preferred>=0&&static_cast<int>(r.op)!=preferred)?preferenceWeight:0.);if(adjusted<lowest){lowest=adjusted;best=r;}}write(root/"search_history.csv",history.str());
// Holdout and stress are NEVER used for candidate selection.
Score s=score(holdout,best,32,purpose),shift=score(stress,best,16,purpose),ref=score(holdout,Rule{0,1,1},32,purpose);auto benchmark=data(501,4096,3);auto bench=score(benchmark,best,100,purpose);auto refbench=score(benchmark,Rule{0,1,1},100,purpose);generate(root/"selected_application",best,purpose);
std::ostringstream report;report<<std::setprecision(12)<<"# Autonomous Software Laboratory - Experimental Report\n\n"<<"- Purpose: "<<purposeName(purpose)<<"\n- Feedback file: "<<(feedbackPath.empty()?"none":feedbackPath.string())<<"\n- Evaluated candidates: "<<budget<<"\n- Objective: minimize mean absolute error for the declared 8-channel task\n"<<"- Selected rule: a="<<best.a<<", b="<<best.b<<", op="<<best.op<<"\n- Holdout MAE: "<<s.mae<<"\n- Reference holdout MAE: "<<ref.mae<<"\n- Shifted-domain MAE: "<<shift.mae<<"\n- Candidate throughput rows/s: "<<bench.throughput<<"\n- Reference throughput rows/s: "<<refbench.throughput<<"\n- Process peak working memory KiB: "<<bench.peakKiB<<"\n- Candidate output variance: "<<s.variance<<"\n"<<"- Result: "<<(s.mae+1e-9<ref.mae?"BETTER_THAN_REFERENCE_ON_SYNTHETIC_HOLDOUT":"NO_VERIFIED_GAIN")<<"\n- GUI: generated Win32 application; requires Windows build and manual interaction test\n- Usability: form controls and file errors implemented; no automated GUI accessibility certification\n- Source mathematics: this laboratory can consume research-generated rules; current experiment is an explicit signal task, not a semantically proved Lean translation\n- Revisions: candidates selected by validation score, no deceptive unlimited retry loop\n";write(root/"LAB_REPORT.md",report.str());std::cout<<"LAB_COMPLETE budget="<<budget<<" selected="<<best.a<<':'<<best.b<<':'<<best.op<<" holdout_mae="<<s.mae<<" baseline_mae="<<ref.mae<<" output="<<root.string()<<'\n';}
}
int main(int argc,char** argv){try{if(argc==2&&std::string(argv[1])=="selftest"){selftest();return 0;}if((argc==4||argc==5||argc==6)&&std::string(argv[1])=="discover"){discover(fs::path(argv[2]),static_cast<unsigned>(std::stoul(argv[3])),argc>=5?parsePurpose(argv[4]):Purpose::delta,argc==6?fs::path(argv[5]):fs::path{});return 0;}std::cerr<<"Usage: math_software_lab selftest | discover OUTPUT BUDGET [delta|mean|maximum|minimum|spread] [feedback.csv]\n";return 2;}catch(const std::exception& e){std::cerr<<"LAB_ERROR: "<<e.what()<<'\n';return 1;}}

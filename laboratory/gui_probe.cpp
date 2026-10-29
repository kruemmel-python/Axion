#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
namespace fs=std::filesystem;
namespace {
struct State { HWND window=nullptr; std::vector<HWND> edits; std::vector<HWND> buttons; std::vector<HWND> statics; };
BOOL CALLBACK enumerate(HWND w,LPARAM p){auto& s=*reinterpret_cast<State*>(p);wchar_t type[64]{};GetClassNameW(w,type,64);const std::wstring cls(type);if(cls==L"Edit")s.edits.push_back(w);else if(cls==L"Button")s.buttons.push_back(w);else if(cls==L"Static")s.statics.push_back(w);return TRUE;}
BOOL CALLBACK seek(HWND w,LPARAM p){auto& s=*reinterpret_cast<State*>(p);wchar_t caption[256]{};GetWindowTextW(w,caption,256);if(std::wstring(caption).find(L"MathResearchEngine - Discovered Application")!=std::wstring::npos){s.window=w;return FALSE;}return TRUE;}
std::string readFile(const fs::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
}
int wmain(int argc,wchar_t** argv){if(argc!=4){std::cerr<<"usage: gui_probe GUI.EXE INPUT.CSV OUTPUT.CSV\n";return 2;}const std::wstring command=L"\""+std::wstring(argv[1])+L"\"";std::vector<wchar_t> buf(command.begin(),command.end());buf.push_back(0);STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};if(!CreateProcessW(nullptr,buf.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi)){std::cerr<<"GUI LAUNCH FAIL\n";return 3;}int result=1;State s;for(int i=0;i<80&&!s.window;++i){EnumWindows(seek,reinterpret_cast<LPARAM>(&s));if(!s.window)Sleep(100);}if(s.window){EnumChildWindows(s.window,enumerate,reinterpret_cast<LPARAM>(&s));if(s.edits.size()>=2&&s.buttons.size()>=3&&s.statics.size()>=3){SetWindowTextW(s.edits[0],argv[2]);SetWindowTextW(s.edits[1],argv[3]);SendMessageW(s.window,WM_COMMAND,MAKEWPARAM(3,BN_CLICKED),reinterpret_cast<LPARAM>(s.buttons[2]));Sleep(300);const std::string output=readFile(argv[3]);size_t lines=0;for(char c:output)if(c=='\n')++lines;result=lines==257?0:5;std::cout<<"GUI_PROBE controls="<<s.edits.size()<<","<<s.buttons.size()<<","<<s.statics.size()<<" output_lines="<<lines<<" status="<<(result==0?"PASS":"FAIL")<<"\n";}else std::cerr<<"GUI CONTROLS MISSING\n";}else std::cerr<<"GUI WINDOW NOT FOUND\n";if(s.window)PostMessageW(s.window,WM_CLOSE,0,0);WaitForSingleObject(pi.hProcess,1500);if(WaitForSingleObject(pi.hProcess,0)==WAIT_TIMEOUT)TerminateProcess(pi.hProcess,1);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return result;}

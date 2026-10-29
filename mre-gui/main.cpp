#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <commdlg.h>
#include <string>
#include <thread>
#include <filesystem>
#include <vector>
#include <atomic>
#include <mutex>
#include <memory>
#include <cwchar>
#include <algorithm>
#include <cstdlib>

namespace fs=std::filesystem;
static HWND win{}, logbox{},status{},keybox{},providerbox{},modelbox{},iterbox{},seedbox{},timeoutbox{},runbtn{},testbtn{},stopbtn{},auditbtn{},krpathbox{},buildmodebox{};
static fs::path root;
static std::atomic<bool> busy{false}, cancelRequested{false};
static std::atomic<HANDLE> activeProcess{nullptr};
static std::atomic<int> candidateFailures{0}, candidatePasses{0}, referencePasses{0};
static std::mutex resultMutex;
static fs::path lastDatabaseFirstResult;
static constexpr UINT WM_LOG=WM_APP+1, WM_FINISHED=WM_APP+2;
static constexpr int ID_RUN=101,ID_TEST=102,ID_STOP=103,ID_AUDIT=104,ID_KR_BROWSE=105,ID_PROVIDER=106;
static std::wstring getText(HWND h) { int n=GetWindowTextLengthW(h);std::wstring s(static_cast<size_t>(n)+1,L'\0');GetWindowTextW(h,s.data(),n+1);s.resize(static_cast<size_t>(n));return s; }
static void append(const std::wstring& s){auto p=new std::wstring(s);PostMessageW(win,WM_LOG,0,reinterpret_cast<LPARAM>(p));}
static std::wstring quote(std::wstring s){std::wstring r=L"\""; for(wchar_t c:s){if(c==L'"') r+=L'\\';r+=c;}return r+L"\"";}
static std::wstring cmake(){fs::path p=L"C:\\Program Files\\CMake\\bin\\cmake.exe";return fs::exists(p)?p.wstring():L"cmake.exe";}
static bool runChild(const std::wstring& exe,const std::wstring& args,const fs::path& cwd,const std::wstring& secret=L"",const std::wstring& krPath=L"",const std::wstring& buildMode=L"AUTO"){
 SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE}; HANDLE rd=nullptr,wr=nullptr;
 if(!CreatePipe(&rd,&wr,&sa,0)){append(L"PIPE ERROR\r\n");return false;}SetHandleInformation(rd,HANDLE_FLAG_INHERIT,0);
 STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;si.hStdOutput=wr;si.hStdError=wr;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
 PROCESS_INFORMATION pi{};std::wstring cmd=quote(exe)+L" "+args;
 // The key is inherited only by the child, never placed in its command line or a file.
 if(!secret.empty())SetEnvironmentVariableW(L"GEMINI_API_KEY",secret.c_str());
 if(!krPath.empty())SetEnvironmentVariableW(L"MRE_KR_COMPILER",krPath.c_str());
 SetEnvironmentVariableW(L"MRE_BUILD_MODE",buildMode.c_str());
 BOOL ok=CreateProcessW(nullptr,cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,cwd.c_str(),&si,&pi);
 if(!secret.empty())SetEnvironmentVariableW(L"GEMINI_API_KEY",nullptr);
 SetEnvironmentVariableW(L"MRE_KR_COMPILER",nullptr);
 SetEnvironmentVariableW(L"MRE_BUILD_MODE",nullptr);
 CloseHandle(wr);
 if(!ok){append(L"Prozessstart fehlgeschlagen: "+std::to_wstring(GetLastError())+L" | "+exe+L"\r\n");CloseHandle(rd);return false;}
 activeProcess=pi.hProcess;CloseHandle(pi.hThread);
 char buf[4096];DWORD n=0;std::string pending;
 while(ReadFile(rd,buf,sizeof(buf),&n,nullptr)&&n){pending.append(buf,n);size_t pos=0;
  while((pos=pending.find('\n'))!=std::string::npos){std::string line=pending.substr(0,pos);pending.erase(0,pos+1);
   if(!line.empty()&&line.back()=='\r')line.pop_back();
   if(line.find("CANDIDATE_DIAGNOSTIC ") == 0 && line.find("status=REFERENCE_CASES_PASS") != std::string::npos) ++referencePasses;
   if(line.find("RESEARCH_RUN_SUMMARY ") == 0) {
     const auto failed=line.find("candidates_failed=");
     const auto passed=line.find("candidates_passed=");
     if(failed!=std::string::npos) candidateFailures=std::atoi(line.c_str()+failed+18);
     if(passed!=std::string::npos) candidatePasses=std::atoi(line.c_str()+passed+18);
   }
   if(line.find("DB_FIRST_RUN id=") == 0) {
    try {
     const auto idPos=line.find("id=")+3, seedPos=line.find(" seed=",idPos);
     const auto endSeed=line.find(' ',seedPos+6);
     const auto runId=std::stoull(line.substr(idPos,seedPos-idPos));
     const auto seed=std::stoull(line.substr(seedPos+6,endSeed-(seedPos+6)));
     std::lock_guard<std::mutex> lock(resultMutex);
     lastDatabaseFirstResult=root/L"generated-database-first-gemini"/(L"database_first_run_"+std::to_wstring(seed)+L"_"+std::to_wstring(runId));
    } catch (...) {}
   }
   if(line.size()>10000)line.resize(10000);
   int need=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,line.c_str(),static_cast<int>(line.size()),nullptr,0);
   if(need>0){std::wstring out(static_cast<size_t>(need),L'\0');MultiByteToWideChar(CP_UTF8,0,line.c_str(),static_cast<int>(line.size()),out.data(),need);append(out+L"\r\n");}
   else append(std::wstring(line.begin(),line.end())+L"\r\n");
  }
  if(cancelRequested){TerminateProcess(pi.hProcess,130);break;}
 }
 if(!pending.empty())append(std::wstring(pending.begin(),pending.end())+L"\r\n");
 WaitForSingleObject(pi.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(pi.hProcess,&code);
 activeProcess=nullptr;CloseHandle(pi.hProcess);CloseHandle(rd);
 append(L"[Prozess beendet: Code "+std::to_wstring(code)+L"]\r\n");return code==0;
}
static bool buildController(){auto cm=cmake();append(L"[1/3] CMake konfigurieren...\r\n");if(!runChild(cm,L"-S . -B build-windows -G \"Visual Studio 17 2022\" -A x64",root))return false;
 append(L"[2/3] Controller unter MSVC bauen...\r\n");return runChild(cm,L"--build build-windows --config Release --target math_research_v03",root);}
static void finish(){PostMessageW(win,WM_FINISHED,0,0);}
static void startJob(int kind){if(busy.exchange(true))return;cancelRequested=false;candidateFailures=0;candidatePasses=0;referencePasses=0;
 EnableWindow(runbtn,FALSE);EnableWindow(testbtn,FALSE);EnableWindow(stopbtn,TRUE);
 auto model=getText(modelbox),key=getText(keybox),it=getText(iterbox),seed=getText(seedbox),timeoutMinutes=getText(timeoutbox);
 int providerIndex=static_cast<int>(SendMessageW(providerbox,CB_GETCURSEL,0,0));
 const bool lmStudio=providerIndex==1;
 const std::wstring provider=lmStudio?L"lm-studio":L"gemini";
 auto krPath=getText(krpathbox);
 int selected=static_cast<int>(SendMessageW(buildmodebox,CB_GETCURSEL,0,0));
 std::wstring buildMode=(selected==1?L"KR":(selected==2?L"MSVC":L"AUTO"));
 if(kind==ID_RUN && buildMode==L"KR" && (krPath.empty() || !fs::exists(krPath))){append(L"KR-Modus: KR-COMPILE.exe ist nicht vorhanden.\r\n");finish();return;}
 if(kind==ID_RUN&&!lmStudio&&key.empty()){append(L"Bitte API-Key eingeben.\r\n");finish();return;}
 if(kind==ID_RUN&&((!lmStudio&&model.empty())||it.empty()||seed.empty()||timeoutMinutes.empty())){append(L"Modell, Iterationen, Seed und Antwortdauer prüfen.\r\n");finish();return;}
 unsigned long timeoutValue=0;try{timeoutValue=std::stoul(timeoutMinutes);}catch(...){timeoutValue=0;}
 if(kind==ID_RUN&&(timeoutValue<1||timeoutValue>240)){append(L"Antwortdauer muss zwischen 1 und 240 Minuten liegen.\r\n");finish();return;}
 // Erase the visible password field immediately after copying into worker memory.
 SetWindowTextW(keybox,L"");
 std::thread([kind,model=std::move(model),key=std::move(key),it=std::move(it),seed=std::move(seed),timeoutValue,provider,krPath=std::move(krPath),buildMode=std::move(buildMode)]()mutable {
   bool ok=false;
   if(kind==ID_RUN){
    append(L"[MRE] Starte Database-First-Forschungsworkflow. Keine PowerShell nötig.\r\n");
    {
     std::lock_guard<std::mutex> lock(resultMutex);
     lastDatabaseFirstResult=root/L"generated-database-first-gemini";
    }
    if(buildController()&&!cancelRequested){
      append(provider==L"lm-studio"?L"[3/3] Anfrage an LM Studio gestartet; Antwortdauer laut GUI-Einstellung (bis zu 240 Minuten)...\r\n":L"[3/3] Anfrage an Gemini gestartet; die Antwort kann mehrere Minuten dauern...\r\n");
      fs::create_directories(root/L"research");fs::create_directories(root/L"generated-database-first-gemini");
      auto exe=(root/L"build-windows"/L"Release"/L"math_research_v03.exe").wstring();
      fs::path sourceDb=L"F:\\MathResearchDB_Backup_20261009\\research.sqlite";
      fs::path leanExe=L"C:\\Users\\ralfk\\.elan\\bin\\lean.exe";
      fs::path leanScript=L"F:\\MRE_Autonomous_Research_GeminiAPI\\lean\\LeanSynthesizer.lean";
      if(!fs::is_regular_file(sourceDb)){append(L"Mathematische Quelldatenbank fehlt: "+sourceDb.wstring()+L"\r\n");finish();return;}
      if(!fs::is_regular_file(leanExe)||!fs::is_regular_file(leanScript)){append(L"Lean-Konfiguration fehlt: "+leanExe.wstring()+L" / "+leanScript.wstring()+L"\r\n");finish();return;}
      fs::path leanRoot=L"F:\\MathResearchDB_v0_4\\lean_cache\\oai_workspace;F:\\MathResearchDB_v0_4\\lean_cache\\mathlib\\.lake\\build\\lib\\lean;F:\\MathResearchDB_v0_4\\lean_cache\\mathlib\\.lake\\packages\\batteries\\.lake\\build\\lib\\lean;F:\\MathResearchDB_v0_4\\lean_cache\\mathlib\\.lake\\packages\\aesop\\.lake\\build\\lib\\lean;F:\\MathResearchDB_v0_4\\lean_cache\\mathlib\\.lake\\packages\\Qq\\.lake\\build\\lib\\lean;F:\\MathResearchDB_v0_4\\lean_cache\\mathlib\\.lake\\packages\\proofwidgets\\.lake\\build\\lib\\lean;F:\\MathResearchDB_v0_4\\lean_cache\\mathlib\\.lake\\packages\\plausible\\.lake\\build\\lib\\lean;F:\\MathResearchDB_v0_4\\lean_cache\\mathlib\\.lake\\packages\\importGraph\\.lake\\build\\lib\\lean;F:\\MathResearchDB_v0_4\\lean_cache\\mathlib\\.lake\\packages\\LeanSearchClient\\.lake\\build\\lib\\lean";
      const std::wstring endpoint=provider==L"lm-studio"?L"http://127.0.0.1:1234/v1/chat/completions":L"https://generativelanguage.googleapis.com/v1beta/openai/chat/completions";
      const std::wstring selectedModel=provider==L"lm-studio"?(model.empty()?L"auto":model):model;
      std::wstring args=L"database-first-software-research "+quote(sourceDb.wstring())+L" "+quote((root/L"research"/L"database_first_gui.sqlite").wstring())+L" "+quote((root/L"generated-database-first-gemini").wstring())+L" "+it+L" "+seed+L" "+quote(endpoint)+L" "+quote(selectedModel)+L" "+quote(leanExe.wstring())+L" "+quote(leanScript.wstring())+L" "+quote(leanRoot.wstring())+L" --provider="+provider+L" --llm-timeout-ms="+std::to_wstring(timeoutValue*60000UL)+L" --require-gemini";
      append(L"[BUILD] Provider="+provider+L" | Modus="+buildMode+L" | KR="+(krPath.empty()?L"nicht eingerichtet":krPath)+L"\r\n");
      ok=runChild(exe,args,root,key,krPath,buildMode);
    }
   }else{
    fs::path sub=root/L"gemini-spectral-validated";
    if(!fs::exists(sub/L"CMakeLists.txt")){append(L"gemini-spectral-validated fehlt. Bitte zuerst Spectral-Overlay entpacken.\r\n");}
    else{
     auto cm=cmake();append(L"[Spectral] Projekt bauen...\r\n");
     const fs::path spectralBuild=sub/L"build-windows-mre";
     ok=runChild(cm,L"-S "+quote(sub.wstring())+L" -B "+quote(spectralBuild.wstring())+L" -G \"Visual Studio 17 2022\" -A x64",sub)
       &&runChild(cm,L"--build "+quote(spectralBuild.wstring())+L" --config Release --target research_candidate",sub)
       &&runChild((spectralBuild/L"Release"/L"research_candidate.exe").wstring(),L"--selftest",sub)
       &&runChild((spectralBuild/L"Release"/L"research_candidate.exe").wstring(),L"--scan",sub);
    }
   }
   std::fill(key.begin(),key.end(),L'\0');
   if(!ok) append(L"[MRE] Workflow fehlgeschlagen oder abgebrochen. Details oben.\r\n");
   else if(kind==ID_RUN && candidateFailures.load()>0) append(L"[MRE] Controller abgeschlossen, aber Softwarekandidat(en) FEHLGESCHLAGEN. Diagnose im Log und CANDIDATE_DIAGNOSTICS.txt.\r\n");
   else if(kind==ID_RUN && referencePasses.load()>0) append(L"[MRE] Unabhaengige mathematische Referenzfaelle bestanden (begrenzte Tests). Neue Entdeckung NICHT nachgewiesen.\r\n");
   else if(kind==ID_RUN && candidatePasses.load()>0) append(L"[MRE] Kandidaten-Selbsttest bestanden; mathematische Forschung NICHT unabhaengig validiert.\r\n");
   else append(L"[MRE] Workflow abgeschlossen.\r\n");
   finish();
 }).detach();}
static HWND label(HWND parent,const wchar_t* t,int x,int y,int w,int h){return CreateWindowW(L"STATIC",t,WS_CHILD|WS_VISIBLE,x,y,w,h,parent,nullptr,nullptr,nullptr);}
static HWND field(HWND parent,const wchar_t* initial,int x,int y,int w,int h,DWORD extra=0){return CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",initial,WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL|extra,x,y,w,h,parent,nullptr,nullptr,nullptr);}
static HWND button(HWND parent,const wchar_t* t,int id,int x,int y,int w){return CreateWindowW(L"BUTTON",t,WS_CHILD|WS_VISIBLE|WS_TABSTOP,x,y,w,32,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);}
static LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_CREATE:{
  win=h; // WM_CREATE runs before CreateWindowW returns; child factories need a valid parent HWND.
  label(h,L"MATH RESEARCH ENGINE | Windows Research Console",18,12,690,25);
  label(h,L"Gemini API-Key (nur im RAM):",18,54,200,20);keybox=field(h,L"",215,49,470,27,ES_PASSWORD);
  label(h,L"Provider:",18,95,80,20);providerbox=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,105,89,130,100,h,reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PROVIDER)),nullptr,nullptr);
  SendMessageW(providerbox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Gemini API"));
  SendMessageW(providerbox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"LM Studio (lokal)"));
  SendMessageW(providerbox,CB_SETCURSEL,0,0);
  label(h,L"Modell:",245,95,55,20);modelbox=field(h,L"gemini-3.8-flash",300,89,170,28);
  label(h,L"Versuche:",478,95,65,20);iterbox=field(h,L"1",543,89,45,28,ES_NUMBER);
  label(h,L"Seed:",595,95,40,20);seedbox=field(h,L"42",635,89,70,28,ES_NUMBER);
  label(h,L"Build-Modus:",18,130,95,21);
  buildmodebox=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,110,125,125,100,h,nullptr,nullptr,nullptr);
  SendMessageW(buildmodebox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Auto: KR -> MSVC"));
  SendMessageW(buildmodebox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Nur KR"));
  SendMessageW(buildmodebox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Nur MSVC"));
  SendMessageW(buildmodebox,CB_SETCURSEL,0,0);
  label(h,L"KR-COMPILE.exe:",244,130,119,21);
  krpathbox=field(h,L"F:\\KR_COMPILER\\KR-Portable-CPP-MultiHost_v2.4\\KR-COMPILE.exe",360,125,305,27);
  button(h,L"...",ID_KR_BROWSE,668,125,35);
  label(h,L"Antwortdauer (Min.):",18,165,120,20);timeoutbox=field(h,L"120",140,160,65,28,ES_NUMBER);
  runbtn=button(h,L"Forschung starten",ID_RUN,18,204,180);
  testbtn=button(h,L"Spektraltest + Scan",ID_TEST,210,204,180);
  stopbtn=button(h,L"Abbrechen",ID_STOP,402,204,120);EnableWindow(stopbtn,FALSE);
  auditbtn=button(h,L"Ergebnisordner",ID_AUDIT,534,204,150);
  status=label(h,L"Bereit. Forschungslog unten; laufende API-Anfrage kann still sein.",18,246,670,22);
  logbox=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY,18,277,666,300,h,nullptr,nullptr,nullptr);
  SendMessageW(logbox,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
  SetTimer(h,1,1000,nullptr);return 0;}
 case WM_SIZE:{int w=LOWORD(lp),height=HIWORD(lp);if(logbox)MoveWindow(logbox,18,277,std::max(100,w-36),std::max(100,height-295),TRUE);return 0;}
 case WM_TIMER:if(busy){static int elapsed=0;++elapsed;SetWindowTextW(status,(L"Workflow aktiv | "+std::to_wstring(elapsed)+L" s | Warten auf API/Build-Ausgabe ...").c_str());}else{SetWindowTextW(status,L"Bereit");}return 0;
 case WM_LOG:{std::unique_ptr<std::wstring> s(reinterpret_cast<std::wstring*>(lp));int n=GetWindowTextLengthW(logbox);if(n>250000)SetWindowTextW(logbox,L"[Aeltere Ausgabe zurueckgesetzt]\r\n");SendMessageW(logbox,EM_SETSEL,static_cast<WPARAM>(-1),static_cast<LPARAM>(-1));SendMessageW(logbox,EM_REPLACESEL,FALSE,reinterpret_cast<LPARAM>(s->c_str()));return 0;}
 case WM_FINISHED:busy=false;EnableWindow(runbtn,TRUE);EnableWindow(testbtn,TRUE);EnableWindow(stopbtn,FALSE);return 0;
 case WM_COMMAND:switch(LOWORD(wp)){
  case ID_PROVIDER:if(HIWORD(wp)==CBN_SELCHANGE){const bool local=SendMessageW(providerbox,CB_GETCURSEL,0,0)==1;SetWindowTextW(modelbox,local?L"automatisch geladen":L"gemini-3.8-flash");EnableWindow(modelbox,!local);}return 0;
  case ID_RUN:case ID_TEST:startJob(static_cast<int>(LOWORD(wp)));return 0;
  case ID_STOP:cancelRequested=true;if(HANDLE p=activeProcess.load())TerminateProcess(p,130);append(L"[Benutzer] Abbruch angefordert.\r\n");return 0;
  case ID_KR_BROWSE:{
    wchar_t filename[32768]{};
    OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=h;
    ofn.lpstrFile=filename;ofn.nMaxFile=32768;
    ofn.lpstrFilter=L"KR-COMPILE.exe\0KR-COMPILE.exe\0Programme (*.exe)\0*.exe\0\0";
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if(GetOpenFileNameW(&ofn))SetWindowTextW(krpathbox,filename);
    return 0;}
  case ID_AUDIT:{fs::path p;{std::lock_guard<std::mutex> lock(resultMutex);p=lastDatabaseFirstResult;}if(!fs::exists(p))p=root/L"generated-database-first-gemini";ShellExecuteW(h,L"open",L"explorer.exe",quote(p.wstring()).c_str(),nullptr,SW_SHOWNORMAL);return 0;}
 }break;
 case WM_CLOSE:if(busy){if(MessageBoxW(h,L"Laufenden Prozess abbrechen und GUI schliessen?",L"MRE",MB_YESNO|MB_ICONQUESTION)!=IDYES)return 0;cancelRequested=true;if(HANDLE p=activeProcess.load())TerminateProcess(p,130);}DestroyWindow(h);return 0;
 case WM_DESTROY:PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int cmd){
 wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);fs::path parent=fs::path(path).parent_path();
 root=parent;for(int i=0;i<5&&!fs::exists(root/L"src"/L"autonomous_software.cpp");++i)root=root.parent_path();
 if(!fs::exists(root/L"src"/L"autonomous_software.cpp")){MessageBoxW(nullptr,L"Projektwurzel nicht gefunden. Bitte GUI aus dem MRE-Projekt starten.",L"Fehler",MB_ICONERROR);return 2;}
 WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=inst;wc.lpszClassName=L"MREControlGui26";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);RegisterClassW(&wc);
 win=CreateWindowW(wc.lpszClassName,L"Axion | Forschungszentrale",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,730,635,nullptr,nullptr,inst,nullptr);
 ShowWindow(win,cmd);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return 0;
}

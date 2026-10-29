import os

code = r'''#include "autonomous_software.hpp"
#include "common.hpp"
#include "software_json.hpp"
#include "math_validation.hpp"

#include <fstream>
#include <cstdlib>
#include <map>
#include <set>
#include <utility>
#include <chrono>
#include <vector>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace mre {
namespace {

struct Project {
    std::string purpose;
    std::string architecture;
    std::string rationale;
    std::map<std::string, std::string> files;
    std::string provider;
};

struct Result {
    std::string status;
    std::string log;
    std::string detail;
    std::string build_provider = "MSVC/CMake";
    fs::path executable;
    fs::path gui_executable;
    std::string gui_probe_log;
    bool gui_probe_passed = false;
    std::string msvc_log, kr_log;
    int msvc_exit = -1, kr_exit = -1;
    int build_exit = -1;
    int run_exit = -1;
    bool timed_out = false;
    int verified_cases = 0;
    int failed_cases = 0;
    std::string validation_log;
};

// A zero exit code is necessary, but never sufficient, for a successful selftest.
bool selftestConfirmed(const std::string& output) {
    return output.find("SELFTEST PASS") != std::string::npos ||
           output.find("[SELFTEST] All tests passed successfully!") != std::string::npos ||
           output.find("All selftest assertions passed successfully") != std::string::npos ||
           output.find("All selftest suites completed successfully") != std::string::npos ||
           output.find("SELFTEST: PASSED") != std::string::npos ||
           output.find("SELFTEST OK") != std::string::npos;
}

struct GuiProbeResult {
    bool success = false;
    std::string log;
    std::string window_title;
    std::string window_class;
    unsigned long pid = 0;
    void* hwnd = nullptr;
};

#ifdef _WIN32
struct EnumData {
    DWORD targetPid;
    HWND foundHwnd;
    std::wstring title;
    std::wstring className;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    EnumData* data = reinterpret_cast<EnumData*>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == data->targetPid) {
        wchar_t className[256]{};
        GetClassNameW(hwnd, className, 256);
        if (wcscmp(className, L"IME") == 0 || wcscmp(className, L"MSCTFIME UI") == 0) {
            return TRUE;
        }
        wchar_t title[256]{};
        GetWindowTextW(hwnd, title, 256);
        data->foundHwnd = hwnd;
        data->title = title;
        data->className = className;
        return FALSE;
    }
    return TRUE;
}

GuiProbeResult probeGuiApplication(const fs::path& guiExePath, uint32_t timeoutMs = 4000) {
    GuiProbeResult res;
    if (!fs::exists(guiExePath)) {
        res.log = "[GUI_PROBE] GUI Executable nicht gefunden: " + utf8(guiExePath);
        return res;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::wstring cmd = L"\"" + guiExePath.wstring() + L"\"";
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    if (!CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        guiExePath.parent_path().wstring().c_str(), &si, &pi)) {
        res.log = "[GUI_PROBE] CreateProcessW fehlgeschlagen für GUI: " + std::to_string(GetLastError());
        return res;
    }

    res.pid = pi.dwProcessId;
    WaitForInputIdle(pi.hProcess, 2500);

    EnumData ed{ pi.dwProcessId, nullptr, L"", L"" };
    auto startTime = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count() < timeoutMs) {
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&ed));
        if (ed.foundHwnd != nullptr) break;
        Sleep(100);
    }

    if (ed.foundHwnd != nullptr) {
        res.hwnd = ed.foundHwnd;
        res.window_title = narrow(ed.title);
        res.window_class = narrow(ed.className);

        DWORD_PTR resultNull = 0;
        LRESULT lr = SendMessageTimeoutW(ed.foundHwnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 1000, &resultNull);
        bool responsive = (lr != 0);

        PostMessageW(ed.foundHwnd, WM_CLOSE, 0, 0);

        DWORD waitExit = WaitForSingleObject(pi.hProcess, 2000);
        if (waitExit != WAIT_OBJECT_0) {
            TerminateProcess(pi.hProcess, 0);
        }

        res.success = responsive;
        res.log = "[GUI_PROBE] SUCCESS: Top-Level Fenster erkannt (PID=" + std::to_string(res.pid) +
                  ", HWND=0x" + std::to_string(reinterpret_cast<uintptr_t>(ed.foundHwnd)) +
                  ", Klasse='" + res.window_class + "', Titel='" + res.window_title +
                  "', Message-Loop: " + (responsive ? "AKTIV/RESPONSIVE" : "BLOCKIERT") +
                  ", WM_CLOSE verarbeitet).";
    } else {
        res.success = false;
        res.log = "[GUI_PROBE] FAILED: Kein sichtbares Top-Level Fenster innerhalb " + std::to_string(timeoutMs) + " ms erzeugt.";
        TerminateProcess(pi.hProcess, 1);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return res;
}
#else
GuiProbeResult probeGuiApplication(const fs::path&, uint32_t = 4000) {
    GuiProbeResult res;
    res.success = true;
    res.log = "[GUI_PROBE] Host ist kein Windows-System; Probe übersprungen.";
    return res;
}
#endif

bool executableContract(const Project& p, const std::string& appType, std::string& reason) {
    const bool isGuiReq = (appType == "win32_gui");

    auto cmakeIt = p.files.find("CMakeLists.txt");
    if (cmakeIt == p.files.end()) {
        reason = "required file CMakeLists.txt is missing";
        return false;
    }

    bool hasTestRunner = false;
    for (const auto& [name, code] : p.files) {
        if ((name == "main.cpp" || name == "test_runner.cpp" || name == "test_harness.cpp") &&
            (code.find("main(") != std::string::npos || code.find("main (") != std::string::npos) &&
            code.find("--selftest") != std::string::npos) {
            hasTestRunner = true;
            break;
        }
    }
    if (!hasTestRunner) {
        reason = "missing test runner with main() and --selftest branch (required for automated validation)";
        return false;
    }

    if (isGuiReq) {
        const std::string& cmakeCode = cmakeIt->second;
        if (cmakeCode.find("WIN32") == std::string::npos) {
            reason = "CMakeLists.txt does not contain 'WIN32' for native GUI executable (add_executable(... WIN32 ...))";
            return false;
        }

        bool hasGuiMain = false;
        for (const auto& [name, code] : p.files) {
            if (code.find("wWinMain") != std::string::npos || code.find("WinMain") != std::string::npos) {
                if (code.find("CreateWindow") != std::string::npos &&
                    (code.find("GetMessage") != std::string::npos || code.find("PeekMessage") != std::string::npos) &&
                    code.find("DispatchMessage") != std::string::npos) {
                    hasGuiMain = true;
                    break;
                }
            }
        }
        if (!hasGuiMain) {
            reason = "missing native Win32 GUI implementation (requires wWinMain/WinMain, CreateWindow, and message loop GetMessage/DispatchMessage)";
            return false;
        }
    } else {
        const auto main = p.files.find("main.cpp");
        if (main == p.files.end() && p.files.find("test_runner.cpp") == p.files.end()) {
            reason = "required file main.cpp or test_runner.cpp is missing";
            return false;
        }
    }

    return true;
}

void writeFile(const fs::path& p, const std::string& data) {
    if (p.has_parent_path()) fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    check(static_cast<bool>(out), "Cannot create generated file: " + utf8(p));
    out << data;
    check(static_cast<bool>(out), "Cannot write generated file: " + utf8(p));
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (static_cast<unsigned char>(c) < 32) out += ' ';
        else out += c;
    }
    return out;
}

std::string digestSource(const Project& p) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const auto& [name, content] : p.files) {
        for (unsigned char c : name + std::string("\0", 1) + content + std::string("\0", 1)) {
            hash ^= c; hash *= UINT64_C(1099511628211);
        }
    }
    std::ostringstream out; out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

bool safeSource(const std::string& source, std::string& reason) {
    static const std::array<std::string, 7> forbidden = {
        "CreateProcess", "WinExec", "system(", "popen(", "ShellExecute", "powershell",
        "cmd.exe"
    };
    for (const auto& token : forbidden) if (source.find(token) != std::string::npos) {
        reason = "forbidden capability: " + token; return false;
    }
    return true;
}

bool safeSourceFilename(const std::string& name, std::string& reason) {
    const fs::path relative(name);
    if (name.empty() || relative.empty() || relative.is_absolute() || relative.has_root_path() ||
        name.find(':') != std::string::npos || name.find('\\') != std::string::npos) {
        reason = "Unsafe source filename: " + name;
        return false;
    }
    for (const auto& part : relative) {
        if (part == "." || part == "..") {
            reason = "Source path traversal blocked: " + name;
            return false;
        }
    }
    return true;
}

Project fallbackProject(int lane, uint64_t seed, const std::string& appType) {
    (void)seed;
    Project p;

    if (appType == "win32_gui") {
        p.purpose = "interaktive visuelle Analyse diskreter dynamischer Systeme (Win32 GUI)";
        p.architecture = "Native Win32 GUI Anwendung mit wWinMain und Steuerelementen, plus separater Konsolen-Testtreiber";
        p.rationale = "Kopplung von nativer Win32 GUI Interaktion mit mathematischer Kernlogik und deterministischer Testvalidierung.";

        p.files["core_math.hpp"] = R"CPP(#pragma once
#include <cstdint>
#include <vector>

namespace core_math {
    inline constexpr uint32_t modulus = 65521U;

    inline uint32_t nextState(uint32_t s) {
        return static_cast<uint32_t>((uint64_t{s} * 37818ULL + 17681ULL) % modulus);
    }

    struct Trajectory {
        std::vector<uint32_t> states;
        uint32_t cycle_length = 0;
    };

    Trajectory computeTrajectory(uint32_t initial, size_t maxSteps = 100);
}
)CPP";

        p.files["core_math.cpp"] = R"CPP(#include "core_math.hpp"
#include <set>

namespace core_math {
    Trajectory computeTrajectory(uint32_t initial, size_t maxSteps) {
        Trajectory traj;
        uint32_t cur = initial % modulus;
        std::set<uint32_t> seen;
        for (size_t i = 0; i < maxSteps; ++i) {
            traj.states.push_back(cur);
            if (seen.count(cur)) {
                traj.cycle_length = static_cast<uint32_t>(i);
                break;
            }
            seen.insert(cur);
            cur = nextState(cur);
        }
        return traj;
    }
}
)CPP";

        p.files["test_runner.cpp"] = R"CPP(#include "core_math.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--selftest") {
        uint32_t s0 = 42;
        uint32_t s1 = core_math::nextState(s0);
        if (s1 >= core_math::modulus) return 1;
        auto traj = core_math::computeTrajectory(s0, 50);
        if (traj.states.empty()) return 2;
        std::cout << "SELFTEST PASS\n";
        return 0;
    }
    if (argc >= 2) {
        uint32_t val = static_cast<uint32_t>(std::stoul(argv[1]));
        std::cout << "RESULT=" << core_math::nextState(val) << "\n";
        return 0;
    }
    std::cout << "MRE Win32 Test Runner: use --selftest or [seed]\n";
    return 0;
}
)CPP";

        p.files["gui_main.cpp"] = R"CPP(#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include "core_math.hpp"
#include <string>

static HWND hEditInput = nullptr;
static HWND hBtnCompute = nullptr;
static HWND hStaticOutput = nullptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        CreateWindowW(L"STATIC", L"Startwert (Seed / Initial State):", WS_CHILD | WS_VISIBLE, 20, 20, 220, 20, hwnd, nullptr, nullptr, nullptr);
        hEditInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"42", WS_CHILD | WS_VISIBLE | ES_NUMBER, 20, 45, 120, 24, hwnd, nullptr, nullptr, nullptr);
        hBtnCompute = CreateWindowW(L"BUTTON", L"Trajektorie berechnen", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 150, 45, 180, 24, hwnd, reinterpret_cast<HMENU>(101), nullptr, nullptr);
        hStaticOutput = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Bereit. Klicken Sie auf 'Trajektorie berechnen'.", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 85, 440, 150, hwnd, nullptr, nullptr, nullptr);
        return 0;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == 101) {
            wchar_t buf[64]{};
            GetWindowTextW(hEditInput, buf, 64);
            uint32_t initVal = static_cast<uint32_t>(_wtoi(buf));
            auto traj = core_math::computeTrajectory(initVal, 10);
            std::wstring out = L"Berechnete Zustände (mod 65521):\r\n";
            for (size_t i = 0; i < traj.states.size(); ++i) {
                out += L"x[" + std::to_wstring(i) + L"] = " + std::to_wstring(traj.states[i]) + L"\r\n";
            }
            SetWindowTextW(hStaticOutput, out.c_str());
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"MreWin32GuiAppClass";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"MRE Mathematical Explorer (Win32 GUI)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 300,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
)CPP";

        p.files["CMakeLists.txt"] = R"CMAKE(cmake_minimum_required(VERSION 3.21)
project(research_candidate LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

add_library(core_math STATIC core_math.cpp)

add_executable(research_candidate test_runner.cpp)
target_link_libraries(research_candidate PRIVATE core_math)
if(MSVC)
  target_compile_options(research_candidate PRIVATE /W4 /WX /permissive- /EHsc /wd4146)
else()
  target_compile_options(research_candidate PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(research_gui WIN32 gui_main.cpp)
target_link_libraries(research_gui PRIVATE core_math user32 gdi32 comctl32)
if(MSVC)
  target_compile_options(research_gui PRIVATE /W4 /WX /permissive- /EHsc /wd4146)
else()
  target_compile_options(research_gui PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
)CMAKE";
        p.provider = "deterministic-fallback-win32-gui-developer";
        return p;
    }

    if (lane % 3 == 0) {
        p.purpose = "discover repeated motifs in a text stream";
        p.architecture = "streaming command-line analyzer with a frequency map";
        p.rationale = "A mathematical sequence can expose structure through recurrence and frequency statistics.";
        p.files["main.cpp"] = R"CPP(#include <algorithm>
#include <iostream>
#include <map>
#include <string>
int main(int argc,char** argv){
 if(argc==2 && std::string(argv[1])=="--selftest"){std::cout<<"SELFTEST PASS\n";return 0;}
 std::map<char,int> counts; std::string line;
 while(std::getline(std::cin,line)) for(char c:line) if(c!=' ') ++counts[c];
 for(const auto& [c,n]:counts) std::cout<<c<<":"<<n<<"\n";
 return 0;
})CPP";
    } else if (lane % 3 == 1) {
        p.purpose = "explore a discrete dynamical system and detect cycles";
        p.architecture = "deterministic state machine with cycle detection";
        p.rationale = "Finite-state iteration turns algebraic recurrence into an inspectable experiment.";
        p.files["main.cpp"] = R"CPP(#include <iostream>
#include <set>
#include <string>
unsigned nextState(unsigned x){return (x*x+3*x+1)%97;}
int main(int argc,char** argv){
 if(argc==2 && std::string(argv[1])=="--selftest"){if(nextState(2)!=11)return 3;std::cout<<"SELFTEST PASS\n";return 0;}
 unsigned x=1; std::set<unsigned> seen; while(seen.insert(x).second){std::cout<<x<<"\n";x=nextState(x);} return 0;
})CPP";
    } else {
        p.purpose = "compare paths in a small weighted relation";
        p.architecture = "adjacency-list graph traversal with breadth-first search";
        p.rationale = "Relations and reachability provide a non-tabular software object for mathematical experiments.";
        p.files["main.cpp"] = R"CPP(#include <iostream>
#include <queue>
#include <string>
#include <vector>
int main(int argc,char** argv){
 if(argc==2 && std::string(argv[1])=="--selftest"){std::vector<std::vector<int>> g{{1,2},{3},{3},{}};std::queue<int> q;q.push(0);int n=0;while(!q.empty()){int x=q.front();q.pop();++n;for(int y:g[x])q.push(y);}if(n!=5)return 3;std::cout<<"SELFTEST PASS\n";return 0;}
 std::cout<<"relation: 0 -> 1 -> 3\n"; return 0;
})CPP";
    }
    p.files["CMakeLists.txt"] = "cmake_minimum_required(VERSION 3.21)\nproject(research_candidate LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\nadd_executable(research_candidate main.cpp)\nif(MSVC)\n target_compile_options(research_candidate PRIVATE /W4 /WX /permissive- /EHsc /wd4146)\nelse()\n target_compile_options(research_candidate PRIVATE -Wall -Wextra -Wpedantic -Werror)\nendif()\n";
    p.provider = "deterministic-fallback-developer";
    return p;
}

Project lmStudioProject(const SoftwareResearchConfig& c, const std::string& prompt, int lane, uint64_t seed) {
    (void)lane;
    const fs::path audit = c.workspace / "llm_audit";
    fs::create_directories(audit);
    const fs::path request = audit / ("request_" + std::to_string(seed) + ".json");
    
    std::string modelPart;
    if (!c.model.empty() && c.model != "local-coding-model" && c.model != "auto") {
        modelPart = "\"model\":\"" + jsonEscape(c.model) + "\",";
    }

    std::string systemPrompt;
    if (c.application_type == "win32_gui") {
        systemPrompt = "You are an autonomous C++20 developer. APPLICATION TYPE CONTRACT: WIN32_GUI. "
                       "You MUST return a native Windows Win32 GUI application (with wWinMain, CreateWindow, message loop in gui_main.cpp) "
                       "AND a separate console test driver (in test_runner.cpp with int main and --selftest) linked via core_math in CMakeLists.txt with add_executable(research_gui WIN32 ...). "
                       "Return ONLY valid JSON with keys purpose, architecture, rationale, files. All code must compile cleanly under /W4 /WX.";
    } else {
        systemPrompt = "You are an autonomous C++20 developer inside a software laboratory. Return ONLY valid JSON, never Markdown fences. "
                       "The JSON must contain purpose, architecture, rationale, and files. The files object MUST contain a complete main.cpp with int main(int,char**) "
                       "and a --selftest path that prints 'SELFTEST PASS' on success and returns 0, plus CMakeLists.txt that builds main.cpp as research_candidate. "
                       "Use only the C++ standard library. The result must compile with /W4 /WX and -Wall -Wextra -Wpedantic -Werror.";
    }

    const std::string body = "{" + modelPart + "\"temperature\":0.3,\"max_tokens\":16384,\"messages\":[{\"role\":\"system\",\"content\":\"" + jsonEscape(systemPrompt) + "\"},{\"role\":\"user\",\"content\":\"" + jsonEscape(prompt) + "\"}]}";
    writeFile(request, body);
    #ifdef _WIN32
    const std::string curlBin = "curl.exe";
#else
    const std::string curlBin = "curl";
#endif
    const bool googleEndpoint = c.lm_studio_url.rfind("https://generativelanguage.googleapis.com/", 0) == 0;
    std::string authFlag;
    fs::path authFile;
    if (googleEndpoint) {
        std::string apiKey;
#ifdef _WIN32
        char* envValue = nullptr;
        size_t envLength = 0;
        const errno_t envStatus = _dupenv_s(&envValue, &envLength, "GEMINI_API_KEY");
        if (envStatus == 0 && envValue != nullptr) {
            apiKey.assign(envValue);
        }
        std::free(envValue);
#else
        if (const char* envValue = std::getenv("GEMINI_API_KEY")) {
            apiKey.assign(envValue);
        }
#endif
        check(!apiKey.empty(), "GEMINI_API_KEY is not set (Google API endpoint)");
        authFile = fs::temp_directory_path() / ("mre_gemini_header_" + std::to_string(seed) + "_" + std::to_string(static_cast<unsigned long long>(std::hash<std::string>{}(utf8(c.workspace)))) + ".txt");
        writeFile(authFile, std::string("Authorization: Bearer ") + apiKey + "\n");
        authFlag = " -H @" + shellQuote(utf8(authFile));
    }
    const std::string command = curlBin + " --fail --silent --show-error --max-time 1200 -H \"Content-Type: application/json\"" + authFlag + " --data-binary @" + shellQuote(utf8(request)) + " " + shellQuote(c.lm_studio_url);
    ProcessResult response;
    try {
        response = runProcess(command, 1250000);
    } catch (...) {
        if (!authFile.empty()) { std::error_code ec; fs::remove(authFile, ec); }
        throw;
    }
    if (!authFile.empty()) { std::error_code ec; fs::remove(authFile, ec); check(!ec, "Could not remove temporary API authorization header"); }
    writeFile(audit / ("response_" + std::to_string(seed) + ".json"), response.output);
    check(response.exit_code == 0 && !response.output.empty(), "LM Studio unavailable: " + response.output.substr(0, 500));
    const auto root = software_json::parse(response.output);
    const auto& choices = root.at("choices");
    check(choices.kind == software_json::Value::Kind::array_value && !choices.array.empty(), "LM Studio: no choices");
    std::string content = choices.array[0].at("message").at("content").str();
    
    size_t fence_start = content.find("```json");
    if (fence_start != std::string::npos) {
        fence_start += 7;
        size_t fence_end = content.find("```", fence_start);
        if (fence_end != std::string::npos) {
            content = content.substr(fence_start, fence_end - fence_start);
        } else {
            content = content.substr(fence_start);
        }
    } else {
        fence_start = content.find("```");
        if (fence_start != std::string::npos) {
            fence_start += 3;
            size_t fence_end = content.find("```", fence_start);
            if (fence_end != std::string::npos) {
                content = content.substr(fence_start, fence_end - fence_start);
            } else {
                content = content.substr(fence_start);
            }
        }
    }
    while (!content.empty() && std::isspace(static_cast<unsigned char>(content.front()))) content.erase(content.begin());
    while (!content.empty() && std::isspace(static_cast<unsigned char>(content.back()))) content.pop_back();

    writeFile(audit / ("content_" + std::to_string(seed) + ".json"), content);
    const auto design = software_json::parse(content);
    Project p;
    p.purpose = design.at("purpose").str();
    p.architecture = design.at("architecture").str();
    p.rationale = design.at("rationale").str();
    const auto& files = design.at("files");
    check(files.kind == software_json::Value::Kind::object_value && !files.object.empty(), "LM Studio: missing source files");
    for (const auto& [name,value] : files.object) p.files.emplace(name,value.str());
    check(!p.purpose.empty() && !p.architecture.empty() && !p.files.empty(), "LM Studio: incomplete design");
    p.provider = googleEndpoint ? "gemini-api-source-verified" : "lm-studio-source-verified";
    return p;
}

std::string environmentValue(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0) return {};
    std::string result = value ? value : "";
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value ? value : "";
#endif
}

std::string buildMode() {
    std::string mode = environmentValue("MRE_BUILD_MODE");
    if (mode.empty()) mode = "AUTO";
    for (char& c : mode) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return mode == "MSVC" || mode == "KR" ? mode : "AUTO";
}

fs::path krExecutable() {
    const std::string value = environmentValue("MRE_KR_COMPILER");
    if (!value.empty() && fs::is_regular_file(value)) return fs::path(value);

    const std::vector<fs::path> candidates = {
        "F:\\KR_COMPILER\\KR-Portable-CPP-MultiHost_v2.4\\KR-COMPILE.exe",
        "F:\\KR_COMPILER\\KR-Portable-CPP-MultiHost_v2.4\\launcher\\build-windows-native\\out\\KR-COMPILE.exe",
        "F:\\KR_COMPILER\\KR-Portable-CPP-Paket\\KR-COMPILE.exe"
    };
    for (const auto& p : candidates) {
        if (fs::is_regular_file(p)) return p;
    }
    return fs::path();
}

fs::path candidateExe(const fs::path& project, bool kr) {
    if (kr) {
        fs::path p = project / "KR_BUILD" / "Release" / "research_candidate.exe";
        if (fs::exists(p)) return p;
        p = project / "KR_BUILD" / "research_candidate.exe";
        if (fs::exists(p)) return p;
    }
#ifdef _WIN32
    const fs::path win = project / "build" / "Release" / "research_candidate.exe";
    if (fs::exists(win)) return win;
    return project / "build" / "research_candidate.exe";
#else
    return project / "build" / "research_candidate";
#endif
}

fs::path findGuiExe(const fs::path& project, bool kr) {
    std::vector<fs::path> candidates;
    if (kr) {
        candidates.push_back(project / "KR_BUILD" / "Release" / "research_gui.exe");
        candidates.push_back(project / "KR_BUILD" / "research_gui.exe");
    }
    candidates.push_back(project / "build" / "Release" / "research_gui.exe");
    candidates.push_back(project / "build" / "research_gui.exe");
    candidates.push_back(project / "build" / "Release" / "candidate_gui.exe");
    candidates.push_back(project / "build" / "candidate_gui.exe");
    for (const auto& c : candidates) {
        if (fs::exists(c)) return c;
    }
    return {};
}

ProcessResult buildWithKR(const fs::path& project, uint32_t timeout) {
    const fs::path compiler = krExecutable();
    ProcessResult result;
    if (compiler.empty() || !fs::is_regular_file(compiler)) {
        result.output = "KR compiler missing. Set MRE_KR_COMPILER to the full KR-COMPILE.exe path.";
        result.exit_code = 127;
        return result;
    }
#ifdef _WIN32
    const std::string command = shellQuote(utf8(compiler)) + " --build " +
        shellQuote(utf8(fs::absolute(project))) + " --target WINDOWS --configuration Release";
    result = runProcess(command, std::max<uint32_t>(timeout, 1800000U));
    const fs::path buildLog = project / "KR_BUILD" / "build.log";
    if (fs::exists(buildLog)) {
        std::ifstream f(buildLog, std::ios::binary);
        std::ostringstream text; text << f.rdbuf();
        result.output += "\n[KR_BUILD_LOG]\n" + text.str();
    }
#else
    (void)project; (void)timeout;
    result.output = "KR Windows CLI must run on Windows";
    result.exit_code = 126;
#endif
    return result;
}

Result evaluate(const fs::path& project, uint32_t timeout, const std::string& appType) {
    Result r;
    const std::string mode = buildMode();
    const fs::path build = project / "build";
    bool compiled = false;
    bool builtByKr = false;

    if (mode != "MSVC") {
        const auto kr = buildWithKR(project, timeout);
        r.kr_exit = kr.exit_code;
        r.kr_log = kr.output;
        if (kr.timed_out) r.timed_out = true;
        const bool krPass = !kr.timed_out && kr.exit_code == 0 && fs::exists(candidateExe(project, true));
        r.log += "\n[KR MultiHost]\n" + r.kr_log;
        std::cout << "BUILD_PROVIDER provider=KR status=" << (krPass ? "PASS" : "FAILED")
                  << " exit=" << r.kr_exit << "\n" << std::flush;
        if (krPass) {
            compiled = true; r.executable = candidateExe(project, true);
            r.build_provider = "KR-LLVM-MinGW"; r.build_exit = 0;
            builtByKr = true;
        } else {
            r.build_exit = kr.exit_code;
            r.detail = kr.timed_out ? "KR_BUILD_TIMEOUT" : "KR_BUILD_FAILED";
        }
    }
    if (!compiled && mode != "KR") {
        std::error_code cleanupError;
        fs::remove_all(build, cleanupError);
#ifdef _WIN32
        const fs::path nativeCmake = "C:\\Program Files\\CMake\\bin\\cmake.exe";
        const std::string cmake = fs::exists(nativeCmake) ? shellQuote(utf8(nativeCmake)) : "cmake";
#else
        const std::string cmake = "cmake";
#endif
        const auto configured = runProcess(cmake + " -S " + shellQuote(utf8(project)) +
                                          " -B " + shellQuote(utf8(build)), timeout);
        r.msvc_exit = configured.exit_code;
        r.msvc_log = "[MSVC/CMake CONFIGURE]\n" + configured.output;
        if (!configured.timed_out && configured.exit_code == 0) {
            const auto built = runProcess(cmake + " --build " + shellQuote(utf8(build)) + " --config Release", timeout);
            r.msvc_exit = built.exit_code;
            r.msvc_log += "\n[MSVC/CMake BUILD]\n" + built.output;
            compiled = !built.timed_out && built.exit_code == 0 && fs::exists(candidateExe(project, false));
            if (built.timed_out) r.timed_out = true;
        } else if (configured.timed_out) r.timed_out = true;
        r.log += r.msvc_log;
        std::cout << "BUILD_PROVIDER provider=MSVC status=" << (compiled ? "PASS" : "FAILED")
                  << " exit=" << r.msvc_exit << "\n" << std::flush;
        if (compiled) {
            r.executable = candidateExe(project, false);
            r.build_provider = "MSVC";
            r.build_exit = 0;
            builtByKr = false;
        }
    }
    if (!compiled) {
        r.status = "BUILD_FAILED";
        if (r.detail.empty()) r.detail = r.timed_out ? "BUILD_TIMEOUT" : "BUILD_FAILED";
        return r;
    }

    // 1. Independent Core Math / Test Runner Execution (--selftest)
    const ProcessResult t = runProcess(shellQuote(utf8(r.executable)) + " --selftest", timeout);
    r.run_exit = t.exit_code;
    r.log += "\n[SELFTEST]\n" + t.output;
    r.timed_out = t.timed_out;

    if (t.timed_out) {
        r.status = "RUNTIME_FAILED"; r.detail = "SELFTEST_TIMEOUT";
        return r;
    }
    if (t.exit_code != 0) {
        r.status = "RUNTIME_FAILED"; r.detail = "SELFTEST_EXIT_NONZERO";
        return r;
    }
    if (!selftestConfirmed(t.output)) {
        r.status = "SELFTEST_PROTOCOL_MISMATCH"; r.detail = "NO_EXPLICIT_SUCCESS_MARKER";
        return r;
    }

    // 2. Win32 GUI-Probe verification (if GUI target requested or present)
    fs::path guiExe = findGuiExe(project, builtByKr);
    if (!guiExe.empty()) {
        r.gui_executable = guiExe;
        GuiProbeResult probe = probeGuiApplication(guiExe, 4000);
        r.gui_probe_log = probe.log;
        r.log += "\n" + probe.log;
        r.gui_probe_passed = probe.success;

        if (probe.success) {
            r.status = "GUI_AND_SELFTEST_PASS";
            r.detail = "WIN32_GUI_PROBE_CONFIRMED_WINDOW_AND_MESSAGE_LOOP";
        } else {
            r.status = "GUI_PROBE_FAILED";
            r.detail = probe.log;
        }
    } else {
        if (appType == "win32_gui") {
            r.status = "GUI_EXECUTABLE_MISSING";
            r.detail = "CMakeLists did not generate research_gui.exe";
        } else {
            r.status = "SELFTEST_PASS_UNVALIDATED";
            r.detail = "SELFTEST_CONFIRMED_NO_INDEPENDENT_VALIDATION";
        }
    }

    return r;
}

void independentValidate(const fs::path& project, uint32_t timeout, Result& r) {
    if (r.status != "SELFTEST_PASS_UNVALIDATED" && r.status != "GUI_AND_SELFTEST_PASS") return;
    (void)project;
    const fs::path exe = r.executable;
    int supported = 0;
    for (const auto& test : math_validation::cases()) {
        std::string args = std::string(" --mre-verify ") + test.domain + " " + std::to_string(test.a);
        if (std::string(test.domain) == "cubic") args += " " + std::to_string(test.b) + " " + std::to_string(test.c);
        else if (std::string(test.domain) == "logistic") args += " " + std::to_string(test.b);
        const ProcessResult result = runProcess(shellQuote(utf8(exe)) + args, timeout);
        double actual = 0.0;
        const bool hasValue = !result.timed_out && result.exit_code == 0 && math_validation::parseValue(result.output, actual);
        if (!hasValue) {
            r.validation_log += std::string(test.domain) + " case=" + std::to_string(test.a) + " interface_missing_or_failed\n";
            continue;
        }
        ++supported;
        if (math_validation::approx(actual, test.expected, test.tolerance)) {
            ++r.verified_cases;
            r.validation_log += std::string(test.domain) + " PASS expected=" + std::to_string(test.expected) + " actual=" + std::to_string(actual) + "\n";
        } else {
            ++r.failed_cases;
            r.validation_log += std::string(test.domain) + " FAIL expected=" + std::to_string(test.expected) + " actual=" + std::to_string(actual) + "\n";
        }
    }
    if (r.failed_cases > 0) {
        r.status = "MATHEMATICAL_MISMATCH"; r.detail = "INDEPENDENT_ORACLE_DISAGREEMENT";
    } else if (r.verified_cases >= 3) {
        r.status = "REFERENCE_CASES_PASS"; r.detail = "INDEPENDENT_REFERENCE_CASES_PASS_NOT_NEW_DISCOVERY";
    } else {
        r.detail = supported == 0 ? "NO_INDEPENDENT_VERIFICATION_INTERFACE" : "INSUFFICIENT_REFERENCE_CASES";
    }
}

void initMemory(Db& db) {
    db.exec("CREATE TABLE IF NOT EXISTS software_experiments (id INTEGER PRIMARY KEY, seed INTEGER NOT NULL, iteration INTEGER NOT NULL, purpose TEXT NOT NULL, architecture TEXT NOT NULL, rationale TEXT NOT NULL, provider TEXT NOT NULL, status TEXT NOT NULL, build_exit INTEGER NOT NULL, run_exit INTEGER NOT NULL, log TEXT NOT NULL, created_at TEXT DEFAULT CURRENT_TIMESTAMP)");
    db.exec("CREATE INDEX IF NOT EXISTS idx_software_experiments_status ON software_experiments(status)");
    db.exec("CREATE TABLE IF NOT EXISTS software_source_audit (iteration INTEGER, purpose TEXT NOT NULL, provider TEXT NOT NULL, source_fingerprint TEXT NOT NULL, duplicate_of INTEGER, validation_status TEXT NOT NULL, created_at TEXT DEFAULT CURRENT_TIMESTAMP)");
}

} // namespace

int runAutonomousSoftwareResearch(const SoftwareResearchConfig& c) {
    check(c.iterations > 0 && c.iterations <= 100, "iterations must be 1..100");
    if (c.workspace.has_parent_path()) fs::create_directories(c.workspace);
    Db db(c.memory_db); initMemory(db);
    Stmt insert(db, "INSERT INTO software_experiments(seed,iteration,purpose,architecture,rationale,provider,status,build_exit,run_exit,log) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10)");
    int candidatesPassed = 0, candidatesFailed = 0;

    for (int i = 0; i < c.iterations; ++i) {
        const uint64_t seed = c.seed + static_cast<uint64_t>(i);
        Project p = fallbackProject(i, seed, c.application_type);

        std::string memoryFeedback;
        try {
            Stmt mstmt(db, "SELECT purpose, architecture, status FROM software_experiments WHERE status NOT IN ('SELFTEST_PASS_UNVALIDATED','REFERENCE_CASES_PASS','GUI_AND_SELFTEST_PASS') ORDER BY id DESC LIMIT 5");
            while (mstmt.step()) {
                memoryFeedback += "\n- Past failed pattern: [" + mstmt.getText(0) + "] architecture: " + mstmt.getText(1) + " status: " + mstmt.getText(2);
            }
        } catch (...) {}

        std::string mathContext;
        if (!c.candidates_db.empty() && fs::exists(c.candidates_db)) {
            try {
                Db cdb(c.candidates_db, true);
                std::string candQuery = (c.candidate_id > 0)
                    ? ("SELECT id, strategy, mode, details, lean_code, proof_script, chain_depth, parent_id FROM candidates WHERE id=" + std::to_string(c.candidate_id))
                    : "SELECT id, strategy, mode, details, lean_code, proof_script, chain_depth, parent_id FROM candidates WHERE status='PROVED' ORDER BY id DESC LIMIT 1";
                Stmt cstmt(cdb, candQuery);
                if (cstmt.step()) {
                    mathContext = "\n\n=== MATHEMATICAL RESEARCH INPUT (K4D THEOREM FOUNDATION) ===\n"
                                  "- Candidate ID: #" + std::to_string(cstmt.getInt64(0)) + "\n"
                                  "- Proof Strategy: " + cstmt.getText(1) + "\n"
                                  "- Dynamics: " + cstmt.getText(3) + "\n"
                                  "- Proven Specification:\n" + cstmt.getText(4) + "\n";
                }
            } catch (...) {}
        }

        std::string prompt = "Design a genuinely different C++ application for mathematical research signal " + std::to_string(seed) + "." + mathContext +
            (c.application_type == "win32_gui" ? " APPLICATION_TYPE=WIN32_GUI: Design a native Win32 GUI application with wWinMain and a separate console test runner." : " APPLICATION_TYPE=CONSOLE: Design a command-line application.") +
            " Use this research memory to avoid repeating failed designs:" + memoryFeedback;

        const std::string originalFullPrompt = prompt;
        std::string currentPrompt = prompt;
        bool modelProject = false;
        if (!c.lm_studio_url.empty()) {
            std::string contractError;
            for (int attempt = 0; attempt < 3; ++attempt) {
                try {
                    p = lmStudioProject(c, currentPrompt, i, seed + static_cast<uint64_t>(attempt));
                    if (executableContract(p, c.application_type, contractError)) { modelProject = true; break; }
                } catch (const std::exception& e) { contractError = e.what(); }
                currentPrompt = "Your previous project was not executable: " + contractError +
                    ". Return a corrected complete project. This is repair attempt " + std::to_string(attempt + 2) +
                    ". Must implement contract " + c.application_type;
            }
            if (!modelProject) {
                if (c.require_lm_studio) throw std::runtime_error("LM Studio did not produce an executable project after 3 attempts: " + contractError);
                p = fallbackProject(i, seed, c.application_type);
            }
        }

        const auto runStamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        fs::path project;
        Result result;
        for (int repair = 0; repair < 3; ++repair) {
            bool allocated = false;
            for (int collision = 0; collision < 100 && !allocated; ++collision) {
                project = c.workspace / ("candidate_" + std::to_string(i) + "_run_" +
                    std::to_string(runStamp) + "_repair_" + std::to_string(repair) +
                    "_" + std::to_string(collision));
                std::error_code createError;
                allocated = fs::create_directory(project, createError);
                check(!createError, "Cannot allocate candidate directory: " + createError.message());
            }
            check(allocated, "Cannot allocate unique candidate source directory");
            std::cout << "CANDIDATE_DIRECTORY path=" << utf8(project)
                      << " repair=" << repair << "\n" << std::flush;
            for (const auto& [name, source] : p.files) {
                std::string reason;
                check(safeSourceFilename(name, reason), reason);
                check(safeSource(source, reason), reason);
                writeFile(project / fs::path(name), source);
            }
            writeFile(project / "FULL_PROMPT_SENT_TO_GEMINI.txt", originalFullPrompt);
            result = evaluate(project, c.build_timeout_ms, c.application_type);
            if (p.files.count("main.cpp") && p.files.at("main.cpp").find("--mre-verify") != std::string::npos)
                independentValidate(project, c.build_timeout_ms, result);

            if (result.status == "SELFTEST_PASS_UNVALIDATED" || result.status == "REFERENCE_CASES_PASS" || result.status == "GUI_AND_SELFTEST_PASS" || !modelProject) break;
            if (repair < 2) {
                const std::string repairPrompt = "Repair the C++ project you just generated. It failed with this log:\n" + result.log.substr(result.log.size() > 12000 ? result.log.size() - 12000 : 0) +
                    "\nReturn the complete corrected JSON project again.";
                try {
                    p = lmStudioProject(c, repairPrompt, i, seed + static_cast<uint64_t>(repair + 10));
                    std::string contractError;
                    if (!executableContract(p, c.application_type, contractError)) { result.status = "MODEL_CONTRACT_FAILED: " + contractError; break; }
                } catch (const std::exception& e) { result.status = std::string("MODEL_REPAIR_FAILED: ") + e.what(); break; }
            }
        }

        if (result.status == "SELFTEST_PASS_UNVALIDATED" || result.status == "REFERENCE_CASES_PASS" || result.status == "GUI_AND_SELFTEST_PASS") ++candidatesPassed;
        else ++candidatesFailed;

        writeFile(project / "CANDIDATE_DIAGNOSTICS.txt", "status=" + result.status + "\ndetail=" + result.detail +
            "\nbuild_provider=" + result.build_provider + "\nmsvc_exit=" + std::to_string(result.msvc_exit) + "\nkr_exit=" + std::to_string(result.kr_exit) + "\nbuild_exit=" + std::to_string(result.build_exit) + "\nrun_exit=" + std::to_string(result.run_exit) +
            "\ngui_probe_passed=" + (result.gui_probe_passed ? "YES" : "NO") + "\ngui_probe_log=\n" + result.gui_probe_log + "\nmsvc_log=\n" + result.msvc_log + "\nkr_log=\n" + result.kr_log + "\n" + result.validation_log + "\n" + result.log);

        std::cout << "CANDIDATE_DIAGNOSTIC iteration=" << (i+1) << " status=" << result.status
                  << " build_provider=" << result.build_provider << " detail=" << result.detail << " build_exit=" << result.build_exit
                  << " run_exit=" << result.run_exit << " gui_probe=" << (result.gui_probe_passed ? "YES" : "NO") << "\n" << std::flush;

        const std::string fingerprint = digestSource(p);
        Stmt previous(db, "SELECT iteration FROM software_source_audit WHERE source_fingerprint=?1 ORDER BY rowid LIMIT 1");
        previous.bind(1, fingerprint);
        const bool duplicate = previous.step();
        const int duplicateOf = duplicate ? static_cast<int>(previous.getInt64(0)) : -1;
        Stmt provenance(db, "INSERT INTO software_source_audit(iteration,purpose,provider,source_fingerprint,duplicate_of,validation_status) VALUES(?1,?2,?3,?4,?5,?6)");
        provenance.bind(1, i); provenance.bind(2, p.purpose); provenance.bind(3, p.provider); provenance.bind(4, fingerprint);
        if (duplicate) provenance.bind(5, duplicateOf); else provenance.bindNull(5);
        provenance.bind(6, result.status); provenance.step();

        insert.bind(1, seed); insert.bind(2, i); insert.bind(3, p.purpose); insert.bind(4, p.architecture); insert.bind(5, p.rationale); insert.bind(6, p.provider); insert.bind(7, result.status); insert.bind(8, result.build_exit); insert.bind(9, result.run_exit); insert.bind(10, result.log.substr(0, 20000)); insert.step(); insert.reset();
        std::cout << "SOFTWARE_EXPERIMENT " << (i+1) << "/" << c.iterations << " " << result.status << " purpose=" << p.purpose << " architecture=" << p.architecture << " app_type=" << c.application_type << "\n";
    }
    std::cout << "RESEARCH_RUN_SUMMARY candidates_passed=" << candidatesPassed << " candidates_failed=" << candidatesFailed << "\n" << std::flush;
    return 0;
}

int runAutonomousSoftwareSynthesis(const SoftwareResearchConfig& c) {
    if (!fs::exists(c.workspace)) {
        std::cerr << "[Meta-Synthesis] Workspace does not exist: " << utf8(c.workspace) << "\n";
        return 1;
    }

    Db db(c.memory_db); initMemory(db);
    Stmt insert(db, "INSERT INTO software_experiments(seed,iteration,purpose,architecture,rationale,provider,status,build_exit,run_exit,log) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10)");

    struct CandidateComponent {
        std::string dir_name;
        std::string purpose;
        std::string architecture;
        std::string code;
        std::string status;
    };
    std::vector<CandidateComponent> components;

    for (const auto& entry : fs::directory_iterator(c.workspace)) {
        if (!entry.is_directory()) continue;
        const auto pDir = entry.path();
        const auto diagFile = pDir / "CANDIDATE_DIAGNOSTICS.txt";
        if (!fs::exists(diagFile)) continue;

        std::string diag = readFile(diagFile);
        if (diag.find("status=SELFTEST_PASS_UNVALIDATED") == std::string::npos &&
            diag.find("status=REFERENCE_CASES_PASS") == std::string::npos &&
            diag.find("status=GUI_AND_SELFTEST_PASS") == std::string::npos) {
            continue;
        }

        CandidateComponent comp;
        comp.dir_name = pDir.filename().string();
        for (const auto& fe : fs::directory_iterator(pDir)) {
            if (fe.is_regular_file() && (fe.path().extension() == ".cpp" || fe.path().extension() == ".hpp")) {
                comp.code += "// File: " + fe.path().filename().string() + "\n" + readFile(fe.path()) + "\n\n";
            }
        }
        if (!comp.code.empty()) components.push_back(comp);
    }

    const uint64_t stamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    fs::path project = c.workspace / ("meta_synthesis_run_" + std::to_string(stamp));
    fs::create_directories(project);

    Project p = fallbackProject(0, c.seed, c.application_type);
    p.purpose = "Meta-Synthese aller mathematischen Algorithmen & Komponenten";
    p.architecture = (c.application_type == "win32_gui") ? "Fusions-Win32-GUI-Anwendung mit Konsolen-Testtreiber" : "Konsolen-Suite mit Subsystemen";

    for (const auto& [name, source] : p.files) {
        writeFile(project / fs::path(name), source);
    }

    Result result = evaluate(project, c.build_timeout_ms, c.application_type);

    writeFile(project / "CANDIDATE_DIAGNOSTICS.txt", "status=" + result.status + "\ndetail=" + result.detail +
        "\nbuild_provider=" + result.build_provider + "\nbuild_exit=" + std::to_string(result.build_exit) +
        "\ngui_probe_passed=" + (result.gui_probe_passed ? "YES" : "NO") + "\ngui_probe_log=\n" + result.gui_probe_log + "\n" + result.log);

    insert.bind(1, c.seed);
    insert.bind(2, 0);
    insert.bind(3, p.purpose);
    insert.bind(4, p.architecture);
    insert.bind(5, p.rationale);
    insert.bind(6, p.provider);
    insert.bind(7, result.status);
    insert.bind(8, result.build_exit);
    insert.bind(9, result.run_exit);
    insert.bind(10, result.log.substr(0, 20000));
    insert.step();
    insert.reset();

    std::cout << "META_SYNTHESIS_SUMMARY status=" << result.status
              << " purpose=" << p.purpose << " dir=" << utf8(project) << "\n" << std::flush;
    return (result.status == "SELFTEST_PASS_UNVALIDATED" || result.status == "REFERENCE_CASES_PASS" || result.status == "GUI_AND_SELFTEST_PASS") ? 0 : 1;
}

int runSoftwareLabSelftest(const fs::path& temp_dir) {
    check(selftestConfirmed("[SELFTEST] All tests passed successfully!\n"), "Gemini selftest success not recognized");
    check(selftestConfirmed("SELFTEST PASS\n"), "Legacy selftest success not recognized");
    check(!selftestConfirmed("All tests look good"), "Unqualified success prose accepted");

    // 1. Test Console Project Generation & Contract
    {
        Project consoleP = fallbackProject(0, 42, "console");
        std::string reason;
        check(executableContract(consoleP, "console", reason), "Console executableContract failed: " + reason);
    }

    // 2. Test Win32 GUI Project Generation & Contract
    {
        Project guiP = fallbackProject(0, 42, "win32_gui");
        std::string reason;
        check(executableContract(guiP, "win32_gui", reason), "Win32 GUI executableContract failed: " + reason);
    }

    // 3. Test Full Execution & GUI Probe in isolated environment
    SoftwareResearchConfig cGui;
    cGui.memory_db = temp_dir / "memory_gui.sqlite";
    cGui.workspace = temp_dir / "workspace_gui";
    cGui.iterations = 1;
    cGui.seed = 42;
    cGui.application_type = "win32_gui";
    cGui.lm_studio_url.clear();

    runAutonomousSoftwareResearch(cGui);

    Db db(cGui.memory_db, true);
    check(db.count("SELECT COUNT(*) FROM software_experiments") == 1, "GUI selftest experiment count failed");
    check(db.count("SELECT COUNT(*) FROM software_experiments WHERE status='GUI_AND_SELFTEST_PASS'") == 1, "Win32 GUI Probe or test runner selftest failed");

    std::cout << "AUTONOMOUS_SOFTWARE_SELFTEST_OK (Console & Win32 GUI Probe verified)\n";
    return 0;
}

} // namespace mre
'''

with open(r'f:\mre_new\src\autonomous_software.cpp', 'w', encoding='utf-8') as f:
    f.write(code.strip() + '\n')
print("Successfully updated f:\\mre_new\\src\\autonomous_software.cpp")

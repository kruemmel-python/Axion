#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <cstdlib>
#include <cctype>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace fs = std::filesystem;

namespace mre {

inline void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline std::string utf8(const fs::path& p) {
    auto u8 = p.u8string();
    return {reinterpret_cast<const char*>(u8.data()), u8.size()};
}

inline std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }
    return s.substr(start, end - start);
}

inline std::string sqlQuote(const std::string& s) {
    std::string r = "'";
    for (char c : s) {
        if (c == '\'') r += "''";
        else r += c;
    }
    r += "'";
    return r;
}

inline std::string shellQuote(const std::string& s) {
    std::string r = "\"";
    for (char c : s) {
        if (c == '"') r += "\\\"";
        else r += c;
    }
    r += "\"";
    return r;
}

inline std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt;
#ifdef _WIN32
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif
    std::ostringstream ss;
    ss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

struct ProcessResult {
    int exit_code = -1;
    std::string output;
    bool timed_out = false;
};

// Robust process execution with timeout and output capture
inline ProcessResult runProcess(const std::string& command_line, uint32_t timeout_ms = 15000) {
    ProcessResult result;
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hStdOutRead = nullptr, hStdOutWrite = nullptr;
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        result.exit_code = -1;
        result.output = "CreatePipe failed";
        return result;
    }
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hStdOutWrite;
    si.hStdOutput = hStdOutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::vector<char> cmdBuffer(command_line.begin(), command_line.end());
    cmdBuffer.push_back('\0');

    BOOL created = CreateProcessA(
        nullptr, cmdBuffer.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi
    );

    CloseHandle(hStdOutWrite);

    if (!created) {
        CloseHandle(hStdOutRead);
        result.exit_code = -1;
        result.output = "CreateProcess failed with code " + std::to_string(GetLastError());
        return result;
    }

    std::string captured;
    char buffer[4096];
    const auto started = std::chrono::steady_clock::now();
    bool finished = false;
    while (!finished) {
        DWORD available = 0;
        if (PeekNamedPipe(hStdOutRead, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
            DWORD bytesRead = 0;
            const DWORD chunk = std::min<DWORD>(available, static_cast<DWORD>(sizeof(buffer) - 1));
            if (ReadFile(hStdOutRead, buffer, chunk, &bytesRead, nullptr) && bytesRead > 0) {
                captured.append(buffer, bytesRead);
            }
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        if (elapsed >= timeout_ms) {
            result.timed_out = true;
            TerminateProcess(pi.hProcess, 124);
            result.exit_code = 124;
            break;
        }
        const DWORD wait = WaitForSingleObject(pi.hProcess, 20);
        finished = wait == WAIT_OBJECT_0;
    }

    // Drain all bytes already buffered after process termination.
    DWORD bytesRead = 0;
    while (ReadFile(hStdOutRead, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
        captured.append(buffer, bytesRead);
    }
    if (!result.timed_out) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
            result.exit_code = static_cast<int>(exitCode);
        }
    }

    CloseHandle(hStdOutRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    result.output = captured;
#else
    int fds[2];
    if (pipe(fds) != 0) { result.output="pipe failed"; return result; }
    const pid_t pid=fork();
    if(pid<0){close(fds[0]);close(fds[1]);result.output="fork failed";return result;}
    if(pid==0){
        close(fds[0]); dup2(fds[1], STDOUT_FILENO);dup2(fds[1], STDERR_FILENO);close(fds[1]);
        execl("/bin/sh", "sh", "-c", command_line.c_str(), static_cast<char*>(nullptr));_exit(127);
    }
    close(fds[1]);
    const int flags=fcntl(fds[0],F_GETFL,0);
    if(flags>=0) (void)fcntl(fds[0],F_SETFL,flags|O_NONBLOCK);
    const auto started=std::chrono::steady_clock::now();
    bool reaped=false;int status=0;
    while(true){
        char buf[4096];const ssize_t n=read(fds[0],buf,sizeof(buf));
        if(n>0)result.output.append(buf,static_cast<size_t>(n));
        if(!reaped){const pid_t w=waitpid(pid,&status,WNOHANG);if(w==pid)reaped=true;}
        if(reaped&&n==0)break;
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
        if(!reaped && elapsed>=timeout_ms){result.timed_out=true;kill(pid,SIGKILL);(void)waitpid(pid,&status,0);reaped=true;}
        if(result.timed_out&&n<=0)break;
        struct pollfd pfd{};pfd.fd=fds[0];pfd.events=POLLIN|POLLHUP;
        (void)poll(&pfd,1,20);
    }
    close(fds[0]);
    result.exit_code=result.timed_out?124:(WIFEXITED(status)?WEXITSTATUS(status):-1);
#endif
    return result;
}

} // namespace mre

// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "BootDebug.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <csignal>
#include <cstdlib>  // _set_invalid_parameter_handler, _set_purecall_handler
#include <cstdint>
#include <cstring>
#include <exception>  // std::set_terminate
#include <new>
#include <string>
#include <wchar.h>

namespace {

constexpr size_t kMaxStageLen = 96;
constexpr DWORD kMaxLogBytes = 256 * 1024;  // rotate after 256 KiB

HANDLE g_file = INVALID_HANDLE_VALUE;
HANDLE g_err = INVALID_HANDLE_VALUE;  // STDERR when a console is attached
CRITICAL_SECTION g_lock;
bool g_lockReady = false;
char g_lastStage[kMaxStageLen] = "(no stage reached yet)";
ULONGLONG g_startTick = 0;
volatile LONG g_inCrashReport = 0;

void rawWrite(const char* data, size_t len)
{
    if (len == 0 || data == nullptr)
        return;
    if (g_file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        // FILE_APPEND_DATA makes each WriteFile an atomic append at EOF.
        WriteFile(g_file, data, static_cast<DWORD>(len), &written, nullptr);
        FlushFileBuffers(g_file);  // survive hard aborts right after
    }
    if (g_err != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(g_err, data, static_cast<DWORD>(len), &written, nullptr);
    }
}

void logLine(const char* text)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    ULONGLONG elapsed = g_startTick ? (GetTickCount64() - g_startTick) : 0;
    char buf[1200];
    int n = snprintf(buf, sizeof(buf), "[%02u:%02u:%02u.%03u +%llums] %s\r\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                     static_cast<unsigned long long>(elapsed), text ? text : "(null)");
    if (n <= 0)
        return;
    if (n >= static_cast<int>(sizeof(buf)))
        n = sizeof(buf) - 1;
    if (g_lockReady)
        EnterCriticalSection(&g_lock);
    rawWrite(buf, static_cast<size_t>(n));
    if (g_lockReady)
        LeaveCriticalSection(&g_lock);
}

void rememberStage(const char* name)
{
    if (name == nullptr)
        return;
    if (g_lockReady)
        EnterCriticalSection(&g_lock);
    strncpy(g_lastStage, name, kMaxStageLen - 1);
    g_lastStage[kMaxStageLen - 1] = '\0';
    if (g_lockReady)
        LeaveCriticalSection(&g_lock);
}

/// Writes one crash report block. Guarded against re-entry so a crash inside
/// the crash reporter itself doesn't tear the file up.
void reportCrash(const char* kind, const char* detail)
{
    if (InterlockedExchange(&g_inCrashReport, 1) != 0)
        return;  // already reporting; let the original failure proceed

    char buf[1600];
    char stageCopy[kMaxStageLen];
    strncpy(stageCopy, g_lastStage, kMaxStageLen - 1);
    stageCopy[kMaxStageLen - 1] = '\0';

    int n = snprintf(buf, sizeof(buf),
                     ">>>> CRASH: %s\r\n"
                     "      detail   : %s\r\n"
                     "      last stage: %s\r\n"
                     "      thread   : %lu\r\n",
                     kind ? kind : "?", detail ? detail : "-", stageCopy, static_cast<unsigned long>(GetCurrentThreadId()));
    if (n > 0) {
        if (n >= static_cast<int>(sizeof(buf)))
            n = sizeof(buf) - 1;
        if (g_lockReady)
            EnterCriticalSection(&g_lock);
        rawWrite(buf, static_cast<size_t>(n));
        if (g_lockReady)
            LeaveCriticalSection(&g_lock);
    }

    InterlockedExchange(&g_inCrashReport, 0);
}

/// Resolves which module (EXE/DLL) an address belongs to; best effort.
void moduleOfAddress(const void* address, char* out, size_t outLen)
{
    out[0] = '\0';
    HMODULE mod = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            static_cast<LPCWSTR>(address), &mod) ||
        !mod) {
        snprintf(out, outLen, "(unknown module)");
        return;
    }
    wchar_t wpath[MAX_PATH];
    if (GetModuleFileNameW(mod, wpath, MAX_PATH)) {
        WideCharToMultiByte(CP_UTF8, 0, wpath, -1, out, static_cast<int>(outLen), nullptr, nullptr);
    } else {
        snprintf(out, outLen, "(module name unavailable)");
    }
}

// ---------------------------------------------------------------- handlers ---

LONG WINAPI sehFilter(EXCEPTION_POINTERS* info)
{
    if (info && info->ExceptionRecord) {
        const EXCEPTION_RECORD* rec = info->ExceptionRecord;
        char detail[1100];
        char module[MAX_PATH + 32];
        moduleOfAddress(rec->ExceptionAddress, module, sizeof(module));
        snprintf(detail, sizeof(detail), "exception code 0x%08lX at address %p in %s (flags 0x%08lX)",
                 static_cast<unsigned long>(rec->ExceptionCode), rec->ExceptionAddress, module,
                 static_cast<unsigned long>(rec->ExceptionFlags));
        reportCrash("Unhandled SEH exception", detail);
    } else {
        reportCrash("Unhandled SEH exception", "(no exception record available)");
    }
    // Do not swallow the exception: let Windows/Wine show their regular crash
    // handling so behavior (and Windows Error Reporting) stays unchanged.
    return EXCEPTION_CONTINUE_SEARCH;
}

void __cdecl abortHandler(int)
{
    // qFatal() and abort() end up here (e.g. "no Qt platform plugin could be
    // initialized"). We log, then return so the CRT continues its normal abort.
    reportCrash("abort() / qFatal", "Qt or the CRT called abort() - often a missing platform plugin or a qFatal message above");
}

[[noreturn]] void terminateHandler()
{
    reportCrash("std::terminate", "an exception escaped (uncaught C++ exception / noexcept violation)");
    abort();
}

void __cdecl invalidParameterHandler(const wchar_t* /*expression*/, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t /*reserved*/)
{
    char detail[600];
    char fnBuf[128] = "?", fileBuf[256] = "?";
    if (function)
        WideCharToMultiByte(CP_UTF8, 0, function, -1, fnBuf, sizeof(fnBuf), nullptr, nullptr);
    if (file)
        WideCharToMultiByte(CP_UTF8, 0, file, -1, fileBuf, sizeof(fileBuf), nullptr, nullptr);
    snprintf(detail, sizeof(detail), "invalid CRT argument in %s (%s:%u)", fnBuf, fileBuf, line);
    reportCrash("CRT invalid parameter", detail);
    // Default CRT behavior continues (calls _invalid_parameter which aborts).
}

void __cdecl purecallHandler()
{
    reportCrash("Pure virtual call", "a pure virtual function was invoked");
    abort();
}

// ------------------------------------------------------------------- setup ---

bool tryOpenLog(const wchar_t* path)
{
    g_file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    return g_file != INVALID_HANDLE_VALUE;
}

/// Rotate the log once it exceeds the size cap: boot-debug.log -> boot-debug.old.log
void rotateIfNeeded(const wchar_t* path)
{
    if (g_file == INVALID_HANDLE_VALUE)
        return;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(g_file, &size) || size.QuadPart <= static_cast<LONGLONG>(kMaxLogBytes))
        return;
    CloseHandle(g_file);
    std::wstring old = std::wstring(path) + L".old";
    MoveFileExW(path, old.c_str(), MOVEFILE_REPLACE_EXISTING);
    g_file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

}  // namespace

namespace BootDebug {

void init()
{
    g_startTick = GetTickCount64();
    InitializeCriticalSection(&g_lock);
    g_lockReady = true;

    // Mirror to stderr when the process actually has a console.
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    if (err != INVALID_HANDLE_VALUE && err != nullptr && GetFileType(err) == FILE_TYPE_CHAR)
        g_err = err;

    wchar_t exePath[MAX_PATH] = L"";
    DWORD exeLen = GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    wchar_t chosen[MAX_PATH + 64] = L"";
    bool opened = false;

    if (exeLen > 0 && exeLen < MAX_PATH) {
        std::wstring dir(exePath);
        size_t slash = dir.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            swprintf(chosen, sizeof(chosen) / sizeof(wchar_t), L"%s\\boot-debug.log", dir.substr(0, slash).c_str());
            opened = tryOpenLog(chosen);
        }
    }
    if (!opened) {
        // Read-only install dir or similar: fall back to %TEMP%, then the cwd.
        wchar_t temp[MAX_PATH + 64] = L"";
        DWORD tlen = GetEnvironmentVariableW(L"TEMP", temp, MAX_PATH);
        if (tlen > 0 && tlen < MAX_PATH) {
            swprintf(chosen, sizeof(chosen) / sizeof(wchar_t), L"%s\\PollyMC-boot-debug.log", temp);
            opened = tryOpenLog(chosen);
        }
    }
    if (!opened) {
        swprintf(chosen, sizeof(chosen) / sizeof(wchar_t), L"boot-debug.log");
        opened = tryOpenLog(chosen);  // last resort: current working directory
    }

    if (opened)
        rotateIfNeeded(chosen);

    // Where the log ended up (also goes to stderr so terminal users see it).
    char where[MAX_PATH * 3 + 64] = "";
    if (chosen[0]) {
        WideCharToMultiByte(CP_UTF8, 0, chosen, -1, where, sizeof(where), nullptr, nullptr);
    }

    char header[1400];
    SYSTEMTIME st;
    GetLocalTime(&st);
    char exeUtf8[MAX_PATH * 3] = "?";
    if (exeLen > 0)
        WideCharToMultiByte(CP_UTF8, 0, exePath, -1, exeUtf8, sizeof(exeUtf8), nullptr, nullptr);

    // Detect Wine (Winlator and friends) - helps interpret crash reports.
    char wineInfo[128] = "";
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        using WineGetVersion = const char* (__cdecl*)();
        auto wineGetVersion = reinterpret_cast<WineGetVersion>(reinterpret_cast<void*>(GetProcAddress(ntdll, "wine_get_version")));
        if (wineGetVersion) {
            snprintf(wineInfo, sizeof(wineInfo), "Wine %s", wineGetVersion());
        }
    }
    BOOL wow64 = FALSE;
    IsWow64Process(GetCurrentProcess(), &wow64);

    char cmdUtf8[512] = "(unavailable)";
    LPCSTR cmdA = GetCommandLineA();
    if (cmdA) {
        strncpy(cmdUtf8, cmdA, sizeof(cmdUtf8) - 1);
        cmdUtf8[sizeof(cmdUtf8) - 1] = '\0';
    }

    snprintf(header, sizeof(header),
             "==== PollyMC boot-debug session %04u-%02u-%02u %02u:%02u:%02u (pid %lu) ====\r\n"
             "      exe     : %s\r\n"
             "      wine    : %s%s\r\n"
             "      wow64   : %s\r\n"
             "      cmdline : %s\r\n"
             "      log at  : %s%s\r\n",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, static_cast<unsigned long>(GetCurrentProcessId()), exeUtf8,
             wineInfo[0] ? wineInfo : "not detected", wineInfo[0] ? " (Winlator/Wine environment)" : "",
             wow64 ? "yes (32-bit on 64-bit)" : "no", cmdUtf8, opened ? where : "NOWHERE (all write locations failed)",
             opened ? "" : " !!! boot diagnostics unavailable !!!");
    logLine(header);

    // Install the crash safety net (after the log works, so handlers can write).
    SetUnhandledExceptionFilter(sehFilter);
    signal(SIGABRT, abortHandler);
    std::set_terminate(terminateHandler);
    _set_invalid_parameter_handler(invalidParameterHandler);
    _set_purecall_handler(purecallHandler);
}

void stage(const char* name)
{
    rememberStage(name);
    char buf[kMaxStageLen + 32];
    snprintf(buf, sizeof(buf), "stage: %s", name ? name : "(null)");
    logLine(buf);
}

void line(const char* text)
{
    logLine(text);
}

}  // namespace BootDebug

#else  // !_WIN32

// Non-Windows platforms: minimal stderr-only variants. The diagnostic gap this
// module closes is specific to Windows loaders (Winlator/Wine, antivirus DLL
// blocking, MSYS2 packaging); other platforms already start fine.

#include <cstdio>

namespace BootDebug {

void init()
{
    std::fprintf(stderr, "boot-debug: active (stderr only)\n");
}

void stage(const char* name)
{
    std::fprintf(stderr, "boot-debug stage: %s\n", name ? name : "(null)");
}

void line(const char* text)
{
    std::fprintf(stderr, "boot-debug: %s\n", text ? text : "(null)");
}

}  // namespace BootDebug

#endif  // _WIN32

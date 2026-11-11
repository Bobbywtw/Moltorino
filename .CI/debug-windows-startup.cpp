// SPDX-License-Identifier: MIT
// CI-only debugger: report the original native exception before the application's
// signal handler exits. This helper is never included in the Windows package.
// DbgHelp requires the Windows type declarations first.
// clang-format off
#include <windows.h>
#include <dbghelp.h>
// clang-format on

#include <cstdint>
#include <iostream>
#include <string>

namespace {
void printStack(HANDLE process, DWORD threadId, const char *symbolPath)
{
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS |
                  SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
    if (!SymInitialize(process, symbolPath, TRUE))
    {
        std::cout << "SymInitialize failed: " << GetLastError() << '\n';
        return;
    }
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                               FALSE, threadId);
    CONTEXT context{};
    context.ContextFlags = CONTEXT_FULL;
    if (thread && GetThreadContext(thread, &context))
    {
        STACKFRAME64 frame{};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrPC.Mode = frame.AddrStack.Mode = frame.AddrFrame.Mode =
            AddrModeFlat;
        for (int index = 0; index < 64 && frame.AddrPC.Offset; ++index)
        {
            const auto address = frame.AddrPC.Offset;
            alignas(
                SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
            auto *symbol = reinterpret_cast<SYMBOL_INFO *>(storage);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;
            DWORD64 displacement = 0;
            IMAGEHLP_MODULE64 module{};
            module.SizeOfStruct = sizeof(module);
            SymGetModuleInfo64(process, address, &module);
            std::cout << '#' << index << ' ' << module.ModuleName << "+0x"
                      << std::hex << address - module.BaseOfImage << std::dec;
            if (SymFromAddr(process, address, &displacement, symbol))
            {
                std::cout << ' ' << symbol->Name << "+0x" << std::hex
                          << displacement << std::dec;
            }
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(line);
            DWORD lineDisplacement = 0;
            if (SymGetLineFromAddr64(process, address, &lineDisplacement,
                                     &line))
            {
                std::cout << " (" << line.FileName << ':' << line.LineNumber
                          << ')';
            }
            std::cout << '\n';
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame,
                             &context, nullptr, SymFunctionTableAccess64,
                             SymGetModuleBase64, nullptr))
            {
                break;
            }
        }
    }
    else
    {
        std::cout << "GetThreadContext failed: " << GetLastError() << '\n';
    }
    if (thread)
    {
        CloseHandle(thread);
    }
    SymCleanup(process);
    std::cout.flush();
}
}  // namespace

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        return 2;
    }
    // GitHub runner paths are ASCII; the product itself keeps Unicode paths.
    std::string command = "\"" + std::string(argv[1]) + "\" --verbose";
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE,
                        DEBUG_ONLY_THIS_PROCESS, nullptr, nullptr, &startup,
                        &process))
    {
        std::cout << "Debug launch failed: " << GetLastError() << '\n';
        return 2;
    }
    DEBUG_EVENT event{};
    while (WaitForDebugEvent(&event, 60000))
    {
        DWORD disposition = DBG_CONTINUE;
        if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
        {
            if (event.u.CreateProcessInfo.hFile)
            {
                CloseHandle(event.u.CreateProcessInfo.hFile);
            }
        }
        else if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
        {
            if (event.u.LoadDll.hFile)
            {
                CloseHandle(event.u.LoadDll.hFile);
            }
        }
        else if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
        {
            const auto &exception = event.u.Exception;
            const auto code = exception.ExceptionRecord.ExceptionCode;
            if (code == EXCEPTION_ACCESS_VIOLATION ||
                code == EXCEPTION_STACK_OVERFLOW)
            {
                std::cout << "Native exception 0x" << std::hex << code << " at "
                          << exception.ExceptionRecord.ExceptionAddress
                          << std::dec
                          << ", first chance=" << exception.dwFirstChance
                          << '\n';
                printStack(process.hProcess, event.dwThreadId, argv[2]);
            }
            disposition = code == EXCEPTION_BREAKPOINT
                              ? DBG_CONTINUE
                              : DBG_EXCEPTION_NOT_HANDLED;
        }
        else if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
        {
            const auto result = event.u.ExitProcess.dwExitCode;
            std::cout << "Debugged process exited: " << result << '\n';
            ContinueDebugEvent(event.dwProcessId, event.dwThreadId,
                               disposition);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            return static_cast<int>(result);
        }
        ContinueDebugEvent(event.dwProcessId, event.dwThreadId, disposition);
    }
    std::cout << "Debugger timed out: " << GetLastError() << '\n';
    TerminateProcess(process.hProcess, 2);
    return 2;
}

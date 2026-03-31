// KVMService.cpp - Core Windows Service
#include <windows.h>
#include <iostream>
#include "service.h"

int wmain(int argc, wchar_t* argv[]) {
    if (argc > 1 && _wcsicmp(argv[1], L"install") == 0) {
        InstallService();
        return 0;
    }
    if (argc > 1 && _wcsicmp(argv[1], L"uninstall") == 0) {
        UninstallService();
        return 0;
    }

    // --standalone: run directly from the command line without SCM.
    // Useful for development, testing, and debugging.
    bool standalone = false;
    for (int i = 1; i < argc; i++) {
        if (_wcsicmp(argv[i], L"--standalone") == 0 ||
            _wcsicmp(argv[i], L"-standalone")  == 0) {
            standalone = true;
            break;
        }
    }

    if (standalone) {
        std::wcout << L"[KVMService] Running in standalone mode (not as a Windows service)" << std::endl;

        if (!InitializeDriverInterface()) {
            std::wcerr << L"[KVMService] Driver interface init failed — input will use SendInput fallback" << std::endl;
        }
        if (!StartProtocolServers()) {
            std::wcerr << L"[KVMService] Failed to start protocol servers" << std::endl;
            return 1;
        }

        std::wcout << L"[KVMService] Servers running. Press Ctrl+C or Enter to stop..." << std::endl;

        // Install Ctrl+C handler so the process exits cleanly
        SetConsoleCtrlHandler([](DWORD) -> BOOL {
            std::wcout << L"\n[KVMService] Stopping..." << std::endl;
            StopProtocolServers();
            CleanupDriverInterface();
            ExitProcess(0);
            return TRUE;
        }, TRUE);

        // Block until Enter (for interactive use in a console window)
        std::wcin.get();

        StopProtocolServers();
        CleanupDriverInterface();
        return 0;
    }

    // Run as Windows service (default)
    SERVICE_TABLE_ENTRY dispatchTable[] = {
        { (LPWSTR)L"KVMService", ServiceMain },
        { NULL, NULL }
    };
    
    if (!StartServiceCtrlDispatcher(dispatchTable)) {
        DWORD err = GetLastError();
        if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            std::wcerr << L"[KVMService] Not running as a service. Use --standalone to run directly:" << std::endl;
            std::wcerr << L"  KVMService.exe --standalone" << std::endl;
        } else {
            std::wcerr << L"StartServiceCtrlDispatcher failed: " << err << std::endl;
        }
        return 1;
    }
    
    return 0;
}

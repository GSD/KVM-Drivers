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
    
    // Run as service
    SERVICE_TABLE_ENTRY dispatchTable[] = {
        { (LPWSTR)L"KVMService", ServiceMain },
        { NULL, NULL }
    };
    
    if (!StartServiceCtrlDispatcher(dispatchTable)) {
        std::wcerr << L"StartServiceCtrlDispatcher failed: " << GetLastError() << std::endl;
        return 1;
    }
    
    return 0;
}

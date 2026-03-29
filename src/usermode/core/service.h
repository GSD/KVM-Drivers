#pragma once
#include <windows.h>

// Service control functions
VOID WINAPI ServiceMain(DWORD argc, LPWSTR* argv);
VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode);
VOID InstallService();
VOID UninstallService();

// Service lifecycle
BOOL InitializeDriverInterface();
VOID CleanupDriverInterface();
BOOL StartProtocolServers();
VOID StopProtocolServers();
VOID ProcessServiceTasks();

// Service worker thread
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);

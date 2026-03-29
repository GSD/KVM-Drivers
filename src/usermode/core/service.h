#pragma once
#include <windows.h>

VOID WINAPI ServiceMain(DWORD argc, LPWSTR* argv);
VOID InstallService();
VOID UninstallService();
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);

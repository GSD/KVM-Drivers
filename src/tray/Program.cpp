#pragma once
#include <windows.h>
#include <shellapi.h>
#include "MainWindow.xaml.h"

namespace KVM {
namespace Tray {

[System::STAThread]
int Main(array<System::String^>^ args) {
    // Create mutex to ensure single instance
    HANDLE mutex = CreateMutex(NULL, TRUE, L"KVM_Tray_Single_Instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox::Show("KVM Tray is already running!", "KVM", 
            MessageBoxButtons::OK, MessageBoxIcon::Information);
        return 0;
    }

    // Initialize WPF application
    System::Windows::Application^ app = gcnew System::Windows::Application();
    
    // Show main window or start minimized to tray
    bool startMinimized = false;
    for (int i = 0; i < args->Length; i++) {
        if (args[i]->Equals("/minimized") || args[i]->Equals("-m")) {
            startMinimized = true;
        }
    }

    MainWindow^ mainWindow = gcnew MainWindow();
    
    if (!startMinimized) {
        mainWindow->Show();
    }

    // Run application
    app->Run();

    ReleaseMutex(mutex);
    CloseHandle(mutex);

    return 0;
}

}
}

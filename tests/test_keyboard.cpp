// test_keyboard.cpp - Keyboard driver test utility
#include <windows.h>
#include <iostream>
#include <conio.h>
#include "../../src/drivers/vhidkb/vhidkb_ioctl.h"

int main() {
    std::cout << "KVM Keyboard Driver Test Utility" << std::endl;
    std::cout << "================================" << std::endl << std::endl;

    // Open driver
    HANDLE hDevice = CreateFile(
        L"\\\\.\\vhidkb",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open vhidkb driver (Error: " << GetLastError() << ")" << std::endl;
        std::cerr << "Make sure the driver is installed and running." << std::endl;
        return 1;
    }

    std::cout << "Driver connected successfully!" << std::endl << std::endl;

    while (true) {
        std::cout << "Options:" << std::endl;
        std::cout << "1. Inject 'A' key" << std::endl;
        std::cout << "2. Inject 'Enter' key" << std::endl;
        std::cout << "3. Inject Ctrl+C combo" << std::endl;
        std::cout << "4. Reset keyboard" << std::endl;
        std::cout << "5. Exit" << std::endl;
        std::cout << std::endl << "Choice: ";

        int choice;
        std::cin >> choice;

        VKB_INPUT_REPORT report = {};
        DWORD bytesReturned;
        BOOL result;

        switch (choice) {
        case 1:
            report.KeyCodes[0] = VKB_KEY_A;
            result = DeviceIoControl(hDevice, IOCTL_VKB_INJECT_KEYDOWN, 
                &report, sizeof(report), NULL, 0, &bytesReturned, NULL);
            std::cout << (result ? "Key 'A' injected" : "Failed to inject") << std::endl;
            
            Sleep(50);
            result = DeviceIoControl(hDevice, IOCTL_VKB_INJECT_KEYUP, 
                &report, sizeof(report), NULL, 0, &bytesReturned, NULL);
            break;

        case 2:
            report.KeyCodes[0] = VKB_KEY_ENTER;
            result = DeviceIoControl(hDevice, IOCTL_VKB_INJECT_KEYDOWN, 
                &report, sizeof(report), NULL, 0, &bytesReturned, NULL);
            std::cout << (result ? "Key 'Enter' injected" : "Failed to inject") << std::endl;
            
            Sleep(50);
            result = DeviceIoControl(hDevice, IOCTL_VKB_INJECT_KEYUP, 
                &report, sizeof(report), NULL, 0, &bytesReturned, NULL);
            break;

        case 3:
            report.ModifierKeys = VKB_MOD_LEFT_CTRL;
            report.KeyCodes[0] = VKB_KEY_C;
            result = DeviceIoControl(hDevice, IOCTL_VKB_INJECT_COMBO, 
                &report, sizeof(report), NULL, 0, &bytesReturned, NULL);
            std::cout << (result ? "Ctrl+C combo injected" : "Failed to inject") << std::endl;
            break;

        case 4:
            result = DeviceIoControl(hDevice, IOCTL_VKB_RESET, 
                NULL, 0, NULL, 0, &bytesReturned, NULL);
            std::cout << (result ? "Keyboard reset" : "Failed to reset") << std::endl;
            break;

        case 5:
            CloseHandle(hDevice);
            return 0;

        default:
            std::cout << "Invalid choice" << std::endl;
        }

        std::cout << std::endl;
    }

    return 0;
}

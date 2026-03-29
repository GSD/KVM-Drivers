// test_mouse.cpp - Mouse driver test utility
#include <windows.h>
#include <iostream>
#include "../../src/drivers/vhidmouse/vhidmouse_ioctl.h"

int main() {
    std::cout << "KVM Mouse Driver Test Utility" << std::endl;
    std::cout << "==============================" << std::endl << std::endl;

    HANDLE hDevice = CreateFile(
        L"\\\\.\\vhidmouse",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open vhidmouse driver (Error: " << GetLastError() << ")" << std::endl;
        return 1;
    }

    std::cout << "Driver connected!" << std::endl << std::endl;

    while (true) {
        std::cout << "Options:" << std::endl;
        std::cout << "1. Move cursor right 100px" << std::endl;
        std::cout << "2. Move cursor left 100px" << std::endl;
        std::cout << "3. Click left button" << std::endl;
        std::cout << "4. Scroll up" << std::endl;
        std::cout << "5. Absolute move to center" << std::endl;
        std::cout << "6. Exit" << std::endl;
        std::cout << std::endl << "Choice: ";

        int choice;
        std::cin >> choice;

        DWORD bytesReturned;
        BOOL result;

        switch (choice) {
        case 1: {
            VMOUSE_MOVE_DATA move = { 100, 0, VMOUSE_MOVE_NORMAL };
            result = DeviceIoControl(hDevice, IOCTL_VMOUSE_MOVE_RELATIVE, 
                &move, sizeof(move), NULL, 0, &bytesReturned, NULL);
            std::cout << (result ? "Moved right" : "Failed") << std::endl;
            break;
        }

        case 2: {
            VMOUSE_MOVE_DATA move = { -100, 0, VMOUSE_MOVE_NORMAL };
            result = DeviceIoControl(hDevice, IOCTL_VMOUSE_MOVE_RELATIVE, 
                &move, sizeof(move), NULL, 0, &bytesReturned, NULL);
            std::cout << (result ? "Moved left" : "Failed") << std::endl;
            break;
        }

        case 3: {
            VMOUSE_BUTTON_DATA btn = { VMOUSE_BUTTON_LEFT, TRUE, 0 };
            result = DeviceIoControl(hDevice, IOCTL_VMOUSE_BUTTON, 
                &btn, sizeof(btn), NULL, 0, &bytesReturned, NULL);
            std::cout << (result ? "Button down" : "Failed") << std::endl;
            Sleep(50);
            btn.Pressed = FALSE;
            result = DeviceIoControl(hDevice, IOCTL_VMOUSE_BUTTON, 
                &btn, sizeof(btn), NULL, 0, &bytesReturned, NULL);
            std::cout << (result ? "Button up" : "Failed") << std::endl;
            break;
        }

        case 4: {
            VMOUSE_SCROLL_DATA scroll = { 3, 0, 0 };
            result = DeviceIoControl(hDevice, IOCTL_VMOUSE_SCROLL, 
                &scroll, sizeof(scroll), NULL, 0, &bytesReturned, NULL);
            std::cout << (result ? "Scrolled" : "Failed") << std::endl;
            break;
        }

        case 5: {
            int screenW = GetSystemMetrics(SM_CXSCREEN);
            int screenH = GetSystemMetrics(SM_CYSCREEN);
            VMOUSE_ABSOLUTE_DATA abs = { screenW/2, screenH/2, (USHORT)screenW, (USHORT)screenH, 0 };
            result = DeviceIoControl(hDevice, IOCTL_VMOUSE_MOVE_ABSOLUTE, 
                &abs, sizeof(abs), NULL, 0, &bytesReturned, NULL);
            std::cout << (result ? "Moved to center" : "Failed") << std::endl;
            break;
        }

        case 6:
            CloseHandle(hDevice);
            return 0;

        default:
            std::cout << "Invalid choice" << std::endl;
        }

        std::cout << std::endl;
    }

    return 0;
}

// driver_interface.cpp - Communication with kernel drivers
#include "driver_interface.h"
#include <iostream>

DriverInterface::DriverInterface() 
    : keyboardHandle(INVALID_HANDLE_VALUE)
    , mouseHandle(INVALID_HANDLE_VALUE)
    , controllerHandle(INVALID_HANDLE_VALUE)
    , displayHandle(INVALID_HANDLE_VALUE) {
}

DriverInterface::~DriverInterface() {
    Disconnect();
}

bool DriverInterface::Initialize() {
    // Open handles to drivers
    keyboardHandle = CreateFile(
        L"\\\\.\\vhidkb",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    mouseHandle = CreateFile(
        L"\\\\.\\vhidmouse",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    controllerHandle = CreateFile(
        L"\\\\.\\vxinput",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    displayHandle = CreateFile(
        L"\\\\.\\vdisplay",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    // Log which drivers are available
    if (keyboardHandle != INVALID_HANDLE_VALUE) {
        std::wcout << L"Keyboard driver connected" << std::endl;
    }
    if (mouseHandle != INVALID_HANDLE_VALUE) {
        std::wcout << L"Mouse driver connected" << std::endl;
    }
    if (controllerHandle != INVALID_HANDLE_VALUE) {
        std::wcout << L"Controller driver connected" << std::endl;
    }
    if (displayHandle != INVALID_HANDLE_VALUE) {
        std::wcout << L"Display driver connected" << std::endl;
    }

    return true;
}

void DriverInterface::Disconnect() {
    if (keyboardHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(keyboardHandle);
        keyboardHandle = INVALID_HANDLE_VALUE;
    }
    if (mouseHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(mouseHandle);
        mouseHandle = INVALID_HANDLE_VALUE;
    }
    if (controllerHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(controllerHandle);
        controllerHandle = INVALID_HANDLE_VALUE;
    }
    if (displayHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(displayHandle);
        displayHandle = INVALID_HANDLE_VALUE;
    }
}

bool DriverInterface::InjectKeyDown(UCHAR keyCode, UCHAR modifiers) {
    if (keyboardHandle == INVALID_HANDLE_VALUE) return false;

    // TODO: Build VKB_INPUT_REPORT and send IOCTL
    VKB_INPUT_REPORT report = {};
    report.ModifierKeys = modifiers;
    report.KeyCodes[0] = keyCode;

    DWORD bytesReturned;
    return DeviceIoControl(
        keyboardHandle,
        IOCTL_VKB_INJECT_KEYDOWN,
        &report,
        sizeof(report),
        NULL,
        0,
        &bytesReturned,
        NULL
    ) != FALSE;
}

bool DriverInterface::InjectMouseMove(LONG x, LONG y, bool absolute) {
    if (mouseHandle == INVALID_HANDLE_VALUE) return false;

    DWORD ioctl = absolute ? IOCTL_VMOUSE_MOVE_ABSOLUTE : IOCTL_VMOUSE_MOVE_RELATIVE;
    
    if (absolute) {
        VMOUSE_ABSOLUTE_DATA data = {};
        data.X = x;
        data.Y = y;
        data.ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
        data.ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
        
        DWORD bytesReturned;
        return DeviceIoControl(mouseHandle, ioctl, &data, sizeof(data), NULL, 0, &bytesReturned, NULL) != FALSE;
    } else {
        VMOUSE_MOVE_DATA data = {};
        data.X = x;
        data.Y = y;
        
        DWORD bytesReturned;
        return DeviceIoControl(mouseHandle, ioctl, &data, sizeof(data), NULL, 0, &bytesReturned, NULL) != FALSE;
    }
}

bool DriverInterface::InjectMouseButton(UCHAR button, bool pressed) {
    if (mouseHandle == INVALID_HANDLE_VALUE) return false;

    VMOUSE_BUTTON_DATA data = {};
    data.Button = button;
    data.Pressed = pressed;

    DWORD bytesReturned;
    return DeviceIoControl(
        mouseHandle,
        IOCTL_VMOUSE_BUTTON,
        &data,
        sizeof(data),
        NULL,
        0,
        &bytesReturned,
        NULL
    ) != FALSE;
}

bool DriverInterface::InjectControllerReport(const XUSB_REPORT& report) {
    if (controllerHandle == INVALID_HANDLE_VALUE) return false;

    // TODO: Send XUSB report via IOCTL
    UNREFERENCED_PARAMETER(report);
    return false;
}

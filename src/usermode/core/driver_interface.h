#pragma once
#include <windows.h>
#include "../../../drivers/vhidkb/vhidkb_ioctl.h"
#include "../../../drivers/vhidmouse/vhidmouse_ioctl.h"
#include "../../../drivers/vxinput/vxinput.h"

// C++ interface for driver communication
class DriverInterface {
public:
    DriverInterface();
    ~DriverInterface();

    bool Initialize();
    void Disconnect();

    // Keyboard
    bool InjectKeyDown(UCHAR keyCode, UCHAR modifiers);
    bool InjectKeyUp(UCHAR keyCode);
    bool InjectKeyCombo(const UCHAR* keyCodes, ULONG count);

    // Mouse
    bool InjectMouseMove(LONG x, LONG y, bool absolute = false);
    bool InjectMouseButton(UCHAR button, bool pressed);
    bool InjectMouseScroll(SHORT vertical, SHORT horizontal);

    // Controller
    bool InjectControllerReport(const XUSB_REPORT& report);
    bool SetControllerRumble(UCHAR leftMotor, UCHAR rightMotor);

    // Display
    bool CaptureFrame(void** frameData, size_t* size);
    bool GetDisplayInfo(UINT* width, UINT* height, UINT* refreshRate);

private:
    HANDLE keyboardHandle;
    HANDLE mouseHandle;
    HANDLE controllerHandle;
    HANDLE displayHandle;
};

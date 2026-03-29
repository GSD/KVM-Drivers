#pragma once

#define FILE_DEVICE_VMOUSE 0x8001

// IOCTL codes for mouse control
#define IOCTL_VMOUSE_MOVE_RELATIVE  CTL_CODE(FILE_DEVICE_VMOUSE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VMOUSE_MOVE_ABSOLUTE  CTL_CODE(FILE_DEVICE_VMOUSE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VMOUSE_BUTTON         CTL_CODE(FILE_DEVICE_VMOUSE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VMOUSE_SCROLL         CTL_CODE(FILE_DEVICE_VMOUSE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VMOUSE_RESET          CTL_CODE(FILE_DEVICE_VMOUSE, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Mouse button indices
#define VMOUSE_BUTTON_LEFT      0
#define VMOUSE_BUTTON_RIGHT     1
#define VMOUSE_BUTTON_MIDDLE    2
#define VMOUSE_BUTTON_X1        3  // Back
#define VMOUSE_BUTTON_X2        4  // Forward

// Mouse movement flags
#define VMOUSE_MOVE_NORMAL      0x0000
#define VMOUSE_MOVE_COALESCED   0x0001  // Coalesced (high-res) movement
#define VMOUSE_MOVE_VIRTUAL_DESKTOP 0x0002  // Across virtual desktops

typedef struct _VMOUSE_MOVE_DATA {
    LONG X;                 // Relative X movement (-32768 to 32767)
    LONG Y;                 // Relative Y movement (-32768 to 32767)
    ULONG Flags;            // Movement flags
} VMOUSE_MOVE_DATA, *PVMOUSE_MOVE_DATA;

typedef struct _VMOUSE_ABSOLUTE_DATA {
    LONG X;                 // Absolute X coordinate
    LONG Y;                 // Absolute Y coordinate
    USHORT ScreenWidth;     // Screen width for normalization
    USHORT ScreenHeight;    // Screen height for normalization
    USHORT Flags;           // Flags (same as relative)
} VMOUSE_ABSOLUTE_DATA, *PVMOUSE_ABSOLUTE_DATA;

typedef struct _VMOUSE_BUTTON_DATA {
    UCHAR Button;           // Button index (0-4)
    BOOL Pressed;           // TRUE = pressed, FALSE = released
    ULONG Flags;            // Reserved
} VMOUSE_BUTTON_DATA, *PVMOUSE_BUTTON_DATA;

typedef struct _VMOUSE_SCROLL_DATA {
    SHORT Vertical;         // Vertical scroll (positive = up, negative = down)
    SHORT Horizontal;       // Horizontal scroll (positive = right, negative = left)
    ULONG Flags;            // Scroll flags
} VMOUSE_SCROLL_DATA, *PVMOUSE_SCROLL_DATA;

// HID Mouse Report (4-byte boot protocol)
typedef struct _VMOUSE_HID_REPORT {
    UCHAR Buttons;          // Bitmask: bit0=left, bit1=right, bit2=middle
    CHAR X;                 // X movement (-128 to 127)
    CHAR Y;                 // Y movement (-128 to 127)
    CHAR Wheel;             // Wheel movement (-128 to 127)
} VMOUSE_HID_REPORT, *PVMOUSE_HID_REPORT;

// Extended HID Mouse Report (5-byte with horizontal scroll)
typedef struct _VMOUSE_HID_REPORT_EX {
    UCHAR Buttons;
    CHAR X;
    CHAR Y;
    CHAR Wheel;
    CHAR HWheel;            // Horizontal wheel
} VMOUSE_HID_REPORT_EX, *PVMOUSE_HID_REPORT_EX;

/*
 * vhidkb_ioctl.h - Virtual HID Keyboard IOCTL Definitions
 */

#pragma once

// Device type for IOCTL
#define FILE_DEVICE_VHIDKB 0x8000

// IOCTL codes
#define IOCTL_VKB_INJECT_KEYDOWN    CTL_CODE(FILE_DEVICE_VHIDKB, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VKB_INJECT_KEYUP      CTL_CODE(FILE_DEVICE_VHIDKB, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VKB_INJECT_COMBO      CTL_CODE(FILE_DEVICE_VHIDKB, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VKB_RESET             CTL_CODE(FILE_DEVICE_VHIDKB, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Maximum simultaneous keys (standard HID boot keyboard)
#define VKB_MAX_KEYS 6

// HID modifier keys bitmask
#define VKB_MOD_LEFT_CTRL   0x01
#define VKB_MOD_LEFT_SHIFT  0x02
#define VKB_MOD_LEFT_ALT    0x04
#define VKB_MOD_LEFT_GUI    0x08
#define VKB_MOD_RIGHT_CTRL  0x10
#define VKB_MOD_RIGHT_SHIFT 0x20
#define VKB_MOD_RIGHT_ALT   0x40
#define VKB_MOD_RIGHT_GUI   0x80

// HID input report structure (8-byte boot keyboard format)
typedef struct _VKB_INPUT_REPORT {
    UCHAR  ReportID;                    // Report ID (usually 0 for boot keyboard)
    UCHAR  ModifierKeys;                // Modifier keys bitmask
    UCHAR  Reserved;                    // Reserved byte (must be 0)
    UCHAR  KeyCodes[VKB_MAX_KEYS];      // Up to 6 simultaneous key codes
} VKB_INPUT_REPORT, *PVKB_INPUT_REPORT;

// Key combo structure for IOCTL_VKB_INJECT_COMBO
typedef struct _VKB_KEY_COMBO {
    ULONG  Count;                       // Number of key events in combo
    struct {
        UCHAR KeyCode;                  // HID key code
        UCHAR ModifierKeys;             // Modifier keys for this event
        ULONG DurationMs;               // How long to hold key (0 = instant press)
        BOOL  KeyUp;                    // TRUE for key up, FALSE for key down
    } Keys[1];                          // Variable length array
} VKB_KEY_COMBO, *PVKB_KEY_COMBO;

// Common HID key codes (subset)
#define VKB_KEY_NONE        0x00
#define VKB_KEY_A           0x04
#define VKB_KEY_B           0x05
#define VKB_KEY_C           0x06
#define VKB_KEY_D           0x07
#define VKB_KEY_E           0x08
#define VKB_KEY_F           0x09
#define VKB_KEY_G           0x0A
#define VKB_KEY_H           0x0B
#define VKB_KEY_I           0x0C
#define VKB_KEY_J           0x0D
#define VKB_KEY_K           0x0E
#define VKB_KEY_L           0x0F
#define VKB_KEY_M           0x10
#define VKB_KEY_N           0x11
#define VKB_KEY_O           0x12
#define VKB_KEY_P           0x13
#define VKB_KEY_Q           0x14
#define VKB_KEY_R           0x15
#define VKB_KEY_S           0x16
#define VKB_KEY_T           0x17
#define VKB_KEY_U           0x18
#define VKB_KEY_V           0x19
#define VKB_KEY_W           0x1A
#define VKB_KEY_X           0x1B
#define VKB_KEY_Y           0x1C
#define VKB_KEY_Z           0x1D
#define VKB_KEY_1           0x1E
#define VKB_KEY_2           0x1F
#define VKB_KEY_3           0x20
#define VKB_KEY_4           0x21
#define VKB_KEY_5           0x22
#define VKB_KEY_6           0x23
#define VKB_KEY_7           0x24
#define VKB_KEY_8           0x25
#define VKB_KEY_9           0x26
#define VKB_KEY_0           0x27
#define VKB_KEY_ENTER       0x28
#define VKB_KEY_ESC         0x29
#define VKB_KEY_BACKSPACE   0x2A
#define VKB_KEY_TAB         0x2B
#define VKB_KEY_SPACE       0x2C
#define VKB_KEY_MINUS       0x2D
#define VKB_KEY_EQUAL       0x2E
#define VKB_KEY_LEFT_BRACE  0x2F
#define VKB_KEY_RIGHT_BRACE 0x30
#define VKB_KEY_BACKSLASH   0x31
#define VKB_KEY_SEMICOLON   0x33
#define VKB_KEY_QUOTE       0x34
#define VKB_KEY_GRAVE       0x35
#define VKB_KEY_COMMA       0x36
#define VKB_KEY_PERIOD      0x37
#define VKB_KEY_SLASH       0x38
#define VKB_KEY_CAPS_LOCK   0x39
#define VKB_KEY_F1          0x3A
#define VKB_KEY_F2          0x3B
#define VKB_KEY_F3          0x3C
#define VKB_KEY_F4          0x3D
#define VKB_KEY_F5          0x3E
#define VKB_KEY_F6          0x3F
#define VKB_KEY_F7          0x40
#define VKB_KEY_F8          0x41
#define VKB_KEY_F9          0x42
#define VKB_KEY_F10         0x43
#define VKB_KEY_F11         0x44
#define VKB_KEY_F12         0x45
#define VKB_KEY_PRINT_SCREEN 0x46
#define VKB_KEY_SCROLL_LOCK 0x47
#define VKB_KEY_PAUSE       0x48
#define VKB_KEY_INSERT      0x49
#define VKB_KEY_HOME        0x4A
#define VKB_KEY_PAGE_UP     0x4B
#define VKB_KEY_DELETE      0x4C
#define VKB_KEY_END         0x4D
#define VKB_KEY_PAGE_DOWN   0x4E
#define VKB_KEY_RIGHT       0x4F
#define VKB_KEY_LEFT        0x50
#define VKB_KEY_DOWN        0x51
#define VKB_KEY_UP          0x52
#define VKB_KEY_LEFT_CTRL   0xE0
#define VKB_KEY_LEFT_SHIFT  0xE1
#define VKB_KEY_LEFT_ALT    0xE2
#define VKB_KEY_LEFT_GUI    0xE3
#define VKB_KEY_RIGHT_CTRL  0xE4
#define VKB_KEY_RIGHT_SHIFT 0xE5
#define VKB_KEY_RIGHT_ALT   0xE6
#define VKB_KEY_RIGHT_GUI   0xE7

/*
 * vhidkb.h - Virtual HID Keyboard Driver Header
 */

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>

#define INITGUID
#include <guiddef.h>

#define POOL_TAG 'kbihV'

typedef struct _DEVICE_CONTEXT {
    WDFDEVICE Device;
    WDFQUEUE DefaultQueue;
    WDFIOTARGET HidIoTarget;
    
    // Current keyboard state
    UCHAR CurrentModifierKeys;
    UCHAR CurrentKeyCodes[VKB_MAX_KEYS];
    
    // HID report descriptor
    WDFMEMORY HidReportDescriptorMemory;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, vhidkbGetDeviceContext)

// Driver callbacks
NTSTATUS vhidkbEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
);

VOID vhidkbEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
);

VOID vhidkbEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
);

VOID vhidkbEvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
);

// Key injection functions
NTSTATUS vhidkbSendHidReport(
    _In_ PDEVICE_CONTEXT DeviceContext,
    _In_ UCHAR ModifierKeys,
    _In_ UCHAR* KeyCodes,
    _In_ UCHAR KeyCount
);

NTSTATUS vhidkbInjectKeyDown(
    _In_ PDEVICE_CONTEXT DeviceContext,
    _In_ WDFREQUEST Request
);

NTSTATUS vhidkbInjectKeyUp(
    _In_ PDEVICE_CONTEXT DeviceContext,
    _In_ WDFREQUEST Request
);

NTSTATUS vhidkbInjectKeyCombo(
    _In_ PDEVICE_CONTEXT DeviceContext,
    _In_ WDFREQUEST Request
);

NTSTATUS vhidkbReset(
    _In_ PDEVICE_CONTEXT DeviceContext
);

// WPP tracing macros
#define WPP_CONTROL_GUIDS \
    WPP_DEFINE_CONTROL_GUID(vhidkbTraceGuid, (a1b2c3d4, e5f6, 7890, abcd, ef1234567890), \
        WPP_DEFINE_BIT(TRACE_DRIVER) \
        WPP_DEFINE_BIT(TRACE_DEVICE) \
        WPP_DEFINE_BIT(TRACE_QUEUE) \
    )

#define TRACE_LEVEL_FATAL       0
#define TRACE_LEVEL_ERROR       1
#define TRACE_LEVEL_WARNING     2
#define TRACE_LEVEL_INFORMATION 3
#define TRACE_LEVEL_VERBOSE     4

// Function entry/exit macros
#define TraceEvents(level, flags, msg, ...) \
    // WPP tracing placeholder

#define TRACE_DRIVER 0x00000001
#define TRACE_DEVICE 0x00000002
#define TRACE_QUEUE  0x00000004

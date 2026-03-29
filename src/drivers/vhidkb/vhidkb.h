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

// HID Device Extension for minidriver
typedef struct _HID_DEVICE_CONTEXT {
    // HID class driver context
    WDFDEVICE Device;
} HID_DEVICE_CONTEXT, *PHID_DEVICE_CONTEXT;

// Legacy device context (for filter driver mode)
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
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(HID_DEVICE_CONTEXT, vhidkbGetHidContext)

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

// HID Minidriver callbacks
NTSTATUS vhidkbHidGetDeviceAttributes(
    _In_ PHID_DEVICE_CONTEXT Context,
    _Out_ PHID_DEVICE_ATTRIBUTES Attributes
);

NTSTATUS vhidkbHidGetReportDescriptor(
    _In_ PHID_DEVICE_CONTEXT Context,
    _Out_writes_bytes_to_(DescriptorLength, *ActualDescriptorLength) UCHAR* Descriptor,
    _In_ ULONG DescriptorLength,
    _Out_ ULONG* ActualDescriptorLength
);

NTSTATUS vhidkbHidReadReport(
    _In_ PHID_DEVICE_CONTEXT Context,
    _In_ WDFREQUEST Request,
    _Out_writes_bytes_to_(ReportLength, *ActualReportLength) UCHAR* Report,
    _In_ ULONG ReportLength,
    _Out_ ULONG* ActualReportLength
);

NTSTATUS vhidkbHidWriteReport(
    _In_ PHID_DEVICE_CONTEXT Context,
    _In_ WDFREQUEST Request,
    _In_reads_bytes_(ReportLength) UCHAR* Report,
    _In_ ULONG ReportLength
);

NTSTATUS vhidkbHidQueryReportType(
    _In_ PHID_DEVICE_CONTEXT Context,
    _In_ UCHAR ReportType,
    _Out_ PHIDP_REPORT_IDS ReportIds
);

// HID Report submission
NTSTATUS vhidkbSubmitHidReport(
    _In_ WDFDEVICE Device,
    _In_ UCHAR ModifierKeys,
    _In_ UCHAR* KeyCodes,
    _In_ UCHAR KeyCount
);

// Key injection functions (legacy)
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

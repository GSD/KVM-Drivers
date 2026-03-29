#pragma once
#include <ntddk.h>
#include <wdf.h>
#define POOL_TAG 'mouV'
typedef struct _MOUSE_CONTEXT {
    WDFDEVICE Device;
} MOUSE_CONTEXT, *PMOUSE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(MOUSE_CONTEXT, vhidmouseGetContext)
NTSTATUS vhidmouseEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit);

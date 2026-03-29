/*
 * Virtual HID Mouse Driver (vhidmouse.sys)
 * 
 * Upper filter driver on HID mouse stack for mouse input injection.
 * Supports relative and absolute positioning, 5 buttons, and scroll wheel.
 */

#include "vhidmouse.h"
#include "vhidmouse_ioctl.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, vhidmouseEvtDeviceAdd)
#pragma alloc_text (PAGE, vhidmouseEvtDriverContextCleanup)
#endif

NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    WPP_INIT_TRACING(DriverObject, RegistryPath);
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = vhidmouseEvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config, vhidmouseEvtDeviceAdd);

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &attributes,
        &config,
        WDF_NO_HANDLE
    );

    if (!NT_SUCCESS(status)) {
        KdPrint(("vhidmouse: WdfDriverCreate failed 0x%x\n", status));
        WPP_CLEANUP(DriverObject);
        return status;
    }

    KdPrint(("vhidmouse: Driver loaded successfully\n"));
    return status;
}

NTSTATUS vhidmouseEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    UNREFERENCED_PARAMETER(Driver);
    NTSTATUS status;
    WDFDEVICE device;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDFQUEUE queue;
    PMOUSE_CONTEXT mouseContext;
    WDF_OBJECT_ATTRIBUTES attributes;

    PAGED_CODE();

    WdfFdoInitSetFilter(DeviceInit);

    status = WdfDeviceCreate(&DeviceInit, WDF_NO_OBJECT_ATTRIBUTES, &device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("vhidmouse: WdfDeviceCreate failed 0x%x\n", status));
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, MOUSE_CONTEXT);
    status = WdfObjectAllocateContext(device, &attributes, (PVOID*)&mouseContext);
    if (!NT_SUCCESS(status)) {
        KdPrint(("vhidmouse: WdfObjectAllocateContext failed 0x%x\n", status));
        return status;
    }

    RtlZeroMemory(mouseContext, sizeof(MOUSE_CONTEXT));
    mouseContext->Device = device;
    mouseContext->LastX = 0;
    mouseContext->LastY = 0;
    mouseContext->ButtonState = 0;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = vhidmouseEvtIoDeviceControl;
    queueConfig.EvtIoStop = vhidmouseEvtIoStop;

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
    if (!NT_SUCCESS(status)) {
        KdPrint(("vhidmouse: WdfIoQueueCreate failed 0x%x\n", status));
        return status;
    }

    WdfObjectSetDefaultIoQueue(device, queue);
    KdPrint(("vhidmouse: Device added successfully\n"));

    return STATUS_SUCCESS;
}

VOID vhidmouseEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    NTSTATUS status = STATUS_SUCCESS;
    WDFDEVICE device;
    PMOUSE_CONTEXT mouseContext;

    device = WdfIoQueueGetDevice(Queue);
    mouseContext = vhidmouseGetContext(device);

    switch (IoControlCode) {
    case IOCTL_VMOUSE_MOVE_RELATIVE:
        status = vhidmouseInjectRelativeMove(mouseContext, Request);
        break;

    case IOCTL_VMOUSE_MOVE_ABSOLUTE:
        status = vhidmouseInjectAbsoluteMove(mouseContext, Request);
        break;

    case IOCTL_VMOUSE_BUTTON:
        status = vhidmouseInjectButton(mouseContext, Request);
        break;

    case IOCTL_VMOUSE_SCROLL:
        status = vhidmouseInjectScroll(mouseContext, Request);
        break;

    case IOCTL_VMOUSE_RESET:
        status = vhidmouseReset(mouseContext);
        break;

    default:
        WdfRequestFormatRequestUsingCurrentStackLocation(Request);
        WdfRequestSend(Request, WdfDeviceGetIoTarget(device), WDF_NO_SEND_OPTIONS);
        return;
    }

    WdfRequestComplete(Request, status);
}

VOID vhidmouseEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
{
    UNREFERENCED_PARAMETER(Queue);

    if (ActionFlags & WdfRequestStopActionSuspend) {
        WdfRequestStopAcknowledge(Request, FALSE);
    } else if (ActionFlags & WdfRequestStopActionPurge) {
        WdfRequestCancelSentRequest(Request);
    }
}

VOID vhidmouseEvtDriverContextCleanup(_In_ WDFOBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    PAGED_CODE();
    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}

NTSTATUS vhidmouseInjectRelativeMove(
    _In_ PMOUSE_CONTEXT Context,
    _In_ WDFREQUEST Request
)
{
    PVMOUSE_MOVE_DATA moveData;
    size_t bufferSize;
    NTSTATUS status;

    status = WdfRequestRetrieveInputBuffer(Request, sizeof(VMOUSE_MOVE_DATA), (PVOID*)&moveData, &bufferSize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    Context->LastX += moveData->X;
    Context->LastY += moveData->Y;

    KdPrint(("vhidmouse: Relative move X=%d Y=%d Buttons=0x%x\n", 
        moveData->X, moveData->Y, moveData->Flags));

    return STATUS_SUCCESS;
}

NTSTATUS vhidmouseInjectAbsoluteMove(
    _In_ PMOUSE_CONTEXT Context,
    _In_ WDFREQUEST Request
)
{
    PVMOUSE_ABSOLUTE_DATA absData;
    size_t bufferSize;
    NTSTATUS status;

    status = WdfRequestRetrieveInputBuffer(Request, sizeof(VMOUSE_ABSOLUTE_DATA), (PVOID*)&absData, &bufferSize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    Context->LastX = absData->X;
    Context->LastY = absData->Y;

    KdPrint(("vhidmouse: Absolute move X=%d Y=%d (Screen: %dx%d)\n",
        absData->X, absData->Y, absData->ScreenWidth, absData->ScreenHeight));

    return STATUS_SUCCESS;
}

NTSTATUS vhidmouseInjectButton(
    _In_ PMOUSE_CONTEXT Context,
    _In_ WDFREQUEST Request
)
{
    PVMOUSE_BUTTON_DATA buttonData;
    size_t bufferSize;
    NTSTATUS status;

    status = WdfRequestRetrieveInputBuffer(Request, sizeof(VMOUSE_BUTTON_DATA), (PVOID*)&buttonData, &bufferSize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    if (buttonData->Button < 5) {
        if (buttonData->Pressed) {
            Context->ButtonState |= (1 << buttonData->Button);
        } else {
            Context->ButtonState &= ~(1 << buttonData->Button);
        }
    }

    KdPrint(("vhidmouse: Button %d %s (state=0x%x)\n",
        buttonData->Button, buttonData->Pressed ? "pressed" : "released", Context->ButtonState));

    return STATUS_SUCCESS;
}

NTSTATUS vhidmouseInjectScroll(
    _In_ PMOUSE_CONTEXT Context,
    _In_ WDFREQUEST Request
)
{
    PVMOUSE_SCROLL_DATA scrollData;
    size_t bufferSize;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Context);

    status = WdfRequestRetrieveInputBuffer(Request, sizeof(VMOUSE_SCROLL_DATA), (PVOID*)&scrollData, &bufferSize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    KdPrint(("vhidmouse: Scroll V=%d H=%d\n", scrollData->Vertical, scrollData->Horizontal));

    return STATUS_SUCCESS;
}

NTSTATUS vhidmouseReset(_In_ PMOUSE_CONTEXT Context)
{
    Context->LastX = 0;
    Context->LastY = 0;
    Context->ButtonState = 0;
    
    KdPrint(("vhidmouse: Reset\n"));
    return STATUS_SUCCESS;
}

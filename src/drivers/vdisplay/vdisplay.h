#pragma once

#include <windows.h>
#include <iddcx.h>

// IDD Plugin class for virtual display
typedef struct _VDISPLAY_CONTEXT {
    IDDCX_ADAPTER Adapter;
    IDDCX_MONITOR Monitor;
    HANDLE FrameCaptureEvent;
    UINT Width;
    UINT Height;
    UINT RefreshRate;
} VDISPLAY_CONTEXT, *PVDISPLAY_CONTEXT;

// IDD Callback functions
class IddSamplePlugin {
public:
    static HRESULT WINAPI InitAdapter(
        IDDCX_ADAPTER AdapterObject,
        const IDD_CX_ADAPTER_INIT_PARAMS* pInArgs,
        IDD_CX_ADAPTER_INIT_PARAMS* pOutArgs
    );
    
    static HRESULT WINAPI ParseMonitorDescription(
        const IDD_CX_MONITOR_DESCRIPTION* pInDescription,
        IDD_CX_MONITOR_DESCRIPTION* pOutDescription
    );
    
    static HRESULT WINAPI FinishFrameProcessing(
        IDD_CX_FRAME_PROCESSING_CONTEXT Context,
        const IDD_CX_FINISH_FRAME_PROCESSING_PARAMS* pParams
    );
};

// IOCTL interface for user-mode
typedef struct _VDISP_FRAMEBUFFER_INFO {
    UINT Width;
    UINT Height;
    UINT Stride;
    DXGI_FORMAT Format;
    HANDLE SharedTextureHandle;
} VDISP_FRAMEBUFFER_INFO, *PVDISP_FRAMEBUFFER_INFO;

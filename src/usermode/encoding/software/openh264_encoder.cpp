// openh264_encoder.cpp - Software H.264 encoder via OpenH264 (Cisco)
// Runtime-loads openh264.dll; falls back gracefully if DLL is absent.
// Download openh264.dll from: https://github.com/cisco/openh264/releases
// Place openh264.dll alongside KVMService.exe.

#include <windows.h>
#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>

// ── Minimal OpenH264 interface declarations (subset of wels/codec_api.h) ──────

typedef enum { SCREEN_CONTENT_REAL_TIME = 2 } EUsageType;
typedef enum { RC_QUALITY_MODE = 0, RC_BITRATE_MODE = 1 } RC_MODES;
typedef enum {
    videoFormatI420 = 2,
} EVideoFormatType;

typedef enum {
    videoFrameTypeInvalid = 0,
    videoFrameTypeIDR,
    videoFrameTypeI,
    videoFrameTypeP,
    videoFrameTypeSkip,
    videoFrameTypeIPMixed
} EVideoFrameType;

typedef struct TagEncParamBase {
    EUsageType iUsageType;
    int        iPicWidth;
    int        iPicHeight;
    int        iTargetBitrate;
    RC_MODES   iRCMode;
    float      fMaxFrameRate;
} SEncParamBase;

typedef struct TagSourcePicture {
    int            iColorFormat;
    int            iStride[4];
    unsigned char* pData[4];
    int            iPicWidth;
    int            iPicHeight;
    long long      uiTimeStamp;
} SSourcePicture;

#define MAX_LAYER_NUM_OF_FRAME 128
#define MAX_NAL_UNITS_IN_LAYER 128

typedef struct TagLayerBsInfo {
    unsigned char  uiTemporalId;
    unsigned char  uiSpatialId;
    unsigned char  uiQualityId;
    EVideoFrameType eFrameType;
    unsigned char  uiLayerType;
    int            iSubSeqId;
    int            iNalCount;
    int*           pNalLengthInByte;
    unsigned char* pBsBuf;
} SLayerBSInfo;

typedef struct TagFrameBSInfo {
    int          iLayerNum;
    SLayerBSInfo sLayerInfo[MAX_LAYER_NUM_OF_FRAME];
    EVideoFrameType eFrameType;
    int          iFrameSizeInBytes;
    long long    uiTimeStamp;
} SFrameBSInfo;

// Vtable offsets in ISVCEncoder (C++ pure-virtual interface)
typedef struct ISVCEncoderVtbl {
    void* __padding[0];  // no base class padding on MSVC
    int (WINAPI* Initialize)(void* self, const SEncParamBase* pParam);
    int (WINAPI* InitializeExt)(void* self, const void* pParam);
    int (WINAPI* GetDefaultParams)(void* self, void* pParam);
    int (WINAPI* Uninitialize)(void* self);
    int (WINAPI* EncodeFrame)(void* self, const SSourcePicture* kpSrcPic, SFrameBSInfo* pBsInfo);
    int (WINAPI* EncodeParameterSets)(void* self, SFrameBSInfo* pBsInfo);
    int (WINAPI* ForceIntraFrame)(void* self, bool bIDR);
    int (WINAPI* SetOption)(void* self, int eOptionId, void* pOption);
    int (WINAPI* GetOption)(void* self, int eOptionId, void* pOption);
} ISVCEncoderVtbl;

typedef struct ISVCEncoder {
    const ISVCEncoderVtbl* lpVtbl;
} ISVCEncoder;

typedef int (*WelsCreateSVCEncoder_fn)(ISVCEncoder** ppEncoder);
typedef void (*WelsDestroySVCEncoder_fn)(ISVCEncoder* pEncoder);

// ── Encoder context ─────────────────────────────────────────────────────────

class OpenH264Encoder {
public:
    OpenH264Encoder()
        : hDll_(nullptr), encoder_(nullptr)
        , pfnCreate_(nullptr), pfnDestroy_(nullptr)
        , width_(0), height_(0), fps_(0), initialized_(false)
    {}

    ~OpenH264Encoder() { Shutdown(); }

    bool Initialize(int width, int height, int fps) {
        width_  = width;
        height_ = height;
        fps_    = fps;

        // Try loading the DLL (user must place openh264.dll next to the exe)
        hDll_ = LoadLibraryA("openh264.dll");
        if (!hDll_) {
            std::cerr << "[OpenH264] openh264.dll not found — software encoding unavailable" << std::endl;
            return false;
        }

        pfnCreate_  = (WelsCreateSVCEncoder_fn) GetProcAddress(hDll_, "WelsCreateSVCEncoder");
        pfnDestroy_ = (WelsDestroySVCEncoder_fn)GetProcAddress(hDll_, "WelsDestroySVCEncoder");
        if (!pfnCreate_ || !pfnDestroy_) {
            std::cerr << "[OpenH264] WelsCreateSVCEncoder not exported" << std::endl;
            FreeLibrary(hDll_); hDll_ = nullptr;
            return false;
        }

        if (pfnCreate_(&encoder_) != 0 || !encoder_) {
            std::cerr << "[OpenH264] WelsCreateSVCEncoder failed" << std::endl;
            FreeLibrary(hDll_); hDll_ = nullptr;
            return false;
        }

        SEncParamBase params = {};
        params.iUsageType    = SCREEN_CONTENT_REAL_TIME;
        params.iPicWidth     = width;
        params.iPicHeight    = height;
        params.iTargetBitrate = 4000000;  // 4 Mbps default
        params.iRCMode       = RC_QUALITY_MODE;
        params.fMaxFrameRate = static_cast<float>(fps);

        if (encoder_->lpVtbl->Initialize(encoder_, &params) != 0) {
            std::cerr << "[OpenH264] Initialize failed" << std::endl;
            pfnDestroy_(encoder_); encoder_ = nullptr;
            FreeLibrary(hDll_); hDll_ = nullptr;
            return false;
        }

        // Pre-allocate NAL output buffer
        nalBuf_.resize((size_t)width * height * 2);

        std::cout << "[OpenH264] Software H.264 encoder ready: "
                  << width << "x" << height << " @" << fps << "fps" << std::endl;
        initialized_ = true;
        return true;
    }

    // Encode one frame.  nv12Data: width*height*3/2 bytes in NV12 layout.
    // output / outputSize: caller receives pointer into internal buffer (valid
    // until the next Encode call).
    bool EncodeFrame(const void* nv12Data, void** output, size_t* outputSize) {
        if (!initialized_ || !encoder_) return false;

        // Convert NV12 → I420 (planar YUV420)
        // NV12: Y plane (W×H) + interleaved UV (W×H/2)
        // I420: Y plane (W×H) + U plane (W/2×H/2) + V plane (W/2×H/2)
        size_t ySize  = (size_t)width_ * height_;
        size_t uvSize = ySize / 4;

        i420Buf_.resize(ySize + uvSize * 2);
        const uint8_t* src   = static_cast<const uint8_t*>(nv12Data);
        uint8_t*       yDst  = i420Buf_.data();
        uint8_t*       uDst  = yDst + ySize;
        uint8_t*       vDst  = uDst + uvSize;

        // Y plane: copy directly
        memcpy(yDst, src, ySize);

        // Deinterleave UV: NV12 has UVUV... → I420 UUUU... VVVV...
        const uint8_t* uvSrc = src + ySize;
        for (size_t i = 0; i < uvSize; i++) {
            uDst[i] = uvSrc[i * 2];
            vDst[i] = uvSrc[i * 2 + 1];
        }

        SSourcePicture pic = {};
        pic.iColorFormat  = videoFormatI420;
        pic.iPicWidth     = width_;
        pic.iPicHeight    = height_;
        pic.iStride[0]    = width_;
        pic.iStride[1]    = width_ / 2;
        pic.iStride[2]    = width_ / 2;
        pic.pData[0]      = yDst;
        pic.pData[1]      = uDst;
        pic.pData[2]      = vDst;
        pic.uiTimeStamp   = frameCount_++ * (1000 / fps_);

        SFrameBSInfo bsInfo = {};
        if (encoder_->lpVtbl->EncodeFrame(encoder_, &pic, &bsInfo) != 0) {
            return false;
        }
        if (bsInfo.eFrameType == videoFrameTypeSkip || bsInfo.iFrameSizeInBytes <= 0) {
            *output     = nullptr;
            *outputSize = 0;
            return true;  // skipped frame — not an error
        }

        // Concatenate all NAL units into nalBuf_
        if ((size_t)bsInfo.iFrameSizeInBytes > nalBuf_.size())
            nalBuf_.resize(bsInfo.iFrameSizeInBytes);

        size_t offset = 0;
        for (int layer = 0; layer < bsInfo.iLayerNum && layer < MAX_LAYER_NUM_OF_FRAME; layer++) {
            const SLayerBSInfo& li = bsInfo.sLayerInfo[layer];
            for (int nal = 0; nal < li.iNalCount && nal < MAX_NAL_UNITS_IN_LAYER; nal++) {
                int nalLen = li.pNalLengthInByte[nal];
                if (nalLen > 0 && offset + nalLen <= nalBuf_.size()) {
                    memcpy(nalBuf_.data() + offset, li.pBsBuf, nalLen);
                    offset += nalLen;
                }
            }
        }

        *output     = nalBuf_.data();
        *outputSize = offset;
        return (offset > 0);
    }

    void Shutdown() {
        if (encoder_ && pfnDestroy_) {
            encoder_->lpVtbl->Uninitialize(encoder_);
            pfnDestroy_(encoder_);
            encoder_ = nullptr;
        }
        if (hDll_) {
            FreeLibrary(hDll_);
            hDll_ = nullptr;
        }
        initialized_ = false;
    }

private:
    HMODULE   hDll_;
    ISVCEncoder* encoder_;
    WelsCreateSVCEncoder_fn  pfnCreate_;
    WelsDestroySVCEncoder_fn pfnDestroy_;

    int  width_, height_, fps_;
    bool initialized_;
    long long frameCount_ = 0;

    std::vector<uint8_t> i420Buf_;
    std::vector<uint8_t> nalBuf_;
};

// ── C interface ───────────────────────────────────────────────────────────────

extern "C" {
    __declspec(dllexport) void* Software_Create() {
        return new OpenH264Encoder();
    }
    __declspec(dllexport) bool Software_Init(void* enc, int w, int h, int fps) {
        return static_cast<OpenH264Encoder*>(enc)->Initialize(w, h, fps);
    }
    __declspec(dllexport) bool Software_Encode(void* enc, void* frame,
                                               void** out, size_t* size) {
        return static_cast<OpenH264Encoder*>(enc)->EncodeFrame(frame, out, size);
    }
    __declspec(dllexport) void Software_Destroy(void* enc) {
        auto* e = static_cast<OpenH264Encoder*>(enc);
        e->Shutdown();
        delete e;
    }
}

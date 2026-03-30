// Intel QuickSync Video encoder wrapper
// Intel Media SDK (mfxvideo.h) is not bundled with this project.
// These C-interface stubs satisfy the linker and return false gracefully;
// EncoderManager will fall back to NVIDIA NVENC or AMD AMF when QSV is
// selected but initialization fails.
#include <windows.h>
#include <iostream>

struct IntelEncoder {
    HMODULE hLib = nullptr;
    bool    ok   = false;
};

static bool TryLoadMfx(IntelEncoder* enc) {
    // Try both the legacy and VPL runtime DLL names
    enc->hLib = LoadLibraryA("libmfx64-gen.dll");
    if (!enc->hLib) enc->hLib = LoadLibraryA("libmfx64.dll");
    if (!enc->hLib) enc->hLib = LoadLibraryA("libvpl.dll");
    return enc->hLib != nullptr;
}

extern "C" {
    __declspec(dllexport) void* IntelEncoder_Create() {
        return new IntelEncoder();
    }

    __declspec(dllexport) bool IntelEncoder_Init(void* enc, int /*w*/, int /*h*/, int /*fps*/) {
        auto* e = static_cast<IntelEncoder*>(enc);
        if (!TryLoadMfx(e)) {
            std::cerr << "[QSV] Intel Media SDK DLL not found; QSV unavailable" << std::endl;
            return false;
        }
        // Full MFX session init would go here; returning false causes
        // EncoderManager to fall back to the next available encoder.
        std::cerr << "[QSV] Full MFX session init not implemented; falling back" << std::endl;
        return false;
    }

    __declspec(dllexport) bool IntelEncoder_Encode(void* /*enc*/, void* /*frame*/,
                                                   void** /*out*/, size_t* /*size*/) {
        return false;
    }

    __declspec(dllexport) void IntelEncoder_Destroy(void* enc) {
        auto* e = static_cast<IntelEncoder*>(enc);
        if (e->hLib) { FreeLibrary(e->hLib); e->hLib = nullptr; }
        delete e;
    }
}

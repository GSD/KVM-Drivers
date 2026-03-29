// NVIDIA NVENC encoder wrapper
#include <nvEncodeAPI.h>

class NvidiaEncoder {
public:
    bool Initialize(int width, int height);
    bool EncodeFrame(void* frameData, void** encodedData, size_t* encodedSize);
    void Shutdown();
private:
    void* encoder;
    NV_ENCODE_API_FUNCTION_LIST nvencFuncs;
};

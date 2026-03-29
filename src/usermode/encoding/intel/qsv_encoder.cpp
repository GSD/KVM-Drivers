// Intel QuickSync Video encoder wrapper
#include <mfxvideo.h>

class IntelEncoder {
public:
    bool Initialize(int width, int height);
    bool EncodeFrame(void* frameData, void** encodedData, size_t* encodedSize);
    void Shutdown();
private:
    mfxSession session;
    mfxVideoParam params;
};

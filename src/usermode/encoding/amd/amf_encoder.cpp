// AMD AMF encoder wrapper
#include <AMF/core/Factory.h>

class AmdEncoder {
public:
    bool Initialize(int width, int height);
    bool EncodeFrame(void* frameData, void** encodedData, size_t* encodedSize);
    void Shutdown();
private:
    amf::AMFComponent* encoder;
    amf::AMFContext* context;
};

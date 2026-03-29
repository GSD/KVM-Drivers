// VNC Server - RFB protocol handler
#include <windows.h>

class VncServer {
public:
    bool Initialize(int port);
    void Run();
    void Shutdown();
    void HandleClient(SOCKET client);
private:
    SOCKET listenSocket;
    int port;
};

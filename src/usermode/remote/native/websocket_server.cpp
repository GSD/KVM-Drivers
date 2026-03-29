// WebSocket Server - Native protocol handler
#include <winsock2.h>
#include <windows.h>

class WebSocketServer {
public:
    bool Initialize(int port);
    void Run();
    void Shutdown();
private:
    SOCKET listenSocket;
};

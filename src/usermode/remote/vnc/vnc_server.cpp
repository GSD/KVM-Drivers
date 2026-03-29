// vnc_server.cpp - RFB 3.8 Protocol Implementation
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

class VncServer {
public:
    VncServer(int port = 5900) : port(port), running(false), listenSocket(INVALID_SOCKET) {}

    bool Start() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            return false;
        }

        listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET) {
            std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
            WSACleanup();
            return false;
        }

        sockaddr_in service;
        service.sin_family = AF_INET;
        service.sin_addr.s_addr = INADDR_ANY;
        service.sin_port = htons(port);

        if (bind(listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
            std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
            closesocket(listenSocket);
            WSACleanup();
            return false;
        }

        if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
            closesocket(listenSocket);
            WSACleanup();
            return false;
        }

        running = true;
        std::cout << "VNC server listening on port " << port << std::endl;

        acceptThread = std::thread(&VncServer::AcceptConnections, this);
        return true;
    }

    void Stop() {
        running = false;
        if (listenSocket != INVALID_SOCKET) {
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
        }
        if (acceptThread.joinable()) {
            acceptThread.join();
        }
        WSACleanup();
    }

private:
    void AcceptConnections() {
        while (running) {
            sockaddr_in clientAddr;
            int clientAddrSize = sizeof(clientAddr);
            SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrSize);

            if (clientSocket == INVALID_SOCKET) {
                if (running) {
                    std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
                }
                continue;
            }

            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            std::cout << "VNC client connected from " << clientIP << std::endl;

            std::thread clientThread(&VncServer::HandleClient, this, clientSocket);
            clientThread.detach();
        }
    }

    void HandleClient(SOCKET clientSocket) {
        // RFB 3.8 Protocol Handshake
        const char* protocolVersion = "RFB 003.008\n";
        send(clientSocket, protocolVersion, 13, 0);

        char clientVersion[13];
        recv(clientSocket, clientVersion, 13, 0);

        char securityTypes[] = { 1, 1 }; // 1 security type, type 1 = None
        send(clientSocket, securityTypes, 2, 0);

        char selectedSecurity;
        recv(clientSocket, &selectedSecurity, 1, 0);

        char securityResult[4] = { 0, 0, 0, 0 };
        send(clientSocket, securityResult, 4, 0);

        char sharedFlag;
        recv(clientSocket, &sharedFlag, 1, 0);

        // Send server init
        struct ServerInit {
            USHORT width, height;
            UCHAR bitsPerPixel, depth, bigEndian, trueColour;
            USHORT redMax, greenMax, blueMax;
            UCHAR redShift, greenShift, blueShift, padding[3];
            ULONG nameLength;
        } init;

        init.width = htons(1920);
        init.height = htons(1080);
        init.bitsPerPixel = 32;
        init.depth = 24;
        init.bigEndian = 0;
        init.trueColour = 1;
        init.redMax = htons(255);
        init.greenMax = htons(255);
        init.blueMax = htons(255);
        init.redShift = 16;
        init.greenShift = 8;
        init.blueShift = 0;
        memset(init.padding, 0, 3);
        const char* desktopName = "KVM Virtual Desktop";
        init.nameLength = htonl(strlen(desktopName));

        send(clientSocket, (char*)&init, sizeof(init), 0);
        send(clientSocket, desktopName, strlen(desktopName), 0);

        std::cout << "VNC handshake complete" << std::endl;
        HandleProtocol(clientSocket);
        closesocket(clientSocket);
    }

    void HandleProtocol(SOCKET clientSocket) {
        char messageType;
        while (recv(clientSocket, &messageType, 1, 0) > 0) {
            switch (messageType) {
            case 0: // SetPixelFormat
                char pixelFormat[16];
                recv(clientSocket, pixelFormat, 16, 0);
                break;

            case 2: // SetEncodings
                char padding;
                USHORT encodingCount;
                recv(clientSocket, &padding, 1, 0);
                recv(clientSocket, (char*)&encodingCount, 2, 0);
                encodingCount = ntohs(encodingCount);
                for (int i = 0; i < encodingCount; i++) {
                    INT32 encoding;
                    recv(clientSocket, (char*)&encoding, 4, 0);
                }
                break;

            case 3: { // FramebufferUpdateRequest
                char incremental, buf[8];
                recv(clientSocket, &incremental, 1, 0);
                recv(clientSocket, buf, 8, 0);
                // TODO: Send framebuffer update
                break;
            }

            case 4: { // KeyEvent
                char downFlag, pad[2];
                UINT32 key;
                recv(clientSocket, &downFlag, 1, 0);
                recv(clientSocket, pad, 2, 0);
                recv(clientSocket, (char*)&key, 4, 0);
                // TODO: Translate X11 keysym and inject
                break;
            }

            case 5: { // PointerEvent
                char buttonMask, pad[2];
                USHORT x, y;
                recv(clientSocket, &buttonMask, 1, 0);
                recv(clientSocket, (char*)&x, 2, 0);
                recv(clientSocket, (char*)&y, 2, 0);
                // TODO: Inject mouse event
                break;
            }

            case 6: { // ClientCutText
                char pad[3];
                UINT32 length;
                recv(clientSocket, pad, 3, 0);
                recv(clientSocket, (char*)&length, 4, 0);
                length = ntohl(length);
                std::vector<char> text(length);
                recv(clientSocket, text.data(), length, 0);
                break;
            }
            }
        }
    }

    int port;
    bool running;
    SOCKET listenSocket;
    std::thread acceptThread;
};

// C interface
extern "C" {
    __declspec(dllexport) void* VncServer_Create(int port) { return new VncServer(port); }
    __declspec(dllexport) bool VncServer_Start(void* server) { return ((VncServer*)server)->Start(); }
    __declspec(dllexport) void VncServer_Stop(void* server) { ((VncServer*)server)->Stop(); }
    __declspec(dllexport) void VncServer_Destroy(void* server) { delete (VncServer*)server; }
}

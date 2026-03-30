// vnc_server.cpp - VNC Server implementation (RFB 3.8 protocol)
// Supports RealVNC, TightVNC, TigerVNC, UltraVNC clients

#include "vnc_server.h"
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>
#include <zlib.h>

#pragma comment(lib, "ws2_32.lib")

namespace KVMDrivers {
namespace Remote {

// RFB Protocol constants
namespace RFB {
    constexpr int ProtocolVersionMajor = 3;
    constexpr int ProtocolVersionMinor = 8;
    
    enum SecurityType {
        SecTypeInvalid = 0,
        SecTypeNone = 1,
        SecTypeVNCAuth = 2,
    };
    
    enum ClientMsgType {
        ClientSetPixelFormat = 0,
        ClientSetEncodings = 2,
        ClientFramebufferUpdateRequest = 3,
        ClientKeyEvent = 4,
        ClientPointerEvent = 5,
        ClientCutText = 6,
    };
    
    enum ServerMsgType {
        ServerFramebufferUpdate = 0,
    };
    
    enum Encoding {
        EncodingRaw = 0,
        EncodingCopyRect = 1,
        EncodingRRE = 2,
        EncodingHextile = 5,
        EncodingZlib = 6,
        EncodingTight = 7,
        EncodingZRLE = 16,
    };
}

// Safe recv wrapper - returns false on timeout/disconnect
static bool RecvAll(SOCKET s, char* buf, int len) {
    int received = 0;
    while (received < len) {
        int ret = recv(s, buf + received, len - received, 0);
        if (ret <= 0) return false;
        received += ret;
    }
    return true;
}

class VncServerImpl {
public:
    VncServerImpl(int port = 5900) 
        : port_(port), running_(false), listenSocket_(INVALID_SOCKET)
        , framebufferWidth_(1920), framebufferHeight_(1080) {
        // Pre-allocate framebuffer to avoid per-request heap allocation
        framebuffer_.resize((size_t)framebufferWidth_ * framebufferHeight_ * 4, 0);
    }

    bool Start() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return false;
        }

        listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket_ == INVALID_SOCKET) {
            WSACleanup();
            return false;
        }

        sockaddr_in service = {};
        service.sin_family = AF_INET;
        service.sin_addr.s_addr = INADDR_ANY;
        service.sin_port = htons(port_);

        if (bind(listenSocket_, (sockaddr*)&service, sizeof(service)) == SOCKET_ERROR) {
            closesocket(listenSocket_);
            WSACleanup();
            return false;
        }

        if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(listenSocket_);
            WSACleanup();
            return false;
        }

        running_ = true;
        acceptThread_ = std::thread(&VncServerImpl::AcceptLoop, this);
        return true;
    }

    void Stop() {
        running_ = false;
        
        // Close listen socket to unblock accept
        if (listenSocket_ != INVALID_SOCKET) {
            closesocket(listenSocket_);
            listenSocket_ = INVALID_SOCKET;
        }
        
        // Wait for accept thread
        if (acceptThread_.joinable()) {
            acceptThread_.join();
        }
        
        // Wait for all client threads to finish
        {
            std::lock_guard<std::mutex> lock(threadsMutex_);
            for (auto& t : clientThreads_) {
                if (t.joinable()) {
                    t.join();
                }
            }
            clientThreads_.clear();
        }
        
        WSACleanup();
    }

private:
    int port_;
    bool running_;
    SOCKET listenSocket_;
    std::thread acceptThread_;

    std::vector<std::thread> clientThreads_;
    std::mutex threadsMutex_;
    std::atomic<int> connectionCount_{0};
    static constexpr int MAX_CONNECTIONS = 10;
    static constexpr int SOCKET_TIMEOUT_MS = 30000;  // 30 seconds

    int framebufferWidth_;
    int framebufferHeight_;
    std::vector<char> framebuffer_;  // Pre-allocated, avoids per-frame heap alloc
    std::mutex framebufferMutex_;

    // Track active encodings per client (not needed globally, but useful for negotiation logging)
    std::vector<INT32> negotiatedEncodings_;

    void AcceptLoop() {
        while (running_) {
            // Use select() instead of blocking accept
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(listenSocket_, &readSet);
            timeval tv = {0, 100000}; // 100ms timeout
            
            if (select(0, &readSet, NULL, NULL, &tv) <= 0) {
                continue;  // Timeout or error, check running_ flag
            }
            
            sockaddr_in clientAddr;
            int addrLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(listenSocket_, (sockaddr*)&clientAddr, &addrLen);

            if (clientSocket == INVALID_SOCKET) {
                continue;
            }
            
            // Check connection limit
            if (connectionCount_ >= MAX_CONNECTIONS) {
                closesocket(clientSocket);
                continue;
            }
            
            // Set socket timeouts to prevent blocking
            DWORD timeout = SOCKET_TIMEOUT_MS;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
            setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
            
            connectionCount_++;
            
            // Track threads instead of detaching
            std::lock_guard<std::mutex> lock(threadsMutex_);
            clientThreads_.emplace_back(&VncServerImpl::HandleClient, this, clientSocket);
        }
    }

    void HandleClient(SOCKET clientSocket) {
        // Log connection
        sockaddr_in clientAddr = {};
        int addrLen = sizeof(clientAddr);
        getpeername(clientSocket, (sockaddr*)&clientAddr, &addrLen);
        char clientIP[INET_ADDRSTRLEN] = "unknown";
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
        int clientPort = ntohs(clientAddr.sin_port);
        std::cout << "[VNC] Client connected: " << clientIP << ":" << clientPort << std::endl;

        // RFB 3.8 Handshake
        const char version[] = "RFB 003.008\n";
        if (send(clientSocket, version, 12, 0) != 12) {
            std::cerr << "[VNC] Failed to send version to " << clientIP << std::endl;
            goto cleanup;
        }

        {
            char clientVersion[13] = {};
            if (!RecvAll(clientSocket, clientVersion, 12)) {
                std::cerr << "[VNC] Client " << clientIP << " disconnected during version exchange" << std::endl;
                goto cleanup;
            }
            clientVersion[12] = '\0';
            std::cout << "[VNC] Client version: " << clientVersion;
        }

        {
            // Security (None)
            char secTypes[] = { 1, 1 };  // 1 type, type 1 = None
            send(clientSocket, secTypes, 2, 0);

            char clientChoice;
            if (!RecvAll(clientSocket, &clientChoice, 1)) goto cleanup;
            std::cout << "[VNC] Client chose security type: " << (int)clientChoice << std::endl;

            // Security result (OK)
            UINT32 secStatus = 0;
            send(clientSocket, (char*)&secStatus, 4, 0);

            // Client init
            char shared;
            if (!RecvAll(clientSocket, &shared, 1)) goto cleanup;
            std::cout << "[VNC] Client init, shared=" << (int)shared << std::endl;

            // Server init
            UINT16 width = htons((u_short)framebufferWidth_);
            UINT16 height = htons((u_short)framebufferHeight_);
            send(clientSocket, (char*)&width, 2, 0);
            send(clientSocket, (char*)&height, 2, 0);

            // Pixel format (32-bit BGRX)
            char pixelFormat[16] = { 32, 24, 0, 1, 0, 0, 255, 255, 255, 16, 8, 0, 0, 0, 0 };
            send(clientSocket, pixelFormat, 16, 0);

            // Desktop name
            const char name[] = "KVM-Drivers VNC";
            UINT32 nameLen = htonl((u_long)strlen(name));
            send(clientSocket, (char*)&nameLen, 4, 0);
            send(clientSocket, name, (int)strlen(name), 0);
            std::cout << "[VNC] Handshake complete with " << clientIP << std::endl;

            // Main message loop
            while (running_) {
                char msgType;
                if (!RecvAll(clientSocket, &msgType, 1)) break;

                switch ((RFB::ClientMsgType)msgType) {
                case RFB::ClientSetPixelFormat:
                    HandleSetPixelFormat(clientSocket);
                    break;
                case RFB::ClientSetEncodings:
                    HandleSetEncodings(clientSocket);
                    break;
                case RFB::ClientFramebufferUpdateRequest:
                    HandleUpdateRequest(clientSocket);
                    break;
                case RFB::ClientKeyEvent:
                    HandleKeyEvent(clientSocket);
                    break;
                case RFB::ClientPointerEvent:
                    HandlePointerEvent(clientSocket);
                    break;
                default:
                    std::cerr << "[VNC] Unknown message type: " << (int)msgType
                              << " from " << clientIP << " - closing" << std::endl;
                    goto cleanup_inner;
                }
            }
        }

        cleanup_inner:;
        cleanup:
        std::cout << "[VNC] Client disconnected: " << clientIP << ":" << clientPort << std::endl;
        connectionCount_--;
        closesocket(clientSocket);
    }

    void HandleSetPixelFormat(SOCKET sock) {
        char buf[19];  // 3 padding + 16 pixel format bytes
        if (!RecvAll(sock, buf, 19)) return;
        std::cout << "[VNC] SetPixelFormat received" << std::endl;
    }

    void HandleSetEncodings(SOCKET sock) {
        char padding;
        if (!RecvAll(sock, &padding, 1)) return;

        UINT16 numEncodingsNet;
        if (!RecvAll(sock, (char*)&numEncodingsNet, 2)) return;
        UINT16 numEncodings = ntohs(numEncodingsNet);

        // CRITICAL: consume ALL encoding entries to keep protocol in sync
        // Previously only consumed min(numEncodings, 10), causing protocol desync
        std::vector<INT32> encodings;
        encodings.reserve(numEncodings);
        for (int i = 0; i < (int)numEncodings; i++) {
            INT32 enc;
            if (!RecvAll(sock, (char*)&enc, 4)) return;
            encodings.push_back(ntohl(enc));
        }

        // Log negotiated encodings
        std::cout << "[VNC] Client offered " << numEncodings << " encodings:";
        for (auto e : encodings) {
            std::cout << " " << e;
        }
        std::cout << std::endl;

        negotiatedEncodings_ = encodings;
    }

    void HandleUpdateRequest(SOCKET sock) {
        char incremental;
        UINT16 x, y, w, h;
        if (!RecvAll(sock, &incremental, 1)) return;
        if (!RecvAll(sock, (char*)&x, 2)) return;
        if (!RecvAll(sock, (char*)&y, 2)) return;
        if (!RecvAll(sock, (char*)&w, 2)) return;
        if (!RecvAll(sock, (char*)&h, 2)) return;

        x = ntohs(x); y = ntohs(y);
        w = ntohs(w); h = ntohs(h);

        // Clamp to framebuffer bounds to prevent overflow
        if (x >= (UINT16)framebufferWidth_) x = (UINT16)(framebufferWidth_ - 1);
        if (y >= (UINT16)framebufferHeight_) y = (UINT16)(framebufferHeight_ - 1);
        if ((int)(x + w) > framebufferWidth_) w = (UINT16)(framebufferWidth_ - x);
        if ((int)(y + h) > framebufferHeight_) h = (UINT16)(framebufferHeight_ - y);

        size_t dataSize = (size_t)w * h * 4;
        if (dataSize == 0) return;

        auto frameStart = std::chrono::high_resolution_clock::now();

        // Send framebuffer update header
        char header[4] = { 0, 0, 0, 1 };  // type=0, padding=0, nRects=1
        send(sock, header, 4, 0);

        // Rectangle header
        UINT16 rectX = htons(x);
        UINT16 rectY = htons(y);
        UINT16 rectW = htons(w);
        UINT16 rectH = htons(h);
        INT32 encoding = htonl(RFB::EncodingRaw);

        send(sock, (char*)&rectX, 2, 0);
        send(sock, (char*)&rectY, 2, 0);
        send(sock, (char*)&rectW, 2, 0);
        send(sock, (char*)&rectH, 2, 0);
        send(sock, (char*)&encoding, 4, 0);

        // Use pre-allocated framebuffer - no per-frame heap allocation
        {
            std::lock_guard<std::mutex> lock(framebufferMutex_);
            // Copy requested region from pre-allocated buffer
            // (In production: copy from actual captured screen data)
            size_t srcOffset = ((size_t)y * framebufferWidth_ + x) * 4;
            if (srcOffset + dataSize <= framebuffer_.size()) {
                send(sock, framebuffer_.data() + srcOffset, (int)dataSize, 0);
            } else {
                // Fallback: send black pixels if out of bounds
                std::vector<char> black(dataSize, 0);
                send(sock, black.data(), (int)dataSize, 0);
            }
        }

        // Log slow frame sends
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - frameStart).count();
        if (elapsed > 100) {
            std::cerr << "[VNC] Slow frame send: " << elapsed << "ms for "
                      << w << "x" << h << " rect" << std::endl;
        }
    }

    void HandleKeyEvent(SOCKET sock) {
        char down;
        char padding[2];
        UINT32 keysym;
        if (!RecvAll(sock, &down, 1)) return;
        if (!RecvAll(sock, padding, 2)) return;
        if (!RecvAll(sock, (char*)&keysym, 4)) return;
        keysym = ntohl(keysym);
        std::cout << "[VNC] KeyEvent: keysym=0x" << std::hex << keysym
                  << std::dec << " down=" << (int)down << std::endl;
        // TODO: dispatch to DriverInterface::InjectKeyDown/Up
    }

    void HandlePointerEvent(SOCKET sock) {
        char buttonMask;
        UINT16 x, y;
        if (!RecvAll(sock, &buttonMask, 1)) return;
        if (!RecvAll(sock, (char*)&x, 2)) return;
        if (!RecvAll(sock, (char*)&y, 2)) return;
        x = ntohs(x); y = ntohs(y);
        std::cout << "[VNC] PointerEvent: buttons=0x" << std::hex << (int)buttonMask
                  << std::dec << " x=" << x << " y=" << y << std::endl;
        // TODO: dispatch to DriverInterface::InjectMouseMove / InjectMouseButton
    }
};

// Public API
VNCServer::VNCServer() : impl_(new VncServerImpl()) {}
VNCServer::~VNCServer() { delete impl_; }
bool VNCServer::Start() { return impl_->Start(); }
void VNCServer::Stop() { impl_->Stop(); }

} // namespace Remote
} // namespace KVMDrivers

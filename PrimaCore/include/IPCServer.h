#pragma once
// ============================================================
// Prima Multi Seat - IPC Server
// Named pipe server for communication between PrimaCore
// (background engine) and PrimaUI (dashboard).
// ============================================================

#include "Common.h"

class SeatManager;

class IPCServer {
public:
    explicit IPCServer(SeatManager* seatManager);
    ~IPCServer();

    bool Start();
    void Stop();
    bool IsRunning() const { return m_running; }

private:
    SeatManager*        m_seatManager;
    std::atomic<bool>   m_running;
    std::thread         m_thread;
    HANDLE              m_hPipe;

    void ServerThread();
    void HandleClient(HANDLE hPipe);
    void ProcessMessage(const IPCPacket& packet, HANDLE hPipe);
    void SendResponse(HANDLE hPipe, IPCMessageType type, const std::string& payload);
};
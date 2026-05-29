// ============================================================
// Prima Multi Seat - IPC Server Implementation
// Named pipe server for UI ↔ Core communication
// ============================================================

#include "../include/IPCServer.h"
#include "../include/SeatManager.h"
#include <sstream>

IPCServer::IPCServer(SeatManager* seatManager)
    : m_seatManager(seatManager)
    , m_running(false)
    , m_hPipe(INVALID_HANDLE_VALUE)
{}

IPCServer::~IPCServer() { Stop(); }

bool IPCServer::Start() {
    m_running = true;
    m_thread  = std::thread(&IPCServer::ServerThread, this);
    LOG_INFO(L"IPC Server started on " IPC_PIPE_NAME);
    return true;
}

void IPCServer::Stop() {
    m_running = false;
    if (m_hPipe != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(m_hPipe);
        CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
    }
    if (m_thread.joinable()) m_thread.join();
    LOG_INFO(L"IPC Server stopped");
}

void IPCServer::ServerThread() {
    while (m_running) {
        m_hPipe = CreateNamedPipeW(
            IPC_PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            IPC_BUFFER_SIZE, IPC_BUFFER_SIZE,
            0, nullptr);

        if (m_hPipe == INVALID_HANDLE_VALUE) {
            LOG_ERROR(L"CreateNamedPipe failed");
            Sleep(1000);
            continue;
        }

        if (ConnectNamedPipe(m_hPipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) {
            HandleClient(m_hPipe);
        }

        DisconnectNamedPipe(m_hPipe);
        CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
    }
}

void IPCServer::HandleClient(HANDLE hPipe) {
    IPCPacket packet;
    DWORD bytesRead;

    while (m_running) {
        BOOL ok = ReadFile(hPipe, &packet, sizeof(packet), &bytesRead, nullptr);
        if (!ok || bytesRead == 0) break;
        ProcessMessage(packet, hPipe);
    }
}

void IPCServer::ProcessMessage(const IPCPacket& packet, HANDLE hPipe) {
    switch (packet.type) {
    case IPCMessageType::Ping:
        SendResponse(hPipe, IPCMessageType::Pong, "pong");
        break;

    case IPCMessageType::GetStatus:
        if (m_seatManager) {
            std::wstring status = m_seatManager->GetStatusJSON();
            int sz = WideCharToMultiByte(CP_UTF8, 0, status.c_str(), -1,
                                          nullptr, 0, nullptr, nullptr);
            std::string utf8(sz - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, status.c_str(), -1,
                                 utf8.data(), sz, nullptr, nullptr);
            SendResponse(hPipe, IPCMessageType::StatusResponse, utf8);
        }
        break;

    case IPCMessageType::StartSeat: {
        int seat = packet.payload[0] - '0';
        if (m_seatManager) m_seatManager->StartSeat(seat);
        SendResponse(hPipe, IPCMessageType::Pong, "ok");
        break;
    }

    case IPCMessageType::StopSeat: {
        int seat = packet.payload[0] - '0';
        if (m_seatManager) m_seatManager->StopSeat(seat);
        SendResponse(hPipe, IPCMessageType::Pong, "ok");
        break;
    }

    case IPCMessageType::Shutdown:
        PostQuitMessage(0);
        break;

    default:
        LOG_WARN(L"Unknown IPC message type");
        break;
    }
}

void IPCServer::SendResponse(HANDLE hPipe, IPCMessageType type,
                               const std::string& payload)
{
    IPCPacket resp;
    ZeroMemory(&resp, sizeof(resp));
    resp.type        = type;
    resp.payloadSize = (DWORD)min(payload.size(), sizeof(resp.payload) - 1);
    memcpy(resp.payload, payload.c_str(), resp.payloadSize);

    DWORD written;
    WriteFile(hPipe, &resp, sizeof(resp), &written, nullptr);
}
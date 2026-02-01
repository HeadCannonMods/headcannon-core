#include "headcannon/protocol/polling_udp_receiver.h"
#include "headcannon/protocol/opentrack_packet.h"

#include <cmath>
#include <cstring>
#include <chrono>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

namespace headcannon {

PollingUdpReceiver::~PollingUdpReceiver() {
    Shutdown();
}

bool PollingUdpReceiver::Initialize(uint16_t port) {
    if (m_initialized) {
        return true;
    }

    m_port = port;

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        return false;
    }
    m_wsaInitialized = true;
#endif

    if (!CreateSocket()) {
#ifdef _WIN32
        WSACleanup();
        m_wsaInitialized = false;
#endif
        return false;
    }

    if (!SetNonBlocking()) {
#ifdef _WIN32
        closesocket(m_socket);
#else
        close(m_socket);
#endif
        m_socket = INVALID_SOCKET;
#ifdef _WIN32
        WSACleanup();
        m_wsaInitialized = false;
#endif
        return false;
    }

    if (!BindSocket()) {
#ifdef _WIN32
        closesocket(m_socket);
#else
        close(m_socket);
#endif
        m_socket = INVALID_SOCKET;
#ifdef _WIN32
        WSACleanup();
        m_wsaInitialized = false;
#endif
        return false;
    }

    m_initialized = true;
    m_lastReceiveTimeMs = 0;
    m_packetsReceived = 0;
    m_bytesReceived = 0;
    m_isRemoteConnection = false;
    m_hasData = false;
    m_hasOffset = false;
    std::memset(m_receiveBuffer, 0, sizeof(m_receiveBuffer));

    return true;
}

void PollingUdpReceiver::Shutdown() {
    if (!m_initialized && !m_wsaInitialized) {
        return;
    }

    if (m_socket != INVALID_SOCKET) {
#ifdef _WIN32
        closesocket(m_socket);
#else
        close(m_socket);
#endif
        m_socket = INVALID_SOCKET;
    }

#ifdef _WIN32
    if (m_wsaInitialized) {
        WSACleanup();
        m_wsaInitialized = false;
    }
#endif

    m_initialized = false;
}

bool PollingUdpReceiver::Poll() {
    if (!m_initialized || m_socket == INVALID_SOCKET) {
        return false;
    }

    bool receivedAny = false;
    int packetsThisFrame = 0;
    constexpr int kMaxPacketsPerFrame = 1000;  // Safety limit

    // Drain ALL pending packets, keeping only the latest
    // This prevents lag from buffered packets when sender is faster than game fps
    while (packetsThisFrame < kMaxPacketsPerFrame) {
        sockaddr_in senderAddr;
#ifdef _WIN32
        int senderAddrLen = sizeof(senderAddr);
#else
        socklen_t senderAddrLen = sizeof(senderAddr);
#endif

        int bytesReceived = recvfrom(
            m_socket,
            m_receiveBuffer,
            static_cast<int>(sizeof(m_receiveBuffer)),
            0,
            reinterpret_cast<sockaddr*>(&senderAddr),
            &senderAddrLen
        );

        if (bytesReceived == SOCKET_ERROR) {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                break;  // No more data available
            }
#else
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                break;  // No more data available
            }
#endif
            break;  // Other error
        }

        if (bytesReceived == 0) {
            break;
        }

        packetsThisFrame++;

        if (ParsePacket(m_receiveBuffer, bytesReceived)) {
            m_packetsReceived++;
            m_bytesReceived += static_cast<uint64_t>(bytesReceived);
            m_lastReceiveTimeMs = GetCurrentTimeMs();
            receivedAny = true;

            // Detect remote connection (non-localhost)
            bool isLocalhost = (senderAddr.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
            m_isRemoteConnection = !isLocalhost;
        }
    }

    return receivedAny;
}

bool PollingUdpReceiver::GetPose(TrackingPose& pose) const {
    if (!m_hasData) {
        return false;
    }

    float yaw = m_yaw;
    float pitch = m_pitch;
    float roll = m_roll;

    if (m_hasOffset) {
        yaw -= m_yawOffset;
        pitch -= m_pitchOffset;
        roll -= m_rollOffset;
    }

    pose = TrackingPose(yaw, pitch, roll);
    return true;
}

bool PollingUdpReceiver::GetRawRotation(float& yaw, float& pitch, float& roll) const {
    if (!m_hasData) {
        return false;
    }

    yaw = m_yaw;
    pitch = m_pitch;
    roll = m_roll;
    return true;
}

bool PollingUdpReceiver::GetRotation(float& yaw, float& pitch, float& roll) const {
    if (!m_hasData) {
        return false;
    }

    yaw = m_yaw;
    pitch = m_pitch;
    roll = m_roll;

    if (m_hasOffset) {
        yaw -= m_yawOffset;
        pitch -= m_pitchOffset;
        roll -= m_rollOffset;
    }

    return true;
}

void PollingUdpReceiver::Recenter() {
    if (m_hasData) {
        m_yawOffset = m_yaw;
        m_pitchOffset = m_pitch;
        m_rollOffset = m_roll;
        m_hasOffset = true;
    }
}

void PollingUdpReceiver::ResetOffset() {
    m_yawOffset = 0.0f;
    m_pitchOffset = 0.0f;
    m_rollOffset = 0.0f;
    m_hasOffset = false;
}

bool PollingUdpReceiver::IsConnected() const {
    if (!m_initialized || m_lastReceiveTimeMs == 0) {
        return false;
    }

    int64_t elapsed = GetCurrentTimeMs() - m_lastReceiveTimeMs;
    return elapsed < kConnectionTimeoutMs;
}

bool PollingUdpReceiver::CreateSocket() {
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) {
        return false;
    }

    // Allow address reuse
    int reuseAddr = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuseAddr), sizeof(reuseAddr));

    // Set receive buffer size
    int recvBufSize = 65536;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF,
               reinterpret_cast<const char*>(&recvBufSize), sizeof(recvBufSize));

    return true;
}

bool PollingUdpReceiver::BindSocket() {
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    if (bind(m_socket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        return false;
    }

    return true;
}

bool PollingUdpReceiver::SetNonBlocking() {
#ifdef _WIN32
    u_long nonBlocking = 1;
    if (ioctlsocket(m_socket, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
        return false;
    }
#else
    int flags = fcntl(m_socket, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    if (fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
        return false;
    }
#endif
    return true;
}

bool PollingUdpReceiver::ParsePacket(const char* buffer, int bytesReceived) {
    // Use shared OpenTrack packet parsing
    TrackingPose pose;
    if (!OpenTrackPacket::TryParse(buffer, static_cast<size_t>(bytesReceived), pose)) {
        return false;
    }

    m_yaw = pose.yaw;
    m_pitch = pose.pitch;
    m_roll = pose.roll;
    m_hasData = true;

    return true;
}

int64_t PollingUdpReceiver::GetCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return ms.count();
}

}  // namespace headcannon

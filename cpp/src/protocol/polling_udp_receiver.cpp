#include "cameraunlock/protocol/polling_udp_receiver.h"
#include "cameraunlock/protocol/opentrack_packet.h"

#include <cmath>
#include <cstring>
#include <chrono>

namespace cameraunlock {

PollingUdpReceiver::~PollingUdpReceiver() {
    Shutdown();
}

bool PollingUdpReceiver::Initialize(uint16_t port) {
    if (m_initialized) {
        return true;
    }

    if (!m_socket.Open(port)) {
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
    if (!m_initialized) {
        return;
    }

    m_socket.Close();
    m_initialized = false;
}

bool PollingUdpReceiver::Poll() {
    if (!m_initialized || !m_socket.IsOpen()) {
        return false;
    }

    bool receivedAny = false;
    int packetsThisFrame = 0;
    constexpr int kMaxPacketsPerFrame = 1000;  // Safety limit

    SOCKET sock = m_socket.GetHandle();

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
            sock,
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
            receivedAny = true;

            m_isRemoteConnection = IsRemoteAddress(senderAddr);
        }
    }

    if (receivedAny) {
        m_lastReceiveTimeMs = GetCurrentTimeMs();
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

}  // namespace cameraunlock

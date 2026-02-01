#include "cameraunlock/protocol/udp_receiver.h"
#include "cameraunlock/protocol/opentrack_packet.h"
#include <chrono>

namespace cameraunlock {

UdpReceiver::~UdpReceiver() {
    Stop();
}

bool UdpReceiver::Start(uint16_t port) {
    if (m_running.load(std::memory_order_acquire)) {
        return true;
    }

    m_failed = false;

    if (!m_socket.Open(port)) {
        m_failed = true;
        return false;
    }

    // Start receiver thread
    m_stopFlag.store(false, std::memory_order_release);
    m_running.store(true, std::memory_order_release);
    m_thread = std::thread(&UdpReceiver::ReceiverThread, this);

    return true;
}

void UdpReceiver::Stop() {
    if (!m_running.load(std::memory_order_acquire)) {
        return;
    }

    m_stopFlag.store(true, std::memory_order_release);

    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_socket.Close();

    m_running.store(false, std::memory_order_release);
    m_trackingData.Reset();
    m_yawOffset.store(0.0f, std::memory_order_relaxed);
    m_pitchOffset.store(0.0f, std::memory_order_relaxed);
    m_rollOffset.store(0.0f, std::memory_order_relaxed);
    m_lastReceiveTimestamp.store(0, std::memory_order_relaxed);
    m_isRemoteConnection.store(false, std::memory_order_relaxed);
}

bool UdpReceiver::IsReceiving() const {
    int64_t lastTs = m_lastReceiveTimestamp.load(std::memory_order_acquire);
    if (lastTs == 0) return false;

    auto now = std::chrono::steady_clock::now();
    int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();

    int64_t elapsedMs = (nowUs - lastTs) / 1000;
    return elapsedMs < kConnectionTimeoutMs;
}

bool UdpReceiver::GetRotation(float& yaw, float& pitch, float& roll) const {
    float rawYaw, rawPitch, rawRoll;
    if (!m_trackingData.Get(rawYaw, rawPitch, rawRoll)) {
        return false;
    }

    yaw = rawYaw - m_yawOffset.load(std::memory_order_relaxed);
    pitch = rawPitch - m_pitchOffset.load(std::memory_order_relaxed);
    roll = rawRoll - m_rollOffset.load(std::memory_order_relaxed);

    return true;
}

void UdpReceiver::Recenter() {
    float yaw, pitch, roll;
    if (m_trackingData.Get(yaw, pitch, roll)) {
        m_yawOffset.store(yaw, std::memory_order_relaxed);
        m_pitchOffset.store(pitch, std::memory_order_relaxed);
        m_rollOffset.store(roll, std::memory_order_relaxed);
    }
}

void UdpReceiver::ReceiverThread() {
    constexpr size_t kReceiveBufferSize = 64;
    alignas(16) char buffer[kReceiveBufferSize];
    sockaddr_in senderAddr = {};
    int senderAddrSize = sizeof(senderAddr);

    SOCKET sock = m_socket.GetHandle();

#ifdef _WIN32
    WSAPOLLFD pollFd = {};
    pollFd.fd = sock;
    pollFd.events = POLLIN;
#endif

    while (!m_stopFlag.load(std::memory_order_relaxed)) {
#ifdef _WIN32
        int pollResult = WSAPoll(&pollFd, 1, 1);
        if (pollResult < 0) break;
        if (pollResult == 0) continue;

        int bytesReceived = recvfrom(
            sock,
            buffer,
            sizeof(buffer),
            0,
            reinterpret_cast<sockaddr*>(&senderAddr),
            &senderAddrSize
        );
#else
        socklen_t addrLen = sizeof(senderAddr);
        int bytesReceived = recvfrom(
            sock,
            buffer,
            sizeof(buffer),
            0,
            reinterpret_cast<sockaddr*>(&senderAddr),
            &addrLen
        );
#endif

        if (bytesReceived >= static_cast<int>(OpenTrackPacket::kMinPacketSize)) {
            TrackingPose pose;
            if (OpenTrackPacket::TryParse(buffer, bytesReceived, pose)) {
                m_trackingData.Set(pose.yaw, pose.pitch, pose.roll);

                m_isRemoteConnection.store(IsRemoteAddress(senderAddr), std::memory_order_relaxed);

                // Update timestamp
                auto now = std::chrono::steady_clock::now();
                int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
                    now.time_since_epoch()).count();
                m_lastReceiveTimestamp.store(nowUs, std::memory_order_release);
            }
        }
    }
}

}  // namespace cameraunlock

#include "headcannon/protocol/udp_receiver.h"
#include "headcannon/protocol/opentrack_packet.h"
#include <chrono>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

namespace headcannon {

UdpReceiver::~UdpReceiver() {
    Stop();
}

bool UdpReceiver::Start(uint16_t port) {
    if (m_running.load(std::memory_order_acquire)) {
        return true;
    }

    m_port = port;
    m_failed = false;

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        m_failed = true;
        return false;
    }
    m_wsaInitialized = true;
#endif

    // Create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) {
#ifdef _WIN32
        WSACleanup();
        m_wsaInitialized = false;
#endif
        m_failed = true;
        return false;
    }

    // Set socket to non-blocking
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(m_socket, FIONBIO, &mode) != 0) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        WSACleanup();
        m_wsaInitialized = false;
        m_failed = true;
        return false;
    }
#else
    int flags = fcntl(m_socket, F_GETFL, 0);
    fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);
#endif

    // Bind to all interfaces
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
#ifdef _WIN32
        closesocket(m_socket);
        WSACleanup();
        m_wsaInitialized = false;
#else
        close(m_socket);
#endif
        m_socket = INVALID_SOCKET;
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

    auto now = std::chrono::high_resolution_clock::now();
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
    alignas(16) char buffer[64];  // Slightly larger than 48 for safety
    sockaddr_in senderAddr = {};
    int senderAddrSize = sizeof(senderAddr);

#ifdef _WIN32
    WSAPOLLFD pollFd = {};
    pollFd.fd = m_socket;
    pollFd.events = POLLIN;
#endif

    while (!m_stopFlag.load(std::memory_order_relaxed)) {
#ifdef _WIN32
        int pollResult = WSAPoll(&pollFd, 1, 1);
        if (pollResult < 0) break;
        if (pollResult == 0) continue;

        int bytesReceived = recvfrom(
            m_socket,
            buffer,
            sizeof(buffer),
            0,
            reinterpret_cast<sockaddr*>(&senderAddr),
            &senderAddrSize
        );
#else
        socklen_t addrLen = sizeof(senderAddr);
        int bytesReceived = recvfrom(
            m_socket,
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

                // Check if remote
                bool isRemote = (ntohl(senderAddr.sin_addr.s_addr) != INADDR_LOOPBACK);
                m_isRemoteConnection.store(isRemote, std::memory_order_relaxed);

                // Update timestamp
                auto now = std::chrono::high_resolution_clock::now();
                int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
                    now.time_since_epoch()).count();
                m_lastReceiveTimestamp.store(nowUs, std::memory_order_release);
            }
        }
    }
}

}  // namespace headcannon

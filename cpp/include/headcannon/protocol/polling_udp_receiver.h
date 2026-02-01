#pragma once

#include <cstdint>
#include "headcannon/data/tracking_pose.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

namespace headcannon {

/// Polling-based UDP receiver for OpenTrack protocol.
/// Designed for single-threaded game loops where Poll() is called each frame.
/// This is an alternative to the threaded UdpReceiver for engines that prefer
/// explicit polling over background threads.
class PollingUdpReceiver {
public:
    /// Default OpenTrack UDP port.
    static constexpr uint16_t kDefaultPort = 4242;

    /// Connection timeout in milliseconds.
    static constexpr int kConnectionTimeoutMs = 1000;

    /// Maximum receive buffer size.
    static constexpr size_t kMaxBufferSize = 256;

    PollingUdpReceiver() = default;
    ~PollingUdpReceiver();

    // Non-copyable, non-movable
    PollingUdpReceiver(const PollingUdpReceiver&) = delete;
    PollingUdpReceiver& operator=(const PollingUdpReceiver&) = delete;
    PollingUdpReceiver(PollingUdpReceiver&&) = delete;
    PollingUdpReceiver& operator=(PollingUdpReceiver&&) = delete;

    /// Initializes the UDP socket on the specified port.
    /// @param port The UDP port to listen on.
    /// @return True if initialization succeeded.
    bool Initialize(uint16_t port = kDefaultPort);

    /// Shuts down the receiver and releases resources.
    void Shutdown();

    /// Polls for incoming data (non-blocking).
    /// Drains all pending packets and keeps only the latest.
    /// Should be called once per frame from the main game loop.
    /// @return True if new data was received this frame.
    bool Poll();

    /// Gets the latest tracking pose (rotation only, with offset applied).
    /// @param pose Output pose if data is available.
    /// @return True if valid data is available.
    bool GetPose(TrackingPose& pose) const;

    /// Gets the raw rotation values without offset applied.
    /// @return True if valid data is available.
    bool GetRawRotation(float& yaw, float& pitch, float& roll) const;

    /// Gets the rotation values with offset applied.
    /// @return True if valid data is available.
    bool GetRotation(float& yaw, float& pitch, float& roll) const;

    /// Sets the current position as the new center point.
    void Recenter();

    /// Resets the center offset to zero.
    void ResetOffset();

    /// True if the receiver is properly initialized.
    bool IsInitialized() const { return m_initialized; }

    /// True if data has been received recently (within timeout).
    bool IsConnected() const;

    /// True if the data source is from a remote (non-localhost) address.
    bool IsRemoteConnection() const { return m_isRemoteConnection; }

    /// Gets statistics about received data.
    uint64_t GetPacketsReceived() const { return m_packetsReceived; }
    uint64_t GetBytesReceived() const { return m_bytesReceived; }

private:
    bool CreateSocket();
    bool BindSocket();
    bool SetNonBlocking();
    bool ParsePacket(const char* buffer, int bytesReceived);
    int64_t GetCurrentTimeMs() const;

    SOCKET m_socket = INVALID_SOCKET;
    uint16_t m_port = kDefaultPort;
    bool m_initialized = false;
    bool m_wsaInitialized = false;

    // Latest received data (rotation only, in degrees)
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_roll = 0.0f;
    bool m_hasData = false;

    // Center offset for recentering
    float m_yawOffset = 0.0f;
    float m_pitchOffset = 0.0f;
    float m_rollOffset = 0.0f;
    bool m_hasOffset = false;

    // Connection state
    int64_t m_lastReceiveTimeMs = 0;
    bool m_isRemoteConnection = false;

    // Statistics
    uint64_t m_packetsReceived = 0;
    uint64_t m_bytesReceived = 0;

    // Receive buffer
    char m_receiveBuffer[kMaxBufferSize];
};

}  // namespace headcannon

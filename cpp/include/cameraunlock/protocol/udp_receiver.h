#pragma once

#include <atomic>
#include <thread>
#include <cstdint>
#include "cameraunlock/data/tracking_pose.h"
#include "cameraunlock/protocol/socket_types.h"
#include "cameraunlock/protocol/udp_socket.h"

namespace cameraunlock {

/// UDP receiver for OpenTrack protocol.
/// Thread-safe with lock-free reads on the game thread.
class UdpReceiver {
public:
    /// Default OpenTrack UDP port.
    static constexpr uint16_t kDefaultPort = 4242;

    /// Connection timeout in milliseconds.
    /// Lower than PollingUdpReceiver (500 vs 1000) because the threaded receiver
    /// checks more frequently and can detect disconnects sooner.
    static constexpr int kConnectionTimeoutMs = 500;

    UdpReceiver() = default;
    ~UdpReceiver();

    // Non-copyable
    UdpReceiver(const UdpReceiver&) = delete;
    UdpReceiver& operator=(const UdpReceiver&) = delete;

    /// Starts the UDP receiver on the specified port.
    /// @return True if started successfully.
    bool Start(uint16_t port = kDefaultPort);

    /// Stops the UDP receiver.
    void Stop();

    /// True if the receiver is running.
    bool IsRunning() const { return m_running.load(std::memory_order_acquire); }

    /// True if data has been received recently.
    bool IsReceiving() const;

    /// True if the data source is from a remote address.
    bool IsRemoteConnection() const { return m_isRemoteConnection.load(std::memory_order_relaxed); }

    /// True if initialization failed.
    bool IsFailed() const { return m_failed; }

    /// Gets the current rotation values with offset applied.
    /// @return True if data is available.
    bool GetRotation(float& yaw, float& pitch, float& roll) const;

    /// Sets the current position as the new center point.
    void Recenter();

private:
    void ReceiverThread();

    UdpSocket m_socket;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopFlag{false};
    bool m_failed = false;

    // Thread-safe tracking data
    TrackingData m_trackingData;

    // Offset for recentering
    std::atomic<float> m_yawOffset{0.0f};
    std::atomic<float> m_pitchOffset{0.0f};
    std::atomic<float> m_rollOffset{0.0f};

    // Timestamp for connection detection
    std::atomic<int64_t> m_lastReceiveTimestamp{0};
    std::atomic<bool> m_isRemoteConnection{false};
};

}  // namespace cameraunlock

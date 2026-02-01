#pragma once

#include <cstdint>
#include <cstddef>
#include "cameraunlock/data/tracking_pose.h"

namespace cameraunlock {

/// OpenTrack packet constants and parsing utilities.
struct OpenTrackPacket {
    /// Minimum packet size (6 doubles = 48 bytes).
    static constexpr size_t kMinPacketSize = 48;

    /// Byte offset of yaw in the packet.
    static constexpr size_t kYawOffset = 24;

    /// Byte offset of pitch in the packet.
    static constexpr size_t kPitchOffset = 32;

    /// Byte offset of roll in the packet.
    static constexpr size_t kRollOffset = 40;

    /// Attempts to parse an OpenTrack packet.
    /// @param data Raw packet data.
    /// @param length Length of the data in bytes.
    /// @param pose Output tracking pose if successful.
    /// @return True if parsing succeeded.
    static bool TryParse(const void* data, size_t length, TrackingPose& pose);
};

}  // namespace cameraunlock

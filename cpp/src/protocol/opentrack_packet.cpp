#include "cameraunlock/protocol/opentrack_packet.h"
#include <cmath>
#include <cstring>

namespace cameraunlock {

bool OpenTrackPacket::TryParse(const void* data, size_t length, TrackingPose& pose) {
    if (data == nullptr || length < kMinPacketSize) {
        return false;
    }

    const auto* bytes = static_cast<const uint8_t*>(data);

    double yaw, pitch, roll;
    std::memcpy(&yaw, bytes + kYawOffset, sizeof(double));
    std::memcpy(&pitch, bytes + kPitchOffset, sizeof(double));
    std::memcpy(&roll, bytes + kRollOffset, sizeof(double));

    // Validate values are not NaN or Infinity
    if (std::isnan(yaw) || std::isinf(yaw) ||
        std::isnan(pitch) || std::isinf(pitch) ||
        std::isnan(roll) || std::isinf(roll)) {
        return false;
    }

    pose = TrackingPose(static_cast<float>(yaw), static_cast<float>(pitch), static_cast<float>(roll));
    return true;
}

}  // namespace cameraunlock

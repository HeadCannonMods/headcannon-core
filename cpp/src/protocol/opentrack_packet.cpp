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

bool OpenTrackPacket::TryParsePosition(const void* data, size_t length, PositionData& position) {
    if (data == nullptr || length < kMinPacketSize) {
        return false;
    }

    const auto* bytes = static_cast<const uint8_t*>(data);

    double px, py, pz;
    std::memcpy(&px, bytes + kPosXOffset, sizeof(double));
    std::memcpy(&py, bytes + kPosYOffset, sizeof(double));
    std::memcpy(&pz, bytes + kPosZOffset, sizeof(double));

    if (std::isnan(px) || std::isinf(px) ||
        std::isnan(py) || std::isinf(py) ||
        std::isnan(pz) || std::isinf(pz)) {
        return false;
    }

    // OpenTrack position is in centimeters, convert to meters
    position = PositionData(
        static_cast<float>(px * 0.01),
        static_cast<float>(py * 0.01),
        static_cast<float>(pz * 0.01)
    );
    return true;
}

bool OpenTrackPacket::TryParseAll(const void* data, size_t length, TrackingPose& pose, PositionData& position) {
    if (data == nullptr || length < kMinPacketSize) {
        return false;
    }

    const auto* bytes = static_cast<const uint8_t*>(data);

    double px, py, pz, yaw, pitch, roll;
    std::memcpy(&px, bytes + kPosXOffset, sizeof(double));
    std::memcpy(&py, bytes + kPosYOffset, sizeof(double));
    std::memcpy(&pz, bytes + kPosZOffset, sizeof(double));
    std::memcpy(&yaw, bytes + kYawOffset, sizeof(double));
    std::memcpy(&pitch, bytes + kPitchOffset, sizeof(double));
    std::memcpy(&roll, bytes + kRollOffset, sizeof(double));

    if (std::isnan(px) || std::isinf(px) ||
        std::isnan(py) || std::isinf(py) ||
        std::isnan(pz) || std::isinf(pz) ||
        std::isnan(yaw) || std::isinf(yaw) ||
        std::isnan(pitch) || std::isinf(pitch) ||
        std::isnan(roll) || std::isinf(roll)) {
        return false;
    }

    pose = TrackingPose(static_cast<float>(yaw), static_cast<float>(pitch), static_cast<float>(roll));
    position = PositionData(
        static_cast<float>(px * 0.01),
        static_cast<float>(py * 0.01),
        static_cast<float>(pz * 0.01)
    );
    return true;
}

}  // namespace cameraunlock

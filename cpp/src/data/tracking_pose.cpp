#include "headcannon/data/tracking_pose.h"
#include <cmath>

namespace headcannon {

// TrackingData implementation
void TrackingData::Set(float y, float p, float r) {
    yaw.store(y, std::memory_order_relaxed);
    pitch.store(p, std::memory_order_relaxed);
    roll.store(r, std::memory_order_relaxed);
    has_data.store(true, std::memory_order_release);
}

bool TrackingData::Get(float& y, float& p, float& r) const {
    if (!has_data.load(std::memory_order_acquire)) {
        return false;
    }
    y = yaw.load(std::memory_order_relaxed);
    p = pitch.load(std::memory_order_relaxed);
    r = roll.load(std::memory_order_relaxed);
    return true;
}

void TrackingData::Reset() {
    yaw.store(0.0f, std::memory_order_relaxed);
    pitch.store(0.0f, std::memory_order_relaxed);
    roll.store(0.0f, std::memory_order_relaxed);
    has_data.store(false, std::memory_order_release);
}

// TrackingPose implementation
TrackingPose::TrackingPose(float y, float p, float r)
    : yaw(y), pitch(p), roll(r), timestamp_us(CurrentTimestamp()) {}

TrackingPose::TrackingPose(float y, float p, float r, int64_t ts)
    : yaw(y), pitch(p), roll(r), timestamp_us(ts) {}

bool TrackingPose::IsRecent(int max_age_ms) const {
    if (timestamp_us == 0) return false;
    int64_t now = CurrentTimestamp();
    int64_t elapsed_us = now - timestamp_us;
    return elapsed_us < (static_cast<int64_t>(max_age_ms) * 1000);
}

TrackingPose TrackingPose::SubtractOffset(const TrackingPose& offset) const {
    return TrackingPose(
        yaw - offset.yaw,
        pitch - offset.pitch,
        roll - offset.roll,
        timestamp_us
    );
}

TrackingPose TrackingPose::Zero() {
    return TrackingPose(0.0f, 0.0f, 0.0f);
}

int64_t TrackingPose::CurrentTimestamp() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

// SensitivitySettings implementation
SensitivitySettings SensitivitySettings::Uniform(float sensitivity) {
    SensitivitySettings s;
    s.yaw = sensitivity;
    s.pitch = sensitivity;
    s.roll = sensitivity;
    return s;
}

// RotationLimits implementation
RotationLimits RotationLimits::Unlimited() {
    RotationLimits r;
    r.yaw_min = -180.0f;
    r.yaw_max = 180.0f;
    r.pitch_min = -90.0f;
    r.pitch_max = 90.0f;
    r.roll_min = -180.0f;
    r.roll_max = 180.0f;
    return r;
}

RotationLimits RotationLimits::Symmetric(float yaw, float pitch, float roll) {
    RotationLimits r;
    r.yaw_min = -yaw;
    r.yaw_max = yaw;
    r.pitch_min = -pitch;
    r.pitch_max = pitch;
    r.roll_min = -roll;
    r.roll_max = roll;
    return r;
}

// DeadzoneSettings implementation
DeadzoneSettings DeadzoneSettings::Uniform(float deadzone) {
    DeadzoneSettings d;
    d.yaw = deadzone;
    d.pitch = deadzone;
    d.roll = deadzone;
    return d;
}

}  // namespace headcannon

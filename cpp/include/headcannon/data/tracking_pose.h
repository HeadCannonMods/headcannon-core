#pragma once

#include <cstdint>
#include <atomic>
#include <chrono>

namespace headcannon {

/// Thread-safe tracking data storage using atomics.
/// Cache-line aligned for optimal memory access in multi-threaded scenarios.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)  // structure was padded due to alignment specifier
#endif
struct alignas(64) TrackingData {
    std::atomic<float> yaw{0.0f};
    std::atomic<float> pitch{0.0f};
    std::atomic<float> roll{0.0f};
    std::atomic<bool> has_data{false};

    void Set(float y, float p, float r);
    bool Get(float& y, float& p, float& r) const;
    void Reset();
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

/// Immutable 3DOF tracking pose with timestamp.
struct TrackingPose {
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
    int64_t timestamp_us = 0;  // Microseconds since epoch

    TrackingPose() = default;
    TrackingPose(float y, float p, float r);
    TrackingPose(float y, float p, float r, int64_t ts);

    bool IsValid() const { return timestamp_us != 0; }
    bool IsRecent(int max_age_ms) const;

    TrackingPose SubtractOffset(const TrackingPose& offset) const;

    static TrackingPose Zero();
    static int64_t CurrentTimestamp();
};

/// Sensitivity multipliers and inversion flags.
struct SensitivitySettings {
    float yaw = 1.0f;
    float pitch = 1.0f;
    float roll = 1.0f;
    bool invert_yaw = false;
    bool invert_pitch = false;
    bool invert_roll = false;

    static SensitivitySettings Default() { return SensitivitySettings{}; }
    static SensitivitySettings Uniform(float sensitivity);
};

/// Rotation limits in degrees.
struct RotationLimits {
    float yaw_min = -75.0f;
    float yaw_max = 75.0f;
    float pitch_min = -45.0f;
    float pitch_max = 45.0f;
    float roll_min = -30.0f;
    float roll_max = 30.0f;

    static RotationLimits Default() { return RotationLimits{}; }
    static RotationLimits Unlimited();
    static RotationLimits Symmetric(float yaw, float pitch, float roll);
};

/// Deadzone thresholds in degrees.
struct DeadzoneSettings {
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;

    static DeadzoneSettings None() { return DeadzoneSettings{}; }
    static DeadzoneSettings Default() { return DeadzoneSettings{0.5f, 0.5f, 0.5f}; }
    static DeadzoneSettings Uniform(float deadzone);
};

}  // namespace headcannon

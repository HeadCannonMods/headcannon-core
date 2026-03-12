#pragma once

namespace cameraunlock {

/// Settings for positional tracking: per-axis sensitivity, limits, smoothing, and inversion.
/// Port of CameraUnlock.Core.Data.PositionSettings (C#).
struct PositionSettings {
    float sensitivity_x = 1.0f;
    float sensitivity_y = 1.0f;
    float sensitivity_z = 1.0f;
    float limit_x = 0.15f;
    float limit_y = 0.10f;
    float limit_z = 0.10f;
    float smoothing = 0.15f;
    bool invert_x = false;
    bool invert_y = false;
    bool invert_z = false;

    PositionSettings() = default;

    PositionSettings(float sens_x, float sens_y, float sens_z,
                     float lim_x, float lim_y, float lim_z,
                     float smooth,
                     bool inv_x = false, bool inv_y = false, bool inv_z = false)
        : sensitivity_x(sens_x), sensitivity_y(sens_y), sensitivity_z(sens_z)
        , limit_x(lim_x), limit_y(lim_y), limit_z(lim_z)
        , smoothing(smooth)
        , invert_x(inv_x), invert_y(inv_y), invert_z(inv_z) {}

    static PositionSettings Default() {
        return PositionSettings(1.0f, 1.0f, 1.0f, 0.15f, 0.10f, 0.10f, 0.15f);
    }
};

/// Configuration for the neck model pivot simulation.
/// Port of CameraUnlock.Core.Data.NeckModelSettings (C#).
struct NeckModelSettings {
    bool enabled = true;
    float neck_height = 0.10f;
    float neck_forward = 0.08f;

    NeckModelSettings() = default;

    NeckModelSettings(bool enabled, float height, float forward)
        : enabled(enabled), neck_height(height), neck_forward(forward) {}

    static NeckModelSettings Default() { return NeckModelSettings(true, 0.10f, 0.08f); }
    static NeckModelSettings Disabled() { return NeckModelSettings(false, 0.0f, 0.0f); }
};

}  // namespace cameraunlock

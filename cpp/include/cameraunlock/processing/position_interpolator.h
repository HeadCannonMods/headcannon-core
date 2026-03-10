#pragma once

#include "cameraunlock/data/position_data.h"

namespace cameraunlock {

/// Fills in frames between low-rate position samples using velocity extrapolation.
/// Port of CameraUnlock.Core.Processing.PositionInterpolator (C#).
class PositionInterpolator {
public:
    PositionInterpolator() = default;

    /// Maximum time (seconds) to extrapolate beyond the last sample.
    float GetMaxExtrapolationTime() const { return m_maxExtrapolationTime; }
    void SetMaxExtrapolationTime(float value) { m_maxExtrapolationTime = value; }

    /// Update with the latest raw position and frame delta time.
    /// Returns an interpolated (extrapolated) position.
    PositionData Update(const PositionData& raw, float delta_time) {
        if (!raw.IsValid()) {
            return raw;
        }

        bool is_new_sample = raw.timestamp_us != m_lastTimestampUs;

        if (is_new_sample) {
            if (m_hasAnySample) {
                // Include current frame's delta — m_timeSinceLastSample only has
                // stale-frame deltas accumulated so far, missing this frame.
                float sample_dt = m_timeSinceLastSample + delta_time;
                if (sample_dt > 0.0f) {
                    float inst_vel_x = (raw.x - m_lastX) / sample_dt;
                    float inst_vel_y = (raw.y - m_lastY) / sample_dt;
                    float inst_vel_z = (raw.z - m_lastZ) / sample_dt;

                    if (m_hasVelocity) {
                        m_velocityX += (inst_vel_x - m_velocityX) * kVelocityBlend;
                        m_velocityY += (inst_vel_y - m_velocityY) * kVelocityBlend;
                        m_velocityZ += (inst_vel_z - m_velocityZ) * kVelocityBlend;
                    } else {
                        m_velocityX = inst_vel_x;
                        m_velocityY = inst_vel_y;
                        m_velocityZ = inst_vel_z;
                        m_hasVelocity = true;
                    }
                }
            }

            m_lastTimestampUs = raw.timestamp_us;
            m_lastX = raw.x;
            m_lastY = raw.y;
            m_lastZ = raw.z;

            m_timeSinceLastSample = 0.0f;
            m_hasAnySample = true;

            return raw;
        }

        // No new sample this frame — extrapolate if we have velocity
        m_timeSinceLastSample += delta_time;

        if (!m_hasVelocity) {
            return raw;
        }

        float extrap_time = m_timeSinceLastSample;
        if (extrap_time > m_maxExtrapolationTime) {
            extrap_time = m_maxExtrapolationTime;
        }

        // Decay factor: velocity influence fades as we get further from the last sample
        // Uses 1/(1+r^2) — gentle near 0, only dampens near max extrapolation time.
        // (The original 1/(1+r)^2 was too aggressive: lost 26.6% at one-frame gaps.)
        float decay = 1.0f;
        if (m_maxExtrapolationTime > 0.0f) {
            float ratio = extrap_time / m_maxExtrapolationTime;
            decay = 1.0f / (1.0f + ratio * ratio);
        }

        float pred_x = m_lastX + m_velocityX * extrap_time * decay;
        float pred_y = m_lastY + m_velocityY * extrap_time * decay;
        float pred_z = m_lastZ + m_velocityZ * extrap_time * decay;

        return PositionData(pred_x, pred_y, pred_z, raw.timestamp_us);
    }

    /// Resets all interpolation state.
    void Reset() {
        m_lastTimestampUs = 0;
        m_lastX = 0.0f;
        m_lastY = 0.0f;
        m_lastZ = 0.0f;
        m_velocityX = 0.0f;
        m_velocityY = 0.0f;
        m_velocityZ = 0.0f;
        m_hasVelocity = false;
        m_timeSinceLastSample = 0.0f;
        m_hasAnySample = false;
    }

private:
    static constexpr float kVelocityBlend = 0.5f;

    float m_maxExtrapolationTime = 0.1f;

    int64_t m_lastTimestampUs = 0;
    float m_lastX = 0.0f;
    float m_lastY = 0.0f;
    float m_lastZ = 0.0f;

    float m_velocityX = 0.0f;
    float m_velocityY = 0.0f;
    float m_velocityZ = 0.0f;
    bool m_hasVelocity = false;

    float m_timeSinceLastSample = 0.0f;
    bool m_hasAnySample = false;
};

}  // namespace cameraunlock

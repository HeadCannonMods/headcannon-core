#include "headcannon/processing/tracking_processor.h"
#include "headcannon/math/deadzone_utils.h"
#include "headcannon/math/smoothing_utils.h"
#include "headcannon/math/angle_utils.h"

namespace headcannon {

TrackingPose TrackingProcessor::Process(float yaw, float pitch, float roll, bool is_remote_connection, float delta_time) {
    // Step 1: Apply center offset
    m_centerManager.ApplyOffset(yaw, pitch, roll);

    // Step 2: Apply deadzone
    yaw = static_cast<float>(math::ApplyDeadzone(yaw, m_deadzone.yaw));
    pitch = static_cast<float>(math::ApplyDeadzone(pitch, m_deadzone.pitch));
    roll = static_cast<float>(math::ApplyDeadzone(roll, m_deadzone.roll));

    // Step 3: Apply smoothing
    double effective_smoothing = math::GetEffectiveSmoothing(m_smoothingFactor, is_remote_connection);

    if (!m_hasSmoothedValue) {
        // First frame, snap to target
        m_smoothedYaw = yaw;
        m_smoothedPitch = pitch;
        m_smoothedRoll = roll;
        m_hasSmoothedValue = true;
    } else {
        m_smoothedYaw = math::Smooth(m_smoothedYaw, static_cast<double>(yaw), effective_smoothing, static_cast<double>(delta_time));
        m_smoothedPitch = math::Smooth(m_smoothedPitch, static_cast<double>(pitch), effective_smoothing, static_cast<double>(delta_time));
        m_smoothedRoll = math::Smooth(m_smoothedRoll, static_cast<double>(roll), effective_smoothing, static_cast<double>(delta_time));
    }

    // Step 4: Apply sensitivity
    float out_yaw = static_cast<float>(m_smoothedYaw) * m_sensitivity.yaw;
    float out_pitch = static_cast<float>(m_smoothedPitch) * m_sensitivity.pitch;
    float out_roll = static_cast<float>(m_smoothedRoll) * m_sensitivity.roll;

    if (m_sensitivity.invert_yaw) out_yaw = -out_yaw;
    if (m_sensitivity.invert_pitch) out_pitch = -out_pitch;
    if (m_sensitivity.invert_roll) out_roll = -out_roll;

    // Step 5: Apply limits
    out_yaw = math::Clamp(out_yaw, m_limits.yaw_min, m_limits.yaw_max);
    out_pitch = math::Clamp(out_pitch, m_limits.pitch_min, m_limits.pitch_max);
    out_roll = math::Clamp(out_roll, m_limits.roll_min, m_limits.roll_max);

    return TrackingPose(out_yaw, out_pitch, out_roll);
}

void TrackingProcessor::Recenter() {
    m_centerManager.SetCenter(
        static_cast<float>(m_smoothedYaw),
        static_cast<float>(m_smoothedPitch),
        static_cast<float>(m_smoothedRoll)
    );
}

void TrackingProcessor::RecenterTo(float yaw, float pitch, float roll) {
    m_centerManager.SetCenter(yaw, pitch, roll);
    m_smoothedYaw = 0.0;
    m_smoothedPitch = 0.0;
    m_smoothedRoll = 0.0;
}

void TrackingProcessor::Reset() {
    m_centerManager.Reset();
    m_smoothedYaw = 0.0;
    m_smoothedPitch = 0.0;
    m_smoothedRoll = 0.0;
    m_hasSmoothedValue = false;
}

}  // namespace headcannon

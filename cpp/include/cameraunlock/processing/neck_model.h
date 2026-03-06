#pragma once

#include "cameraunlock/math/vec3.h"
#include "cameraunlock/math/quat4.h"
#include "cameraunlock/data/position_settings.h"

namespace cameraunlock {

/// Computes eye position offset from head rotation, simulating that the head
/// rotates around the neck pivot rather than the eye center.
/// Port of CameraUnlock.Core.Processing.NeckModel (C#).
struct NeckModel {
    /// Computes the eye position offset caused by rotating around the neck pivot.
    /// Formula: headRotation.Rotate(neckToEyes) - neckToEyes
    static math::Vec3 ComputeOffset(const math::Quat4& head_rotation, const NeckModelSettings& settings) {
        if (!settings.enabled) {
            return math::Vec3::Zero();
        }

        math::Vec3 neck_to_eyes(0.0f, settings.neck_height, settings.neck_forward);
        math::Vec3 rotated = head_rotation.Rotate(neck_to_eyes);
        return rotated - neck_to_eyes;
    }
};

}  // namespace cameraunlock

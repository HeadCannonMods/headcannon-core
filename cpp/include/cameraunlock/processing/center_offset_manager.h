#pragma once

#include "cameraunlock/data/tracking_pose.h"

namespace cameraunlock {

/// Manages the center offset for head tracking recentering.
class CenterOffsetManager {
public:
    CenterOffsetManager() = default;

    /// The current center offset.
    const TrackingPose& GetCenterOffset() const { return m_centerOffset; }

    /// True if a center has been set.
    bool HasValidCenter() const { return m_hasValidCenter; }

    /// Sets the center offset to the specified pose.
    void SetCenter(const TrackingPose& pose);

    /// Sets the center offset using individual components.
    void SetCenter(float yaw, float pitch, float roll);

    /// Applies the offset to individual values.
    void ApplyOffset(float& yaw, float& pitch, float& roll) const;

    /// Resets the center offset.
    void Reset();

private:
    TrackingPose m_centerOffset;
    bool m_hasValidCenter = false;
};

}  // namespace cameraunlock

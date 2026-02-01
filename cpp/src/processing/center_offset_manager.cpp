#include "cameraunlock/processing/center_offset_manager.h"

namespace cameraunlock {

void CenterOffsetManager::SetCenter(const TrackingPose& pose) {
    m_centerOffset = TrackingPose(pose.yaw, pose.pitch, pose.roll, 0);
    m_hasValidCenter = true;
}

void CenterOffsetManager::SetCenter(float yaw, float pitch, float roll) {
    m_centerOffset = TrackingPose(yaw, pitch, roll, 0);
    m_hasValidCenter = true;
}

void CenterOffsetManager::ApplyOffset(float& yaw, float& pitch, float& roll) const {
    if (!m_hasValidCenter) {
        return;
    }
    yaw -= m_centerOffset.yaw;
    pitch -= m_centerOffset.pitch;
    roll -= m_centerOffset.roll;
}

void CenterOffsetManager::Reset() {
    m_centerOffset = TrackingPose();
    m_hasValidCenter = false;
}

}  // namespace cameraunlock

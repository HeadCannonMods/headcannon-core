#pragma once

#include <cameraunlock/math/rotation_utils.h>
#include <cmath>

namespace cameraunlock::rendering {

// Parameters for crosshair projection calculation
struct CrosshairProjectionParams {
    // Screen dimensions
    float screenWidth = 1920.0f;
    float screenHeight = 1080.0f;

    // Camera field of view in degrees (horizontal)
    float fovDegrees = 75.0f;

    // Head tracking offsets in degrees
    // Positive yaw = looking right, positive pitch = looking up
    float yawOffset = 0.0f;
    float pitchOffset = 0.0f;
    float rollOffset = 0.0f;

    // Base camera pitch from game input (gamepad/mouse) in radians
    // This is the pitch before head tracking is applied
    float gameCameraPitch = 0.0f;
};

// Result of crosshair projection
struct ScreenPosition {
    float x = 0.0f;
    float y = 0.0f;
    bool valid = false;
};

// Constants
constexpr float kDegToRad = 0.0174532925f;  // pi / 180

// Project crosshair position based on head tracking and camera state
// This computes where the "body aim" direction appears on screen when
// the camera has been rotated by head tracking.
//
// The crosshair represents where the player's body is actually aiming,
// which moves opposite to head movement on screen.
//
// Coordinate system assumption (modify for your game):
// - Forward is +X
// - Up is +Y
// - Right is -Z (left-handed, Z points left)
inline ScreenPosition ProjectCrosshair(const CrosshairProjectionParams& params) {
    ScreenPosition result;
    result.valid = false;

    // Convert angles to radians
    // Negate so crosshair moves opposite to head movement
    float yawRad = -params.yawOffset * kDegToRad;
    float pitchRad = -params.pitchOffset * kDegToRad;
    float rollRad = -params.rollOffset * kDegToRad;

    // Precompute trig values
    float cosYaw = std::cos(yawRad), sinYaw = std::sin(yawRad);
    float cosPitch = std::cos(pitchRad), sinPitch = std::sin(pitchRad);
    float cosRoll = std::cos(rollRad), sinRoll = std::sin(rollRad);

    // FOV for perspective projection
    float aspectRatio = params.screenWidth / params.screenHeight;
    float hFovRad = params.fovDegrees * kDegToRad;
    float tanHalfHFov = std::tan(hFovRad / 2.0f);
    float tanHalfVFov = tanHalfHFov / aspectRatio;

    // Body starts at camera forward (1, 0, 0) in camera space before head rotation
    float bx = 1.0f, by = 0.0f, bz = 0.0f;

    // World up in camera space (when camera is pitched, world up tilts)
    // Coordinate system: X=forward, Y=up, Z=left
    float gamePitch = params.gameCameraPitch;
    float worldUpX = std::sin(gamePitch);
    float worldUpY = std::cos(gamePitch);

    // Game applies rotation order: Yaw (world Y) -> Pitch (post-yaw right) -> Roll (post-yaw-pitch forward)
    // We compute inverse to find where body aims in head-rotated camera space

    // Compute post-yaw right axis by rotating original right (0,0,-1) around worldUp by yaw
    float postYawRightX = -worldUpY * sinYaw;
    float postYawRightY = worldUpX * sinYaw;
    float postYawRightZ = -cosYaw;

    // Compute post-yaw forward by rotating original forward (1,0,0) around worldUp by yaw
    float omcYaw = 1.0f - cosYaw;
    float postYawFwdX = cosYaw + worldUpX * worldUpX * omcYaw;
    float postYawFwdY = worldUpX * worldUpY * omcYaw;
    float postYawFwdZ = -worldUpY * sinYaw;

    // Compute final forward by rotating post-yaw forward around post-yaw right by pitch
    float finalFwdAxis[3] = { postYawRightX, postYawRightY, postYawRightZ };
    float postYawFwd[3] = { postYawFwdX, postYawFwdY, postYawFwdZ };
    float finalFwd[3];
    cameraunlock::math::RotateAroundAxis(postYawFwd, finalFwdAxis, cosPitch, sinPitch, finalFwd);

    // Step 1: Inverse roll around final forward axis
    float b1[3];
    float bodyIn[3] = { bx, by, bz };
    cameraunlock::math::RotateAroundAxis(bodyIn, finalFwd, cosRoll, -sinRoll, b1);

    // Step 2: Inverse pitch around post-yaw right axis
    float b2[3];
    cameraunlock::math::RotateAroundAxis(b1, finalFwdAxis, cosPitch, -sinPitch, b2);

    // Step 3: Inverse yaw around world up axis
    float worldUp[3] = { worldUpX, worldUpY, 0.0f };
    float bodyFinal[3];
    cameraunlock::math::RotateAroundAxis(b2, worldUp, cosYaw, -sinYaw, bodyFinal);

    bx = bodyFinal[0];
    by = bodyFinal[1];
    bz = bodyFinal[2];

    // Prevent division by zero when looking backwards
    if (bx < 0.01f) bx = 0.01f;

    // Check for NaN
    if (bx != bx || by != by || bz != bz) {
        bx = 1.0f; by = 0.0f; bz = 0.0f;
    }

    // Project to normalized screen coordinates using perspective division
    float normalizedX = bz / (bx * tanHalfHFov);
    float normalizedY = by / (bx * tanHalfVFov);

    // Convert to screen pixels (Y is inverted for screen coordinates)
    float cx = (params.screenWidth / 2.0f) + normalizedX * (params.screenWidth / 2.0f);
    float cy = (params.screenHeight / 2.0f) - normalizedY * (params.screenHeight / 2.0f);

    // Final NaN check
    if (cx != cx || cy != cy) {
        cx = params.screenWidth / 2.0f;
        cy = params.screenHeight / 2.0f;
    }

    result.x = cx;
    result.y = cy;
    result.valid = true;

    return result;
}

// Clamp screen position to visible area with margin
inline void ClampToScreen(ScreenPosition& pos, float screenWidth, float screenHeight, float margin = 10.0f) {
    if (pos.x < margin) pos.x = margin;
    if (pos.x > screenWidth - margin) pos.x = screenWidth - margin;
    if (pos.y < margin) pos.y = margin;
    if (pos.y > screenHeight - margin) pos.y = screenHeight - margin;
}

} // namespace cameraunlock::rendering

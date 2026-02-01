using UnityEngine;

namespace CameraUnlock.Core.Unity.Tracking
{
    /// <summary>
    /// Helper for implementing decoupled look/movement in first-person games.
    ///
    /// Many FPS games use the camera or player body rotation to determine movement direction.
    /// When head tracking is applied to the camera, this couples look direction with movement,
    /// causing the player to walk where they're looking rather than where they're aiming.
    ///
    /// This helper implements the "split transform" pattern:
    /// - Player body transform: Controls movement direction (pure mouse/gamepad aim)
    /// - Camera transform: Controls view direction (aim + head tracking offset)
    ///
    /// Usage:
    /// 1. Track the "pure aim" yaw (mouse input only, without head tracking)
    /// 2. Apply tracking yaw to camera's local rotation (child of player body)
    /// 3. Keep player body at pure aim yaw (what movement system reads)
    /// 4. Apply tracking pitch/roll to player body (they don't affect horizontal movement)
    /// </summary>
    public static class DecoupledMovementHelper
    {
        /// <summary>
        /// Applies head tracking in decoupled mode, where tracking yaw doesn't affect movement.
        /// </summary>
        /// <param name="playerBodyTransform">The player body transform (parent, controls movement direction).</param>
        /// <param name="cameraTransform">The camera transform (child, controls view direction).</param>
        /// <param name="pureAimYaw">The pure aim yaw from mouse/gamepad input (degrees, 0-360).</param>
        /// <param name="trackingYaw">Head tracking yaw in degrees.</param>
        /// <param name="trackingPitch">Head tracking pitch in degrees.</param>
        /// <param name="trackingRoll">Head tracking roll in degrees.</param>
        /// <param name="invertPitch">Whether to invert pitch for natural head movement (default: true).</param>
        public static void ApplyDecoupled(
            Transform playerBodyTransform,
            Transform cameraTransform,
            float pureAimYaw,
            float trackingYaw,
            float trackingPitch,
            float trackingRoll,
            bool invertPitch = true)
        {
            if (playerBodyTransform == null)
            {
                return;
            }

            // === PLAYER BODY TRANSFORM ===
            // Keep yaw at pure aim only (this is what movement systems read)
            // Apply pitch and roll here (they don't affect horizontal movement direction)
            float bodyYaw = NormalizeAngle(pureAimYaw);
            float bodyPitch = invertPitch ? -trackingPitch : trackingPitch;
            float bodyRoll = trackingRoll;

            playerBodyTransform.localEulerAngles = new Vector3(bodyPitch, bodyYaw, bodyRoll);

            // === CAMERA TRANSFORM ===
            // Apply tracking yaw as local rotation offset on camera
            // This allows looking around without affecting movement direction
            if (cameraTransform != null && cameraTransform != playerBodyTransform)
            {
                var cameraEuler = cameraTransform.localEulerAngles;
                // Keep current pitch (controlled by game's pitch system), add tracking yaw
                cameraTransform.localEulerAngles = new Vector3(cameraEuler.x, trackingYaw, cameraEuler.z);
            }
        }

        /// <summary>
        /// Fades out head tracking smoothly while maintaining decoupled mode.
        /// </summary>
        /// <param name="playerBodyTransform">The player body transform.</param>
        /// <param name="cameraTransform">The camera transform.</param>
        /// <param name="pureAimYaw">The pure aim yaw from mouse/gamepad input (degrees, 0-360).</param>
        /// <param name="lastTrackingYaw">The last tracking yaw that was applied.</param>
        /// <param name="lastTrackingPitch">The last tracking pitch that was applied.</param>
        /// <param name="lastTrackingRoll">The last tracking roll that was applied.</param>
        /// <param name="fadeProgress">Fade progress from 0 (full tracking) to 1 (no tracking).</param>
        /// <param name="invertPitch">Whether pitch was inverted during tracking.</param>
        public static void ApplyDecoupledFadeOut(
            Transform playerBodyTransform,
            Transform cameraTransform,
            float pureAimYaw,
            float lastTrackingYaw,
            float lastTrackingPitch,
            float lastTrackingRoll,
            float fadeProgress,
            bool invertPitch = true)
        {
            if (playerBodyTransform == null)
            {
                return;
            }

            // Fade all tracking values toward zero
            float fadedYaw = Mathf.Lerp(lastTrackingYaw, 0f, fadeProgress);
            float fadedPitch = Mathf.Lerp(lastTrackingPitch, 0f, fadeProgress);
            float fadedRoll = Mathf.Lerp(lastTrackingRoll, 0f, fadeProgress);

            // === PLAYER BODY TRANSFORM ===
            float bodyYaw = NormalizeAngle(pureAimYaw);
            float bodyPitch = invertPitch ? -fadedPitch : fadedPitch;

            playerBodyTransform.localEulerAngles = new Vector3(bodyPitch, bodyYaw, fadedRoll);

            // === CAMERA TRANSFORM ===
            if (cameraTransform != null && cameraTransform != playerBodyTransform)
            {
                var cameraEuler = cameraTransform.localEulerAngles;
                cameraTransform.localEulerAngles = new Vector3(cameraEuler.x, fadedYaw, cameraEuler.z);
            }
        }

        /// <summary>
        /// Resets the camera's local yaw offset to zero (used when tracking ends).
        /// </summary>
        /// <param name="cameraTransform">The camera transform.</param>
        /// <param name="playerBodyTransform">The player body transform (to ensure camera is different).</param>
        public static void ResetCameraYawOffset(Transform cameraTransform, Transform playerBodyTransform)
        {
            if (cameraTransform != null && cameraTransform != playerBodyTransform)
            {
                var euler = cameraTransform.localEulerAngles;
                cameraTransform.localEulerAngles = new Vector3(euler.x, 0f, euler.z);
            }
        }

        /// <summary>
        /// Normalizes an angle to the 0-360 range.
        /// </summary>
        private static float NormalizeAngle(float angle)
        {
            while (angle < 0f) angle += 360f;
            while (angle >= 360f) angle -= 360f;
            return angle;
        }

        /// <summary>
        /// Converts an angle from 0-360 to -180 to +180 range.
        /// </summary>
        public static float ToSignedAngle(float angle)
        {
            if (angle > 180f) return angle - 360f;
            return angle;
        }
    }
}

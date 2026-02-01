using System;
using CameraUnlock.Core.Math;

namespace CameraUnlock.Core.Aim
{
    /// <summary>
    /// Framework-agnostic screen offset calculation using FOV-based tangent projection.
    /// Based on the proven pattern from green-hell's AimController.
    /// </summary>
    public static class ScreenOffsetCalculator
    {
        /// <summary>
        /// Calculates screen offset using tangent-based FOV projection.
        /// This method works without Unity's camera system.
        /// </summary>
        /// <param name="yawDegrees">Head tracking yaw offset in degrees.</param>
        /// <param name="pitchDegrees">Head tracking pitch offset in degrees.</param>
        /// <param name="rollDegrees">Head tracking roll offset in degrees.</param>
        /// <param name="horizontalFov">Camera horizontal FOV in degrees.</param>
        /// <param name="verticalFov">Camera vertical FOV in degrees.</param>
        /// <param name="screenWidth">Screen width in pixels.</param>
        /// <param name="screenHeight">Screen height in pixels.</param>
        /// <param name="compensationScale">Optional scale factor for sensitivity tuning (default 1.0).</param>
        /// <param name="offsetX">Output: Screen offset X in pixels from center.</param>
        /// <param name="offsetY">Output: Screen offset Y in pixels from center.</param>
        public static void Calculate(
            float yawDegrees,
            float pitchDegrees,
            float rollDegrees,
            float horizontalFov,
            float verticalFov,
            float screenWidth,
            float screenHeight,
            float compensationScale,
            out float offsetX,
            out float offsetY)
        {
            float halfWidth = screenWidth * 0.5f;
            float halfHeight = screenHeight * 0.5f;

            // Calculate tangent of half FOV angles
            float tanHalfFovX = (float)System.Math.Tan(horizontalFov * MathConstants.DegToRad * 0.5f);
            float tanHalfFovY = (float)System.Math.Tan(verticalFov * MathConstants.DegToRad * 0.5f);

            // Calculate screen offset using tangent projection
            // Negative yaw because head turn right should move crosshair left
            offsetX = -(float)System.Math.Tan(yawDegrees * MathConstants.DegToRad) / tanHalfFovX * halfWidth * compensationScale;
            offsetY = (float)System.Math.Tan(pitchDegrees * MathConstants.DegToRad) / tanHalfFovY * halfHeight * compensationScale;

            // Apply roll as 2D screen-space rotation
            // This prevents cross-axis contamination that occurs with 3D roll application
            ApplyRollRotation(offsetX, offsetY, rollDegrees, out offsetX, out offsetY);
        }

#if NETSTANDARD2_0
        /// <summary>
        /// Calculates screen offset (tuple version for modern runtimes).
        /// </summary>
        public static (float x, float y) CalculateTuple(
            float yawDegrees,
            float pitchDegrees,
            float rollDegrees,
            float horizontalFov,
            float verticalFov,
            float screenWidth,
            float screenHeight,
            float compensationScale = 1.0f)
        {
            Calculate(yawDegrees, pitchDegrees, rollDegrees, horizontalFov, verticalFov,
                screenWidth, screenHeight, compensationScale, out float x, out float y);
            return (x, y);
        }
#endif

        /// <summary>
        /// Pre-computes the tangent of half-FOV values for use with the optimized Calculate overload.
        /// Call this once when FOV changes, then use the tangent values every frame.
        /// </summary>
        /// <param name="horizontalFov">Camera horizontal FOV in degrees.</param>
        /// <param name="verticalFov">Camera vertical FOV in degrees.</param>
        /// <param name="tanHalfFovX">Output: Pre-computed tan(horizontalFov/2).</param>
        /// <param name="tanHalfFovY">Output: Pre-computed tan(verticalFov/2).</param>
        public static void PrecomputeFovTangents(float horizontalFov, float verticalFov,
            out float tanHalfFovX, out float tanHalfFovY)
        {
            tanHalfFovX = (float)System.Math.Tan(horizontalFov * MathConstants.DegToRad * 0.5f);
            tanHalfFovY = (float)System.Math.Tan(verticalFov * MathConstants.DegToRad * 0.5f);
        }

        /// <summary>
        /// Calculates screen offset using pre-computed FOV tangent values.
        /// Use <see cref="PrecomputeFovTangents"/> to compute tangent values once when FOV changes.
        /// </summary>
        /// <param name="yawDegrees">Head tracking yaw offset in degrees.</param>
        /// <param name="pitchDegrees">Head tracking pitch offset in degrees.</param>
        /// <param name="rollDegrees">Head tracking roll offset in degrees.</param>
        /// <param name="tanHalfFovX">Pre-computed tan(horizontalFov/2).</param>
        /// <param name="tanHalfFovY">Pre-computed tan(verticalFov/2).</param>
        /// <param name="halfWidth">Half screen width in pixels.</param>
        /// <param name="halfHeight">Half screen height in pixels.</param>
        /// <param name="compensationScale">Scale factor for sensitivity tuning.</param>
        /// <param name="offsetX">Output: Screen offset X in pixels from center.</param>
        /// <param name="offsetY">Output: Screen offset Y in pixels from center.</param>
        public static void CalculatePrecomputed(
            float yawDegrees,
            float pitchDegrees,
            float rollDegrees,
            float tanHalfFovX,
            float tanHalfFovY,
            float halfWidth,
            float halfHeight,
            float compensationScale,
            out float offsetX,
            out float offsetY)
        {
            offsetX = -(float)System.Math.Tan(yawDegrees * MathConstants.DegToRad) / tanHalfFovX * halfWidth * compensationScale;
            offsetY = (float)System.Math.Tan(pitchDegrees * MathConstants.DegToRad) / tanHalfFovY * halfHeight * compensationScale;

            ApplyRollRotation(offsetX, offsetY, rollDegrees, out offsetX, out offsetY);
        }

        /// <summary>
        /// Calculates horizontal FOV from vertical FOV and aspect ratio.
        /// </summary>
        /// <param name="verticalFov">Vertical FOV in degrees.</param>
        /// <param name="aspectRatio">Screen aspect ratio (width/height).</param>
        /// <returns>Horizontal FOV in degrees.</returns>
        public static float CalculateHorizontalFov(float verticalFov, float aspectRatio)
        {
            float tanHalfVFov = (float)System.Math.Tan(verticalFov * MathConstants.DegToRad * 0.5f);
            return 2f * (float)System.Math.Atan(tanHalfVFov * aspectRatio) / MathConstants.DegToRad;
        }

        /// <summary>
        /// Applies 2D roll rotation to screen offset.
        /// </summary>
        /// <param name="x">X offset input.</param>
        /// <param name="y">Y offset input.</param>
        /// <param name="rollDegrees">Roll angle in degrees.</param>
        /// <param name="rotatedX">Output: Rotated X offset.</param>
        /// <param name="rotatedY">Output: Rotated Y offset.</param>
        public static void ApplyRollRotation(float x, float y, float rollDegrees, out float rotatedX, out float rotatedY)
        {
            if (System.Math.Abs(rollDegrees) < 0.001f)
            {
                rotatedX = x;
                rotatedY = y;
                return;
            }

            // Negate roll to match view matrix convention
            float rollRad = -rollDegrees * MathConstants.DegToRad;
            float cos = (float)System.Math.Cos(rollRad);
            float sin = (float)System.Math.Sin(rollRad);

            rotatedX = x * cos - y * sin;
            rotatedY = x * sin + y * cos;
        }

#if NETSTANDARD2_0
        /// <summary>
        /// Applies 2D roll rotation (tuple version for modern runtimes).
        /// </summary>
        public static (float x, float y) ApplyRollRotationTuple(float x, float y, float rollDegrees)
        {
            ApplyRollRotation(x, y, rollDegrees, out float rx, out float ry);
            return (rx, ry);
        }
#endif
    }
}

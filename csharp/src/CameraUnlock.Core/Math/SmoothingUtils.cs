using System;

#if !NET35 && !NET40
using System.Runtime.CompilerServices;
#endif

namespace CameraUnlock.Core.Math
{
    /// <summary>
    /// Frame-rate independent exponential smoothing utilities.
    /// Performance-critical methods are aggressively inlined on supported frameworks.
    /// </summary>
    public static class SmoothingUtils
    {
        /// <summary>
        /// Baseline smoothing factor for remote (non-localhost) connections.
        /// Compensates for network latency jitter.
        /// 0.15 gives ~40% per frame at 60fps, settling in ~100-150ms.
        /// </summary>
        public const float RemoteConnectionBaseline = 0.15f;

        // Pre-computed constants to avoid repeated calculation
        private const float SmoothingSpeedMax = 50f;
        private const float SmoothingSpeedMin = 0.1f;
        private const float SmoothingSpeedRange = SmoothingSpeedMax - SmoothingSpeedMin;
        private const float SmoothingThreshold = 0.001f;

        /// <summary>
        /// Calculates the smoothing interpolation factor for the current frame.
        /// Uses frame-rate independent exponential smoothing.
        /// </summary>
        /// <param name="smoothing">Smoothing factor 0-1. 0=instant, 1=very slow.</param>
        /// <param name="deltaTime">Time since last frame in seconds.</param>
        /// <returns>Interpolation factor to use with Lerp/Slerp.</returns>
#if !NET35 && !NET40
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
#endif
        public static float CalculateSmoothingFactor(float smoothing, float deltaTime)
        {
            if (smoothing < SmoothingThreshold)
            {
                return 1f;
            }

            // Optimized: avoid Lerp call, direct calculation
            float smoothingSpeed = SmoothingSpeedMax - SmoothingSpeedRange * smoothing;
            return 1f - (float)System.Math.Exp(-smoothingSpeed * deltaTime);
        }

        /// <summary>
        /// Applies smoothing to a single value.
        /// </summary>
        /// <param name="current">Current smoothed value.</param>
        /// <param name="target">Target value to smooth towards.</param>
        /// <param name="smoothing">Smoothing factor 0-1.</param>
        /// <param name="deltaTime">Time since last frame in seconds.</param>
        /// <returns>New smoothed value.</returns>
#if !NET35 && !NET40
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
#endif
        public static float Smooth(float current, float target, float smoothing, float deltaTime)
        {
            float t = CalculateSmoothingFactor(smoothing, deltaTime);
            return current + (target - current) * t;
        }

        /// <summary>
        /// Applies smoothing to a double value.
        /// </summary>
#if !NET35 && !NET40
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
#endif
        public static double Smooth(double current, double target, float smoothing, float deltaTime)
        {
            float t = CalculateSmoothingFactor(smoothing, deltaTime);
            return current + (target - current) * t;
        }

        /// <summary>
        /// Gets the effective smoothing factor, applying baseline for remote connections.
        /// </summary>
        /// <param name="baseSmoothing">Base smoothing factor from configuration.</param>
        /// <param name="isRemoteConnection">True if data is from a non-localhost source.</param>
        /// <returns>Effective smoothing factor to use.</returns>
#if !NET35 && !NET40
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
#endif
        public static float GetEffectiveSmoothing(float baseSmoothing, bool isRemoteConnection)
        {
            if (isRemoteConnection && baseSmoothing < RemoteConnectionBaseline)
            {
                return RemoteConnectionBaseline;
            }
            return baseSmoothing;
        }
    }
}

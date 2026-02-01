using CameraUnlock.Core.Data;
using CameraUnlock.Core.Math;

namespace CameraUnlock.Core.Processing
{
    /// <summary>
    /// Complete tracking data processing pipeline.
    /// Pipeline: raw -> offset -> deadzone -> smooth -> sensitivity
    /// </summary>
    public sealed class TrackingProcessor : ITrackingProcessor
    {
        private readonly CenterOffsetManager _centerManager = new CenterOffsetManager();

        // Smoothed values (stored as doubles for precision)
        private double _smoothedYaw;
        private double _smoothedPitch;
        private double _smoothedRoll;
        private bool _hasSmoothedValue;

        /// <summary>
        /// Sensitivity settings.
        /// </summary>
        public SensitivitySettings Sensitivity { get; set; } = SensitivitySettings.Default;

        /// <summary>
        /// Deadzone settings.
        /// </summary>
        public DeadzoneSettings Deadzone { get; set; } = DeadzoneSettings.None;

        /// <summary>
        /// Smoothing factor (0 = instant, 1 = very slow).
        /// </summary>
        public float SmoothingFactor { get; set; } = 0f;

        /// <summary>
        /// The center offset manager for recentering.
        /// </summary>
        public CenterOffsetManager CenterManager => _centerManager;

        /// <summary>
        /// Gets the current smoothed rotation values.
        /// </summary>
        /// <param name="yaw">Output: smoothed yaw.</param>
        /// <param name="pitch">Output: smoothed pitch.</param>
        /// <param name="roll">Output: smoothed roll.</param>
        public void GetSmoothedRotation(out float yaw, out float pitch, out float roll)
        {
            yaw = (float)_smoothedYaw;
            pitch = (float)_smoothedPitch;
            roll = (float)_smoothedRoll;
        }

#if NETSTANDARD2_0
        /// <summary>
        /// Gets the current smoothed rotation values (tuple version for modern runtimes).
        /// </summary>
        public (float Yaw, float Pitch, float Roll) SmoothedRotation =>
            ((float)_smoothedYaw, (float)_smoothedPitch, (float)_smoothedRoll);
#endif

        /// <summary>
        /// Processes a raw tracking pose through the full pipeline.
        /// </summary>
        public TrackingPose Process(TrackingPose rawPose, bool isRemoteConnection, float deltaTime)
        {
            if (!rawPose.IsValid)
            {
                return rawPose;
            }

            // Step 1: Apply center offset
            _centerManager.ApplyOffset(rawPose.Yaw, rawPose.Pitch, rawPose.Roll, out float yaw, out float pitch, out float roll);

            // Step 2: Apply deadzone
            yaw = (float)DeadzoneUtils.Apply(yaw, Deadzone.Yaw);
            pitch = (float)DeadzoneUtils.Apply(pitch, Deadzone.Pitch);
            roll = (float)DeadzoneUtils.Apply(roll, Deadzone.Roll);

            // Step 3: Apply smoothing
            float effectiveSmoothing = SmoothingUtils.GetEffectiveSmoothing(SmoothingFactor, isRemoteConnection);

            if (!_hasSmoothedValue)
            {
                // First frame, snap to target
                _smoothedYaw = yaw;
                _smoothedPitch = pitch;
                _smoothedRoll = roll;
                _hasSmoothedValue = true;
            }
            else
            {
                // Calculate smoothing factor once for all axes (avoids 3x Math.Exp calls)
                float t = SmoothingUtils.CalculateSmoothingFactor(effectiveSmoothing, deltaTime);
                _smoothedYaw = _smoothedYaw + (yaw - _smoothedYaw) * t;
                _smoothedPitch = _smoothedPitch + (pitch - _smoothedPitch) * t;
                _smoothedRoll = _smoothedRoll + (roll - _smoothedRoll) * t;
            }

            // Step 4: Apply sensitivity
            return new TrackingPose((float)_smoothedYaw, (float)_smoothedPitch, (float)_smoothedRoll, rawPose.TimestampTicks)
                .ApplySensitivity(Sensitivity);
        }

        /// <summary>
        /// Sets the current smoothed pose as the center.
        /// </summary>
        public void Recenter()
        {
            _centerManager.SetCenter((float)_smoothedYaw, (float)_smoothedPitch, (float)_smoothedRoll);
        }

        /// <summary>
        /// Sets a specific pose as the center (for recentering without prior data).
        /// </summary>
        public void RecenterTo(TrackingPose pose)
        {
            _centerManager.SetCenter(pose);
            _smoothedYaw = 0;
            _smoothedPitch = 0;
            _smoothedRoll = 0;
        }

        /// <summary>
        /// Resets the processor state.
        /// </summary>
        public void Reset()
        {
            _centerManager.Reset();
            _smoothedYaw = 0;
            _smoothedPitch = 0;
            _smoothedRoll = 0;
            _hasSmoothedValue = false;
        }
    }
}

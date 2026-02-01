using System;

namespace HeadCannon.Core.Data
{
    /// <summary>
    /// Min/max rotation limits for each axis in degrees.
    /// </summary>
    public struct RotationLimits : IEquatable<RotationLimits>
    {
        public float YawMin { get; }
        public float YawMax { get; }
        public float PitchMin { get; }
        public float PitchMax { get; }
        public float RollMin { get; }
        public float RollMax { get; }

        /// <summary>Default limits: +/-75 yaw, +/-45 pitch, +/-30 roll.</summary>
        public static RotationLimits Default => new RotationLimits(-75f, 75f, -45f, 45f, -30f, 30f);

        /// <summary>No limits (effectively unlimited rotation).</summary>
        public static RotationLimits Unlimited => new RotationLimits(-180f, 180f, -90f, 90f, -180f, 180f);

        public RotationLimits(float yawMin, float yawMax, float pitchMin, float pitchMax, float rollMin, float rollMax)
        {
            YawMin = yawMin;
            YawMax = yawMax;
            PitchMin = pitchMin;
            PitchMax = pitchMax;
            RollMin = rollMin;
            RollMax = rollMax;
        }

        /// <summary>
        /// Creates symmetric limits (e.g., +/-45 degrees).
        /// </summary>
        public static RotationLimits Symmetric(float yaw, float pitch, float roll)
        {
            return new RotationLimits(-yaw, yaw, -pitch, pitch, -roll, roll);
        }

        /// <summary>
        /// Creates a copy with the specified yaw limits.
        /// </summary>
        public RotationLimits WithYaw(float min, float max)
        {
            return new RotationLimits(min, max, PitchMin, PitchMax, RollMin, RollMax);
        }

        /// <summary>
        /// Creates a copy with the specified pitch limits.
        /// </summary>
        public RotationLimits WithPitch(float min, float max)
        {
            return new RotationLimits(YawMin, YawMax, min, max, RollMin, RollMax);
        }

        /// <summary>
        /// Creates a copy with the specified roll limits.
        /// </summary>
        public RotationLimits WithRoll(float min, float max)
        {
            return new RotationLimits(YawMin, YawMax, PitchMin, PitchMax, min, max);
        }

        public bool Equals(RotationLimits other)
        {
            return YawMin == other.YawMin && YawMax == other.YawMax &&
                   PitchMin == other.PitchMin && PitchMax == other.PitchMax &&
                   RollMin == other.RollMin && RollMax == other.RollMax;
        }

        public override bool Equals(object obj)
        {
            if (obj is RotationLimits)
            {
                return Equals((RotationLimits)obj);
            }
            return false;
        }

        public override int GetHashCode()
        {
            unchecked
            {
                int hash = 17;
                hash = hash * 31 + YawMin.GetHashCode();
                hash = hash * 31 + YawMax.GetHashCode();
                hash = hash * 31 + PitchMin.GetHashCode();
                hash = hash * 31 + PitchMax.GetHashCode();
                hash = hash * 31 + RollMin.GetHashCode();
                hash = hash * 31 + RollMax.GetHashCode();
                return hash;
            }
        }

        public static bool operator ==(RotationLimits left, RotationLimits right) => left.Equals(right);
        public static bool operator !=(RotationLimits left, RotationLimits right) => !left.Equals(right);
    }
}

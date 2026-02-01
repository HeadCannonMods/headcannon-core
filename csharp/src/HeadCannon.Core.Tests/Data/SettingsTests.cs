using Xunit;
using HeadCannon.Core.Data;

namespace HeadCannon.Core.Tests.Data
{
    public class SettingsTests
    {
        #region SensitivitySettings Tests

        [Fact]
        public void SensitivitySettings_Default_HasCorrectValues()
        {
            SensitivitySettings s = SensitivitySettings.Default;
            Assert.Equal(1f, s.Yaw);
            Assert.Equal(1f, s.Pitch);
            Assert.Equal(1f, s.Roll);
            Assert.False(s.InvertYaw);
            Assert.False(s.InvertPitch);
            Assert.False(s.InvertRoll);
        }

        [Fact]
        public void SensitivitySettings_Uniform_SetsAllAxes()
        {
            SensitivitySettings s = SensitivitySettings.Uniform(2f);
            Assert.Equal(2f, s.Yaw);
            Assert.Equal(2f, s.Pitch);
            Assert.Equal(2f, s.Roll);
        }

        [Fact]
        public void SensitivitySettings_WithYaw_CreatesNewInstance()
        {
            SensitivitySettings s = SensitivitySettings.Default.WithYaw(2f);
            Assert.Equal(2f, s.Yaw);
            Assert.Equal(1f, s.Pitch);
            Assert.Equal(1f, s.Roll);
        }

        [Fact]
        public void SensitivitySettings_WithPitch_CreatesNewInstance()
        {
            SensitivitySettings s = SensitivitySettings.Default.WithPitch(2f);
            Assert.Equal(1f, s.Yaw);
            Assert.Equal(2f, s.Pitch);
            Assert.Equal(1f, s.Roll);
        }

        [Fact]
        public void SensitivitySettings_WithRoll_CreatesNewInstance()
        {
            SensitivitySettings s = SensitivitySettings.Default.WithRoll(2f);
            Assert.Equal(1f, s.Yaw);
            Assert.Equal(1f, s.Pitch);
            Assert.Equal(2f, s.Roll);
        }

        [Fact]
        public void SensitivitySettings_Equality_Works()
        {
            var a = new SensitivitySettings(1f, 2f, 3f, true, false, true);
            var b = new SensitivitySettings(1f, 2f, 3f, true, false, true);
            var c = new SensitivitySettings(1f, 2f, 3f, false, false, true);
            Assert.True(a == b);
            Assert.False(a == c);
            Assert.True(a != c);
        }

        [Fact]
        public void SensitivitySettings_GetHashCode_SameForEqual()
        {
            var a = new SensitivitySettings(1f, 2f, 3f, true, false, true);
            var b = new SensitivitySettings(1f, 2f, 3f, true, false, true);
            Assert.Equal(a.GetHashCode(), b.GetHashCode());
        }

        #endregion

        #region RotationLimits Tests

        [Fact]
        public void RotationLimits_Default_HasCorrectValues()
        {
            RotationLimits l = RotationLimits.Default;
            Assert.Equal(-75f, l.YawMin);
            Assert.Equal(75f, l.YawMax);
            Assert.Equal(-45f, l.PitchMin);
            Assert.Equal(45f, l.PitchMax);
            Assert.Equal(-30f, l.RollMin);
            Assert.Equal(30f, l.RollMax);
        }

        [Fact]
        public void RotationLimits_Unlimited_HasLargeValues()
        {
            RotationLimits l = RotationLimits.Unlimited;
            Assert.Equal(-180f, l.YawMin);
            Assert.Equal(180f, l.YawMax);
            Assert.Equal(-90f, l.PitchMin);
            Assert.Equal(90f, l.PitchMax);
            Assert.Equal(-180f, l.RollMin);
            Assert.Equal(180f, l.RollMax);
        }

        [Fact]
        public void RotationLimits_Symmetric_CreatesSymmetricLimits()
        {
            RotationLimits l = RotationLimits.Symmetric(45f, 30f, 15f);
            Assert.Equal(-45f, l.YawMin);
            Assert.Equal(45f, l.YawMax);
            Assert.Equal(-30f, l.PitchMin);
            Assert.Equal(30f, l.PitchMax);
            Assert.Equal(-15f, l.RollMin);
            Assert.Equal(15f, l.RollMax);
        }

        [Fact]
        public void RotationLimits_WithYaw_CreatesNewInstance()
        {
            RotationLimits l = RotationLimits.Default.WithYaw(-90f, 90f);
            Assert.Equal(-90f, l.YawMin);
            Assert.Equal(90f, l.YawMax);
            Assert.Equal(RotationLimits.Default.PitchMin, l.PitchMin);
        }

        [Fact]
        public void RotationLimits_WithPitch_CreatesNewInstance()
        {
            RotationLimits l = RotationLimits.Default.WithPitch(-60f, 60f);
            Assert.Equal(-60f, l.PitchMin);
            Assert.Equal(60f, l.PitchMax);
            Assert.Equal(RotationLimits.Default.YawMin, l.YawMin);
        }

        [Fact]
        public void RotationLimits_WithRoll_CreatesNewInstance()
        {
            RotationLimits l = RotationLimits.Default.WithRoll(-45f, 45f);
            Assert.Equal(-45f, l.RollMin);
            Assert.Equal(45f, l.RollMax);
            Assert.Equal(RotationLimits.Default.YawMin, l.YawMin);
        }

        [Fact]
        public void RotationLimits_Equality_Works()
        {
            var a = new RotationLimits(-45f, 45f, -30f, 30f, -15f, 15f);
            var b = new RotationLimits(-45f, 45f, -30f, 30f, -15f, 15f);
            var c = new RotationLimits(-45f, 45f, -30f, 30f, -15f, 20f);
            Assert.True(a == b);
            Assert.False(a == c);
        }

        #endregion

        #region DeadzoneSettings Tests

        [Fact]
        public void DeadzoneSettings_None_AllZero()
        {
            DeadzoneSettings d = DeadzoneSettings.None;
            Assert.Equal(0f, d.Yaw);
            Assert.Equal(0f, d.Pitch);
            Assert.Equal(0f, d.Roll);
        }

        [Fact]
        public void DeadzoneSettings_Default_HasValues()
        {
            DeadzoneSettings d = DeadzoneSettings.Default;
            Assert.Equal(0.5f, d.Yaw);
            Assert.Equal(0.5f, d.Pitch);
            Assert.Equal(0.5f, d.Roll);
        }

        [Fact]
        public void DeadzoneSettings_Uniform_SetsAllAxes()
        {
            DeadzoneSettings d = DeadzoneSettings.Uniform(2f);
            Assert.Equal(2f, d.Yaw);
            Assert.Equal(2f, d.Pitch);
            Assert.Equal(2f, d.Roll);
        }

        [Fact]
        public void DeadzoneSettings_Equality_Works()
        {
            var a = new DeadzoneSettings(1f, 2f, 3f);
            var b = new DeadzoneSettings(1f, 2f, 3f);
            var c = new DeadzoneSettings(1f, 2f, 4f);
            Assert.True(a == b);
            Assert.False(a == c);
        }

        [Fact]
        public void DeadzoneSettings_GetHashCode_SameForEqual()
        {
            var a = new DeadzoneSettings(1f, 2f, 3f);
            var b = new DeadzoneSettings(1f, 2f, 3f);
            Assert.Equal(a.GetHashCode(), b.GetHashCode());
        }

        #endregion
    }
}

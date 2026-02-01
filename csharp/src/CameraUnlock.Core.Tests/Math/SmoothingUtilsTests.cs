using Xunit;
using CameraUnlock.Core.Math;

namespace CameraUnlock.Core.Tests.Math
{
    public class SmoothingUtilsTests
    {
        private const float DeltaTime60Fps = 1f / 60f;

        [Fact]
        public void CalculateSmoothingFactor_ZeroSmoothing_ReturnsOne()
        {
            float result = SmoothingUtils.CalculateSmoothingFactor(0f, DeltaTime60Fps);
            Assert.Equal(1f, result);
        }

        [Fact]
        public void CalculateSmoothingFactor_VerySmallSmoothing_ReturnsOne()
        {
            float result = SmoothingUtils.CalculateSmoothingFactor(0.0001f, DeltaTime60Fps);
            Assert.Equal(1f, result);
        }

        [Fact]
        public void CalculateSmoothingFactor_NormalSmoothing_ReturnsBetweenZeroAndOne()
        {
            float result = SmoothingUtils.CalculateSmoothingFactor(0.5f, DeltaTime60Fps);
            Assert.InRange(result, 0f, 1f);
        }

        [Fact]
        public void CalculateSmoothingFactor_HighSmoothing_ReturnsLowerFactor()
        {
            float lowSmoothing = SmoothingUtils.CalculateSmoothingFactor(0.2f, DeltaTime60Fps);
            float highSmoothing = SmoothingUtils.CalculateSmoothingFactor(0.8f, DeltaTime60Fps);
            Assert.True(highSmoothing < lowSmoothing);
        }

        [Fact]
        public void CalculateSmoothingFactor_LargerDeltaTime_ReturnsHigherFactor()
        {
            float smallDelta = SmoothingUtils.CalculateSmoothingFactor(0.5f, 1f / 120f);
            float largeDelta = SmoothingUtils.CalculateSmoothingFactor(0.5f, 1f / 30f);
            Assert.True(largeDelta > smallDelta);
        }

        [Fact]
        public void Smooth_Float_MovesTowardsTarget()
        {
            float current = 0f;
            float target = 100f;
            float result = SmoothingUtils.Smooth(current, target, 0.5f, DeltaTime60Fps);
            Assert.True(result > 0f);
            Assert.True(result < 100f);
        }

        [Fact]
        public void Smooth_Double_MovesTowardsTarget()
        {
            double current = 0.0;
            double target = 100.0;
            double result = SmoothingUtils.Smooth(current, target, 0.5f, DeltaTime60Fps);
            Assert.True(result > 0.0);
            Assert.True(result < 100.0);
        }

        [Fact]
        public void Smooth_ZeroSmoothing_ReturnsTarget()
        {
            float current = 0f;
            float target = 100f;
            float result = SmoothingUtils.Smooth(current, target, 0f, DeltaTime60Fps);
            Assert.Equal(100f, result);
        }

        [Fact]
        public void GetEffectiveSmoothing_LocalConnection_ReturnsBaseSmoothing()
        {
            float result = SmoothingUtils.GetEffectiveSmoothing(0.05f, false);
            Assert.Equal(0.05f, result);
        }

        [Fact]
        public void GetEffectiveSmoothing_RemoteConnection_LowBase_ReturnsBaseline()
        {
            float result = SmoothingUtils.GetEffectiveSmoothing(0.05f, true);
            Assert.Equal(SmoothingUtils.RemoteConnectionBaseline, result);
        }

        [Fact]
        public void GetEffectiveSmoothing_RemoteConnection_HighBase_ReturnsBaseSmoothing()
        {
            float highSmoothing = 0.5f;
            float result = SmoothingUtils.GetEffectiveSmoothing(highSmoothing, true);
            Assert.Equal(highSmoothing, result);
        }
    }
}

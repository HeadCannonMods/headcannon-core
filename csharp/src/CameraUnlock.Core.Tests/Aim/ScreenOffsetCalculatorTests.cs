using System;
using Xunit;
using CameraUnlock.Core.Aim;

namespace CameraUnlock.Core.Tests.Aim
{
    public class ScreenOffsetCalculatorTests
    {
        private const float ScreenWidth = 1920f;
        private const float ScreenHeight = 1080f;
        private const float HorizontalFov = 90f;
        private const float VerticalFov = 50.625f;
        private const float CompensationScale = 1f;

        [Fact]
        public void Calculate_ZeroAngles_ReturnsZeroOffset()
        {
            ScreenOffsetCalculator.Calculate(
                0f, 0f, 0f,
                HorizontalFov, VerticalFov,
                ScreenWidth, ScreenHeight,
                CompensationScale,
                out float x, out float y);

            Assert.Equal(0f, x, precision: 5);
            Assert.Equal(0f, y, precision: 5);
        }

        [Fact]
        public void Calculate_YawRight_OffsetLeft()
        {
            ScreenOffsetCalculator.Calculate(
                10f, 0f, 0f,
                HorizontalFov, VerticalFov,
                ScreenWidth, ScreenHeight,
                CompensationScale,
                out float x, out float y);

            Assert.True(x < 0, "Yaw right should produce negative X offset (aim moves left)");
            Assert.Equal(0f, y, precision: 5);
        }

        [Fact]
        public void Calculate_YawLeft_OffsetRight()
        {
            ScreenOffsetCalculator.Calculate(
                -10f, 0f, 0f,
                HorizontalFov, VerticalFov,
                ScreenWidth, ScreenHeight,
                CompensationScale,
                out float x, out float y);

            Assert.True(x > 0, "Yaw left should produce positive X offset (aim moves right)");
            Assert.Equal(0f, y, precision: 5);
        }

        [Fact]
        public void Calculate_PitchUp_OffsetUp()
        {
            ScreenOffsetCalculator.Calculate(
                0f, 10f, 0f,
                HorizontalFov, VerticalFov,
                ScreenWidth, ScreenHeight,
                CompensationScale,
                out float x, out float y);

            Assert.Equal(0f, x, precision: 5);
            Assert.True(y > 0, "Pitch up should produce positive Y offset");
        }

        [Fact]
        public void Calculate_PitchDown_OffsetDown()
        {
            ScreenOffsetCalculator.Calculate(
                0f, -10f, 0f,
                HorizontalFov, VerticalFov,
                ScreenWidth, ScreenHeight,
                CompensationScale,
                out float x, out float y);

            Assert.Equal(0f, x, precision: 5);
            Assert.True(y < 0, "Pitch down should produce negative Y offset");
        }

        [Fact]
        public void Calculate_CompensationScale_ScalesOffset()
        {
            ScreenOffsetCalculator.Calculate(
                10f, 10f, 0f,
                HorizontalFov, VerticalFov,
                ScreenWidth, ScreenHeight,
                1f,
                out float x1, out float y1);

            ScreenOffsetCalculator.Calculate(
                10f, 10f, 0f,
                HorizontalFov, VerticalFov,
                ScreenWidth, ScreenHeight,
                2f,
                out float x2, out float y2);

            Assert.Equal(x1 * 2f, x2, precision: 3);
            Assert.Equal(y1 * 2f, y2, precision: 3);
        }

        [Fact]
        public void ApplyRollRotation_ZeroRoll_ReturnsUnchanged()
        {
            ScreenOffsetCalculator.ApplyRollRotation(100f, 50f, 0f, out float rx, out float ry);
            Assert.Equal(100f, rx, precision: 5);
            Assert.Equal(50f, ry, precision: 5);
        }

        [Fact]
        public void ApplyRollRotation_VerySmallRoll_ReturnsUnchanged()
        {
            ScreenOffsetCalculator.ApplyRollRotation(100f, 50f, 0.0001f, out float rx, out float ry);
            Assert.Equal(100f, rx, precision: 3);
            Assert.Equal(50f, ry, precision: 3);
        }

        [Fact]
        public void ApplyRollRotation_90Degrees_RotatesCorrectly()
        {
            ScreenOffsetCalculator.ApplyRollRotation(100f, 0f, 90f, out float rx, out float ry);
            Assert.Equal(0f, rx, precision: 2);
            Assert.True(System.Math.Abs(ry) > 90f);
        }

        [Fact]
        public void CalculateHorizontalFov_FromVertical_ReturnsCorrectValue()
        {
            float hFov = ScreenOffsetCalculator.CalculateHorizontalFov(60f, 16f / 9f);
            Assert.True(hFov > 60f, "Horizontal FOV should be larger than vertical for widescreen");
            Assert.True(hFov < 120f, "Horizontal FOV should be reasonable");
        }

        [Fact]
        public void CalculateHorizontalFov_SquareAspect_ReturnsSameAsVertical()
        {
            float hFov = ScreenOffsetCalculator.CalculateHorizontalFov(60f, 1f);
            Assert.Equal(60f, hFov, precision: 3);
        }

        [Fact]
        public void Calculate_SymmetricAngles_ProduceSymmetricOffsets()
        {
            ScreenOffsetCalculator.Calculate(
                10f, 10f, 0f,
                HorizontalFov, VerticalFov,
                ScreenWidth, ScreenHeight,
                CompensationScale,
                out float x1, out float y1);

            ScreenOffsetCalculator.Calculate(
                -10f, -10f, 0f,
                HorizontalFov, VerticalFov,
                ScreenWidth, ScreenHeight,
                CompensationScale,
                out float x2, out float y2);

            Assert.Equal(-x1, x2, precision: 3);
            Assert.Equal(-y1, y2, precision: 3);
        }
    }
}

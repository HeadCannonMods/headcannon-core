using Xunit;
using CameraUnlock.Core.Config;

namespace CameraUnlock.Core.Tests.Config
{
    public class ConfigParsingUtilsTests
    {
        [Fact]
        public void TryParseColor_Float01_ParsedAsIs()
        {
            Assert.True(ConfigParsingUtils.TryParseColor("1.0,0.5,0.25", out float[] rgba));
            Assert.Equal(1.0f, rgba[0], precision: 5);
            Assert.Equal(0.5f, rgba[1], precision: 5);
            Assert.Equal(0.25f, rgba[2], precision: 5);
            Assert.Equal(1.0f, rgba[3], precision: 5);
        }

        [Fact]
        public void TryParseColor_Byte0to255_ScaledTo01()
        {
            Assert.True(ConfigParsingUtils.TryParseColor("255,128,64", out float[] rgba));
            Assert.Equal(1.0f, rgba[0], precision: 3);
            Assert.Equal(128f / 255f, rgba[1], precision: 3);
            Assert.Equal(64f / 255f, rgba[2], precision: 3);
            Assert.Equal(1.0f, rgba[3], precision: 5);
        }

        [Fact]
        public void TryParseColor_WithAlpha_ParsesAllFour()
        {
            Assert.True(ConfigParsingUtils.TryParseColor("0.1,0.2,0.3,0.4", out float[] rgba));
            Assert.Equal(0.1f, rgba[0], precision: 5);
            Assert.Equal(0.4f, rgba[3], precision: 5);
        }

        [Fact]
        public void TryParseColor_InvalidAlpha_ReturnsFalse()
        {
            // Regression: previously silently defaulted alpha to 1.0 when the
            // alpha component was provided but unparseable, hiding config typos.
            Assert.False(ConfigParsingUtils.TryParseColor("1.0,0.5,0.25,abc", out _));
        }

        [Fact]
        public void TryParseColor_TooFewComponents_ReturnsFalse()
        {
            Assert.False(ConfigParsingUtils.TryParseColor("1.0,0.5", out _));
        }

        [Fact]
        public void TryParseColor_Empty_ReturnsFalse()
        {
            Assert.False(ConfigParsingUtils.TryParseColor("", out _));
            Assert.False(ConfigParsingUtils.TryParseColor((string)null!, out _));
        }

        [Fact]
        public void TryParseColor_MaxBasedDetection_TreatsLargestAsScaleHint()
        {
            // 200 makes it clear the user is in 0-255 mode; the small companions
            // get scaled the same way for consistency.
            Assert.True(ConfigParsingUtils.TryParseColor("0,0,200", out float[] rgba));
            Assert.Equal(0f, rgba[0], precision: 5);
            Assert.Equal(0f, rgba[1], precision: 5);
            Assert.Equal(200f / 255f, rgba[2], precision: 3);
        }

        [Fact]
        public void TryParseColor_ClampsNegativeIn01Mode()
        {
            // Pure 0-1 input with a negative component: 0-1 mode is preserved
            // (max is 1.0, not above), and the negative clamps to 0.
            Assert.True(ConfigParsingUtils.TryParseColor("-0.5,0.5,1.0", out float[] rgba));
            Assert.Equal(0f, rgba[0], precision: 5);
            Assert.Equal(0.5f, rgba[1], precision: 5);
            Assert.Equal(1.0f, rgba[2], precision: 5);
        }

        [Fact]
        public void TryParseColor_AlphaScaledIndependently()
        {
            // Regression: alpha=255 with 0-1 RGB used to scale RGB by 1/255 too.
            // Now RGB stays in 0-1 mode (max=1.0) and only alpha is divided by 255.
            Assert.True(ConfigParsingUtils.TryParseColor("1.0,0.5,0.25,255", out float[] rgba));
            Assert.Equal(1.0f, rgba[0], precision: 5);
            Assert.Equal(0.5f, rgba[1], precision: 5);
            Assert.Equal(0.25f, rgba[2], precision: 5);
            Assert.Equal(1.0f, rgba[3], precision: 5);
        }
    }
}

using System;
using UnityEngine;

namespace CameraUnlock.Core.Unity.Rendering
{
    /// <summary>
    /// Delegate that provides the screen position for the reticle.
    /// </summary>
    /// <param name="screenX">Output: X position in screen pixels (0 = left).</param>
    /// <param name="screenY">Output: Y position in screen pixels (0 = bottom in Unity coords).</param>
    /// <returns>True if the reticle should be drawn, false to hide it.</returns>
    public delegate bool ReticlePositionProvider(out float screenX, out float screenY);

    /// <summary>
    /// Style of reticle to render.
    /// </summary>
    public enum ReticleStyle
    {
        /// <summary>Solid filled circle (dot).</summary>
        Dot,
        /// <summary>Circle outline (ring).</summary>
        Circle
    }

    /// <summary>
    /// Renders a reticle using Unity's IMGUI system.
    /// Supports dot (filled circle) and circle (outline) styles.
    /// Attach to a GameObject and call Initialize() to set up.
    /// Based on proven pattern from gone-home and green-hell mods.
    /// </summary>
    public sealed class IMGUIReticle : MonoBehaviour
    {
        private ReticlePositionProvider _positionProvider;
        private Texture2D _reticleTexture;

        // Configuration
        private const int ReferenceHeight = 1080;
        private int _baseSizeAt1080p = 4;
        private int _thicknessAt1080p = 2;
        private int _currentTextureSize;
        private int _currentThickness;
        private int _lastScreenHeight;
        private Color _reticleColor = Color.white;
        private bool _isVisible = true;
        private ReticleStyle _style = ReticleStyle.Dot;

        /// <summary>
        /// Gets or sets the base size of the reticle at 1080p resolution.
        /// Actual size scales with screen height.
        /// </summary>
        public int BaseSizeAt1080p
        {
            get => _baseSizeAt1080p;
            set
            {
                if (value != _baseSizeAt1080p)
                {
                    _baseSizeAt1080p = Mathf.Max(1, value);
                    RecreateTexture();
                }
            }
        }

        /// <summary>
        /// Gets or sets the reticle color.
        /// </summary>
        public Color ReticleColor
        {
            get => _reticleColor;
            set
            {
                if (value != _reticleColor)
                {
                    _reticleColor = value;
                    RecreateTexture();
                }
            }
        }

        /// <summary>
        /// Gets or sets whether the reticle is visible.
        /// </summary>
        public bool IsVisible
        {
            get => _isVisible;
            set => _isVisible = value;
        }

        /// <summary>
        /// Gets or sets the reticle style (Dot or Circle).
        /// </summary>
        public ReticleStyle Style
        {
            get => _style;
            set
            {
                if (value != _style)
                {
                    _style = value;
                    RecreateTexture();
                }
            }
        }

        /// <summary>
        /// Gets or sets the thickness of the circle outline at 1080p (only used for Circle style).
        /// Default is 2 pixels. Actual thickness scales with screen height.
        /// </summary>
        public int ThicknessAt1080p
        {
            get => _thicknessAt1080p;
            set
            {
                if (value != _thicknessAt1080p)
                {
                    _thicknessAt1080p = Mathf.Max(1, value);
                    RecreateTexture();
                }
            }
        }

        /// <summary>
        /// Initializes the reticle with a position provider.
        /// </summary>
        /// <param name="positionProvider">
        /// Delegate that provides screen position each frame.
        /// Return false to hide the reticle temporarily.
        /// </param>
        public void Initialize(ReticlePositionProvider positionProvider)
        {
            _positionProvider = positionProvider;
            UpdateTextureForResolution();
        }

        /// <summary>
        /// Initializes the reticle with fixed screen offset from center.
        /// </summary>
        /// <param name="getOffset">
        /// Delegate that returns (offsetX, offsetY) from screen center.
        /// </param>
        /// <param name="shouldDraw">
        /// Optional delegate that returns whether to draw the reticle.
        /// </param>
        public void InitializeWithOffset(Func<Vector2> getOffset, Func<bool> shouldDraw = null)
        {
            _positionProvider = (out float x, out float y) =>
            {
                if (shouldDraw != null && !shouldDraw())
                {
                    x = 0;
                    y = 0;
                    return false;
                }
                Vector2 offset = getOffset();
                x = Screen.width * 0.5f + offset.x;
                y = Screen.height * 0.5f + offset.y;
                return true;
            };
            UpdateTextureForResolution();
        }

        private int ComputeScaledSize()
        {
            float scale = (float)Screen.height / ReferenceHeight;
            int size = Mathf.RoundToInt(_baseSizeAt1080p * scale);
            return Mathf.Max(2, size);
        }

        private int ComputeScaledThickness()
        {
            float scale = (float)Screen.height / ReferenceHeight;
            int thickness = Mathf.RoundToInt(_thicknessAt1080p * scale);
            return Mathf.Max(1, thickness);
        }

        private void UpdateTextureForResolution()
        {
            int scaledSize = ComputeScaledSize();
            int scaledThickness = ComputeScaledThickness();
            if (_reticleTexture == null || scaledSize != _currentTextureSize ||
                scaledThickness != _currentThickness || Screen.height != _lastScreenHeight)
            {
                _currentTextureSize = scaledSize;
                _currentThickness = scaledThickness;
                _lastScreenHeight = Screen.height;
                RecreateTexture();
            }
        }

        private void RecreateTexture()
        {
            if (_reticleTexture != null)
            {
                Destroy(_reticleTexture);
                _reticleTexture = null;
            }

            int size = _currentTextureSize;
            if (size < 2) size = 2;

            _reticleTexture = new Texture2D(size, size, TextureFormat.ARGB32, false);
            _reticleTexture.filterMode = FilterMode.Bilinear;

            Color transparent = new Color(0, 0, 0, 0);
            Color[] pixels = new Color[size * size];

            for (int i = 0; i < pixels.Length; i++)
            {
                pixels[i] = transparent;
            }

            float center = (size - 1) * 0.5f;
            float radius = size * 0.5f;

            if (_style == ReticleStyle.Dot)
            {
                // Draw filled circle with anti-aliased edge
                for (int y = 0; y < size; y++)
                {
                    for (int x = 0; x < size; x++)
                    {
                        float dx = x - center;
                        float dy = y - center;
                        float dist = Mathf.Sqrt(dx * dx + dy * dy);

                        if (dist <= radius)
                        {
                            float alpha = Mathf.Clamp01(radius - dist + 0.5f);
                            Color pixelColor = _reticleColor;
                            pixelColor.a *= alpha;
                            pixels[y * size + x] = pixelColor;
                        }
                    }
                }
            }
            else // Circle style
            {
                // Draw circle outline with anti-aliased edges
                float thickness = _currentThickness;
                float innerRadius = radius - thickness;
                if (innerRadius < 0) innerRadius = 0;

                for (int y = 0; y < size; y++)
                {
                    for (int x = 0; x < size; x++)
                    {
                        float dx = x - center;
                        float dy = y - center;
                        float dist = Mathf.Sqrt(dx * dx + dy * dy);

                        // Anti-aliased ring: fade in at inner edge, fade out at outer edge
                        float outerAlpha = Mathf.Clamp01(radius - dist + 0.5f);
                        float innerAlpha = Mathf.Clamp01(dist - innerRadius + 0.5f);
                        float alpha = outerAlpha * innerAlpha;

                        if (alpha > 0)
                        {
                            Color pixelColor = _reticleColor;
                            pixelColor.a *= alpha;
                            pixels[y * size + x] = pixelColor;
                        }
                    }
                }
            }

            _reticleTexture.SetPixels(pixels);
            _reticleTexture.Apply();
        }

        private void OnGUI()
        {
            if (!_isVisible || _positionProvider == null) return;

            // Check for resolution change
            UpdateTextureForResolution();

            if (_reticleTexture == null) return;

            // Get position from provider
            if (!_positionProvider(out float screenX, out float screenY))
            {
                return; // Provider says don't draw
            }

            // Convert to GUI coordinates (GUI Y is inverted - origin at top-left)
            float guiY = Screen.height - screenY;

            int size = _currentTextureSize;
            Rect reticleRect = new Rect(
                screenX - size * 0.5f,
                guiY - size * 0.5f,
                size,
                size
            );

            GUI.DrawTexture(reticleRect, _reticleTexture);
        }

        private void OnDestroy()
        {
            if (_reticleTexture != null)
            {
                Destroy(_reticleTexture);
                _reticleTexture = null;
            }
        }
    }
}

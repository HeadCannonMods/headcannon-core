using System;
using UnityEngine;

namespace CameraUnlock.Core.Unity.Tracking
{
    /// <summary>
    /// Manages static camera callbacks that need to survive GameObject destruction.
    /// In BepInEx plugins and similar modding scenarios, the plugin's GameObject may be
    /// destroyed during scene transitions, but the camera callbacks need to persist.
    ///
    /// This class stores callbacks as static fields and manages their registration/unregistration
    /// with the Unity Camera and Canvas systems.
    ///
    /// Usage:
    /// 1. Create instance in plugin Awake
    /// 2. Register callbacks using RegisterPreCull, RegisterPreRender, RegisterWillRenderCanvases
    /// 3. Dispose on application quit (NOT on GameObject destroy)
    /// </summary>
    public sealed class CameraCallbackLifecycle : IDisposable
    {
        // Static storage to survive GameObject destruction
        private static Camera.CameraCallback _staticPreCullCallback;
        private static Camera.CameraCallback _staticPreRenderCallback;
        private static Action _staticWillRenderCanvasesCallback;

        // Track whether we own the current static callbacks
        private bool _ownsPreCull;
        private bool _ownsPreRender;
        private bool _ownsWillRenderCanvases;
        private bool _disposed;

        /// <summary>
        /// Returns true if this instance owns the preCull callback.
        /// </summary>
        public bool HasPreCull => _ownsPreCull && !_disposed;

        /// <summary>
        /// Returns true if this instance owns the preRender callback.
        /// </summary>
        public bool HasPreRender => _ownsPreRender && !_disposed;

        /// <summary>
        /// Returns true if this instance owns the willRenderCanvases callback.
        /// </summary>
        public bool HasWillRenderCanvases => _ownsWillRenderCanvases && !_disposed;

        /// <summary>
        /// Returns true if this instance has been disposed.
        /// </summary>
        public bool IsDisposed => _disposed;

        /// <summary>
        /// Registers a callback that will be called before each camera culls the scene.
        /// The callback receives the Camera that is about to cull.
        /// This is the ideal place to modify view matrices for head tracking.
        /// </summary>
        /// <param name="callback">The callback to register.</param>
        /// <exception cref="ArgumentNullException">Thrown when callback is null.</exception>
        /// <exception cref="ObjectDisposedException">Thrown when this instance is disposed.</exception>
        /// <exception cref="InvalidOperationException">Thrown when a preCull callback is already registered.</exception>
        public void RegisterPreCull(Camera.CameraCallback callback)
        {
            if (_disposed)
            {
                throw new ObjectDisposedException(nameof(CameraCallbackLifecycle));
            }

            if (callback == null)
            {
                throw new ArgumentNullException(nameof(callback));
            }

            if (_staticPreCullCallback != null)
            {
                throw new InvalidOperationException("A preCull callback is already registered. Call UnregisterPreCull first or Dispose the previous lifecycle.");
            }

            _staticPreCullCallback = callback;
            Camera.onPreCull += _staticPreCullCallback;
            _ownsPreCull = true;
        }

        /// <summary>
        /// Unregisters the preCull callback if this instance owns it.
        /// </summary>
        public void UnregisterPreCull()
        {
            if (_ownsPreCull && _staticPreCullCallback != null)
            {
                Camera.onPreCull -= _staticPreCullCallback;
                _staticPreCullCallback = null;
                _ownsPreCull = false;
            }
        }

        /// <summary>
        /// Registers a callback that will be called before each camera renders.
        /// The callback receives the Camera that is about to render.
        /// This is called after culling and is useful for final adjustments.
        /// </summary>
        /// <param name="callback">The callback to register.</param>
        /// <exception cref="ArgumentNullException">Thrown when callback is null.</exception>
        /// <exception cref="ObjectDisposedException">Thrown when this instance is disposed.</exception>
        /// <exception cref="InvalidOperationException">Thrown when a preRender callback is already registered.</exception>
        public void RegisterPreRender(Camera.CameraCallback callback)
        {
            if (_disposed)
            {
                throw new ObjectDisposedException(nameof(CameraCallbackLifecycle));
            }

            if (callback == null)
            {
                throw new ArgumentNullException(nameof(callback));
            }

            if (_staticPreRenderCallback != null)
            {
                throw new InvalidOperationException("A preRender callback is already registered. Call UnregisterPreRender first or Dispose the previous lifecycle.");
            }

            _staticPreRenderCallback = callback;
            Camera.onPreRender += _staticPreRenderCallback;
            _ownsPreRender = true;
        }

        /// <summary>
        /// Unregisters the preRender callback if this instance owns it.
        /// </summary>
        public void UnregisterPreRender()
        {
            if (_ownsPreRender && _staticPreRenderCallback != null)
            {
                Camera.onPreRender -= _staticPreRenderCallback;
                _staticPreRenderCallback = null;
                _ownsPreRender = false;
            }
        }

        /// <summary>
        /// Registers a callback that will be called right before canvases are rendered.
        /// This is the last opportunity to modify UI element positions.
        /// </summary>
        /// <param name="callback">The callback to register.</param>
        /// <exception cref="ArgumentNullException">Thrown when callback is null.</exception>
        /// <exception cref="ObjectDisposedException">Thrown when this instance is disposed.</exception>
        /// <exception cref="InvalidOperationException">Thrown when a willRenderCanvases callback is already registered.</exception>
        public void RegisterWillRenderCanvases(Action callback)
        {
            if (_disposed)
            {
                throw new ObjectDisposedException(nameof(CameraCallbackLifecycle));
            }

            if (callback == null)
            {
                throw new ArgumentNullException(nameof(callback));
            }

            if (_staticWillRenderCanvasesCallback != null)
            {
                throw new InvalidOperationException("A willRenderCanvases callback is already registered. Call UnregisterWillRenderCanvases first or Dispose the previous lifecycle.");
            }

            _staticWillRenderCanvasesCallback = callback;
            Canvas.willRenderCanvases += OnWillRenderCanvasesWrapper;
            _ownsWillRenderCanvases = true;
        }

        /// <summary>
        /// Unregisters the willRenderCanvases callback if this instance owns it.
        /// </summary>
        public void UnregisterWillRenderCanvases()
        {
            if (_ownsWillRenderCanvases && _staticWillRenderCanvasesCallback != null)
            {
                Canvas.willRenderCanvases -= OnWillRenderCanvasesWrapper;
                _staticWillRenderCanvasesCallback = null;
                _ownsWillRenderCanvases = false;
            }
        }

        /// <summary>
        /// Wrapper for willRenderCanvases that calls our static callback.
        /// This is needed because Canvas.willRenderCanvases uses Canvas.WillRenderCanvases delegate
        /// which has a different signature than System.Action.
        /// </summary>
        private static void OnWillRenderCanvasesWrapper()
        {
            _staticWillRenderCanvasesCallback?.Invoke();
        }

        /// <summary>
        /// Disposes of this lifecycle, unregistering all callbacks.
        /// Call this on application quit, NOT on GameObject destroy.
        /// </summary>
        public void Dispose()
        {
            if (_disposed)
            {
                return;
            }

            UnregisterPreCull();
            UnregisterPreRender();
            UnregisterWillRenderCanvases();

            _disposed = true;
        }

        /// <summary>
        /// Forces cleanup of all static callbacks regardless of ownership.
        /// Use this only in emergency situations (e.g., unhandled exceptions during shutdown).
        /// </summary>
        public static void ForceCleanupAll()
        {
            if (_staticPreCullCallback != null)
            {
                Camera.onPreCull -= _staticPreCullCallback;
                _staticPreCullCallback = null;
            }

            if (_staticPreRenderCallback != null)
            {
                Camera.onPreRender -= _staticPreRenderCallback;
                _staticPreRenderCallback = null;
            }

            if (_staticWillRenderCanvasesCallback != null)
            {
                Canvas.willRenderCanvases -= OnWillRenderCanvasesWrapper;
                _staticWillRenderCanvasesCallback = null;
            }
        }
    }
}

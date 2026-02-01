using CameraUnlock.Core.Data;

namespace CameraUnlock.Core.Protocol
{
    /// <summary>
    /// Interface for tracking data sources.
    /// </summary>
    public interface ITrackingDataSource
    {
        /// <summary>
        /// True if data has been received within the connection timeout.
        /// </summary>
        bool IsReceiving { get; }

        /// <summary>
        /// True if the data source is from a remote (non-localhost) address.
        /// Used to apply baseline smoothing for network latency compensation.
        /// </summary>
        bool IsRemoteConnection { get; }

        /// <summary>
        /// True if the data source failed to initialize.
        /// </summary>
        bool IsFailed { get; }

        /// <summary>
        /// Gets the latest tracking pose with sensitivity and offset applied.
        /// </summary>
        TrackingPose GetLatestPose();

        /// <summary>
        /// Gets the raw rotation values without sensitivity or offset applied.
        /// </summary>
        /// <param name="yaw">Output: raw yaw value.</param>
        /// <param name="pitch">Output: raw pitch value.</param>
        /// <param name="roll">Output: raw roll value.</param>
        void GetRawRotation(out float yaw, out float pitch, out float roll);

        /// <summary>
        /// Sets the current position as the new center point.
        /// </summary>
        void Recenter();
    }
}

using System;

namespace HeadCannon.Core.Config
{
    /// <summary>
    /// Interface for configs that support change notification.
    /// Useful for hot-reloadable configurations (e.g., BepInEx ConfigurationManager).
    /// </summary>
    public interface IConfigChangeNotifier
    {
        /// <summary>
        /// Raised when any configuration value changes.
        /// </summary>
        event EventHandler ConfigChanged;
    }

    /// <summary>
    /// Interface combining config reading with change notification.
    /// </summary>
    public interface INotifyingHeadTrackingConfig : IHeadTrackingConfig, IConfigChangeNotifier
    {
    }
}

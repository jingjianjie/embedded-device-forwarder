namespace DeviceForwarder.Daemon.Configuration;

/// <summary>
/// Represents the TCP host link configuration.
/// </summary>
public sealed class HostLinkConfig
{
    /// <summary>
    /// IP address to bind to (default: 0.0.0.0 for all interfaces).
    /// </summary>
    public string BindAddress { get; set; } = "0.0.0.0";

    /// <summary>
    /// TCP port to listen on (default: 5000).
    /// </summary>
    public int Port { get; set; } = 5000;

    /// <summary>
    /// Maximum number of concurrent client connections (default: 10).
    /// </summary>
    public int MaxConnections { get; set; } = 10;

    /// <summary>
    /// Connection timeout in seconds (default: 30).
    /// </summary>
    public int ConnectionTimeoutSeconds { get; set; } = 30;

    /// <summary>
    /// Heartbeat interval in seconds (default: 10, 0 to disable).
    /// </summary>
    public int HeartbeatIntervalSeconds { get; set; } = 10;
}

namespace DeviceForwarder.Daemon.Configuration;

/// <summary>
/// Root configuration for the device forwarder daemon.
/// </summary>
public sealed class ForwarderConfig
{
    /// <summary>
    /// TCP host link configuration.
    /// </summary>
    public HostLinkConfig HostLink { get; set; } = new();

    /// <summary>
    /// List of configured serial channels.
    /// </summary>
    public List<ChannelConfig> Channels { get; set; } = new();

    /// <summary>
    /// Log level (Trace, Debug, Information, Warning, Error, Critical).
    /// </summary>
    public string LogLevel { get; set; } = "Information";
}

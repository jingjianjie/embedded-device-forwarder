namespace DeviceForwarder.Daemon.Configuration;

/// <summary>
/// Represents the host link configuration (TCP or USB Serial).
/// </summary>
public sealed class HostLinkConfig
{
    /// <summary>
    /// Link mode: "tcp" for TCP server, "usb" for USB virtual serial port (default: "tcp").
    /// </summary>
    public string Mode { get; set; } = "tcp";

    /// <summary>
    /// IP address to bind to (default: 0.0.0.0 for all interfaces). Used in TCP mode.
    /// </summary>
    public string BindAddress { get; set; } = "0.0.0.0";

    /// <summary>
    /// TCP port to listen on (default: 5000). Used in TCP mode.
    /// </summary>
    public int Port { get; set; } = 5000;

    /// <summary>
    /// Maximum number of concurrent client connections (default: 10). Used in TCP mode.
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

    /// <summary>
    /// USB serial device path (default: /dev/ttyGS0). Used in USB mode.
    /// This is typically a USB gadget CDC ACM device.
    /// </summary>
    public string? UsbSerialDevice { get; set; } = "/dev/ttyGS0";
}

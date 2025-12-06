using System.IO.Ports;

namespace DeviceForwarder.Daemon.Configuration;

/// <summary>
/// Represents the configuration for a serial channel.
/// </summary>
public sealed class ChannelConfig
{
    /// <summary>
    /// Unique identifier for the channel (0-255).
    /// </summary>
    public byte Id { get; set; }

    /// <summary>
    /// Path to the serial device (e.g., /dev/ttyUSB0, /dev/ttyAMA0).
    /// </summary>
    public string DevicePath { get; set; } = string.Empty;

    /// <summary>
    /// Baud rate for serial communication.
    /// </summary>
    public int Baudrate { get; set; } = 9600;

    /// <summary>
    /// Number of data bits (default: 8).
    /// </summary>
    public int DataBits { get; set; } = 8;

    /// <summary>
    /// Parity mode (default: None).
    /// </summary>
    public string Parity { get; set; } = "None";

    /// <summary>
    /// Number of stop bits (default: 1).
    /// </summary>
    public string StopBits { get; set; } = "One";

    /// <summary>
    /// Optional name/description for the channel.
    /// </summary>
    public string? Name { get; set; }

    /// <summary>
    /// Whether the channel is enabled (default: true).
    /// </summary>
    public bool Enabled { get; set; } = true;

    /// <summary>
    /// Parses the Parity string to System.IO.Ports.Parity enum.
    /// </summary>
    public Parity GetParity()
    {
        return Parity?.ToLowerInvariant() switch
        {
            "none" => System.IO.Ports.Parity.None,
            "odd" => System.IO.Ports.Parity.Odd,
            "even" => System.IO.Ports.Parity.Even,
            "mark" => System.IO.Ports.Parity.Mark,
            "space" => System.IO.Ports.Parity.Space,
            _ => System.IO.Ports.Parity.None
        };
    }

    /// <summary>
    /// Parses the StopBits string to System.IO.Ports.StopBits enum.
    /// </summary>
    public StopBits GetStopBits()
    {
        return StopBits?.ToLowerInvariant() switch
        {
            "none" => System.IO.Ports.StopBits.None,
            "one" or "1" => System.IO.Ports.StopBits.One,
            "two" or "2" => System.IO.Ports.StopBits.Two,
            "onepointfive" or "1.5" => System.IO.Ports.StopBits.OnePointFive,
            _ => System.IO.Ports.StopBits.One
        };
    }
}

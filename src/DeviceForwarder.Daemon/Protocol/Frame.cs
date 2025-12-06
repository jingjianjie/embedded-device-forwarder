namespace DeviceForwarder.Daemon.Protocol;

/// <summary>
/// Represents a binary frame used for communication between the host and the daemon.
/// Frame format: [Magic(2)] [Version(1)] [MsgType(1)] [ChannelId(1)] [PayloadLen(2)] [Payload(N)]
/// </summary>
public sealed class Frame
{
    /// <summary>
    /// Magic bytes to identify valid frames (0xAA55).
    /// </summary>
    public const ushort Magic = 0xAA55;

    /// <summary>
    /// Current protocol version.
    /// </summary>
    public const byte CurrentVersion = 1;

    /// <summary>
    /// Header size in bytes: Magic(2) + Version(1) + MsgType(1) + ChannelId(1) + PayloadLen(2) = 7.
    /// </summary>
    public const int HeaderSize = 7;

    /// <summary>
    /// Maximum payload size (64KB - 1).
    /// </summary>
    public const int MaxPayloadSize = 65535;

    /// <summary>
    /// Protocol version of the frame.
    /// </summary>
    public byte Version { get; set; } = CurrentVersion;

    /// <summary>
    /// Message type indicating the frame's purpose.
    /// </summary>
    public MessageType MsgType { get; set; }

    /// <summary>
    /// Channel identifier for routing to the correct serial port.
    /// </summary>
    public byte ChannelId { get; set; }

    /// <summary>
    /// Payload data to be forwarded.
    /// </summary>
    public byte[] Payload { get; set; } = Array.Empty<byte>();
}

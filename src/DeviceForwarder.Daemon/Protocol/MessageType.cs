namespace DeviceForwarder.Daemon.Protocol;

/// <summary>
/// Defines the message types used in the binary framing protocol.
/// </summary>
public enum MessageType : byte
{
    /// <summary>
    /// Data frame containing payload to forward to/from a serial port.
    /// </summary>
    Data = 0x01,

    /// <summary>
    /// Acknowledgment frame indicating successful receipt.
    /// </summary>
    Ack = 0x02,

    /// <summary>
    /// Error frame indicating a problem with the channel or data.
    /// </summary>
    Error = 0x03,

    /// <summary>
    /// Heartbeat frame to maintain connection liveness.
    /// </summary>
    Heartbeat = 0x04,

    /// <summary>
    /// Configuration query to get channel information.
    /// </summary>
    ConfigQuery = 0x05,

    /// <summary>
    /// Configuration response with channel information.
    /// </summary>
    ConfigResponse = 0x06
}

using System.Buffers;
using System.Buffers.Binary;

namespace DeviceForwarder.Daemon.Protocol;

/// <summary>
/// Provides encoding and decoding functionality for the binary frame protocol.
/// </summary>
public static class FrameCodec
{
    /// <summary>
    /// Encodes a frame into a byte array.
    /// </summary>
    /// <param name="frame">The frame to encode.</param>
    /// <returns>The encoded byte array.</returns>
    /// <exception cref="ArgumentException">Thrown when payload exceeds maximum size.</exception>
    public static byte[] Encode(Frame frame)
    {
        ArgumentNullException.ThrowIfNull(frame);

        if (frame.Payload.Length > Frame.MaxPayloadSize)
        {
            throw new ArgumentException($"Payload size {frame.Payload.Length} exceeds maximum {Frame.MaxPayloadSize}");
        }

        var buffer = new byte[Frame.HeaderSize + frame.Payload.Length];
        var span = buffer.AsSpan();

        // Write Magic (big-endian)
        BinaryPrimitives.WriteUInt16BigEndian(span[0..2], Frame.Magic);

        // Write Version
        span[2] = frame.Version;

        // Write MsgType
        span[3] = (byte)frame.MsgType;

        // Write ChannelId
        span[4] = frame.ChannelId;

        // Write PayloadLen (big-endian)
        BinaryPrimitives.WriteUInt16BigEndian(span[5..7], (ushort)frame.Payload.Length);

        // Write Payload
        if (frame.Payload.Length > 0)
        {
            frame.Payload.CopyTo(span[Frame.HeaderSize..]);
        }

        return buffer;
    }

    /// <summary>
    /// Tries to decode a frame from a buffer.
    /// </summary>
    /// <param name="buffer">The buffer to decode from.</param>
    /// <param name="frame">The decoded frame if successful.</param>
    /// <param name="bytesConsumed">The number of bytes consumed from the buffer.</param>
    /// <returns>True if a complete frame was decoded; otherwise, false.</returns>
    public static bool TryDecode(ReadOnlySpan<byte> buffer, out Frame? frame, out int bytesConsumed)
    {
        frame = null;
        bytesConsumed = 0;

        // Check if we have enough bytes for the header
        if (buffer.Length < Frame.HeaderSize)
        {
            return false;
        }

        // Check Magic
        var magic = BinaryPrimitives.ReadUInt16BigEndian(buffer[0..2]);
        if (magic != Frame.Magic)
        {
            // Invalid magic - consume one byte to allow resync
            bytesConsumed = 1;
            return false;
        }

        // Read PayloadLen
        var payloadLen = BinaryPrimitives.ReadUInt16BigEndian(buffer[5..7]);

        // Check if we have the complete frame
        var totalFrameSize = Frame.HeaderSize + payloadLen;
        if (buffer.Length < totalFrameSize)
        {
            return false;
        }

        // Decode the frame
        frame = new Frame
        {
            Version = buffer[2],
            MsgType = (MessageType)buffer[3],
            ChannelId = buffer[4],
            Payload = buffer.Slice(Frame.HeaderSize, payloadLen).ToArray()
        };

        bytesConsumed = totalFrameSize;
        return true;
    }

    /// <summary>
    /// Creates a data frame for the specified channel.
    /// </summary>
    /// <param name="channelId">The channel identifier.</param>
    /// <param name="data">The payload data.</param>
    /// <returns>A new data frame.</returns>
    public static Frame CreateDataFrame(byte channelId, byte[] data)
    {
        return new Frame
        {
            Version = Frame.CurrentVersion,
            MsgType = MessageType.Data,
            ChannelId = channelId,
            Payload = data
        };
    }

    /// <summary>
    /// Creates an error frame for the specified channel.
    /// </summary>
    /// <param name="channelId">The channel identifier.</param>
    /// <param name="errorMessage">The error message.</param>
    /// <returns>A new error frame.</returns>
    public static Frame CreateErrorFrame(byte channelId, string errorMessage)
    {
        return new Frame
        {
            Version = Frame.CurrentVersion,
            MsgType = MessageType.Error,
            ChannelId = channelId,
            Payload = System.Text.Encoding.UTF8.GetBytes(errorMessage)
        };
    }

    /// <summary>
    /// Creates a heartbeat frame.
    /// </summary>
    /// <returns>A new heartbeat frame.</returns>
    public static Frame CreateHeartbeatFrame()
    {
        return new Frame
        {
            Version = Frame.CurrentVersion,
            MsgType = MessageType.Heartbeat,
            ChannelId = 0,
            Payload = Array.Empty<byte>()
        };
    }
}

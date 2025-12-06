using DeviceForwarder.Daemon.Protocol;

namespace DeviceForwarder.Tests;

public class FrameCodecTests
{
    [Fact]
    public void Encode_DataFrame_ReturnsValidBytes()
    {
        // Arrange
        var frame = new Frame
        {
            Version = Frame.CurrentVersion,
            MsgType = MessageType.Data,
            ChannelId = 5,
            Payload = new byte[] { 0x01, 0x02, 0x03, 0x04 }
        };

        // Act
        var encoded = FrameCodec.Encode(frame);

        // Assert
        Assert.Equal(Frame.HeaderSize + 4, encoded.Length);
        
        // Check Magic (big-endian 0xAA55)
        Assert.Equal(0xAA, encoded[0]);
        Assert.Equal(0x55, encoded[1]);
        
        // Check Version
        Assert.Equal(Frame.CurrentVersion, encoded[2]);
        
        // Check MsgType
        Assert.Equal((byte)MessageType.Data, encoded[3]);
        
        // Check ChannelId
        Assert.Equal(5, encoded[4]);
        
        // Check PayloadLen (big-endian)
        Assert.Equal(0x00, encoded[5]);
        Assert.Equal(0x04, encoded[6]);
        
        // Check Payload
        Assert.Equal(0x01, encoded[7]);
        Assert.Equal(0x02, encoded[8]);
        Assert.Equal(0x03, encoded[9]);
        Assert.Equal(0x04, encoded[10]);
    }

    [Fact]
    public void Encode_EmptyPayload_ReturnsHeaderOnly()
    {
        // Arrange
        var frame = new Frame
        {
            Version = Frame.CurrentVersion,
            MsgType = MessageType.Heartbeat,
            ChannelId = 0,
            Payload = Array.Empty<byte>()
        };

        // Act
        var encoded = FrameCodec.Encode(frame);

        // Assert
        Assert.Equal(Frame.HeaderSize, encoded.Length);
        Assert.Equal(0x00, encoded[5]); // PayloadLen high byte
        Assert.Equal(0x00, encoded[6]); // PayloadLen low byte
    }

    [Fact]
    public void Encode_PayloadExceedsMaxSize_ThrowsArgumentException()
    {
        // Arrange
        var frame = new Frame
        {
            Version = Frame.CurrentVersion,
            MsgType = MessageType.Data,
            ChannelId = 1,
            Payload = new byte[Frame.MaxPayloadSize + 1]
        };

        // Act & Assert
        var ex = Assert.Throws<ArgumentException>(() => FrameCodec.Encode(frame));
        Assert.Contains("Payload size", ex.Message);
    }

    [Fact]
    public void Encode_NullFrame_ThrowsArgumentNullException()
    {
        // Act & Assert
        Assert.Throws<ArgumentNullException>(() => FrameCodec.Encode(null!));
    }

    [Fact]
    public void TryDecode_ValidFrame_ReturnsTrue()
    {
        // Arrange
        var originalFrame = new Frame
        {
            Version = Frame.CurrentVersion,
            MsgType = MessageType.Data,
            ChannelId = 10,
            Payload = new byte[] { 0xDE, 0xAD, 0xBE, 0xEF }
        };
        var encoded = FrameCodec.Encode(originalFrame);

        // Act
        var result = FrameCodec.TryDecode(encoded, out var decodedFrame, out var bytesConsumed);

        // Assert
        Assert.True(result);
        Assert.NotNull(decodedFrame);
        Assert.Equal(originalFrame.Version, decodedFrame.Version);
        Assert.Equal(originalFrame.MsgType, decodedFrame.MsgType);
        Assert.Equal(originalFrame.ChannelId, decodedFrame.ChannelId);
        Assert.Equal(originalFrame.Payload, decodedFrame.Payload);
        Assert.Equal(encoded.Length, bytesConsumed);
    }

    [Fact]
    public void TryDecode_IncompleteHeader_ReturnsFalse()
    {
        // Arrange - only 5 bytes, header requires 7
        var buffer = new byte[] { 0xAA, 0x55, 0x01, 0x01, 0x05 };

        // Act
        var result = FrameCodec.TryDecode(buffer, out var frame, out var bytesConsumed);

        // Assert
        Assert.False(result);
        Assert.Null(frame);
        Assert.Equal(0, bytesConsumed);
    }

    [Fact]
    public void TryDecode_IncompletePayload_ReturnsFalse()
    {
        // Arrange - header says 4 bytes payload, but only 2 provided
        var buffer = new byte[] { 0xAA, 0x55, 0x01, 0x01, 0x05, 0x00, 0x04, 0x01, 0x02 };

        // Act
        var result = FrameCodec.TryDecode(buffer, out var frame, out var bytesConsumed);

        // Assert
        Assert.False(result);
        Assert.Null(frame);
        Assert.Equal(0, bytesConsumed);
    }

    [Fact]
    public void TryDecode_InvalidMagic_ConsumesOneByte()
    {
        // Arrange - wrong magic bytes
        var buffer = new byte[] { 0x00, 0x00, 0x01, 0x01, 0x05, 0x00, 0x00 };

        // Act
        var result = FrameCodec.TryDecode(buffer, out var frame, out var bytesConsumed);

        // Assert
        Assert.False(result);
        Assert.Null(frame);
        Assert.Equal(1, bytesConsumed); // Should skip one byte for resync
    }

    [Fact]
    public void TryDecode_MultipleFrames_DecodesFirstOnly()
    {
        // Arrange - two complete frames
        var frame1 = FrameCodec.CreateDataFrame(1, new byte[] { 0x01 });
        var frame2 = FrameCodec.CreateDataFrame(2, new byte[] { 0x02, 0x03 });
        var combined = FrameCodec.Encode(frame1).Concat(FrameCodec.Encode(frame2)).ToArray();

        // Act
        var result = FrameCodec.TryDecode(combined, out var decodedFrame, out var bytesConsumed);

        // Assert
        Assert.True(result);
        Assert.NotNull(decodedFrame);
        Assert.Equal(1, decodedFrame.ChannelId);
        Assert.Single(decodedFrame.Payload);
        Assert.Equal(0x01, decodedFrame.Payload[0]);
        Assert.Equal(Frame.HeaderSize + 1, bytesConsumed);
    }

    [Fact]
    public void TryDecode_EmptyBuffer_ReturnsFalse()
    {
        // Arrange
        var buffer = Array.Empty<byte>();

        // Act
        var result = FrameCodec.TryDecode(buffer, out var frame, out var bytesConsumed);

        // Assert
        Assert.False(result);
        Assert.Null(frame);
        Assert.Equal(0, bytesConsumed);
    }

    [Fact]
    public void CreateDataFrame_ReturnsCorrectFrame()
    {
        // Arrange
        byte channelId = 7;
        var data = new byte[] { 0x11, 0x22, 0x33 };

        // Act
        var frame = FrameCodec.CreateDataFrame(channelId, data);

        // Assert
        Assert.Equal(Frame.CurrentVersion, frame.Version);
        Assert.Equal(MessageType.Data, frame.MsgType);
        Assert.Equal(channelId, frame.ChannelId);
        Assert.Equal(data, frame.Payload);
    }

    [Fact]
    public void CreateErrorFrame_ReturnsCorrectFrame()
    {
        // Arrange
        byte channelId = 3;
        var errorMessage = "Test error";

        // Act
        var frame = FrameCodec.CreateErrorFrame(channelId, errorMessage);

        // Assert
        Assert.Equal(Frame.CurrentVersion, frame.Version);
        Assert.Equal(MessageType.Error, frame.MsgType);
        Assert.Equal(channelId, frame.ChannelId);
        Assert.Equal(errorMessage, System.Text.Encoding.UTF8.GetString(frame.Payload));
    }

    [Fact]
    public void CreateHeartbeatFrame_ReturnsCorrectFrame()
    {
        // Act
        var frame = FrameCodec.CreateHeartbeatFrame();

        // Assert
        Assert.Equal(Frame.CurrentVersion, frame.Version);
        Assert.Equal(MessageType.Heartbeat, frame.MsgType);
        Assert.Equal(0, frame.ChannelId);
        Assert.Empty(frame.Payload);
    }

    [Fact]
    public void RoundTrip_AllMessageTypes_PreservesData()
    {
        // Arrange
        var messageTypes = Enum.GetValues<MessageType>();

        foreach (var msgType in messageTypes)
        {
            var original = new Frame
            {
                Version = Frame.CurrentVersion,
                MsgType = msgType,
                ChannelId = (byte)(int)msgType,
                Payload = new byte[] { 0x01, 0x02 }
            };

            // Act
            var encoded = FrameCodec.Encode(original);
            var decoded = FrameCodec.TryDecode(encoded, out var result, out _);

            // Assert
            Assert.True(decoded);
            Assert.NotNull(result);
            Assert.Equal(original.Version, result.Version);
            Assert.Equal(original.MsgType, result.MsgType);
            Assert.Equal(original.ChannelId, result.ChannelId);
            Assert.Equal(original.Payload, result.Payload);
        }
    }

    [Fact]
    public void RoundTrip_MaxPayloadSize_Works()
    {
        // Arrange
        var payload = new byte[Frame.MaxPayloadSize];
        new Random(42).NextBytes(payload);
        
        var original = new Frame
        {
            Version = Frame.CurrentVersion,
            MsgType = MessageType.Data,
            ChannelId = 255,
            Payload = payload
        };

        // Act
        var encoded = FrameCodec.Encode(original);
        var decoded = FrameCodec.TryDecode(encoded, out var result, out var bytesConsumed);

        // Assert
        Assert.True(decoded);
        Assert.NotNull(result);
        Assert.Equal(original.Payload, result.Payload);
        Assert.Equal(Frame.HeaderSize + Frame.MaxPayloadSize, bytesConsumed);
    }

    [Fact]
    public void TryDecode_ResyncAfterInvalidMagic_FindsValidFrame()
    {
        // Arrange - garbage bytes followed by valid frame
        var validFrame = FrameCodec.CreateDataFrame(5, new byte[] { 0xFF });
        var validEncoded = FrameCodec.Encode(validFrame);
        var garbage = new byte[] { 0x00, 0x01, 0x02 };
        var combined = garbage.Concat(validEncoded).ToArray();

        // Act - decode multiple times to skip garbage
        var offset = 0;
        Frame? decodedFrame = null;
        
        while (offset < combined.Length)
        {
            var result = FrameCodec.TryDecode(combined.AsSpan(offset), out decodedFrame, out var consumed);
            if (result && decodedFrame != null)
            {
                break;
            }
            offset += consumed > 0 ? consumed : 1;
        }

        // Assert
        Assert.NotNull(decodedFrame);
        Assert.Equal(5, decodedFrame.ChannelId);
        Assert.Single(decodedFrame.Payload);
        Assert.Equal(0xFF, decodedFrame.Payload[0]);
    }
}

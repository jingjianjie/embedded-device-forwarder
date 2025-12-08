using System.IO.Ports;
using DeviceForwarder.Daemon.Configuration;
using DeviceForwarder.Daemon.Protocol;
using Microsoft.Extensions.Logging;

namespace DeviceForwarder.Daemon.Services;

/// <summary>
/// Host link implementation using USB virtual serial port (CDC ACM).
/// This allows direct USB connection from PC to the forwarder device.
/// </summary>
public sealed class UsbSerialLink : IHostLink
{
    private readonly ILogger<UsbSerialLink> _logger;
    private readonly HostLinkConfig _config;
    private readonly IChannelManager _channelManager;
    private SerialPort? _serialPort;
    private readonly byte[] _readBuffer = new byte[8192];
    private readonly MemoryStream _frameBuffer = new();
    private CancellationTokenSource? _cts;
    private Task? _readTask;
    private bool _disposed;

    /// <inheritdoc />
    public bool IsActive => _serialPort?.IsOpen ?? false;

    public UsbSerialLink(
        HostLinkConfig config,
        IChannelManager channelManager,
        ILogger<UsbSerialLink> logger)
    {
        _config = config ?? throw new ArgumentNullException(nameof(config));
        _channelManager = channelManager ?? throw new ArgumentNullException(nameof(channelManager));
        _logger = logger ?? throw new ArgumentNullException(nameof(logger));
    }

    /// <inheritdoc />
    public async Task StartAsync(CancellationToken cancellationToken)
    {
        _cts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);

        try
        {
            // Use the device path from config (e.g., /dev/ttyGS0 for USB gadget)
            var devicePath = _config.UsbSerialDevice ?? "/dev/ttyGS0";
            
            _serialPort = new SerialPort(devicePath)
            {
                BaudRate = 115200, // Standard baud rate for USB CDC ACM
                DataBits = 8,
                Parity = Parity.None,
                StopBits = StopBits.One,
                ReadTimeout = 1000,
                WriteTimeout = 1000,
                // Enable hardware flow control for more reliable communication
                Handshake = Handshake.None
            };

            _serialPort.Open();
            _logger.LogInformation("USB Serial host link opened on {DevicePath}", devicePath);

            // Subscribe to channel data events
            _channelManager.ChannelDataReceived += OnChannelDataReceived;

            // Start reading frames from the serial port
            _readTask = Task.Run(() => ReadFramesAsync(_cts.Token), _cts.Token);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to open USB serial host link");
            throw;
        }

        // Keep the task running
        await Task.Delay(Timeout.Infinite, cancellationToken).ConfigureAwait(false);
    }

    /// <inheritdoc />
    public Task StopAsync()
    {
        _cts?.Cancel();
        _channelManager.ChannelDataReceived -= OnChannelDataReceived;

        if (_serialPort != null && _serialPort.IsOpen)
        {
            _serialPort.Close();
            _serialPort.Dispose();
            _serialPort = null;
        }

        _logger.LogInformation("USB Serial host link stopped");
        return Task.CompletedTask;
    }

    /// <inheritdoc />
    public void SendToHost(byte channelId, byte[] data)
    {
        if (_serialPort == null || !_serialPort.IsOpen)
        {
            _logger.LogWarning("Cannot send to host - USB serial port is not open");
            return;
        }

        try
        {
            var frame = FrameCodec.CreateDataFrame(channelId, data);
            var encoded = FrameCodec.Encode(frame);

            _serialPort.Write(encoded, 0, encoded.Length);
            _logger.LogDebug("Sent {ByteCount} bytes to host for channel {ChannelId}", 
                encoded.Length, channelId);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to send data to host");
        }
    }

    private async Task ReadFramesAsync(CancellationToken cancellationToken)
    {
        try
        {
            while (!cancellationToken.IsCancellationRequested && _serialPort != null && _serialPort.IsOpen)
            {
                try
                {
                    var bytesToRead = _serialPort.BytesToRead;
                    if (bytesToRead > 0)
                    {
                        var bytesRead = _serialPort.Read(_readBuffer, 0, Math.Min(bytesToRead, _readBuffer.Length));
                        if (bytesRead > 0)
                        {
                            _frameBuffer.Write(_readBuffer, 0, bytesRead);
                            ProcessFrameBuffer();
                        }
                    }
                    else
                    {
                        // Small delay to prevent busy-waiting
                        await Task.Delay(10, cancellationToken);
                    }
                }
                catch (TimeoutException)
                {
                    // Read timeout is normal, continue
                }
                catch (OperationCanceledException)
                {
                    break;
                }
            }
        }
        catch (Exception ex)
        {
            if (!cancellationToken.IsCancellationRequested)
            {
                _logger.LogError(ex, "Error reading from USB serial port");
            }
        }
    }

    private void ProcessFrameBuffer()
    {
        var buffer = _frameBuffer.ToArray();
        var offset = 0;

        while (offset < buffer.Length)
        {
            var span = buffer.AsSpan(offset);
            if (FrameCodec.TryDecode(span, out var frame, out var bytesConsumed))
            {
                if (frame != null)
                {
                    HandleReceivedFrame(frame);
                }
                offset += bytesConsumed;
            }
            else if (bytesConsumed > 0)
            {
                // Invalid magic, skip one byte
                offset += bytesConsumed;
            }
            else
            {
                // Incomplete frame, need more data
                break;
            }
        }

        // Keep remaining bytes in buffer
        _frameBuffer.SetLength(0);
        if (offset < buffer.Length)
        {
            _frameBuffer.Write(buffer, offset, buffer.Length - offset);
        }
    }

    private void HandleReceivedFrame(Frame frame)
    {
        _logger.LogDebug("Received frame from host: Type={MsgType}, Channel={ChannelId}, PayloadLen={PayloadLen}",
            frame.MsgType, frame.ChannelId, frame.Payload.Length);

        switch (frame.MsgType)
        {
            case MessageType.Data:
                ForwardToChannel(frame.ChannelId, frame.Payload);
                break;

            case MessageType.Heartbeat:
                // Echo heartbeat back
                var response = FrameCodec.Encode(FrameCodec.CreateHeartbeatFrame());
                if (_serialPort != null && _serialPort.IsOpen)
                {
                    _serialPort.Write(response, 0, response.Length);
                }
                break;

            case MessageType.ConfigQuery:
                // TODO: Send channel configuration
                break;

            default:
                _logger.LogWarning("Unknown message type: {MsgType}", frame.MsgType);
                break;
        }
    }

    private void ForwardToChannel(byte channelId, byte[] data)
    {
        var channel = _channelManager.GetChannel(channelId);
        if (channel == null)
        {
            _logger.LogWarning("Unknown channel ID: {ChannelId}", channelId);
            return;
        }

        if (!channel.IsOpen)
        {
            _logger.LogWarning("Channel {ChannelId} is not open", channelId);
            return;
        }

        channel.Write(data);
    }

    private void OnChannelDataReceived(object? sender, ChannelDataEventArgs e)
    {
        SendToHost(e.ChannelId, e.Data);
    }

    public void Dispose()
    {
        if (_disposed) return;
        StopAsync().Wait();
        _cts?.Dispose();
        _frameBuffer.Dispose();
        _disposed = true;
    }
}

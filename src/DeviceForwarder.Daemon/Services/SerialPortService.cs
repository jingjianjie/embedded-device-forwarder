using System.IO.Ports;
using DeviceForwarder.Daemon.Configuration;
using Microsoft.Extensions.Logging;

namespace DeviceForwarder.Daemon.Services;

/// <summary>
/// Interface for serial port operations.
/// </summary>
public interface ISerialPortService
{
    /// <summary>
    /// Opens the serial port.
    /// </summary>
    /// <returns>True if successful; otherwise, false.</returns>
    bool Open();

    /// <summary>
    /// Closes the serial port.
    /// </summary>
    void Close();

    /// <summary>
    /// Writes data to the serial port.
    /// </summary>
    /// <param name="data">Data to write.</param>
    void Write(byte[] data);

    /// <summary>
    /// Event raised when data is received from the serial port.
    /// </summary>
    event EventHandler<byte[]>? DataReceived;

    /// <summary>
    /// Gets whether the port is open.
    /// </summary>
    bool IsOpen { get; }

    /// <summary>
    /// Gets the channel configuration.
    /// </summary>
    ChannelConfig Config { get; }
}

/// <summary>
/// Manages a serial port connection for a configured channel.
/// </summary>
public sealed class SerialPortService : ISerialPortService, IDisposable
{
    private readonly ILogger<SerialPortService> _logger;
    private readonly ChannelConfig _config;
    private SerialPort? _serialPort;
    private readonly byte[] _readBuffer = new byte[4096];
    private bool _disposed;

    /// <inheritdoc />
    public event EventHandler<byte[]>? DataReceived;

    /// <inheritdoc />
    public bool IsOpen => _serialPort?.IsOpen ?? false;

    /// <inheritdoc />
    public ChannelConfig Config => _config;

    public SerialPortService(ChannelConfig config, ILogger<SerialPortService> logger)
    {
        _config = config ?? throw new ArgumentNullException(nameof(config));
        _logger = logger ?? throw new ArgumentNullException(nameof(logger));
    }

    /// <inheritdoc />
    public bool Open()
    {
        try
        {
            if (_serialPort != null && _serialPort.IsOpen)
            {
                _logger.LogWarning("Channel {ChannelId}: Port {DevicePath} is already open", _config.Id, _config.DevicePath);
                return true;
            }

            _serialPort = new SerialPort(_config.DevicePath)
            {
                BaudRate = _config.Baudrate,
                DataBits = _config.DataBits,
                Parity = _config.GetParity(),
                StopBits = _config.GetStopBits(),
                ReadTimeout = 1000,
                WriteTimeout = 1000
            };

            _serialPort.DataReceived += OnSerialDataReceived;
            _serialPort.Open();

            _logger.LogInformation(
                "Channel {ChannelId}: Opened {DevicePath} at {Baudrate} baud ({DataBits}{Parity}{StopBits})",
                _config.Id, _config.DevicePath, _config.Baudrate, _config.DataBits, 
                _config.Parity[0], _config.StopBits == "One" ? "1" : _config.StopBits);

            return true;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Channel {ChannelId}: Failed to open {DevicePath}", _config.Id, _config.DevicePath);
            return false;
        }
    }

    /// <inheritdoc />
    public void Close()
    {
        try
        {
            if (_serialPort == null) return;

            if (_serialPort.IsOpen)
            {
                _serialPort.Close();
                _logger.LogInformation("Channel {ChannelId}: Closed {DevicePath}", _config.Id, _config.DevicePath);
            }

            _serialPort.DataReceived -= OnSerialDataReceived;
            _serialPort.Dispose();
            _serialPort = null;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Channel {ChannelId}: Error closing {DevicePath}", _config.Id, _config.DevicePath);
        }
    }

    /// <inheritdoc />
    public void Write(byte[] data)
    {
        if (_serialPort == null || !_serialPort.IsOpen)
        {
            _logger.LogWarning("Channel {ChannelId}: Cannot write - port is not open", _config.Id);
            return;
        }

        try
        {
            _serialPort.Write(data, 0, data.Length);
            _logger.LogDebug("Channel {ChannelId}: Wrote {ByteCount} bytes", _config.Id, data.Length);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Channel {ChannelId}: Error writing {ByteCount} bytes", _config.Id, data.Length);
        }
    }

    private void OnSerialDataReceived(object sender, SerialDataReceivedEventArgs e)
    {
        try
        {
            if (_serialPort == null || !_serialPort.IsOpen) return;

            var bytesToRead = _serialPort.BytesToRead;
            if (bytesToRead <= 0) return;

            var buffer = new byte[bytesToRead];
            var bytesRead = _serialPort.Read(buffer, 0, bytesToRead);

            if (bytesRead > 0)
            {
                var data = new byte[bytesRead];
                Array.Copy(buffer, data, bytesRead);
                _logger.LogDebug("Channel {ChannelId}: Received {ByteCount} bytes", _config.Id, bytesRead);
                DataReceived?.Invoke(this, data);
            }
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Channel {ChannelId}: Error reading data", _config.Id);
        }
    }

    public void Dispose()
    {
        if (_disposed) return;
        Close();
        _disposed = true;
    }
}

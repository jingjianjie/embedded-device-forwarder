using DeviceForwarder.Daemon.Configuration;
using Microsoft.Extensions.Logging;

namespace DeviceForwarder.Daemon.Services;

/// <summary>
/// Manages all serial port channels.
/// </summary>
public interface IChannelManager : IDisposable
{
    /// <summary>
    /// Gets a channel by its ID.
    /// </summary>
    /// <param name="channelId">The channel identifier.</param>
    /// <returns>The serial port service, or null if not found.</returns>
    ISerialPortService? GetChannel(byte channelId);

    /// <summary>
    /// Gets all configured channels.
    /// </summary>
    IEnumerable<ISerialPortService> GetAllChannels();

    /// <summary>
    /// Opens all configured channels.
    /// </summary>
    void OpenAllChannels();

    /// <summary>
    /// Closes all channels.
    /// </summary>
    void CloseAllChannels();

    /// <summary>
    /// Event raised when data is received from any channel.
    /// </summary>
    event EventHandler<ChannelDataEventArgs>? ChannelDataReceived;
}

/// <summary>
/// Event arguments for channel data received events.
/// </summary>
public sealed class ChannelDataEventArgs : EventArgs
{
    public byte ChannelId { get; }
    public byte[] Data { get; }

    public ChannelDataEventArgs(byte channelId, byte[] data)
    {
        ChannelId = channelId;
        Data = data;
    }
}

/// <summary>
/// Implementation of the channel manager.
/// </summary>
public sealed class ChannelManager : IChannelManager
{
    private readonly ILogger<ChannelManager> _logger;
    private readonly ILoggerFactory _loggerFactory;
    private readonly Dictionary<byte, ISerialPortService> _channels = new();
    private bool _disposed;

    /// <inheritdoc />
    public event EventHandler<ChannelDataEventArgs>? ChannelDataReceived;

    public ChannelManager(
        ForwarderConfig config,
        ILoggerFactory loggerFactory,
        ILogger<ChannelManager> logger)
    {
        _loggerFactory = loggerFactory ?? throw new ArgumentNullException(nameof(loggerFactory));
        _logger = logger ?? throw new ArgumentNullException(nameof(logger));

        if (config == null) throw new ArgumentNullException(nameof(config));

        foreach (var channelConfig in config.Channels.Where(c => c.Enabled))
        {
            var serialLogger = _loggerFactory.CreateLogger<SerialPortService>();
            var service = new SerialPortService(channelConfig, serialLogger);
            service.DataReceived += OnChannelDataReceived;
            _channels[channelConfig.Id] = service;
            _logger.LogInformation("Registered channel {ChannelId}: {Name} ({DevicePath})", 
                channelConfig.Id, channelConfig.Name ?? "unnamed", channelConfig.DevicePath);
        }
    }

    /// <inheritdoc />
    public ISerialPortService? GetChannel(byte channelId)
    {
        return _channels.TryGetValue(channelId, out var channel) ? channel : null;
    }

    /// <inheritdoc />
    public IEnumerable<ISerialPortService> GetAllChannels() => _channels.Values;

    /// <inheritdoc />
    public void OpenAllChannels()
    {
        foreach (var (channelId, service) in _channels)
        {
            if (!service.Config.Enabled) continue;
            
            if (!service.Open())
            {
                _logger.LogWarning("Failed to open channel {ChannelId}", channelId);
            }
        }
    }

    /// <inheritdoc />
    public void CloseAllChannels()
    {
        foreach (var service in _channels.Values)
        {
            service.Close();
        }
    }

    private void OnChannelDataReceived(object? sender, byte[] data)
    {
        if (sender is ISerialPortService service)
        {
            ChannelDataReceived?.Invoke(this, new ChannelDataEventArgs(service.Config.Id, data));
        }
    }

    public void Dispose()
    {
        if (_disposed) return;
        
        CloseAllChannels();
        
        foreach (var service in _channels.Values)
        {
            if (service is IDisposable disposable)
            {
                disposable.Dispose();
            }
        }
        
        _channels.Clear();
        _disposed = true;
    }
}

using DeviceForwarder.Daemon.Services;

namespace DeviceForwarder.Daemon;

/// <summary>
/// Background worker that orchestrates the device forwarder daemon.
/// </summary>
public sealed class Worker : BackgroundService
{
    private readonly ILogger<Worker> _logger;
    private readonly IChannelManager _channelManager;
    private readonly IHostLink _hostLink;

    public Worker(
        ILogger<Worker> logger,
        IChannelManager channelManager,
        IHostLink hostLink)
    {
        _logger = logger ?? throw new ArgumentNullException(nameof(logger));
        _channelManager = channelManager ?? throw new ArgumentNullException(nameof(channelManager));
        _hostLink = hostLink ?? throw new ArgumentNullException(nameof(hostLink));
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _logger.LogInformation("Device Forwarder Daemon starting...");

        try
        {
            // Open all configured serial channels
            _channelManager.OpenAllChannels();

            // Start the host link (TCP or USB Serial)
            await _hostLink.StartAsync(stoppingToken);
        }
        catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
        {
            _logger.LogInformation("Device Forwarder Daemon shutdown requested");
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Device Forwarder Daemon error");
            throw;
        }
    }

    public override async Task StopAsync(CancellationToken cancellationToken)
    {
        _logger.LogInformation("Device Forwarder Daemon stopping...");
        
        await _hostLink.StopAsync();
        _channelManager.CloseAllChannels();
        
        await base.StopAsync(cancellationToken);
        _logger.LogInformation("Device Forwarder Daemon stopped");
    }
}

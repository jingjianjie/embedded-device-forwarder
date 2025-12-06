using DeviceForwarder.Daemon.Services;

namespace DeviceForwarder.Daemon;

/// <summary>
/// Background worker that orchestrates the device forwarder daemon.
/// </summary>
public sealed class Worker : BackgroundService
{
    private readonly ILogger<Worker> _logger;
    private readonly IChannelManager _channelManager;
    private readonly IHostLinkService _hostLinkService;

    public Worker(
        ILogger<Worker> logger,
        IChannelManager channelManager,
        IHostLinkService hostLinkService)
    {
        _logger = logger ?? throw new ArgumentNullException(nameof(logger));
        _channelManager = channelManager ?? throw new ArgumentNullException(nameof(channelManager));
        _hostLinkService = hostLinkService ?? throw new ArgumentNullException(nameof(hostLinkService));
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _logger.LogInformation("Device Forwarder Daemon starting...");

        try
        {
            // Open all configured serial channels
            _channelManager.OpenAllChannels();

            // Start the TCP host link
            await _hostLinkService.StartAsync(stoppingToken);
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
        
        await _hostLinkService.StopAsync();
        _channelManager.CloseAllChannels();
        
        await base.StopAsync(cancellationToken);
        _logger.LogInformation("Device Forwarder Daemon stopped");
    }
}

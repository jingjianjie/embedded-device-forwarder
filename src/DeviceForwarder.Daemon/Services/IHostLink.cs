using DeviceForwarder.Daemon.Protocol;

namespace DeviceForwarder.Daemon.Services;

/// <summary>
/// Interface for host communication link (TCP or USB Serial).
/// Abstracts the transport layer for PC-to-forwarder communication.
/// </summary>
public interface IHostLink : IDisposable
{
    /// <summary>
    /// Starts the host link.
    /// </summary>
    /// <param name="cancellationToken">Cancellation token.</param>
    Task StartAsync(CancellationToken cancellationToken);

    /// <summary>
    /// Stops the host link.
    /// </summary>
    Task StopAsync();

    /// <summary>
    /// Sends data to the host(s) for a specific channel.
    /// </summary>
    /// <param name="channelId">The channel identifier.</param>
    /// <param name="data">The data to send.</param>
    void SendToHost(byte channelId, byte[] data);

    /// <summary>
    /// Gets whether the link is active.
    /// </summary>
    bool IsActive { get; }
}

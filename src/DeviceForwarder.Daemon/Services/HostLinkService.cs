using System.Buffers;
using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using DeviceForwarder.Daemon.Configuration;
using DeviceForwarder.Daemon.Protocol;
using Microsoft.Extensions.Logging;

namespace DeviceForwarder.Daemon.Services;

/// <summary>
/// Interface for the TCP host link service.
/// </summary>
public interface IHostLinkService : IHostLink
{
    /// <summary>
    /// Broadcasts data to all connected clients for a specific channel.
    /// </summary>
    /// <param name="channelId">The channel identifier.</param>
    /// <param name="data">The data to broadcast.</param>
    void BroadcastToClients(byte channelId, byte[] data);

    /// <summary>
    /// Gets the number of connected clients.
    /// </summary>
    int ConnectedClientCount { get; }
}

/// <summary>
/// Manages TCP connections from host clients.
/// </summary>
public sealed class HostLinkService : IHostLinkService
{
    private readonly ILogger<HostLinkService> _logger;
    private readonly HostLinkConfig _config;
    private readonly IChannelManager _channelManager;
    private TcpListener? _listener;
    private readonly ConcurrentDictionary<Guid, ClientConnection> _clients = new();
    private CancellationTokenSource? _cts;
    private bool _disposed;

    /// <inheritdoc />
    public int ConnectedClientCount => _clients.Count;

    /// <inheritdoc />
    public bool IsActive => _listener?.Server?.IsBound ?? false;

    public HostLinkService(
        HostLinkConfig config,
        IChannelManager channelManager,
        ILogger<HostLinkService> logger)
    {
        _config = config ?? throw new ArgumentNullException(nameof(config));
        _channelManager = channelManager ?? throw new ArgumentNullException(nameof(channelManager));
        _logger = logger ?? throw new ArgumentNullException(nameof(logger));
    }

    /// <inheritdoc />
    public async Task StartAsync(CancellationToken cancellationToken)
    {
        _cts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        
        var endpoint = new IPEndPoint(IPAddress.Parse(_config.BindAddress), _config.Port);
        _listener = new TcpListener(endpoint);
        _listener.Start();

        _logger.LogInformation("TCP host link listening on {BindAddress}:{Port}", _config.BindAddress, _config.Port);

        // Subscribe to channel data events
        _channelManager.ChannelDataReceived += OnChannelDataReceived;

        // Accept connections in a loop
        try
        {
            while (!_cts.Token.IsCancellationRequested)
            {
                var tcpClient = await _listener.AcceptTcpClientAsync(_cts.Token);
                HandleNewClient(tcpClient);
            }
        }
        catch (OperationCanceledException) when (_cts.Token.IsCancellationRequested)
        {
            // Normal shutdown
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error accepting client connections");
        }
    }

    /// <inheritdoc />
    public Task StopAsync()
    {
        _cts?.Cancel();
        _listener?.Stop();
        _channelManager.ChannelDataReceived -= OnChannelDataReceived;

        foreach (var client in _clients.Values)
        {
            client.Dispose();
        }
        _clients.Clear();

        _logger.LogInformation("TCP host link stopped");
        return Task.CompletedTask;
    }

    /// <inheritdoc />
    public void BroadcastToClients(byte channelId, byte[] data)
    {
        var frame = FrameCodec.CreateDataFrame(channelId, data);
        var encoded = FrameCodec.Encode(frame);

        foreach (var client in _clients.Values)
        {
            try
            {
                client.SendAsync(encoded).ConfigureAwait(false);
            }
            catch (Exception ex)
            {
                _logger.LogWarning(ex, "Failed to send to client {ClientId}", client.Id);
            }
        }
    }

    /// <inheritdoc />
    public void SendToHost(byte channelId, byte[] data)
    {
        // For TCP link, this is the same as BroadcastToClients
        BroadcastToClients(channelId, data);
    }

    private void HandleNewClient(TcpClient tcpClient)
    {
        if (_clients.Count >= _config.MaxConnections)
        {
            _logger.LogWarning("Maximum connections reached, rejecting new client");
            tcpClient.Close();
            return;
        }

        var client = new ClientConnection(tcpClient, _logger, OnClientFrameReceived, OnClientDisconnected);
        _clients[client.Id] = client;
        _logger.LogInformation("Client {ClientId} connected from {RemoteEndPoint}", 
            client.Id, tcpClient.Client.RemoteEndPoint);

        // Start reading from the client
        _ = client.StartReadingAsync(_cts!.Token);
    }

    private void OnClientFrameReceived(Guid clientId, Frame frame)
    {
        _logger.LogDebug("Received frame from client {ClientId}: Type={MsgType}, Channel={ChannelId}, PayloadLen={PayloadLen}",
            clientId, frame.MsgType, frame.ChannelId, frame.Payload.Length);

        switch (frame.MsgType)
        {
            case MessageType.Data:
                ForwardToChannel(frame.ChannelId, frame.Payload);
                break;

            case MessageType.Heartbeat:
                // Echo heartbeat back
                if (_clients.TryGetValue(clientId, out var client))
                {
                    var response = FrameCodec.Encode(FrameCodec.CreateHeartbeatFrame());
                    _ = client.SendAsync(response);
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

    private void OnClientDisconnected(Guid clientId)
    {
        if (_clients.TryRemove(clientId, out var client))
        {
            _logger.LogInformation("Client {ClientId} disconnected", clientId);
            client.Dispose();
        }
    }

    private void OnChannelDataReceived(object? sender, ChannelDataEventArgs e)
    {
        BroadcastToClients(e.ChannelId, e.Data);
    }

    public void Dispose()
    {
        if (_disposed) return;
        StopAsync().Wait();
        _cts?.Dispose();
        _disposed = true;
    }
}

/// <summary>
/// Represents a connected client.
/// </summary>
internal sealed class ClientConnection : IDisposable
{
    private readonly TcpClient _tcpClient;
    private readonly NetworkStream _stream;
    private readonly ILogger _logger;
    private readonly Action<Guid, Frame> _onFrameReceived;
    private readonly Action<Guid> _onDisconnected;
    private readonly byte[] _receiveBuffer = new byte[8192];
    private readonly MemoryStream _frameBuffer = new();
    private bool _disposed;

    public Guid Id { get; } = Guid.NewGuid();

    public ClientConnection(
        TcpClient tcpClient,
        ILogger logger,
        Action<Guid, Frame> onFrameReceived,
        Action<Guid> onDisconnected)
    {
        _tcpClient = tcpClient ?? throw new ArgumentNullException(nameof(tcpClient));
        _stream = tcpClient.GetStream();
        _logger = logger ?? throw new ArgumentNullException(nameof(logger));
        _onFrameReceived = onFrameReceived ?? throw new ArgumentNullException(nameof(onFrameReceived));
        _onDisconnected = onDisconnected ?? throw new ArgumentNullException(nameof(onDisconnected));
    }

    public async Task StartReadingAsync(CancellationToken cancellationToken)
    {
        try
        {
            while (!cancellationToken.IsCancellationRequested && _tcpClient.Connected)
            {
                var bytesRead = await _stream.ReadAsync(_receiveBuffer, cancellationToken);
                if (bytesRead == 0)
                {
                    // Connection closed
                    break;
                }

                _frameBuffer.Write(_receiveBuffer, 0, bytesRead);
                ProcessFrameBuffer();
            }
        }
        catch (OperationCanceledException)
        {
            // Normal shutdown
        }
        catch (IOException)
        {
            // Connection reset
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error reading from client {ClientId}", Id);
        }
        finally
        {
            _onDisconnected(Id);
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
                    _onFrameReceived(Id, frame);
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

    public async Task SendAsync(byte[] data)
    {
        if (!_tcpClient.Connected) return;
        
        try
        {
            await _stream.WriteAsync(data);
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Failed to send data to client {ClientId}", Id);
        }
    }

    public void Dispose()
    {
        if (_disposed) return;
        _stream.Dispose();
        _tcpClient.Dispose();
        _frameBuffer.Dispose();
        _disposed = true;
    }
}

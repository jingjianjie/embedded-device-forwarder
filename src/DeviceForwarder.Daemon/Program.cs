using DeviceForwarder.Daemon;
using DeviceForwarder.Daemon.Configuration;
using DeviceForwarder.Daemon.Services;

var builder = Host.CreateApplicationBuilder(args);

// Load configuration from YAML file
var configPath = args.Length > 0 ? args[0] : "config.yaml";
if (!File.Exists(configPath))
{
    configPath = Path.Combine(AppContext.BaseDirectory, "config.yaml");
}

ForwarderConfig config;
try
{
    config = ConfigLoader.LoadFromFile(configPath);
    Console.WriteLine($"Loaded configuration from {configPath}");
}
catch (Exception ex)
{
    Console.Error.WriteLine($"Failed to load configuration: {ex.Message}");
    Console.Error.WriteLine($"Expected config file at: {configPath}");
    return 1;
}

// Register configuration as singleton
builder.Services.AddSingleton(config);
builder.Services.AddSingleton(config.HostLink);

// Register services
builder.Services.AddSingleton<IChannelManager, ChannelManager>();
builder.Services.AddSingleton<IHostLinkService, HostLinkService>();

// Register the worker
builder.Services.AddHostedService<Worker>();

// Configure logging
builder.Logging.SetMinimumLevel(Enum.TryParse<LogLevel>(config.LogLevel, true, out var level) ? level : LogLevel.Information);

var host = builder.Build();
host.Run();

return 0;

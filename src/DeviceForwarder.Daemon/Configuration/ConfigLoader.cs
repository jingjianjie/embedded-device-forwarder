using YamlDotNet.Serialization;
using YamlDotNet.Serialization.NamingConventions;

namespace DeviceForwarder.Daemon.Configuration;

/// <summary>
/// Provides loading functionality for YAML configuration files.
/// </summary>
public static class ConfigLoader
{
    /// <summary>
    /// Loads configuration from a YAML file.
    /// </summary>
    /// <param name="filePath">Path to the YAML configuration file.</param>
    /// <returns>The loaded configuration.</returns>
    /// <exception cref="FileNotFoundException">Thrown when the configuration file does not exist.</exception>
    /// <exception cref="InvalidOperationException">Thrown when the configuration is invalid.</exception>
    public static ForwarderConfig LoadFromFile(string filePath)
    {
        if (!File.Exists(filePath))
        {
            throw new FileNotFoundException($"Configuration file not found: {filePath}", filePath);
        }

        var yaml = File.ReadAllText(filePath);
        return LoadFromYaml(yaml);
    }

    /// <summary>
    /// Loads configuration from a YAML string.
    /// </summary>
    /// <param name="yaml">YAML configuration string.</param>
    /// <returns>The loaded configuration.</returns>
    /// <exception cref="InvalidOperationException">Thrown when the configuration is invalid.</exception>
    public static ForwarderConfig LoadFromYaml(string yaml)
    {
        var deserializer = new DeserializerBuilder()
            .WithNamingConvention(UnderscoredNamingConvention.Instance)
            .IgnoreUnmatchedProperties()
            .Build();

        var config = deserializer.Deserialize<ForwarderConfig>(yaml);
        
        if (config == null)
        {
            throw new InvalidOperationException("Failed to parse configuration");
        }

        ValidateConfig(config);
        return config;
    }

    /// <summary>
    /// Validates the loaded configuration.
    /// </summary>
    /// <param name="config">The configuration to validate.</param>
    /// <exception cref="InvalidOperationException">Thrown when validation fails.</exception>
    private static void ValidateConfig(ForwarderConfig config)
    {
        if (config.HostLink.Port <= 0 || config.HostLink.Port > 65535)
        {
            throw new InvalidOperationException($"Invalid port number: {config.HostLink.Port}");
        }

        var channelIds = new HashSet<byte>();
        foreach (var channel in config.Channels)
        {
            if (!channelIds.Add(channel.Id))
            {
                throw new InvalidOperationException($"Duplicate channel ID: {channel.Id}");
            }

            if (string.IsNullOrWhiteSpace(channel.DevicePath))
            {
                throw new InvalidOperationException($"Channel {channel.Id} has no device path specified");
            }

            if (channel.Baudrate <= 0)
            {
                throw new InvalidOperationException($"Channel {channel.Id} has invalid baud rate: {channel.Baudrate}");
            }
        }
    }
}

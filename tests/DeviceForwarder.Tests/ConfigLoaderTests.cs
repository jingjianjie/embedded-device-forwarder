using DeviceForwarder.Daemon.Configuration;

namespace DeviceForwarder.Tests;

public class ConfigLoaderTests
{
    [Fact]
    public void LoadFromYaml_ValidConfig_ReturnsConfig()
    {
        // Arrange
        var yaml = @"
host_link:
  bind_address: ""127.0.0.1""
  port: 8080
  max_connections: 5

channels:
  - id: 1
    name: ""Test Channel""
    device_path: ""/dev/ttyUSB0""
    baudrate: 115200
    enabled: true

log_level: Debug
";

        // Act
        var config = ConfigLoader.LoadFromYaml(yaml);

        // Assert
        Assert.NotNull(config);
        Assert.Equal("127.0.0.1", config.HostLink.BindAddress);
        Assert.Equal(8080, config.HostLink.Port);
        Assert.Equal(5, config.HostLink.MaxConnections);
        Assert.Single(config.Channels);
        Assert.Equal(1, config.Channels[0].Id);
        Assert.Equal("Test Channel", config.Channels[0].Name);
        Assert.Equal("/dev/ttyUSB0", config.Channels[0].DevicePath);
        Assert.Equal(115200, config.Channels[0].Baudrate);
        Assert.True(config.Channels[0].Enabled);
        Assert.Equal("Debug", config.LogLevel);
    }

    [Fact]
    public void LoadFromYaml_MultipleChannels_ReturnsAllChannels()
    {
        // Arrange
        var yaml = @"
host_link:
  port: 5000

channels:
  - id: 1
    device_path: ""/dev/ttyUSB0""
    baudrate: 9600
  - id: 2
    device_path: ""/dev/ttyUSB1""
    baudrate: 115200
  - id: 3
    device_path: ""/dev/ttyAMA0""
    baudrate: 38400
";

        // Act
        var config = ConfigLoader.LoadFromYaml(yaml);

        // Assert
        Assert.Equal(3, config.Channels.Count);
        Assert.Equal(1, config.Channels[0].Id);
        Assert.Equal(2, config.Channels[1].Id);
        Assert.Equal(3, config.Channels[2].Id);
    }

    [Fact]
    public void LoadFromYaml_DuplicateChannelIds_ThrowsInvalidOperationException()
    {
        // Arrange
        var yaml = @"
host_link:
  port: 5000

channels:
  - id: 1
    device_path: ""/dev/ttyUSB0""
    baudrate: 9600
  - id: 1
    device_path: ""/dev/ttyUSB1""
    baudrate: 115200
";

        // Act & Assert
        var ex = Assert.Throws<InvalidOperationException>(() => ConfigLoader.LoadFromYaml(yaml));
        Assert.Contains("Duplicate channel ID", ex.Message);
    }

    [Fact]
    public void LoadFromYaml_InvalidPort_ThrowsInvalidOperationException()
    {
        // Arrange
        var yaml = @"
host_link:
  port: -1

channels:
  - id: 1
    device_path: ""/dev/ttyUSB0""
    baudrate: 9600
";

        // Act & Assert
        var ex = Assert.Throws<InvalidOperationException>(() => ConfigLoader.LoadFromYaml(yaml));
        Assert.Contains("Invalid port", ex.Message);
    }

    [Fact]
    public void LoadFromYaml_MissingDevicePath_ThrowsInvalidOperationException()
    {
        // Arrange
        var yaml = @"
host_link:
  port: 5000

channels:
  - id: 1
    baudrate: 9600
";

        // Act & Assert
        var ex = Assert.Throws<InvalidOperationException>(() => ConfigLoader.LoadFromYaml(yaml));
        Assert.Contains("no device path", ex.Message);
    }

    [Fact]
    public void LoadFromYaml_InvalidBaudrate_ThrowsInvalidOperationException()
    {
        // Arrange
        var yaml = @"
host_link:
  port: 5000

channels:
  - id: 1
    device_path: ""/dev/ttyUSB0""
    baudrate: 0
";

        // Act & Assert
        var ex = Assert.Throws<InvalidOperationException>(() => ConfigLoader.LoadFromYaml(yaml));
        Assert.Contains("invalid baud rate", ex.Message);
    }

    [Fact]
    public void LoadFromYaml_DefaultValues_AreApplied()
    {
        // Arrange
        var yaml = @"
channels:
  - id: 1
    device_path: ""/dev/ttyUSB0""
    baudrate: 9600
";

        // Act
        var config = ConfigLoader.LoadFromYaml(yaml);

        // Assert
        // Default host_link values
        Assert.Equal("0.0.0.0", config.HostLink.BindAddress);
        Assert.Equal(5000, config.HostLink.Port);
        Assert.Equal(10, config.HostLink.MaxConnections);
        
        // Default channel values
        Assert.Equal(8, config.Channels[0].DataBits);
        Assert.Equal("None", config.Channels[0].Parity);
        Assert.Equal("One", config.Channels[0].StopBits);
        Assert.True(config.Channels[0].Enabled);
    }

    [Fact]
    public void LoadFromFile_NonExistentFile_ThrowsFileNotFoundException()
    {
        // Act & Assert
        Assert.Throws<FileNotFoundException>(() => ConfigLoader.LoadFromFile("/nonexistent/path/config.yaml"));
    }
}

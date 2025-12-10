using DeviceForwarder.Daemon.Configuration;
using Xunit;

namespace DeviceForwarder.Tests;

/// <summary>
/// Tests for HostLinkConfig to ensure USB and TCP mode configurations work correctly.
/// </summary>
public sealed class HostLinkConfigTests
{
    [Fact]
    public void HostLinkConfig_DefaultValues_AreCorrect()
    {
        // Arrange & Act
        var config = new HostLinkConfig();

        // Assert
        Assert.Equal("tcp", config.Mode);
        Assert.Equal("0.0.0.0", config.BindAddress);
        Assert.Equal(5000, config.Port);
        Assert.Equal(10, config.MaxConnections);
        Assert.Equal(30, config.ConnectionTimeoutSeconds);
        Assert.Equal(10, config.HeartbeatIntervalSeconds);
        Assert.Equal("/dev/ttyGS0", config.UsbSerialDevice);
    }

    [Theory]
    [InlineData("tcp")]
    [InlineData("TCP")]
    [InlineData("usb")]
    [InlineData("USB")]
    public void HostLinkConfig_Mode_AcceptsValidValues(string mode)
    {
        // Arrange & Act
        var config = new HostLinkConfig { Mode = mode };

        // Assert
        Assert.Equal(mode, config.Mode);
    }

    [Theory]
    [InlineData("/dev/ttyGS0")]
    [InlineData("/dev/ttyGS1")]
    [InlineData("/dev/ttyACM0")]
    public void HostLinkConfig_UsbSerialDevice_AcceptsValidPaths(string devicePath)
    {
        // Arrange & Act
        var config = new HostLinkConfig { UsbSerialDevice = devicePath };

        // Assert
        Assert.Equal(devicePath, config.UsbSerialDevice);
    }

    [Fact]
    public void HostLinkConfig_TcpProperties_AreIndependent()
    {
        // Arrange
        var config = new HostLinkConfig
        {
            Mode = "tcp",
            BindAddress = "127.0.0.1",
            Port = 8080,
            MaxConnections = 5
        };

        // Act & Assert
        Assert.Equal("tcp", config.Mode);
        Assert.Equal("127.0.0.1", config.BindAddress);
        Assert.Equal(8080, config.Port);
        Assert.Equal(5, config.MaxConnections);
    }

    [Fact]
    public void HostLinkConfig_UsbProperties_AreIndependent()
    {
        // Arrange
        var config = new HostLinkConfig
        {
            Mode = "usb",
            UsbSerialDevice = "/dev/ttyGS1"
        };

        // Act & Assert
        Assert.Equal("usb", config.Mode);
        Assert.Equal("/dev/ttyGS1", config.UsbSerialDevice);
    }
}

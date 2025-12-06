using System.IO.Ports;
using DeviceForwarder.Daemon.Configuration;

namespace DeviceForwarder.Tests;

public class ChannelConfigTests
{
    [Theory]
    [InlineData("None", Parity.None)]
    [InlineData("none", Parity.None)]
    [InlineData("NONE", Parity.None)]
    [InlineData("Odd", Parity.Odd)]
    [InlineData("odd", Parity.Odd)]
    [InlineData("Even", Parity.Even)]
    [InlineData("even", Parity.Even)]
    [InlineData("Mark", Parity.Mark)]
    [InlineData("mark", Parity.Mark)]
    [InlineData("Space", Parity.Space)]
    [InlineData("space", Parity.Space)]
    public void GetParity_ValidValues_ReturnsCorrectParity(string input, Parity expected)
    {
        // Arrange
        var config = new ChannelConfig { Parity = input };

        // Act
        var result = config.GetParity();

        // Assert
        Assert.Equal(expected, result);
    }

    [Theory]
    [InlineData("invalid")]
    [InlineData("")]
    [InlineData(null)]
    public void GetParity_InvalidValues_ReturnsNone(string? input)
    {
        // Arrange
        var config = new ChannelConfig { Parity = input ?? string.Empty };

        // Act
        var result = config.GetParity();

        // Assert
        Assert.Equal(Parity.None, result);
    }

    [Theory]
    [InlineData("None", StopBits.None)]
    [InlineData("none", StopBits.None)]
    [InlineData("One", StopBits.One)]
    [InlineData("one", StopBits.One)]
    [InlineData("1", StopBits.One)]
    [InlineData("Two", StopBits.Two)]
    [InlineData("two", StopBits.Two)]
    [InlineData("2", StopBits.Two)]
    [InlineData("OnePointFive", StopBits.OnePointFive)]
    [InlineData("onepointfive", StopBits.OnePointFive)]
    [InlineData("1.5", StopBits.OnePointFive)]
    public void GetStopBits_ValidValues_ReturnsCorrectStopBits(string input, StopBits expected)
    {
        // Arrange
        var config = new ChannelConfig { StopBits = input };

        // Act
        var result = config.GetStopBits();

        // Assert
        Assert.Equal(expected, result);
    }

    [Theory]
    [InlineData("invalid")]
    [InlineData("")]
    [InlineData(null)]
    public void GetStopBits_InvalidValues_ReturnsOne(string? input)
    {
        // Arrange
        var config = new ChannelConfig { StopBits = input ?? string.Empty };

        // Act
        var result = config.GetStopBits();

        // Assert
        Assert.Equal(StopBits.One, result);
    }

    [Fact]
    public void ChannelConfig_DefaultValues_AreCorrect()
    {
        // Arrange & Act
        var config = new ChannelConfig();

        // Assert
        Assert.Equal(0, config.Id);
        Assert.Equal(string.Empty, config.DevicePath);
        Assert.Equal(9600, config.Baudrate);
        Assert.Equal(8, config.DataBits);
        Assert.Equal("None", config.Parity);
        Assert.Equal("One", config.StopBits);
        Assert.Null(config.Name);
        Assert.True(config.Enabled);
    }
}

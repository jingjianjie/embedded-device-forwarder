# Embedded Device Forwarder

A C#/.NET 8 daemon for ARM Linux (Raspberry Pi/Orange Pi) that provides a universal device forwarding solution. It bridges TCP connections from a host PC with serial ports on the embedded device, enabling transparent data forwarding using a custom binary framing protocol.

## Features

- **TCP Host Link**: Accepts multiple concurrent TCP connections from host clients
- **Binary Framing Protocol**: Efficient, versioned protocol with magic bytes, channel routing, and variable-length payloads
- **YAML Configuration**: Easy-to-edit configuration for channels and network settings
- **Serial Port Management**: Automatic serial port handling with configurable baud rate, parity, stop bits, etc.
- **Channel-based Routing**: Route data to specific serial ports using channel IDs (0-255)
- **.NET Generic Host**: Built using .NET's Generic Host for proper lifecycle management
- **Dependency Injection**: Clean architecture with DI for testability
- **Comprehensive Logging**: Structured logging with configurable log levels
- **ARM Linux Support**: Targets ARM architectures (arm, arm64) for Raspberry Pi, Orange Pi, and similar boards

## Protocol Specification

### Frame Format

| Field       | Size (bytes) | Description                              |
|-------------|--------------|------------------------------------------|
| Magic       | 2            | Magic bytes (0xAA 0x55) - big-endian     |
| Version     | 1            | Protocol version (currently 1)           |
| MsgType     | 1            | Message type (see below)                 |
| ChannelId   | 1            | Target channel ID (0-255)                |
| PayloadLen  | 2            | Payload length (0-65535) - big-endian    |
| Payload     | N            | Variable-length payload data             |

### Message Types

| Type           | Value | Description                              |
|----------------|-------|------------------------------------------|
| Data           | 0x01  | Data frame for serial port forwarding    |
| Ack            | 0x02  | Acknowledgment frame                     |
| Error          | 0x03  | Error notification frame                 |
| Heartbeat      | 0x04  | Connection keepalive                     |
| ConfigQuery    | 0x05  | Request channel configuration            |
| ConfigResponse | 0x06  | Channel configuration response           |

## Installation

### Prerequisites

- .NET 8 SDK
- ARM Linux device (Raspberry Pi, Orange Pi, etc.) or x64 Linux/Windows for development

### Building

```bash
# Clone the repository
git clone https://github.com/jingjianjie/embedded-device-forwarder.git
cd embedded-device-forwarder

# Build for ARM Linux (Raspberry Pi 32-bit)
dotnet publish src/DeviceForwarder.Daemon -c Release -r linux-arm --self-contained

# Build for ARM64 Linux (Raspberry Pi 64-bit, Orange Pi)
dotnet publish src/DeviceForwarder.Daemon -c Release -r linux-arm64 --self-contained

# Build for current platform (development)
dotnet build
```

### Running Tests

```bash
dotnet test
```

## Configuration

Create a `config.yaml` file in the application directory:

```yaml
# TCP Host Link Configuration
host_link:
  bind_address: "0.0.0.0"    # Listen on all interfaces
  port: 5000                  # TCP port
  max_connections: 10         # Max concurrent clients
  connection_timeout_seconds: 30
  heartbeat_interval_seconds: 10

# Serial Channel Configuration
channels:
  - id: 1
    name: "USB Serial 0"
    device_path: "/dev/ttyUSB0"
    baudrate: 115200
    data_bits: 8
    parity: "None"       # None, Odd, Even, Mark, Space
    stop_bits: "One"     # None, One, Two, OnePointFive
    enabled: true

  - id: 2
    name: "Pi UART"
    device_path: "/dev/ttyAMA0"
    baudrate: 9600
    enabled: true

# Logging
log_level: "Information"  # Trace, Debug, Information, Warning, Error, Critical
```

## Usage

### Running the Daemon

```bash
# Using default config.yaml in current directory
./DeviceForwarder.Daemon

# Using custom config file
./DeviceForwarder.Daemon /path/to/config.yaml
```

### Running as a systemd Service

Create `/etc/systemd/system/device-forwarder.service`:

```ini
[Unit]
Description=Embedded Device Forwarder
After=network.target

[Service]
Type=simple
ExecStart=/opt/device-forwarder/DeviceForwarder.Daemon /opt/device-forwarder/config.yaml
WorkingDirectory=/opt/device-forwarder
Restart=always
RestartSec=10
User=root

[Install]
WantedBy=multi-user.target
```

Enable and start the service:

```bash
sudo systemctl enable device-forwarder
sudo systemctl start device-forwarder
```

### Client Example (Python)

```python
import socket
import struct

# Connect to the forwarder
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('192.168.1.100', 5000))

# Create a data frame for channel 1
magic = 0xAA55
version = 1
msg_type = 0x01  # Data
channel_id = 1
payload = b'Hello, Serial!'
payload_len = len(payload)

# Pack the frame (big-endian)
frame = struct.pack('>HBBBH', magic, version, msg_type, channel_id, payload_len) + payload

sock.send(frame)

# Receive response
response = sock.recv(4096)
```

## Project Structure

```
embedded-device-forwarder/
├── DeviceForwarder.sln
├── README.md
├── src/
│   └── DeviceForwarder.Daemon/
│       ├── Configuration/
│       │   ├── ChannelConfig.cs      # Serial channel settings
│       │   ├── ConfigLoader.cs       # YAML configuration loader
│       │   ├── ForwarderConfig.cs    # Root configuration
│       │   └── HostLinkConfig.cs     # TCP settings
│       ├── Protocol/
│       │   ├── Frame.cs              # Frame data structure
│       │   ├── FrameCodec.cs         # Encode/decode logic
│       │   └── MessageType.cs        # Message type enum
│       ├── Services/
│       │   ├── ChannelManager.cs     # Serial port management
│       │   ├── HostLinkService.cs    # TCP server
│       │   └── SerialPortService.cs  # Serial port wrapper
│       ├── Program.cs                # Application entry point
│       ├── Worker.cs                 # Background service
│       └── config.yaml               # Example configuration
└── tests/
    └── DeviceForwarder.Tests/
        ├── ChannelConfigTests.cs     # Configuration tests
        ├── ConfigLoaderTests.cs      # YAML loader tests
        └── FrameCodecTests.cs        # Protocol codec tests
```

## Architecture

The daemon uses .NET Generic Host with the following key components:

1. **Worker**: Background service that orchestrates startup and shutdown
2. **HostLinkService**: TCP server that accepts client connections and routes frames
3. **ChannelManager**: Manages serial port instances for configured channels
4. **SerialPortService**: Wraps individual serial ports with event-driven data reception
5. **FrameCodec**: Encodes/decodes the binary framing protocol

Data flow:
```
Host Client <--TCP--> HostLinkService <--Frames--> ChannelManager <--Bytes--> SerialPortService <---> Serial Device
```

## License

MIT License

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

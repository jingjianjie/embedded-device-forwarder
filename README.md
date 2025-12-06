# 嵌入式设备转发器

一个运行在 ARM Linux（Raspberry Pi/Orange Pi）上的 C#/.NET 8 守护进程，提供通用的设备转发解决方案。它在主机 PC 的 TCP 连接与嵌入式设备的串口之间搭建桥梁，使用自定义的二进制帧协议实现透明的数据转发。

## 功能特性

- **TCP 主机连接**：支持来自主机客户端的多个并发 TCP 连接
- **二进制帧协议**：高效的版本化协议，包含魔术字节、通道路由和可变长度有效负载
- **YAML 配置**：易于编辑的通道和网络设置配置
- **串口管理**：自动处理串口，支持可配置的波特率、校验位、停止位等
- **基于通道的路由**：使用通道 ID（0-255）将数据路由到特定串口
- **.NET 通用主机**：使用 .NET 的通用主机构建，实现适当的生命周期管理
- **依赖注入**：采用依赖注入的简洁架构，提高可测试性
- **全面的日志记录**：结构化日志记录，支持可配置的日志级别
- **ARM Linux 支持**：面向 ARM 架构（arm、arm64），适用于 Raspberry Pi、Orange Pi 等开发板

## 协议规范

### 帧格式

| 字段        | 大小（字节） | 描述                                     |
|-------------|--------------|------------------------------------------|
| Magic       | 2            | 魔术字节（0xAA 0x55）- 大端序            |
| Version     | 1            | 协议版本（当前为 1）                     |
| MsgType     | 1            | 消息类型（见下表）                       |
| ChannelId   | 1            | 目标通道 ID（0-255）                     |
| PayloadLen  | 2            | 有效负载长度（0-65535）- 大端序          |
| Payload     | N            | 可变长度的有效负载数据                   |

### 消息类型

| 类型           | 值    | 描述                                     |
|----------------|-------|------------------------------------------|
| Data           | 0x01  | 串口转发的数据帧                         |
| Ack            | 0x02  | 确认帧                                   |
| Error          | 0x03  | 错误通知帧                               |
| Heartbeat      | 0x04  | 连接保活                                 |
| ConfigQuery    | 0x05  | 请求通道配置                             |
| ConfigResponse | 0x06  | 通道配置响应                             |

## 安装

### 前置要求

- .NET 8 SDK
- ARM Linux 设备（Raspberry Pi、Orange Pi 等）或用于开发的 x64 Linux/Windows

### 构建

```bash
# 克隆仓库
git clone https://github.com/jingjianjie/embedded-device-forwarder.git
cd embedded-device-forwarder

# 为 ARM Linux 构建（Raspberry Pi 32 位）
dotnet publish src/DeviceForwarder.Daemon -c Release -r linux-arm --self-contained

# 为 ARM64 Linux 构建（Raspberry Pi 64 位、Orange Pi）
dotnet publish src/DeviceForwarder.Daemon -c Release -r linux-arm64 --self-contained

# 为当前平台构建（开发用）
dotnet build
```

### 运行测试

```bash
dotnet test
```

## 配置

在应用程序目录中创建 `config.yaml` 文件：

```yaml
# TCP 主机连接配置
host_link:
  bind_address: "0.0.0.0"    # 监听所有网络接口
  port: 5000                  # TCP 端口
  max_connections: 10         # 最大并发客户端数
  connection_timeout_seconds: 30
  heartbeat_interval_seconds: 10

# 串口通道配置
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

# 日志
log_level: "Information"  # Trace, Debug, Information, Warning, Error, Critical
```

## 使用方法

### 运行守护进程

```bash
# 使用当前目录的默认 config.yaml
./DeviceForwarder.Daemon

# 使用自定义配置文件
./DeviceForwarder.Daemon /path/to/config.yaml
```

### 作为 systemd 服务运行

创建 `/etc/systemd/system/device-forwarder.service`：

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

启用并启动服务：

```bash
sudo systemctl enable device-forwarder
sudo systemctl start device-forwarder
```

### 客户端示例（Python）

```python
import socket
import struct

# 连接到转发器
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('192.168.1.100', 5000))

# 为通道 1 创建数据帧
magic = 0xAA55
version = 1
msg_type = 0x01  # Data
channel_id = 1
payload = b'Hello, Serial!'
payload_len = len(payload)

# 打包帧（大端序）
frame = struct.pack('>HBBBH', magic, version, msg_type, channel_id, payload_len) + payload

sock.send(frame)

# 接收响应
response = sock.recv(4096)
```

## 项目结构

```
embedded-device-forwarder/
├── DeviceForwarder.sln
├── README.md
├── src/
│   └── DeviceForwarder.Daemon/
│       ├── Configuration/
│       │   ├── ChannelConfig.cs      # 串口通道设置
│       │   ├── ConfigLoader.cs       # YAML 配置加载器
│       │   ├── ForwarderConfig.cs    # 根配置
│       │   └── HostLinkConfig.cs     # TCP 设置
│       ├── Protocol/
│       │   ├── Frame.cs              # 帧数据结构
│       │   ├── FrameCodec.cs         # 编码/解码逻辑
│       │   └── MessageType.cs        # 消息类型枚举
│       ├── Services/
│       │   ├── ChannelManager.cs     # 串口管理
│       │   ├── HostLinkService.cs    # TCP 服务器
│       │   └── SerialPortService.cs  # 串口包装器
│       ├── Program.cs                # 应用程序入口
│       ├── Worker.cs                 # 后台服务
│       └── config.yaml               # 配置示例
└── tests/
    └── DeviceForwarder.Tests/
        ├── ChannelConfigTests.cs     # 配置测试
        ├── ConfigLoaderTests.cs      # YAML 加载器测试
        └── FrameCodecTests.cs        # 协议编解码测试
```

## 架构

该守护进程使用 .NET 通用主机，包含以下关键组件：

1. **Worker**：编排启动和关闭的后台服务
2. **HostLinkService**：接受客户端连接并路由帧的 TCP 服务器
3. **ChannelManager**：管理已配置通道的串口实例
4. **SerialPortService**：使用事件驱动的数据接收包装各个串口
5. **FrameCodec**：编码/解码二进制帧协议

数据流：
```
主机客户端 <--TCP--> HostLinkService <--帧--> ChannelManager <--字节--> SerialPortService <---> 串口设备
```

## 许可证

MIT License

## 贡献

欢迎贡献！请随时提交 Pull Request。

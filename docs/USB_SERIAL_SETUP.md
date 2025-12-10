# USB 虚拟串口设置指南

本文档说明如何配置 ARM Linux 开发板（如 Orange Pi Zero3）以使用 USB 虚拟串口（CDC ACM）模式与上位机通信。

## 概述

USB 虚拟串口模式允许通过 USB 线缆直接连接开发板与 PC，无需网络配置。这种方式具有以下优势：

- 简化连接流程，无需网络配置
- 在某些场景下具有更低的延迟
- 适合便携式应用和现场调试
- 使用标准 USB CDC ACM 协议，兼容性好

## 硬件要求

1. **Orange Pi Zero3** 或其他支持 USB OTG 的 ARM 开发板
2. **USB 线缆**：连接开发板的 OTG 口到 PC
3. **PC 端**：Windows、Linux 或 macOS，支持 USB CDC ACM 设备

## 配置步骤

### 1. 启用 USB Gadget 模式

在 Orange Pi Zero3 上，需要启用 USB gadget 模式并加载 CDC ACM 功能。

#### 方法 A：使用 ConfigFS（推荐）

创建启动脚本 `/usr/local/bin/setup-usb-gadget.sh`：

```bash
#!/bin/bash
# USB Gadget CDC ACM 配置脚本

set -e

# 加载必要的内核模块
modprobe libcomposite

# 配置 USB gadget
cd /sys/kernel/config/usb_gadget/

# 创建 gadget
mkdir -p g1
cd g1

# 设置 USB 设备描述符
echo 0x1d6b > idVendor  # Linux Foundation
echo 0x0104 > idProduct # Multifunction Composite Gadget
echo 0x0100 > bcdDevice # v1.0.0
echo 0x0200 > bcdUSB    # USB 2.0

# 创建字符串描述符
mkdir -p strings/0x409
echo "0123456789" > strings/0x409/serialnumber
echo "Orange Pi" > strings/0x409/manufacturer
echo "Device Forwarder" > strings/0x409/product

# 创建配置
mkdir -p configs/c.1
mkdir -p configs/c.1/strings/0x409
echo "CDC ACM" > configs/c.1/strings/0x409/configuration
echo 250 > configs/c.1/MaxPower

# 创建 ACM 功能
mkdir -p functions/acm.usb0

# 链接功能到配置
ln -s functions/acm.usb0 configs/c.1/

# 查找 UDC 设备并启用 gadget
UDC=$(ls /sys/class/udc)
echo $UDC > UDC

echo "USB Gadget CDC ACM configured successfully"
echo "Device: /dev/ttyGS0"
```

设置权限并运行：

```bash
chmod +x /usr/local/bin/setup-usb-gadget.sh
/usr/local/bin/setup-usb-gadget.sh
```

#### 方法 B：使用 g_serial 模块

```bash
# 加载 g_serial 模块
modprobe g_serial

# 这将创建 /dev/ttyGS0 设备
```

### 2. 开机自动启动

创建 systemd 服务 `/etc/systemd/system/usb-gadget.service`：

```ini
[Unit]
Description=USB Gadget CDC ACM Setup
DefaultDependencies=no
After=local-fs.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/setup-usb-gadget.sh
RemainAfterExit=yes

[Install]
WantedBy=sysinit.target
```

启用服务：

```bash
systemctl enable usb-gadget.service
systemctl start usb-gadget.service
```

### 3. 配置 Device Forwarder

修改 `config.yaml`：

```yaml
# 使用 USB 模式
host_link:
  mode: "usb"
  usb_serial_device: "/dev/ttyGS0"
  connection_timeout_seconds: 30
  heartbeat_interval_seconds: 10

# 串口通道配置保持不变
channels:
  - id: 1
    name: "USB Serial 0"
    device_path: "/dev/ttyUSB0"
    baudrate: 115200
    enabled: true
```

### 4. PC 端设置

#### Linux

USB 设备会自动识别为 `/dev/ttyACM0` 或类似设备。检查：

```bash
dmesg | grep tty
ls -l /dev/ttyACM*
```

#### Windows

Windows 会自动安装 CDC ACM 驱动，设备会出现在设备管理器的"端口 (COM 和 LPT)"下。

#### macOS

设备会出现为 `/dev/cu.usbmodem*` 或 `/dev/tty.usbmodem*`。

## 验证连接

### 在开发板上：

```bash
# 查看 USB gadget 状态
cat /sys/kernel/config/usb_gadget/g1/UDC

# 测试串口设备
ls -l /dev/ttyGS0
```

### 在 PC 上（Linux 示例）：

使用 Python 测试连接：

```python
import serial
import struct

# 打开串口
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)

# 创建测试帧
magic = 0xAA55
version = 1
msg_type = 0x04  # Heartbeat
channel_id = 0
payload_len = 0

# 打包帧（大端序）
frame = struct.pack('>HBBBH', magic, version, msg_type, channel_id, payload_len)

# 发送心跳帧
ser.write(frame)

# 接收响应
response = ser.read(7)  # 读取响应帧头
print(f"Received: {response.hex()}")

ser.close()
```

## 故障排除

### 问题 1：/dev/ttyGS0 不存在

**解决方案**：

```bash
# 检查内核模块
lsmod | grep g_serial
lsmod | grep libcomposite

# 重新加载模块
modprobe -r g_serial
modprobe g_serial

# 或重新运行 ConfigFS 脚本
/usr/local/bin/setup-usb-gadget.sh
```

### 问题 2：PC 无法识别 USB 设备

**解决方案**：

1. 确认使用的是 OTG 口而非普通 USB 口
2. 检查 USB 线缆是否支持数据传输（有些线缆仅支持充电）
3. 检查开发板的 USB OTG 配置

### 问题 3：权限问题

**解决方案**：

```bash
# 添加用户到 dialout 组（Linux）
sudo usermod -a -G dialout $USER

# 或临时更改设备权限
sudo chmod 666 /dev/ttyGS0  # 开发板
sudo chmod 666 /dev/ttyACM0 # PC 端（Linux）
```

### 问题 4：连接不稳定

**解决方案**：

1. 检查 USB 线缆质量
2. 增加 USB 缓冲区大小（如需要）
3. 确保设备供电充足

## USB vs TCP 模式对比

| 特性 | USB 模式 | TCP 模式 |
|------|----------|----------|
| 连接方式 | USB 线缆 | 网络（WiFi/以太网） |
| 配置复杂度 | 需要配置 USB gadget | 需要网络配置 |
| 速度 | 高速（USB 2.0: 480 Mbps） | 取决于网络（10-1000 Mbps） |
| 延迟 | 低 | 中等（取决于网络） |
| 并发连接 | 单一连接 | 多个客户端 |
| 适用场景 | 便携式、现场调试、点对点 | 远程访问、多客户端、分布式 |
| 依赖 | USB 线缆 | 网络基础设施 |

## 参考资料

- [Linux USB Gadget API](https://www.kernel.org/doc/html/latest/usb/gadget.html)
- [USB CDC ACM 规范](https://www.usb.org/document-library/class-definitions-communication-devices-12)
- [Orange Pi Zero3 用户手册](http://www.orangepi.org/html/hardWare/computerAndMicrocontrollers/details/Orange-Pi-Zero-3.html)

## 技术细节

### USB CDC ACM 协议

CDC ACM (Communication Device Class - Abstract Control Model) 是 USB 虚拟串口的标准协议：

- 在主机侧表现为标准串口设备
- 支持标准波特率和串口参数（虽然在虚拟串口中这些参数通常被忽略）
- 在应用层使用与普通串口相同的 API

### 帧协议

无论使用 TCP 还是 USB 模式，Device Forwarder 都使用相同的二进制帧协议：

| 字段 | 大小 | 描述 |
|------|------|------|
| Magic | 2 字节 | 0xAA55 |
| Version | 1 字节 | 1 |
| MsgType | 1 字节 | 消息类型 |
| ChannelId | 1 字节 | 通道号 |
| PayloadLen | 2 字节 | 负载长度 |
| Payload | N 字节 | 实际数据 |

这保证了在两种传输模式之间的完全兼容性。

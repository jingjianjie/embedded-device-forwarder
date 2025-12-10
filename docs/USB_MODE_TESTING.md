# USB 模式测试指南

本文档说明如何测试 USB 虚拟串口模式的实现。

## 自动化测试

### 单元测试

项目包含全面的单元测试，覆盖 USB 模式的配置和功能：

```bash
# 运行所有测试
dotnet test

# 运行特定测试类
dotnet test --filter "FullyQualifiedName~HostLinkConfigTests"
dotnet test --filter "FullyQualifiedName~ConfigLoaderTests"
```

### 测试覆盖

- **HostLinkConfigTests** (6 个测试)
  - 默认配置值验证
  - TCP/USB 模式切换
  - USB 设备路径配置
  - 配置属性独立性

- **ConfigLoaderTests** (新增 3 个测试)
  - USB 模式 YAML 加载
  - TCP 模式 YAML 加载
  - 默认模式（缺少 mode 配置时）

- **总计**: 66 个测试全部通过 ✅

## 手动测试（需要硬件）

以下测试需要实际的 Orange Pi Zero3 或类似硬件。

### 前置条件

1. Orange Pi Zero3 开发板
2. USB 数据线（连接 OTG 口）
3. PC（Linux/Windows/macOS）
4. 至少一个串口设备用于测试通道

### 测试步骤

#### 1. 配置 USB Gadget

在开发板上：

```bash
# 按照 USB_SERIAL_SETUP.md 配置 USB gadget
sudo /usr/local/bin/setup-usb-gadget.sh

# 验证设备创建
ls -l /dev/ttyGS0
```

#### 2. 配置 Device Forwarder

创建测试配置文件 `config-test-usb.yaml`：

```yaml
host_link:
  mode: "usb"
  usb_serial_device: "/dev/ttyGS0"
  heartbeat_interval_seconds: 5

channels:
  - id: 1
    name: "Test Serial"
    device_path: "/dev/ttyUSB0"
    baudrate: 115200
    enabled: true

log_level: "Debug"
```

#### 3. 启动服务

```bash
# 构建项目
dotnet build -c Release

# 运行服务
sudo ./src/DeviceForwarder.Daemon/bin/Release/net8.0/DeviceForwarder.Daemon config-test-usb.yaml
```

预期输出：
```
Loaded configuration from config-test-usb.yaml
Using USB Serial host link mode
Device Forwarder Daemon starting...
Registered channel 1: Test Serial (/dev/ttyUSB0)
Channel 1: Opened /dev/ttyUSB0 at 115200 baud (8N1)
USB Serial host link opened on /dev/ttyGS0
```

#### 4. PC 端连接

在 PC 上：

**Linux:**
```bash
# 查找设备
dmesg | tail -20
ls -l /dev/ttyACM*

# 使用 screen 连接
screen /dev/ttyACM0 115200
```

**Python 测试脚本:**

```python
#!/usr/bin/env python3
import serial
import struct
import time

def create_frame(msg_type, channel_id, payload):
    """创建帧"""
    magic = 0xAA55
    version = 1
    payload_len = len(payload)
    header = struct.pack('>HBBBH', magic, version, msg_type, channel_id, payload_len)
    return header + payload

def parse_frame(data):
    """解析帧"""
    if len(data) < 7:
        return None
    magic, version, msg_type, channel_id, payload_len = struct.unpack('>HBBBH', data[:7])
    if magic != 0xAA55:
        return None
    payload = data[7:7+payload_len]
    return {
        'version': version,
        'msg_type': msg_type,
        'channel_id': channel_id,
        'payload': payload
    }

# 连接到 USB 虚拟串口
port = '/dev/ttyACM0'  # Linux
# port = 'COM3'        # Windows
# port = '/dev/cu.usbmodem14201'  # macOS

ser = serial.Serial(port, 115200, timeout=1)
print(f"Connected to {port}")

# 测试 1: 发送心跳
print("\n=== Test 1: Heartbeat ===")
heartbeat = create_frame(0x04, 0, b'')
ser.write(heartbeat)
print(f"Sent: {heartbeat.hex()}")

response = ser.read(100)
if response:
    print(f"Received: {response.hex()}")
    frame = parse_frame(response)
    if frame and frame['msg_type'] == 0x04:
        print("✅ Heartbeat response received")
    else:
        print("❌ Invalid heartbeat response")
else:
    print("❌ No response")

# 测试 2: 发送数据到通道 1
print("\n=== Test 2: Send Data to Channel 1 ===")
test_data = b'Hello Serial Device!'
data_frame = create_frame(0x01, 1, test_data)
ser.write(data_frame)
print(f"Sent to channel 1: {test_data.decode()}")

time.sleep(0.5)

# 测试 3: 接收数据（如果串口设备有回应）
print("\n=== Test 3: Receive Data ===")
response = ser.read(100)
if response:
    frame = parse_frame(response)
    if frame:
        print(f"Received from channel {frame['channel_id']}: {frame['payload'].hex()}")
        print("✅ Data frame received")
    else:
        print("❌ Invalid frame")
else:
    print("⚠️  No data received (may be normal if device doesn't respond)")

ser.close()
print("\n=== Tests Complete ===")
```

运行测试：
```bash
chmod +x test_usb_mode.py
python3 test_usb_mode.py
```

### 5. 验证功能

检查以下功能：

- [ ] USB 设备被 PC 正确识别
- [ ] 服务启动无错误
- [ ] 心跳帧正常收发
- [ ] 数据帧能发送到串口通道
- [ ] 从串口接收的数据能转发到 PC
- [ ] 断开重连功能正常
- [ ] 日志输出正确详细

### 6. 性能测试

测试数据吞吐量和延迟：

```python
import serial
import time
import struct

port = '/dev/ttyACM0'
ser = serial.Serial(port, 115200, timeout=1)

# 吞吐量测试
print("=== Throughput Test ===")
data_size = 1024  # 1KB
iterations = 100

start = time.time()
for i in range(iterations):
    payload = b'X' * data_size
    frame = create_frame(0x01, 1, payload)
    ser.write(frame)
    
elapsed = time.time() - start
throughput = (data_size * iterations) / elapsed / 1024  # KB/s
print(f"Sent {iterations} frames of {data_size} bytes")
print(f"Throughput: {throughput:.2f} KB/s")

# 延迟测试
print("\n=== Latency Test ===")
latencies = []
for i in range(50):
    start = time.time()
    heartbeat = create_frame(0x04, 0, b'')
    ser.write(heartbeat)
    response = ser.read(7)
    elapsed = time.time() - start
    if len(response) >= 7:
        latencies.append(elapsed * 1000)  # ms

if latencies:
    avg_latency = sum(latencies) / len(latencies)
    print(f"Average latency: {avg_latency:.2f} ms")
    print(f"Min latency: {min(latencies):.2f} ms")
    print(f"Max latency: {max(latencies):.2f} ms")

ser.close()
```

### 7. 故障恢复测试

测试异常情况处理：

1. **USB 断开重连**
   - 拔掉 USB 线
   - 观察服务日志
   - 重新连接 USB
   - 验证是否能继续工作

2. **串口设备故障**
   - 拔掉下游串口设备
   - 发送数据到该通道
   - 观察错误处理
   - 重新连接设备

3. **高负载测试**
   - 同时向多个通道发送大量数据
   - 监控 CPU 和内存使用
   - 验证无丢包或崩溃

## 测试清单

### 基础功能
- [ ] 配置加载正确（mode=usb）
- [ ] USB gadget 设备成功创建
- [ ] 服务成功启动
- [ ] PC 识别 USB 设备
- [ ] 串口通道正常打开

### 协议功能
- [ ] 心跳帧收发正常
- [ ] 数据帧正确路由到通道
- [ ] 从通道接收数据正常
- [ ] 帧编解码正确
- [ ] 错误帧处理正确

### 稳定性
- [ ] 长时间运行稳定
- [ ] 断线重连正常
- [ ] 高负载无崩溃
- [ ] 内存无泄漏

### 性能
- [ ] 吞吐量达标（>100 KB/s）
- [ ] 延迟可接受（<50 ms）
- [ ] CPU 使用率正常（<30%）

## 常见问题

### 问题：PC 无法识别 USB 设备

**检查:**
1. USB gadget 是否正确配置
2. 是否使用正确的 USB 口（OTG 口）
3. USB 线缆是否支持数据传输

### 问题：服务启动失败

**检查:**
1. /dev/ttyGS0 是否存在
2. 权限是否正确（可能需要 root）
3. 配置文件路径是否正确

### 问题：数据无法收发

**检查:**
1. 帧格式是否正确
2. 通道 ID 是否存在
3. 串口设备是否正常工作
4. 查看调试日志

## 与 TCP 模式对比测试

可以进行对比测试以验证两种模式的功能等效性：

```bash
# TCP 模式测试
dotnet run -- config-tcp.yaml

# USB 模式测试
dotnet run -- config-usb.yaml
```

验证两种模式下：
- 相同的帧协议
- 相同的通道管理
- 相同的错误处理
- 相同的性能特性

## 自动化测试脚本

完整的自动化测试脚本可在 `scripts/` 目录找到（待开发）。

## 结论

USB 模式测试应覆盖配置、功能、性能和稳定性各方面。通过自动化测试和手动硬件测试的结合，可以充分验证实现的正确性。

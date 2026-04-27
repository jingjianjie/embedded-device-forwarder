# embedded-device-router · RPD(重构与路线图)

> **范围**: ez_router 的下一阶段演进。现有代码量小,**直接重构,不保留任何前向兼容**。
> **目标读者**: 接手本项目的工程师 / 协助硬件协调的同事。
> **状态**: 草案 v1。需评审后冻结。

---

## 0. 角色与术语

| 术语 | 含义 |
|---|---|
| **ez_router** | 守护进程,跑在开发板上。**只做基础设施**: 路由 / 协议 / 监管 / 日志 / 升级。 |
| **子程序** | ez_router 的下游程序,**只写设备控制业务**。形态可为独立进程或 `.so` 插件。 |
| **上位机 / Host** | PC 端控制软件,通过 TCP 连接 ez_router。 |
| **协议(本文专指)** | 子程序 ↔ ez_router 之间的统一规约。**新设计,与现有 IPC 不兼容**。 |
| **测试机 / Target** | OrangePi @ `192.168.31.6`,`root / orangepi`。当前唯一开发板。 |

---

## 1. 北极星目标

子程序工程师**只需写一个 `main()`**,实现具体设备的控制逻辑,然后:

- 不必关心: 进程守护、心跳、重启、watchdog、日志落盘策略、TF 卡寿命、固件升级流程、与上位机的 TCP 连接
- 只需调用: 几个 SDK 接口(注册元信息、收发数据、写日志、注册命令回调)

ez_router 把其余全部包了。

---

## 2. 重构原则

1. **无前向兼容**: 现有 `ipc_header_t`、`ezr_*` SDK 接口、`config.json` 字段全部可改。
2. **统一协议优先**: 协议层是地基,先于任何上层模块定型。
3. **测试与实现等重**: 每个阶段的产出 = 实现代码 + 自动化测试。无测试不算完成。
4. **测试先在 dev 机跑单测,再 SCP 到 OrangePi 跑集成**: 见 §6。
5. **硬件相关测试需要真设备时,在工单里 @人**: 见 §7。

---

## 3. 系统架构(目标态)

```
                        ┌────────────────────────────┐
                        │       上位机 / Host        │
                        └────────────┬───────────────┘
                                     │ TCP / 帧协议
                                     │
       ┌─────────────────────────────▼────────────────────────────┐
       │                       ez_router                          │
       │  ┌─────────┐  ┌──────────┐  ┌─────────┐  ┌────────────┐ │
       │  │ Reactor │  │ Registry │  │  Log    │  │ Supervisor │ │
       │  │ (epoll) │  │  (元信息)│  │ Sink    │  │ +Watchdog  │ │
       │  └────┬────┘  └────┬─────┘  └────┬────┘  └─────┬──────┘ │
       │       │            │             │              │        │
       │  ┌────▼──── 协议层 (proto_codec) ──────────────────────┐ │
       │  └──────────────────────────────────────────────────────┘ │
       └────────────┬─────────────────────────────────┬───────────┘
                    │ AF_UNIX SOCK_SEQPACKET          │ dlopen
                    │                                  │
            ┌───────▼────────┐                ┌────────▼─────────┐
            │ 子程序(进程外)│                │ 子程序(.so 插件)│
            │  + libezr.a    │                │  + libezr.a       │
            └───────┬────────┘                └────────┬──────────┘
                    │                                  │
              UART / USB / GPIO            ……         具体设备
```

---

## 4. 关键架构决策

| 决策 | 选定 | 理由 |
|---|---|---|
| 子程序部署形态 | **进程外为主、插件为辅(共享同一协议)** | 进程外隔离强、崩溃不连累 router、便于独立升级 |
| 协议传输 | `AF_UNIX SOCK_SEQPACKET` | 保留消息边界,免拆包 |
| 帧格式 | 固定二进制头 + payload(控制 = JSON / 数据 = raw) | 控制类可读、数据类零拷贝 |
| 配置语言 | 沿用 cJSON(已在用) | 体量小,无引入新依赖必要 |
| 单测框架(C) | **Unity (ThrowTheSwitch)** | 单文件、零依赖、嵌入式标准 |
| 集成测试编排 | **pytest + paramiko**(在 dev 机跑,远程驱动 OrangePi) | 表达力强、好做 fixture/teardown |
| 硬件 watchdog | **`/dev/watchdog0` + ioctl,RK3588 自带**(已实测可用) | 测试机为 OP5+,Synopsys DesignWare WDT,默认 44s |
| 插件崩溃策略 | **不隔离**,plugin segfault 直接拖垮 ez_router | 由 hw watchdog 在超时后整板复位重启,简化设计 |
| 日志落盘 | 内存 ring + 时间(30s)/大小(1MB)双触发 + 信号强刷 | 平衡 SD 卡寿命和数据完整性 |
| 日志后端 | **只用自管 log_sink,不双写 journald** | 减少复杂度;journald 自动从 systemd unit 收 stderr 已足够诊断崩溃 |
| C# SDK 目标框架 | **`net8.0` only**(无 netstandard 兼容层) | 上位机统一 .NET 8 |
| 协议演进 | **第一版激进设计,不预留兼容字段** | 现有代码量小,直接重构;后续靠 `version` 字段 bump |
| 升级载荷 | 走协议帧分片 + SHA-256 + 自动回滚 | 不引入额外通道 |

---

## 5. 路线图

### 阶段 0 — 现状加固

| # | 任务 | 文件 | 状态 |
|---|---|---|---|
| 0.1 | 修 plugin 返回值越界(可能栈溢出) | `routerd/src/reactor.c:241-252` | open |
| 0.2 | `strcpy(addr.sun_path,...)` → `strncpy` | `ipc_server.c:33`、`reactor.c:182,202` | open |
| 0.3 | 实现 TCP_SERVER 的 `port_send` | `port_manager.c:369-371` | open |
| 0.4 | TCP/IPC client 断开时释放 `calloc` 的 `port_def_t` | `reactor.c:181,199,223-226` | ✅ **commit `5246de2`(Developer)** |
| 0.5 | 删大段被注释的死代码 | `reactor.c` | open |
| 0.6 | 端到端 loopback 集成测试(TCP↔TCP、UART↔UART) | `tests/integration/test_loopback.py` | open(队列满覆盖问题已被 `5246de2` 顺手解决) |
| 0.7 | router_core.c NULL 解引用顺序 | `router_core.c:13-19` | ✅ **commit `5246de2`(Developer)** |
| 0.8 | port_manager.c PORT_IPC_SERVER case 缺 break(fallthrough) | `port_manager.c:254` | ✅ **commit `5246de2`(Developer)** |
| 0.9 | port_manager.c TCP listen 失败缺 return | `port_manager.c:127` | ✅ **commit `5246de2`(Developer)** |
| 0.10 | reactor.c read 缓冲区 off-by-one | `reactor.c:215` | ✅ **commit `5246de2`(Developer)** |
| 0.11 | event_queue.c 队列满覆盖(双 cond 阻塞 push) | `event_queue.c` | ✅ **commit `5246de2`(Developer)** |

**交付**: 一个稳定的"基本透明转发器"(对应原目标 3)。
**验收**: 单测 100% 通过 + 集成 loopback 跑 1 小时无泄漏(valgrind on target)。

---

### 阶段 1 — 协议层(地基)

#### 1.1 帧格式

```c
// routerd/include/protocol.h
#define PROTO_MAGIC 0xEB7C
#define PROTO_VERSION 1

typedef enum {
    PROTO_REGISTER       = 0x01,
    PROTO_REGISTER_ACK   = 0x02,
    PROTO_HEARTBEAT      = 0x03,
    PROTO_DATA           = 0x10,
    PROTO_CMD            = 0x11,
    PROTO_LOG            = 0x20,
    PROTO_FW_BEGIN       = 0x30,
    PROTO_FW_CHUNK       = 0x31,
    PROTO_FW_END         = 0x32,
    PROTO_FW_QUERY       = 0x33,
} proto_cmd_t;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  version;
    uint8_t  cmd;
    uint32_t seq;
    uint32_t payload_len;
    uint8_t  payload[];
} proto_frame_t;
```

#### 1.2 注册载荷(JSON)

```json
{
  "model": "spectrometer-x1",
  "fw_version": "1.2.3",
  "build_date": "2026-04-15",
  "device_id": "SN20260101-001",
  "ports": [
    {"name":"DEV_UART", "type":"tty", "tty":{...}}
  ]
}
```

→ 覆盖原目标 2.1 / 2.2 / 2.3。

#### 1.3 模块拆分

| 新建 | 职责 |
|---|---|
| `routerd/include/protocol.h` | 帧定义、命令码 |
| `routerd/src/proto_codec.c` | 编码/解码(纯逻辑,易单测) |
| `routerd/include/registry.h` + `src/registry.c` | 子程序元信息表 |
| `routerd/src/proto_dispatcher.c` | 帧分发到 registry / log_sink / supervisor |
| `doc/router_protocol.md` | 协议规约(目前是空文档,正好填) |

**交付**: 协议规约 + 编解码器 + 注册表(对应原目标 2 核心)。
**验收**: codec roundtrip 测试 100%、`afl-fuzz` 跑 1 小时无 crash、注册流程 e2e 通过。

---

### 阶段 2 — 子程序监管(优先)

#### 2.1 配置扩展

```json
"subprocesses": [
  {
    "name": "spectrometer_ctrl",
    "exec": "/usr/local/bin/spectrometer_ctrl",
    "args": ["--port", "/dev/ttyUSB0"],
    "env": {"LOG_LEVEL":"info"},
    "restart": "on-failure",          // always / on-failure / never
    "max_restarts": 5,
    "restart_window_sec": 60,
    "heartbeat_timeout_ms": 3000,
    "graceful_kill_ms": 1000
  }
]
```

#### 2.2 模块

| 新建 | 职责 |
|---|---|
| `routerd/src/supervisor.c` | fork+execvp、SIGCHLD、重启策略、指数退避 |
| `routerd/src/sw_watchdog.c` | 心跳超时检测 → SIGTERM → SIGKILL → 重启 |
| `routerd/src/hw_watchdog.c` | `/dev/watchdog` ioctl,主线程 keepalive,优雅关闭写 `'V'` |

**交付**: 子程序工程师无需写守护逻辑(对应原目标 5/6)。
**验收**: 杀子程序 N 次能正确重启、心跳超时触发重启、`max_restarts` 上限生效、watchdog 喂狗周期正确(用示波器或 `/sys/class/watchdog/watchdog0/timeout` 验证)。

---

### 阶段 3 — 统一日志

#### 3.1 模块

| 新建 | 职责 |
|---|---|
| `routerd/src/log_sink.c` | 内存 ring(默认 4 MB),双触发刷盘,文件滚动,信号强刷 |
| `routerd/src/log_dedup.c` | 同秒重复消息合并 |

#### 3.2 子程序 API(在 SDK 里)

```c
ezr_log(h, EZR_LOG_INFO, "tag", "x=%d", x);
```

子程序无文件 IO、无路径配置,只管打。

**交付**: TF 卡友好的统一日志(对应原目标 7)。
**验收**: 24 小时 soak、`/sys/block/mmcblk0/stat` 写入次数 < 上限阈值、SIGKILL 时丢失 ≤ 1 个 batch。

---

### 阶段 4 — SDK

#### 4.1 C SDK

```c
ezr_handle_t h = ezr_connect("/run/ez_router/ez_router.sock");
ezr_register(h, &(ezr_meta_t){...});
ezr_set_cmd_handler(h, on_cmd);
ezr_send(h, "DEV_UART", buf, len);
ezr_send_zerocopy(h, "HOST", fd, off, len);  // 走 splice/sendfile
ezr_log(h, EZR_LOG_INFO, "tag", "...");
```

#### 4.2 C# SDK(全新)

- TFM: **`net8.0` only**(上位机统一 .NET 8,无需兼容老框架)
- `Task<int> SendAsync(string port, ReadOnlyMemory<byte> data, CancellationToken ct)`
- 大数据零拷贝路径: `ArrayPool<byte>.Shared` + `PipeReader/Writer`
- NuGet 包名: `EmbeddedDeviceRouter.Sdk`

**交付**: 嵌入端 + 上位机的统一编程接口(对应原目标 4)。
**验收**: SDK 单测、图像吞吐 benchmark(对比朴素 `byte[]` 路径)、跨平台冒烟。

---

### 阶段 5 — 固件升级

| 升级目标 | 流程 |
|---|---|
| ez_router 自身 | 收 blob → 校验 → 写 `out/ez_router.new` → atomic rename → exec 重启(systemd 接住) |
| 子程序 binary | 同上 → supervisor 重启该子程序 |
| 物理设备固件 | 透传 `FW_*` 帧,**子程序自己写设备**(YMODEM/MES/具体协议) |

**交付**: 三类目标统一升级通道(对应原目标 2.5)。
**验收**: 中途断电不变砖、错版本被拒、60s 内未注册自动回滚。

---

### 阶段 6 — 插件 hook 完善(低优)

- 触发点: `on_register / on_disconnect / on_data_in / on_data_out / on_cmd`
- handler 签名标准化为 `(ctx_t*, buf, len)`,ctx 注入 src/dst/meta

**交付**: 完整 hook 体系(对应原目标 4)。

---

## 6. 测试策略(本 RPD 的核心之一)

### 6.1 测试机

| 项 | 值 |
|---|---|
| 主机 | **Orange Pi 5 Plus**(RK3588,aarch64) |
| IP | `192.168.31.6` |
| 账号 | `root` / `orangepi` |
| OS | **Armbian**,内核 `6.1.115-vendor-rk35xx` |
| 安全等级 | **无要求**,可随意 ssh、随意写文件、随意重启 |
| 部署路径 | `/opt/ez_router/`(代码 + 配置 + 日志) |
| 硬件 watchdog | **`/dev/watchdog0` 可用**,Synopsys DesignWare,默认 timeout 44s。`MAGICCLOSE=0`(不支持 'V' 字符优雅停),具体 close 行为由内核 `nowayout` 决定,阶段 2 实测 |
| 工具链 | `wdctl`(util-linux)已装,`gcc`/`make`/`systemd` 齐全 |

### 6.2 测试金字塔

```
                ┌──────────────────────┐
                │   Soak / 长跑(慢)  │  4-5 个,夜间跑,< 1/天
                ├──────────────────────┤
                │ HIL 硬件在环(中)   │  ~ 20 个,需真设备
                ├──────────────────────┤
                │  Integration(中快)│  ~ 80 个,target 上跑
                ├──────────────────────┤
                │     Unit(快)       │  数百,dev 机跑
                └──────────────────────┘
```

### 6.3 三类测试

#### A. 单元测试(C / dev 机)

- 框架: **Unity**(`tests/unit/unity/`)
- 覆盖: `proto_codec`、`registry`、`config_store`、`log_dedup`、`log_sink` 内存逻辑、`ring_buffer`
- 跑法: `make test-unit`,毫秒级
- 必须有: codec roundtrip property test、配置 JSON 异常输入(missing field、type mismatch、超长字符串)
- **每个 §5 修的 bug 必须先有一个能复现的失败测试,再修**

#### B. 集成测试(pytest / 远程驱动 target)

- 框架: **pytest + paramiko**(`tests/integration/`)
- 流程:
  1. dev 机 `make` 出 `ez_router` 和 plugins
  2. `scp` 到 `192.168.31.6:/opt/ez_router/`
  3. SSH 起 ez_router(后台)
  4. 起一个或多个"假子程序"(简单 C 程序,用 SDK 注册 + 心跳 + 收发)
  5. 上位机模拟器(pytest 进程内)走 TCP 连入
  6. 断言数据流路径、心跳、重启、注册表内容
  7. teardown: 杀进程、收日志、清 socket
- 关键 fixture: `target_router`、`fake_subproc`、`tcp_host_client`
- 跑法: `make test-integration`(自动 deploy + run)
- 报告: 收集 target 上的 log + core dump 回 dev 机

#### C. 硬件在环 / 长跑(target / 半人工)

- valgrind 长跑(检测阶段 0 的内存修复是否复发)
- 24 小时日志 soak: 每秒 100 条日志,看 SD 卡写入计数
- watchdog 物理验证: **故意挂死 ez_router**,验证板子在硬 watchdog 超时后被复位 → 这条**需要人配合**(见 §7)
- 真 UART 互通: FTDI 接 `/dev/ttyUSB0`,跑 baud 9600/115200/921600

### 6.4 各阶段测试清单

| 阶段 | 必加测试 |
|---|---|
| 0 | bug 复现单测 ×6;TCP↔TCP / UART(socat)↔UART loopback;valgrind 1h |
| 1 | codec roundtrip;libFuzzer/AFL 1h;注册流程 e2e;并发注册同 device_id 冲突处理 |
| 2 | 子程序起停 ×100;心跳超时重启;`max_restarts` 上限;指数退避时间正确;SIGCHLD 不丢;**HW watchdog 物理复位** |
| 3 | ring 满覆盖正确性;时间触发;大小触发;SIGTERM 强刷;SIGKILL 丢失界限;24h soak;dedup 正确性 |
| 4 | C SDK 全接口冒烟;C# SDK 跨平台冒烟;图像零拷贝吞吐 benchmark(目标 ≥ 80% 内存带宽) |
| 5 | 升级 happy path;中途断电不变砖(用 `kill -9 ez_router` 模拟);错 device_id 拒绝;60s 自动回滚 |
| 6 | 插件加载/卸载/重载;插件 segfault 不连累 router(进程内做不到完全隔离,至少要有 sigaction 防护);hook 调用顺序 |

### 6.5 部署与执行脚本

| 脚本 | 作用 |
|---|---|
| `scripts/deploy_target.sh` | 编译 → scp → ssh 起 ez_router |
| `scripts/run_remote_tests.sh` | dev 机一键跑全部集成测试 |
| `scripts/collect_target_logs.sh` | 拉日志、core、journalctl 回本地 |
| `tests/conftest.py` | pytest fixture: target SSH、router 起停、subproc 起停 |

`scripts/deploy_target.sh` 草案:

```bash
#!/bin/bash
set -euo pipefail
TARGET=${TARGET:-192.168.31.6}
PASS=${PASS:-orangepi}
BIN_DIR=/opt/ez_router

cd routerd && make && cd -
cd plugins && make && cd -

sshpass -p "$PASS" ssh -o StrictHostKeyChecking=no root@$TARGET \
  "mkdir -p $BIN_DIR/plugin && systemctl stop ez_router 2>/dev/null || true"
sshpass -p "$PASS" scp out/ez_router root@$TARGET:$BIN_DIR/
sshpass -p "$PASS" scp out/plugin/*.so root@$TARGET:$BIN_DIR/plugin/
sshpass -p "$PASS" scp out/config.json root@$TARGET:$BIN_DIR/
sshpass -p "$PASS" ssh root@$TARGET "cd $BIN_DIR && nohup ./ez_router -log > router.log 2>&1 &"
echo "deployed."
```

### 6.6 CI(轻量)

- 第一阶段: 本地 `make test-unit` + `make test-integration`,工程师自驱
- 后期: GitHub Actions runner 在 dev 机自托管,push 即跑;失败发钉钉/企微

---

## 7. 硬件资源协调

**唤起人类协助**的场景与所需:

| 场景 | 阶段 | 需要 |
|---|---|---|
| 真 UART 互通测试 | 0 / 2 / 5 | 1× FTDI USB-UART,接 OrangePi 的某个 ttyUSB 或 GPIO UART |
| USB CDC 设备测试 | 0 / 5 | 1× 真实 CDC 设备(或第二块板子虚拟出 CDC) |
| 硬件 watchdog 复位验证 | 2 | 有人盯着板子: 看是否真断电重启;最好接示波器或电源监控 |
| 固件升级断电模拟 | 5 | 一个可远程通断电的开关(智能插座即可) |
| 长跑 soak | 3 / 0 | 板子整夜插电、温度可控环境 |
| 板子坏了/卡死 | 任意 | 物理重启 |

**唤起方式**: 在 issue / chat 中 @人,附上需要的硬件清单和**预计时长**,让对方好排时间。

---

## 8. 重构推进顺序与最小演示

依赖图:

```
阶段 0 ─┐
        ├─► 阶段 1 ─┬─► 阶段 2 ★ ─┐
                    ├─► 阶段 3    ├─► 阶段 5
                    ├─► 阶段 4    │
                    └─────────────┘
        └─► 阶段 6(任何时候,低优)
```

**两周 MVP**:阶段 0 + 阶段 1 的 `REGISTER/HEARTBEAT/DATA` + 阶段 2 的进程外子程序 + 软 watchdog + 阶段 3 最小日志 + C SDK 包前述能力。
**演示场景**: 一个"假仪器"子程序在 OrangePi 上被 ez_router 拉起 → 注册 → 心跳 → 故意 hang → 被 SIGTERM/SIGKILL → 重启 → 日志在 SD 卡里能看到。

---

## 9. 已决议事项

R1-R5 在 v1 草案评审时已敲定,记录于此供回溯。新的未决问题请在下方追加。

| # | 问题 | 决议 |
|---|---|---|
| R1 | 测试机型号、`/dev/watchdog` 是否可用? | **OP5+ / RK3588 / Armbian 6.1.115**,`/dev/watchdog0` Synopsys DesignWare 实测可用,默认 timeout 44s。`MAGICCLOSE=0`,具体 close 行为待阶段 2 跑实测确定(`nowayout` 内核选项)。详见 §6.1。 |
| R2 | C# SDK 最低 .NET 版本? | **`net8.0` only**。 |
| R3 | 插件 segfault 是否必须不连累 router? | **接受连累**。由 hw watchdog 整板复位重启,简化设计。 |
| R4 | 是否双写 journald? | **不双写**,只用自管 log_sink。systemd unit 的 stderr 仍会被 journald 收(自动),无需主动配置。 |
| R5 | 协议演进策略? | **第一版激进设计,不预留兼容字段**;后续靠帧头 `version` bump。现有代码量小直接重构。 |

### 待解决

| # | 问题 | 决策点 |
|---|---|---|
| O1 | watchdog 在 daemon 退出时是否需要"停"? | 阶段 2 跑 `cat /sys/module/dw_wdt/parameters/nowayout`(若有)及 close-fd 实测;若 nowayout=Y,只能接受"daemon 一旦退出板子在 44s 内重启",作为运维约束写入 build_guide |

---

## 10. 文档维护

- 本 RPD 是**唯一权威路线图**。变更必须改这里。
- 协议帧定型后,细节移到 `doc/router_protocol.md`,本文只保留概要。
- SDK 用法移到 `doc/sdk_api.md`(目前空文件)。
- 编译/部署细节移到 `doc/build_guide.md`(目前空文件)。

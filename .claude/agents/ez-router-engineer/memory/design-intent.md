# 设计意图与隐性契约

> 本文记录"代码里没写、但所有人都默认遵守"的约定。
> 修改时务必谨慎 — 这些约定可能影响多个模块,一旦破坏后果会延迟显现。

---

## 1. 项目北极星

**ez_router 是基础设施,不是业务逻辑。**

路由器自己**不解析协议、不理解数据、不做选片决策**。所有业务智能都在子程序或插件里。
ez_router 只做:路由、监管、协议传输、日志、升级。

子程序工程师**只需要写一个 `main()`**,实现具体设备的控制。其余由 ez_router 包了:
- 不必关心: 进程守护、心跳、重启、watchdog、日志落盘策略、TF 卡寿命、固件升级流程、与上位机的 TCP 连接
- 只需调用 SDK: 注册元信息、收发数据、写日志、注册命令回调

---

## 2. 协议是地基

**协议层是其他所有模块的基础**(daemon 监管、日志、SDK、固件升级都依赖协议)。
任何上层改动,**先**确认协议契约是否需要先动。

**当前状态**:
- 计划帧定义: `routerd/include/protocol.h`(阶段 1 待建,见 RPD §5.1)
- 临时 IPC: `routerd/src/ipc_server.c` 用 `ipc_header_t`,**已计划废弃**
- 协议第一版**激进设计,不预留兼容字段**(决议 R5)

帧格式约定(草案):
```c
proto_frame_t = { magic=0xEB7C, version, cmd, seq, payload_len, payload[] }
```
命令集见 RPD §5.1.2(REGISTER / HEARTBEAT / DATA / CMD / LOG / FW_*)。

---

## 3. 端口模型

`port_def_t`(`routerd/include/config_store.h`)是端口的统一抽象。
当前支持类型(`config_store.h`):
- `tty` — UART / USB 虚拟串口
- `tcp_server` / `tcp_client`
- `udp`
- `ipc` — Unix socket

**新增端口类型时**,需改三处:
1. `routerd/src/config_store.c` `parse_ports()` 加 case
2. `routerd/src/port_manager.c` `port_open_single()` 和 `port_send()` 加 case
3. `routerd/src/reactor.c` 处理 fd 注册和事件分发

**遗留**: `port_send()` 对 `PORT_TCP_SERVER` 是占位实现(`routerd/src/port_manager.c:369-371`),阶段 0 任务 0.3 修复。

---

## 4. 插件机制

插件 = `.so`,`dlopen(RTLD_NOW)` 加载,通过构造函数 `__attribute__((constructor))` 自注册。
注册名规则:`"<plugin_name>.<handler_name>"`,与 `config.json.routes[].handler` 匹配。

### 隐性契约

- 插件 handler 当前签名:`int (*)(void* buf, int len)`,**返回新长度**
- 插件可以**就地修改 buf**,但**不能扩展超过 1024 字节**(`MAX_DATA` in `routerd/include/event.h`)
  - **当前 reactor 不校验返回值边界** → `routerd/src/reactor.c:241-252` 有栈溢出风险,阶段 0 任务 0.1
- 插件不可被 dlclose / 重新加载(目前)
- **插件 segfault 会拖垮 ez_router**(决议 R3 接受,由 hw watchdog 整板复位兜底)

参考: `plugin_loader.c:50-62`、示例 `plugins/filter.c:40-48`。

---

## 5. 数据流不变量

```
[fd] → reactor (epoll, routerd/src/reactor.c)
     → plugin handler (可选)
     → event_queue (生产者/消费者, routerd/src/event_queue.c, QSIZE=128)
     → router_core (查路由表, routerd/src/router_core.c)
     → port_send (routerd/src/port_manager.c)
     → 目标 fd
```

### 关键常量

| 常量 | 值 | 位置 |
|---|---|---|
| `MAX_PORTS` | 32 | `routerd/include/config_store.h` |
| `MAX_PLUGINS` | 32 | 同上 |
| `MAX_ROUTES` | 64 | 同上 |
| `QSIZE`(事件队列) | 128 | `routerd/src/event_queue.c` |
| `MAX_DATA`(单包) | 1024 | `routerd/include/event.h` |

阶段 1 协议层重构后,这些常量将统一在 `protocol.h`。

### 已知风险

- 事件队列**满了无溢出处理**,可能覆盖老数据(待确认,参考 `event_queue.c`)
- `g_config` / `g_port_table` 全局可变状态,IPC 线程改路由表时与 reactor 读路由表存在竞态(`reactor.c` 定义了 `reactor_lock/unlock` 但 epoll 主循环未使用)

---

## 6. 监管与生命周期(阶段 2 目标)

子程序部署形态:**进程外为主、插件为辅**(共享同一协议)。

软 watchdog:每个子程序最后心跳时间戳;主循环检查超时 → SIGTERM(宽限期)→ SIGKILL → 重启。
硬 watchdog:`/dev/watchdog0`,默认 timeout 44s,主线程 ioctl `WDIOC_KEEPALIVE`。

**未实现**,详见 RPD §5 阶段 2。

---

## 7. 测试机

- **板子**: Orange Pi 5 Plus(RK3588,aarch64)
- **OS**: Armbian,内核 6.1.115-vendor-rk35xx
- **SSH**: `root@192.168.31.6` 密码 `orangepi`
- **部署路径**: `/opt/ez_router/`
- **硬件 watchdog**: `/dev/watchdog0`,Synopsys DesignWare,默认 44s
  - **MAGICCLOSE=0**:不能用写 'V' 字符优雅停;close fd 行为依赖内核 `nowayout`,daemon 退出可能导致整板重启 — **O1 待阶段 2 实测**
- **安全**: 无要求,可随意 ssh/写文件/重启

---

## 8. 命名约定

- **项目名 / 仓库 / 文档**:`embedded-device-router`
- **二进制 / 符号 / 路径**:`ez_router`(无 `d` 后缀)
- **已清除的旧命名**:`ez_routerd`、`Forwarder`(仅 `介绍.md:5` 保留一处历史说明),见 commit `ce3ecad`
- **目录约定**:`routerd/` 目录名保留(改它影响 Makefile 引用路径,价值不高)

---

## 9. 工作流约束

- **无前向兼容**:现有代码量小,直接重构。**不要**为兼容性写 shim(决议 R5)
- **协议演进**:靠帧头 `version` 字段 bump,不在 payload 里塞兼容字段
- **删代码先 grep**:删之前先查 xref / Makefile 引用 / config.json 引用是否真的没用了
- **测试与实现等重**:RPD §3 第 3 条铁律。无测试不算完成
- **Bug 修复必须先有失败测试**:RPD §6.3.A
- **日志只走自管 log_sink,不双写 journald**(决议 R4)

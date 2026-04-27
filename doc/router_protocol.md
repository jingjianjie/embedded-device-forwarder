# ez_router 子程序协议规约

> **版本**: 1
> **状态**: 阶段 1 **完整闭合**。C-1 codec + C-2 registry/dispatcher/IPC 切换均已落地,旧 `ipc_header_t` 在 routerd 侧删除(SDK 副本待阶段 4 一并淘汰)。
> **范围**: 子程序 ↔ ez_router、上位机 ↔ ez_router 之间统一帧协议。
> **决议引用**: RPD §5.1.1 / R-2 / R-3 / R-5。
> **可执行真理**: `routerd/include/protocol.h` + `routerd/src/proto_codec.c` + `routerd/src/registry.c` + `routerd/src/proto_dispatcher.c` + `routerd/src/ipc_server.c` + 三个 test-as-doc(见 §8)。本文档若与代码冲突,以代码为准。

---

## 1. 帧格式

```
Offset  0       1       2       3       4       5       6       7       8       9      10      11
       +-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
       |     magic     |version|  cmd  |              seq              |          payload_len          |
       |   0xEB 0x7C   | 0x01  | 0x..  |          (LE u32)             |          (LE u32)             |
       +-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
       |                                          payload [payload_len bytes]                          |
       |                                       (任意字节,语义由 cmd 决定)                                |
       +-------------------------------------------------------------------------------------------------+
```

- **帧头固定 12 字节**(`PROTO_HDR_SIZE`),`__attribute__((packed))`,无 padding。
- **payload 上限** `PROTO_MAX_PAYLOAD = 65536`(64 KiB)。够 FW_CHUNK 固件分片;远低于 `uint32` 上限,避免 DoS。

## 2. 字节序与对齐

- **小端字节序(little-endian)**。target = RK3588 (LE),上位机 = x86-64 (LE),C# `BinaryReader` 默认 LE。codec 内部用 `memcpy` 整头拷贝(不按字段解引用),直接保留主机序。
- 若未来引入大端客户端,需在 codec 内增加 `htole*` 转换路径,届时 bump `PROTO_VERSION`。
- `__attribute__((packed))` 在某些 ARM 上对**字段级解引用**会报对齐警告,本实现规避方式 = `memcpy` 整头到栈本地变量后再读字段,见 `proto_codec.c::proto_decode`。

## 3. 命令码

每个 cmd 段:**语义 / payload 形状 / 谁是发起方**。详细 schema 由 `proto_dispatcher`(C-2)校验。

| 值 | 名 | 发起方 | payload 形状(概述) | C-2 实现状态 |
|---:|---|---|---|---|
| `0x01` | `PROTO_REGISTER` | 子程序 → router | 元信息 JSON(必填: `device_id` / `model` / `fw_version` / `build_date`;可选: `ports[].name`),见 RPD §5.1.2 | ✅ 已实现:`registry_register` |
| `0x02` | `PROTO_REGISTER_ACK` | router → 子程序 | `{"ok": true|false}`,seq 等于 REGISTER 帧 seq | ✅ 已实现:`proto_dispatcher.c::send_register_ack` |
| `0x03` | `PROTO_HEARTBEAT` | 双向 | 空 payload。心跳保活,不期待响应 | ✅ 已实现:`registry_update_heartbeat` |
| `0x10` | `PROTO_DATA` | 双向 | 端口数据原文 + 路由元信息(schema 阶段 2 定) | 🟡 stub:LOG_INFO,实现待阶段 2 |
| `0x11` | `PROTO_CMD` | 双向 | 命令调用与响应 | 🟡 stub:LOG_INFO,实现待阶段 2 |
| `0x20` | `PROTO_LOG` | 子程序 → router | 日志行 | 🟡 stub:LOG_INFO,实现待阶段 3 log_sink |
| `0x30` | `PROTO_FW_BEGIN` | 上位机 → 子程序 | 升级元信息 | 🟡 stub:LOG_WARN,实现待阶段 5 |
| `0x31` | `PROTO_FW_CHUNK` | 上位机 → 子程序 | `{offset, data}` 分片 | 🟡 stub,阶段 5 |
| `0x32` | `PROTO_FW_END` | 上位机 → 子程序 | `{checksum}` | 🟡 stub,阶段 5 |
| `0x33` | `PROTO_FW_QUERY` | 双向 | 查询固件版本 / 状态 | 🟡 stub,阶段 5 |

**未知 cmd / 保留区(`0x00` / `0x40-0xFF`)**:dispatcher 容错 LOG_WARN + 不断连(`proto_dispatcher.c` 默认分支)。新增 cmd 必须**同步改 4 处**:`protocol.h` 枚举 + 本文档表 + dispatcher 分支 + 测试矩阵。

## 4. `seq` 字段语义(R-3 决议钉死)

- **由发送方单调递增分配**,每个发送方持有独立计数器。
- **接收方仅用于 ACK / 响应帧的关联**:`REGISTER_ACK.seq == REGISTER.seq`、`CMD` 响应帧的 seq 与请求 seq 相同。
- **不**用于查重 / 幂等 / 排序保证。业务层有需要自己加幂等键到 payload。
- **发送方重启后从 0 重新计数**(无需持久化)— 接收方对此不可见,因为 SEQPACKET 连接断开会重新走 REGISTER 流程,旧 seq 自然失效。

## 5. 错误码(codec 返回值)

负值表示错误,正值或 0 表示成功的字节数。

| 码 | 名 | 时机 | caller 应做 |
|---:|---|---|---|
| `-1` | `PROTO_ERR_BUF_TOO_SMALL` | encode: 输出 buffer 容量 < `PROTO_HDR_SIZE + payload_len` | 扩大 out_buf 或拒绝发送 |
| `-2` | `PROTO_ERR_BAD_MAGIC` | decode: 帧头 magic ≠ `0xEB7C` | 视为流损坏,断开重连或重同步 |
| `-3` | `PROTO_ERR_BAD_VERSION` | decode: version ≠ 1 | 拒绝(R-5 无前向兼容);记录对端版本以便升级 |
| `-4` | `PROTO_ERR_PAYLOAD_TOO_LARGE` | encode/decode: `payload_len > PROTO_MAX_PAYLOAD` | 拒绝;视为对端 bug 或恶意 |
| `-5` | `PROTO_ERR_INCOMPLETE` | decode: in_len < 整帧 | **继续 read** 更多数据后重试。**不是错误,是流式协议的正常状态** |

**实现细节**(见 `proto_codec.c`):
- decode 优先级:`INCOMPLETE(头不全)` → `BAD_MAGIC` → `BAD_VERSION` → `PAYLOAD_TOO_LARGE` → `INCOMPLETE(payload 不全)` → 成功。
- 这意味着头部读全后才能区分 magic / version / 长度错误;头都没读够时不解释 magic。

## 6. 与旧 `ipc_header_t` 的不兼容声明(R-5)

| 项 | 旧 `ipc_header_t` | 新 `proto_frame_hdr_t` |
|---|---|---|
| 头长 | 12 字节 | 12 字节(巧合,字段不同) |
| magic | `uint32_t = 0xE7F12345` | `uint16_t = 0xEB7C` |
| cmd 宽度 | `uint16_t` | `uint8_t` |
| 序号字段 | 无 | `uint32_t seq` |
| 长度字段 | `uint32_t length` | `uint32_t payload_len` |
| reserved | `uint16_t` 占位 | 无 |

**已切换**(C-2):routerd 侧 `routerd/include/ipc_server.h` 与 `routerd/src/ipc_server.c` 已**完全删除**旧 `ipc_header_t` / `IPC_MAGIC` / `ez_router_link_t` / `ipc_payload_*` / `process_frame()` / `ipc_handle_frame()`。新 IPC 主循环走 `proto_decode → proto_dispatch`,见 `routerd/src/ipc_server.c::handle_client`(单连接独立 64KiB read buffer + INCOMPLETE 循环 read,契约 21)。

**SDK 副本**(`sdk/ez_router_sdk.h` 内的 `ipc_header_t`)未删除 — 阶段 4 SDK 重写时一并淘汰。届时 SDK 客户端连接 routerd 会因协议不识别被断连(R-5 一刀切),这是预期行为。

**字面字节差异**(均为 LE):
```
ipc_header_t  : 45 23 F1 E7 | XX XX | 00 00 | LL LL LL LL
proto_frame   : 7C EB | 01 | XX | SS SS SS SS | LL LL LL LL
```
首字节(`0x45` vs `0x7C`)即可一眼辨识,有助于排查混用。

## 7. 二进制示例:最小 REGISTER 帧

假设子程序发起注册,`seq=1`,payload 是 5 字节的 `{}` 占位 JSON `'{ABC}'`(实际会更长,这里只示意位布局):

```
Offset  Bytes              Meaning
------  ----------------   -----------------------------
0x00    7C EB              magic = 0xEB7C (LE)
0x02    01                 version = 1
0x03    01                 cmd = PROTO_REGISTER (0x01)
0x04    01 00 00 00        seq = 1 (LE u32)
0x08    05 00 00 00        payload_len = 5 (LE u32)
0x0C    7B 41 42 43 7D     payload = '{','A','B','C','}'
```

总长 17 字节(12 头 + 5 payload)。

## 8. 参考实现 / 测试

- 帧定义: `routerd/include/protocol.h`
- 编解码: `routerd/src/proto_codec.c`
- 注册表: `routerd/include/registry.h` + `routerd/src/registry.c`(32 槽,pthread_mutex 短临界区,JSON 解析锁外做)
- 分发器: `routerd/include/proto_dispatcher.h` + `routerd/src/proto_dispatcher.c`(REGISTER/HEARTBEAT 已实现,DATA/CMD/LOG/FW_* 占位)
- IPC 入口: `routerd/src/ipc_server.c`(每连接独立 64KiB read buffer + 流式 decode)
- 单测(test-as-doc,target aarch64 PASS):
  - `tests/unit/test_proto_codec.c` — 27/27 codec 行为
  - `tests/unit/test_registry.c` — 27/27 注册表行为(register/unregister/find/heartbeat/overflow/conflict/malformed)
  - `tests/unit/test_dispatcher.c` — 18/18 分发行为(ACK seq 关联、心跳更新、未知 cmd 容错)
- 集成冒烟: `tests/unit/smoke_ipc_client.c`(临时,target 上跑通 REGISTER + ACK + HEARTBEAT 完整链路)

## 9. 待解决

- ACK / 数据 write 当前是阻塞 socket。慢客户端会卡住 IPC 线程(reactor 不直接受影响,但 IPC 线程卡住后续连接的 dispatch 会延迟)。**阶段 2 起前**改非阻塞 socket + 写队列(契约 14)。
- fuzz 接入(R-9):afl-fuzz 喂任意字节流给 `proto_decode`,验证不 crash 不越界(阶段 1 收尾时跑 1h)。
- DATA / CMD / LOG / FW_* schema 由阶段 2/3/5 各自模块上线时填本文档。

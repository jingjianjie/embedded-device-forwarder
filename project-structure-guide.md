# project-structure-guide — 架构地图

> **目的**: 沉淀长探索的导航路径,避免每次重复跨文件追踪。
> **维护规则**: 见 `CLAUDE.md` §2。条目保持架构层级,**不写具体代码改动**。
> **格式**: 每条 = 核心发现 / 涉及文件 / 隐式依赖 / 推荐导航路径。

---

## E1. 数据流主路径(从 fd 到 fd)

**核心发现**: 一份数据从硬件端口读入到从另一个端口送出,要穿过 5 个模块,**每个模块都可能成为故障点**。

**涉及文件**:
- `routerd/src/reactor.c` — epoll 事件循环,fd 读
- `routerd/src/plugin_loader.c` — handler 注册表
- `routerd/src/event_queue.c` — 生产者-消费者环形队列(`QSIZE=128`)
- `routerd/src/router_core.c` — 查路由表
- `routerd/src/port_manager.c` — 写出

**隐式依赖**:
- 改 `MAX_DATA`(`event.h`)需要同时考虑 reactor 读缓冲、plugin 返回长度边界、event_msg_t 内嵌 data[]
- 改路由表 / 端口配置需要重启 daemon — 当前无运行时热加载
- `g_config` / `g_port_table` 是全局可变,IPC 线程改路由表时与 reactor 读路由表存在竞态(`reactor.c` 定义了 `reactor_lock/unlock` 但 epoll 主循环未使用)

**推荐导航**:
1. 从 `ez_router.c::main()` 看启动顺序 → 三个线程怎么组合
2. 跟一个 fd:`port_manager.c::port_open_single()` 注册 → `reactor.c::reactor_thread()` epoll 监听 → `router_core.c::router_core_handle()` 查路由
3. plugin 路径:`plugin_loader.c` 的注册表 + `reactor.c:241` 的调用点

---

## E2. 端口类型扩展点(三处必改)

**核心发现**: 添加新端口类型需要在**三个文件、三个 switch 块**里同步加 case,漏一处会得到"配置能解析但端口不工作"的诡异 bug。

**涉及文件**:
- `routerd/include/config_store.h` — `enum port_type_t`
- `routerd/src/config_store.c` — `parse_ports()` 解析 JSON
- `routerd/src/port_manager.c` — `port_open_single()` 打开 + `port_send()` 写出
- `routerd/src/reactor.c` — fd 注册和事件分发

**隐式依赖**:
- `port_send()` 对 `PORT_TCP_SERVER` 是占位实现(`port_manager.c:369-371`),阶段 0 任务 0.3 修复
- 新端口类型如果有"被动监听"特性(类似 tcp_server / ipc_server),还要在 `reactor.c::reactor_thread()` 里加 accept 分支

**推荐导航**:
1. 看现有的 `tty` 实现作为最简模板:`port_manager.c:31-99`
2. 看 `tcp_server` 实现理解监听-accept 拓展模式:`port_manager.c:105-133`、`reactor.c:166-188`
3. **依赖关系无虚表/多态**,纯 enum 调度的 union — 三处都要手动同步

---

## E3. 协议与 IPC(当前 vs 计划)

**核心发现**: 当前 IPC 协议(`ipc_header_t`)与未来子程序协议(`proto_frame_t`)是**两套**,前者将被废弃。新代码不要再扩展旧协议。

**涉及文件**:
- 当前: `routerd/src/ipc_server.c`、`routerd/include/ipc_server.h`、`sdk/ez_router_sdk.h`(SDK 走旧 IPC)
- 计划: `routerd/include/protocol.h`(阶段 1 待建),配套 `proto_codec.c`、`proto_dispatcher.c`、`registry.c`
- 协议规约文档: `doc/router_protocol.md`(目前空,阶段 1 落实)

**隐式依赖**:
- 旧 SDK `ezr_*` 接口都基于旧 IPC,阶段 1 要全部重写
- `examples/` 下的 client_example.c / sdk_test.c 也走旧 SDK

**推荐导航**:
- 计划路线见 `doc/RPD.md` §5 阶段 1
- 不要在 `ipc_server.c` 加新功能,直接在新协议层做

---

## E4. 测试机部署链路(dev → target)

**核心发现**: 测试机是 OrangePi 5 Plus,部署/测试需要通过 SSH 远程操作。无 sshpass 时用 Python paramiko 脚本(项目集成测试栈选择 pytest+paramiko 也是这个原因)。

**涉及文件**:
- `scripts/manage_ez_router.sh` — systemd 服务管理脚本(在 target 上跑)
- `scripts/deploy_target.py` — Python 部署 helper(已落地,RPD §6.5 兑现)。**注**: 用流式 SFTP 绕开 paramiko 4.0 sftp.put bug,见 `PROJECT_CONTEXT.MD` 条目 19
- `tests/integration/conftest.py` — RPD §6.5 计划新增,pytest fixture
- 全局 memory:`C:\Users\win\.claude\projects\F--013-device-forwarder-embedded-device-forwarder\` 是会话存储位置(对应 ez-router-engineer 的 inbox/jsonl 索引来源)

**隐式依赖**:
- 测试机 ssh 凭据是 `root/orangepi`(在 `doc/RPD.md` §6.1 与 `PROJECT_CONTEXT.MD` 都记录,**不要**在代码里硬编码)
- target 部署路径约定 `/opt/ez_router_test/`(测试用)和 `/opt/ez_router/`(生产),改这个会破坏 systemd unit
- 硬件 watchdog 隐性约束:`/dev/watchdog0` 可用,`CONFIG_WATCHDOG_NOWAYOUT not set`,daemon close 安全(R6 闭合)
- target 代理: `http_proxy=http://192.168.31.75:7890`(`/etc/environment`),`git pull` 走代理 OK

**推荐导航**:
1. SSH 探查命令模板见 `memory/change-log.md` 2026-04-27 RPD R1 验证那一段
2. 部署脚本: `scripts/deploy_target.py` exec/put/put_dir,已含 paramiko 4.0 bug workaround

---

## E5. `routerd/src/net_handler.c` 已删除(原死代码导航陷阱)

**核心发现**: 截至 commit `3a08516`,`net_handler.c` + `net_handler.h` 存在但**完全无效**:
- 文件名暗示"网络处理",但实际无任何调用方
- Makefile `SRCS` 未列入(隐式排除),所以全 build 干净
- 引用了已删除的类型(`event_msg_t.type` / `EVT_NET_IN`)— 单独编译会失败

**已在阶段 0.5 删除**(commit 待定)。

**导航教训**: 看到 `net_handler.c` / `uart_handler.c` / `forward.c` 这种以"职责名"命名的文件**先看 Makefile 是否引用**。Makefile lines 24-25 仍有 `# src/uart_handler.c` `# src/forward.c` 注释占位 — 它们也是死代码(同期废弃),保留行号是早期作者的"未来扩展点占位",但 grep 全仓发现都已无引用。如新人接手对这些感到疑惑,**先 grep 全仓再删,而不是先看名字**(`PROJECT_CONTEXT.MD` 条目 18)。

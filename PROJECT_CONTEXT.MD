# PROJECT_CONTEXT — embedded-device-router

> **版本**: `v1`
> **维护规则**: 见 `CLAUDE.md` §4。修改时自增版本号,新条目用最新版本号 `[LM_<v>]` 标记。
> **可写者**: 主 agent + 人类。**subagent 只读**(见 `CLAUDE.md` §10.1)。
> **侧重**: 改动背后的**意图/背景/隐性契约**,不记录易过时的具体细节(那些放 `git log` / `RPD` / 代码注释里)。

---

## 项目北极星

**1.[LM_1]** ez_router 是基础设施,不是业务逻辑。路由器**不解析协议、不理解数据、不做选片决策**。所有业务智能在子程序或插件里。子程序工程师**只写一个 main()**,其余(进程守护、心跳、重启、watchdog、日志落盘、TF 卡寿命管理、固件升级、与上位机的 TCP 连接)由 ez_router 包揽。**Why**: 让"无下位机开发"成为标准件,降低嵌入式接入门槛。

**2.[LM_1]** **协议层是地基**,任何上层模块(子程序监管 / 日志 / SDK / 固件升级)改动前**先确认协议契约**是否需要先动。当前协议帧定义计划落在 `routerd/include/protocol.h`(阶段 1 待建),临时 IPC `ipc_header_t` 已计划废弃。

---

## 命名与重构原则

**3.[LM_1]** **项目层面命名 = `embedded-device-router`,代码层面 = `ez_router`**(无 `d` 后缀)。`routerd/` 目录名保留(改它影响 Makefile 引用路径,价值不高 vs 收益)。**Why**: 历史上有 `ez_routerd` 与 `Forwarder` 旧名分散在多处,2026-04-27 commit `ce3ecad` 统一清理。

**4.[LM_1]** **无前向兼容**。现有代码量小,直接重构。**不要**为兼容性写 shim 或预留扩展字段。**Why**: 决议 R5,见 RPD §9。协议演进靠帧头 `version` 字段 bump 解决,不在 payload 里塞兼容字段。

---

## 隐性契约(代码里没写但必须遵守)

**5.[LM_1]** **插件 handler 当前签名 `int (*)(void* buf, int len)` 必须返回新长度,且不能扩展超过 `MAX_DATA = 1024`**(`routerd/include/event.h`)。当前 reactor 不校验返回值边界(`routerd/src/reactor.c:241-252`),返回 >1024 会栈溢出 — 阶段 0 任务 0.1 修复。**Why**: event_msg_t.data 固定 1024 字节,plugin 是数据流上的中间步骤,扩展边界由载体决定。

**6.[LM_1]** **新增端口类型必须改三处**:`config_store.c::parse_ports()` 加 case + `port_manager.c::port_open_single()/port_send()` 加 case + `reactor.c` 处理 fd 注册。漏改任一处会得到"配置能解析但端口不工作"的诡异 bug。**Why**: 端口抽象 `port_def_t` 是一个 enum 调度的 union 设计,无虚表,所以每个分支都要手动同步。

**7.[LM_1]** **插件 segfault 会拖垮 ez_router**,且当前没有也不会做隔离。运维侧的兜底是硬件 watchdog `/dev/watchdog0` 在 44 秒内整板复位重启。**Why**: 决议 R3。隔离插件需要进程化或沙箱,代价高于简单"崩了就重启"的策略。

**8.[LM_1]** **测试机 `/dev/watchdog0` 的 `MAGICCLOSE=0`**,意味着不能用写 'V' 字符优雅停。close fd 后行为取决于内核 `CONFIG_WATCHDOG_NOWAYOUT`。当前**保守假设**:daemon 一旦退出板子在 44s 内整板重启。运维约束写入 `doc/build_guide.md`,实测在阶段 2(O1 待解决)。**Why**: 测试机为 Orange Pi 5 Plus / RK3588 / Synopsys DesignWare watchdog,默认 timeout 44s。

---

## 工程纪律

**9.[LM_1]** **测试与实现等重**。RPD §3 第 3 条铁律:无测试不算完成。Bug 修复**必须先有失败测试**(RPD §6.3.A),再修。**Why**: 项目曾经经历过"代码无人懂"的状态(2026-04-27 之前),无测试加无文档让代码在演进中失去校验,加测试是逆向加固。

**10.[LM_1]** **删代码前先 grep**。删除任何看似无用的代码 / 文件 / 配置前,先扫 xref / Makefile 引用 / `config.json` 引用。看似多余的代码先假设它在避坑(Chesterton's Fence,`CLAUDE.md` §6.4)。**Why**: 项目早期残留较多,曾删除过被 Makefile 隐式引用但 IDE 看不出的文件。

**11.[LM_1]** **日志只走自管 `log_sink`,不双写 journald**。systemd unit 的 stderr 自动被 journald 收已足够诊断崩溃,无需主动配置。**Why**: 决议 R4。双写增加复杂度无收益。

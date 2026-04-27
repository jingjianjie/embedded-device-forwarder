# PROJECT_CONTEXT — embedded-device-router

> **版本**: `v5`
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

**5.[LM_3]** **插件 handler 签名 `int (*)(void* buf, int len)` 必须返回新长度,且不能扩展超过 `MAX_DATA = 1024`**(`routerd/include/event.h`)。**reactor 已 clamp 越界返回值**(`routerd/src/reactor.c:258-272`):`<0` → drop,`>MAX_DATA` → truncate + WARN。plugin 仍应自律,越界即代表实现错误。**Why**: event_msg_t.data 固定 1024 字节,plugin 是数据流上的中间步骤,扩展边界由载体决定。**契约固化**: `tests/unit/test_plugin_clamp.c` 是 test-as-doc(`CLAUDE.md` §6.5),clamp 规则改动须同步改测试模型。**修复**: HEAD,RPD 阶段 0 任务 0.1 ✅。

**6.[LM_1]** **新增端口类型必须改三处**:`config_store.c::parse_ports()` 加 case + `port_manager.c::port_open_single()/port_send()` 加 case + `reactor.c` 处理 fd 注册。漏改任一处会得到"配置能解析但端口不工作"的诡异 bug。**Why**: 端口抽象 `port_def_t` 是一个 enum 调度的 union 设计,无虚表,所以每个分支都要手动同步。

**7.[LM_1]** **插件 segfault 会拖垮 ez_router**,且当前没有也不会做隔离。运维侧的兜底是硬件 watchdog `/dev/watchdog0` 在 44 秒内整板复位重启。**Why**: 决议 R3。隔离插件需要进程化或沙箱,代价高于简单"崩了就重启"的策略。

**8.[LM_1]** **测试机 `/dev/watchdog0` 的 `MAGICCLOSE=0`**,意味着不能用写 'V' 字符优雅停。close fd 后行为取决于内核 `CONFIG_WATCHDOG_NOWAYOUT`。当前**保守假设**:daemon 一旦退出板子在 44s 内整板重启。运维约束写入 `doc/build_guide.md`,实测在阶段 2(O1 待解决)。**Why**: 测试机为 Orange Pi 5 Plus / RK3588 / Synopsys DesignWare watchdog,默认 timeout 44s。

---

## 工程纪律

**9.[LM_1]** **测试与实现等重**。RPD §3 第 3 条铁律:无测试不算完成。Bug 修复**必须先有失败测试**(RPD §6.3.A),再修。**Why**: 项目曾经经历过"代码无人懂"的状态(2026-04-27 之前),无测试加无文档让代码在演进中失去校验,加测试是逆向加固。

**10.[LM_1]** **删代码前先 grep**。删除任何看似无用的代码 / 文件 / 配置前,先扫 xref / Makefile 引用 / `config.json` 引用。看似多余的代码先假设它在避坑(Chesterton's Fence,`CLAUDE.md` §6.4)。**Why**: 项目早期残留较多,曾删除过被 Makefile 隐式引用但 IDE 看不出的文件。

**11.[LM_1]** **日志只走自管 `log_sink`,不双写 journald**。systemd unit 的 stderr 自动被 journald 收已足够诊断崩溃,无需主动配置。**Why**: 决议 R4。双写增加复杂度无收益。

---

## 协作约束(v2 新增)

**12.[LM_2]** **存在并行开发者**。2026-04-27 我们做仓库清理 + RPD + agent 基建期间,远端被推入 commit `5246de2`(Author: `Developer <developer@example.com>`),修了 RPD 阶段 0 中 0.4 + 顺手解决了 0.6 队列溢出 + 自带 4 项我们没列入的 bug 修复(NULL 解引用顺序、PORT_IPC_SERVER fallthrough、TCP listen 错误路径、read off-by-one)。**Why**: 项目原本被认为"没人懂",但实际上有人在并行修 bug。**How to apply**: (a) 工作开始前必 `git fetch` 并查 origin/main 是否有新 commit;(b) 委派 agent 任务前**先核对 RPD 阶段表的状态列**,不要重复劳动;(c) 长会话中也建议中途 fetch 一次,降低撞车概率;(d) 重要重构(如 rename / 大移动)开工前在 issue / chat 知会团队。

**13.[LM_2]** **rename + 远端并行修改的合并方式 = `git rebase origin/main`**。Git 的 rename 检测会自动把对方对旧文件名的修改 forward 到新文件,前提是行级别不重叠。本次 `ez_routerd.c → ez_router.c` 的 rename 与 Developer 对 `ez_routerd.c` 的 `unistd.h` / `return NULL` 改动通过 rebase 干净合并(commit `9b072ba` 之前)。**Why**: 决策时间 2026-04-27,作为后续遇到类似撞车的标准动作。**How to apply**: 不要 force push、不要 `--allow-unrelated-histories`,先 rebase,冲突时停下来手工解。

---

## 运行时不变量(v4 新增)

**14.[LM_5]** **reactor 线程不可被任何 send 路径阻塞**。reactor 是 epoll 单线程消费,如果 `port_send` / `queue_push` 阻塞它,整个 epoll 循环卡死,所有端口卡死。**Why**: 决议 R3 的兜底前提是"reactor 卡死后被 hw watchdog 复位",但 reactor 还在跑就还在喂狗 → watchdog 永不超时 → 整板假死。**How to apply**: (a) `queue_push` 已 timeout drop+WARN(阶段 0.12 闭合,timeout=100ms,见 `event_queue.h`);(b) **`port_send` 对 `PORT_TCP_SERVER` 当前是阻塞 socket 的 broadcast(阶段 0.3),仍违反本契约 — 阶段 1 传输层重写时改非阻塞 socket + EAGAIN 处理**;(c) 任何新引入的"等待"操作都要在 reactor 线程外做。**Source**: `doc/reviews/rpd-v1-2026-04-27.md` C-A。

**15.[LM_4]** **`/var/log` 在 target 上是 zram volatile**(armbian-ramlog 默认 50 MB,断电丢失)。**Why**: Armbian 默认配置,目的是减少 SD 卡写入。**How to apply**: ez_router 持久化日志写 **`/var/lib/ez_router/log/`**(SD 卡,但 mmcblk1 baseline 6w 上电只写 4.4 GB,寿命压力可接受);诊断日志走 stderr 让 systemd journald 自动收(也走 zram,断电丢失但运行时可读);**不要**把 `/var/log/ez_router.log` 当持久存储。

**16.[LM_4]** **协议帧 `seq` 字段是发送方递增计数**。接收方仅用于 ACK/响应帧关联(REGISTER ↔ REGISTER_ACK),**不**做查重 / 幂等 / 排序保证。发送方重启后从 0 复位。**Why**: R-3 决议(`doc/RPD.md` §9 / `doc/reviews/rpd-v1-2026-04-27.md` C-C)。R5 激进无兼容字段原则下,后期改 seq 语义比加字段还贵,所以现在就钉死。**How to apply**: 实现 `proto_codec.c` 时按本契约;业务层若需要幂等,自加幂等键到 payload。

**17.[LM_4]** **运行时热加载是 non-goal**。改 `config.json` / 路由表 / 端口配置 = 重启 daemon。**Why**: R-8 决议(RPD §2 第 6 条)。SDK 的 IPC 接口仅用于上位机控制平面(下发命令、查询状态),**不接受路由结构变更请求**。**How to apply**: 不要把 `ezr_set_routes` 之类的 API 重新引入;`config_store.c` 的解析路径不需要并发安全(只在启动时走一次)。

---

## 工程实践案例(v5 新增)

**18.[LM_5]** **删代码前必 `grep` 全仓**(`CLAUDE.md` §10 / §6.4 落实案例)。2026-04-28 阶段 0.5 收尾发现 `routerd/src/net_handler.c` + `routerd/include/net_handler.h` 是死代码:文件头自引用 + Makefile 未列入 SRCS + 无任何 `.c` 调用其符号 + 引用了已删除类型(`event_msg_t.type` / `EVT_NET_IN`)→ 导致单独编译会失败。已删除。**Why**: 文件名暗示"网络处理",但实际职责为零,新人会被误导。删除时必须满足三条:(a) 全仓 `grep` 仅自引用;(b) 任何 Makefile / build script 不列入;(c) 引用了已不存在的符号(强证据)。**How to apply**: 看到可疑死代码,先 grep 三遍再删 — 不要单凭 "Makefile 没列" 就删,可能是被预备的扩展点。

**19.[LM_5]** **paramiko 4.0 的 `sftp.put()` 在 Windows + Armbian 组合下偶发 ENOENT**。`scripts/deploy_target.py` 已用流式写绕开:`with open(local,"rb") as src, sftp.open(remote,"wb") as dst: dst.write(src.read())`。**Why**: 协议握手 / 路径规范化的版本兼容问题,paramiko 4.0 改了 SFTP 实现。**How to apply**: 任何 dev → target 文件传输都走 `scripts/deploy_target.py` 的 `put` / `put_dir`,不要用 `sftp.put()`。

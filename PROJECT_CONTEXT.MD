# PROJECT_CONTEXT — embedded-device-router

> **版本**: `v8`
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

**19.[LM_5]** **paramiko 4.0 的 `sftp.put()` 在 Windows + Armbian 组合下偶发 ENOENT**。`scripts/deploy_target.py` 已用流式写绕开:`with open(local,"rb") as src, sftp.open(remote,"wb") as dst: dst.write(src.read())`。**Why**: 协议握手 / 路径规范化的版本兼容问题,paramiko 4.0 改了 SFTP 实现。**How to apply**: 任何 dev → target 文件传输都走 `scripts/deploy_target.py` 的 `put` / `put_dir`,不要用 `sftp.put()`。**v6 补丁**: 流式 sftp.open 在某些路径仍偶发 ENOENT,有效 fallback 是 `ssh exec "base64 -d > /path"` + 客户端 base64 编码。等真的频繁触发时再内化进 `deploy_target.py`,目前手工 inline 已够用。

**v7 补丁**: (a) 多文件批量传 fallback 走 `tar + base64 over ssh stdin` 比逐文件 base64 更省时;(b) **paramiko `ssh exec` 启动后台 daemon 必须用 `setsid bash -c '...' < /dev/null > /dev/null 2>&1`**,否则 `recv_exit_status()` 会一直挂(channel 不干净退出)。任何在 ssh 内启动 ez_router 做集成测试的脚本要遵守这条。

**v8 补丁**: ssh exec 启动 daemon 后用 `pgrep -f "<cmdline>"` 拿 PID 时**必须 `| tail -1`,不是 `head -1`**。`setsid bash -c '...'` 会先创建一个父 bash 包装进程,真实 daemon 是它的子进程;按 PID 排序父在前子在后,`head -1` 拿到的是父 bash(干掉它子进程不会停),`tail -1` 拿到的才是真 daemon。阶段 2 D-1 smoke 编排时踩到。

---

## 协议层契约(v6 新增,阶段 1 C-1 落地)

**20.[LM_6]** **packed wire-format struct 必须用 `memcpy` 整头拷贝**,不通过结构指针按字段访问。**Why**: ARM (RK3588 / 嵌入式 ARM 普遍)对 packed struct 字段的 misaligned 访问可能触发 UB 或性能塌方;memcpy 是显式声明字节流意图的安全方式。**How to apply**: `proto_codec.c` 已示范(`memcpy(buf, hdr, PROTO_HDR_SIZE)`);后续 `proto_dispatcher.c` / SDK / `registry.c` 中**任何**接触 `proto_frame_hdr_t` 二进制流的地方,都按这套规则。**禁止**: `frame_ptr->cmd` 这类直接 cast + 解引用 buffer。

**21.[LM_6]** **`PROTO_ERR_INCOMPLETE` 不是真错误**,是流式协议的正常状态(继续 `read()` 凑够帧)。区别于 `BAD_MAGIC` / `BAD_VERSION` / `PAYLOAD_TOO_LARGE` 这些**致命**错误(应断连或丢弃整个连接的缓冲区)。**Why**: 阶段 1 C-2 起的 dispatcher 在调用 `proto_decode()` 时如果把所有负值统一处理(比如都断连),会在正常的"读了一半"场景下错误断连,造成假性故障。**How to apply**: dispatcher / SDK 必须对 `INCOMPLETE` 与其他错误码分别处理。`doc/router_protocol.md` 错误码表已显式标注。

---

## Registry / IPC 并发(v7 新增,阶段 1 C-2 落地)

**22.[LM_7]** **`registry_find_by_*` 借出内部指针,不是拷贝**。锁在返回前释放,**caller 不可在并发触发 `registry_unregister` 的情况下持指针**。当前 IPC 单线程消费场景下安全(同一 fd 的 register / find / unregister 都在同一线程串行),但**阶段 2 supervisor 上线时**(supervisor 线程 + IPC 线程并发)需要重新评估:若需长持元信息应**复制出来再用**,而不是持借用指针。**Why**: 短临界区是契约 14 的延伸(锁不可跨 IO / malloc / 业务逻辑),所以 registry API 不能在锁内做任何"长操作";代价就是返回的指针生命周期与 unregister 竞争。**How to apply**: 阶段 2 supervisor `iterate` 风格的回调内**只允许复制数据**(`memcpy(local_copy, entry, sizeof)`),`kill()` / 重启逻辑必须放到锁外做。

**23.[LM_7]** **业务层边界可以比协议层更紧,但不可超过**。例: `parse_register_json` 的 4096 字节 ≤ `PROTO_MAX_PAYLOAD = 65536`。**Why**: 协议层 limit 是"传输层 DoS 防御"(避免恶意客户端宣称巨 payload 让接收方分配巨 buffer),业务层 limit 是"业务语义合理性"(REGISTER 帧 4 KiB JSON 已绰绰有余,更大代表客户端有问题)。两层都需要,且业务层必须更紧。**How to apply**: 后续 cmd 实现时(LOG / FW_CHUNK / DATA),各自内部应有业务上限,且 ≤ `PROTO_MAX_PAYLOAD`;不要图省事直接信任协议层 limit。

**24.[LM_7]** **契约 14 的延伸:`dispatcher` / IPC 线程的 `write()` 也不可阻塞**。reactor 是最严的不可阻塞线程,但 IPC 线程虽然不喂 watchdog,仍是新连接 dispatch 的瓶颈;阻塞 `write()` 写慢客户端会让其他客户端的 REGISTER / HEARTBEAT 排队延迟。**Why**: C-2 当前 `proto_dispatcher::send_register_ack` 用阻塞 `write()`,文件头有 TODO 注释 — 这是已知的延伸违规,**阶段 2 supervisor 上线前必须修**(改非阻塞 socket + 写队列 / EAGAIN 处理)。**How to apply**: 阶段 2 第一项任务 = 把所有 IPC 写路径(ACK / 命令下发 / supervisor 主动通知)改非阻塞 + 上层重试或降级。

**v8 实现说明**(阶段 2 任务 2.0 落地,commit `c4408a7`): IPC 写非阻塞**用 `send(MSG_DONTWAIT | MSG_NOSIGNAL)` per-call,不是 `fcntl(fd, F_SETFL, O_NONBLOCK)`**。理由: `fcntl` 改 fd 属性是双向的 — recv 也会变非阻塞,IPC 线程的 `handle_client` 主循环会 busy loop。`MSG_DONTWAIT` 只对当次系统调用生效,保留 recv 阻塞语义不变。配合 EAGAIN 重试(8 次 × 1ms = 上限 8ms),短 ACK 一发即走,EAGAIN 8 次说明客户端真的拥塞 → 丢日志 + 后续 recv 自然识别断连。`MSG_NOSIGNAL` 防止对端先关时收到 SIGPIPE 把 daemon 干掉。**禁止**: 任何 IPC 写路径用 `fcntl O_NONBLOCK` + 普通 `write()` 的组合。

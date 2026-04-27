# 变更记录

> 只记录"为什么改",不记录"改了什么"(后者 git log 已有)。
> 时间倒序,最新在最上。

---

## 2026-04-27 — RPD v1 与 R1-R5 决议

**Why**: 没人懂当前代码了,需要落一份权威路线图。同时把"协议地基 / 子程序 daemon / 统一日志 / SDK / 固件升级"五件大事一次串起来,并把测试机硬件 watchdog 实测确认。

**关键决策**:
- **R1**: 测试机为 OP5+ / RK3588 / Armbian 6.1.115,`/dev/watchdog0` Synopsys DesignWare 实测可用,默认 44s。`MAGICCLOSE=0`,close 行为待阶段 2 实测(O1)
- **R2**: C# SDK 仅 `net8.0`,无 netstandard 兼容层
- **R3**: 插件 segfault 不隔离,由 hw watchdog 整板复位兜底重启,简化设计
- **R4**: 不双写 journald,只用自管 log_sink。systemd unit 的 stderr 自动被收已足够
- **R5**: 协议第一版激进设计,不预留兼容字段。后续靠帧头 `version` bump

**产物**: `doc/RPD.md`(431 行,涵盖路线图 / 测试策略 / 硬件协调清单)

**Commits**: `085472c`(R1-R5 落实)、之前的 `ce3ecad`(RPD v1 主体)

---

## 2026-04-27 — 仓库清理 + 命名统一

**Why**: 项目从早期 C# / .NET 方案演进到当前 C 实现的过程中,残留了大量过时文件(`src/DeviceForwarder.Daemon/`、`tests/DeviceForwarder.Tests/`、`.vs/DeviceForwarder/`、孤儿 PDF / yaml)。命名也不一致(`ez_routerd` vs `ez_router`,`Forwarder` vs `Router`),增加新人理解成本。

**关键决策**:
- 项目层面统一为 `embedded-device-router`,代码层面统一为 `ez_router`
- `routerd/` 目录名**保留**(改它影响 Makefile 引用路径,价值不高 vs 收益)
- `介绍.md` 整体重写(原内容描述的是已废弃的 C# / .NET 方案,完全失实)
- 唯一保留的 "Forwarder" 字样在 `介绍.md:5` 作历史说明

**Commit**: `ce3ecad`

---

## 历史(早期,无确切 commit 标注)

- 早期一段时间项目尝试用 C# / .NET 8 实现(目录代号 `DeviceForwarder.Daemon`),后被废弃
- 当前 C 实现从 `routerd/` 起步
- `port_manager.c` 是较新的模块(最近一次独立提交 `94351bf`)
- `reactor.c:94-148` 仍有大段被注释的旧代码待清理(阶段 0 任务 0.5)
- 早期有一次"网口/串口 5 路由测试"通过(commit `bbe1971`),但当时的测试方式未持久化为自动化

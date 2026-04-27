# 未决问题

> 每条问题写明:背景、决策点、阻塞了什么。
> 关闭时移到 `change-log.md`。

---

## O1 — watchdog 在 daemon 退出时是否会重启板子?

**背景**: 测试机 `/dev/watchdog0` 的 `MAGICCLOSE=0`,意味着不能用写 'V' 字符优雅停。close fd 后的行为取决于内核 `CONFIG_WATCHDOG_NOWAYOUT`。

**决策点**: 阶段 2 在测试机上验证:
1. `cat /sys/module/dw_wdt/parameters/nowayout`(若有)
2. 实测 close-fd 后是否在 44s 内复位

**阻塞**: 阶段 2 supervisor 的 daemon 退出策略 — 优雅关闭代码要不要尝试停 watchdog,还是直接接受"daemon 一旦退出板子在 44s 内整板重启"。

**临时立场**: 保守假设"daemon 退出 → 板子在 44s 内整板重启",作为运维约束写入 `doc/build_guide.md`。

**记录于**: `doc/RPD.md` §9。

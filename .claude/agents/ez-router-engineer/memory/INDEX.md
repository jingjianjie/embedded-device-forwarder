# memory 索引

> **工作启动时必读**。文件按读取重要性排序。

## Canonical(只读 / dream 时由本 agent 写)

| 文件 | 内容 | 最后更新 |
|---|---|---|
| `design-intent.md` | 项目设计意图、隐性契约、不变量 | 2026-04-27 |
| `change-log.md` | 重要变更的"为什么"(不是"是什么") | 2026-04-27 |
| `open-questions.md` | 当前未决问题 | 2026-04-27 |
| `conversations.md` | dream 后的浓缩时间线(尚未 dream 过) | — |

## 项目级权威文档(只读,§10.1 不可写)

| 文件 | 内容 |
|---|---|
| `../../../../CLAUDE.md` | 项目 agent 协作守则(主 agent / subagent / 编码原则) |
| `../../../../PROJECT_CONTEXT.MD` | 项目背景 / 隐性契约的公共版,版本号管理 |
| `../../../../project-structure-guide.md` | 长探索沉淀的架构地图与隐式依赖 |
| `../../../../doc/RPD.md` | 路线图:阶段 0-6 + 测试策略 + 硬件协调 |

## 工作流(可写)

| 路径 | 用途 | 由谁写 |
|---|---|---|
| `inbox/` | 每会话独立条目,等待 dream | 普通会话末尾 |
| `dreams/` | dream 浓缩输出 | dream 时本 agent |
| `.dream.lock` | dream 并发互斥锁 | dream 取/释放 |

---

## 状态速览

- **当前 inbox 待消化**: 1 条(`2026-04-27T16-30_2c535370.md`,本 agent 创建会话本身)
- **最近一次 dream**: 无
- **最近一次重要变更**: 2026-04-27 RPD v1 + 仓库清理改名 + agent 基建
- **当前最高优先级**: RPD 阶段 0 现状加固(plugin 越界、`strcpy` 改 `strncpy`、TCP_SERVER `port_send` 实现等 6 项)

## 项目权威文档简表

- 路线图: `doc/RPD.md`
- 协议规约: `doc/router_protocol.md`(空,阶段 1 落实)
- SDK API: `doc/sdk_api.md`(空,阶段 4 落实)
- 构建/部署: `doc/build_guide.md`(空)
- 入门: `README.md`、`介绍.md`

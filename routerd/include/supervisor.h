#ifndef EZ_ROUTER_SUPERVISOR_H
#define EZ_ROUTER_SUPERVISOR_H

// supervisor.h — 子程序监管(RPD 阶段 2,D-1 skeleton)
//
// 职责(D-1):
//   - fork + execvp 拉起子程序,记录 PID + 名字
//   - 周期检查 registry 中条目的 last_heartbeat_ms,超时 LOG_WARN
//
// 不职责(D-1):
//   - 重启策略 / 指数退避 / max_restarts(D-2)
//   - SIGCHLD 精细处理(D-2 起用 sigaction 拿 exit status,目前 SIG_IGN
//     让内核自动 reap)
//   - 注册表对接 / config.json 驱动 spawn(D-2)
//
// 与 registry 的关系(隐性契约):
//   supervisor 内部只维护"我 spawn 的子进程的 PID 表",注册表存的是子进程
//   通过 IPC 注册的元信息。两者通过**子程序自己声明的 device_id**关联 —
//   spawn 时 supervisor 不知道也不关心 device_id,只在 stale 检查时报告
//   registry 中 last_heartbeat_ms 超时的 device_id。要想关联到具体 PID
//   做 kill / restart,D-2 时再加映射(目前一个 supervisor 实例 N 个 PID,
//   一个 registry 实例 N 个 device_id,关联仅靠"运维约定 child name 应等于
//   子程序自己声明的 device_id")。
//
// 并发(契约 14 / U9):
//   supervisor_check_heartbeats_at 内部不直接锁 registry — 调用
//   registry_iterate 把 entry 复制到栈数组(锁内仅 memcpy,U9 inbox
//   2026-04-28T00-45),锁外再判超时 + LOG_WARN。这样 LOG 等慢操作不会
//   持 registry 锁。
//
// 测试:tests/unit/test_supervisor.c
//   - spawn /bin/sleep 验 PID > 0 + waitpid 收尸
//   - 注入伪 registry entry 验 stale 检出计数

#include <stdint.h>

// 子进程小表上限。与 registry 对齐(REGISTRY_MAX_SUBPROCS=32)避免不一致。
#define SUPERVISOR_MAX_CHILDREN   32

// 子程序名上限。与 PROTO 注册的 device_id 同尺度,但本字段是 supervisor
// 内部记账用,允许独立演进。
#define SUPERVISOR_NAME_LEN       64

// 初始化:清空内部小表 + 安装 SIGCHLD = SIG_IGN(让内核自动 reap)。
// 幂等,可重入。
void supervisor_init(void);

// 释放(进程退出前调用,可选)。
void supervisor_close(void);

// fork + execvp 拉起子程序。
//   name : 内部记账用(supervisor 自己看的标签),与 registry 无关
//   argv : NULL 结尾,argv[0] 为可执行路径(execvp 解析 PATH)
// 返回 PID(>0)成功;-1 失败(fork 失败 / 表满)。
// 注意:exec 失败由子进程 exit(127),父进程**不感知** — D-1 只能靠后续
// stale 心跳间接发现。D-2 切 sigaction + waitpid 后能拿 exit code。
int supervisor_spawn(const char* name, char* const argv[]);

// 心跳超时检查(可测变体,注入 now_ms)。
//   now_ms     : 当前时间(ms),通常 = registry_now_ms()
//   timeout_ms : 超时阈值(ms)
// 行为:遍历 registry,对 (now_ms - last_heartbeat_ms > timeout_ms) 的
//   条目 LOG_WARN。
// 返回检测到的 stale 条目数(0 表示全部健康)。
int supervisor_check_heartbeats_at(uint64_t now_ms, uint64_t timeout_ms);

// wrapper:now_ms = registry_now_ms()。生产路径用这个。
int supervisor_check_heartbeats(uint64_t timeout_ms);

// 测试辅助:返回当前活跃 child 数(内部 in_use 计数)。
int supervisor_child_count(void);

#endif // EZ_ROUTER_SUPERVISOR_H

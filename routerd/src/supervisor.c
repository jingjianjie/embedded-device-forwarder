// supervisor.c — 子程序监管实现(RPD 阶段 2 D-1 skeleton)
//
// D-1 范围:fork+execvp + 心跳超时检测,不做重启 / 退避。
// 详见 supervisor.h 文件头。
//
// 测试:tests/unit/test_supervisor.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include "supervisor.h"
#include "registry.h"
#include "log.h"

typedef struct {
    int      in_use;
    pid_t    pid;
    uint64_t spawn_ms;
    char     name[SUPERVISOR_NAME_LEN];
} sup_child_t;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static sup_child_t     g_children[SUPERVISOR_MAX_CHILDREN];
static int             g_inited = 0;

void supervisor_init(void)
{
    pthread_mutex_lock(&g_lock);
    if (!g_inited) {
        memset(g_children, 0, sizeof(g_children));
        // SIGCHLD = SIG_IGN 让内核自动 reap,避免僵尸。D-2 改 sigaction
        // 取 exit status 后实现 restart 策略。
        signal(SIGCHLD, SIG_IGN);
        g_inited = 1;
    }
    pthread_mutex_unlock(&g_lock);
}

void supervisor_close(void)
{
    // 锁静态初始化,无需 destroy;SIGCHLD handler 留待 main 退出
}

int supervisor_spawn(const char* name, char* const argv[])
{
    if (!g_inited) supervisor_init();
    if (!name || !argv || !argv[0]) {
        LOG_WARN("[sup] spawn invalid args\n");
        return -1;
    }

    pthread_mutex_lock(&g_lock);
    int slot = -1;
    for (int i = 0; i < SUPERVISOR_MAX_CHILDREN; i++) {
        if (!g_children[i].in_use) { slot = i; break; }
    }
    if (slot < 0) {
        pthread_mutex_unlock(&g_lock);
        LOG_WARN("[sup] child table full (%d)\n", SUPERVISOR_MAX_CHILDREN);
        return -1;
    }
    pthread_mutex_unlock(&g_lock);

    // fork 在锁外:fork 是较慢系统调用,持锁过程会阻塞 spawn 并发请求。
    // 占槽用"先选好 slot 再 fork"的乐观方式;并发 spawn 时多线程可能选到
    // 同一个 slot,但 fork 后在锁内最终化,看到冲突就放弃 slot 重选 —
    // D-1 只在主线程调用 supervisor_spawn,目前不会真冲突;D-2 若引入
    // 后台 supervisor 线程再加二阶段提交。
    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("[sup] fork failed: %s\n", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        // 子进程
        execvp(argv[0], argv);
        // exec 失败:子进程 exit 127(D-1 父进程不感知,见 .h 注释)
        _exit(127);
    }

    // 父进程
    pthread_mutex_lock(&g_lock);
    // 重新选 slot(防其他线程同时进来),但 D-1 只主线程调,理论 slot 仍有效
    if (g_children[slot].in_use) {
        // 真冲突:再找一个
        slot = -1;
        for (int i = 0; i < SUPERVISOR_MAX_CHILDREN; i++) {
            if (!g_children[i].in_use) { slot = i; break; }
        }
        if (slot < 0) {
            pthread_mutex_unlock(&g_lock);
            // 子进程已起来但表满 — 罕见;杀掉它
            kill(pid, SIGKILL);
            LOG_ERROR("[sup] race: child table full after fork, killed pid=%d\n", pid);
            return -1;
        }
    }
    g_children[slot].in_use   = 1;
    g_children[slot].pid      = pid;
    g_children[slot].spawn_ms = registry_now_ms();
    strncpy(g_children[slot].name, name, SUPERVISOR_NAME_LEN - 1);
    g_children[slot].name[SUPERVISOR_NAME_LEN - 1] = '\0';
    pthread_mutex_unlock(&g_lock);

    LOG_INFO("[sup] spawned pid=%d name=%s slot=%d\n", pid, name, slot);
    return pid;
}

// U9: registry_iterate 内只 memcpy,锁外做超时判断与 LOG。
// 用 user 指针传 (entries[], count, cap) 方便 cb 写。
typedef struct {
    subproc_entry_t entries[REGISTRY_MAX_SUBPROCS];
    int             count;
} stale_collect_ctx_t;

static void collect_cb(const subproc_entry_t* e, void* user)
{
    stale_collect_ctx_t* ctx = (stale_collect_ctx_t*)user;
    if (ctx->count < REGISTRY_MAX_SUBPROCS) {
        // 锁内只做 memcpy(U9 inbox 2026-04-28T00-45)
        ctx->entries[ctx->count++] = *e;
    }
}

int supervisor_check_heartbeats_at(uint64_t now_ms, uint64_t timeout_ms)
{
    stale_collect_ctx_t ctx = {.count = 0};
    registry_iterate(collect_cb, &ctx);

    // 锁外:扫描复制出来的快照判超时 + LOG
    int stale = 0;
    for (int i = 0; i < ctx.count; i++) {
        const subproc_entry_t* e = &ctx.entries[i];
        // now_ms 可能小于 last_heartbeat_ms(测试时注入)→ 等价于"未超时"
        if (now_ms <= e->last_heartbeat_ms) continue;
        uint64_t age = now_ms - e->last_heartbeat_ms;
        if (age > timeout_ms) {
            LOG_WARN("[sup] stale heartbeat: device_id=%s fd=%d age=%lums>%lums\n",
                     e->device_id, e->client_fd,
                     (unsigned long)age, (unsigned long)timeout_ms);
            stale++;
        }
    }
    return stale;
}

int supervisor_check_heartbeats(uint64_t timeout_ms)
{
    return supervisor_check_heartbeats_at(registry_now_ms(), timeout_ms);
}

int supervisor_child_count(void)
{
    if (!g_inited) return 0;
    pthread_mutex_lock(&g_lock);
    int n = 0;
    for (int i = 0; i < SUPERVISOR_MAX_CHILDREN; i++)
        if (g_children[i].in_use) n++;
    pthread_mutex_unlock(&g_lock);
    return n;
}

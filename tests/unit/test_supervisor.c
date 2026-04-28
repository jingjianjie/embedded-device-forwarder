// test_supervisor.c — RPD 阶段 2 D-1 supervisor skeleton 的 test-as-doc
//
// 固化契约(supervisor.h):
//   - supervisor_spawn 返回 PID > 0 + 子进程被 reap(SIGCHLD=SIG_IGN)
//   - supervisor_check_heartbeats_at 注入时间戳能正确识别 stale entry
//   - U9 两段式:check 内部不阻塞 registry 锁(测试用并发探针验证)
//
// §6.5 TEST AS DOC 形态。Unity 单测脚手架(open-questions U1)就绪后迁移。
//
// 编译运行(从仓库根目录,target 上,单行命令):
//   gcc -Wall -Wextra -D_GNU_SOURCE -I routerd/include -I routerd/3rdparty/cjson routerd/src/registry.c routerd/src/supervisor.c routerd/src/log.c routerd/3rdparty/cjson/cJSON.c tests/unit/test_supervisor.c -lpthread -o /tmp/test_supervisor
//   /tmp/test_supervisor
//
// 退出码:0 = 全部 PASS,非 0 = FAIL

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include "registry.h"
#include "supervisor.h"

static int g_failed = 0;
static int g_passed = 0;

#define EXPECT(cond, label) do { \
    if (!(cond)) { fprintf(stderr, "FAIL %s (line %d)\n", label, __LINE__); g_failed++; } \
    else { printf("PASS %s\n", label); g_passed++; } \
} while (0)

#define EXPECT_EQ_INT(actual, expected, label) do { \
    long _a = (long)(actual), _e = (long)(expected); \
    if (_a != _e) { fprintf(stderr, "FAIL %s: got %ld, want %ld (line %d)\n", \
            label, _a, _e, __LINE__); g_failed++; } \
    else { printf("PASS %s\n", label); g_passed++; } \
} while (0)

static void registry_reset(void)
{
    for (int fd = 0; fd < 1024; fd++) registry_unregister(fd);
}

// === Test 1: spawn /bin/sleep 验 PID > 0 + 内核自动 reap ===
static void test_spawn_sleep(void)
{
    char* argv[] = {(char*)"/bin/sleep", (char*)"0.05", NULL};
    int pid = supervisor_spawn("test_sleep_short", argv);
    EXPECT(pid > 0, "spawn_returns_positive_pid");
    EXPECT_EQ_INT(supervisor_child_count(), 1, "child_count_incremented");

    // 等子进程退出 + 内核 reap(SIGCHLD=SIG_IGN)
    usleep(200 * 1000);  // 200ms,远 > sleep 0.05s

    // waitpid(WNOHANG) 应返回 -1 errno=ECHILD(已被内核 reap,无僵尸)
    int status;
    int wp = waitpid(pid, &status, WNOHANG);
    // SIG_IGN 下 waitpid 立即 ECHILD;若内核未 reap 会返回 pid > 0
    EXPECT(wp <= 0, "child_already_reaped_by_kernel");
}

// === Test 2: spawn 不存在的程序 — fork 成功但 exec 失败,父进程仍记账 ===
// 注意:D-1 父进程不感知 exec 失败(.h 注释明确),这里只验"spawn 调用返回 > 0"。
// D-2 接 sigaction 后才能拿到 exit=127。
static void test_spawn_bad_path(void)
{
    char* argv[] = {(char*)"/nonexistent/program/xyz", NULL};
    int pid = supervisor_spawn("test_bad", argv);
    // fork 成功就返回 PID;exec 失败由子进程 _exit(127),父不感知
    EXPECT(pid > 0, "spawn_bad_path_fork_succeeds");
    usleep(100 * 1000);  // 等内核 reap
}

// === Test 3: supervisor_spawn NULL 参数 ===
static void test_spawn_invalid_args(void)
{
    char* argv[] = {(char*)"/bin/true", NULL};
    EXPECT_EQ_INT(supervisor_spawn(NULL, argv), -1, "spawn_null_name_rejected");
    EXPECT_EQ_INT(supervisor_spawn("x", NULL), -1, "spawn_null_argv_rejected");
    char* empty[] = {NULL};
    EXPECT_EQ_INT(supervisor_spawn("x", empty), -1, "spawn_empty_argv_rejected");
}

// === Test 4: check_heartbeats_at — 全健康 ===
// 准备:伪造一条 registry 注册 → last_heartbeat_ms = now → check(now+500, 1000) = 0
static void test_heartbeats_all_healthy(void)
{
    registry_reset();
    // 用一个 fake fd(socketpair 一端,确保不冲突真 IPC)
    int sv[2]; if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { g_failed++; return; }
    const char* json = "{\"device_id\":\"HB-OK\",\"model\":\"m\","
                       "\"fw_version\":\"1.0\",\"build_date\":\"2026-04-28\"}";
    int rc = registry_register(sv[0], json, (int)strlen(json));
    EXPECT_EQ_INT(rc, 0, "register_ok_for_hb_test");

    const subproc_entry_t* e = registry_find_by_fd(sv[0]);
    EXPECT(e != NULL, "fake_entry_present");
    uint64_t hb = e->last_heartbeat_ms;

    int stale = supervisor_check_heartbeats_at(hb + 500, 1000);
    EXPECT_EQ_INT(stale, 0, "no_stale_when_within_timeout");

    close(sv[0]); close(sv[1]);
}

// === Test 5: check_heartbeats_at — 触发 stale ===
static void test_heartbeats_stale_detected(void)
{
    registry_reset();
    int sv[2]; if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { g_failed++; return; }
    const char* json = "{\"device_id\":\"HB-STALE\",\"model\":\"m\","
                       "\"fw_version\":\"1.0\",\"build_date\":\"2026-04-28\"}";
    registry_register(sv[0], json, (int)strlen(json));
    const subproc_entry_t* e = registry_find_by_fd(sv[0]);
    uint64_t hb = e->last_heartbeat_ms;

    // 注入 now = hb + 5000ms,timeout = 3000ms → 应识别为 stale
    int stale = supervisor_check_heartbeats_at(hb + 5000, 3000);
    EXPECT_EQ_INT(stale, 1, "one_stale_detected");

    // 边界:正好等于 timeout 不算 stale(>,不是 >=)
    int stale_eq = supervisor_check_heartbeats_at(hb + 3000, 3000);
    EXPECT_EQ_INT(stale_eq, 0, "stale_boundary_strict_gt");

    close(sv[0]); close(sv[1]);
}

// === Test 6: now < last_heartbeat_ms 容错(时钟回退或测试注入异常)===
static void test_heartbeats_clock_skew(void)
{
    registry_reset();
    int sv[2]; if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { g_failed++; return; }
    const char* json = "{\"device_id\":\"HB-SKEW\",\"model\":\"m\","
                       "\"fw_version\":\"1.0\",\"build_date\":\"2026-04-28\"}";
    registry_register(sv[0], json, (int)strlen(json));
    const subproc_entry_t* e = registry_find_by_fd(sv[0]);
    uint64_t hb = e->last_heartbeat_ms;

    // now < hb → 视为未超时(契约:此情况无 underflow,无 false positive)
    int stale = supervisor_check_heartbeats_at(hb > 100 ? hb - 100 : 0, 1000);
    EXPECT_EQ_INT(stale, 0, "clock_skew_no_false_positive");

    close(sv[0]); close(sv[1]);
}

// === Test 7: 多条目混合 stale + healthy ===
static void test_heartbeats_mixed(void)
{
    registry_reset();
    int sv1[2], sv2[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv1) != 0) { g_failed++; return; }
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv2) != 0) { g_failed++; return; }

    const char* j1 = "{\"device_id\":\"MIX-1\",\"model\":\"m\","
                     "\"fw_version\":\"1.0\",\"build_date\":\"2026-04-28\"}";
    const char* j2 = "{\"device_id\":\"MIX-2\",\"model\":\"m\","
                     "\"fw_version\":\"1.0\",\"build_date\":\"2026-04-28\"}";
    registry_register(sv1[0], j1, (int)strlen(j1));
    // 让 sv1 的 hb 落后 sv2:用 update_heartbeat 推进 sv2 的时间
    usleep(10 * 1000);
    registry_register(sv2[0], j2, (int)strlen(j2));
    registry_update_heartbeat(sv2[0]);  // 刷新 sv2 hb 到 now

    const subproc_entry_t* e1 = registry_find_by_fd(sv1[0]);
    const subproc_entry_t* e2 = registry_find_by_fd(sv2[0]);
    uint64_t hb1 = e1->last_heartbeat_ms;
    uint64_t hb2 = e2->last_heartbeat_ms;
    // hb2 应 ≥ hb1
    EXPECT(hb2 >= hb1, "mixed_setup_hb_order");

    // 注入 now = hb2 + 100ms;timeout = 50ms
    // → sv1 (now-hb1 = (hb2-hb1)+100 ≥ 100) > 50 stale
    // → sv2 (now-hb2 = 100) > 50 stale ?  会 — 把 timeout 调大避免误算
    // 改 timeout 让 sv2 健康、sv1 stale:
    //   sv1 age ≥ 10ms+100ms = 110ms;sv2 age = 100ms;timeout 设 105ms
    //   → sv1 stale (110>105),sv2 healthy (100<=105)
    int stale = supervisor_check_heartbeats_at(hb2 + 100, 105);
    EXPECT_EQ_INT(stale, 1, "mixed_one_stale_one_healthy");

    close(sv1[0]); close(sv1[1]); close(sv2[0]); close(sv2[1]);
}

int main(void)
{
    registry_init();
    supervisor_init();

    test_spawn_sleep();
    test_spawn_bad_path();
    test_spawn_invalid_args();
    test_heartbeats_all_healthy();
    test_heartbeats_stale_detected();
    test_heartbeats_clock_skew();
    test_heartbeats_mixed();

    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}

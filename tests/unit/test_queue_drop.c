// .tmp_test_queue_drop.c — RPD 阶段 0.12 / R-1 的 test-as-doc(临时形态)
//
// 不进 tests/(subagent §10.1 限制)。等主 agent 决定后挪到 tests/unit/。
//
// 固化契约(PROJECT_CONTEXT v4 §14):
//   reactor 线程不可被任何 send 路径阻塞。
//   queue_push 在队列满时最多 timed_wait QUEUE_PUSH_TIMEOUT_MS,
//   超时返回 -1 + WARN + drop_count++,reactor 永不卡死。
//
/* 编译运行(在 target 上):
     cd /opt/ez_router_test
     gcc -Wall -Wextra -pthread -I routerd/include -I routerd/src \
         routerd/src/event_queue.c routerd/src/log.c \
         /tmp/test_queue_drop.c -o /tmp/test_queue_drop
     /tmp/test_queue_drop
*/
//
// 退出码:0 = PASS,非 0 = FAIL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "event_queue.h"
#include "log.h"

#define QSIZE 128  // 必须与 event_queue.c 内一致

static int g_failed = 0;
#define EXPECT_EQ(actual, expected, label) do { \
    long _a = (long)(actual), _e = (long)(expected); \
    if (_a != _e) { \
        fprintf(stderr, "FAIL %s: got %ld, want %ld (line %d)\n", label, _a, _e, __LINE__); \
        g_failed++; \
    } else { printf("PASS %s\n", label); } \
} while (0)

static long elapsed_ms(struct timespec a, struct timespec b)
{
    return (b.tv_sec - a.tv_sec) * 1000L + (b.tv_nsec - a.tv_nsec) / 1000000L;
}

int main(void) {
    log_init(0, LOG_LEVEL_WARN);  // 抑制 INFO/DEBUG 噪音
    queue_init();

    event_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    strcpy(msg.dst, "TEST_DST");
    msg.len = 4;
    memcpy(msg.data, "ABCD", 4);

    // ---- Case 1: 单次正常 push/pop,返回 0 ----
    EXPECT_EQ(queue_push(&msg), 0, "case1: normal push returns 0");
    event_msg_t got;
    queue_pop(&got);
    EXPECT_EQ(got.len, 4, "case1: pop len matches");
    EXPECT_EQ(memcmp(got.data, "ABCD", 4), 0, "case1: pop payload matches");

    // ---- Case 2: 灌满队列(QSIZE-1 = 127 条)前都成功 ----
    queue_init();
    for (int i = 0; i < QSIZE - 1; i++) {
        if (queue_push(&msg) != 0) {
            fprintf(stderr, "FAIL case2: push %d unexpectedly failed\n", i);
            g_failed++;
            break;
        }
    }
    printf("PASS case2: filled %d entries without timeout\n", QSIZE - 1);

    // ---- Case 3: 队列满后第 128 次 push 应在 ~100ms 内 timeout drop ----
    unsigned long drops_before = queue_get_drop_count();
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int rc = queue_push(&msg);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long ms = elapsed_ms(t0, t1);
    EXPECT_EQ(rc, -1, "case3: full-queue push returns -1");
    if (ms < 80 || ms > 250) {
        fprintf(stderr, "FAIL case3: timeout took %ld ms, expected ~100ms (80-250 tolerance)\n", ms);
        g_failed++;
    } else {
        printf("PASS case3: timeout in %ld ms (in [80,250])\n", ms);
    }
    EXPECT_EQ(queue_get_drop_count() - drops_before, 1, "case3: drop_count incremented by 1");

    // ---- Case 4: 多次满 push 累计 drop_count ----
    drops_before = queue_get_drop_count();
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(queue_push(&msg), -1, "case4: full push returns -1");
    }
    EXPECT_EQ(queue_get_drop_count() - drops_before, 3, "case4: drop_count incremented by 3");

    // ---- Case 5: pop 释放后 push 立即 ok(不 timeout) ----
    queue_pop(&got);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    rc = queue_push(&msg);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ms = elapsed_ms(t0, t1);
    EXPECT_EQ(rc, 0, "case5: post-pop push returns 0");
    if (ms > 20) {
        fprintf(stderr, "FAIL case5: push took %ld ms, expected <20ms\n", ms);
        g_failed++;
    } else {
        printf("PASS case5: push completed in %ld ms\n", ms);
    }

    if (g_failed) {
        fprintf(stderr, "\n%d FAILED\n", g_failed);
        return 1;
    }
    printf("\nAll tests passed (QUEUE_PUSH_TIMEOUT_MS=%d, QSIZE=%d)\n",
           QUEUE_PUSH_TIMEOUT_MS, QSIZE);
    return 0;
}

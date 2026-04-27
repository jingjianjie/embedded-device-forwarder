// test_plugin_clamp.c — RPD 阶段 0 任务 0.1 的 test-as-doc
//
// 固化契约(PROJECT_CONTEXT.MD 条目 5 / design-intent.md §4):
//   plugin handler 返回长度必须被 reactor 夹到 [0, MAX_DATA]。
//   <0   → 丢弃本路由(continue)
//   >MAX → 截断为 MAX_DATA
//   合法 → 透传
//
// 这个文件是 §6.5 TEST AS DOC 形态:用最小复现样例固化逻辑切面,
// 等 Unity 单测脚手架就绪(open-questions.md U1)后迁移过去。
//
// 编译运行:
//   gcc -Wall -Wextra -I ../../routerd/include tests/unit/test_plugin_clamp.c -o /tmp/test_plugin_clamp
//   /tmp/test_plugin_clamp
//
// 退出码:0 = 全部 PASS,非 0 = FAIL

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "event.h"  // MAX_DATA

// --- 待测逻辑(从 reactor.c 抽取的纯函数版,行为与生产代码逐行对应)---
//
// 返回值约定:
//   0  → drop(对应 reactor 的 `continue`,不入队)
//   >0 → 入队的有效长度(已被夹到 [1, MAX_DATA])
//
// 这是 reactor.c clamp 块的等价模型,任何对生产代码的修改都应该
// 同步更新这里,否则该测试会变成"测了模型但没测代码"。
static int clamp_handler_return(int handler_ret) {
    if (handler_ret < 0) return 0;          // drop
    if (handler_ret > MAX_DATA) return MAX_DATA;
    return handler_ret;
}

// --- 测试框架(零依赖) ---
static int g_failed = 0;
#define EXPECT_EQ(actual, expected, label) do { \
    int _a = (actual), _e = (expected); \
    if (_a != _e) { \
        fprintf(stderr, "FAIL %s: got %d, want %d (line %d)\n", \
                label, _a, _e, __LINE__); \
        g_failed++; \
    } else { \
        printf("PASS %s\n", label); \
    } \
} while (0)

int main(void) {
    // 边界 / 越界 / 合法
    EXPECT_EQ(clamp_handler_return(-1),            0,         "negative -> drop");
    EXPECT_EQ(clamp_handler_return(-9999),         0,         "very_negative -> drop");
    EXPECT_EQ(clamp_handler_return(0),             0,         "zero -> drop_or_empty");
    EXPECT_EQ(clamp_handler_return(1),             1,         "one -> passthrough");
    EXPECT_EQ(clamp_handler_return(512),           512,       "midrange -> passthrough");
    EXPECT_EQ(clamp_handler_return(MAX_DATA),      MAX_DATA,  "exact_max -> passthrough");
    EXPECT_EQ(clamp_handler_return(MAX_DATA + 1),  MAX_DATA,  "max_plus_one -> truncated");
    EXPECT_EQ(clamp_handler_return(99999),         MAX_DATA,  "huge -> truncated");
    EXPECT_EQ(clamp_handler_return(0x7fffffff),    MAX_DATA,  "INT_MAX -> truncated");

    if (g_failed) {
        fprintf(stderr, "\n%d FAILED\n", g_failed);
        return 1;
    }
    printf("\nAll tests passed (MAX_DATA=%d)\n", MAX_DATA);
    return 0;
}

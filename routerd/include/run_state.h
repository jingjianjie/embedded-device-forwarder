#ifndef RUN_STATE_H
#define RUN_STATE_H

#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =========================
// 运行状态控制模块
// =========================

// 全局运行状态变量
extern volatile atomic_int g_running;

// 初始化：注册信号处理函数
void run_state_init(void);

// 主动停止运行（外部调用）
void run_state_stop(void);

// 查询当前是否仍在运行
bool run_state_is_running(void);

// 信号处理函数（内部使用）
void run_state_handle_signal(int sig);

#ifdef __cplusplus
}
#endif

#endif // RUN_STATE_H

#include "run_state.h"
#include <stdio.h>
#include <unistd.h>

volatile atomic_int g_running = 1;  // 默认运行状态

// 信号处理函数
void run_state_handle_signal(int sig)
{
    if (sig == SIGTERM ) {
        g_running = 0;
        printf("[signal] caught signal %d, exiting...\n", sig);
        fflush(stdout);
    }
}

// 初始化信号机制
void run_state_init(void)
{
    // signal(SIGINT,  run_state_handle_signal);
    signal(SIGTERM, run_state_handle_signal);
    // signal(SIGQUIT, run_state_handle_signal);
    printf("[run_state] signal handlers installed\n");
}

// 外部主动停止（用于内部控制，如IPC退出命令）
void run_state_stop(void)
{
    g_running = 0;
    printf("[run_state] stop requested\n");
}

// 查询运行状态
bool run_state_is_running(void)
{
    return atomic_load(&g_running);
}

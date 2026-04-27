#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include "event.h"

// queue_push 超时阈值。
// 不变量(PROJECT_CONTEXT v4 §14):reactor 线程不可被任何 send 路径阻塞。
// 100ms 比 watchdog 44s 兜底窗口短两个数量级,慢消费者下及时降级,避免 reactor 卡死。
#define QUEUE_PUSH_TIMEOUT_MS 100

void queue_init(void);

// 入队;返回 0 = 成功,-1 = timeout drop(队列持续满 QUEUE_PUSH_TIMEOUT_MS)。
// caller 当前可忽略返回值;计数器由 queue_get_drop_count() 暴露,
// 阶段 1 STATS 帧落地时(open-questions U3)会用到。
int  queue_push(event_msg_t* msg);

void queue_pop(event_msg_t* msg);

// 累计 timeout drop 计数,monotonically 增长。
unsigned long queue_get_drop_count(void);

#endif

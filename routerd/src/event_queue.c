#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include "event_queue.h"
#include "log.h"

#define QSIZE 128

static event_msg_t queue[QSIZE];
static int head = 0, tail = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond_not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  cond_not_full  = PTHREAD_COND_INITIALIZER;

static unsigned long g_drop_count = 0;  // 仅在 lock 下读写

void queue_init(void)
{
    head = tail = 0;
    g_drop_count = 0;
}

// R-1 解法(RPD 阶段 0.12):reactor 不可阻塞契约。
// 队列满时最多 timed_wait QUEUE_PUSH_TIMEOUT_MS,超时则 drop+WARN。
// 慢消费者掉包合理,reactor 永不阻塞,hw watchdog 兜底前提保住。
int queue_push(event_msg_t* msg)
{
    pthread_mutex_lock(&lock);

    if (((tail + 1) % QSIZE) == head) {
        // 队列满,等到不满或超时
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec  += QUEUE_PUSH_TIMEOUT_MS / 1000;
        deadline.tv_nsec += (QUEUE_PUSH_TIMEOUT_MS % 1000) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec  += 1;
            deadline.tv_nsec -= 1000000000L;
        }

        while (((tail + 1) % QSIZE) == head) {
            int rc = pthread_cond_timedwait(&cond_not_full, &lock, &deadline);
            if (rc == ETIMEDOUT) {
                g_drop_count++;
                unsigned long total = g_drop_count;
                pthread_mutex_unlock(&lock);
                LOG_WARN("[queue] push timeout %dms, drop dst=%s len=%d total_drops=%lu\n",
                         QUEUE_PUSH_TIMEOUT_MS, msg->dst, msg->len, total);
                return -1;
            }
            // rc==0 → 被 signal 唤醒;循环条件重检
        }
    }

    queue[tail] = *msg;
    tail = (tail + 1) % QSIZE;

    pthread_cond_signal(&cond_not_empty);
    pthread_mutex_unlock(&lock);
    return 0;
}

void queue_pop(event_msg_t* msg)
{
    pthread_mutex_lock(&lock);

    while (head == tail)
        pthread_cond_wait(&cond_not_empty, &lock);

    *msg = queue[head];
    head = (head + 1) % QSIZE;

    // 通知生产者队列不再满
    pthread_cond_signal(&cond_not_full);
    pthread_mutex_unlock(&lock);
}

unsigned long queue_get_drop_count(void)
{
    unsigned long c;
    pthread_mutex_lock(&lock);
    c = g_drop_count;
    pthread_mutex_unlock(&lock);
    return c;
}

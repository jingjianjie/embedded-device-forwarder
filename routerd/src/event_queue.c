#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include "event_queue.h"

#define QSIZE 128

static event_msg_t queue[QSIZE];
static int head = 0, tail = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond_not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  cond_not_full  = PTHREAD_COND_INITIALIZER;

void queue_init(void)
{
    head = tail = 0;
}

void queue_push(event_msg_t* msg)
{
    pthread_mutex_lock(&lock);

    // 等待队列非满
    while (((tail + 1) % QSIZE) == head) {
        printf("[queue] WARNING: queue full, waiting...\n");
        pthread_cond_wait(&cond_not_full, &lock);
    }

    queue[tail] = *msg;
    tail = (tail + 1) % QSIZE;

    pthread_cond_signal(&cond_not_empty);
    pthread_mutex_unlock(&lock);
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

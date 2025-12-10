#include <string.h>
#include <pthread.h>
#include "event_queue.h"

#define QSIZE 128

static event_msg_t queue[QSIZE];
static int head = 0, tail = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;

void queue_init(void)
{
    head = tail = 0;
}

void queue_push(event_msg_t* msg)
{
    pthread_mutex_lock(&lock);

    queue[tail] = *msg;
    tail = (tail + 1) % QSIZE;

    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
}

void queue_pop(event_msg_t* msg)
{
    pthread_mutex_lock(&lock);

    while (head == tail)
        pthread_cond_wait(&cond, &lock);

    *msg = queue[head];
    head = (head + 1) % QSIZE;

    pthread_mutex_unlock(&lock);
}

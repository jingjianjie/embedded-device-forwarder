#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include "event.h"

void queue_init(void);
void queue_push(event_msg_t* msg);
void queue_pop(event_msg_t* msg);

#endif

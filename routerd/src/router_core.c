#include <stdio.h>
#include <string.h>
#include "router_core.h"
#include "router_link.h"
#include "port_map.h"
// #include "forward.h"
// #include "config_store.h"
#include "port_manager.h"

void router_core_handle(event_msg_t* msg)
{
    if (!msg) return;

    port_def_t* dst = config_find_port(msg->dst);
    if (!dst) {
        printf("[router] ERROR: dst port '%s' not found\n", msg->dst);
        return;
    }

    printf("[router] write data to %s\n", msg->dst);

    int w = port_send(dst, msg->data, msg->len);
    if (w < 0) {
        printf("[router] ERROR: send failed on %s\n", msg->dst);
    }
}

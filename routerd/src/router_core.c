#include <stdio.h>
#include <string.h>
#include "router_core.h"
#include "router_link.h"
#include "port_map.h"
// #include "forward.h"
// #include "config_store.h"
#include "port_manager.h"
#include "log.h"
void router_core_handle(event_msg_t* msg)
{
    if (!msg) return;

    LOG_INFO("[router] enter routers and get port");
    port_def_t* dst = port_find(msg->dst);
    LOG_INFO("[router] find port success");
    if (!dst) {
        LOG_ERROR("[router] ERROR: dst port '%s' not found\n", msg->dst);
        return;
    }

    LOG_INFO("[router] write data to %s\n", msg->dst);

    int w = port_send(dst, msg->data, msg->len);
    if (w < 0) {
        LOG_ERROR("[router] ERROR: send failed on %s\n", msg->dst);
    }
}

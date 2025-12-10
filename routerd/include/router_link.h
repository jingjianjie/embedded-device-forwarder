#ifndef ROUTER_LINK_H
#define ROUTER_LINK_H

#include <stdint.h>
#include "plugin_loader.h"

#define MAX_LINKS 64

typedef struct {
    int in_port;
    int out_port;
    int bidir;

    // 插件名（来自 config.json）
    char hook_in_name[128];
    char hook_out_name[128];

    // 函数指针（运行时）
    // hook_func_t hook_in;
    // hook_func_t hook_out;
} router_link_t;

extern router_link_t g_links[MAX_LINKS];
extern int g_link_count;

void router_clear_routes();
void router_register_routes(router_link_t* src, int count);
void router_restore_from_config();

#endif

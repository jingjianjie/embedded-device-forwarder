#include "router_link.h"
#include "reactor.h"
#include "plugin_loader.h"
#include "config_store.h"
#include <stdio.h>
#include <string.h>

router_link_t g_links[MAX_LINKS];
int g_link_count = 0;

// ------------------------------------------------
// 清空所有路由
// ------------------------------------------------
void router_clear_routes()
{
    // g_link_count = 0;
    // memset(g_links, 0, sizeof(g_links));

    // reactor_clear_all_ports();

    printf("[router] routes cleared\n");
}

// ------------------------------------------------
// 注册路由表（来自 SDK）
// ------------------------------------------------
void router_register_routes(router_link_t* src, int count)
{
    // router_clear_routes();

    // for (int i = 0; i < count; i++) {

    //     router_link_t* dst = &g_links[g_link_count];
    //     *dst = src[i];

    //     // === 绑定插件 hook ===
    //     if (dst->hook_in_name[0])
    //         dst->hook_in = plugin_get_hook_in(dst->hook_in_name);

    //     if (dst->hook_out_name[0])
    //         dst->hook_out = plugin_get_hook_out(dst->hook_out_name);

    //     g_link_count++;

    //     // === 绑定端口 ===
    //     reactor_add_fd_by_port(dst->in_port);

    //     if (dst->bidir)
    //         reactor_add_fd_by_port(dst->out_port);
    // }

    // printf("[router] %d routes registered\n", g_link_count);
}

// ------------------------------------------------
// 从 config.json 恢复路由表
// ------------------------------------------------
void router_restore_from_config()
{
    // router_link_t tmp[MAX_LINKS];
    // int count = config_load_routes(tmp);

    // if (count <= 0)
    //     return;

    // router_clear_routes();

    // for (int i = 0; i < count; i++) {

    //     router_link_t* dst = &g_links[g_link_count];
    //     *dst = tmp[i];

    //     // === 按插件名称重新绑定 hook ===
    //     if (dst->hook_in_name[0])
    //         dst->hook_in = plugin_get_hook_in(dst->hook_in_name);

    //     if (dst->hook_out_name[0])
    //         dst->hook_out = plugin_get_hook_out(dst->hook_out_name);

    //     g_link_count++;

    //     reactor_add_fd_by_port(dst->in_port);

    //     if (dst->bidir)
    //         reactor_add_fd_by_port(dst->out_port);
    // }

    // printf("[router] restored %d routes from config\n", g_link_count);
}

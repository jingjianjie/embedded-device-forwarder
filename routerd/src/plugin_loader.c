#include "plugin_loader.h"
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include "log.h"


static handler_entry_t g_handlers[MAX_HANDLERS];
static int g_handler_count = 0;

void plugin_register_handler(
        const char* plugin_name,
        const char* handler_name,
        plugin_handler_t func)
{
    if (g_handler_count >= MAX_HANDLERS)
        return;

    snprintf(g_handlers[g_handler_count].full_name,
             sizeof(g_handlers[g_handler_count].full_name),
             "%s.%s", plugin_name, handler_name);

    g_handlers[g_handler_count].func = func;

    LOG_INFO("[plugin] register handler: %s -> %p\n",
           g_handlers[g_handler_count].full_name, func);

    g_handler_count++;
}

plugin_handler_t plugin_get_handler(const char* fullname)
{
    for (int i = 0; i < g_handler_count; ++i) {
        if (strcmp(g_handlers[i].full_name, fullname) == 0)
            return g_handlers[i].func;
    }
    return NULL;
}

void plugin_list_handlers()
{
    printf("=== Registered Handlers ===\n");
    for (int i = 0; i < g_handler_count; ++i) {
        LOG_INFO("%s -> %p\n", g_handlers[i].full_name, g_handlers[i].func);
    }
    LOG_INFO("===========================\n");
}


int plugin_load(const char* path)
{
    void* handle = dlopen(path, RTLD_NOW);
    if (!handle) {
        LOG_ERROR("dlopen error: %s\n", dlerror());
        return -1;
    }

    LOG_INFO("[plugin] loaded %s\n", path);
    // plugin_list_handlers();  // 展示目前所有插件注册的函数

    return 0;
}

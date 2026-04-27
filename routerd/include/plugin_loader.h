#ifndef PLUGIN_LOADER_H
#define PLUGIN_LOADER_H

#include <stdint.h>

// 插件 handler 契约(见 PROJECT_CONTEXT.MD 条目 5):
// - 就地修改 data,返回新长度
// - 新长度必须在 [0, MAX_DATA] 区间(MAX_DATA=1024,见 event.h)
// - 调用方(reactor)会 clamp 越界返回值,但 plugin 仍应自律,
//   越界即代表 plugin 实现错误,会被截断并打 WARN
typedef int (*plugin_handler_t)(uint8_t* data, int len);

typedef struct{
    char full_name[128];
    plugin_handler_t func;
}handler_entry_t;

#define MAX_HANDLERS 256

void plugin_register_handler(const char* plugin_name,
                             const char* handler_name,
                             plugin_handler_t func);

plugin_handler_t plugin_get_handler(const char* flullname);

void plugin_list_handlers();
int plugin_load(const char*);

#endif

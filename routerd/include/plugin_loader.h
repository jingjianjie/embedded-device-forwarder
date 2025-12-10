#ifndef PLUGIN_LOADER_H
#define PLUGIN_LOADER_H

#include <stdint.h>

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

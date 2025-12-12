#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config_store.h"
#include "cJSON.h"
#include "log.h"

config_t g_config;
// 安全读取字符串字段
#define GET_STR(obj, key, dst)                             \
    do {                                                   \
        cJSON* __v = cJSON_GetObjectItem(obj, key);        \
        if (__v && cJSON_IsString(__v))                    \
            strncpy(dst, __v->valuestring, sizeof(dst)-1); \
        else                                               \
            dst[0] = '\0';                                 \
    } while (0)

// 安全读取整数字段
#define GET_INT(obj, key, dst)                             \
    do {                                                   \
        cJSON* __v = cJSON_GetObjectItem(obj, key);        \
        if (__v && cJSON_IsNumber(__v))                    \
            dst = __v->valueint;                           \
        else                                               \
            dst = 0;                                       \
    } while (0)






// ============ 工具函数 ============
static void clear_config()
{
    memset(&g_config, 0, sizeof(g_config));
}

port_def_t* config_find_port(const char* name)
{
    if (!name || name[0] == '\0') {
        LOG_INFO("[config] ERROR: config_find_port: name is NULL\n");
        return NULL;
    }

    for (int i = 0; i < g_config.port_count; i++) {
        port_def_t* p = &g_config.ports[i];

        // 安全比较
        if (strncmp(p->base.name, name, sizeof(p->base.name)) == 0) {
            return p;
        }
    }

    // 未找到
    return NULL;
}

// port_def_t* config_find_port_by_fd(int fd){
//     for(int i=0;i<g_config.port_count;i++){
//         if(g_config.ports[i].base.fd==fd){
//             return &g_config.ports[i];
//         }
//     }
//     return NULL;
// }



int config_find_plugin(const char* name)
{
    for (int i = 0; i < g_config.plugin_count; i++) {
        if (strcmp(g_config.plugins[i].name, name) == 0)
            return i;
    }
    return -1;
}

// ============ JSON → config_t ============
static void parse_ports(cJSON* arr)
{
    if (!arr || !cJSON_IsArray(arr)) {
        LOG_INFO("[config] ERROR: ports missing or not array\n");
        return;
    }

    g_config.port_count = 0;
    cJSON* item;

    cJSON_ArrayForEach(item, arr)
    {
        if (g_config.port_count >= MAX_PORTS)
            break;

        port_def_t* p = &g_config.ports[g_config.port_count++];

        // ---- 基础字段 ----
        GET_STR(item, "name", p->base.name);

        //----
        {
            cJSON* jf = cJSON_GetObjectItem(item, "use_frame");
            if (cJSON_IsBool(jf))
                p->base.use_frame = cJSON_IsTrue(jf);
            else
                p->base.use_frame = 0;   // 默认 false
        }


        // type
        char type_str[32] = {0};
        GET_STR(item, "type", type_str);

        // ---- 根据 type 解析 ----
        if (strcmp(type_str, "tty") == 0) {
            p->base.type = PORT_TTY;

            cJSON* tty = cJSON_GetObjectItem(item, "tty");

            if (!tty) {
                LOG_INFO("[config] ERROR: tty config missing for %s\n", p->base.name);
                continue;
            }

            GET_STR(tty, "path", p->cfg.tty.path);
            GET_INT(tty, "baudrate", p->cfg.tty.baudrate);
            GET_INT(tty, "databits", p->cfg.tty.databits);
            GET_INT(tty, "stopbits", p->cfg.tty.stopbits);
            GET_INT(tty, "parity",   p->cfg.tty.parity);
            GET_INT(tty, "flow",     p->cfg.tty.flow);
        }
        else if (strcmp(type_str, "tcp_server") == 0) {
            p->base.type = PORT_TCP_SERVER;

            cJSON* ts = cJSON_GetObjectItem(item, "tcp_server");

            GET_STR(ts, "bind", p->cfg.tcp_server.bind_addr);
            GET_INT(ts, "port", p->cfg.tcp_server.port);
            GET_INT(ts, "backlog", p->cfg.tcp_server.backlog);
        }
        else if (strcmp(type_str, "tcp_client") == 0) {
            p->base.type = PORT_TCP_CLIENT;

            cJSON* tc = cJSON_GetObjectItem(item, "tcp_client");

            GET_STR(tc, "addr", p->cfg.tcp_client.addr);
            GET_INT(tc, "port", p->cfg.tcp_client.port);
        }
        else if (strcmp(type_str, "udp") == 0) {
            p->base.type = PORT_UDP;

            cJSON* u = cJSON_GetObjectItem(item, "udp");

            GET_STR(u, "bind", p->cfg.udp.bind_addr);
            GET_INT(u, "port", p->cfg.udp.port);
        }
        else if (strcmp(type_str, "usb") == 0) {
            p->base.type = PORT_USB;

            cJSON* u = cJSON_GetObjectItem(item, "usb");

            GET_STR(u, "path", p->cfg.usb.path);
        }
        else {
            LOG_INFO("[config] ERROR: unknown port type: %s\n", type_str);
        }

        p->base.fd = -1; // runtime use
    }
}



static void parse_plugins(cJSON* arr)
{
    if (!arr || !cJSON_IsArray(arr)) {
        LOG_ERROR("[config] plugins missing or invalid\n");
        return;
    }

    cJSON* item;
    g_config.plugin_count = 0;

    cJSON_ArrayForEach(item, arr) {

        if (g_config.plugin_count >= MAX_PLUGINS) {
            LOG_ERROR("[config] too many plugins\n");
            break;
        }

        plugin_def_t* p = &g_config.plugins[g_config.plugin_count++];

        GET_STR(item, "name", p->name);
        GET_STR(item, "path", p->path);
    }
}


static void parse_routes(cJSON* arr)
{
    if (!arr || !cJSON_IsArray(arr)) {
        LOG_ERROR("[config] routes missing or invalid\n");
        return;
    }

    cJSON* item;
    g_config.route_count = 0;

    cJSON_ArrayForEach(item, arr) {

        if (g_config.route_count >= MAX_ROUTES) {
            LOG_ERROR("[config] too many routes\n");
            break;
        }

        route_def_t* r = &g_config.routes[g_config.route_count++];

        GET_STR(item, "src",     r->src);
        GET_STR(item, "dst",     r->dst);
        GET_STR(item, "plugin",  r->plugin);
        GET_STR(item, "handler", r->handler);
    }
}


// ============ load_config() ============
int load_config(const char* filename)
{
    clear_config();

    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        perror("open config");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char* text = (char*)malloc(size + 1);
    fread(text, 1, size, fp);
    fclose(fp);
    text[size] = 0;

    cJSON* root = cJSON_Parse(text);
    free(text);

    if (!root) {
        LOG_ERROR("config parse error\n");
        return -1;
    }

    LOG_INFO("[config] before parse!\n");
    parse_ports(cJSON_GetObjectItem(root, "ports"));
    parse_plugins(cJSON_GetObjectItem(root, "plugins"));
    parse_routes(cJSON_GetObjectItem(root, "routes"));

    cJSON_Delete(root);

    LOG_INFO("[config] loaded: %d ports, %d plugins, %d routes\n",
        g_config.port_count,
        g_config.plugin_count,
        g_config.route_count);

    return 0;
}

// ============ save_config() ============
int save_config(const char* filename)
{
    cJSON* root = cJSON_CreateObject();

    cJSON* arr_ports = cJSON_AddArrayToObject(root, "ports");

    for (int i = 0; i < g_config.port_count; i++) {

        port_def_t* p = &g_config.ports[i];
        cJSON* o = cJSON_CreateObject();
        cJSON_AddItemToArray(arr_ports, o);

        // 基础字段
        cJSON_AddStringToObject(o, "name", p->base.name);

        // 根据类型写入 type + 子对象
        switch (p->base.type)
        {
        case PORT_TTY: {
            cJSON_AddStringToObject(o, "type", "tty");

            cJSON* tty = cJSON_CreateObject();
            cJSON_AddItemToObject(o, "tty", tty);

            cJSON_AddStringToObject(tty, "path", p->cfg.tty.path);
            cJSON_AddNumberToObject(tty, "baudrate", p->cfg.tty.baudrate);
            cJSON_AddNumberToObject(tty, "databits", p->cfg.tty.databits);
            cJSON_AddNumberToObject(tty, "stopbits", p->cfg.tty.stopbits);
            cJSON_AddNumberToObject(tty, "parity",   p->cfg.tty.parity);
            cJSON_AddNumberToObject(tty, "flow",     p->cfg.tty.flow);
            break;
        }

    case PORT_TCP_SERVER: {
        cJSON_AddStringToObject(o, "type", "tcp_server");

        cJSON* ts = cJSON_CreateObject();
        cJSON_AddItemToObject(o, "tcp_server", ts);

        cJSON_AddStringToObject(ts, "bind", p->cfg.tcp_server.bind_addr);
        cJSON_AddNumberToObject(ts, "port", p->cfg.tcp_server.port);
        cJSON_AddNumberToObject(ts, "backlog", p->cfg.tcp_server.backlog);
        break;
    }

case PORT_TCP_CLIENT: {
    cJSON_AddStringToObject(o, "type", "tcp_client");

    cJSON* tc = cJSON_CreateObject();
    cJSON_AddItemToObject(o, "tcp_client", tc);

    cJSON_AddStringToObject(tc, "addr", p->cfg.tcp_client.addr);
    cJSON_AddNumberToObject(tc, "port", p->cfg.tcp_client.port);
    break;
}

case PORT_UDP: {
    cJSON_AddStringToObject(o, "type", "udp");

    cJSON* u = cJSON_CreateObject();
    cJSON_AddItemToObject(o, "udp", u);

    cJSON_AddStringToObject(u, "bind", p->cfg.udp.bind_addr);
    cJSON_AddNumberToObject(u, "port", p->cfg.udp.port);
    break;
}

case PORT_USB: {
    cJSON_AddStringToObject(o, "type", "usb");

    cJSON* u = cJSON_CreateObject();
    cJSON_AddItemToObject(o, "usb", u);

    cJSON_AddStringToObject(u, "path", p->cfg.usb.path);
    break;
}

default:
    LOG_INFO("[config] WARNING: unknown port type %d\n", p->base.type);
    break;
}
}

    // plugins
cJSON* arr_plugins = cJSON_AddArrayToObject(root, "plugins");
for (int i = 0; i < g_config.plugin_count; i++) {
    plugin_def_t* p = &g_config.plugins[i];
    cJSON* o = cJSON_CreateObject();
    cJSON_AddItemToArray(arr_plugins, o);
    cJSON_AddStringToObject(o, "name", p->name);
    cJSON_AddStringToObject(o, "path", p->path);
}

    // routes
cJSON* arr_routes = cJSON_AddArrayToObject(root, "routes");
for (int i = 0; i < g_config.route_count; i++) {
    route_def_t* r = &g_config.routes[i];
    cJSON* o = cJSON_CreateObject();
    cJSON_AddItemToArray(arr_routes, o);
    cJSON_AddStringToObject(o, "src",     r->src);
    cJSON_AddStringToObject(o, "dst",     r->dst);
    cJSON_AddStringToObject(o, "plugin",  r->plugin);
    cJSON_AddStringToObject(o, "handler", r->handler);
}

char* out = cJSON_Print(root);

FILE* fp = fopen(filename, "wb");
fwrite(out, 1, strlen(out), fp);
fclose(fp);

free(out);
cJSON_Delete(root);

return 0;
}


//===================================================
// find all routes
//===================================================
int config_find_routes_by_src(
        const char* src_name,
        route_def_t* out[],
        int max_out)
{
    if (!src_name || !out || max_out <= 0)
        return 0;

    int count = 0;

    for (int i = 0; i < g_config.route_count; i++) {
        route_def_t* r = &g_config.routes[i];

        // 比较 src
        if (strcmp(r->src, src_name) == 0) {
            if (count < max_out) {
                out[count++] = r;
            }
        }
    }

    return count;
}





void config_print()
{
    LOG_INFO("\n================ CONFIG DUMP ================\n");

    /* ----------- PORTS ----------- */
    LOG_INFO("\n[PORTS] count = %d\n", g_config.port_count);

    for (int i = 0; i < g_config.port_count; i++) {

        port_def_t* p = &g_config.ports[i];
        LOG_INFO("  [%d]\n", i);
        LOG_INFO("    name : %s\n", p->base.name);
        LOG_INFO("    use_frame: %d\n",p->base.use_frame);
        LOG_INFO("    type : ");

        switch (p->base.type)
        {
            case PORT_TTY:
                LOG_INFO("tty\n");
                LOG_INFO("      path      : %s\n", p->cfg.tty.path);
                LOG_INFO("      baudrate  : %d\n", p->cfg.tty.baudrate);
                LOG_INFO("      databits  : %d\n", p->cfg.tty.databits);
                LOG_INFO("      stopbits  : %d\n", p->cfg.tty.stopbits);
                LOG_INFO("      parity    : %d\n", p->cfg.tty.parity);
                LOG_INFO("      flow      : %d\n", p->cfg.tty.flow);
                break;

            case PORT_TCP_SERVER:
                LOG_INFO("tcp_server\n");
                LOG_INFO("      bind      : %s\n", p->cfg.tcp_server.bind_addr);
                LOG_INFO("      port      : %d\n", p->cfg.tcp_server.port);
                LOG_INFO("      backlog   : %d\n", p->cfg.tcp_server.backlog);
                break;

            case PORT_TCP_CLIENT:
                LOG_INFO("tcp_client\n");
                LOG_INFO("      addr      : %s\n", p->cfg.tcp_client.addr);
                LOG_INFO("      port      : %d\n", p->cfg.tcp_client.port);
                break;

            case PORT_UDP:
                LOG_INFO("udp\n");
                LOG_INFO("      bind      : %s\n", p->cfg.udp.bind_addr);
                LOG_INFO("      port      : %d\n", p->cfg.udp.port);
                break;

            case PORT_USB:
                LOG_INFO("usb\n");
                LOG_INFO("      path      : %s\n", p->cfg.usb.path);
                break;

            default:
                LOG_INFO("UNKNOWN(%d)\n", p->base.type);
                break;
        }

        LOG_INFO("    fd : %d\n", p->base.fd);
    }

    /* ----------- PLUGINS ----------- */
    LOG_INFO("\n[PLUGINS] count = %d\n", g_config.plugin_count);
    for (int i = 0; i < g_config.plugin_count; i++) {
        plugin_def_t* p = &g_config.plugins[i];
        LOG_INFO("  [%d]\n", i);
        LOG_INFO("    name : %s\n", p->name);
        LOG_INFO("    path : %s\n", p->path);
    }

    /* ----------- ROUTES ----------- */
    LOG_INFO("\n[ROUTES] count = %d\n", g_config.route_count);
    for (int i = 0; i < g_config.route_count; i++) {
        route_def_t* r = &g_config.routes[i];
        LOG_INFO("  [%d]\n", i);
        LOG_INFO("    src     : %s\n", r->src);
        LOG_INFO("    dst     : %s\n", r->dst);
        LOG_INFO("    plugin  : %s\n", r->plugin);
        LOG_INFO("    handler : %s\n", r->handler);
    }

    LOG_INFO("\n=============================================\n\n");
}

#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <stdint.h>
#include "cJSON.h"

#define MAX_PORTS    32
#define MAX_PLUGINS  32
#define MAX_ROUTES   64

typedef enum {
    PORT_TTY,
    PORT_TCP_SERVER,
    PORT_TCP_CLIENT,
    PORT_UDP,
    PORT_USB,
    PORT_IPC_SERVER,
    PORT_IPC_CLIENT,
} port_type_t;

typedef enum {
    PARITY_NONE = 0,
    PARITY_ODD  = 1,
    PARITY_EVEN = 2
} parity_t;

typedef enum {
    FLOW_NONE    = 0,
    FLOW_RTSCTS  = 1,
    FLOW_XONXOFF = 2
} flow_t;

typedef struct {
    char name[32];
    port_type_t type;
    int fd;
    int use_frame;
} port_base_t;

typedef struct {
    char path[128];
    int baudrate;
    int databits;
    int stopbits;
    parity_t parity;
    flow_t flow;
} port_tty_conf_t;

typedef struct {
    char bind_addr[64];
    int  port;
    int  backlog;
} port_tcp_server_conf_t;

typedef struct {
    char addr[64];
    int  port;
} port_tcp_client_conf_t;

typedef struct {
    char bind_addr[64];
    int port;
} port_udp_conf_t;

typedef struct {
    char path[128];
} port_usb_conf_t;

typedef struct{
    char path[128];
}port_ipc_conf_t;


typedef struct {
    port_base_t base;

    union {
        port_tty_conf_t        tty;
        port_tcp_server_conf_t tcp_server;
        port_tcp_client_conf_t tcp_client;
        port_udp_conf_t        udp;
        port_usb_conf_t        usb;
        port_ipc_conf_t        ipc;
    } cfg;

} port_def_t;



typedef struct {
    char name[32];
    char path[256];
} plugin_def_t;

typedef struct {
    char src[32];
    char dst[32];
    char plugin[32];
    char handler[64];
} route_def_t;

typedef struct {
    port_def_t    ports[MAX_PORTS];
    int           port_count;

    plugin_def_t  plugins[MAX_PLUGINS];
    int           plugin_count;

    route_def_t   routes[MAX_ROUTES];
    int           route_count;
} config_t;

extern config_t g_config;

// 加载与保存
int load_config(const char* filename);
int save_config(const char* filename);

// 工具函数
port_def_t* config_find_port(const char* name);
int config_find_plugin(const char* name);
void config_print();

// find routes
int config_find_routes_by_src(
        const char* src_name,
        route_def_t* out[],
        int max_out);

// port_def_t* config_find_port_by_fd(int fd);

extern config_t g_config;
#endif

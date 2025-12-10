#ifndef EZ_ROUTER_SDK_H
#define EZ_ROUTER_SDK_H

#include <stdint.h>

#define MAX_LINKS 64

// SDK IPC 命令编号
#define CMD_SET_ROUTES    1
#define CMD_CLEAR_ROUTES  2
#define CMD_SAVE_CONFIG   3
#define CMD_LOAD_PLUGIN   4
#define CMD_UNLOAD_PLUGIN 5

// 端口 ID（你要与 daemon 的 enum PORT_* 完全一致）
typedef enum {
    PORT_USB1 = 1,
    PORT_UART1 = 2,
    PORT_UART2 = 3,
    PORT_NET0 = 4,
} port_id_t;


typedef struct {
    int in_port;
    int out_port;
    int bidir;              // 0 = 单向, 1 = 双向

    char hook_in_plugin[64];
    char hook_in_func[64];

    char hook_out_plugin[64];
    char hook_out_func[64];
} ez_router_link_t;



typedef struct{
    uint32_t magic;
    uint16_t cmd;
    uint16_t reserved;
    uint32_t length;
}ipc_header_t;

typedef struct{
    int count;
    ez_router_link_t links[];
}ipc_payload_set_routes_t;

typedef struct{
    uint32_t len;
    char plugin_path[];
}ipc_payload_plugin_t;



// typedef struct {
//     int in_port;       // 输入端口
//     int out_port;      // 输出端口
//     int bidir;         // 0=单向, 1=双向
//     char hook_in[64];  // hook 入口函数名
//     char hook_out[64]; // hook 出口函数名
// } ez_router_link_t;




// SDK API
int ezr_set_routes(ez_router_link_t* links, int count);
int ezr_clear_routes();
int ezr_save_config();

int ezr_load_plugin(const char* plugin_path);
int ezr_unload_plugin( const char* plugin_path);
int ezr_init(const char* sockpath);
void ezr_close();

#endif

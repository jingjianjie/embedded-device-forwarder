#ifndef IPC_SERVER_H
#define IPC_SERVER_H
#include<stdint.h>


#define IPC_MAGIC 0xE7F12345

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




// 初始化 IPC server，返回 server fd
int ipc_server_init(const char* sockpath);
void* ipc_thread(void* arg);
void process_frame(uint8_t* buf,int n);
void ipc_handle_frame(uint16_t cmd, uint8_t* payload, uint32_t len);

#endif

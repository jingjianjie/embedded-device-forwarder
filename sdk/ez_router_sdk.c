#include "ez_router_sdk.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>


#define IPC_MAGIC 0xE7F12345


static int g_sdk_fd=-1;

// 建立 IPC socket 连接
 int ezr_connect(const char* sockpath)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, sockpath);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int ezr_init(const char* sockpath){
    g_sdk_fd=ezr_connect(sockpath);
    return g_sdk_fd<0? -1:0;
}

// =========================
// 清空路由
// =========================
int ezr_clear_routes()
{
    if(g_sdk_fd<0) return -1;

    ipc_header_t* hdr=malloc(sizeof(ipc_header_t));
    hdr->magic=IPC_MAGIC;
    hdr->cmd=CMD_CLEAR_ROUTES;
    hdr->length=0;
    send(g_sdk_fd,hdr,sizeof(ipc_header_t),0);
    free(hdr);
    return 0;
}

// =========================
// 设置路由表
// =========================
int ezr_set_routes(ez_router_link_t* links, int count)
{
    //int fd = ipc_connect(sockpath);
    if(g_sdk_fd<0) return -1;

    uint32_t payload_len=sizeof(int)+sizeof(ez_router_link_t)*count;
    uint32_t frame_len=sizeof(ipc_header_t)+payload_len;
    uint8_t* buf=malloc(frame_len);//calc length
    ipc_header_t* hdr=(ipc_header_t*)buf;//use base address as header
    hdr->magic=IPC_MAGIC;
    hdr->cmd=CMD_SET_ROUTES;
    hdr->length=payload_len;

    //payload
    uint8_t* payload=buf+sizeof(ipc_header_t);
    memcpy(payload,&count,sizeof(int));// copy payload_length,links count
    memcpy(payload+sizeof(int),links,sizeof(ez_router_link_t)*count);

    send(g_sdk_fd,buf,frame_len,0);
    free(buf);
    return 0;
}


// =========================
// 保存配置（写 config.json）
// =========================
int ezr_save_config()
{
    if(g_sdk_fd<0) return -1;

    ipc_header_t* hdr=malloc(sizeof(ipc_header_t));
    hdr->magic=IPC_MAGIC;
    hdr->cmd=CMD_SAVE_CONFIG;
    hdr->length=0;
    send(g_sdk_fd,hdr,sizeof(ipc_header_t),0);
    free(hdr);
    return 0;
}

// =========================
// 加载插件 (*.so)
// =========================
int ezr_load_plugin(const char* plugin_path)
{
    if(g_sdk_fd<0) return -1;

    uint32_t path_len=strlen(plugin_path)+1;
    uint32_t payload_len=sizeof(uint32_t)+path_len;

    uint32_t frame_len=sizeof(ipc_header_t)+payload_len;
    uint8_t* buf=malloc(frame_len);
    if(!buf) return -1;
    ipc_header_t* hdr=(ipc_header_t*)buf;
    hdr->magic=IPC_MAGIC;
    hdr->cmd=CMD_LOAD_PLUGIN;
    hdr->reserved=0;
    hdr->length=payload_len;

    uint8_t* payload=buf+sizeof(ipc_header_t);
    memcpy(payload,&path_len,sizeof(uint32_t));
    memcpy(payload+sizeof(uint32_t),plugin_path,path_len);
    send(g_sdk_fd,buf,frame_len,0);
    free(buf);
    return 0;
}

// =========================
// 卸载插件 (*.so)
// =========================
int ezr_unload_plugin(const char* plugin_path)
{
      if (g_sdk_fd < 0) return -1;

    uint32_t path_len = strlen(plugin_path) + 1;     // 包含 '\0'
    uint32_t payload_len = sizeof(uint32_t) + path_len;

    uint32_t frame_len = sizeof(ipc_header_t) + payload_len;
    uint8_t* buf = malloc(frame_len);
    if (!buf) return -1;

    // ------ header ------
    ipc_header_t* hdr = (ipc_header_t*)buf;
    hdr->magic    = IPC_MAGIC;
    hdr->cmd      = CMD_UNLOAD_PLUGIN;
    hdr->reserved = 0;
    hdr->length   = payload_len;

    // ------ payload ------
    uint8_t* payload = buf + sizeof(ipc_header_t);

    memcpy(payload, &path_len, sizeof(uint32_t));                 // 写入路径长度
    memcpy(payload + sizeof(uint32_t), plugin_path, path_len);    // 写入路径字符串

    // ------ send ------
    send(g_sdk_fd, buf, frame_len, 0);

    free(buf);
    return 0;
}


void ezr_close(){
    if(g_sdk_fd>=0){
        close(g_sdk_fd);
        g_sdk_fd=-1;
    }
}
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ipc_server.h"
#include "router_link.h"
#include "config_store.h"
#include <unistd.h>
#include <errno.h>
#include "reactor.h"
#include "plugin_loader.h"
#include "log.h"
#include "run_state.h"

#define CMD_SET_ROUTES    1
#define CMD_CLEAR_ROUTES  2
#define CMD_SAVE_CONFIG   3
#define CMD_LOAD_PLUGIN   4
#define CMD_UNLOAD_PLUGIN 5

static int g_ipc_fd = -1;

int ipc_server_init(const char* sockpath)
{
    unlink(sockpath);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, sockpath);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        return -1;

    listen(fd, 5);

    g_ipc_fd = fd;r
    return fd;
}

void process_frame(uint8_t* buf,int n){

     if (n < sizeof(ipc_header_t)) {
        LOG_INFO("[IPC] frame too small: %d bytes\n", n);
        return;
    }

    ipc_header_t* hdr = (ipc_header_t*)buf;

    if (hdr->magic != IPC_MAGIC) {
        LOG_INFO("[IPC] invalid magic: 0x%X\n", hdr->magic);
        return;
    }

    uint32_t payload_len = hdr->length;

    if (sizeof(ipc_header_t) + payload_len > n) {
        LOG_INFO("[IPC] incomplete frame: need %u bytes, got %d\n",
               sizeof(ipc_header_t) + payload_len, n);
        return;
    }

    uint8_t* payload = buf + sizeof(ipc_header_t);

    // 分发给上层处理
    ipc_handle_frame(hdr->cmd, payload, payload_len);
}




void* ipc_thread(void* arg){


     LOG_INFO("[IPC] waiting for SDK...\n");
    while (run_state_is_running()) {

        int client_fd = accept(g_ipc_fd, NULL, NULL);
        if (client_fd < 0) {
            // sleep(10);
            // perror("accept");
            usleep(50*1000);
            continue;
        }

        LOG_INFO("[IPC] SDK connected, fd=%d\n", client_fd);

        //use long connect ,until broke
        while (run_state_is_running()) {
            uint8_t buf[4096];
            int n = recv(client_fd, buf, sizeof(buf), 0);

            if (n > 0) {
                process_frame(buf, n);
            }
            else if (n == 0) {
                LOG_INFO("[IPC] SDK disconnected.\n");
                close(client_fd);
                break;  // 回到 accept 等下一次连接
            }
            else {
                if (errno == EINTR)
                    continue;

                LOG_INFO("[IPC] recv error");
                close(client_fd);
                break;
            }
        }
    }
    return NULL;
}


//-----------------------------------------------------------------
// ipc-handle frame
//----------------------------------------------------------------
void ipc_handle_frame(uint16_t cmd, uint8_t* payload, uint32_t len)
{
    LOG_INFO("[IPC] cmd=%d\n",cmd);
    switch (cmd) {

    case CMD_SET_ROUTES: {
        if (len < sizeof(int)) {
            LOG_INFO("[IPC] SET_ROUTES payload too small\n");
            return;
        }

        int count = *(int*)payload;
        size_t expect = sizeof(int) + sizeof(ez_router_link_t) * count;

        if (len < expect) {
            LOG_INFO("[IPC] SET_ROUTES payload mismatch\n");
            return;
        }

        // ez_router_link_t* links =
        //     (ez_router_link_t*)(payload + sizeof(int));

        LOG_INFO("[IPC] set routes\n");
        // router_set_routes_from_ipc(links, count);
        break;
    }

    //clear routes
    case CMD_CLEAR_ROUTES:
        LOG_INFO("clear routes\n");
        break;

    case CMD_SAVE_CONFIG:
        // save_config_json();
        break;

    case CMD_LOAD_PLUGIN: {
        uint32_t path_len = *(uint32_t*)payload;
        if (len < sizeof(uint32_t) + path_len) return;

        char* path = (char*)(payload + sizeof(uint32_t));
        plugin_load(path);//load
        LOG_INFO("load plugin\n");
        break;
    }

    case CMD_UNLOAD_PLUGIN: {
        uint32_t path_len = *(uint32_t*)payload;
        if (len < sizeof(uint32_t) + path_len) return;

        // char* path = (char*)(payload + sizeof(uint32_t));
        // plugin_unload(path);
        break;
    }

    default:
        LOG_INFO("[IPC] unknown cmd=%d\n", cmd);
        break;
    }
}


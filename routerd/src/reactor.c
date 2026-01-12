#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "reactor.h"
#include "event_queue.h"
#include "event.h"
#include "port_map.h"
// #include "ipc_server.h"
#include <pthread.h>
#include "plugin_loader.h"
#include "log.h"
#include "run_state.h"

static int epfd = -1;
static int reactor_fds[MAX_REACT_FDS];

//
// #define MAX_REACTOR_PORTS 128
// static reactor_entry_t g_reactor_table[MAX_REACTOR_PORTS];
// reactor_entry_t reactor_table[MAX_REACT_FDS] = {0};
// static int g_client_fd=-1;
// static int g_ipc_fd=-1;

// 初始化 reactor
void reactor_init(void)
{
    epfd = epoll_create(64);
    if (epfd < 0) {
        LOG_WARN("[reactor] epoll_create failed\n");
        return;
    }
    LOG_INFO("[reactor] init ok\n");
}

// 注册已有的 fd
// void reactor_add_fd(int fd)
// {
//     if (fd < 0) return;

//     struct epoll_event ev;
//     ev.events = EPOLLIN;
//     ev.data.fd = fd;

//     epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

//     // 放入 table
//     for (int i = 0; i < MAX_REACT_FDS; i++) {
//         if (reactor_table[fd].fd < 0) {
//             reactor_table[fd].fd = fd;
//             break;
//         }
//     }

//     printf("[reactor] add fd=%d\n", fd);
// }

// 根据 port 打开 fd 并注册
void reactor_add_fd_by_port(int port)
{
    int fd = port_open_fd(port);
    if (fd < 0) {
        printf("[reactor] port %d open failed\n", port);
        return;
    }
    // reactor_add_fd(fd);
}

// 清空所有端口（用于清空路由）
void reactor_clear_all_ports(void)
{
    //int ipc_fd=ipc_server_fd();
    // for (int i = 0; i < MAX_REACT_FDS; i++) {

    //     int fd = reactor_fds[i];
    //     if (fd >= 0) {
    //         epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    //         close(fd);
    //         reactor_fds[i] = -1;
    //     }
    // }

    LOG_INFO("[reactor] all ports cleared\n");
}

// reactor 主事件循环
// void* reactor_thread(void* arg)
// {
//     struct epoll_event evs[16];
//     printf("[reactor] thread started\n");

//     while (1) {
//         int n = epoll_wait(epfd, evs, 16, -1);
//         if (n <= 0) continue;

//         for (int i = 0; i < n; i++) {

//             port_def_t* port=evs[i].data.ptr;
//             int fd=port->base.fd;
//             // ================================
//             // ★ 普通数据端口读取路径
//             // ================================
//             uint8_t buf[1024];
//             int len = read(fd, buf, sizeof(buf));

//             if (len <= 0)
//                 continue;

//             printf("[reactor] recv from %s fd=%d len=%d\n",
//              port->base.name, fd, len);

//             printf("[reactor] data: %s\n",buf);

//             route_def_t* routes[16];
//             int n = config_find_routes_by_src(port->base.name, routes, 16);

//             for (int i = 0; i < n; i++) {
//                 route_def_t* r = routes[i];

//                 plugin_handler_t handler=plugin_get_handler(r->handler);
//                 int data_len=len;
//                 if(handler){
//                 data_len=handler(buf,len);// handler function
//             }
//             printf("Route: %s -> %s handler=%s plugin=%s\n",
//                r->src, r->dst, r->handler, r->plugin);

//             event_msg_t msg;
//             // msg.dst=r->dst;
//             memcpy(msg.dst,r->dst,sizeof(msg.dst));
//             msg.len = data_len;
//             memcpy(msg.data, buf, data_len);
//             queue_push(&msg);

//            printf("push message to queue\n");
//         }
//     }
// }

// return NULL;
// }

void* reactor_thread(void* arg)
{
    struct epoll_event evs[16];
    printf("[reactor] thread started\n");

    while (run_state_is_running()) {
        int n = epoll_wait(epfd, evs, 16, -1);
        if (n <= 0) continue;

        for (int i = 0; i < n; i++) {
            port_def_t* port = evs[i].data.ptr;
            int fd = port->base.fd;

            // ============================================
            // ① TCP 服务器监听端口：有新连接到达
            // ============================================
            if (port->base.type == PORT_TCP_SERVER) {
                struct sockaddr_in client_addr;
                socklen_t len = sizeof(client_addr);
                int client_fd = accept(fd, (struct sockaddr*)&client_addr, &len);
                if (client_fd < 0) {
                    perror("[reactor] accept");
                    continue;
                }

                LOG_INFO("[reactor] new client connected from %s:%d, fd=%d\n",
                 inet_ntoa(client_addr.sin_addr),
                 ntohs(client_addr.sin_port),
                 client_fd);

                // 建立新的 port_def_t 结构体用于 client_fd
                port_def_t* client_port = calloc(1, sizeof(port_def_t));
                strcpy(client_port->base.name, port->base.name);
                client_port->base.fd = client_fd;
                client_port->base.type = PORT_TCP_CLIENT;
                reactor_add_port(client_port);
                LOG_INFO("set up client with server\n");

                continue;  // 不要再往下执行 read()
            }

            if (port->base.type == PORT_IPC_SERVER) {

                int client_fd = accept(fd, NULL, NULL);
                if (client_fd < 0) {
                    perror("[reactor] ipc accept");
                    continue;
                }

                port_def_t* client = calloc(1, sizeof(port_def_t));

                // 继承 server 的逻辑名字（一般是 "HOST"）
                strcpy(client->base.name, port->base.name);

                client->base.fd   = client_fd;
                client->base.type = PORT_IPC_CLIENT;   // ✅ 正确
                reactor_add_port(client);
                LOG_INFO("[ipc] new %s client fd=%d\n",
                client->base.name, client_fd);
                continue;   // 不再往下 read()
                }

            // ============================================
            // ② 普通客户端或设备端口：读取数据
            // ============================================
                uint8_t buf[1024];
                int len = read(fd, buf, sizeof(buf));
                if(len>0){
                    buf[len]='\0';
                }

                if (len <= 0) {
                // 客户端断开连接
                    LOG_INFO("[reactor] fd=%d closed\n", fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    continue;
                }

                LOG_INFO("[reactor] recv from %s fd=%d len=%d\n",
                   port->base.name, fd, len);
                LOG_INFO("[reactor] data: %d ,%s\n", len, buf);

            // === 路由转发逻辑 ===
                route_def_t* routes[16];
                LOG_INFO("port name=%s\n",port->base.name);
                int rn = config_find_routes_by_src(port->base.name, routes, 16);
                LOG_INFO("find routes counte=%d\n",rn);
                for (int j = 0; j < rn; j++) {
                    route_def_t* r = routes[j];
                    plugin_handler_t handler = plugin_get_handler(r->handler);
                    int data_len = len;
                    if (handler)
                        data_len = handler(buf, len);

                    event_msg_t msg;

                //r->dst type


                    memcpy(msg.dst, r->dst, sizeof(msg.dst));
                    msg.len = data_len;
                    memcpy(msg.data, buf, data_len);
                    queue_push(&msg);

                    LOG_INFO("Route: %s -> %s, push message to queue\n", r->src, r->dst);
                }
            }
        }
    }





static pthread_mutex_t reactor_mutex=PTHREAD_MUTEX_INITIALIZER;
void reactor_lock(){
    pthread_mutex_lock(&reactor_mutex);
}

void reactor_unlock(){
    pthread_mutex_unlock(&reactor_mutex);
}

void reactor_clear_fds(){
    reactor_lock();
    for(int i=0;i<MAX_REACT_FDS;i++){
        if(reactor_fds[i]>0){
            epoll_ctl(epfd, EPOLL_CTL_DEL,reactor_fds[i], NULL);
            reactor_fds[i]=-1;
            // reactor_table[i].fd=-1;
            // reactor_table[i].handler=NULL;
        }
    }
    reactor_unlock();
}

void reactor_add_fd(int fd){

    if (fd < 0) return;

    reactor_lock();
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;

    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    reactor_fds[fd]=fd;
    // reactor_table[fd].fd=fd;
    // reactor_table[fd].handler=h;
    reactor_unlock();
    LOG_INFO("[reactor] add fd=%d\n", fd);
}

// port_def_t* reactor_find_port(int fd)
// {
//     for (int i = 0; i < MAX_REACTOR_PORTS; i++) {
//         if (g_reactor_table[i].used && g_reactor_table[i].fd == fd)
//             return g_reactor_table[i].port;
//     }
//     return NULL;
// }


void reactor_add_port(port_def_t* port){

   if(!port) return;

   int fd=port->base.fd;
   if(fd<0) return;

   for(int i=0;i<MAX_PORTS;i++){
        if(!g_port_table[i].used){
            g_port_table[i].used=1;
            g_port_table[i].fd=port->base.fd;
            g_port_table[i].port=port;
            break;
        }
    }
    LOG_INFO("add reactor table");

    reactor_lock();
    struct epoll_event ev;
    ev.events = EPOLLIN;
   //ev.data.fd = fd;
    ev.data.ptr=port;

    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
   // reactor_fds[fd]=fd;
    // reactor_table[fd].fd=fd;
    // reactor_table[fd].handler=h;
    reactor_unlock();
    LOG_INFO("[reactor] add fd=%d\n", fd);
}
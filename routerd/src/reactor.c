#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "reactor.h"
#include "event_queue.h"
#include "event.h"
#include "port_map.h"
// #include "ipc_server.h"
#include <pthread.h>
#include "plugin_loader.h"

static int epfd = -1;
static int reactor_fds[MAX_REACT_FDS];
// reactor_entry_t reactor_table[MAX_REACT_FDS] = {0};
// static int g_client_fd=-1;
// static int g_ipc_fd=-1;

// 初始化 reactor
void reactor_init(void)
{
    epfd = epoll_create(64);
    if (epfd < 0) {
        printf("[reactor] epoll_create failed\n");
        return;
    }

    // for (int i = 0; i < MAX_REACT_FDS; i++){
    //     reactor_fds[i]=-1;
    // }
    printf("[reactor] init ok\n");
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

    printf("[reactor] all ports cleared\n");
}

// reactor 主事件循环
void* reactor_thread(void* arg)
{
    struct epoll_event evs[16];
    printf("[reactor] thread started\n");

    while (1) {
        int n = epoll_wait(epfd, evs, 16, -1);
        if (n <= 0) continue;

        for (int i = 0; i < n; i++) {

            port_def_t* port=evs[i].data.ptr;
            int fd=port->base.fd;
            // ================================
            // ★ 普通数据端口读取路径
            // ================================
            uint8_t buf[1024];
            int len = read(fd, buf, sizeof(buf));

            if (len <= 0)
                continue;

            printf("[reactor] recv from %s fd=%d len=%d\n",
             port->base.name, fd, len);

            printf("[reactor] data: %s\n",buf);

            route_def_t* routes[16];
            int n = config_find_routes_by_src(port->base.name, routes, 16);

            for (int i = 0; i < n; i++) {
                route_def_t* r = routes[i];

                plugin_handler_t handler=plugin_get_handler(r->handler);
                int data_len=len;
                if(handler){
                data_len=handler(buf,len);// handler function
            }
            printf("Route: %s -> %s handler=%s plugin=%s\n",
               r->src, r->dst, r->handler, r->plugin);

            event_msg_t msg;
            // msg.dst=r->dst;
            memcpy(msg.dst,r->dst,sizeof(msg.dst));
            msg.len = data_len;
            memcpy(msg.data, buf, data_len);
            queue_push(&msg);

           printf("push message to queue\n");
        }
    }
}

return NULL;
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
    printf("[reactor] add fd=%d\n", fd);
}

void reactor_add_port(port_def_t* port){

   if(!port) return;

   int fd=port->base.fd;
   if(fd<0) return;

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
   printf("[reactor] add fd=%d\n", fd);
}
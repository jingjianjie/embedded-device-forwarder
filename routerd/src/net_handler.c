#include "net_handler.h"
#include "event.h"
#include "event_queue.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "log.h"

static void set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int net_tcp_server_create(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("[NET] TCP socket error"); return -1; }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[NET] TCP bind error");
        close(fd);
        return -1;
    }

    listen(fd, 8);
    set_nonblock(fd);

    printf("[NET] TCP server listening on port %d, fd=%d\n", port, fd);
    return fd;
}

int net_udp_create(int port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("[NET] UDP socket error"); return -1; }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[NET] UDP bind error");
        close(fd);
        return -1;
    }

    set_nonblock(fd);

    LOG_INFO("[NET] UDP listening on port %d, fd=%d\n", port, fd);
    return fd;
}

void net_handle_tcp(int fd)
{
    char buf[256];
    int n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) return;

    event_msg_t ev;
    ev.type = EVT_NET_IN;
    ev.len = n;
    memcpy(ev.data, buf, n);

    LOG_INFO("[NET] TCP received %d bytes\n", n);
    queue_push(&ev);
}

void net_handle_udp(int fd)
{
    char buf[256];
    struct sockaddr_in src;
    socklen_t len = sizeof(src);

    int n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr*)&src, &len);
    if (n <= 0) return;

    event_msg_t ev;
    ev.type = EVT_NET_IN;
    ev.len = n;
    memcpy(ev.data, buf, n);

    LOG_INFO("[NET] UDP received %d bytes\n", n);
    queue_push(&ev);
}

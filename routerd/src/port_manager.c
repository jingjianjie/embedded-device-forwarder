#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "port_manager.h"

// ========================================================
//  打开 TTY 端口 (/dev/ttyS0, /dev/ttyUSB0, /dev/ttyACM0, /dev/ttyGS0...)
// ========================================================
static int port_open_tty(port_def_t* p)
{
    int fd = open(p->cfg.tty.path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror("[port_tty] open");
        return -1;
    }

    struct termios tio;
    tcgetattr(fd, &tio);

    // 设置波特率
    cfsetispeed(&tio, p->cfg.tty.baudrate);
    cfsetospeed(&tio, p->cfg.tty.baudrate);

    // 数据位
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= (p->cfg.tty.databits == 7) ? CS7 : CS8;

    // 停止位
    if (p->cfg.tty.stopbits == 2)
        tio.c_cflag |= CSTOPB;
    else
        tio.c_cflag &= ~CSTOPB;

    // 奇偶校验
    if (p->cfg.tty.parity == PARITY_NONE) {
        tio.c_cflag &= ~PARENB;
    } else if (p->cfg.tty.parity == PARITY_ODD) {
        tio.c_cflag |= PARENB | PARODD;
    } else {
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
    }

    // 流控
    if (p->cfg.tty.flow == FLOW_RTSCTS)
        tio.c_cflag |= CRTSCTS;
    else
        tio.c_cflag &= ~CRTSCTS;

    // 设置属性
    tcsetattr(fd, TCSANOW, &tio);

    return fd;
}

// ========================================================
//  打开 TCP Server
// ========================================================
static int port_open_tcp_server(port_def_t* p)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("[port_tcp_server] socket");
        return -1;
    }

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(p->cfg.tcp_server.bind_addr);
    addr.sin_port = htons(p->cfg.tcp_server.port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[port_tcp_server] bind");
        close(fd);
        return -1;
    }

    listen(fd, p->cfg.tcp_server.backlog);
    return fd;
}

// ========================================================
//  打开 TCP Client
// ========================================================
static int port_open_tcp_client(port_def_t* p)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("[port_tcp_client] socket");
        return -1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(p->cfg.tcp_client.addr);
    addr.sin_port = htons(p->cfg.tcp_client.port);

    connect(fd, (struct sockaddr*)&addr, sizeof(addr));

    return fd;
}

// ========================================================
//  打开 UDP
// ========================================================
static int port_open_udp(port_def_t* p)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("[port_udp] socket");
        return -1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(p->cfg.udp.bind_addr);
    addr.sin_port = htons(p->cfg.udp.port);

    bind(fd, (struct sockaddr*)&addr, sizeof(addr));

    return fd;
}

// ========================================================
//  USB（等同于 TTY）
// ========================================================
static int port_open_usb(port_def_t* p)
{
    return port_open_tty(p);
}

// ========================================================
//  打开单个端口（统一调度）
// ========================================================
int port_open_single(port_def_t* p)
{
    int fd = -1;

    switch (p->base.type) {
        case PORT_TTY:
            fd = port_open_tty(p);
            break;
        case PORT_TCP_SERVER:
            fd = port_open_tcp_server(p);
            break;
        case PORT_TCP_CLIENT:
            fd = port_open_tcp_client(p);
            break;
        case PORT_UDP:
            fd = port_open_udp(p);
            break;
        case PORT_USB:
            fd = port_open_usb(p);
            break;
        default:
            printf("[port] unknown type %d\n", p->base.type);
            return -1;
    }

    if (fd >= 0) {
        p->base.fd = fd;
        // reactor_add_fd(fd);
        printf("[port] opened %s (fd=%d)\n", p->base.name, fd);
    }

    return fd;
}

// ========================================================
//  打开所有端口
// ========================================================
void ports_open_all()
{
    for (int i = 0; i < g_config.port_count; i++)
        port_open_single(&g_config.ports[i]);
}

// ========================================================
//  关闭单个端口
// ========================================================
void port_close_single(port_def_t* p)
{
    if (p->base.fd >= 0) {
        close(p->base.fd);
        p->base.fd = -1;
    }
}

// ========================================================
//  关闭所有端口
// ========================================================
void ports_close_all()
{
    for (int i = 0; i < g_config.port_count; i++)
        port_close_single(&g_config.ports[i]);
}

// ========================================================
//  根据端口名查找
// ========================================================
port_def_t* port_find_by_name(const char* name)
{
    for (int i = 0; i < g_config.port_count; i++) {
        if (strcmp(g_config.ports[i].base.name, name) == 0)
            return &g_config.ports[i];
    }
    return NULL;
}

//=========================================================
// get port fd
//=========================================================

int port_get_fd_by_name(const char* name){
     for (int i = 0; i < g_config.port_count; i++) {
        if (strcmp(g_config.ports[i].base.name, name) == 0)
            return g_config.ports[i].base.fd;
    }
    return -1;
}





// ========================================================
//  根据 fd 查找（用于 reactor）
// ========================================================
port_def_t* port_find_by_fd(int fd)
{
    for (int i = 0; i < g_config.port_count; i++) {
        if (g_config.ports[i].base.fd == fd)
            return &g_config.ports[i];
    }
    return NULL;
}




int port_send(port_def_t* p, const uint8_t* data, int len)
{
    if (!p || p->base.fd < 0) return -1;

    int fd = p->base.fd;

    switch (p->base.type)
    {
        case PORT_TTY:
        case PORT_USB:
            return write(fd, data, len);

        case PORT_TCP_CLIENT:
            return write(fd, data, len);

        case PORT_TCP_SERVER:
            // 广播给所有已连接客户端
            // for (int i = 0; i < p->cfg.tcp_server.client_count; i++) {
            //     int cfd = p->cfg.tcp_server.clients[i];
            //     if (cfd >= 0) write(cfd, data, len);
            // }
            return len;

        case PORT_UDP: {
            // struct sockaddr_in* addr = &p->cfg.udp.peer_addr;
            // return sendto(fd, data, len, 0,
                    // (struct sockaddr*)addr, sizeof(*addr));
            return 0;
        }

        default:
            printf("[port] unknown type = %d\n", p->base.type);
            return -1;
    }
}

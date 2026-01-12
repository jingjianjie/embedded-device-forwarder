#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>

#include "port_manager.h"
#include "log.h"

port_entry_t g_port_table[MAX_PORTS];//define ports
// ========================================================
//  打开 TTY 端口 (/dev/ttyS0, /dev/ttyUSB0, /dev/ttyACM0, /dev/ttyGS0...)
// ========================================================
static speed_t baud_to_speed(int baud)
{
    switch (baud) {
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    default: return B115200; // 或者返回 0 表示不支持
    }
}

static int port_open_tty(port_def_t* p)
{
    int fd = open(p->cfg.tty.path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror("[port_tty] open");
        return -1;
    }

    struct termios tio;
    if (tcgetattr(fd, &tio) != 0) {
        perror("[port_tty] tcgetattr");
        close(fd);
        return -1;
    }

    // 1) 先切 raw（关键：关掉 echo/icanon/onlcr/icrnl/ixon 等）
    cfmakeraw(&tio);

    // 2) 明确关掉回显/规范输入（双保险）
    tio.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ICANON | ISIG | IEXTEN);
    tio.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR);
    tio.c_oflag &= ~(OPOST);        // 禁止 \n -> \r\n 等转换

    // 3) 波特率（注意：用 Bxxxx 宏）
    speed_t spd = baud_to_speed(p->cfg.tty.baudrate);
    if (cfsetispeed(&tio, spd) != 0 || cfsetospeed(&tio, spd) != 0) {
        perror("[port_tty] cfsetispeed/cfsetospeed");
        // USB CDC 这里失败也未必致命，但建议处理
    }

    // 4) 8N1/7E1 等
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= (p->cfg.tty.databits == 7) ? CS7 : CS8;

    if (p->cfg.tty.stopbits == 2) tio.c_cflag |= CSTOPB;
    else                          tio.c_cflag &= ~CSTOPB;

    if (p->cfg.tty.parity == PARITY_NONE) {
        tio.c_cflag &= ~PARENB;
    } else if (p->cfg.tty.parity == PARITY_ODD) {
        tio.c_cflag |= (PARENB | PARODD);
    } else { // even
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
    }

    // 5) 流控
    if (p->cfg.tty.flow == FLOW_RTSCTS) tio.c_cflag |= CRTSCTS;
    else                               tio.c_cflag &= ~CRTSCTS;

    // 6) 本地连接 + 使能接收
    tio.c_cflag |= (CLOCAL | CREAD);

    // 7) 非阻塞 read 的“交付策略”（按你需求）
    //    这里设置成：至少1字节就返回，永不超时等待
    tio.c_cc[VMIN]  = 1;
    tio.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        perror("[port_tty] tcsetattr");
        close(fd);
        return -1;
    }

    // 可选：清一下缓冲
    tcflush(fd, TCIOFLUSH);

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

    if(listen(fd, p->cfg.tcp_server.backlog)<0){
        perror("[Tcp server] listen");
        close(fd);
    }
    LOG_INFO("listen %s \n",p->cfg.tcp_server);
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
//  ipc open ports
// ========================================================

int port_open_ipc_server(port_def_t* p)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, p->cfg.ipc.path, sizeof(addr.sun_path)-1);

    // 删除残留 socket 文件
    unlink(p->cfg.ipc.path);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 5) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    p->base.fd   = fd;
    p->base.type = PORT_IPC_SERVER;

    LOG_INFO("[ipc] listen %s fd=%d\n", p->cfg.ipc.path, fd);
    return fd;
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
            LOG_INFO("port_open_tty\n");
            break;
        case PORT_TCP_SERVER:
            fd = port_open_tcp_server(p);
            LOG_INFO("port_open_tcp_server\n");
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
        case PORT_IPC_SERVER:
            fd=port_open_ipc_server(p);
        default:
            LOG_INFO("[port] unknown type %d\n", p->base.type);
            return -1;
    }

    if (fd >= 0) {
        p->base.fd = fd;
        // reactor_add_fd(fd);
        LOG_INFO("[port] opened %s (fd=%d)\n", p->base.name, fd);
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
    LOG_INFO("[port_send] send message to dest\n");
    int status=-1;
    switch (p->base.type)
    {
    case PORT_TTY:{
        status=write(fd,data,len);
        break;
    }
case PORT_USB:{
    status= write(fd, data, len);
    break;
}

case PORT_TCP_CLIENT:{
    status=write(fd, data, len);
    break;
}
case PORT_TCP_CLIENT:{
    status=write(fd,data,len);
    break;
}

case PORT_TCP_SERVER:{
    status=len;
    break;
}
case PORT_TCP_CLIENT:{
    status=write(fd,data,len);
}

case PORT_UDP: {
            // struct sockaddr_in* addr = &p->cfg.udp.peer_addr;
            // return sendto(fd, data, len, 0,
                    // (struct sockaddr*)addr, sizeof(*addr));
    status=0;
    break;
}

default:{

    status=-1;
    break;
}
}

LOG_INFO("[port_send] send message finish\n");
return status;
}



port_def_t* port_find(const char* name)
{
    if (!name) return NULL;

    for (int i = 0; i < MAX_PORTS; i++) {
        if (!g_port_table[i].used)
            continue;

        port_def_t* port = g_port_table[i].port;
        if (!port)
            continue;

        if (strcmp(port->base.name, name) == 0) {
            if (port->base.type == PORT_TCP_SERVER)
                continue;
            else if(port->base.type==PORT_IPC_SERVER)
                continue;
            else
                return port;
        }
    }

    return NULL;
}



// port_def_t* reactor_find_port(int fd)
// {
//     for (int i = 0; i < MAX_REACTOR_PORTS; i++) {
//         if (g_reactor_table[i].used && g_reactor_table[i].fd == fd)
//             return g_reactor_table[i].port;
//     }
//     return NULL;
// }
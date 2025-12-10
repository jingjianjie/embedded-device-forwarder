#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "port_map.h"
#include "event.h"

// ====================================================
// 端口定义表
// ====================================================
//
// 根据你的硬件修改：
// PORT_USB1  -> /dev/ttyGS0   (USB Gadget Serial)
// PORT_UART1 -> /dev/ttyS1
// PORT_UART2 -> /dev/ttyS2
// PORT_NET0  -> /dev/tap0     (可作为虚拟网口)
// ====================================================

typedef struct {
    int port_id;              // 端口 ID（PORT_USB1 等）
    const char* dev_path;     // 对应的设备文件
    int fd;                   // 打开的 FD
} port_entry_t;

static port_entry_t port_table[] = {
    { PORT_USB1,  "/dev/ttyGS0", -1 },
    { PORT_UART1, "/dev/ttyS1",  -1 },
    { PORT_UART2, "/dev/ttyS2",  -1 },
    { PORT_NET0,  "/dev/tap0",   -1 },
};

static int port_table_size = sizeof(port_table) / sizeof(port_table[0]);

// ====================================================
// 初始化 port_table（daemon 启动时执行）
// ====================================================
void port_map_init()
{
    for (int i = 0; i < port_table_size; i++)
        port_table[i].fd = -1;
}

// ====================================================
// 端口名字符串 → 端口 ID
// 用于 JSON 配置文件中 "in_port": "USB1"
// ====================================================
int port_str_to_id(const char* name)
{
    if (strcasecmp(name, "USB1") == 0)  return PORT_USB1;
    if (strcasecmp(name, "UART1") == 0) return PORT_UART1;
    if (strcasecmp(name, "UART2") == 0) return PORT_UART2;
    if (strcasecmp(name, "NET0") == 0)  return PORT_NET0;

    return -1;
}

// ====================================================
// 打开端口（USB/UART/NET）
// reactor_add_fd_by_port 会来调用这个
// ====================================================
int port_open_fd(int port)
{
    for (int i = 0; i < port_table_size; i++) {

        if (port_table[i].port_id == port) {

            // 如果 fd 已经打开，直接返回
            if (port_table[i].fd >= 0)
                return port_table[i].fd;

            printf("[port_map] opening %s for port %d...\n",
                port_table[i].dev_path, port);

            int fd = open(port_table[i].dev_path, O_RDWR | O_NONBLOCK);
            if (fd < 0) {
                printf("[port_map] failed to open %s\n",
                    port_table[i].dev_path);
                return -1;
            }

            port_table[i].fd = fd;
            return fd;
        }
    }

    printf("[port_map] unknown port %d\n", port);
    return -1;
}

// ====================================================
// 根据 port ID 取 fd
// ====================================================
int port_to_fd(int port)
{
    for (int i = 0; i < port_table_size; i++)
        if (port_table[i].port_id == port)
            return port_table[i].fd;

    return -1;
}

// ====================================================
// 根据 fd 找回端口编号
// reactor_thread 用于解析事件来源
// ====================================================
int fd_to_port(int fd)
{
    for (int i = 0; i < port_table_size; i++)
        if (port_table[i].fd == fd)
            return port_table[i].port_id;

    return -1;
}

// ====================================================
// 端口 → 事件类型
// reactor_thread 用来填写 event_msg_t.type
// ====================================================
int port_to_event_type(int port)
{
    switch (port) {

    case PORT_USB1:
        return EVT_USB_IN;

    case PORT_UART1:
    case PORT_UART2:
        return EVT_UART_IN;

    default:
        // 如果以后添加 SPI、I2C、CAN，也可以扩展
        return EVT_USB_IN;
    }
}

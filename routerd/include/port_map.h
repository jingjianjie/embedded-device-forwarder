#ifndef PORT_MAP_H
#define PORT_MAP_H

#include <stdint.h>

// =============================
// 固定端口 ID（全局统一）
// =============================
enum {
    PORT_USB1  = 1,
    PORT_UART1 = 2,
    PORT_UART2 = 3,
    PORT_NET0  = 4,
};

int port_to_fd(int port);
int fd_to_port(int fd);

int port_str_to_id(const char* name);

void port_map_init();

int port_open_fd(int port);
int port_to_event_type(int port);

#endif

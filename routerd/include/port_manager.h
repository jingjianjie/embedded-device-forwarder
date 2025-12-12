#ifndef PORT_MANAGER_H
#define PORT_MANAGER_H

#include "config_store.h"   // 需要 g_config 和 port_def_t
// #include "reactor.h"        // 需要 reactor_add_fd()

// 打开单个端口
int port_open_single(port_def_t* p);

void ports_open_all();

// 关闭单个端口
void port_close_single(port_def_t* p);

// 关闭所有端口
void ports_close_all();

// 根据端口名查找
port_def_t* port_find_by_name(const char* name);

// 根据 fd 查找端口
port_def_t* port_find_by_fd(int fd);

int port_get_fd_by_name(const char* name);

int port_send(port_def_t* p, const uint8_t* data, int len);


// #define MAX_PORTS 128
typedef struct{
	int used;
	int fd;
	port_def_t* port;
}port_entry_t;

extern port_entry_t g_port_table[MAX_PORTS];

port_def_t* port_find(const char* name);
#endif

#ifndef REACTOR_H
#define REACTOR_H

#include <stdint.h>
#include "config_store.h"
#include "port_manager.h"

#define MAX_REACT_FDS 64

// 初始化
void reactor_init(void);

// 向 epoll 添加 fd
// void reactor_add_fd(int fd);

// 根据 port 打开 fd 并添加到 epoll
void reactor_add_fd_by_port(int port);

// reactor 主线程
void* reactor_thread(void* arg);

// 清空所有 epoll 端口（重置路由使用）
void reactor_clear_all_ports(void);
void reactor_clear_fds();

//thread safe
void reactor_lock();
void reactor_unlock();

typedef int (*handler_t)(uint8_t* data,int len);
void reactor_add_fd(int fd);

// typedef struct{
// 	int used;
// 	int fd;
// 	port_def*t port;
// }reactor_entry_t;

void reactor_add_port(port_def_t* port);

// port_def_t* reactor_find_port(int fd);

// extern reactor_entry_t reactor_table[MAX_REACT_FDS];

#endif

#ifndef NET_HANDLER_H
#define NET_HANDLER_H

int net_tcp_server_create(int port);
int net_udp_create(int port);

void net_handle_tcp(int fd);
void net_handle_udp(int fd);

#endif

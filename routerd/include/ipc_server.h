#ifndef IPC_SERVER_H
#define IPC_SERVER_H

// ipc_server.h — IPC 入口(RPD 阶段 1 任务 C-2 后已切到新协议)
//
// 监听 AF_UNIX SOCK_STREAM,每个连接由 ipc_thread 内的 inner loop 流式读,
// 调用 proto_decode → proto_dispatch。socket path 默认
// /run/ez_router/ez_router.sock(由 ez_router.c 启动时传入)。
//
// 旧 ipc_header_t / ez_router_link_t / ipc_payload_* / ipc_handle_frame /
// process_frame 已删除(R-5 无前向兼容,见 doc/router_protocol.md §6)。
//
// 注意 SDK 侧 sdk/ez_router_sdk.h 的 ipc_header_t 副本未删,SDK 在
// 阶段 4 重写时同步淘汰(届时与 routerd 这一侧的协议自然对齐)。

// 初始化 IPC server,bind+listen,返回 server fd(失败 -1)
int ipc_server_init(const char* sockpath);

// IPC 主线程入口:accept + 流式读 + dispatch
void* ipc_thread(void* arg);

#endif

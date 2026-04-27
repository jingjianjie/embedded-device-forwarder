#ifndef EZ_ROUTER_PROTO_DISPATCHER_H
#define EZ_ROUTER_PROTO_DISPATCHER_H

// proto_dispatcher.h — 帧分发(RPD 阶段 1 任务 C-2)
//
// caller(ipc_server)在 proto_decode 返回 ok 后,调用 proto_dispatch
// 把帧路由到对应模块(registry / log_sink-stub / supervisor-stub)。
// hdr 与 payload 由 caller 持有(payload 指向 caller 的 read buffer 内部),
// dispatcher 不持久化任何指针。
//
// ACK 编码只对 PROTO_REGISTER 触发,seq 与请求帧相同(R-3 关联约束)。

#include "protocol.h"

// 返回:
//   0  正常处理(包括"未知 cmd"等容错路径)
//   -1 致命:caller 应断连(目前仅在 ACK write 失败时可能,reactor 阻塞 TODO)
int proto_dispatch(int client_fd,
                   const proto_frame_hdr_t* hdr,
                   const uint8_t* payload);

#endif // EZ_ROUTER_PROTO_DISPATCHER_H

// ipc_server.c — IPC 入口实现(RPD 阶段 1 C-2 切换到新协议)
//
// 数据流:
//   accept() → 每连接独立 read buffer → 循环 proto_decode():
//     INCOMPLETE → break inner loop, 继续 recv (契约 21)
//     其他负值   → 断连(致命错误,流损坏)
//     ok        → proto_dispatch()  → memmove 推进读指针
//
// buffer 大小: PROTO_HDR_SIZE + PROTO_MAX_PAYLOAD = 12 + 65536 ≈ 64 KiB,
//   单个连接独占,放在 ipc_thread 的栈上(默认线程栈 8MB,足够)。
//
// 阻塞性: 当前 IPC 线程是阻塞 socket。慢客户端 recv 卡住只影响 IPC 线程,
//   不直接影响 reactor。dispatch 内部 ACK write 也是阻塞,见 dispatcher
//   文件头 TODO。

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include "ipc_server.h"
#include "protocol.h"
#include "proto_dispatcher.h"
#include "registry.h"
#include "log.h"
#include "run_state.h"

#define IPC_RECV_BUF_SIZE (PROTO_HDR_SIZE + PROTO_MAX_PAYLOAD)

static int g_ipc_fd = -1;

int ipc_server_init(const char* sockpath)
{
    unlink(sockpath);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    // sun_path 仅 108 字节(linux),不够长直接截断比 stack smash 安全
    strncpy(addr.sun_path, sockpath, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        return -1;

    listen(fd, 5);

    g_ipc_fd = fd;
    return fd;
}

// 单连接处理:阻塞读 → 循环 decode → dispatch。返回时连接已关闭。
static void handle_client(int client_fd)
{
    uint8_t  buf[IPC_RECV_BUF_SIZE];
    size_t   buf_len = 0;

    while (run_state_is_running()) {
        ssize_t n = recv(client_fd, buf + buf_len,
                         IPC_RECV_BUF_SIZE - buf_len, 0);

        if (n == 0) {
            LOG_INFO("[IPC] client fd=%d disconnected\n", client_fd);
            break;
        }
        if (n < 0) {
            if (errno == EINTR) continue;
            LOG_WARN("[IPC] recv fd=%d errno=%d\n", client_fd, errno);
            break;
        }

        buf_len += (size_t)n;

        // 流式 decode:可能一次 recv 含多帧,循环到 INCOMPLETE 为止
        while (1) {
            proto_frame_hdr_t hdr;
            const uint8_t* payload = NULL;
            int r = proto_decode(buf, buf_len, &hdr, &payload);

            if (r == PROTO_ERR_INCOMPLETE) {
                // 契约 21: 不是错误,继续 recv
                break;
            }
            if (r < 0) {
                LOG_WARN("[IPC] decode err=%d on fd=%d, drop conn\n",
                         r, client_fd);
                buf_len = 0;
                goto done;  // 致命解码错 → 断连
            }

            // 成功,r = 整帧字节数
            proto_dispatch(client_fd, &hdr, payload);

            // 推进读指针:把剩余字节移到 buf 头
            size_t consumed = (size_t)r;
            if (consumed < buf_len)
                memmove(buf, buf + consumed, buf_len - consumed);
            buf_len -= consumed;
        }

        // buf 满但仍是 INCOMPLETE → 客户端发了超大帧或 buf 已塞满未消费
        // (前者已被 PAYLOAD_TOO_LARGE 拦截,后者理论不发生因为 dispatch
        //  会消费,但作为兜底加 sanity check)
        if (buf_len == IPC_RECV_BUF_SIZE) {
            LOG_WARN("[IPC] buffer full without progress on fd=%d, drop\n",
                     client_fd);
            break;
        }
    }

done:
    registry_unregister(client_fd);  // 槽位回收(可能未注册,registry 容错)
    close(client_fd);
}

void* ipc_thread(void* arg)
{
    (void)arg;
    LOG_INFO("[IPC] waiting for SDK...\n");

    while (run_state_is_running()) {
        int client_fd = accept(g_ipc_fd, NULL, NULL);
        if (client_fd < 0) {
            usleep(500 * 1000);  // 与原代码一致的非阻塞 accept 间隔
            continue;
        }
        LOG_INFO("[IPC] client connected, fd=%d\n", client_fd);
        handle_client(client_fd);
    }
    return NULL;
}

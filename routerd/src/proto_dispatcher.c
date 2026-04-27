// proto_dispatcher.c — 帧分发(RPD 阶段 1 任务 C-2)
//
// 设计:
//   - 纯 cmd 调度,不持有状态;调用 registry / log / supervisor 各家
//   - PROTO_REGISTER 必回 ACK,seq 与请求帧相同(R-3 / 契约 16)
//   - PROTO_HEARTBEAT 不回 ACK(高频,回 ACK 反向放大流量)
//   - DATA / CMD / LOG / FW_*  阶段 1 全部占位 LOG_*,不丢但也不处理
//   - 未知 cmd 容错 LOG_WARN + return 0(不断连)
//
// 阻塞性: write(fd, ack) 是阻塞 socket;慢客户端会卡住调用线程(目前 IPC
// 线程,不影响 reactor)。这违反契约 14 的精神(reactor 不可阻塞)。
// TODO: 阶段 2 改非阻塞 fd + 写队列,或在 IPC 线程独立维护 outbox。
//
// 测试: tests/unit/test_dispatcher.c

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "proto_dispatcher.h"
#include "registry.h"
#include "log.h"

// ACK payload 用最小 JSON,够 dispatcher 自己拼,不必引入 cJSON 编码器
// (cJSON_Print 会 malloc,且 ACK 字段固定,手拼更轻)。
static int build_register_ack_payload(int ok, char* buf, size_t cap)
{
    int n = snprintf(buf, cap, "{\"ok\":%s}", ok ? "true" : "false");
    if (n < 0 || (size_t)n >= cap) return -1;
    return n;
}

// 阻塞 write,慢客户端可能卡住。见文件头 TODO。
static int send_register_ack(int fd, uint32_t req_seq, int ok)
{
    char pl[32];
    int pl_len = build_register_ack_payload(ok, pl, sizeof(pl));
    if (pl_len < 0) return -1;

    uint8_t frame[PROTO_HDR_SIZE + 32];
    proto_frame_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.cmd         = PROTO_REGISTER_ACK;
    hdr.seq         = req_seq;          // 契约 16: ACK 与 REGISTER 配对
    hdr.payload_len = (uint32_t)pl_len;

    int n = proto_encode(&hdr, pl, frame, sizeof(frame));
    if (n < 0) {
        LOG_WARN("[dispatch] encode ACK failed: %d\n", n);
        return -1;
    }

    ssize_t w = write(fd, frame, (size_t)n);
    if (w != n) {
        LOG_WARN("[dispatch] write ACK partial: %zd/%d\n", w, n);
        return -1;
    }
    return 0;
}

int proto_dispatch(int client_fd,
                   const proto_frame_hdr_t* hdr,
                   const uint8_t* payload)
{
    switch (hdr->cmd) {

    case PROTO_REGISTER: {
        int rc = registry_register(client_fd,
                                   (const char*)payload,
                                   (int)hdr->payload_len);
        // ACK 失败仅 log,不上报致命 — 客户端 / 网络问题不该让 router 自杀
        send_register_ack(client_fd, hdr->seq, rc == 0);
        return 0;
    }

    case PROTO_HEARTBEAT:
        // 心跳仅刷新时间戳;未注册的 fd 收到 HB 是异常但容错
        if (registry_update_heartbeat(client_fd) < 0) {
            LOG_WARN("[dispatch] HEARTBEAT from unregistered fd=%d\n",
                     client_fd);
        }
        return 0;

    case PROTO_DATA:
        LOG_INFO("[dispatch] DATA len=%u (stage 2 stub)\n", hdr->payload_len);
        return 0;

    case PROTO_CMD:
        LOG_INFO("[dispatch] CMD len=%u (stage 2 stub)\n", hdr->payload_len);
        return 0;

    case PROTO_LOG:
        LOG_INFO("[dispatch] LOG len=%u (stage 3 stub)\n", hdr->payload_len);
        return 0;

    case PROTO_FW_BEGIN:
    case PROTO_FW_CHUNK:
    case PROTO_FW_END:
    case PROTO_FW_QUERY:
        LOG_WARN("[dispatch] FW cmd=0x%02x len=%u (stage 5 not implemented)\n",
                 hdr->cmd, hdr->payload_len);
        return 0;

    case PROTO_REGISTER_ACK:
        // router 是 ACK 的发起方,不应收到对端的 ACK
        LOG_WARN("[dispatch] unexpected REGISTER_ACK from fd=%d\n",
                 client_fd);
        return 0;

    default:
        LOG_WARN("[dispatch] unknown cmd=0x%02x len=%u, dropped\n",
                 hdr->cmd, hdr->payload_len);
        return 0;
    }
}

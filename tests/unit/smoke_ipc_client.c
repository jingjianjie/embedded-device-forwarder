// smoke_ipc_client.c — RPD 阶段 1 C-2 集成冒烟(临时,非持久测试)
//
// 不属于 test-as-doc 矩阵,只是为了验证 ez_router 的 IPC 入口链路:
//   socket → REGISTER → 读 ACK → HEARTBEAT → 关闭。
// 用法(target 上):
//   ./out/ez_router -log &  (启 daemon)
//   gcc -Wall -D_GNU_SOURCE -I routerd/include routerd/src/proto_codec.c tests/unit/smoke_ipc_client.c -o /tmp/smoke
//   /tmp/smoke
// 退出码 0 = ACK 收到且 ok=true,非 0 = 失败。

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "protocol.h"

int main(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    strncpy(addr.sun_path, "/run/ez_router/ez_router.sock", sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect"); return 2;
    }

    const char* json = "{\"device_id\":\"SMOKE-1\",\"model\":\"smoke\","
                       "\"fw_version\":\"0.0.1\",\"build_date\":\"2026-04-28\"}";
    uint8_t frame[256];
    proto_frame_hdr_t hdr = {.cmd = PROTO_REGISTER, .seq = 42,
                             .payload_len = (uint32_t)strlen(json)};
    int n = proto_encode(&hdr, json, frame, sizeof(frame));
    if (n < 0) { fprintf(stderr, "encode failed: %d\n", n); return 3; }

    if (send(fd, frame, n, 0) != n) { perror("send REGISTER"); return 4; }
    printf("sent REGISTER %d bytes (seq=42)\n", n);

    // 读 ACK
    uint8_t ack_buf[256];
    ssize_t r = recv(fd, ack_buf, sizeof(ack_buf), 0);
    if (r <= 0) { perror("recv ACK"); return 5; }
    printf("recv %zd bytes\n", r);

    proto_frame_hdr_t ack_hdr;
    const uint8_t* ack_pl = NULL;
    int dr = proto_decode(ack_buf, (size_t)r, &ack_hdr, &ack_pl);
    if (dr <= 0) { fprintf(stderr, "decode ACK failed: %d\n", dr); return 6; }
    printf("ACK cmd=0x%02x seq=%u plen=%u payload=%.*s\n",
           ack_hdr.cmd, ack_hdr.seq, ack_hdr.payload_len,
           (int)ack_hdr.payload_len, (const char*)ack_pl);

    if (ack_hdr.cmd != PROTO_REGISTER_ACK) return 7;
    if (ack_hdr.seq != 42) return 8;
    if (memmem(ack_pl, ack_hdr.payload_len, "true", 4) == NULL) {
        fprintf(stderr, "ACK does not say ok:true\n"); return 9;
    }

    // HEARTBEAT(无 ACK)
    proto_frame_hdr_t hb = {.cmd = PROTO_HEARTBEAT, .seq = 43, .payload_len = 0};
    int hn = proto_encode(&hb, NULL, frame, sizeof(frame));
    if (send(fd, frame, hn, 0) != hn) { perror("send HB"); return 10; }
    printf("sent HEARTBEAT %d bytes\n", hn);

    close(fd);
    printf("OK\n");
    return 0;
}

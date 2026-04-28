// smoke_supervised_child.c — RPD 阶段 2 D-1 集成冒烟伴侣
//
// 目的:配合本会话方案的"双 ssh + 真实 IPC"smoke 路径。
//   ssh 1: ./ez_router -debug
//   ssh 2: ./smoke_supervised_child &
//   ssh 3: kill -STOP <child_pid>     (停心跳但不死,触发 stale)
//   ssh 4: tail ez_router.log         (5s+ 后看 [sup] stale heartbeat WARN)
//
// 行为:
//   connect /run/ez_router/ez_router.sock → REGISTER → 周期 1s 发 HEARTBEAT,
//   收 SIGTERM 干净退出。
//   仅靠 supervisor_check_heartbeats 周期检查 → 不需要 ez_router 主动 spawn。
//
// 编译运行(target 上,单行命令):
//   gcc -Wall -D_GNU_SOURCE -I routerd/include routerd/src/proto_codec.c tests/unit/smoke_supervised_child.c -o /tmp/smoke_supervised_child
//   setsid /tmp/smoke_supervised_child < /dev/null > /tmp/scsl.log 2>&1 &
//
// 约束:本程序故意不 fork 自 ez_router supervisor 表 — 这里测的是
//   "supervisor.check 检查 registry 中 *任何* 条目的心跳",与是否被 spawn 无关。

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "protocol.h"

static volatile int g_running = 1;

static void on_term(int sig) { (void)sig; g_running = 0; }

int main(int argc, char* argv[])
{
    const char* device_id = (argc > 1) ? argv[1] : "SMOKE-SUP-1";
    int hb_period_ms      = (argc > 2) ? atoi(argv[2]) : 1000;
    if (hb_period_ms <= 0) hb_period_ms = 1000;

    signal(SIGTERM, on_term);
    signal(SIGINT,  on_term);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    strncpy(addr.sun_path, "/run/ez_router/ez_router.sock", sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect"); return 2;
    }

    // REGISTER
    char json[256];
    int jl = snprintf(json, sizeof(json),
        "{\"device_id\":\"%s\",\"model\":\"smoke-sup\","
        "\"fw_version\":\"0.0.1\",\"build_date\":\"2026-04-28\"}", device_id);

    uint8_t frame[512];
    proto_frame_hdr_t hdr = {.cmd = PROTO_REGISTER, .seq = 1,
                             .payload_len = (uint32_t)jl};
    int n = proto_encode(&hdr, json, frame, sizeof(frame));
    if (n < 0) { fprintf(stderr, "encode: %d\n", n); return 3; }
    if (send(fd, frame, n, 0) != n) { perror("send REG"); return 4; }
    fprintf(stderr, "[smoke_child pid=%d] REGISTER sent device_id=%s\n",
            getpid(), device_id);

    // 读 ACK(简单消费,不深查)
    uint8_t ack[256];
    ssize_t r = recv(fd, ack, sizeof(ack), 0);
    if (r > 0) fprintf(stderr, "[smoke_child] ACK %zd bytes\n", r);

    // 周期 HEARTBEAT
    uint32_t seq = 2;
    while (g_running) {
        proto_frame_hdr_t hb = {.cmd = PROTO_HEARTBEAT, .seq = seq++,
                                .payload_len = 0};
        int hn = proto_encode(&hb, NULL, frame, sizeof(frame));
        if (send(fd, frame, hn, 0) != hn) {
            fprintf(stderr, "[smoke_child] send HB failed, exit\n");
            break;
        }
        usleep((useconds_t)hb_period_ms * 1000);
    }

    fprintf(stderr, "[smoke_child] exit\n");
    close(fd);
    return 0;
}

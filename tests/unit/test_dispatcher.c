// test_dispatcher.c — RPD 阶段 1 任务 C-2 的 test-as-doc
//
// 固化契约(routerd/include/proto_dispatcher.h + design-intent §2):
//   - PROTO_REGISTER → 落表 + REGISTER_ACK 帧(seq 与请求帧相同,契约 16)
//   - PROTO_HEARTBEAT → 更新 registry.last_heartbeat_ms
//   - 未知 cmd → 返回 0(容错,不断连)
//
// 用 socketpair 替代真 IPC fd:把 dispatcher 视作"在 fd 上写 ACK 的纯函数",
// 单测从 socket 另一端读出 ACK 帧并 decode 校验。
//
// §6.5 TEST AS DOC 形态。Unity 单测脚手架(open-questions U1)就绪后迁移。
//
// 编译运行(从仓库根目录):
//   gcc -Wall -Wextra -I routerd/include -I routerd/3rdparty/cjson routerd/src/registry.c routerd/src/proto_dispatcher.c routerd/src/proto_codec.c routerd/src/log.c routerd/3rdparty/cjson/cJSON.c tests/unit/test_dispatcher.c -lpthread -o /tmp/test_dispatcher
//   /tmp/test_dispatcher
//
// 退出码:0 = 全部 PASS,非 0 = FAIL

#define _GNU_SOURCE   // memmem
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "registry.h"
#include "proto_dispatcher.h"
#include "protocol.h"

static int g_failed = 0;
static int g_passed = 0;

#define EXPECT(cond, label) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s (line %d)\n", label, __LINE__); \
        g_failed++; \
    } else { \
        printf("PASS %s\n", label); \
        g_passed++; \
    } \
} while (0)

#define EXPECT_EQ_INT(actual, expected, label) do { \
    long _a = (long)(actual), _e = (long)(expected); \
    if (_a != _e) { \
        fprintf(stderr, "FAIL %s: got %ld, want %ld (line %d)\n", \
                label, _a, _e, __LINE__); \
        g_failed++; \
    } else { \
        printf("PASS %s\n", label); \
        g_passed++; \
    } \
} while (0)

static void registry_reset(void)
{
    for (int fd = 0; fd < 1024; fd++) registry_unregister(fd);
}

// === Test 1: REGISTER 帧 → registry 落表 + ACK 帧 seq 与请求一致 ===
static void test_register_dispatch_with_ack(void)
{
    registry_reset();
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        fprintf(stderr, "FAIL socketpair: errno\n"); g_failed++; return;
    }

    const char* json = "{\"device_id\":\"D1\",\"model\":\"m\","
                       "\"fw_version\":\"1.0\",\"build_date\":\"2026-04-27\"}";
    proto_frame_hdr_t hdr = {.cmd = PROTO_REGISTER, .seq = 0xCAFEBABEu,
                             .payload_len = (uint32_t)strlen(json)};
    int rc = proto_dispatch(sv[0], &hdr, (const uint8_t*)json);
    EXPECT_EQ_INT(rc, 0, "dispatch_register_returns_0");

    // registry 应落表
    const subproc_entry_t* e = registry_find_by_fd(sv[0]);
    EXPECT(e != NULL, "register_dispatched_to_registry");

    // 从 sv[1] 读 ACK 帧
    uint8_t buf[256];
    ssize_t n = read(sv[1], buf, sizeof(buf));
    EXPECT(n > (ssize_t)PROTO_HDR_SIZE, "ack_frame_received");

    proto_frame_hdr_t ack_hdr;
    const uint8_t* ack_pl = NULL;
    int dr = proto_decode(buf, (size_t)n, &ack_hdr, &ack_pl);
    EXPECT(dr > 0, "ack_decodable");
    EXPECT_EQ_INT(ack_hdr.cmd, PROTO_REGISTER_ACK, "ack_cmd");
    EXPECT_EQ_INT(ack_hdr.seq, (long)0xCAFEBABEu, "ack_seq_matches_request");

    // payload 应是 {"ok":true}
    EXPECT(ack_hdr.payload_len > 0, "ack_payload_nonempty");
    EXPECT(memmem(ack_pl, ack_hdr.payload_len, "true", 4) != NULL,
           "ack_payload_has_ok_true");

    close(sv[0]); close(sv[1]);
}

// === Test 2: REGISTER 失败 → ACK 携带 ok:false ===
static void test_register_fail_ack(void)
{
    registry_reset();
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        fprintf(stderr, "FAIL socketpair\n"); g_failed++; return;
    }

    const char* bad = "{not json}";
    proto_frame_hdr_t hdr = {.cmd = PROTO_REGISTER, .seq = 7,
                             .payload_len = (uint32_t)strlen(bad)};
    proto_dispatch(sv[0], &hdr, (const uint8_t*)bad);

    uint8_t buf[256];
    ssize_t n = read(sv[1], buf, sizeof(buf));
    EXPECT(n > 0, "ack_received_for_failed_register");

    proto_frame_hdr_t ack_hdr;
    const uint8_t* ack_pl = NULL;
    proto_decode(buf, (size_t)n, &ack_hdr, &ack_pl);
    EXPECT_EQ_INT(ack_hdr.seq, 7, "fail_ack_seq_matches");
    EXPECT(memmem(ack_pl, ack_hdr.payload_len, "false", 5) != NULL,
           "fail_ack_payload_has_ok_false");

    close(sv[0]); close(sv[1]);
}

// === Test 3: HEARTBEAT 更新时间戳 + 不发 ACK ===
static void test_heartbeat_dispatch(void)
{
    registry_reset();
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        fprintf(stderr, "FAIL socketpair\n"); g_failed++; return;
    }

    // 先注册
    const char* json = "{\"device_id\":\"H1\",\"model\":\"m\","
                       "\"fw_version\":\"1.0\",\"build_date\":\"2026-04-27\"}";
    proto_frame_hdr_t reg = {.cmd = PROTO_REGISTER, .seq = 1,
                             .payload_len = (uint32_t)strlen(json)};
    proto_dispatch(sv[0], &reg, (const uint8_t*)json);
    // 消费 ACK
    uint8_t junk[64]; read(sv[1], junk, sizeof(junk));

    const subproc_entry_t* e1 = registry_find_by_fd(sv[0]);
    uint64_t t0 = e1 ? e1->last_heartbeat_ms : 0;

    usleep(5000);
    proto_frame_hdr_t hb = {.cmd = PROTO_HEARTBEAT, .seq = 2, .payload_len = 0};
    int rc = proto_dispatch(sv[0], &hb, NULL);
    EXPECT_EQ_INT(rc, 0, "dispatch_heartbeat_returns_0");

    const subproc_entry_t* e2 = registry_find_by_fd(sv[0]);
    EXPECT(e2 != NULL && e2->last_heartbeat_ms > t0,
           "heartbeat_advances_timestamp");

    // sv[1] 不应有数据(HB 不回 ACK):用 MSG_DONTWAIT 非阻塞读
    char b;
    ssize_t r = recv(sv[1], &b, 1, MSG_DONTWAIT);
    EXPECT(r < 0, "heartbeat_no_ack_emitted");

    close(sv[0]); close(sv[1]);
}

// === Test 4: 未知 cmd 容错 ===
static void test_unknown_cmd(void)
{
    registry_reset();
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        fprintf(stderr, "FAIL socketpair\n"); g_failed++; return;
    }

    proto_frame_hdr_t hdr = {.cmd = 0xFE, .seq = 0, .payload_len = 0};
    int rc = proto_dispatch(sv[0], &hdr, NULL);
    EXPECT_EQ_INT(rc, 0, "unknown_cmd_returns_0_no_disconnect");

    // 不应写 ACK
    char b;
    ssize_t r = recv(sv[1], &b, 1, MSG_DONTWAIT);
    EXPECT(r < 0, "unknown_cmd_no_ack_emitted");

    close(sv[0]); close(sv[1]);
}

// === Test 5: stub cmd(DATA / LOG / FW_*) 返回 0 不写 ACK ===
static void test_stub_cmds(void)
{
    registry_reset();
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        fprintf(stderr, "FAIL socketpair\n"); g_failed++; return;
    }

    uint8_t cmds[] = {PROTO_DATA, PROTO_CMD, PROTO_LOG,
                      PROTO_FW_BEGIN, PROTO_FW_CHUNK,
                      PROTO_FW_END, PROTO_FW_QUERY};
    int all_ok = 1;
    for (size_t i = 0; i < sizeof(cmds)/sizeof(cmds[0]); i++) {
        proto_frame_hdr_t hdr = {.cmd = cmds[i], .seq = (uint32_t)i,
                                  .payload_len = 0};
        if (proto_dispatch(sv[0], &hdr, NULL) != 0) { all_ok = 0; break; }
    }
    EXPECT(all_ok, "all_stub_cmds_return_0");

    char b;
    ssize_t r = recv(sv[1], &b, 1, MSG_DONTWAIT);
    EXPECT(r < 0, "stub_cmds_no_ack_emitted");

    close(sv[0]); close(sv[1]);
}

int main(void)
{
    registry_init();

    test_register_dispatch_with_ack();
    test_register_fail_ack();
    test_heartbeat_dispatch();
    test_unknown_cmd();
    test_stub_cmds();

    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}

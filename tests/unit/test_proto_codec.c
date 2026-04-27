// test_proto_codec.c — RPD 阶段 1 任务 C-1 的 test-as-doc
//
// 固化契约(doc/router_protocol.md 帧格式 + 错误码表):
//   - LE 主机序直拷,12 字节帧头
//   - magic = 0xEB7C,version = 1
//   - PROTO_MAX_PAYLOAD = 65536(64 KiB)
//   - decode 零拷贝(payload_out 指向 in_buf 内部)
//   - INCOMPLETE 与 BAD_MAGIC / BAD_VERSION 严格区分(caller 行为不同)
//
// 这是 §6.5 TEST AS DOC 形态,等 Unity 单测脚手架就绪(open-questions U1)
// 后迁移过去。
//
// 编译运行(从仓库根目录),命令在一行:
//   gcc -Wall -Wextra -I routerd/include routerd/src/proto_codec.c tests/unit/test_proto_codec.c -o /tmp/test_proto_codec
//   /tmp/test_proto_codec
//
// 退出码:0 = 全部 PASS,非 0 = FAIL

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"

// --- 极简测试框架(零依赖) ---
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

// --- helper: 用 known-good buffer 直接造一帧,绕开 proto_encode ---
// 用于专门测试 decode 行为(独立于 encode 正确性)
static size_t craft_frame(uint8_t* buf, uint16_t magic, uint8_t ver, uint8_t cmd,
                          uint32_t seq, uint32_t plen,
                          const uint8_t* payload)
{
    memcpy(buf + 0, &magic, 2);
    buf[2] = ver;
    buf[3] = cmd;
    memcpy(buf + 4, &seq, 4);
    memcpy(buf + 8, &plen, 4);
    if (plen > 0 && payload) memcpy(buf + 12, payload, plen);
    return 12 + plen;
}

// === Test 1: roundtrip with empty payload (0 字节) ===
static void test_roundtrip_empty(void)
{
    uint8_t buf[64];
    proto_frame_hdr_t in = {.cmd = PROTO_HEARTBEAT, .seq = 42, .payload_len = 0};
    int n = proto_encode(&in, NULL, buf, sizeof(buf));
    EXPECT_EQ_INT(n, PROTO_HDR_SIZE, "encode_empty_payload");

    proto_frame_hdr_t out;
    const uint8_t* p = NULL;
    int r = proto_decode(buf, n, &out, &p);
    EXPECT_EQ_INT(r, PROTO_HDR_SIZE, "decode_empty_total");
    EXPECT_EQ_INT(out.magic, PROTO_MAGIC, "decode_empty_magic");
    EXPECT_EQ_INT(out.version, PROTO_VERSION, "decode_empty_version");
    EXPECT_EQ_INT(out.cmd, PROTO_HEARTBEAT, "decode_empty_cmd");
    EXPECT_EQ_INT(out.seq, 42, "decode_empty_seq");
    EXPECT_EQ_INT(out.payload_len, 0, "decode_empty_plen");
}

// === Test 2: roundtrip with 1 字节 payload ===
static void test_roundtrip_one(void)
{
    uint8_t buf[64];
    uint8_t pl[1] = {0xAB};
    proto_frame_hdr_t in = {.cmd = PROTO_DATA, .seq = 1, .payload_len = 1};
    int n = proto_encode(&in, pl, buf, sizeof(buf));
    EXPECT_EQ_INT(n, PROTO_HDR_SIZE + 1, "encode_one_byte");

    proto_frame_hdr_t out;
    const uint8_t* p = NULL;
    int r = proto_decode(buf, n, &out, &p);
    EXPECT_EQ_INT(r, PROTO_HDR_SIZE + 1, "decode_one_total");
    EXPECT_EQ_INT(out.payload_len, 1, "decode_one_plen");
    EXPECT(p == buf + PROTO_HDR_SIZE, "decode_one_zerocopy_ptr");
    EXPECT_EQ_INT(p[0], 0xAB, "decode_one_payload_value");
}

// === Test 3: roundtrip MAX_PAYLOAD - 1 ===
static void test_roundtrip_max_minus_one(void)
{
    const size_t pl_len = PROTO_MAX_PAYLOAD - 1;
    uint8_t* buf = (uint8_t*)malloc(PROTO_HDR_SIZE + pl_len);
    uint8_t* pl  = (uint8_t*)malloc(pl_len);
    for (size_t i = 0; i < pl_len; i++) pl[i] = (uint8_t)(i & 0xFF);

    proto_frame_hdr_t in = {.cmd = PROTO_FW_CHUNK, .seq = 7, .payload_len = (uint32_t)pl_len};
    int n = proto_encode(&in, pl, buf, PROTO_HDR_SIZE + pl_len);
    EXPECT_EQ_INT(n, (int)(PROTO_HDR_SIZE + pl_len), "encode_max_minus_one");

    proto_frame_hdr_t out;
    const uint8_t* p = NULL;
    int r = proto_decode(buf, n, &out, &p);
    EXPECT_EQ_INT(r, (int)(PROTO_HDR_SIZE + pl_len), "decode_max_minus_one_total");
    EXPECT(memcmp(p, pl, pl_len) == 0, "decode_max_minus_one_payload_eq");

    free(buf);
    free(pl);
}

// === Test 4: roundtrip exactly MAX_PAYLOAD ===
static void test_roundtrip_max(void)
{
    const size_t pl_len = PROTO_MAX_PAYLOAD;
    uint8_t* buf = (uint8_t*)malloc(PROTO_HDR_SIZE + pl_len);
    uint8_t* pl  = (uint8_t*)malloc(pl_len);
    memset(pl, 0x5A, pl_len);

    proto_frame_hdr_t in = {.cmd = PROTO_FW_CHUNK, .seq = 8, .payload_len = (uint32_t)pl_len};
    int n = proto_encode(&in, pl, buf, PROTO_HDR_SIZE + pl_len);
    EXPECT_EQ_INT(n, (int)(PROTO_HDR_SIZE + pl_len), "encode_exact_max");

    proto_frame_hdr_t out;
    const uint8_t* p = NULL;
    int r = proto_decode(buf, n, &out, &p);
    EXPECT_EQ_INT(r, (int)(PROTO_HDR_SIZE + pl_len), "decode_exact_max_total");
    EXPECT(memcmp(p, pl, pl_len) == 0, "decode_exact_max_payload_eq");

    free(buf);
    free(pl);
}

// === Test 5: bad magic 检测 ===
static void test_bad_magic(void)
{
    uint8_t buf[64];
    size_t n = craft_frame(buf, 0xDEAD, PROTO_VERSION, PROTO_HEARTBEAT, 0, 0, NULL);
    proto_frame_hdr_t out;
    const uint8_t* p = NULL;
    int r = proto_decode(buf, n, &out, &p);
    EXPECT_EQ_INT(r, PROTO_ERR_BAD_MAGIC, "bad_magic_rejected");
}

// === Test 6: bad version 检测 ===
static void test_bad_version(void)
{
    uint8_t buf[64];
    size_t n = craft_frame(buf, PROTO_MAGIC, 99, PROTO_HEARTBEAT, 0, 0, NULL);
    proto_frame_hdr_t out;
    const uint8_t* p = NULL;
    int r = proto_decode(buf, n, &out, &p);
    EXPECT_EQ_INT(r, PROTO_ERR_BAD_VERSION, "bad_version_rejected");
}

// === Test 7: payload_len 超 MAX 拒绝(decode 路径) ===
static void test_payload_too_large_decode(void)
{
    uint8_t buf[64];
    // 帧头宣称 payload_len = MAX+1,但实际 in_len 只到头部 — codec 应该
    // 在校验长度上限时就拒绝,而不是返回 INCOMPLETE
    size_t n = craft_frame(buf, PROTO_MAGIC, PROTO_VERSION, PROTO_DATA,
                           0, PROTO_MAX_PAYLOAD + 1, NULL);
    (void)n;  // 我们刻意只给 12 字节,模拟 payload 还没到
    proto_frame_hdr_t out;
    const uint8_t* p = NULL;
    int r = proto_decode(buf, PROTO_HDR_SIZE, &out, &p);
    EXPECT_EQ_INT(r, PROTO_ERR_PAYLOAD_TOO_LARGE, "decode_payload_too_large");
}

// === Test 8: payload_len 超 MAX 拒绝(encode 路径) ===
static void test_payload_too_large_encode(void)
{
    uint8_t buf[64];
    proto_frame_hdr_t in = {.cmd = PROTO_DATA, .seq = 0,
                             .payload_len = PROTO_MAX_PAYLOAD + 1};
    int r = proto_encode(&in, NULL, buf, sizeof(buf));
    EXPECT_EQ_INT(r, PROTO_ERR_PAYLOAD_TOO_LARGE, "encode_payload_too_large");
}

// === Test 9: incomplete frame(头部不全) ===
static void test_incomplete_header(void)
{
    uint8_t buf[8] = {0};  // 不到 12 字节
    memcpy(buf, &(uint16_t){PROTO_MAGIC}, 2);
    proto_frame_hdr_t out;
    const uint8_t* p = NULL;
    int r = proto_decode(buf, sizeof(buf), &out, &p);
    EXPECT_EQ_INT(r, PROTO_ERR_INCOMPLETE, "incomplete_header_returns_incomplete");
}

// === Test 10: incomplete frame(payload 半截) ===
static void test_incomplete_payload(void)
{
    uint8_t buf[64];
    uint8_t pl[10] = {1,2,3,4,5,6,7,8,9,10};
    size_t n = craft_frame(buf, PROTO_MAGIC, PROTO_VERSION, PROTO_DATA, 5, 10, pl);
    // 但 caller 只读到 PROTO_HDR_SIZE + 5 字节(payload 只到一半)
    proto_frame_hdr_t out;
    const uint8_t* p = NULL;
    int r = proto_decode(buf, PROTO_HDR_SIZE + 5, &out, &p);
    (void)n;
    EXPECT_EQ_INT(r, PROTO_ERR_INCOMPLETE, "incomplete_payload_returns_incomplete");
}

// === Test 11: encode 输出 buffer 不够 ===
static void test_encode_buf_too_small(void)
{
    uint8_t buf[10];  // 不到 12 字节头都装不下
    proto_frame_hdr_t in = {.cmd = PROTO_HEARTBEAT, .seq = 0, .payload_len = 0};
    int r = proto_encode(&in, NULL, buf, sizeof(buf));
    EXPECT_EQ_INT(r, PROTO_ERR_BUF_TOO_SMALL, "encode_buf_too_small_for_hdr");

    uint8_t buf2[20];
    uint8_t pl[16] = {0};
    proto_frame_hdr_t in2 = {.cmd = PROTO_DATA, .seq = 0, .payload_len = 16};
    int r2 = proto_encode(&in2, pl, buf2, sizeof(buf2));
    EXPECT_EQ_INT(r2, PROTO_ERR_BUF_TOO_SMALL, "encode_buf_too_small_for_payload");
}

// === Test 12: roundtrip 多 cmd 多 seq(矩阵覆盖) ===
static void test_roundtrip_matrix(void)
{
    const proto_cmd_t cmds[] = {
        PROTO_REGISTER, PROTO_REGISTER_ACK, PROTO_HEARTBEAT,
        PROTO_DATA, PROTO_CMD, PROTO_LOG,
        PROTO_FW_BEGIN, PROTO_FW_CHUNK, PROTO_FW_END, PROTO_FW_QUERY
    };
    const uint32_t seqs[] = {0, 1, 0xFFFFFFFFu, 0x80000000u};

    int ok = 1;
    for (size_t i = 0; i < sizeof(cmds)/sizeof(cmds[0]); i++) {
        for (size_t j = 0; j < sizeof(seqs)/sizeof(seqs[0]); j++) {
            uint8_t buf[64];
            uint8_t pl[8] = {(uint8_t)i, (uint8_t)j, 0xCA, 0xFE, 0xBA, 0xBE, 0xDE, 0xAD};
            proto_frame_hdr_t in = {.cmd = (uint8_t)cmds[i], .seq = seqs[j], .payload_len = 8};
            int n = proto_encode(&in, pl, buf, sizeof(buf));
            if (n != PROTO_HDR_SIZE + 8) { ok = 0; break; }

            proto_frame_hdr_t out;
            const uint8_t* p = NULL;
            int r = proto_decode(buf, n, &out, &p);
            if (r != n) { ok = 0; break; }
            if (out.cmd != cmds[i] || out.seq != seqs[j]) { ok = 0; break; }
            if (memcmp(p, pl, 8) != 0) { ok = 0; break; }
        }
    }
    EXPECT(ok, "roundtrip_matrix_cmd_x_seq");
}

int main(void)
{
    test_roundtrip_empty();
    test_roundtrip_one();
    test_roundtrip_max_minus_one();
    test_roundtrip_max();
    test_bad_magic();
    test_bad_version();
    test_payload_too_large_decode();
    test_payload_too_large_encode();
    test_incomplete_header();
    test_incomplete_payload();
    test_encode_buf_too_small();
    test_roundtrip_matrix();

    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}

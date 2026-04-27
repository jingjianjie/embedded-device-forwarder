// proto_codec.c — 协议帧编解码(RPD 阶段 1, 任务 C-1)
//
// 设计:
//   - 纯函数,无并发 state,不 malloc,不依赖 socket / fd
//   - LE 主机序直拷(见 protocol.h 字节序声明)
//   - decode 零拷贝:payload_out 指向 in_buf 内部
//
// 测试: tests/unit/test_proto_codec.c (test-as-doc)

#include <string.h>
#include "protocol.h"

int proto_encode(const proto_frame_hdr_t* hdr,
                 const void* payload,
                 uint8_t* out_buf, size_t out_cap)
{
    // payload_len 上限校验(防恶意/Bug)
    if (hdr->payload_len > PROTO_MAX_PAYLOAD)
        return PROTO_ERR_PAYLOAD_TOO_LARGE;

    const size_t need = (size_t)PROTO_HDR_SIZE + hdr->payload_len;
    if (out_cap < need)
        return PROTO_ERR_BUF_TOO_SMALL;

    // 用本地变量统一填好 magic/version,避免依赖 caller 的预填
    proto_frame_hdr_t h;
    h.magic       = PROTO_MAGIC;
    h.version     = PROTO_VERSION;
    h.cmd         = hdr->cmd;
    h.seq         = hdr->seq;
    h.payload_len = hdr->payload_len;

    // 整头 memcpy,LE 直拷,无字段级解引用 → 规避 packed misalign UB
    memcpy(out_buf, &h, PROTO_HDR_SIZE);

    if (hdr->payload_len > 0)
        memcpy(out_buf + PROTO_HDR_SIZE, payload, hdr->payload_len);

    return (int)need;
}

int proto_decode(const uint8_t* in_buf, size_t in_len,
                 proto_frame_hdr_t* hdr_out,
                 const uint8_t** payload_out)
{
    // 头都没读全 → INCOMPLETE,告诉 caller 继续 read
    if (in_len < PROTO_HDR_SIZE)
        return PROTO_ERR_INCOMPLETE;

    // 整头 memcpy 到栈上(对齐安全)
    proto_frame_hdr_t h;
    memcpy(&h, in_buf, PROTO_HDR_SIZE);

    if (h.magic != PROTO_MAGIC)
        return PROTO_ERR_BAD_MAGIC;

    if (h.version != PROTO_VERSION)
        return PROTO_ERR_BAD_VERSION;

    if (h.payload_len > PROTO_MAX_PAYLOAD)
        return PROTO_ERR_PAYLOAD_TOO_LARGE;

    const size_t total = (size_t)PROTO_HDR_SIZE + h.payload_len;
    if (in_len < total)
        return PROTO_ERR_INCOMPLETE;  // payload 还差几字节,caller 继续 read

    *hdr_out     = h;
    *payload_out = in_buf + PROTO_HDR_SIZE;
    return (int)total;
}

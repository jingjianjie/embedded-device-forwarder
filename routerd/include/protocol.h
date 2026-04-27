#ifndef EZ_ROUTER_PROTOCOL_H
#define EZ_ROUTER_PROTOCOL_H

// protocol.h — ez_router 子程序协议帧定义(RPD 阶段 1.1)
//
// 字节序: little-endian 主机序直拷。
//   target = RK3588 (LE),上位机 = x86-64 (LE),C# BinaryReader 默认 LE。
//   全链路无 swap 需求,encode/decode 用 memcpy 整头拷贝,无 htole* 调用。
//   未来若引入 BE 客户端,需要在 codec 内增加字节序适配,届时 bump version。
//
// 与旧 ipc_header_t 不兼容(R-5 决议,无前向兼容):
//   旧: magic=u32(0xE7F12345), cmd=u16, reserved=u16, length=u32
//   新: magic=u16(0xEB7C), version=u8, cmd=u8, seq=u32, payload_len=u32
//   见 doc/router_protocol.md 切换说明。

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

// 帧识别魔数。两字节即够,选 0xEB7C 与 ASCII 区分明显,扫日志易识别。
#define PROTO_MAGIC   0xEB7Cu

// 协议大版本。语义破坏性改动 → bump。同版本内不引入兼容字段(R-5)。
#define PROTO_VERSION 1u

// 单帧 payload 上限。与端口 MAX_DATA(1024)解耦:
//   - MAX_DATA 是端口缓冲单元(event_msg_t.data),受路由数据流模型约束
//   - PROTO_MAX_PAYLOAD 是协议层传输 limit,要支撑 FW_CHUNK 固件分片(典型 4-32 KiB)
// 取 64 KiB:够固件分片,远低于 uint32 上限(4 GiB),避免恶意客户端宣称大 payload
// 让接收方分配巨 buffer 的 DoS 风险。
#define PROTO_MAX_PAYLOAD (64u * 1024u)

// 帧头字节数。固定 12 字节,packed 后无 padding。
#define PROTO_HDR_SIZE 12u

// 命令码(RPD §5.1.1)。新增 cmd 值时同步 doc/router_protocol.md 命令码表。
typedef enum {
    PROTO_REGISTER       = 0x01,  // 子程序 → router: 上报元信息
    PROTO_REGISTER_ACK   = 0x02,  // router → 子程序: 注册结果
    PROTO_HEARTBEAT      = 0x03,  // 双向: 心跳保活
    PROTO_DATA           = 0x10,  // 双向: 端口数据
    PROTO_CMD            = 0x11,  // 双向: 命令下发与响应
    PROTO_LOG            = 0x20,  // 子程序 → router: 日志记录
    PROTO_FW_BEGIN       = 0x30,  // 上位机 → 子程序: 固件升级开始
    PROTO_FW_CHUNK       = 0x31,  // 上位机 → 子程序: 固件数据分片
    PROTO_FW_END         = 0x32,  // 上位机 → 子程序: 固件传输结束
    PROTO_FW_QUERY       = 0x33,  // 双向: 查询固件版本/状态
} proto_cmd_t;

// 帧头(纯字段视图,12 字节,packed)。
//
// 注意: 我们用 memcpy 在 codec 中整头拷贝 buf <-> 此结构,
//       不通过结构指针按字段访问 — 规避 packed struct 在某些 arch 上的
//       misalign UB(详见 doc/router_protocol.md "字节序与对齐"小节)。
typedef struct __attribute__((packed)) {
    uint16_t magic;        // = PROTO_MAGIC
    uint8_t  version;      // = PROTO_VERSION
    uint8_t  cmd;          // proto_cmd_t
    uint32_t seq;          // R-3: 发送方递增,接收方仅用于 ACK 关联,不查重
    uint32_t payload_len;  // payload 字节数,<= PROTO_MAX_PAYLOAD
} proto_frame_hdr_t;

// 编译期钉死帧头大小,防止有人不小心改了字段引入 padding。
_Static_assert(sizeof(proto_frame_hdr_t) == PROTO_HDR_SIZE,
               "proto_frame_hdr_t must be exactly 12 bytes");

// 错误码(负值,0/正值表示成功的字节数)。
// 详细列表见 doc/router_protocol.md 错误码表。
#define PROTO_ERR_BUF_TOO_SMALL      -1  // encode: 输出 buffer 不够
#define PROTO_ERR_BAD_MAGIC          -2  // decode: magic 不匹配
#define PROTO_ERR_BAD_VERSION        -3  // decode: version 不匹配
#define PROTO_ERR_PAYLOAD_TOO_LARGE  -4  // encode/decode: payload_len 越界
#define PROTO_ERR_INCOMPLETE         -5  // decode: 输入不足整帧(caller 再读)

// 编码: 把帧头 + payload 序列化到 out_buf。
//   hdr->magic / hdr->version 字段会被本函数覆盖填充(caller 不必预填)。
//   hdr->cmd / hdr->seq / hdr->payload_len 由 caller 填好。
//   payload 可为 NULL 当且仅当 hdr->payload_len == 0。
// 返回:
//   >0  实际写入字节数(= PROTO_HDR_SIZE + payload_len)
//   <0  错误码(BUF_TOO_SMALL / PAYLOAD_TOO_LARGE)
int proto_encode(const proto_frame_hdr_t* hdr,
                 const void* payload,
                 uint8_t* out_buf, size_t out_cap);

// 解码: 从 in_buf 解析出帧头并定位 payload(零拷贝)。
//   payload_out 指向 in_buf 内部,不复制;生命周期与 in_buf 相同。
//   payload_len == 0 时 *payload_out 仍指向头后位置,但不应被读。
// 返回:
//   >0  整帧字节数(= PROTO_HDR_SIZE + payload_len),caller 据此推进读指针
//   <0  错误码(INCOMPLETE / BAD_MAGIC / BAD_VERSION / PAYLOAD_TOO_LARGE)
int proto_decode(const uint8_t* in_buf, size_t in_len,
                 proto_frame_hdr_t* hdr_out,
                 const uint8_t** payload_out);

#endif // EZ_ROUTER_PROTOCOL_H

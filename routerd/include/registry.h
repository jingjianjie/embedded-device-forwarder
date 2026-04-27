#ifndef EZ_ROUTER_REGISTRY_H
#define EZ_ROUTER_REGISTRY_H

// registry.h — 子程序元信息表(RPD 阶段 1 任务 C-2)
//
// 由 IPC server 在收到 PROTO_REGISTER 帧后调用 registry_register 落表,
// PROTO_HEARTBEAT 触发 registry_update_heartbeat。后续阶段 2 supervisor
// 通过 registry_iterate 检查心跳超时。
//
// 并发(契约 14: reactor 不可阻塞):
//   - 所有公开 API 内部加同一个 mutex,临界区 = 纯内存读写,不做 IO/malloc
//   - cJSON_Parse 在 register 内部锁外完成,提到结构后再进锁占槽
//   - find_by_* 返回的指针仅在持锁期间稳定;dispatcher 帧路径单线程消费,
//     在调用与使用之间不释放锁是 caller 的责任。当前 IPC 线程取得指针后
//     立即读取字段(无后续阻塞),所以借出指针是安全的。

#include <stdint.h>
#include <stddef.h>

// 上限。32 与 MAX_PORTS 同尺度,够单板用,过大会浪费栈/锁开销。
#define REGISTRY_MAX_SUBPROCS            32
#define REGISTRY_MAX_PORTS_PER_SUBPROC   8

// 字段长度上限。device_id 偏长是因为厂内 SN 通常 ~32 字节,留 64 备扩。
#define REGISTRY_DEVICE_ID_LEN     64
#define REGISTRY_MODEL_LEN         32
#define REGISTRY_FW_VERSION_LEN    16
#define REGISTRY_BUILD_DATE_LEN    16
#define REGISTRY_PORT_NAME_LEN     32

// 单条子程序元信息。client_fd = -1 表示空槽。
typedef struct {
    int      client_fd;
    int      in_use;
    char     device_id  [REGISTRY_DEVICE_ID_LEN];
    char     model      [REGISTRY_MODEL_LEN];
    char     fw_version [REGISTRY_FW_VERSION_LEN];
    char     build_date [REGISTRY_BUILD_DATE_LEN];
    uint32_t last_seq;            // 最近一帧 seq(诊断用,非幂等键,见 R-3)
    uint64_t last_heartbeat_ms;   // CLOCK_MONOTONIC 毫秒
    int      port_count;          // 子程序声明的端口数
    char     port_names[REGISTRY_MAX_PORTS_PER_SUBPROC][REGISTRY_PORT_NAME_LEN];
} subproc_entry_t;

// 迭代回调签名。e 仅在回调期间有效;不要持久化指针。
typedef void (*registry_iter_cb)(const subproc_entry_t* e, void* user);

// 初始化锁与槽位。可重入(幂等),无前置依赖。
void registry_init(void);

// 释放锁(进程退出前调用,不严格要求)。
void registry_close(void);

// 注册子程序。json 是 PROTO_REGISTER 帧 payload(JSON,长度 json_len 字节,
// 不必以 \0 结尾;函数内部会复制到本地栈缓冲补 \0)。
//   返回 0  = 成功
//   返回 -1 = JSON 解析失败 / 字段缺失 / device_id 冲突 / 槽满
int registry_register(int fd, const char* json, int json_len);

// 注销:fd 关闭时调用,释放对应槽位。fd 未注册返回 -1(可忽略)。
int registry_unregister(int fd);

// 更新心跳时间戳。fd 未注册返回 -1。
int registry_update_heartbeat(int fd);

// 查找。返回的指针仅在持锁期间有效(见文件头并发契约)。
const subproc_entry_t* registry_find_by_fd(int fd);
const subproc_entry_t* registry_find_by_device_id(const char* device_id);

// 遍历:对每个 in_use 槽位调用 cb。整个遍历持锁,cb 内不可调用 registry_*
// (会死锁)。cb 应只做 read-only / 复制操作。
void registry_iterate(registry_iter_cb cb, void* user);

// CLOCK_MONOTONIC 当前毫秒。提到接口里方便单测断言。
uint64_t registry_now_ms(void);

#endif // EZ_ROUTER_REGISTRY_H

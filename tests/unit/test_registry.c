// test_registry.c — RPD 阶段 1 任务 C-2 的 test-as-doc
//
// 固化契约(routerd/include/registry.h + design-intent §2):
//   - 32 槽上限,满后 register 返回 -1
//   - device_id 冲突拒绝
//   - 同 fd 重复注册拒绝(避免槽位泄漏)
//   - find_by_fd / find_by_device_id 命中性正确
//   - heartbeat 更新时间戳
//   - unregister 释放槽位 + 后续 find 返回 NULL
//
// §6.5 TEST AS DOC 形态。Unity 单测脚手架(open-questions U1)就绪后迁移。
//
// 编译运行(从仓库根目录):
//   gcc -Wall -Wextra -I routerd/include -I routerd/3rdparty/cjson routerd/src/registry.c routerd/src/log.c routerd/3rdparty/cjson/cJSON.c tests/unit/test_registry.c -lpthread -o /tmp/test_registry
//   /tmp/test_registry
//
// 退出码:0 = 全部 PASS,非 0 = FAIL

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "registry.h"

// --- 极简测试框架(零依赖,与 test_proto_codec.c 一致) ---
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

// 构造合法 register JSON。caller 提供 buf 与 device_id。
static int make_json(char* buf, size_t cap, const char* device_id,
                     const char* model, int n_ports)
{
    int n = snprintf(buf, cap,
        "{\"device_id\":\"%s\","
        "\"model\":\"%s\","
        "\"fw_version\":\"1.2.3\","
        "\"build_date\":\"2026-04-15\"",
        device_id, model);
    if (n_ports > 0) {
        n += snprintf(buf + n, cap - n, ",\"ports\":[");
        for (int i = 0; i < n_ports; i++) {
            n += snprintf(buf + n, cap - n,
                          "%s{\"name\":\"P%d\",\"type\":\"tty\"}",
                          i ? "," : "", i);
        }
        n += snprintf(buf + n, cap - n, "]");
    }
    n += snprintf(buf + n, cap - n, "}");
    return n;
}

// 把 registry 清空到初始状态(test-only,通过反复 unregister 所有可能 fd)
// 这里取巧: 直接遍历 fd 范围 unregister,失败即说明该 fd 未注册,容错
static void registry_reset(void)
{
    for (int fd = 0; fd < 1024; fd++) registry_unregister(fd);
}

// === Test 1: register happy path → find_by_fd 命中 ===
static void test_register_happy(void)
{
    registry_reset();
    char json[512];
    int len = make_json(json, sizeof(json), "SN-001", "spec-x1", 2);

    int rc = registry_register(100, json, len);
    EXPECT_EQ_INT(rc, 0, "register_happy_returns_0");

    const subproc_entry_t* e = registry_find_by_fd(100);
    EXPECT(e != NULL, "find_by_fd_hit");
    if (e) {
        EXPECT(strcmp(e->device_id, "SN-001") == 0, "field_device_id");
        EXPECT(strcmp(e->model, "spec-x1") == 0, "field_model");
        EXPECT(strcmp(e->fw_version, "1.2.3") == 0, "field_fw_version");
        EXPECT(strcmp(e->build_date, "2026-04-15") == 0, "field_build_date");
        EXPECT_EQ_INT(e->port_count, 2, "field_port_count");
        EXPECT(strcmp(e->port_names[0], "P0") == 0, "field_port_name_0");
    }
}

// === Test 2: unregister 后 find 返回 NULL ===
static void test_unregister(void)
{
    registry_reset();
    char json[512];
    int len = make_json(json, sizeof(json), "SN-002", "m", 0);
    EXPECT_EQ_INT(registry_register(101, json, len), 0, "pre_register");

    int rc = registry_unregister(101);
    EXPECT_EQ_INT(rc, 0, "unregister_returns_0");

    const subproc_entry_t* e = registry_find_by_fd(101);
    EXPECT(e == NULL, "find_after_unregister_returns_NULL");

    // 再 unregister 同 fd 应失败(已不存在)
    EXPECT_EQ_INT(registry_unregister(101), -1, "double_unregister_fails");
}

// === Test 3: find_by_device_id 命中 / 未命中 ===
static void test_find_by_device_id(void)
{
    registry_reset();
    char json[512];
    int len = make_json(json, sizeof(json), "SN-DEV", "m", 0);
    EXPECT_EQ_INT(registry_register(200, json, len), 0, "pre_register");

    const subproc_entry_t* e = registry_find_by_device_id("SN-DEV");
    EXPECT(e != NULL, "find_by_device_id_hit");
    if (e) EXPECT_EQ_INT(e->client_fd, 200, "by_device_id_fd_match");

    EXPECT(registry_find_by_device_id("not-exist") == NULL,
           "find_by_device_id_miss");
}

// === Test 4: heartbeat 更新时间戳 ===
static void test_heartbeat_update(void)
{
    registry_reset();
    char json[512];
    int len = make_json(json, sizeof(json), "SN-HB", "m", 0);
    EXPECT_EQ_INT(registry_register(300, json, len), 0, "pre_register");

    const subproc_entry_t* e1 = registry_find_by_fd(300);
    uint64_t t0 = e1 ? e1->last_heartbeat_ms : 0;

    usleep(5000);   // 5ms
    EXPECT_EQ_INT(registry_update_heartbeat(300), 0, "heartbeat_returns_0");

    const subproc_entry_t* e2 = registry_find_by_fd(300);
    uint64_t t1 = e2 ? e2->last_heartbeat_ms : 0;
    EXPECT(t1 > t0, "heartbeat_advances_timestamp");

    // 未注册 fd
    EXPECT_EQ_INT(registry_update_heartbeat(999), -1,
                  "heartbeat_unregistered_fd_fails");
}

// === Test 5: 满 32 槽再 register 拒绝 ===
static void test_overflow(void)
{
    registry_reset();
    char json[512];
    char dev[32];
    for (int i = 0; i < REGISTRY_MAX_SUBPROCS; i++) {
        snprintf(dev, sizeof(dev), "SN-FILL-%d", i);
        int len = make_json(json, sizeof(json), dev, "m", 0);
        int rc = registry_register(1000 + i, json, len);
        if (rc != 0) {
            fprintf(stderr, "FAIL fill iteration %d failed early\n", i);
            g_failed++;
            return;
        }
    }
    g_passed++;
    printf("PASS fill_32_slots\n");

    // 第 33 个应失败
    int len = make_json(json, sizeof(json), "SN-OVERFLOW", "m", 0);
    int rc = registry_register(2000, json, len);
    EXPECT_EQ_INT(rc, -1, "register_overflow_rejected");
}

// === Test 6: device_id 冲突拒绝 + 同 fd 二次注册拒绝 ===
static void test_conflict(void)
{
    registry_reset();
    char json[512];
    int len = make_json(json, sizeof(json), "SN-CONFLICT", "m", 0);
    EXPECT_EQ_INT(registry_register(400, json, len), 0, "pre_register");

    // 不同 fd,但同 device_id → 拒绝
    EXPECT_EQ_INT(registry_register(401, json, len), -1,
                  "duplicate_device_id_rejected");

    // 同 fd 二次注册(无论 device_id 同不同)→ 拒绝
    int len2 = make_json(json, sizeof(json), "SN-OTHER", "m", 0);
    EXPECT_EQ_INT(registry_register(400, json, len2), -1,
                  "double_register_same_fd_rejected");
}

// === Test 7: malformed JSON 拒绝 ===
static void test_malformed(void)
{
    registry_reset();
    const char* bad1 = "{not json";
    EXPECT_EQ_INT(registry_register(500, bad1, (int)strlen(bad1)), -1,
                  "malformed_json_rejected");

    const char* bad2 = "{\"device_id\":\"x\"}";   // 缺 model/fw/build
    EXPECT_EQ_INT(registry_register(501, bad2, (int)strlen(bad2)), -1,
                  "missing_required_field_rejected");
}

int main(void)
{
    registry_init();

    test_register_happy();
    test_unregister();
    test_find_by_device_id();
    test_heartbeat_update();
    test_overflow();
    test_conflict();
    test_malformed();

    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}

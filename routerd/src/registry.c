// registry.c — 子程序元信息表实现(RPD 阶段 1 任务 C-2)
//
// 关键不变量:
//   - 公开 API 内部锁短:所有路径只在锁内做内存读写,JSON 解析在锁外
//   - find_by_* 借出内部指针,caller 负责在锁外不持久化(见 registry.h 注释)
//   - 超容量 / 冲突 / 解析失败一律返回 -1,由 caller(dispatcher)记日志 + ACK
//
// 测试: tests/unit/test_registry.c

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "registry.h"
#include "cJSON.h"
#include "log.h"

static pthread_mutex_t   g_lock = PTHREAD_MUTEX_INITIALIZER;
static subproc_entry_t   g_table[REGISTRY_MAX_SUBPROCS];
static int               g_inited = 0;

// 截断 + NUL 安全的字段拷贝。返回写入的字节数(不含 NUL)。
static void copy_field(char* dst, size_t cap, const char* src)
{
    if (cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

// 从 cJSON 对象取 string 字段;不存在或非 string 返回 NULL。
static const char* json_str(const cJSON* obj, const char* key)
{
    cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return (v && cJSON_IsString(v) && v->valuestring) ? v->valuestring : NULL;
}

uint64_t registry_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000);
}

void registry_init(void)
{
    pthread_mutex_lock(&g_lock);
    if (!g_inited) {
        memset(g_table, 0, sizeof(g_table));
        for (int i = 0; i < REGISTRY_MAX_SUBPROCS; i++)
            g_table[i].client_fd = -1;
        g_inited = 1;
    }
    pthread_mutex_unlock(&g_lock);
}

void registry_close(void)
{
    // 锁本身静态初始化,不必 destroy。这里留空 hook 给未来需要时填。
}

// 解析 JSON → 栈结构 e(锁外)。失败返回 -1。
// 必填:device_id, model, fw_version, build_date。ports 可缺(为 0)。
static int parse_register_json(const char* json, int json_len, subproc_entry_t* e)
{
    // 拷到栈缓冲补 \0,避免 cJSON_Parse 越界读
    if (json_len <= 0 || json_len > 4096) {  // register payload 不该超 4KB
        LOG_WARN("[registry] register payload size invalid: %d\n", json_len);
        return -1;
    }
    char buf[4097];
    memcpy(buf, json, json_len);
    buf[json_len] = '\0';

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        LOG_WARN("[registry] cJSON_Parse failed\n");
        return -1;
    }

    const char* device_id  = json_str(root, "device_id");
    const char* model      = json_str(root, "model");
    const char* fw_version = json_str(root, "fw_version");
    const char* build_date = json_str(root, "build_date");

    if (!device_id || !model || !fw_version || !build_date) {
        LOG_WARN("[registry] missing required field(s)\n");
        cJSON_Delete(root);
        return -1;
    }

    copy_field(e->device_id,  sizeof(e->device_id),  device_id);
    copy_field(e->model,      sizeof(e->model),      model);
    copy_field(e->fw_version, sizeof(e->fw_version), fw_version);
    copy_field(e->build_date, sizeof(e->build_date), build_date);

    e->port_count = 0;
    cJSON* ports = cJSON_GetObjectItemCaseSensitive(root, "ports");
    if (ports && cJSON_IsArray(ports)) {
        cJSON* it = NULL;
        cJSON_ArrayForEach(it, ports) {
            if (e->port_count >= REGISTRY_MAX_PORTS_PER_SUBPROC) break;
            const char* name = json_str(it, "name");
            if (!name) continue;
            copy_field(e->port_names[e->port_count],
                       REGISTRY_PORT_NAME_LEN, name);
            e->port_count++;
        }
    }

    cJSON_Delete(root);
    return 0;
}

int registry_register(int fd, const char* json, int json_len)
{
    if (!g_inited) registry_init();

    // 锁外解析
    subproc_entry_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    if (parse_register_json(json, json_len, &parsed) < 0)
        return -1;

    pthread_mutex_lock(&g_lock);

    // 同 device_id 冲突检查 + fd 已注册检查
    int free_slot = -1;
    for (int i = 0; i < REGISTRY_MAX_SUBPROCS; i++) {
        if (g_table[i].in_use) {
            if (g_table[i].client_fd == fd) {
                pthread_mutex_unlock(&g_lock);
                LOG_WARN("[registry] fd=%d already registered\n", fd);
                return -1;
            }
            if (strcmp(g_table[i].device_id, parsed.device_id) == 0) {
                pthread_mutex_unlock(&g_lock);
                LOG_WARN("[registry] device_id=%s already registered\n",
                         parsed.device_id);
                return -1;
            }
        } else if (free_slot < 0) {
            free_slot = i;
        }
    }

    if (free_slot < 0) {
        pthread_mutex_unlock(&g_lock);
        LOG_WARN("[registry] table full (%d slots)\n", REGISTRY_MAX_SUBPROCS);
        return -1;
    }

    parsed.client_fd         = fd;
    parsed.in_use            = 1;
    parsed.last_seq          = 0;
    parsed.last_heartbeat_ms = registry_now_ms();
    g_table[free_slot]       = parsed;

    pthread_mutex_unlock(&g_lock);

    LOG_INFO("[registry] registered fd=%d device_id=%s model=%s fw=%s slot=%d\n",
             fd, parsed.device_id, parsed.model, parsed.fw_version, free_slot);
    return 0;
}

int registry_unregister(int fd)
{
    if (!g_inited) return -1;

    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < REGISTRY_MAX_SUBPROCS; i++) {
        if (g_table[i].in_use && g_table[i].client_fd == fd) {
            memset(&g_table[i], 0, sizeof(g_table[i]));
            g_table[i].client_fd = -1;
            pthread_mutex_unlock(&g_lock);
            LOG_INFO("[registry] unregistered fd=%d slot=%d\n", fd, i);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return -1;
}

int registry_update_heartbeat(int fd)
{
    if (!g_inited) return -1;

    uint64_t now = registry_now_ms();
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < REGISTRY_MAX_SUBPROCS; i++) {
        if (g_table[i].in_use && g_table[i].client_fd == fd) {
            g_table[i].last_heartbeat_ms = now;
            pthread_mutex_unlock(&g_lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return -1;
}

const subproc_entry_t* registry_find_by_fd(int fd)
{
    if (!g_inited) return NULL;

    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < REGISTRY_MAX_SUBPROCS; i++) {
        if (g_table[i].in_use && g_table[i].client_fd == fd) {
            pthread_mutex_unlock(&g_lock);
            return &g_table[i];   // 见头文件并发契约
        }
    }
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

const subproc_entry_t* registry_find_by_device_id(const char* device_id)
{
    if (!g_inited || !device_id) return NULL;

    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < REGISTRY_MAX_SUBPROCS; i++) {
        if (g_table[i].in_use &&
            strcmp(g_table[i].device_id, device_id) == 0) {
            pthread_mutex_unlock(&g_lock);
            return &g_table[i];
        }
    }
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

void registry_iterate(registry_iter_cb cb, void* user)
{
    if (!g_inited || !cb) return;

    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < REGISTRY_MAX_SUBPROCS; i++) {
        if (g_table[i].in_use) cb(&g_table[i], user);
    }
    pthread_mutex_unlock(&g_lock);
}

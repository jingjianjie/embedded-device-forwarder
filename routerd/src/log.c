#include "log.h"

int g_log_enable = 0;                     // 默认关闭
log_level_t g_log_level = LOG_LEVEL_INFO; // 默认等级

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// ============================
// 内部函数
// ============================
static const char* level_to_str(log_level_t level)
{
    switch (level) {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        default:              return "NONE";
    }
}

// ============================
// 接口实现
// ============================
void log_init(int enable, log_level_t level)
{
    g_log_enable = enable;
    g_log_level  = level;

    if (enable)
        fprintf(stdout, "[log] enabled, level=%s\n", level_to_str(level));
    else
        fprintf(stdout, "[log] disabled\n");
}

void log_set_level(log_level_t level)
{
    g_log_level = level;
}

void log_enable(int enable)
{
    g_log_enable = enable;
}

void log_toggle(void)
{
    g_log_enable = !g_log_enable;
    fprintf(stdout, "[log] switched %s\n", g_log_enable ? "ON" : "OFF");
}

// ============================
// 核心日志打印函数
// ============================
void log_write(log_level_t level, const char* fmt, ...)
{
    if (!g_log_enable || level > g_log_level)
        return;

    pthread_mutex_lock(&log_mutex);

    // 时间戳
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", t);

    fprintf(stdout, "[%s] [%s] ", timebuf, level_to_str(level));

    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);

    fprintf(stdout, "\n");
    fflush(stdout);

    pthread_mutex_unlock(&log_mutex);
}

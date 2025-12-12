#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

// ============================
// 日志等级定义
// ============================
typedef enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
} log_level_t;

// ============================
// 全局变量定义
// ============================
extern int g_log_enable;           // 是否启用日志
extern log_level_t g_log_level;    // 当前日志等级

// ============================
// API 声明
// ============================
void log_init(int enable, log_level_t level);    // 初始化
void log_set_level(log_level_t level);           // 设置日志等级
void log_enable(int enable);                     // 启用/关闭日志
void log_toggle(void);                           // 动态切换日志开关
void log_write(log_level_t level, const char* fmt, ...); // 写日志

// ============================
// 宏封装（方便调用）
// ============================
#define LOG_ERROR(fmt, ...) log_write(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_write(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_write(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) log_write(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

#endif

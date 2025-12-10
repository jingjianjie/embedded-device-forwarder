#ifndef EVENT_H
#define EVENT_H

#include<stdint.h>

#define MAX_DATA 1024

typedef enum {
    EVT_USB_IN = 1,
    EVT_UART_IN,
    EVT_CMD_FROM_SDK,
    EVT_GENERIC_IN      // 可选：通用“来自任意端口”
} event_type_t;

typedef struct {
    // int src_port;        // ★ 新增：表示数据来自哪个物理端口（USB/UART/NET/SPI）
    char dst[32];           // known where to send
    //event_type_t type;   // 保留：事件类型
    int len;
    uint8_t data[MAX_DATA];
} event_msg_t;

#endif

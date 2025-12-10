#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "plugin_loader.h"   

//-------------------------------------------------------
// Handler 1: USB → UART 方向的数据处理
//-------------------------------------------------------
int usb_to_uart(uint8_t* data, int len)
{
    printf("[filter] usb_to_uart called, len=%d\n", len);

    // 示例：把数据转换成大写（随便做点事）
    for (int i = 0; i < len; i++) {
        if (data[i] >= 'a' && data[i] <= 'z')
            data[i] -= 32;
    }

    return len; // 返回新数据长度（可以不变）
}

//-------------------------------------------------------
// Handler 2: UART → USB 方向的数据处理
//-------------------------------------------------------
int uart_to_usb(uint8_t* data, int len)
{
    printf("[filter] uart_to_usb called, len=%d\n", len);

    // 示例：前面加一个 0xAA 标记
    if (len + 1 > 2048) return len;
    memmove(data + 1, data, len);
    data[0] = 0xAA;

    return len + 1;
}

//-------------------------------------------------------
// 插件初始化函数（自动执行）
//-------------------------------------------------------
__attribute__((constructor))
void plugin_init()
{
    printf("[filter] constructor: registering handlers...\n");
    //plugin_register_handler("plugin_name", "func_name", func_by_name);

    plugin_register_handler("filter", "usb_to_uart", usb_to_uart);
    plugin_register_handler("filter", "uart_to_usb", uart_to_usb);
}

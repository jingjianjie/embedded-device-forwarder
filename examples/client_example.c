#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "ez_router_sdk.h"

// ---------------------------
// Hook 示例函数
// ---------------------------

// 入方向 hook（USB → UART 时触发）
int hook_usb_to_uart(uint8_t* data, int* len)
{
    printf("[HOOK IN] usb→uart, recv %d bytes\n", *len);
    return 1; // 返回 1 表示允许继续路由
}

// 出方向 hook（UART → USB 时触发）
int hook_uart_to_usb(uint8_t* data, int* len)
{
    printf("[HOOK OUT] uart→usb, recv %d bytes\n", *len);

    // 示例：自动加一条字符串
    const char* extra = "[routed]";
    int extra_len = strlen(extra);

    if (*len + extra_len < 256) {
        memcpy(data + *len, extra, extra_len);
        *len += extra_len;
    }

    return 1; // 继续转发
}

// ---------------------------
// 示例：注册路由表
// ---------------------------
void example_register()
{
    ez_router_link_t links[3];

    // USB1 → UART1 双向
    links[0].in_port  = PORT_USB1;
    links[0].out_port = PORT_UART1;
    links[0].bidir    = 1;
    links[0].hook_in  = hook_usb_to_uart;
    links[0].hook_out = hook_uart_to_usb;

    // USB1 → NET0 （单向）
    links[1].in_port  = PORT_USB1;
    links[1].out_port = PORT_NET0;
    links[1].bidir    = 0;   // 单向
    links[1].hook_in  = NULL;
    links[1].hook_out = NULL;

    // USB1 → SPI0（单向）
    links[2].in_port  = PORT_USB1;
    links[2].out_port = PORT_SPI0;
    links[2].bidir    = 0;
    links[2].hook_in  = NULL;
    links[2].hook_out = NULL;

    printf("→ Registering routes ...\n");
    ez_router_register_routes(links, 3);
}

// ---------------------------
// 主函数
// ---------------------------
int main()
{
    printf("=== ez_router SDK Example ===\n");

    // 初始化 SDK（目前无操作）
    ez_router_sdk_init();

    // 先清空路由表（安全做法）
    printf("→ Clearing routes...\n");
    ez_router_clear_routes();

    // 注册自定义路由表
    example_register();

    printf("\nRoutes registered successfully.\n");
    printf("请查看 routerd 的日志输出以验证路由是否生效。\n");

    return 0;
}

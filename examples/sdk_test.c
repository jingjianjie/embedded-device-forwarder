#include "ez_router_sdk.h"
#include <stdio.h>

#define SOCKPATH "/tmp/ez_router.sock"

int main()
{
    printf("=== test: clear routes ===\n");
    ezr_init(SOCKPATH);
    // ezr_clear_routes();

    printf("=== test: set routes ===\n");

    ez_router_link_t links[1] = {
        { "/tty", "/tty", 1, "filter.so", "usb_to_uart","filter.so","uart_to_usb" }
    };

    // ezr_set_routes(links, 1);

    printf("=== load plugin ===\n");
    ezr_load_plugin("/home/aniston/Desktop/ez_router/plugins/filter.so");

    printf("=== save config ===\n");
    ezr_save_config();
    ezr_close();

    return 0;
}

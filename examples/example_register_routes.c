#include <stdio.h>
#include "ez_router_sdk.h"

int main()
{
    ez_router_connect();

    sdk_route_t routes[2];

    routes[0].in_port = 1;  // USB1
    routes[0].out_port = 2; // UART1
    routes[0].bidir = 1;

    strcpy(routes[0].hook_in_plugin,  "filter_A");
    strcpy(routes[0].hook_in_func,    "usb_to_uart");

    strcpy(routes[0].hook_out_plugin, "filter_B");
    strcpy(routes[0].hook_out_func,   "uart_to_usb");

    routes[1].in_port = 1;  // USB1
    routes[1].out_port = 3; // NET0
    routes[1].bidir = 0;

    strcpy(routes[1].hook_in_plugin,  "filter_B");
    strcpy(routes[1].hook_in_func,    "clean_noise");

    ez_router_set_routes(routes, 2);
    ez_router_save();

    printf("Routes updated.\n");
    return 0;
}

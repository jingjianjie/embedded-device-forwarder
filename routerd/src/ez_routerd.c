#include <stdio.h>
#include <pthread.h>
#include "reactor.h"
#include "ipc_server.h"
#include "router_link.h"
#include "plugin_loader.h"
#include "config_store.h"
#include "event_queue.h"
#include "router_core.h"
#include "port_manager.h"

void* dispatcher_thread(void* arg)
{
    event_msg_t msg;
    while (1) {
        queue_pop(&msg);
        router_core_handle(&msg);
    }
}

int main()
{
    printf("ez_routerd starting...\n");

    // 初始化插件系统
    // plugin_registry_init();

    // 初始化 reactor
    reactor_init();

    // IPC server
    ipc_server_init("/tmp/ez_router.sock");
    // reactor_add_fd(ipc_server_fd());

    // 载入 config.json
    if (!load_config("/home/aniston/Desktop/ez_router/out/config.json")) {
        printf("[daemon] restoring routes...\n");
        // router_restore_from_config();
        printf("[daemon] load plugins\n");

        //load plugin
        for(int i=0;i<g_config.plugin_count;i++){
            plugin_def_t* p=&g_config.plugins[i];
            plugin_load(p->path);
        }

        config_print();
        printf("[daemon] open ports\n");
        ports_open_all();

        //set routes reactor
        for(int i=0;i<g_config.port_count;i++){
            // route_def_t* rt=&g_config.routes[i];
            // printf("src=%s,  handler=%s\n",rt->src,rt->handler);
            // port_def_t* port=port_find_by_name(rt->src);
            // reactor_add_fd(fd);// add fd
            printf("reactor add port name=%s \n",g_config.ports[i].base.name);
            reactor_add_port(&g_config.ports[i]);
        }

    }


    pthread_t th_reactor, th_disp, th_ipc;
    pthread_create(&th_reactor, NULL, reactor_thread, NULL);
    pthread_create(&th_disp, NULL, dispatcher_thread, NULL);
    pthread_create(&th_ipc,NULL,ipc_thread,NULL);

    //control plane: ipc command handler
    //pthread_create(&th_ipc,NULL,ipc_thread,NULL);
    printf("ez_routerd ready.\n");

    pthread_join(th_reactor, NULL);
    pthread_join(th_ipc,NULL);
    return 0;
}

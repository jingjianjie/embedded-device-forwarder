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
#include "log.h"
#include "run_state.h"


void* dispatcher_thread(void* arg)
{
    event_msg_t msg;
    while (run_state_is_running()) {
        queue_pop(&msg);
        router_core_handle(&msg);
    }
}


int main(int argc,char* argv[])
{
   int enable_log = 0;
   log_level_t level = LOG_LEVEL_INFO;
   run_state_init();

    // 命令行参数解析
   for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-log") == 0) {
            enable_log = 1;
            level = LOG_LEVEL_INFO;
        }
        else if (strcmp(argv[i], "-debug") == 0) {
            enable_log = 1;
            level = LOG_LEVEL_DEBUG;
        }
        else if (strcmp(argv[i], "-quiet") == 0) {
            enable_log = 0;
        }else{
            //do nothing
        }
    }
    log_init(enable_log, level);

    LOG_INFO("ez_routerd started");

    LOG_INFO("ez_routerd starting...\n");

    // 初始化插件系统
    // 初始化 reactor
    reactor_init();

    // IPC server
    ipc_server_init("/run/ez_router/ez_router.sock");

    // 载入 config.json
    // if (!load_config("/home/aniston/Desktop/ez_router/out/config.json")) {
    if (!load_config("config.json")) {
        LOG_INFO("[daemon] restoring routes...\n");
        LOG_INFO("[daemon] load plugins\n");

        //load plugin
        for(int i=0;i<g_config.plugin_count;i++){
            plugin_def_t* p=&g_config.plugins[i];
            plugin_load(p->path);
        }

        config_print();
        LOG_INFO("[daemon] open ports\n");
        ports_open_all();

        //set routes reactor
        for(int i=0;i<g_config.port_count;i++){
            // route_def_t* rt=&g_config.routes[i];
            // printf("src=%s,  handler=%s\n",rt->src,rt->handler);
            // port_def_t* port=port_find_by_name(rt->src);
            // reactor_add_fd(fd);// add fd
            LOG_INFO("reactor add port name=%s \n",g_config.ports[i].base.name);
            reactor_add_port(&g_config.ports[i]);
        }

    }


    pthread_t th_reactor, th_disp, th_ipc;
    pthread_create(&th_reactor, NULL, reactor_thread, NULL);
    pthread_create(&th_disp, NULL, dispatcher_thread, NULL);
    pthread_create(&th_ipc,NULL,ipc_thread,NULL);

    //control plane: ipc command handler
    //pthread_create(&th_ipc,NULL,ipc_thread,NULL);
    LOG_INFO("ez_routerd ready.\n");


    while (run_state_is_running()) {
        sleep(1);
    }

    pthread_join(th_reactor, NULL);
    pthread_join(th_ipc,NULL);
    pthread_join(th_disp,NULL);
    LOG_INFO("[main] exit complete");
    return 0;
}

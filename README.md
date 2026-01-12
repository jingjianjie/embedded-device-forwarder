# ez_router  
**OrangePi 本地 端口路由器**

---

## 1. ez_router 是什么

`ez_router` 是运行在 OrangePi（Linux）上的一个**本地端口转发器**。

它通过 reactor->router机制，把多个本地端口挂载到后台进程中，并按照 `config.json` 中定义的路由规则，在这些端口之间**转发数据流**。

它不解析协议、不理解业务，只做一件事：

> 按配置，把一个端口收到的数据原样转发给另一个端口。

---

## 2. 程序运行

  进入out目录，配置好config.json路由表，执行./ez_router 
  config.json中ports列出ez_router用到的所有端口，plugin字段标记转发过程的数据拦截插件,routers字段标记转发源和转发方向





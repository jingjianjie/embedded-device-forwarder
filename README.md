# ez_router  
**OrangePi 本地 端口路由器**

---

## 1. ez_router 是什么

`ez_router` 是运行在 OrangePi（Linux）上的一个**本地端口转发器**。

它通过事件触发机制，把多个本地端口挂载到后台进程中，并按照 `config.json` 中定义的路由规则，在这些端口之间**转发数据流**。

它不解析协议、不理解业务，只做一件事：

> 按配置，把一个端口收到的数据原样转发给另一个端口。

---

## 2. 程序运行

  进入out目录，配置好config.json路由表，执行./ez_router 
  config.json中ports列出ez_router用到的所有端口，类型有
  1） tty：串口
  2） ipc：unix socket文件
  3） tcp_server 网口

    "ports": [
        {
            "name": "UART1",
            "type": "tty",
            "use_frame":true,
            "tty": {
                "path": "/tmp/ttyV0",
                "baudrate": 115200,
                "databits": 8,
                "stopbits": 1,
                "parity": 0,
                "flow": 0
            }
        },
        {
            "name": "USB_CDC",
            "type": "tty",
            "use_frame":false,
            "tty": {
                "path": "/tmp/ttyV2",
                "baudrate": 115200,
                "databits": 8,
                "stopbits": 1,
                "parity": 0,
                "flow": 0
            }
        },
        {
            "name": "UART2",
            "type": "tty",
            "use_frame":false,
            "tty": {
                "path": "/tmp/ttyV4",
                "baudrate": 9600,
                "databits": 8,
                "stopbits": 1,
                "parity": 0,
                "flow": 0
            }
        },
        {
            "name": "HOST_IPC",
            "type": "ipc",
            "use_frame":false,
            "ipc":{
                "path":"/tmp/test.sock"
            }


        },
        {
            "name": "NET0",
            "type": "tcp_server",
            "use_frame":false,
            "tcp_server": {
                "bind": "192.169.30.2",
                "port": 9000,
                "backlog": 5
            }
        }
    ],




## 3.程序编译

  编译ez_router程序，进入routerd目录，执行 make 指令，编译结果在out目录下

## 4.插件编译and使用
  
  插件编译： 进入ez_router下的plugins目录执行 make   编译后的插件会自动放到out/plugin/
  插件配置： config.json中配置插件内容如下，plugins中填写插件名称及所在目录，在要拦截数据的地方配置插件的handler，
  按照${插件名}.${函数名} 

            "plugins": [
                    {
                        "name": "filter",
                        "path": "plugin/filter.so"
                    },
                    {
                        "name": "codec",
                        "path": "plugins/codec.so"
                    }
                ],

                "routes": [
                    {
                        "src": "UART1",
                        "dst": "HOST_IPC",
                        "plugin": "filter",
                        "handler": "filter.uart_to_usb"
                    },
                    {
                        "src": "HOST_IPC",
                        "dst": "UART1",
                        "plugin": "filter",
                        "handler": "filter.usb_to_uart"
                    }
                ]

  

## 5.示例测试
  uart1 <----> NET0(网口)
  uart1 <----> orangepi 供电口(供电口也可虚拟出串口)
  "/tmp/test.sock"  <----> uart




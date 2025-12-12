#!/bin/bash
# manage_ezrouterd.sh - 管理 ez_routerd 守护进程

SERVICE_NAME=ez_routerd
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN_SRC="$SCRIPT_DIR/../out/ez_routerd"
BIN_DST="/usr/local/bin/$SERVICE_NAME"
SERVICE_FILE="/etc/systemd/system/$SERVICE_NAME.service"

CONFIG_SRC="$SCRIPT_DIR/../out/config.json"
PLUGIN_SRC="$SCRIPT_DIR/../out/plugin"
CONFIG_DIR="/etc/ez_router"
PLUGIN_DIR="$CONFIG_DIR/plugin"

# -------------------------
# 通用函数
# -------------------------
check_root() {
    if [[ $EUID -ne 0 ]]; then
        echo "请使用 sudo 运行此脚本。"
        exit 1
    fi
}

reload_systemd() {
    systemctl daemon-reload
    systemctl reset-failed
}

kill_old_process() {
    pkill -x $SERVICE_NAME 2>/dev/null
    sleep 1
    pkill -9 -x $SERVICE_NAME 2>/dev/null
}

check_running() {
    if pgrep -x $SERVICE_NAME >/dev/null; then
        echo "$SERVICE_NAME 正在运行 (PID: $(pgrep -x $SERVICE_NAME | tr '\n' ' '))"
        return 0
    else
        echo "$SERVICE_NAME 未在运行"
        return 1
    fi
}

# -------------------------
# install
# -------------------------
install_service() {
    check_root

    echo "=== 安装 EZ Router 守护进程 ==="

    # 拷贝主程序
    if [[ ! -f $BIN_SRC ]]; then
        echo "错误: 未找到可执行文件 $BIN_SRC"
        exit 1
    fi
    echo "复制可执行文件到 $BIN_DST"
    cp -f "$BIN_SRC" "$BIN_DST"
    chmod +x "$BIN_DST"

    # 拷贝配置文件
    echo "复制配置文件到 $CONFIG_DIR"
    mkdir -p "$CONFIG_DIR"
    if [[ -f $CONFIG_SRC ]]; then
        cp -f "$CONFIG_SRC" "$CONFIG_DIR/config.json"
    else
        echo "警告: 未找到配置文件 $CONFIG_SRC"
    fi

    # 拷贝插件目录
    echo "复制插件目录到 $PLUGIN_DIR"
    mkdir -p "$PLUGIN_DIR"
    if [[ -d $PLUGIN_SRC ]]; then
        cp -rf "$PLUGIN_SRC/"* "$PLUGIN_DIR/" 2>/dev/null
    else
        echo "警告: 未找到插件目录 $PLUGIN_SRC"
    fi

    # 创建 systemd 服务文件
    echo "创建 systemd 服务文件..."
    cat <<EOF > "$SERVICE_FILE"
[Unit]
Description=EZ Router Daemon
After=network-online.target
Wants=network-online.target

[Service]
ExecStart=$BIN_DST -log
WorkingDirectory=$CONFIG_DIR
Restart=on-failure
User=root

[Install]
WantedBy=multi-user.target
EOF

    reload_systemd
    systemctl enable $SERVICE_NAME
    systemctl start $SERVICE_NAME
    sleep 1
    systemctl status $SERVICE_NAME --no-pager
    echo "安装完成并已启动。"
}

# -------------------------
# uninstall
# -------------------------
uninstall_service() {
    check_root
    echo "停止并删除服务..."
    systemctl stop $SERVICE_NAME 2>/dev/null
    systemctl disable $SERVICE_NAME 2>/dev/null
    kill_old_process
    rm -f "$SERVICE_FILE"
    rm -f "$BIN_DST"
    reload_systemd
    echo "卸载完成，服务文件和旧进程已清理。"
}

# -------------------------
# stop
# -------------------------
stop_service() {
    check_root
    echo "尝试停止 $SERVICE_NAME..."
    systemctl stop $SERVICE_NAME 2>/dev/null
    kill_old_process
    echo "已停止。"
}

# -------------------------
# start
# -------------------------
start_service() {
    check_root
    echo "启动 $SERVICE_NAME..."
    systemctl start $SERVICE_NAME 2>/dev/null || "$BIN_DST" -log &
    sleep 1
    check_running
}

# -------------------------
# status
# -------------------------
status_service() {
    check_running
    systemctl status $SERVICE_NAME 2>/dev/null | grep -E "Loaded|Active|ExecStart"
    ss -tnlp | grep $SERVICE_NAME || echo "未检测到监听端口。"
}

# -------------------------
# disable-now
# -------------------------
disable_now() {
    check_root
    echo "彻底停用并清理 $SERVICE_NAME ..."
    systemctl stop $SERVICE_NAME 2>/dev/null
    systemctl disable $SERVICE_NAME 2>/dev/null
    kill_old_process
    rm -f "$SERVICE_FILE"
    reload_systemd
    echo "已彻底禁用，旧进程与服务文件已删除。"
}

# -------------------------
# 主控制逻辑
# -------------------------
case "$1" in
    install)
        install_service
        ;;
    uninstall)
        uninstall_service
        ;;
    start)
        start_service
        ;;
    stop)
        stop_service
        ;;
    status)
        status_service
        ;;
    disable-now)
        disable_now
        ;;
    *)
        echo "用法: sudo ./manage_ezrouterd.sh [install|uninstall|start|stop|status|disable-now]"
        ;;
esac

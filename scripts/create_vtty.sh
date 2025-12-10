#!/bin/bash

# -----------------------------
#  虚拟串口创建脚本
#  用法：
#     ./create_vtty.sh 3
#  创建 3 对虚拟串口（6 个端口）
# -----------------------------

# 1. 检查是否提供数量参数
if [ -z "$1" ]; then
    echo "用法: $0 <pairs>"
    echo "例如: $0 3  # 创建3对串口"
    exit 1
fi

PAIRS=$1

# 2. 检查 socat 是否安装
if ! command -v socat >/dev/null 2>&1; then
    echo "错误: socat 未安装，请运行："
    echo "sudo apt install socat"
    exit 1
fi

# 3. 清理旧的虚拟串口链接
echo "清理旧的 /tmp/ttyV* ..."
rm -f /tmp/ttyV*

# 4. 启动虚拟串口
echo "启动 $PAIRS 对虚拟串口..."
echo

for ((i=0; i<$PAIRS; i++)); do
    A=$((i*2))
    B=$((i*2+1))

    LINK_A="/tmp/ttyV${A}"
    LINK_B="/tmp/ttyV${B}"

    echo "创建虚拟串口对:  $LINK_A  <-->  $LINK_B"

    socat -d -d PTY,link=$LINK_A,raw,echo=0 PTY,link=$LINK_B,raw,echo=0 &
    sleep 0.2
done

echo
echo "虚拟串口创建完成！"
echo "使用 ls -l /tmp/ttyV* 查看结果"
echo
echo "使用示例:"
echo "  cat /tmp/ttyV1"
echo "  echo hello > /tmp/ttyV0"
echo
echo "按需使用 killall socat 清理后台进程"
echo

# 编译器与参数
CC = gcc
CFLAGS = -Wall -pthread -Iinclude -rdynamic

# 输出目录
OUT_DIR = ../out

# 目标文件
TARGET = $(OUT_DIR)/ez_routerd

# 源文件
SRCS = src/ez_routerd.c \
	src/reactor.c \
	src/router_link.c \
	src/ipc_server.c \
	src/event_queue.c \
	src/router_core.c \
	src/port_map.c \
	src/config_store.c \
	src/plugin_loader.c \
	src/port_manager.c \
	src/log.c \
	src/run_state.c \
# 	src/uart_handler.c 
# 	src/forward.c \



SRCS+=3rdparty/cjson/cJSON.c
CFLAGS+=-I3rdparty/cjson


# GDB debug 
DEBUG_CFLAGS= -g -DDEBUG

# 默认规则
all: $(OUT_DIR) $(TARGET)

# 自动创建 out 目录
$(OUT_DIR):
	mkdir -p $(OUT_DIR)

# 编译目标
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)
	@echo ""
	@echo "Build success!"
	@echo "Executable output → $(TARGET)"
	@echo ""


# 调试目标
debug: $(OUT_DIR) $(SRCS)
	$(CC) $(CFLAGS) $(DEBUG_CFLAGS) -o $(TARGET) $(SRCS)
	@echo ""
	@echo "Debug build success!"
	@echo "Executable output → $(TARGET)"
	@echo "Start GDB with: gdb $(TARGET)"
	@echo ""

# 清理
clean:
	rm -f $(TARGET)

# 编译器参数
CC = gcc
CFLAGS = -Wall -fPIC -I.

# 输出
SDK_OBJS = ez_router_sdk.o
SDK_LIB = libezrouter_sdk.a

all: $(SDK_LIB)

# 编译 SDK 实现文件
ez_router_sdk.o: ez_router_sdk.c ez_router_sdk.h
	$(CC) $(CFLAGS) -c ez_router_sdk.c -o ez_router_sdk.o

# 打包静态库
$(SDK_LIB): $(SDK_OBJS)
	ar rcs $(SDK_LIB) $(SDK_OBJS)

clean:
	rm -f *.o *.a

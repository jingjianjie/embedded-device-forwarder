CC=gcc
CFLAGS=-fPIC -shared -Wall -I../routerd/include   # 根据情况调整头文件路径

all: filter.so

filter.so: filter.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f filter.so

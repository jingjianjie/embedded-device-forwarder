# CC=gcc
# CFLAGS=-Wall -I../routerd/include

# all: client_test

# client_test: client_test.c
# 	$(CC) $(CFLAGS) -o client_test client_test.c

# clean:
# 	rm -f client_test
CC = gcc
CFLAGS = -Wall -I../sdk

# sdk 静态库路径
SDK_LIB = ../sdk/libezrouter_sdk.a

all: sdk_test

sdk_test: sdk_test.c $(SDK_LIB)
	$(CC) $(CFLAGS) -o sdk_test sdk_test.c $(SDK_LIB)

clean:
	rm -f sdk_test

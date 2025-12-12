# ========================
# ez_router 插件统一构建系统
# ========================

CC      = gcc
CFLAGS  = -fPIC -shared -Wall -I../routerd/include
OUTDIR  = ../out/plugin

# 自动识别当前目录下所有 .c 文件
SRCS    := $(wildcard *.c)
# 把每个 .c 文件对应成 ../out/plugin/xxx.so
TARGETS := $(patsubst %.c,$(OUTDIR)/%.so,$(SRCS))

# 默认目标：编译所有插件
all: $(OUTDIR) $(TARGETS)

# 生成每个插件的 .so
$(OUTDIR)/%.so: %.c
	@echo "Building plugin: $@"
	$(CC) $(CFLAGS) -o $@ $^

# 输出目录
$(OUTDIR):
	mkdir -p $(OUTDIR)

# 清理
clean:
	rm -f $(TARGETS)

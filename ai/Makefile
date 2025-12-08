obj-m := myipc_mod.o
# KDIR 指向内核源码的构建目录
KDIR := $(HOME)/sourceCodes/WSL2-Linux-Kernel-linux-msft-wsl-6.6.87.2
# PWD 是当前工作目录
PWD := $(shell pwd)
# 默认目标
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules KLIB=$(KDIR) KLIB_BUILD=$(KDIR)
	gcc -o ipc libipc.c ipc_test.c -lpthread -lmnl
	gcc -o mq_server mq_server.c libipc.c -lpthread -lmnl
	gcc -o mq_client mq_client.c libipc.c -lpthread -lmnl	
# 清理目标
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf ipc mq_server mq_client

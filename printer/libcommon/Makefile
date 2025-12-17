TOPDIR := $(shell cd ../;pwd)
include $(TOPDIR)/build/base.mk

TARGET := libcommon.a
OBJS := $(patsubst %.c,%.o,$(wildcard *.c))
CFLAGS += -I./include -DTLS_OPENSSL

all: $(TARGET)
include $(MAKE_STATIC_LIB)
include $(MAKE_CLEAN)

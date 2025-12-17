TOPDIR := $(shell cd ../../../;pwd)
include $(TOPDIR)/build/base.mk

TARGET := libtspl.so
OBJS := $(patsubst %.c,%.o,$(wildcard *.c))
CFLAGS += -I../inc -I../inc/cairo -I../inc/freetype2 -I../inc/fontconfig -I../inc/harfbuzz -fPIC
LDFLAGS += -lcairo -lfreetype -lzint -lm -ldl

all: $(TARGET)
include $(MAKE_DYN_LIB)
include $(MAKE_CLEAN)

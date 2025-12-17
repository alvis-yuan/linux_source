TOPDIR := $(shell cd ../../;pwd)
include $(TOPDIR)/build/base.mk

TARGET := PrintServer
OBJS := $(patsubst %.c,%.o,$(wildcard src/*.c))
CFLAGS += -Iinc -Iinc/cairo -Iinc/freetype2 -Iinc/harfbuzz -Isrc/tspl
LDFLAGS += -pthread -lcommon -lzint -lrt -lcrypto -lcJSON -ldl -lm -lz

all: tspl_static_lib $(TARGET)

tspl_static_lib:
	make -C TSPL clean
	make -C TSPL install

include $(MAKE_APP)
include $(MAKE_CLEAN)

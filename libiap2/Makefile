TOPDIR := $(shell cd ../..;pwd)
include $(TOPDIR)/build/base.mk

TARGET := libiap2.so

# 定义所有需要处理的子目录
MFI_SUBDIRS := iAP2Link iAP2Session driver iAP2Utility iAP2UtilityImplementation iAP2Transport iAP2Adapt

# 一次性处理所有子目录
OBJS += $(foreach dir, $(MFI_SUBDIRS), $(patsubst %.c,%.o,$(wildcard $(dir)/*.c)))
CFLAGS += -I . $(foreach dir, $(MFI_SUBDIRS), -I $(dir))

#CFLAGS += -DDEBUG=1
CFLAGS += -Wall -ffunction-sections -O2 -mhard-float -s -mips32r2 --sysroot=$(SYS_ROOT) -fPIC


include $(MAKE_DYN_LIB)
include $(MAKE_CLEAN)


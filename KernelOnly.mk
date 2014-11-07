#Android makefile to build kernel as a part of Android Build
PERL		= perl

ifeq ($(TARGET_PREBUILT_KERNEL),)

KERNEL_OUT := arch/arm/boot
TARGET_PREBUILT_INT_KERNEL := $(KERNEL_OUT)/zImage
TARGET_PREBUILT_FOR_DTB := $(KERNEL_OUT)/zImage_dtb
export KERNEL_ONLY := true

ifeq ($(TARGET_USES_UNCOMPRESSED_KERNEL),true)
$(info Using uncompressed kernel)
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/piggy
else
TARGET_PREBUILT_KERNEL := $(TARGET_PREBUILT_FOR_DTB)
endif


DTS_FILES = $(wildcard arch/arm/boot/dts/liger/liger*.dts)
DTS_FILE = $(lastword $(subst /, ,$(1)))
DTB_FILE = $(addprefix $(KERNEL_OUT)/,$(patsubst %.dts,%.dtb,$(call DTS_FILE,$(1))))
ZIMG_FILE = $(addprefix $(KERNEL_OUT)/,$(patsubst %.dts,%-zImage,$(call DTS_FILE,$(1))))
KERNEL_ZIMG = $(KERNEL_OUT)/zImage
DTC = scripts/dtc/dtc
KERNEL_TEMP = $(KERNEL_OUT)/zImage_dtb

define make-dtb
$(foreach d, $(DTS_FILES), \
	$(DTC) -p 1024 -O dtb -o $(call DTB_FILE,$(d)) $(d);)
endef

define append-dtb
	cat $(KERNEL_ZIMG) $(wildcard $(KERNEL_OUT)/*.dtb) > $(KERNEL_TEMP)
endef

define mv-modules
mdpath=`find $(KERNEL_MODULES_OUT) -type f -name modules.dep`;\
if [ "$$mdpath" != "" ];then\
mpath=`dirname $$mdpath`;\
ko=`find $$mpath/kernel -type f -name *.ko`;\
for i in $$ko; do mv $$i $(KERNEL_MODULES_OUT)/; done;\
fi
endef

all:
	ARCH=arm make ${SEL_DEFCONFIG}
	ARCH=arm CROSS_COMPILE=../../tools/toolchain/gcc-linaro-arm-linux-gnueabihf-4.7-2013.04-20130415_linux/bin/arm-linux-gnueabihf- make -j${J_OP}
	ARCH=arm CROSS_COMPILE=../../tools/toolchain/gcc-linaro-arm-linux-gnueabihf-4.7-2013.04-20130415_linux/bin/arm-linux-gnueabihf- make modules
	INSTALL_MOD_PATH=./temp ARCH=arm CROSS_COMPILE=../../tools/toolchain/gcc-linaro-arm-linux-gnueabihf-4.7-2013.04-20130415_linux/bin/arm-linux-gnueabihf- make modules_install
	$(make-dtb)

adtb:
	$(append-dtb)
endif

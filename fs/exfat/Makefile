
obj-m += exfatfs.o

exfatfs-y := exfat_super.o exfat.o exfat_api.o exfat_blkdev.o exfat_cache.o \
				exfat_data.o exfat_global.o exfat_nls.o exfat_oal.o exfat_upcase.o

ANDROID_PRODUCT_OUT := $(strip $(ANDROID_PRODUCT_OUT))
ifeq ($(ANDROID_PRODUCT_OUT),)
KOUT := /lib/modules/$(shell uname -r)/build
else
KOUT := $(ANDROID_PRODUCT_OUT)/obj/KERNEL_OBJ
endif
ARM_EABI_TOOLCHAIN := $(strip $(ARM_EABI_TOOLCHAIN))
ifneq ($(ARM_EABI_TOOLCHAIN),)
L_ARCH := "ARCH=arm"
L_ARCH64 := "ARCH=arm64"
L_CROSS_COMPILE := "CROSS_COMPILE=$(ARM_EABI_TOOLCHAIN)/arm-eabi-"
L_CROSS_COMPILE64 := "CROSS_COMPILE=$(ARM_EABI_TOOLCHAIN)/../../../aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-"
else
#FIXME
#L_ARCH64 := "ARCH=arm64"
#L_CROSS_COMPILE64 := "CROSS_COMPILE=arm-linux-gnueabi-"
endif

PWD	:= $(shell pwd)

all:
	$(MAKE) -C $(KOUT) $(L_CROSS_COMPILE) $(L_ARCH) \
		M=$(PWD) modules

arm64:
	$(MAKE) -C $(KOUT) $(L_CROSS_COMPILE64) $(L_ARCH64) \
	M=$(PWD) modules

clean:
	$(MAKE) -C $(KOUT) $(L_CROSS_COMPILE) $(L_ARCH) \
	M=$(PWD) clean
	$(MAKE) -C $(KOUT) $(L_CROSS_COMPILE64) $(L_ARCH64) \
	M=$(PWD) clean

help:
	@echo 'arm64	- Build arm64 target'
	@echo 'clean	- remove generated files'

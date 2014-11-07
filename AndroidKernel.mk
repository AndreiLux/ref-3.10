#Android makefile to build kernel as a part of Android Build
PERL		= perl

ifeq ($(TARGET_PREBUILT_KERNEL),)

KERNEL_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
KERNEL_CONFIG := $(KERNEL_OUT)/.config
TARGET_PREBUILT_INT_KERNEL := $(KERNEL_OUT)/arch/arm/boot/zImage
TARGET_PREBUILT_FOR_DTB := $(KERNEL_OUT)/arch/arm/boot/zImage_dtb
KERNEL_HEADERS_INSTALL := $(KERNEL_OUT)/usr
KERNEL_MODULES_INSTALL := system
KERNEL_MODULES_OUT := $(TARGET_OUT)/lib/modules
KERNEL_IMG=$(KERNEL_OUT)/arch/arm/boot/Image

ifeq ($(TARGET_USES_UNCOMPRESSED_KERNEL),true)
$(info Using uncompressed kernel)
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/piggy
else
TARGET_PREBUILT_KERNEL := $(TARGET_PREBUILT_FOR_DTB)
endif


DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/$(DTS_TARGET)/$(DTS_NAME)*.dts)
DTS_FILE = $(lastword $(subst /, ,$(1)))
DTB_FILE = $(addprefix $(KERNEL_OUT)/arch/arm/boot/,$(patsubst %.dts,%.dtb,$(call DTS_FILE,$(1))))
ZIMG_FILE = $(addprefix $(KERNEL_OUT)/arch/arm/boot/,$(patsubst %.dts,%-zImage,$(call DTS_FILE,$(1))))
KERNEL_ZIMG = $(KERNEL_OUT)/arch/arm/boot/zImage
DTC = $(KERNEL_OUT)/scripts/dtc/dtc
KERNEL_TEMP = $(KERNEL_OUT)/arch/arm/boot/zImage_dtb

define make-dtb
mkdir -p $(KERNEL_OUT)/arch/arm/boot;\
$(foreach d, $(DTS_FILES), \
	$(DTC) -p 1024 -O dtb -o $(call DTB_FILE,$(d)) $(d);)
endef

define append-dtb
	cat $(KERNEL_ZIMG) $(wildcard $(KERNEL_OUT)/arch/arm/boot/*.dtb) > $(KERNEL_TEMP)
endef

define mv-modules
mdpath=`find $(KERNEL_MODULES_OUT) -type f -name modules.dep`;\
if [ "$$mdpath" != "" ];then\
mpath=`dirname $$mdpath`;\
ko=`find $$mpath/kernel -type f -name *.ko`;\
for i in $$ko; do mv $$i $(KERNEL_MODULES_OUT)/; done;\
fi
endef

define clean-module-folder
mdpath=`find $(KERNEL_MODULES_OUT) -type f -name modules.dep`;\
if [ "$$mdpath" != "" ];then\
mpath=`dirname $$mdpath`; rm -rf $$mpath;\
fi
endef

$(KERNEL_OUT):
	mkdir -p $(KERNEL_OUT)

$(KERNEL_CONFIG): $(KERNEL_OUT)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- $(KERNEL_DEFCONFIG)

$(KERNEL_OUT)/piggy : $(TARGET_PREBUILT_INT_KERNEL)
	$(hide) gunzip -c $(KERNEL_OUT)/arch/arm/boot/compressed/piggy.gzip > $(KERNEL_OUT)/piggy

$(TARGET_PREBUILT_INT_KERNEL): $(KERNEL_OUT) $(KERNEL_CONFIG) $(KERNEL_HEADERS_INSTALL)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules
	$(MAKE) -C kernel O=../$(KERNEL_OUT) INSTALL_MOD_PATH=../../$(KERNEL_MODULES_INSTALL) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules_install
	$(make-dtb)
	$(mv-modules)
	$(clean-module-folder)

$(TARGET_PREBUILT_FOR_DTB) : $(TARGET_PREBUILT_INT_KERNEL)
	$(append-dtb)

ifeq ($(PRODUCT_SUPPORT_EXFAT), y)
	@cp -f $(ANDROID_BUILD_TOP)/kernel/scripts/tuxera_update.sh $(ANDROID_BUILD_TOP)
	@sh tuxera_update.sh --target target/lg.d/mobile-gh14 --use-cache --latest --max-cache-entries 2 --source-dir $(ANDROID_BUILD_TOP)/kernel --output-dir $(ANDROID_BUILD_TOP)/$(KERNEL_OUT) -a --user lg-mobile --pass AumlTsj0ou
	@tar -xzf tuxera-exfat*.tgz
	@mkdir -p $(TARGET_OUT_EXECUTABLES)
	@cp $(ANDROID_BUILD_TOP)/tuxera-exfat*/exfat/kernel-module/texfat.ko $(ANDROID_BUILD_TOP)/$(TARGET_OUT_EXECUTABLES)/../lib/modules/
	@cp $(ANDROID_BUILD_TOP)/tuxera-exfat*/exfat/tools/* $(TARGET_OUT_EXECUTABLES)
	@rm -f kheaders*.tar.bz2
	@rm -f tuxera-exfat*.tgz
	@rm -rf tuxera-exfat*
	@rm -f tuxera_update.sh
endif

$(KERNEL_HEADERS_INSTALL): $(KERNEL_OUT) $(KERNEL_CONFIG)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- headers_install

kerneltags: $(KERNEL_OUT) $(KERNEL_CONFIG)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- tags

kernelconfig: $(KERNEL_OUT) $(KERNEL_CONFIG)
	env KCONFIG_NOTIMESTAMP=true \
	     $(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- menuconfig
	env KCONFIG_NOTIMESTAMP=true \
	     $(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- savedefconfig
	cp $(KERNEL_OUT)/defconfig kernel/arch/arm/configs/$(KERNEL_DEFCONFIG)

endif

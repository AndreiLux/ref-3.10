#Android makefile to build kernel as a part of Android Build

#ifeq ($(TARGET_PREBUILT_KERNEL),)

KERNEL_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
KERNEL_CONFIG := $(KERNEL_OUT)/.config
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/arm/boot/zImage

KERNEL_ARCH_ARM_CONFIGS := kernel/arch/arm/configs
KERNEL_GEN_CONFIG_FILE := huawei_hi3630_$(HW_PRODUCT)_defconfig
KERNEL_GEN_CONFIG_PATH := $(KERNEL_ARCH_ARM_CONFIGS)/$(KERNEL_GEN_CONFIG_FILE)

KERNEL_COMMON_DEFCONFIG := $(KERNEL_ARCH_ARM_CONFIGS)/$(KERNEL_DEFCONFIG)
KERNEL_PRODUCT_CONFIGS  := device/hisi/hi3630/product_spec/$(HW_PRODUCT)/kernel_config
KERNEL_DEBUG_CONFIGS := $(KERNEL_ARCH_ARM_CONFIGS)/debug

#<DTS2013122705121, add by l00202565 2013.12.17 begin
KERNEL_MODULES_INSTALL := system
KERNEL_MODULES_OUT := $(TARGET_OUT)/lib/modules
#DTS2013122705121 add by l00202565 2013.12.17 end>

ifeq ($(TARGET_BUILD_TYPE),release)
  KERNEL_DEBUG_CONFIGFILE := $(KERNEL_COMMON_DEFCONFIG)

$(KERNEL_DEBUG_CONFIGFILE):FORCE
	echo "do nothing"

else
  KERNEL_DEBUG_CONFIGFILE :=  $(KERNEL_ARCH_ARM_CONFIGS)/huawei_hi3630_$(HW_PRODUCT)_debug_defconfig

$(KERNEL_DEBUG_CONFIGFILE):FORCE
	@device/hisi/hi3630/kernel-config.sh -f $(KERNEL_COMMON_DEFCONFIG) -d $(KERNEL_DEBUG_CONFIGS) -o $(KERNEL_DEBUG_CONFIGFILE)
endif

ifeq ($(KERNEL_DEBUG_CONFIGFILE),$(KERNEL_COMMON_DEFCONFIG))
KERNEL_TOBECLEAN_CONFIGFILE :=
else
KERNEL_TOBECLEAN_CONFIGFILE := $(KERNEL_DEBUG_CONFIGFILE)
endif

#<DTS2013122705121, add by l00202565 2013.12.17 begin
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
#DTS2013122705121 add by l00202565 2013.12.17 end>

DTS_PARSE_CONFIG:
	@-rm -f $(PRODUCT_OUT)/dt.img
	@-rm -rf $(KERNEL_OUT)/arch/arm/boot/dts/auto-generate/
	@cd device/hisi/customize/hsad; ./auto_dts_gen.sh

$(KERNEL_OUT):
	mkdir -p $(KERNEL_OUT)

$(KERNEL_GEN_CONFIG_PATH): $(KERNEL_DEBUG_CONFIGFILE)
	@device/hisi/hi3630/kernel-config.sh -f $(KERNEL_DEBUG_CONFIGFILE) -d $(KERNEL_PRODUCT_CONFIGS) -o $(KERNEL_GEN_CONFIG_PATH)

$(KERNEL_CONFIG): $(KERNEL_OUT) $(KERNEL_GEN_CONFIG_PATH)
	@if [ "$(HIDE_PRODUCT_INFO)" == "true" ]; then echo CONFIG_HIDE_PRODUCT_INFO=y >> $(KERNEL_GEN_CONFIG_PATH); fi
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- $(KERNEL_GEN_CONFIG_FILE)
	@rm -frv $(KERNEL_GEN_CONFIG_PATH)
	@rm -frv $(KERNEL_TOBECLEAN_CONFIGFILE)

$(TARGET_PREBUILT_KERNEL): $(KERNEL_CONFIG) DTS_PARSE_CONFIG
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi-
	$(hide) $(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- -j 18
	$(hide) $(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- -j 18 zImage
#<DTS2013122705121, add by l00202565 2013.12.17 begin
	$(MAKE) -C kernel O=../$(KERNEL_OUT) INSTALL_MOD_PATH=../../$(KERNEL_MODULES_INSTALL) INSTALL_MOD_STRIP=1 ARCH=arm CROSS_COMPILE=arm-eabi- modules_install
	$(mv-modules)
	$(clean-module-folder)
#DTS2013122705121 add by l00202565 2013.12.17 end>

kernelconfig: $(KERNEL_OUT) $(KERNEL_GEN_CONFIG_PATH)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- $(KERNEL_GEN_CONFIG_FILE) menuconfig
	@rm -frv $(KERNEL_GEN_CONFIG_PATH)

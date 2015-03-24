#######################################
# Sepcify source files
#######################################
MASP_CORE_DRIVER_DIR := $(src)

asf_link := ..
lib_obj := module/sec_mod.o
ccflags-y += -Werror

ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)

include $(MASP_CORE_DRIVER_DIR)/$(asf_link)/Makefile.core
include $(MASP_CORE_DRIVER_DIR)/mach/Makefile.mach

ifeq ($(CONFIG_CUSTOM_SEC_AUTH_SUPPORT),y)
CCCI_CUSTOM_DRIVER_DIR := $(call my-dir)$(call to-root,$(obj))mediatek/custom/out/$(call lc,$(MTK_PROJECT))/kernel/ccci
lib_obj += $(CCCI_CUSTOM_DRIVER_DIR)/cust_auth.o
else
include $(MASP_CORE_DRIVER_DIR)/$(asf_link)/Makefile.crypto
lib_obj += $(crypto_obj)
lib_obj += $(asf_link)/auth/sec_wrapper.o
endif

else
ccflags-y += -DMTK_SECURITY_MODULE_LITE
core_obj += $(asf_link)/core/sec_ops.o
core_obj += $(asf_link)/core/alg_sha1.o
mach_obj += mach/sec_uid.o
EXTRA_CFLAGS += -I$(src)/$(asf_link)/asf_inc
endif

# MODULE or BUILD-IN
#ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)
#obj-m += sec.o
#else
#obj-y += sec.o
#endif
# ALWAYS built-in kernel driver for KK
obj-y += sec.o

# HEADER FILES
EXTRA_CFLAGS += -I$(src)/module

# INCLUDE ASP LIBRARY
sec-y += $(lib_obj) $(core_obj) $(mach_obj)

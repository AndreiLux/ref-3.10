# This top-level kbuild makefile builds all the Linux kernel modules in the
# DDK. To run kbuild, this makefile is copied to $(TARGET_OUT)/kbuild/Makefile
# and make is invoked in $(TARGET_OUT)/kbuild.

# This makefile doesn't define any kbuild special variables apart from
# ccflags-y and obj-m. The variables for objects are picked up by including
# the kbuild makefile fragments named in $(INTERNAL_KBUILD_MAKEFILES). The
# list of objects that these fragments make is collected in
# $(INTERNAL_KBUILD_OBJECTS) and $(INTERNAL_EXTRA_KBUILD_OBJECTS). These
# variables are set according to the build's $(KERNEL_COMPONENTS) and
# $(EXTRA_PVRSRVKM_COMPONENTS). To add a new kernel module to the build, edit
# these variables in the per-build Makefile.

#TARGET_BUILD_TYPE ?= debug
TARGETING_ARM := 1
# Customize this build as per the TARGET_PRODUCT setting
#
ifeq ($(CONFIG_RGX_WODEN),y)
TARGET_PRODUCT := woden
HAL_VARIANT := woden
PVR_SYSTEM := rgx_woden
RGX_BVNC := 1.6.4.8
else ifeq ($(CONFIG_RGX_ODIN),y)
TARGET_DEVICE := arabica
HAL_VARIANT := arabica
PVR_SYSTEM := rgx_odin
RGX_BVNC := 1.49.4.15
else ifeq ($(CONFIG_RGX_ODIN_ES2),y)
TARGET_DEVICE := arabica
HAL_VARIANT := arabica
PVR_SYSTEM := rgx_odin
RGX_BVNC := 1.81.4.15
else
TARGET_PRODUCT := nohw
HAL_VARIANT := nohw
PVR_SYSTEM := rgx_nohw
RGX_BVNC := 1.7.4.5
endif

ifeq ($(CONFIG_RGX_ION),y)
SUPPORT_ION := 1
else
SUPPORT_ION := 0
endif

SUPPORT_ODIN_ION := 1

ifeq ($(CONFIG_RGX_APM_RASCALDUST),y)
RGX_APM_RASCALDUST := 1
else
RGX_APM_RASCALDUST := 0
endif
ifeq ($(CONFIG_RGX_APM_TLA),y)
RGX_APM_TLA := 1
else
RGX_APM_TLA := 0
endif
ifeq ($(CONFIG_RGX_DVFS),y)
RGX_DVFS := 1
else
RGX_DVFS := 0
endif

ifeq ($(CONFIG_AVOID_LOCKDEP_FALSE_POSITIVE_WARN), y)
PVR_AVOID_LOCKDEP_WARN := 1
else
PVR_AVOID_LOCKDEP_WARN := 0
endif

# ddk top with the specific version
KERNELDIR = $(srctree)
RGXKMTOP = $(KERNELDIR)/drivers/gpu/rgx/
ANDROID_ROOT = $(KERNELDIR)/..
OUT_DIR ?= $(ANDROID_ROOT)/out

ifeq ($(TARGET_BUILD_TYPE),debug)
TARGET_ROOT := $(OUT_DIR)/debug/target
else
TARGET_ROOT := $(OUT_DIR)/target
endif

$(warning $(RGXKMTOP))
OUT = $(KERNEL_OUT)

include $(RGXKMTOP)/build/linux/config/preconfig.mk
include $(RGXKMTOP)/build/linux/common/android/paths.mk
include $(RGXKMTOP)/build/linux/common/android/armv7-a.mk
include $(RGXKMTOP)/build/linux/common/android/features.mk

ifeq ($(DRM_DC_MODULE),1)
 DISPLAY_CONTROLLER := dc_drmexample
 DISPLAY_CONTROLLER_DIR := dc_drmexample
 SUPPORT_DRM := 1
 SUPPORT_DRM_DC_MODULE := 1
else
 DISPLAY_CONTROLLER := dc_fbdev
 DISPLAY_CONTROLLER_DIR := dc_fbdev
endif

ifeq ($(SUPPORT_DRM),1)
 LDM_PLATFORM := 1
 PVR_DRM_PLATFORM_DEV := 1
 PVR_DRM_STATIC_BUS_ID := 1
 ifeq ($(call kernel-version-at-least,3,1),true)
  PVR_DRM_DEV_BUS_ID := "\"platform:rgx_odin:-1\""
 endif
 ifeq ($(SUPPORT_DRM_DC_MODULE),1)
  KERNEL_COMPONENTS += $(DISPLAY_CONTROLLER)
 else
  EXTRA_PVRSRVKM_COMPONENTS += $(DISPLAY_CONTROLLER_DIR)
 endif
endif
ifneq ($(strip $(KERNELDIR)),)
 include $(RGXKMTOP)/build/linux/config/kernel_version.mk
 ifeq ($(call kernel-version-at-least,2,6,39),true)
  ifeq ($(LDM_PLATFORM),1)
   PVR_LDM_PLATFORM_PRE_REGISTERED := 1
   PVR_LDM_PLATFORM_PRE_REGISTERED_DEV := "\"pvrsrvkm\""
  else
   LDM_DEVICE_CLASS := 1
   KERNEL_COMPONENTS += $(DISPLAY_CONTROLLER)
  endif
 endif
endif

#If release build have to turn off PDUMP
PDUMP := 0
BUILD := release
NO_HARDWARE := 0
LMA := 0

ifeq ($(TARGETING_ARM),1)
CACHEFLUSH_TYPE ?= CACHEFLUSH_GENERIC
else
CACHEFLUSH_TYPE ?= CACHEFLUSH_X86
endif

#odin Modifying
PVR_MMAP_USE_VM_INSERT := 0
SUPPORT_MMU_PxE_MAP_ON_DEMAND := 0
PVR_SECURE_HANDLES := 1
RGXFW_ALIGNCHECKS := 0
#debug mode
PVRSRV_ENABLE_MEMORY_STATS := 0
PVR_RI_DEBUG := 0
SUPPORT_MMU_MODIFICATION_LOGGING := 0
PVR_ANDROID_NATIVE_FENCE_DEBUG := 0
PVRSRV_NEED_PVR_DPF := 1
PVR_SYNC_PRIM_ALLOC_TRACE := 0
SUPPORT_KERNEL_HWPERF := 0
RGX_HWPERF_SLC_PERF := 0
SUPPORT_DUMP_CLIENT_CCB_COMMANDS := 0
PVRSRV_DEBUG_CCB_MAX := 256
PVRSRV_ENABLE_FW_TRACE_DEBUGFS := 0
DEBUG_HANDLEALLOC_KM := 0
PVR_DBG_BREAK_ASSERT_FAIL := 0
PVRSRV_RESET_ON_HWTIMEOUT := 0
DEBUG_LINUX_MEMORY_ALLOCATIONS := 0
DEBUG_LINUX_MEM_AREAS := 0
DEBUG_LINUX_MMAP_AREAS := 0
DEBUG_BRIDGE_KM := 0
PVRGDB := 0
PVR_DEBUG_MUTEXES := 0
SRC_DEBUG := 0
REFCOUNT_DEBUG := 0

# Should be last
include $(RGXKMTOP)/build/linux/config/core.mk
include $(RGXKMTOP)/build/linux/common/android/extra_config.mk


# Kbuild starts from here..

include $(OUT)/config_kernel.mk


.SECONDARY:

define symlink-source-file
@if [ ! -e $(dir $@) ]; then mkdir -p $(dir $@); fi
@if [ ! -h $@ ]; then ln -sf $< $@; fi
endef

bridge_base := $(RGXKMTOP)/generated

$(OUT)/target/kbuild/external/%.c: $(EXTRA_KBUILD_SOURCE)/%.c
	$(symlink-source-file)

$(OUT)/target/kbuild/%.c: $(RGXKMTOP)/%.c
	$(symlink-source-file)

$(OUT)/target/kbuild/generated/%.c: $(OUT)/target/intermediates/%.c
	$(symlink-source-file)

ccflags-y += -D__linux__ -include $(OUT)/config_kernel.h \
 -DDEBUG_LOG_PATH_TRUNCATE="\"$(OUT)/target/kbuild\"" \
 -I$(OUT)/include \
 -I$(RGXKMTOP)/include \
 -I$(RGXKMTOP)/hwdefs/km \
 -I$(RGXKMTOP)/services/include \
 -I$(RGXKMTOP)/services/include/shared \
 -I$(RGXKMTOP)/services/shared/include \
 -I$(RGXKMTOP)/services/shared/common \
 -I$(RGXKMTOP)/services/system/$(PVR_SYSTEM) \
 -I$(RGXKMTOP)/services/system/include \
 -I$(RGXKMTOP)/services/server/common \
 -I$(RGXKMTOP)/services/server/devices/rgx \
 -I$(RGXKMTOP)/services/server/env/linux \
 -I$(RGXKMTOP)/services/server/include

# include $(RGXKMTOP)/build/linux/toplevel.mk
 include $(RGXKMTOP)/services/server/env/linux/Kbuild.mk
 include $(RGXKMTOP)/services/3rdparty/dc_fbdev/Kbuild.mk
 include $(RGXKMTOP)/tools/services/debug/dbgdriv/Kbuild.mk

include $(INTERNAL_KBUILD_MAKEFILES)

$(if $(pvrsrvkm-y),,$(error pvrsrvkm-y was empty, which could mean that srvkm is missing from $$(KERNEL_COMPONENTS)))
pvrsrvkm-y += $(foreach _m,$(INTERNAL_EXTRA_KBUILD_OBJECTS:.o=),$($(_m)-y))

ifeq ($(CONFIG_RGX_WODEN),y)
obj-$(CONFIG_RGX_WODEN) += $(INTERNAL_KBUILD_OBJECTS)
obj-$(CONFIG_RGX_WODEN) += dbgdrv.o
obj-$(CONFIG_RGX_WODEN) += pvrsrvkm.o
obj-$(CONFIG_RGX_WODEN) += dc_fbdev.o
else ifeq ($(CONFIG_RGX_ODIN),y)
obj-$(CONFIG_RGX_ODIN) += $(INTERNAL_KBUILD_OBJECTS)
obj-$(CONFIG_RGX_ODIN) += dbgdrv.o
obj-$(CONFIG_RGX_ODIN) += pvrsrvkm.o
obj-$(CONFIG_RGX_ODIN) += dc_fbdev.o
else ifeq ($(CONFIG_RGX_ODIN_ES2),y)
obj-$(CONFIG_RGX_ODIN_ES2) += $(INTERNAL_KBUILD_OBJECTS)
obj-$(CONFIG_RGX_ODIN_ES2) += dbgdrv.o
obj-$(CONFIG_RGX_ODIN_ES2) += pvrsrvkm.o
obj-$(CONFIG_RGX_ODIN_ES2) += dc_fbdev.o
endif



#
# Makefile for the kernel mobicore drivers
#

include $(MTK_ROOT_BUILD)/Makefile

GUD_ROOT_FOLDER := $(dir $(lastword $(MAKEFILE_LIST)))
# add our modules to kernel.
obj-y += mcKernelApi.o
obj-y += mcDrvModule.o

mcDrvModule-objs := MobiCoreDriver/logging.o \
		MobiCoreDriver/ops.o \
		MobiCoreDriver/mem.o \
		MobiCoreDriver/api.o \
		MobiCoreDriver/pm.o \
		MobiCoreDriver/main.o

mcKernelApi-objs := MobiCoreKernelApi/main.o \
		MobiCoreKernelApi/clientlib.o \
		MobiCoreKernelApi/device.o \
		MobiCoreKernelApi/session.o \
		MobiCoreKernelApi/connection.o

# Release mode by default
ccflags-y := -DNDEBUG -include $(GUD_ROOT_FOLDER)build_tag.h
ccflags-y += -Wno-declaration-after-statement

ccflags-$(CONFIG_MOBICORE_DEBUG) += -DDEBUG
ccflags-$(CONFIG_MOBICORE_VERBOSE) += -DDEBUG_VERBOSE

# Choose one platform from the folder
#MOBICORE_PLATFORM := $(shell (ls -1 $(GUD_ROOT_FOLDER)MobiCoreDriver/platforms | tail -1) )
ccflags-y += -DMC_NETLINK_COMPAT_V37

# Use the available platform folder
#ccflags-y += -I$(GUD_ROOT_FOLDER)MobiCoreDriver/platforms/$(MOBICORE_PLATFORM)
# MobiCore Driver includes
ccflags-y += -I$(GUD_ROOT_FOLDER)MobiCoreDriver/public
# MobiCore KernelApi required incldes
ccflags-y += -I$(GUD_ROOT_FOLDER)MobiCoreKernelApi/include \
		-I$(GUD_ROOT_FOLDER)MobiCoreKernelApi/public
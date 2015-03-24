
#ifeq ($(MTK_GPU_CHIP), MALI400MP1)
#ccflags-y += -DMALI_SUPPORT
#ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/gpu/mali/mali/common
#ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/gpu/mali/mali/include
#ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/gpu/mali/mali/linux
#endif

ccflags-$(CONFIG_ZRAM) += -I$(srctree)/drivers/staging/zram
ccflags-$(CONFIG_ZSMALLOC) += -I$(srctree)/drivers/staging/zsmalloc

obj-y += mlog.o
mlog-y := mlog_dump.o mlog_logger.o

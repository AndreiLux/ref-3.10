ifeq ($(CONFIG_MTK_INTERNAL), y)
ifeq ($(MTK_USE_RESERVED_EXT_MEM), yes)
KBUILD_CFLAGS += -DMTK_USE_RESERVED_EXT_MEM
endif
endif

ccflags-y += -I$(srctree)
obj-y += mt_storage_logger.o
obj-y += mt_io_logger.o
obj-$(CONFIG_MT65XX_TRACER) += trace_mt65xx_mon.o
obj-y += sched/
obj-$(CONFIG_MTK_MEMCFG) += mtk_memcfg.o
obj-y += mtk_meminfo.o
obj-y += system.o

# Hibernation
obj-$(CONFIG_MTK_HIBERNATION) += hibcore/
obj-y += mt_cache_v7.o
obj-y += fiq_smp_call.o

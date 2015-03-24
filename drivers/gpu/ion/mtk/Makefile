#obj-$(CONFIG_ION_MTK) += ion_drv.o ion_mm_heap_old.o ion_sys_contig_heap.o
obj-$(CONFIG_ION_MTK) += ion_drv.o ion_mm_heap.o ion_mm_carveout_heap.o
ccflags-$(CONFIG_ION_MTK)  += -I$(srctree)/drivers/gpu/ion

ifeq ($(CONFIG_MT_ENG_BUILD), y)
ccflags-y += -DION_RUNTIME_DEBUGGER=1
else
ccflags-y += -DION_RUNTIME_DEBUGGER=0
endif

ccflags-y += -I$(srctree)/include -I$(srctree)/include/trustzone

ifeq ($(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT), y)
ccflags-y += -DION_SUPPORT_TEE=1
else
ccflags-y += -DION_SUPPORT_TEE=0
endif

#obj-y   +=  ion_test_drv/

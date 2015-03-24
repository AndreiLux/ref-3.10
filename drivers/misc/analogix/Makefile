include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-$(CONFIG_SLIMPORT_ANX3618) := slimport.o
slimport-objs := anx3618.o slimport_tx_drv.o

ifeq ($(CONFIG_SLIMPORT_ANX3618),m)
#ccflags-y += -DLOAD_MODULE
endif


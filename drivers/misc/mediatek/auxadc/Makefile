include $(srctree)/drivers/misc/mediatek/Makefile.custom
AUXADC_PLATFORM_DRIVER_DIR :=  $(srctree)/drivers/misc/mediatek/auxadc/$(CONFIG_MTK_PLATFORM)
obj-y += mt_auxadc.o
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
ccflags-y  += -I$(AUXADC_PLATFORM_DRIVER_DIR)

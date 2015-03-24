include $(srctree)/drivers/misc/mediatek/Makefile.custom
LEDS_PLATFORM_DRIVER_DIR :=  $(srctree)/drivers/misc/mediatek/leds/$(CONFIG_MTK_PLATFORM)
obj-$(CONFIG_MTK_LEDS) += leds_drv.o
obj-$(CONFIG_MTK_LEDS) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
ccflags-y  += -I$(LEDS_PLATFORM_DRIVER_DIR)

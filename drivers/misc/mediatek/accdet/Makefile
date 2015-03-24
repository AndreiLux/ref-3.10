include $(srctree)/drivers/misc/mediatek/Makefile.custom
LED_PLATFORM_DRIVER_DIR :=  $(srctree)/drivers/misc/mediatek/accdet/$(CONFIG_MTK_PLATFORM)
obj-$(CONFIG_MTK_ACCDET) := accdet_drv.o
obj-$(CONFIG_MTK_ACCDET) += $(subst ",,$(CONFIG_MTK_PLATFORM))/

ccflags-y  += -I$(LED_PLATFORM_DRIVER_DIR)/

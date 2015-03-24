include $(srctree)/drivers/misc/mediatek/Makefile.custom
obj-$(CONFIG_MTK_FB) := dsi_drv.o dpi_drv.o  lcd_drv.o dpi1_drv.o disp_drv_dbi.o disp_drv_dpi.o disp_drv_dsi.o disp_hal.o disp_intr.o
# tvc_drv.o tve_drv.o tvrot_drv.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/video \
             -I$(srctree)/drivers/misc/mediatek/video/$(CONFIG_MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/dispsys/$(CONFIG_MTK_PLATFORM) \
             -I$(srctree)/drivers/staging/android

ifeq ($(CONFIG_MTK_INTERNAL_HDMI_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/hdmitx/$(CONFIG_MTK_PLATFORM)
endif
ifeq ($(CONFIG_MTK_INTERNAL_MHL_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/mhltx/$(CONFIG_MTK_PLATFORM)
endif
ccflags-y += -Werror

ifeq ($(CONFIG_MTK_OVERLAY_ENGINE_SUPPORT), y)
ccflags-y += -DMTK_OVERLAY_ENGINE_SUPPORT
endif

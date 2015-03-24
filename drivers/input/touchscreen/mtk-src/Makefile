
include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video \
             -I$(srctree)/drivers/misc/mediatek/video/$(CONFIG_MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/dispsys/$(CONFIG_MTK_PLATFORM)

obj-y += tpd_init.o
obj-y += tpd_misc.o
obj-y += tpd_calibrate.o
obj-y += tpd_default.o
obj-y += tpd_button.o
obj-y += tpd_debug.o
obj-y += tpd_setting.o
obj-y += mtk_tpd.o

ifeq ($(CONFIG_MTK_INTERNAL_HDMI_SUPPORT), y)
include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/hdmitx/$(CONFIG_MTK_PLATFORM)

obj-y := hdmi_drv.o hdmi_ctrl.o hdmiavd.o hdmicec.o hdmiddc.o hdmiedid.o hdmihdcp.o hdmicmd.o hdmictrl.o

ifneq ($(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT),)
ccflags-y += -I$(srctree)/include/trustzone/
obj-y += hdmi_ca.o
endif

endif

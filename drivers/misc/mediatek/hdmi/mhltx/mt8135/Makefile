ifeq ($(CONFIG_MTK_INTERNAL_MHL_SUPPORT), y)
include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/mhltx/$(CONFIG_MTK_PLATFORM)

obj-y	:=mhl_drv.o
obj-y	+=mhl_avd.o
obj-y	+=mhl_ctrl.o
obj-y	+=mhl_cbus.o
obj-y	+=mhl_cbus_ctrl.o
obj-y	+=mhl_edid.o
obj-y	+=mhl_hdcp.o
obj-y	+=mhl_keycode.o

ifneq ($(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT),)
ccflags-y += -I$(srctree)/include/trustzone/
obj-y += hdmi_ca.o
endif

endif

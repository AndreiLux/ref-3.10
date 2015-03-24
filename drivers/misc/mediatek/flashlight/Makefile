include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y += flashlight.o

ccflags-y += -I$(src)/inc -I$(srctree)/include/linux/camera

ifeq ($(CONFIG_MTK_FLASHLIGHT_DUMMYFLASHLIGHT_SUPPORT),y)
ccflags-y += -DDUMMY_FLASHLIGHT
flashlight-y += dummy_flashlight.o
endif

flashlight-y += kd_flashlightlist.o

include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y += cam_cal.o

ccflags-y += -I$(src)/inc -I$(srctree)/include/linux/camera

ifeq ($(CONFIG_MTK_CAMCAL_DUMMYCAMCAL_SUPPORT),y)
cam_cal-y += dummy_cam_cal.o
endif

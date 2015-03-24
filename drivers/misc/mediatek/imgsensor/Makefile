include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y += imgsensor.o

ccflags-y += -I$(src)/inc -I$(srctree)/include/linux/camera

ifeq ($(CONFIG_MTK_IMGSENSOR_HI253YUV_SUPPORT),y)
ccflags-y += -DHI253_YUV
imgsensor-y += hi253_yuv/hi253yuv_Sensor.o
endif

ifeq ($(CONFIG_MTK_IMGSENSOR_HI704YUV_SUPPORT),y)
ccflags-y += -DHI704_YUV
imgsensor-y += hi704_yuv/hi704yuv_Sensor.o
endif

ifeq ($(CONFIG_MTK_IMGSENSOR_OV2722MIPIRAW_SUPPORT),y)
ccflags-y += -DOV2722_MIPI_RAW
imgsensor-y += ov2722_mipi_raw/ov2722mipiraw_Sensor.o
endif

ifeq ($(CONFIG_MTK_IMGSENSOR_OV2724MIPIRAW_SUPPORT),y)
ccflags-y += -DOV2724_MIPI_RAW
imgsensor-y += ov2724_mipi_raw/ov2724mipiraw_Sensor.o
endif

imgsensor-y += src/kd_sensorlist.o

video-odin-camera-objs-y :=	odin-css-system.o \
							odin-css-clk.o \
							odin-capture.o \
							odin-camera.o \
							odin-framegrabber.o \
							odin-css-platdrv.o \
							odin-css-utils.o \
							odin-isp.o \
							odin-facedetection.o \
							odin-facialpartsdetection.o \
							odin-mipicsi.o \
							odin-isp-wrap.o \
							odin-3dnr.o \
							odin-mdis.o \
							odin-bayercontrol.o 


video-odin-camera-objs-$(CONFIG_VIDEO_ODIN_S3DC) += \
							odin-s3dc.o

video-odin-camera-objs-$(CONFIG_VIDEO_ODIN_SYSFS) += \
							odin-css-sysfs.o

video-odin-camera-objs-$(CONFIG_VIDEO_ODIN_FLASH) += \
							odin-flash.o

video-odin-camera-objs-$(CONFIG_VIDEO_ODIN_SENSOR_A) += \
							extern/odin-sensor-default.o

video-odin-camera-objs-$(CONFIG_VIDEO_ODIN_SENSOR_IMX111) += \
							extern/odin-sensor-imx111.o

video-odin-camera-objs-$(CONFIG_VIDEO_ODIN_SENSOR_DUMMY) += \
							extern/odin-sensor-dummy.o

video-odin-camera-objs-$(CONFIG_VIDEO_ODIN_SENSOR_IMX091) += \
							extern/odin-sensor-imx091.o

video-odin-camera-objs-$(CONFIG_VIDEO_ODIN_SENSOR_IMX132) += \
							extern/odin-sensor-imx132.o

video-odin-camera-objs-$(CONFIG_VIDEO_ODIN_SENSOR_IMX135) += \
							extern/odin-sensor-imx135.o \
							extern/ois/odin-ois-i2c.o extern/ois/lgit_ois_old.o \
							extern/ois/ff_ois.o extern/ois/lgit_ois.o 

video-odin-camera-objs-$(CONFIG_VIDEO_ODIN_SENSOR_IMX135) += \
							extern/proxy/odin_proxy.o \
							extern/proxy/odin_proxy_i2c.o

video-odin-camera-objs-$(CONFIG_VIDEO_ODIN_SENSOR_IMX208) += \
							extern/odin-sensor-imx208.o

video-odin-camera-objs := $(video-odin-camera-objs-y)

obj-$(CONFIG_VIDEO_ODIN) += video-odin-camera.o

obj-$(CONFIG_VIDEO_ODIN_JPEG_ENCODER) += jpegenc/

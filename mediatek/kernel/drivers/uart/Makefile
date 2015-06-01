obj-$(CONFIG_MTK_SERIAL) := uart.o
obj-$(CONFIG_FIQ_DEBUGGER) += mt_fiq_debugger.o
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/uart/

ifeq ($(MTK_HDMI_SUPPORT),yes)
ccflags-y += 	-DMTK_HDMI_SUPPORT

ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/video/   \
             -I$(MTK_PATH_SOURCE)/drivers/video/     \
             -I$(MTK_PATH_PLATFORM)/drivers/ext_disp/     \
             -I$(MTK_PATH_PLATFORM)/drivers/dispsys/ \
             -I$(MTK_PATH_PLATFORM)/drivers/cmdq/ \

endif

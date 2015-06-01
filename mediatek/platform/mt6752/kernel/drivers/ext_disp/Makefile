#
# Makefile for external display driver.
#

ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/externaldisplay/  \
             -I$(MTK_PATH_SOURCE)/drivers/video/     \
             -I$(MTK_PATH_PLATFORM)/drivers/dispsys/ \
             -I$(MTK_PATH_PLATFORM)/drivers/videox/ \
             -I$(MTK_PATH_SOURCE)/drivers/sync/ \
             -I$(MTK_PATH_SOURCE)/drivers/mhl/driver/mhl_tx_drv/ \
             -I$(MTK_PATH_PLATFORM)/drivers/cmdq/ \
             -I$(srctree)/drivers/staging/android/ion


ifeq ($(CUSTOM_KERNEL_HDMI),Sii8338)
	ccflags-y += 	-DMHL_SII8338
endif

ifeq ($(CUSTOM_KERNEL_HDMI),Sii8348)
	ccflags-y += 	-DMHL_SII8348
endif

obj-y :=  \
    mtk_ext_disp_mgr.o   \
    extd_lcm.o \
    extd_fence.o \
    external_display.o \
    extd_debug.o \
    extd_utils.o \
    Kara_1280x720.o \
    extd_hdmi_drv.o \

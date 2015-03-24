# drivers/video/mtk/Makefile
#
#

include $(srctree)/drivers/misc/mediatek/Makefile.custom
obj-$(CONFIG_MTK_FB) += disp_drv.o                                 \
         mtkfb_console.o                            \
         disp_assert_layer.o                        \
         mtkfb.o                                    \
		 mtkfb_vsync.o								\
         debug.o                                    \
         mtkfb_proc.o
obj-$(CONFIG_MTK_FB) += $(subst ",,$(CONFIG_MTK_PLATFORM))/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/video \
             -I$(srctree)/drivers/misc/mediatek/video/$(CONFIG_MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/dispsys \
             -I$(srctree)/drivers/staging/android \
             -I$(srctree)/drivers/misc/mediatek/dispsys/$(CONFIG_MTK_PLATFORM)

ifeq ($(CONFIG_MTK_OVERLAY_ENGINE_SUPPORT), y)
obj-y += \
    disp_ovl_engine_api.o \
    disp_ovl_engine_core.o \
    disp_ovl_engine_dev.o \
    disp_ovl_engine_sw.o \
    disp_ovl_engine_hw.o

ccflags-y += -DMTK_OVERLAY_ENGINE_SUPPORT



ifeq ($(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)),y)
ccflags-y += -I$(srctree)/include -I$(srctree)/include/trustzone
ccflags-y += -DMTK_SEC_VIDEO_PATH_SUPPORT
endif

endif

ifeq ($(CONFIG_MTK_DITHERING_SUPPORT), y)
ccflags-y += -DDITHERING_SUPPORT
endif

ifeq ($(CONFIG_MTK_GPU_SUPPORT), y)
ccflags-y += -DHWGPU_SUPPORT
endif

# drivers/dispsys

include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video \
             -I$(srctree)/drivers/misc/mediatek/video/$(CONFIG_MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/dispsys/$(CONFIG_MTK_PLATFORM)

obj-y += ddp_drv.o
obj-y += ddp_ovl.o
obj-y += ddp_bls.o
obj-y += ddp_color.o
obj-y += ddp_tdshp.o
obj-y += ddp_rdma.o
obj-y += ddp_wdma.o
obj-y += ddp_scl.o
obj-y += ddp_rot.o
obj-y += ddp_path.o
obj-y += ddp_debug.o
obj-y += ddp_dpfd.o
obj-y += ddp_aal.o
obj-y += data_rgb888_64x64.o
obj-y += data_rgb888_64x64_golden.o

ifeq ($(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)),y)
ccflags-y += -I$(srctree)/include -I$(srctree)/include/trustzone
ccflags-y += -DMTK_SEC_VIDEO_PATH_SUPPORT
endif

ifeq ($(CONFIG_MTK_OVERLAY_ENGINE_SUPPORT), y)
ccflags-y += -DMTK_OVERLAY_ENGINE_SUPPORT
endif

# EOF

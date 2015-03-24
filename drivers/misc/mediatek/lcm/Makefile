
include $(srctree)/drivers/misc/mediatek/Makefile.custom

CONFIG_CUSTOM_KERNEL_LCM := $(shell echo $(patsubst "%",%,$(CONFIG_CUSTOM_KERNEL_LCM)) | tr a-z A-Z)

CUSTOM_KERNEL_FLAGS := $(foreach var,$(CONFIG_CUSTOM_KERNEL_LCM), -D$(var))
ccflags-y += $(CUSTOM_KERNEL_FLAGS)

obj-y					+= mt65xx_lcm_list.o
obj-$(CONFIG_CM_N070ICE_DSI_VDO)	+= cm_n070ice_dsi_vdo.o
obj-$(CONFIG_CM_N070ICE_DSI_VDO_MT8135)	+= cm_n070ice_dsi_vdo_mt8135.o
obj-$(CONFIG_NT35521_WXGA_DSI_VDO_HH060IA)	+= nt35521_wxga_dsi_vdo_hh060ia.o
obj-$(CONFIG_NT51012_WXGA_DSI_VDO) += nt51012_wxga_dsi_vdo.o
obj-$(CONFIG_HSD070PFW3_8135)		+= HSD070PFW3_8135.o
obj-$(CONFIG_CPT_CLAA101FP01_DSI_VDO)		+= CPT_CLAA101FP01_DSI_VDO.o

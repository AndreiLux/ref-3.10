ifeq ($(CONFIG_OF),y)

ifeq ($(CONFIG_MACH_MSM8994_S3),y)
DTS_TARGET := s3
endif

ifeq ($(CONFIG_MACH_MSM8994_Z2_SPR_US),y)
DTS_TARGET := z2_spr_us
endif

ifeq ($(CONFIG_MACH_MSM8994_Z2_KR),y)
DTS_TARGET := z2_kr
endif

ifeq ($(CONFIG_MACH_MSM8994_G4_KR),y)
DTS_TARGET := g4_kr
endif

ifeq ($(CONFIG_MACH_MSM8994_G4_ATT_US),y)
DTS_TARGET := g4_att_us
endif

ifeq ($(CONFIG_MACH_MSM8994_G4_USC_US),y)
DTS_TARGET := g4_usc_us
endif

ifeq ($(CONFIG_MACH_MSM8994_G4_SPR_US),y)
DTS_TARGET := g4_spr_us
endif

ifeq ($(CONFIG_MACH_MSM8994_G4_TMO_US),y)
DTS_TARGET := g4_tmo_us
endif

ifeq ($(CONFIG_MACH_MSM8994_Z2_GLOBAL_COM),y)
DTS_TARGET := z2_global_com
endif

ifeq ($(CONFIG_MACH_MSM8994_Z2_GLOBAL_ESA),y)
DTS_TARGET := z2_global_esa
endif

ifeq ($(CONFIG_MACH_MSM8994_G4_GLOBAL_COM),y)
DTS_TARGET := g4_global_com
endif

ifeq ($(CONFIG_MACH_MSM8994_Z2_USC_US),y)
DTS_TARGET := z2_usc_us
endif

ifeq ($(CONFIG_MACH_MSM8994_Z2_ATT_US),y)
DTS_TARGET := z2_att_us
endif

ifeq ($(CONFIG_MACH_MSM8994_G4_VZW),y)
DTS_TARGET := g4_vzw
endif


ifeq ($(CONFIG_MACH_MSM8994_G4_CN),y)
DTS_TARGET := g4_cn
endif

ifeq ($(CONFIG_MACH_MSM8994_G4_KDDI_JP),y)
DTS_TARGET := g4_kddi_jp
endif

DTS_TARGET_DIR := msm8994-$(DTS_TARGET)

dtb-y += msm8994-v1-$(DTS_TARGET).dtb \
	msm8994-v2-$(DTS_TARGET).dtb

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif

targets += dtbs
targets += $(addprefix ../, $(DTB_LIST))
endif

$(obj)/../%.dtb: $(src)/$(DTS_TARGET_DIR)/%.dts FORCE
	$(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../,$(DTB_LIST))

clean-files := *.dtb

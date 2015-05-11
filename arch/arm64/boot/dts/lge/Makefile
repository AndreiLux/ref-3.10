ifeq ($(CONFIG_OF),y)

ifeq ($(CONFIG_MACH_MSM8992_P1),y)
DTS_TARGET := msm8992-p1_hdk
endif

ifeq ($(CONFIG_MACH_MSM8992_P1_KR),y)
DTS_TARGET := msm8992-p1_kr
endif

ifeq ($(CONFIG_MACH_MSM8992_P1_ATT_US),y)
DTS_TARGET := msm8992-p1_att_us
endif

ifeq ($(CONFIG_MACH_MSM8992_P1_SPR_US),y)
DTS_TARGET := msm8992-p1_spr_us
endif

ifeq ($(CONFIG_MACH_MSM8992_P1_GLOBAL_COM),y)
DTS_TARGET := msm8992-p1_global_com
endif

ifeq ($(CONFIG_MACH_MSM8992_P1_VZW),y)
DTS_TARGET := msm8992-p1_vzw
endif

ifeq ($(CONFIG_MACH_MSM8992_P1_USC_US),y)
DTS_TARGET := msm8992-p1_usc_us
endif

ifeq ($(CONFIG_MACH_MSM8992_P1_TMO_US),y)
DTS_TARGET := msm8992-p1_tmo_us
endif

ifeq ($(CONFIG_MACH_MSM8992_P1_CN),y)
DTS_TARGET := msm8992-p1_cn
endif

ifeq ($(CONFIG_MACH_MSM8992_P1_KDDI_JP),y)
DTS_TARGET := msm8992-p1_kddi_jp
endif

ifeq ($(CONFIG_MACH_MSM8992_P1_CA),y)
DTS_TARGET := msm8992-p1_ca
endif

ifeq ($(CONFIG_MACH_MSM8992_P1_ACG_US),y)
DTS_TARGET := msm8992-p1_acg_us
endif

ALL_DTS := $(notdir $(wildcard echo $(srctree)/$(src)/$(DTS_TARGET)/*.dts))
dtb-y += $(patsubst %.dts,%.dtb,$(ALL_DTS))

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif

targets += dtbs
targets += $(addprefix ../, $(DTB_LIST))
endif

$(obj)/../%.dtb: $(src)/$(DTS_TARGET)/%.dts FORCE
	$(call if_changed_dep,dtc)
dtbs: $(addprefix $(obj)/../,$(DTB_LIST))

clean-files := *.dtb

ifeq ($(CONFIG_MTK_WLAN_SUPPORT), y)

# Enable warning to error in all MTK subdir.
subdir-ccflags-y += -Werror

SUPPORT_MT6620 := no
SUPPORT_MT6628 := no

$(warning check_mt6620)
SUPPORT_MT6620 := $(if $(filter $(CONFIG_MTK_WLAN_CHIP), "MT6620"),yes,no)
$(warning check_mt6620_end)

$(warning check_mt6628)
SUPPORT_MT6628 := $(if $(filter $(CONFIG_MTK_WLAN_CHIP), "MT6628"),yes,no)
$(warning check_mt6628_end)

$(warning $(SUPPORT_MT6628));

ifeq ($(SUPPORT_MT6620), yes)
    obj-$(CONFIG_MTK_COMBO_WIFI) += mt6620/
    $(warning include mt6620)
endif

ifeq ($(SUPPORT_MT6628), yes)
    obj-$(CONFIG_MTK_COMBO_WIFI) += mt6628/
    $(warning include mt6628)
endif


endif

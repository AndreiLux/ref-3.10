# only support 6628 currently, do not support on-binary
ifneq ($(filter "MT6628",$(CONFIG_MTK_COMBO_CHIP)),)
# ---------------------------------------------------
# Compile Options
# ---------------------------------------------------
	ccflags-y += -DLINUX -DMT6628 -DDBG=1
	obj-$(CONFIG_MTK_COMBO_WIFI) += mtk_wifi_loopback.o
# ---------------------------------------------------
# Directory List
# ---------------------------------------------------
	mtk_wifi_loopback-objs  := fw_download.o mt6628-core.o
endif

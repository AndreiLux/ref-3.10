include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(srctree)
obj-y += dbg.o
obj-y += sd.o mt_sd_misc.o

obj-$(CONFIG_MTK_AEE_IPANIC) += mt_dump.o
obj-$(CONFIG_MTK_EMMC_SUPPORT_OTP) += emmc_otp.o

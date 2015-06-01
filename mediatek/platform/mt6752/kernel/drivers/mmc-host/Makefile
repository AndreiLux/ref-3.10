#add kernel source code path as head file seach path
ccflags-y += -I$(srctree)

obj-$(CONFIG_MTK_MMC)		+= msdc.o

msdc-y += dbg.o
msdc-y += sd.o mt_sd_misc.o
msdc-y += sdio_autok.o sdio_autok_proc.o ettagent.o

# kdump/kpainc interface move to lk 
ifeq ($(CONFIG_MTK_AEE_IPANIC),y)
msdc-y += mt_dump.o
endif

ifeq ($(MTK_EMMC_SUPPORT_OTP),yes)
msdc-y += emmc_otp.o
endif

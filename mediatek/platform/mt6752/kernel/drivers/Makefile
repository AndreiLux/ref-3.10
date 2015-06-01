include $(MTK_PATH_BUILD)/common.mk
#Fix-me: modified for early porting
#$(call all-subdir-src-or-makefile)
obj-y   += keypad/
obj-y   += uart/
obj-y   += gpio/
obj-y   += wdt/
obj-y   += irq/
obj-$(CONFIG_MTK_CIRQ) += cirq/
#obj-y   += cqdma/
obj-y   += boot/
obj-y   += chip/
obj-y   += spm/
obj-$(CONFIG_MD32_SUPPORT)   += md32/
obj-$(CONFIG_MTK_SYSTRACKER)   += systracker/
obj-y   += power/
obj-y   += mmc-host/
ifeq ($(MTK_HDMI_SUPPORT),yes)
obj-y   += ext_disp/
endif
ifeq ($(MTK_NFC_SUPPORT),yes)
obj-y	+= nfc/
endif

obj-y   += i2c/
obj-y   += pmic_wrap/
obj-y   += usb20/
obj-y   += usb11/
#obj-y   += mu3phy/
obj-y   += ./../../../common/kernel/drivers/cmdq/
obj-y   += cmdq/
obj-y   += m4u/
obj-y   += dispsys/
obj-y   += videox/
obj-y   += jpeg/
obj-$(CONFIG_MTK_VIDEOCODEC_DRIVER) += videocodec/
#ifneq ($(CONFIG_MTK_FPGA),y)
obj-y   += pwm/
#obj-y   += dual_ccci/
obj-y   += eccci/
obj-y   += btcvsd/
#obj-y   += power_gs/
obj-y   += smi/
#obj-y   += mmc-host/
obj-y   += rtc/
obj-y   += mjc/
#obj-y   += sound/
obj-y   += devinfo/
obj-y   += devapc/
obj-y   += auxadc/
#obj-y   += ldvt/
obj-y   += masp/
obj-y   += accdet/
obj-y   += vibrator/
obj-y   += leds/
#obj-y   += smartpa/
obj-y   += thermal/
include $(MTK_PATH_BUILD)/common.mk
obj-y += $(call all-subdirs-with-makefile,cpu_stress)
#ifeq ($(HAVE_CMMB_FEATURE),yes)
#obj-m += cmmbdrv/
#endif
ifneq	($(MTK_GPU_SUPPORT),no)
obj-y   += gpu/
endif
ifeq ($(MTK_GPT_SCHEME_SUPPORT), yes)
obj-y   += partition/
endif
#obj-y   += nand/
#obj-m   += mdrvb/
#obj-y   += touchpanel/
obj-y   += spi/
#obj-y   += ccci/
#obj-y   += mmc-host/
#obj-y   += rtc/
obj-y   += eint/
obj-y   += l2c_share/
obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += gud/
obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += secmem/

obj-y	+= $(call all-subdirs-with-makefile,met)

obj-$(CONFIG_MTK_BTIF) += btif/

ifneq ($(CONFIG_MTK_COMBO),)
ifneq ($(filter CONSYS_6752,$(MTK_COMBO_CHIP)),)
obj-y   += wmt/
endif
endif

obj-y   += dramc/
obj-y   += lastpc/
ifeq ($(MTK_VOW_SUPPORT),yes)
obj-y	+= vow/
endif

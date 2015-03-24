
# Enable warning to error in all MTK subdir.
subdir-ccflags-y += -Werror

MTK_FM_RX_AUDIO_FLAG := $(addprefix -D, $(shell echo $(CONFIG_MTK_FM_RX_AUDIO) | tr a-z A-Z))
subdir-ccflags-y += $(MTK_FM_RX_AUDIO_FLAG)

obj-y   +=  mtprof/
obj-y   +=  sched/
obj-y   +=  mmp/
obj-y   +=  xlog/
obj-y   +=  uart/
obj-y   +=  eint/
obj-y   +=  boot/
obj-$(CONFIG_ZRAM) +=  mlog/
obj-$(CONFIG_MTK_LEGACY_GPIO_DRIVER)	+=	gpio/
obj-y   +=  aee/
obj-y   +=  wdk/
obj-y   +=  m4u/
obj-y   +=  video/
obj-y   +=  lcm/
obj-y   +=  pmic_wrap/
ifneq ($(MTK_BATTERY_NO_HAL),yes)
obj-y   += power/
endif


obj-y   +=  eint/
obj-$(CONFIG_MTK_BTIF)   +=  btif/


ifneq ($(CONFIG_MTK_FPGA),y)
obj-$(CONFIG_MTK_RTC)   +=  rtc/
obj-y   +=  ccci/
obj-y   +=  dual_ccci/

ifeq ($(CONFIG_MTK_EEMCS_DEVICES), y)
obj-y   +=  eemcs/
endif

obj-y   +=  fmradio/
obj-y   +=  gps/
obj-y   +=  hwmon/
obj-y   +=  mhl/
obj-y   +=  almk/


obj-y   +=  ccmni/

obj-y   +=  emd_ctl/

# gator-driver is not custom-release and will be removed
obj-y	+= $(call all-subdirs-with-makefile,gator-driver)

obj-y   +=  hdmi/
obj-y   +=  leds/
obj-y   +=  auxadc/
obj-y   +=  mmp/
obj-y		+=  ram_console/
obj-y   +=  videocodec/
obj-$(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)  +=  trustzone/

ifeq ($(CONFIG_MTK_COMBO),m)

	BUILD_COMBO := no
	BUILD_SOC := no

	ifneq ($(filter "MT6620E3",$(CONFIG_MTK_COMBO_CHIP)),)
		BUILD_COMBO := yes
	endif

	ifneq ($(filter "MT6628",$(CONFIG_MTK_COMBO_CHIP)),)
		BUILD_COMBO := yes
	endif

	ifneq ($(filter "MT6630",$(CONFIG_MTK_COMBO_CHIP)),)
		BUILD_COMBO := yes
	endif

	ifeq ($(BUILD_COMBO), yes)
		obj-y   +=  combo/
		subdir-ccflags-y += -DMTK_WCN_COMBO_CHIP_SUPPORT
	endif

	ifneq ($(filter "CONSYS_6572",$(CONFIG_MTK_COMBO_CHIP)),)
		BUILD_SOC := yes
	endif

	ifneq ($(filter "CONSYS_6582",$(CONFIG_MTK_COMBO_CHIP)),)
		BUILD_SOC := yes
	endif

	ifneq ($(filter "CONSYS_6592",$(CONFIG_MTK_COMBO_CHIP)),)
		BUILD_SOC := yes
	endif

	ifeq ($(BUILD_SOC), yes)
		obj-y   +=  conn_soc/
	endif

	obj-y   +=  common_detect/
	subdir-ccflags-y += -DMTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT

endif

obj-$(CONFIG_MTK_NFC_SUPPORT)	+= nfc/

obj-y   +=  accdet/
obj-y   +=  vibrator/
obj-y	+=	pwm/
obj-y   +=  freqhopping/
obj-y   += sync/
obj-$(CONFIG_MTK_ETM) += etm/
obj-y   +=      dbg_dump/
endif#!CONFIG_MTK_FPGA

obj-y   +=  kernel/

ifneq ($(CONFIG_MTK_PLATFORM),)
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ifeq ($(CONFIG_HAVE_CMMB_FEATURE),y)
obj-y   += cmmbdrv/$(MTK_PLATFORM)/
endif
obj-y   += devapc/$(MTK_PLATFORM)/
obj-y   += devinfo/$(MTK_PLATFORM)/
obj-m   += dumpEfuseInfo/$(MTK_PLATFORM)/
obj-y   += dispsys/$(MTK_PLATFORM)/
obj-y   += gcpu/$(MTK_PLATFORM)/
obj-y   += gdma/$(MTK_PLATFORM)/
obj-y   += jpeg/$(MTK_PLATFORM)/
obj-$(CONFIG_MTK_LDVT) += ldvt/$(MTK_PLATFORM)/
#obj-y   += masp/$(MTK_PLATFORM)/
obj-$(CONFIG_MTK_MMC_LEGACY) += mmc-host/$(MTK_PLATFORM)/
obj-y   += power/$(MTK_PLATFORM)/
obj-y   += power-loss-test/$(MTK_PLATFORM)/
obj-y   += smi/$(MTK_PLATFORM)/
obj-y   += spi/$(MTK_PLATFORM)/
obj-y   += touchpanel/$(MTK_PLATFORM)/
obj-y   += wdt/$(MTK_PLATFORM)/
obj-y   += camera/$(MTK_PLATFORM)/
endif

obj-y   += thermal/
obj-y   += pa/
obj-y   += imgsensor/
obj-y   += eeprom/
obj-y   += cam_cal/
obj-y   += flashlight/
obj-y   += lens/

obj-y   += selinux_warning/
obj-y   += dynamic_boost/

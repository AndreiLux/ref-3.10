obj-$(CONFIG_MACH_ODIN)			:= board-odin.o odin_irq.o
obj-$(CONFIG_MACH_ODIN_FPGA)		+= board-odin-fpga.o
obj-$(CONFIG_SMP)			+= headsmp.o platsmp.o
obj-$(CONFIG_HOTPLUG_CPU)		+= hotplug.o
obj-$(CONFIG_PM_SLEEP)			+= pm.o
obj-$(CONFIG_ODIN_BL_POWEROPS)		+= odin_bl_powerops.o odin_bl_setup.o
obj-$(CONFIG_ODIN_PD_OPS)		+= odin_pd_ops.o
obj-$(CONFIG_ODIN_LPA_OPS)		+= odin_lpa_ops.o
obj-$(CONFIG_ODIN_PMIC_OPS)		+= odin_pmic_ops.o
obj-$(CONFIG_ODIN_TEE)			+= odin_tz.o
obj-$(CONFIG_ODIN_SECURE_SMMU)		+= odin_sec_smmu.o
obj-$(CONFIG_PM_GENERIC_DOMAINS)	+= odin_pm_domain.o
obj-$(CONFIG_IMC_DRV)                   += platform_modem_ctrl.o
# LGE_CHANGE_S
obj-$(CONFIG_LGE_CRASH_HANDLER)		+= lge_crash_handler.o
# LGE_CHANGE_E
obj-$(CONFIG_LGE_BOOT_TIME_CHECK)	+= lge_boot_time_checker.o
ifneq ($(CONFIG_BCMDHD),)
obj-$(CONFIG_WIFI_CONTROL_FUNC) += wifi_bcm_lge.o
endif
obj-$(CONFIG_ODIN_PSTORE)		+= odin_pstore.o
obj-$(CONFIG_LGE_AUTO_SUSPEND_TEST)	+= lge_suspend_autotest.o
obj-$(CONFIG_IMC_DRV)                   += platform_modem_ctrl.o
obj-$(CONFIG_ODIN_SYS_FS)		+= odin_sysfs.o
obj-$(CONFIG_USB_ODIN_DRD)		+= odin_usb3.o
obj-$(CONFIG_PRE_SELF_DIAGNOSIS)        += lge_pre_selfd.o

obj-$(CONFIG_MACH_LGE) += devices_lge.o
obj-$(CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO) += lge_charging_scenario.o
obj-$(CONFIG_LGE_HANDLE_PANIC) += lge_handle_panic.o
obj-$(CONFIG_LGE_EARJACK_DEBUGGER) += lge_earjack_debugger.o

obj-$(CONFIG_LGE_BOOTLOADER_LOG) += lge_bootloader_log.o
obj-$(CONFIG_LGE_BOOTLOADER_TIME_CHECKER) += lge_bootloader_time_checker.o

obj-$(CONFIG_WIFI_CONTROL_FUNC) += wifi_bcm_lge.o
obj-$(CONFIG_PRE_SELF_DIAGNOSIS) += lge_pre_selfd.o
obj-$(CONFIG_MACH_LGE) += lge_lcd_misc_ctrl.o

obj-$(CONFIG_LGE_QFPROM_INTERFACE) += lge_qfprom_access.o

obj-$(CONFIG_LGE_QSDL_SUPPORT) += lge_qsdl.o

CFLAGS_lge_bootloader_log.o += -I$(srctree)/arch/arm/mach-msm/include

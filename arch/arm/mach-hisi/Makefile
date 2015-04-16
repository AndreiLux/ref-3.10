#
# Makefile for Hisilicon HI3630FPGA line
#

obj-$(CONFIG_ARCH_HI3630FPGA)	+= hisi_3630fpga.o
obj-$(CONFIG_ARCH_HI3630EMULATOR)	+= hisi_3630emulator.o
obj-$(CONFIG_ARCH_HI3630)	+= hisi_3630.o
obj-$(CONFIG_EASY_SHELL)       += easy_shell.o
obj-$(CONFIG_HISI_NOC)		+= hisi_noc/
obj-$(CONFIG_HI6421_OCP)	+= hisi_hi6421_ocp.o
obj-$(CONFIG_HISI_MAILBOX)	+= hisi_mailbox.o
obj-$(CONFIG_USB_SUPPORT)	+= usb_phy.o
obj-$(CONFIG_HISI_LPM3)		+= hisi_lowpowerm3.o
obj-$(CONFIG_HISI_AXI)		+= hisi_axi.o
obj-$(CONFIG_HISI_UDP_BOARDID)		+= hisi_udp_board_id.o
obj-$(CONFIG_HISI_IRQ_AFFINITY)	+= hisi_irq_affinity.o

ifeq ($(CONFIG_SECURE_EXTENSION), y)
obj-$(CONFIG_ARM_PSCI)          += hisi_pm_psci.o
else
obj-$(CONFIG_HI3630_LPM3_IPC)	+= hisi_ipc.o
obj-$(CONFIG_SMP)		+= hisi_pm.o hisi_pm_setup.o
obj-$(CONFIG_HISI_EFUSE)	+= hisi_efuse.o
endif
ifeq ($(CONFIG_TZDRIVER), y)
obj-$(CONFIG_HISI_EFUSE)	+= hisi_efuse.o
EXTRA_CFLAGS += -Idrivers/tzdriver
endif
# obj-$(CONFIG_PM)    += poweroff.o
obj-$(CONFIG_PM_AUTOSLEEP) += pm.o
obj-$(CONFIG_PM_AUTOSLEEP) += hisi_sync_th.o
obj-$(CONFIG_HW_CMDLINE_PARSE)	+= hw_cmdline_parse.o
obj-$(CONFIG_HISI_SR_DEBUG) += hisi_lpregs.o
obj-$(CONFIG_HISI_EXTERN_WDT) += hisi_extern_wdt.o
obj-$(CONFIG_HISI_LPM3_VIEWER)	+= hisi_lpm3_viewer.o

obj-$(CONFIG_HISI_BUS_SWITCH)		+= hisi_switcher.o
obj-$(CONFIG_HISI_SIM_HOTPLUG)		+=hisi_sim_hotplug.o
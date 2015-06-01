
# Makefile for the linux kernel.
#
# Only aarch32 need to build core/ (need machine descriptor)
ifneq ($(CONFIG_ARM64), yes)
## Fix-me: marked others for early porting
#ifeq ($(MTK_INTERNAL), yes)
#ifeq ($(MTK_USE_RESERVED_EXT_MEM), yes)
#KBUILD_CFLAGS += -DMTK_USE_RESERVED_EXT_MEM
#endif
#endif
#
obj-y := core.o
obj-y += emi_mpu.o emi_bwl.o
#obj-y += irq.o
#obj-y += timer.o
obj-y += mt_devs.o
#obj-y += mt_boot.o
#obj-y += board.o
#obj-y += ca7_timer.o
obj-y += mt_cpuxgpt.o
obj-y += mt_gpt.o
obj-y += ca53_timer.o
##obj-y += mt_dma.o
obj-y += mt_device_apc.o
obj-y += mt_pm_init.o
#ifeq ($(CONFIG_SMP),y)
obj-y += mt-smp.o
obj-y += hotplug.o
obj-y += mt-headsmp.o
obj-y += mt_hotplug.o
#obj-y += mt_hotplug_mechanism.o
obj-y += mt_hotplug_strategy_main.o mt_hotplug_strategy_core.o mt_hotplug_strategy_procfs.o mt_hotplug_strategy_cpu.o mt_hotplug_strategy_api.o mt_hotplug_strategy_algo.o
#endif
## TO-FIX add # for do early porting in JB migration
#obj-y += eint.o
obj-y += mt_freqhopping.o
#obj-$(CONFIG_MT65XX_TRACER) += pmu_v7.o mon_interface.o mt_mon.o mt_emi_bm.o
#obj-y += emi_bwl.o
#obj-y += emi_mpu.o
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/cmdq
obj-y += camera_isp.o
obj-y += camera_sysram.o
##obj-y += camera_eis.o
obj-y += camera_fdvt.o
obj-y += mt_pm_ldo.o
#obj-y += innercache.o
#obj-y += mt_l2c.o
#obj-$(CONFIG_MTK_CIRQ) += mt_cirq.o
#ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/power \
#             -I$(MTK_PATH_PLATFORM)/drivers/gpu/mali/mali/include \
#             -I$(MTK_PATH_SOURCE)/drivers/wdk
obj-y += mt_spm_mtcmos.o
obj-y += mt_sleep.o
obj-y += mt_cpufreq.o mt_cpu_ss.o
obj-y += mt_gpufreq.o
obj-y += mt_proc_cpu.o
##obj-y += mt_emifreq.o
#
MT_CPUIDLE:=y
ifneq ($(MT_CPUIDLE),y)
obj-y += mt_dbg.o
obj-y += mt_dormant.o
else
obj-y += mt_cpuidle.o
KBUILD_CFLAGS += -DMT_CPUIDLE
KBUILD_AFLAGS += -DMT_CPUIDLE
endif
#
obj-y += cpu_dormant.o
obj-y += mt_idle.o
obj-y += mt_dcm.o
obj-y += mt_clkmgr_common.o
obj-y += mt_clkmgr.o
obj-y += mt_clkbuf_ctl.o
obj-y += mt_golden_setting.o
#obj-y += sbchk_base.o
##obj-y += mtk_ccci_helper.o
#
#ifneq ($(CONFIG_MTK_CCCI_DEVICES),y)
#ccflags-y  += -D__USING_DUMMY_CCCI_API__
#endif
#
#obj-y += mt_static_power.o
#

ifneq ($(MTK_TABLET_HARDWARE), )
ccflags-y += -DMTK_TABLET_PLATFORM=\"$(shell echo $(MTK_TABLET_HARDWARE) | tr a-z A-Z )\"
endif

obj-y += mt_ptp.o
obj-y += mt_ptp2.o
##obj-y += mssv.o
#
## Hibernation
#AFLAGS_swsusp.o	:= -DTEXT_OFFSET=$(TEXT_OFFSET)
obj-$(CONFIG_HIBERNATION)	+= cpu_hibernate.o swsusp.o
#
#KBUILD_AFLAGS	:=$(KBUILD_AFLAGS:-msoft-float=-Wa,-mfpu=neon)
#
##KBUILD_CFLAGS += -DMT_DORMANT_UT
##KBUILD_CFLAGS += -DMT_SMP_VIRTUAL_BOOT_ADDR
endif

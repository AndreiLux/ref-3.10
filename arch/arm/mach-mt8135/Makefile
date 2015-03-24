#
# Makefile for the linux kernel.
#

obj-y := core.o irq.o timer.o mt_dbg.o
obj-y += mt_devs.o mt_gpt.o mt_boot.o board.o mt_pm_init.o
obj-y += ca7_timer.o

ifeq ($(CONFIG_SMP),y)
ccflags-y += -I$(srctree)
obj-y += mt-smp.o hotplug.o mt-headsmp.o mt_hotplug.o mt_hotplug_mechanism.o
endif

obj-y += mt_platform_gpio.o
obj-y += eint.o mt_freqhopping.o
obj-$(CONFIG_MT65XX_TRACER) += pmu_v7.o mon_interface.o mt_mon.o mt_emi_bm.o
obj-y += emi_bwl.o emi_mpu.o
#obj-y += camera_isp.o
#obj-y += camera_sysram.o
#obj-y += camera_pipe_mgr.o
#obj-y += camera_eis.o
#obj-y += camera_fdvt.o
obj-y += innercache.o mt_cache_dump.o
obj-y += mt_dma.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/power/$(CONFIG_MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/smi/$(CONFIG_MTK_PLATFORM) \
             -I$(srctree)/drivers/staging/android \
             -I$(srctree)
ccflags-$(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) += -I$(srctree)/include -I$(srctree)/include/trustzone
obj-y += mt_sleep.o
obj-y += mt_cpu_ss.o
obj-y += mt_dormant.o cpu_dormant.o
obj-y += mt_dcm.o
obj-y += board-common-audio.o

# Customize boards
BOARD_COMMON := board-common.o board-common-battery.o board-common-tmp103.o mt_clk.o

obj-$(CONFIG_MT8135_EVB_GENERIC) += $(BOARD_COMMON) board-evb.o board-evb-battery.o
obj-$(CONFIG_MT8135_EVBP1_V2_WSVGA) += $(BOARD_COMMON) board-evbp1_v2_wsvga.o board-evb-battery.o
obj-$(CONFIG_MT8135_EVBP1_V2_WUXGA_CPT) += $(BOARD_COMMON) board-evbp1_v2_wuxga_cpt.o board-evb-battery.o
obj-$(CONFIG_MT8135_STINGRAY)	 += $(BOARD_COMMON) board-stingray.o board-stingray-battery.o mt_cam.o
obj-$(CONFIG_MT8135_ARIEL)	 += $(BOARD_COMMON) board-ariel.o board-ariel-battery.o mt_cam.o
obj-$(CONFIG_MT8135_ASTON)	 += $(BOARD_COMMON)board-aston.o board-aston-battery.o mt_cam.o

# FPGA/LDVT codes
ifneq ($(MTK_PROJECT), mt8135fpga_ldvt)
ifneq ($(MTK_PROJECT), mt8135_fpga)
#obj-y += mssv.o
#obj-y += mt_auxadc.o
obj-y += mt_clkmgr_common.o
obj-y += mt_clkmgr.o
obj-y += mt_cpufreq.o
#obj-y += mt_emifreq.o
obj-y += mt_golden_setting.o
obj-y += mt_gpufreq.o
obj-y += mt_idle.o
obj-y += mt_pm_ldo.o
#obj-y += mt_spm.o mt_spm_sleep.o mt_spm_idle.o mt_spm_mtcmos.o
obj-y += mt_spm.o mt_spm_sleep.o mt_spm_mtcmos.o
obj-y += rq_stats.o

endif
endif

obj-y += sbchk_base.o
obj-$(CONFIG_MTK_CIRQ) += mt_cirq.o
obj-$(CONFIG_MTK_MCI) += mt_mci.o
obj-$(CONFIG_MTK_CCCI_DEVICES) += mtk_ccci_helper.o
obj-$(CONFIG_MTK_ETM) += pftracer.o

ifneq ($(CONFIG_MTK_CCCI_DEVICES),y)
ccflags-y  += -D__USING_DUMMY_CCCI_API__
endif

# Hibernation
AFLAGS_swsusp.o	:= -DTEXT_OFFSET=$(TEXT_OFFSET)
obj-$(CONFIG_HIBERNATION)	+= cpu_hibernate.o swsusp.o

obj-y += mt_ptp.o
obj-y += mt_cpu_stats.o
# For cpuidle
obj-y += mt_cpuidle_stats.o

# For cluster
obj-y += mt_cluster_stats.o

# For serial
obj-$(CONFIG_MTK_SERIAL) +=  board-common-serial.o

KBUILD_AFLAGS	:=$(KBUILD_AFLAGS:-msoft-float=-Wa,-mfpu=neon)

ccflags-y  += -Werror

include $(srctree)/drivers/misc/mediatek/Makefile.custom

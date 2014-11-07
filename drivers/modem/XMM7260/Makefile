#
# Makefile for the IMC modem drivers.
#

# Flashed [START]
obj-$(CONFIG_IMC_MODEM)	+= baseband-xmm-power.o

#EXTRA_CFLAGS            += -DDEBUG
#EXTRA_CFLAGS            += -DDEBUG_DEVICE_DESCRIPTOR
#EXTRA_CFLAGS            += -DEXPORT_GPIOS
EXTRA_CFLAGS			+= -DCONFIG_MIGRATION_CTL_DRV
#EXTRA_CFLAGS            += -DDEBUG_PM_TIME
EXTRA_CFLAGS    		+= -DODIN_DWC_HOST_SET_CPU_AFFINITY
#EXTRA_CFLAGS            += -DREMOVE_LOGS
#baseband-xmm-power-objs := board-x3-baseband.o
# Flashed [END]

# Flashless [START]
obj-$(CONFIG_IMC_DRV)	+= mdm_ctrl.o \
		mdm_util.o \
		mcd_mdm.o \
		mcd_cpu.o \
		mcd_pmic.o \
		mcd_pm.o

#mdm_ctrl-objs := mdm_util.o mcd_mdm.o mcd_cpu.o mcd_pmic.o mcd_pm.o
# Flashless [END]

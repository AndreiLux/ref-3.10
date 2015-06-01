obj-$(CONFIG_MTK_ECCCI_DRIVER) := ccci.o

ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/eccci/
ccflags-y += -I$(MTK_ROOT)/kernel/drivers/conn_md/include/

ifeq ($(MTK_SECURITY_SW_SUPPORT), yes)
ccflags-y  += -DENABLE_MD_IMG_SECURITY_FEATURE
endif
ifeq ($(MTK_SEC_MODEM_NVRAM_ANTI_CLONE), yes)
ccflags-y  += -DMTK_SEC_MODEM_NVRAM_ANTI_CLONE
endif

# common services
ccci-y := ccci_core.o ccci_bm.o ccci_support.o

# port routines
ccci-y += port_char.o port_net.o port_kernel.o port_ipc.o port_cfg.o

# modem drivers
# modem_xxx.o and modem_ut.o should not be enabled both, unless you checked code (device number. etc.).
ifeq ($(CONFIG_MTK_ECCCI_UT), y)
ccci-y += modem_ut.o
endif

ifeq ($(CONFIG_MTK_ECCCI_CCIF), y)
ccci-y += ccci_ringbuf.o modem_ccif.o
endif

ifeq ($(CONFIG_MTK_ECCCI_CLDMA), y)
ccci-y += modem_cldma.o
endif

# other dependencies
ifneq ($(EVDO_DT_SUPPORT), yes)
ifeq ($(MTK_DT_SUPPORT), yes)
ccflags-y += -DENABLE_EXT_MD_DSDA
endif
endif

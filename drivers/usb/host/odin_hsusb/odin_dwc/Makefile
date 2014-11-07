#
# Makefile for DWC_otg Highspeed USB controller driver
#

ifneq ($(KERNELRELEASE),)

BUS_INTERFACE =  -DPLATFORM_DEVICE_INTERFACE

#EXTRA_CFLAGS	+= -DDEBUG

#EXTRA_CFLAGS	+= -DODIN_DWC_HOST_DEBUG
EXTRA_CFLAGS	+= -DODIN_DWC_HOST_RECOV_BY_SWRESET
#EXTRA_CFLAGS	+= -DPANIC_DEBUG
EXTRA_CFLAGS    += -DODIN_DWC_HOST_SET_CPU_AFFINITY
EXTRA_CFLAGS	+= -DODIN_DWC_HOST_ENABLE_SOF_INT
EXTRA_CFLAGS	+= -DODIN_DWC_HOST_FLB_FIXED_MEMCLK

EXTRA_CFLAGS        += -DDWC_HOST_ONLY
#EXTRA_CFLAGS	+= -Dlinux -DDWC_HS_ELECT_TST
#EXTRA_CFLAGS	+= -DDWC_EN_ISOC
#EXTRA_CFLAGS   	+= -I$(PWD)/../dwc_common_port
EXTRA_CFLAGS   	+= -I$(PWD)/drivers/usb/host/odin_hsusb/dwc_common_port
#EXTRA_CFLAGS   	+= -I$(PORTLIB)
EXTRA_CFLAGS   	+= -DDWC_LINUX
EXTRA_CFLAGS   	+= $(CFI)
EXTRA_CFLAGS	+= $(BUS_INTERFACE)
#EXTRA_CFLAGS	+= -DDWC_DEV_SRPCAP


obj-$(CONFIG_ODIN_DWC_USB_HOST)	:= dwc_host.o

dwc_host-objs	:= odin_dwc_host.o
dwc_host-objs	+= dwc_otg_cil.o dwc_otg_cil_intr.o
dwc_host-objs	+= odin_dwc_host_ext.o dwc_otg_hcd.o dwc_otg_hcd_linux.o dwc_otg_hcd_intr.o dwc_otg_hcd_queue.o dwc_otg_hcd_ddma.o
dwc_host-objs	+= dwc_otg_adp.o
ifneq ($(CFI),)
dwc_host-objs	+= dwc_otg_cfi.o
endif

else

PWD		:= $(shell pwd)
PORTLIB		:= $(PWD)/../odin_dwc_common_port

# Command paths
CTAGS		:= $(CTAGS)
DOXYGEN		:= $(DOXYGEN)

default: portlib
	$(MAKE) -C$(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

install: default
	$(MAKE) -C$(KDIR) M=$(PORTLIB) modules_install
	$(MAKE) -C$(KDIR) M=$(PWD) modules_install

portlib:
	$(MAKE) -C$(KDIR) M=$(PORTLIB) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	cp $(PORTLIB)/Module.symvers $(PWD)/

docs:	$(wildcard *.[hc]) doc/doxygen.cfg
	$(DOXYGEN) doc/doxygen.cfg

tags:	$(wildcard *.[hc])
	$(CTAGS) -e $(wildcard *.[hc]) $(wildcard linux/*.[hc]) $(wildcard $(KDIR)/include/linux/usb*.h)


clean:
	rm -rf   *.o *.ko .*cmd *.mod.c .tmp_versions Module.symvers

endif

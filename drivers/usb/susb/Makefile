#
# Makefile for DWC_otg Highspeed USB controller driver
#

DWCPATH=$(PWD)/drivers/usb/susb/dwc_common_port
BUS_INTERFACE = -DLM_INTERFACE

#ccflags-y	+= -DDEBUG
#ccflags-y        += -DDWC_HOST_ONLY
#ccflags-y        += -DDWC_DEVICE_ONLY

ccflags-y	+= -Dlinux -DDWC_HS_ELECT_TST
#ccflags-y	+= -DDWC_EN_ISOC
ccflags-y	+= -DFORCE_ID_STATE

ccflags-y	+= -DDWC_LINUX
#ccflags-y	+= $(CFI)
ccflags-y	+= $(BUS_INTERFACE)
#ccflags-y	+= -DDWC_DEV_SRPCAP

ccflags-y	+= -I$(DWCPATH)
ccflags-y	+= -I$(PWD)
obj-y	:= dwc_otg.o
obj-y   += dwc_common_port/
dwc_otg-objs	:= dwc_otg_driver.o dwc_otg_attr.o
dwc_otg-objs	+= dwc_otg_cil.o dwc_otg_cil_intr.o #dwc_otg_core_param.o
dwc_otg-objs	+= dwc_otg_pcd_linux.o dwc_otg_pcd.o dwc_otg_pcd_intr.o
dwc_otg-objs	+= dwc_otg_hcd.o dwc_otg_hcd_linux.o dwc_otg_hcd_intr.o dwc_otg_hcd_queue.o dwc_otg_hcd_ddma.o
dwc_otg-objs	+= dwc_otg_adp.o
dwc_otg-objs	+= lm.o

ifeq ($(CONFIG_ARCH_K3V2),y)
dwc_otg-objs    += dwc_otg_k3v2.o
else
dwc_otg-objs    += dwc_otg_hi3630.o
dwc_otg-objs	+= hisi_usb_vbus.o
endif

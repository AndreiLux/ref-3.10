#
# Makefile for DWC_common library
#
ifneq ($(KERNELRELEASE),)

EXTRA_CFLAGS	+= -DDWC_LINUX
#EXTRA_CFLAGS	+= -DDEBUG
#EXTRA_CFLAGS	+= -DDWC_DEBUG_REGS
#EXTRA_CFLAGS	+= -DDWC_DEBUG_MEMORY

#EXTRA_CFLAGS	+= -DDWC_LIBMODULE
#EXTRA_CFLAGS	+= -DDWC_CCLIB
#EXTRA_CFLAGS	+= -DDWC_CRYPTOLIB
#EXTRA_CFLAGS	+= -DDWC_NOTIFYLIB
#EXTRA_CFLAGS	+= -DDWC_UTFLIB

obj-$(CONFIG_ODIN_DWC_USB_HOST)	:= dwc_common_port_lib.o
dwc_common_port_lib-objs := dwc_cc.o dwc_modpow.o dwc_dh.o \
			    dwc_crypto.o dwc_notifier.o \
			    dwc_common_linux.o dwc_mem.o

else


ifeq ($(DOXYGEN),)
DOXYGEN		:= $(DOXYGEN)
endif

default:
	$(MAKE) -C$(KDIR) M=$(PWD) modules

docs:	$(wildcard *.[hc]) doc/doxygen.cfg
	$(DOXYGEN) doc/doxygen.cfg

tags:	$(wildcard *.[hc])
	$(CTAGS) -e $(wildcard *.[hc]) $(wildcard linux/*.[hc]) $(wildcard $(KDIR)/include/linux/usb*.h)

endif

clean:
	rm -rf   *.o *.ko .*cmd *.mod.c .tmp_versions Module.symvers


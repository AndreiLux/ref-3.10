#
# Makefile for DWC_common library
#

ccflags-y	:= -DDWC_LINUX
#ccflags-y	+= -DDEBUG
#ccflags-y	+= -DDWC_DEBUG_REGS
#ccflags-y	+= -DDWC_DEBUG_MEMORY

ccflags-y	+= -DDWC_LIBMODULE
ccflags-y	+= -DDWC_CCLIB
#ccflags-y	+= -DDWC_CRYPTOLIB
ccflags-y	+= -DDWC_NOTIFYLIB
ccflags-y	+= -DDWC_UTFLIB

obj-y			 := dwc_common_port_lib.o

dwc_common_port_lib-y := dwc_cc.o
#dwc_common_port_lib-y +=  dwc_modpow.o
dwc_common_port_lib-y += dwc_dh.o
#dwc_common_port_lib-y += dwc_crypto.o
dwc_common_port_lib-y += dwc_notifier.o
dwc_common_port_lib-y += dwc_common_linux.o
dwc_common_port_lib-y += dwc_mem.o

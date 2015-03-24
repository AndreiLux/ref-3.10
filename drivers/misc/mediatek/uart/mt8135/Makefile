include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -Werror \
             -I$(srctree)/drivers/misc/mediatek/uart/$(CONFIG_MTK_PLATFORM)
obj-$(CONFIG_MTK_SERIAL) := platform_uart.o
obj-y += platform_fiq_debugger.o

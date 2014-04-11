# Makefile of modem_if

EXTRA_CFLAGS += -Idrivers/misc/modem_v1

obj-y += sipc5_modem.o sipc5_io_device.o sipc5_common.o
obj-y += modem_variation.o modem_utils.o

obj-$(CONFIG_UMTS_MODEM_SS300) += modem_modemctl_device_ss300.o
obj-$(CONFIG_BOOT_DEVICE_SPI) += modem_boot_device_spi.o

obj-$(CONFIG_LINK_DEVICE_MEMORY) += modem_link_device_memory.o
obj-$(CONFIG_LINK_DEVICE_LLI) += modem_link_device_lli.o

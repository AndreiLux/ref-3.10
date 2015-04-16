EXTRA_CFLAGS += -I../vendor/hisi/include/drv/
EXTRA_CFLAGS += -I../vendor/hisi/platform/hi3630/soc/
EXTRA_CFLAGS += -I../vendor/hisi/config/product/hi3630_udp/config/
EXTRA_CFLAGS += -Idrivers/hisi/ipcm
EXTRA_CFLAGS += -DBSP_CORE_APP
ifdef CONFIG_HIFI_RESET
EXTRA_CFLAGS += -D_HIFI_WD_DEBUG
endif
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox_table.o
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox_gut.o
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox_ifc.o
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox_msg.o
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox_debug.o
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox_port_linux.o
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox_ifc.o
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox.o

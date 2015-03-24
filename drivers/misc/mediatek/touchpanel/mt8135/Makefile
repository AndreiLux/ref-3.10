ccflags-y += -Werror
obj-$(CONFIG_MTK_TOUCHPANEL_DUMMY) += dummy.o
ifeq ("generic", $(CONFIG_CUSTOM_KERNEL_TOUCHPANEL))
obj-$(CONFIG_MTK_TOUCHPANEL) += tpd_adc.o
obj-$(CONFIG_MTK_TOUCHPANEL) += tpd_adc_driver.o
endif

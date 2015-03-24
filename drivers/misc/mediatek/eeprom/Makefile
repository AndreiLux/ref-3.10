include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y += eeprom.o

ccflags-y += -I$(src)/inc -I$(srctree)/include/linux/camera

ifeq ($(CONFIG_MTK_EEPROM_DUMMYEEPROM_SUPPORT),y)
eeprom-y += dummy_eeprom.o
endif

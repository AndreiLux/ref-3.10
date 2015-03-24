include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y += lens.o

ifeq ($(CONFIG_MTK_LENS_DUMMYLENS_SUPPORT),y)
ccflags-y += -DDUMMY_LENS
lens-y += dummy_lens.o
endif

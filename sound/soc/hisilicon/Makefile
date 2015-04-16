# HI3630 Platform Support
ifeq ($(CONFIG_SND_SOC_PCM_HI3630),y)
EXTRA_CFLAGS += -I../vendor/hisi/include/drv/
EXTRA_CFLAGS += -Idrivers/hisi/mailbox
endif
snd-soc-hi3630-objs := hi3630_sio.o hi3630_srcup_normal.o hi3630_srcup_dsp.o hi3630_asp_dma.o hi3630_asp_common.o hi3630_srcdown.o hi3630_fake_fm.o hi3630_fake_sio.o

ifeq ($(CONFIG_HDMI_K3), y)
snd-soc-hi3630-objs += hi3630_pcm_hdmi.o hi3630_hdmi_i2s.o
endif 
obj-$(CONFIG_SND_SOC_PCM_HI3630) += snd-soc-hi3630.o

# HI3630-HI6401 Machine Support
snd-soc-hi3630-hi6401-objs := hi3630_hi6401.o

obj-$(CONFIG_SND_HI3630_HI6401) += snd-soc-hi3630-hi6401.o

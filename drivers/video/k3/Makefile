obj-$(CONFIG_FB_FAKE) := k3fb_fake.o

k3fb_fake-objs := \
	k3_fb_fake.o

obj-$(CONFIG_FB_K3) := k3fb.o
#obj-m := k3fb.o

k3fb-objs := \
	k3_mipi_dsi_host.o \
	k3_mipi_dsi.o \
	k3_pwm.o \
	k3_blpwm.o \
	k3_dpe.o \
	k3_dpe_utils.o \
	k3_fb_panel.o \
	k3_fb_isr.o \
	k3_fb_vsync.o \
	k3_fb_buf_sync.o \
	k3_fb_bl.o \
	k3_fb_utils.o \
	k3_fb.o \
	k3_overlay_utils.o \
	k3_overlay_online.o \
	k3_overlay_offline.o \
	k3_block_algorithm.o \
	k3_ov_cmd_list_utils.o \
	k3_offlinecomposer_panel.o \


k3fb-objs += panel/mipi_toshiba_MDY90.o
k3fb-objs += panel/mipi_jdi_OTM1902B.o
k3fb-objs += panel/mipi_jdi_NT35695.o
k3fb-objs += voltage/tps65132.o


EXTRA_CFLAGS += -Iarch/arm/mach-hisi \
		-Idrivers/video/k3 \
		-Idrivers/video/k3/panel

clean:
	rm *.o .*cmd

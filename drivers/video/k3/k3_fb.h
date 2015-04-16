/* Copyright (c) 2013-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
* GNU General Public License for more details.
*
*/
#ifndef K3_FB_H
#define K3_FB_H

#include <linux/console.h>
#include <linux/uaccess.h>
#include <linux/leds.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/raid/pq.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/time.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/backlight.h>
#include <linux/pwm.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>

#if defined(CONFIG_ARCH_HI3630FPGA)
#include <linux/spi/spi.h>
#endif

#include <linux/ion.h>
#include <linux/hisi_ion.h>
#include <linux/gpio.h>

#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>

#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/pinctrl/consumer.h>
#include <linux/file.h>
#include <linux/dma-buf.h>
#include <linux/genalloc.h>
#include <linux/huawei/dsm_pub.h>

#include <linux/huawei/hisi_irq_affinity.h>
#include "hisi_udp_board_id.h"

#include "k3_fb_def.h"
#include "k3_fb_panel.h"
#include "k3_dss_regs.h"
#include "k3_dss.h"
#include "k3_dpe_utils.h"
#include "k3_mipi_dsi.h"
#include "k3_ov_cmd_list_utils.h"


#define K3_FB0_NUM	(3)
#define K3_FB1_NUM	(2)
#define K3_FB2_NUM	(1)

#define K3_FB_MAX_DEV_LIST (32)
#define K3_FB_MAX_FBI_LIST (32)

#define K3_DSS_OFFLINE_MAX_NUM	(2)
#define K3_DSS_OFFLINE_MAX_BLOCK	(64)
#define K3_DSS_OFFLINE_MAX_LIST	(128)

//#define CONFIG_K3_FB_COLORBAR_USED
//#define CONFIG_K3_FB_VSYNC_THREAD
#define CONFIG_K3_FB_BACKLIGHT_DELAY
//#define CONFIG_FAKE_VSYNC_USED
#define CONFIG_BUF_SYNC_USED
//#define CONFIG_K3_FB_HEAP_CARVEOUT_USED
//#define CONFIG_MIPIDSI_BUGFIX
#define CONFIG_FIX_DIRTY_REGION_UPDT

#define VSYNC_CTRL_EXPIRE_COUNT	(4)

//Multi-Resolution====begin
enum {
    DISPLAY_LOW_POWER_LEVEL_FHD = 0x0,
    DISPLAY_LOW_POWER_LEVEL_HD = 0x1,
    DISPLAY_LOW_POWER_LEVEL_MAX,
};
//Multi-Resolution====end

struct k3fb_vsync {
	wait_queue_head_t vsync_wait;
	ktime_t vsync_timestamp;
	int vsync_created;
	int vsync_enabled;

	int vsync_ctrl_expire_count;
	int vsync_ctrl_enabled;
	int vsync_ctrl_disabled_set;
	struct work_struct vsync_ctrl_work;
	spinlock_t spin_lock;

	struct mutex vsync_lock;
#ifdef CONFIG_K3_FB_VSYNC_THREAD
	struct task_struct *vsync_thread;
#endif

	atomic_t buffer_updated;
	void (*vsync_report_fnc) (int buffer_updated);

	struct k3_fb_data_type *k3fd;
};

#ifdef CONFIG_BUF_SYNC_USED
#include <linux/sync.h>
#include <linux/sw_sync.h>

struct k3fb_buf_sync {
	struct sw_sync_timeline *timeline;
	int timeline_max;
	int refresh;
	spinlock_t refresh_lock;

};
#endif

struct k3fb_backlight {
#ifdef CONFIG_K3_FB_BACKLIGHT_DELAY
	struct delayed_work bl_worker;
#endif
	struct semaphore bl_sem;
	int bl_updated;
	int bl_level_old;

};


struct k3_fb_data_type {
	u32 index;
	u32 ref_cnt;
	u32 fb_num;
	u32 fb_imgType;
	u32 bl_level;

	char __iomem *dss_base;
	char __iomem *crgperi_base;

	u32 dpe_irq;
	u32 dsi0_irq;
	u32 dsi1_irq;

	struct regulator_bulk_data *dpe_regulator;

	const char *dss_axi_clk_name;
	const char *dss_pri_clk_name;
	const char *dss_aux_clk_name;
	const char *dss_pxl0_clk_name;
	const char *dss_pxl1_clk_name;
	const char *dss_pclk_clk_name;
	const char *dss_dphy0_clk_name;
	const char *dss_dphy1_clk_name;

	struct clk *dss_axi_clk;
	struct clk *dss_pri_clk;
	struct clk *dss_aux_clk;
	struct clk *dss_pxl0_clk;
	struct clk *dss_pxl1_clk;
	struct clk *dss_pclk_clk;
	struct clk *dss_dphy0_clk;
	struct clk *dss_dphy1_clk;

	struct k3_panel_info panel_info;
	bool panel_power_on;
	struct semaphore blank_sem;

	void (*pm_runtime_register) (struct platform_device *pdev);
	void (*pm_runtime_unregister) (struct platform_device *pdev);
	void (*pm_runtime_get) (struct k3_fb_data_type *k3fd);
	void (*pm_runtime_put) (struct k3_fb_data_type *k3fd);
	void (*bl_register) (struct platform_device *pdev);
	void (*bl_unregister) (struct platform_device *pdev);
	void (*bl_update) (struct k3_fb_data_type *k3fd);
	void (*bl_cancel) (struct k3_fb_data_type *k3fd);
	int (*sbl_ctrl_fnc) (struct fb_info *info, void __user *argp);
	void (*sbl_isr_handler)(struct k3_fb_data_type *k3fd);
	void (*vsync_register) (struct platform_device *pdev);
	void (*vsync_unregister) (struct platform_device *pdev);
	int (*vsync_ctrl_fnc) (struct fb_info *info, void __user *argp);
	void (*vsync_isr_handler) (struct k3_fb_data_type *k3fd);
	void (*buf_sync_register) (struct platform_device *pdev);
	void (*buf_sync_unregister) (struct platform_device *pdev);
	void (*buf_sync_signal) (struct k3_fb_data_type *k3fd);
	void (*buf_sync_suspend) (struct k3_fb_data_type *k3fd);

	bool (*set_fastboot_fnc) (struct fb_info *info);
	int (*open_sub_fnc) (struct fb_info *info);
	int (*release_sub_fnc) (struct fb_info *info);
	int (*on_fnc) (struct k3_fb_data_type *k3fd);
	int (*off_fnc) (struct k3_fb_data_type *k3fd);
	int (*frc_fnc) (struct k3_fb_data_type *k3fd, int fps);
	int (*esd_fnc) (struct k3_fb_data_type *k3fd);
	int (*pan_display_fnc) (struct k3_fb_data_type *k3fd);
	int (*ov_ioctl_handler) (struct k3_fb_data_type *k3fd, u32 cmd, void *arg);
	int (*ov_online_play) (struct k3_fb_data_type *k3fd, unsigned long *argp);
	int (*ov_offline_play) (struct k3_fb_data_type *k3fd, unsigned long *argp);
	int (*ov_online_wb) (struct k3_fb_data_type *k3fd, unsigned long *argp);
	int (*ov_optimized) (struct k3_fb_data_type *k3fd);
	int (*ov_online_wb_ctrl) (struct k3_fb_data_type *k3fd, int *argp);
	void (*ov_wb_isr_handler) (struct k3_fb_data_type *k3fd);
	void (*ov_vactive0_start_isr_handler) (struct k3_fb_data_type *k3fd);
	void (*set_reg) (struct k3_fb_data_type *k3fd,
		char __iomem *addr, u32 val, u8 bw, u8 bs);

	struct k3fb_backlight backlight;
	int sbl_enable;
	dss_sbl_t sbl;

	struct k3fb_vsync vsync_ctrl;
#ifdef CONFIG_BUF_SYNC_USED
	struct k3fb_buf_sync buf_sync_ctrl;
#endif
	struct dss_clk_rate dss_clk_rate;

#ifdef CONFIG_FAKE_VSYNC_USED
	bool fake_vsync_used;
	struct hrtimer fake_vsync_hrtimer;
#endif

	struct semaphore ov_wb_sem;
	wait_queue_head_t ov_wb_wq;
	u32 ov_wb_done;
	int ov_wb_enabled;
	bool is_hdmi_power_full;

	wait_queue_head_t offline_writeback_wq[K3_DSS_OFFLINE_MAX_NUM];
	u32 offline_wb_done[K3_DSS_OFFLINE_MAX_NUM];
	u32 offline_wb_status[K3_DSS_OFFLINE_MAX_NUM];
	struct list_head offline_cmdlist_head[K3_DSS_OFFLINE_MAX_NUM];
	dss_rect_t *block_rects[K3_DSS_OFFLINE_MAX_BLOCK];
	dss_overlay_t *block_overlay;
	struct semaphore dpe3_ch0_sem;
	struct semaphore dpe3_ch1_sem;
	dss_cmdlist_node_t * cmdlist_node[K3_DSS_OFFLINE_MAX_LIST];

	u8 ovl_type;
	dss_overlay_t ov_req;
	dss_overlay_t ov_wb_req;
	dss_global_reg_t dss_glb;
	dss_module_reg_t dss_module;
	dss_exception_t dss_exception;
	dss_exception_t dss_wb_exception;
	dss_rptb_info_t dss_rptb_info_prev[K3_DSS_OFFLINE_MAX_NUM];
	dss_rptb_info_t dss_rptb_info_cur[K3_DSS_OFFLINE_MAX_NUM];
	dss_rptb_info_t dss_wb_rptb_info_prev[K3_DSS_OFFLINE_MAX_NUM];
	dss_rptb_info_t dss_wb_rptb_info_cur[K3_DSS_OFFLINE_MAX_NUM];

	bool dss_module_resource_initialized;
	dss_global_reg_t dss_glb_default;
	dss_module_reg_t dss_module_default;

	struct dss_rect dirty_region_updt;
	u8 dirty_region_updt_enable;

	struct ion_client *ion_client;
	struct ion_handle *ion_handle;
	struct iommu_map_format iommu_format;

	struct fb_info *fbi;
	struct platform_device *pdev;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
    //Multi Resolution====begin
    int switch_res_flag;
    //Multi Resolution====end

	wait_queue_head_t vactive0_start_wq;
	u32 vactive0_start_flag;
	u32 ldi_data_gate_en;
};


/******************************************************************************
** FUNCTIONS PROTOTYPES
*/
extern int g_debug_ldi_underflow;
extern int g_debug_mmu_error;
extern int g_debug_ovl_osd;
extern int g_debug_online_vsync;
extern int g_debug_ovl_online_composer;
extern int g_debug_ovl_offline_composer;
extern int g_debug_ovl_offline_cmdlist;
extern int g_debug_ovl_optimized;
extern int g_enable_ovl_optimized;
extern int g_scf_stretch_threshold;
extern int g_enable_dirty_region_updt;
extern int g_debug_dirty_region_updt;

extern int g_debug_mipi_dphy_lp;
extern int g_debug_dss_clk_lp;

extern int g_debug_dss_adp_sr;

extern int g_primary_lcd_xres;
extern int g_primary_lcd_yres;

extern struct fb_info *fbi_list[K3_FB_MAX_FBI_LIST];
extern struct k3_fb_data_type *k3fd_list[K3_FB_MAX_FBI_LIST];

extern unsigned int get_boot_into_recovery_flag(void);

/* backlight */
void k3fb_backlight_update(struct k3_fb_data_type *k3fd);
void k3fb_backlight_cancel(struct k3_fb_data_type *k3fd);
void k3fb_backlight_register(struct platform_device *pdev);
void k3fb_backlight_unregister(struct platform_device *pdev);
void k3fb_sbl_isr_handler(struct k3_fb_data_type *k3fd);

/* vsync */
#ifdef CONFIG_FAKE_VSYNC_USED
enum hrtimer_restart k3fb_fake_vsync(struct hrtimer *timer);
#endif
void k3fb_activate_vsync(struct k3_fb_data_type *k3fd);
void k3fb_deactivate_vsync(struct k3_fb_data_type *k3fd);
int k3fb_vsync_ctrl(struct fb_info *info, void __user *argp);
int k3fb_vsync_resume(struct k3_fb_data_type *k3fd);
int k3fb_vsync_suspend(struct k3_fb_data_type *k3fd);
void k3fb_vsync_isr_handler(struct k3_fb_data_type *k3fd);
void k3fb_vsync_register(struct platform_device *pdev);
void k3fb_vsync_unregister(struct platform_device *pdev);

/* buffer sync */
int k3fb_buf_sync_wait(int fence_fd);
void k3fb_buf_sync_signal(struct k3_fb_data_type *k3fd);
void k3fb_buf_sync_suspend(struct k3_fb_data_type *k3fd);
int k3fb_buf_sync_create_fence(struct k3_fb_data_type *k3fd, unsigned value);
void k3fb_buf_sync_register(struct platform_device *pdev);
void k3fb_buf_sync_unregister(struct platform_device *pdev);

/* control */
int k3fb_ctrl_fastboot(struct k3_fb_data_type *k3fd);
int k3fb_ctrl_on(struct k3_fb_data_type *k3fd);
int k3fb_ctrl_off(struct k3_fb_data_type *k3fd);
int k3fb_ctrl_sbl(struct fb_info *info, void __user *argp);
int k3fb_ctrl_dss_clk_rate(struct fb_info *info, void __user *argp);
int k3fb_ctrl_frc(struct k3_fb_data_type *k3fd, int fps);
int k3fb_ctrl_esd(struct k3_fb_data_type *k3fd);

void set_reg(char __iomem *addr, u32 val, u8 bw, u8 bs);
u32 set_bits32(u32 old_val, u32 val, u8 bw, u8 bs);
void k3fb_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *addr, u32 val, u8 bw, u8 bs);
u32 k3fb_line_length(int index, u32 xres, int bpp);
unsigned int k3fb_ms2jiffies(unsigned long ms);
void k3fb_get_timestamp(struct timeval *tv);
u32 k3fb_timestamp_diff(struct timeval *lasttime, struct timeval *curtime);

struct platform_device *k3_fb_device_alloc(struct k3_fb_panel_data *pdata,
	u32 type, u32 id);
struct platform_device *k3_fb_add_device(struct platform_device *pdev);

int k3_fb1_blank(int blank_mode);
void k3_fb1_power_ctrl(bool power_full);

#endif /* K3_FB_H */

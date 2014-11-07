/*
 *  drivers/video/odin/dss/dss.h
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ODIN_DSS_H__
#define __ODIN_DSS_H__

#include "du.h"
#include <video/odindss.h>
#include <linux/clk.h>

#define DSSCOMP_FENCE_SYNC	/* fence sync in dsscomp node */
#ifdef DSSCOMP_FENCE_SYNC
#define FENCE_SYNC_LOG	0	/* fence sync log */
#else
#endif

#define ODIN_ION_FD
/* #define DSS_DFS_MUTEX_LOCK */
#ifdef CONFIG_PANEL_LGLH600WF2
#define DSS_DFS_SPINLOCK
#endif
//#define DSS_VIDEO_PLAYBACK_CABC
#define DSS_REGION_CHECK_DFS_REQUEST_CLK
#ifdef DSS_REGION_CHECK_DFS_REQUEST_CLK
/* 0/1 : intersection check or layer cnt check */
#define DSS_LAYER_CNT_DFS_REQUEST_CLK	0
#endif
#define DSS_INTERRUPT_COMMAND_SEND
#define ODIN_DSS_UNDERRUN_RESTORE
#define DSS_UNDERRUN_LOG
/* #define DSS_TIME_CHECK_LOG */
/* #define DSS_CACHE_BACKUP */
#define MIPI_PKT_SIZE_CONTROL
#define WRITEBACK_FRAMEDONE_POLLONG
#define LCD_ESD_CHECK

#ifdef DSS_TIME_CHECK_LOG
#include <linux/ktime.h>
#ifdef DSS_TIME_CHECK_LOG
enum odin_dss_time_stamp {
	ODIN_CMD_START_END_TIME		= 0, /* From UPDATE START TO FRAME DONE */
	ODIN_CMD_VSYNC_END_TIME		= 1, /* From VSYNC END TO FRAME DONE */
	ODIN_CMD_QOS_LOCK_TIME		= 2, /* From QOS Start TO SPIN LOCK */
	ODIN_CMD_TEMP_TIME			= 3, /* TEMP */
	ODIN_VIDEO_FPS				= 4, /* FPS */
	ODIN_MAX_TIME_NUM			= 5
};
#endif
extern enum odin_dss_time_stamp check_time;
#endif

#ifdef CONFIG_ODIN_DSS_DEBUG_SUPPORT
#define DEBUG
#endif

extern unsigned int dss_debug;
#ifdef DEBUG
#ifdef DSS_SUBSYS_NAME
#define DSSDBG(format, ...) \
	if (dss_debug) \
		printk(KERN_DEBUG "odindss " DSS_SUBSYS_NAME ": " format, \
		## __VA_ARGS__)
#else
#define DSSDBG(format, ...) \
	if (dss_debug) \
		printk(KERN_DEBUG "odindss: " format, ## __VA_ARGS__)
#endif

#ifdef DSS_SUBSYS_NAME
#define DSSDBGF(format, ...) \
	if (dss_debug) \
		printk(KERN_DEBUG "odindss " DSS_SUBSYS_NAME \
				": %s(" format ")\n", \
				__func__, \
				## __VA_ARGS__)
#else
#define DSSDBGF(format, ...) \
	if (dss_debug) \
		printk(KERN_DEBUG "odindss: " \
				": %s(" format ")\n", \
				__func__, \
				## __VA_ARGS__)
#endif

#else /* DEBUG */
#define DSSDBG(format, ...)
#define DSSDBGF(format, ...)
#endif


#ifdef DSS_SUBSYS_NAME
#define DSSERR(format, ...) \
	printk(KERN_ERR "odindss " DSS_SUBSYS_NAME " error: " format, \
	## __VA_ARGS__)
#else
#define DSSERR(format, ...) \
	printk(KERN_ERR "odindss error: " format, ## __VA_ARGS__)
#endif

#ifdef DSS_SUBSYS_NAME
#define DSSINFO(format, ...) \
	printk(KERN_INFO "odindss " DSS_SUBSYS_NAME ": " format, \
	## __VA_ARGS__)
#else
#define DSSINFO(format, ...) \
	printk(KERN_INFO "odindss: " format, ## __VA_ARGS__)
#endif

#ifdef DSS_SUBSYS_NAME
#define DSSWARN(format, ...) \
	printk(KERN_WARNING "odindss " DSS_SUBSYS_NAME ": " format, \
	## __VA_ARGS__)
#else
#define DSSWARN(format, ...) \
	printk(KERN_WARNING "odindss: " format, ## __VA_ARGS__)
#endif

#if CONFIG_PANEL_LGLH600WF2
#define RUNTIME_CLK_GATING	1
#if RUNTIME_CLK_GATING
/* #define RUNTIME_CLK_GATING_LOG*/
#define DP_PLL_GATING 	1
#endif
#endif

struct seq_file;
struct platform_device;
extern struct pm_qos_request dss_mem_qos;
extern struct pm_qos_request sync1_mem_qos;
extern struct pm_qos_request sync2_mem_qos;

enum odin_mipi_dsi_index
{
	ODIN_DSI_CH0 = 0,
	ODIN_DSI_CH1,
	ODIN_DSI_CH_MAX,
};

enum odin_overlay_operation {
	ODIN_DSS_OVL_COP	= 0,
	ODIN_DSS_OVL_ROP	= 1,
};

/* core */
struct bus_type *dss_get_bus(void);
int	 odin_dss_register_driver(struct odin_dss_driver *dssdriver);
void 	odin_dss_unregister_driver(struct odin_dss_driver *dssdriver);


/* display */
int 	dss_suspend_all_devices(void);
int 	dss_resume_all_devices(void);
void 	dss_disable_all_devices(void);

void 	dss_init_device(struct platform_device *pdev,
			struct odin_dss_device *dssdev);
void 	dss_uninit_device(struct platform_device *pdev,
		struct odin_dss_device *dssdev);
bool 	dss_use_replication(struct odin_dss_device *dssdev,
		enum odin_color_mode mode);
void 	default_get_overlay_fifo_thresholds(enum odin_dma_plane plane,
		u32 fifo_size, enum odin_burst_size *burst_size,
		u32 *fifo_low, u32 *fifo_high);


void 	odindss_default_get_resolution(struct odin_dss_device *dssdev,
			u16 *xres, u16 *yres);
int 	odindss_default_get_recommended_bpp(struct odin_dss_device *dssdev);

/* mipi-dsi */
void odin_mipi_dsi_phy_power(enum odin_mipi_dsi_index resource_index,
	bool enable);
void odin_mipi_hs_video_start_stop(enum odin_mipi_dsi_index resource_index,
	struct odin_overlay_manager *mgr, bool start_stop);
int odin_mipi_dsi_vsync_wait_for_irq_interruptible_timeout
		(unsigned long timeout);
bool odin_mipi_cmd_loop_get(enum odin_mipi_dsi_index resource_index);
int odin_mipi_cmd_wait_for_framedone(u32 irqmask, u32 sub_irqmask,
		unsigned long timeout);
s64 odin_mip_dsi_time_after_update(void);
int odin_mipi_dsi_get_update_status(void);
void odin_mipi_dsi_set_update_status(bool value);
int odin_mipi_dsi_update_done_wait(struct odin_overlay_manager *mgr,
							unsigned long timeout);
u32 odin_mipi_dsi_vsync_after_time(void);
void dipc_datapath_en(enum odin_mipi_dsi_index ch,
	enum odin_display_type type);
unsigned int odin_mipi_dsi_get_irq_mask
	(enum odin_mipi_dsi_index resource_index);

unsigned int odin_mipi_dsi_irq_clear(enum odin_mipi_dsi_index resource_index,
	u32 src0_ix);

unsigned int odin_mipi_dsi_irq_mask(enum odin_mipi_dsi_index resource_index,
	u32 src0_ix);

unsigned int odin_mipi_dsi_get_irq_status
	(enum odin_mipi_dsi_index resource_index);
int odin_mipi_dsi_get_state(void);

/* manager */
bool odin_mgr_iova_check(u32 iova_addr);
int odin_mgr_dss_init_overlay_managers(struct platform_device *pdev);
void odin_mgr_dss_uninit_overlay_managers(struct platform_device *pdev);
int odin_mgr_dss_wait_for_go_ovl(struct odin_overlay *ovl);
void odin_mgr_dss_start_update(struct odin_dss_device *dssdev);
int odin_mgr_dss_ovl_set_info(struct odin_overlay *ovl,
	struct odin_overlay_info *info);

#ifdef DSS_TIME_CHECK_LOG
void odin_mgr_dss_time_start(enum odin_dss_time_stamp time_item);
s64 odin_mgr_dss_time_end(enum odin_dss_time_stamp time_item);
s64 odin_mgr_dss_time_end_ms(enum odin_dss_time_stamp time_item);
#endif

/* overlay */
void odin_ovl_dss_init_overlays(struct platform_device *pdev);
void odin_ovl_dss_uninit_overlays(struct platform_device *pdev);
int odin_ovl_dss_check_overlay(struct odin_overlay *ovl,
	struct odin_dss_device *dssdev);
void odin_ovl_dss_overlay_setup_du_manager(struct odin_overlay_manager *mgr);
void odin_ovl_dss_recheck_connections(struct odin_dss_device *dssdev,
	bool force);

/* writeback */
int odin_wb_dss_get_num_writebacks(void);
void odin_wb_dss_init_writeback(struct platform_device *pdev);
void odin_wb_dss_uninit_writeback(struct platform_device *pdev);
bool odin_dss_check_wb(struct writeback_cache_data *wb, int overlayId,
			int managerId);

/* DU */
void odin_du_set_ovl_cop_or_rop(enum odin_channel channel, bool ovl_op);
u32 odin_du_get_ovl_src_size(int zorder);
u32 odin_du_get_ovl_src_pos(int zorder);
u32 odin_du_get_rd_staus(enum odin_dma_plane plane);
bool odin_du_iova_check(u32 iova_addr);
int odin_du_check_overlay_src_using_in_ch(enum odin_channel channel,
	enum odin_dma_plane plane);
int odin_du_check_overlay_src_using(enum odin_dma_plane plane);
u32 odin_du_get_ovl_src_go(void);
void odin_du_enable_channel(enum odin_channel channel,
	enum odin_display_type type, bool enable);
u32 odin_du_vsync_after_time(void);
#if 0
int du_runtime_get(void);
void du_runtime_put(void);
#endif

int odin_du_ch_all_disable_plane(enum odin_channel channel);
int odin_du_enable_plane(enum odin_dma_plane plane, enum odin_channel channel,
	bool enable, enum dsscomp_setup_mode mode, bool invisible,
	bool frame_skip);
int odin_du_enable_wb_plane(enum odin_channel sync_src,
			    enum odin_dma_plane wb_plane,
			    enum odin_writeback_mode mode, bool enable);
bool odin_du_go_busy(enum odin_channel channel);
void odin_du_go(enum odin_channel channel);
int odin_du_setup_plane(enum odin_dma_plane plane,
		u16 width, u16 height,
		u16 crop_x, u16 crop_y,
		u16 crop_w, u16 crop_h,
		u16 out_x, u16 out_y,
		u16 out_w, u16 out_h,
		enum odin_color_mode color_mode,
		u8 rotation, bool mirror,
		enum odin_channel channel, u32 p_y_addr,
		     u32 p_u_addr, u32 p_v_addr,
		enum odin_overlay_zorder zorder,
		u8 blending,
		u8 global_alpha,
		u8 pre_mult_alpha,
		u8 mgr_flip,
		bool mxd_use,
		bool invisible,
		u8 mode
);

int odin_du_setup_wb_plane(enum odin_dma_plane wb_plane,
		u16 width, u16 height,
		u16 out_x, u16 out_y,
		u16 out_w, u16 out_h,
		enum odin_color_mode color_mode,
		u32 p_y_addr, u32 p_uv_addr,
		enum odin_channel sync_src,
		enum odin_writeback_mode mode
);
int odin_du_query_sync_wb_done(enum odin_dma_plane wb_plane);
void odin_du_set_default_color(enum odin_channel channel, u32 color);
u32 odin_du_get_ovl_src_sel(enum odin_channel channel);
void odin_du_set_dst_select(enum odin_channel channel, u8 ovl_dst_sel);
void odin_du_set_ovl_width_height(enum odin_channel channel,
	u16 size_x, u16 size_y);
void odin_du_set_ovl_mixer_default(enum odin_channel channel);
void odin_du_set_channel_sync_enable(u8 sync_numeber, u8 enable);
void odin_du_set_parallel_lcd_test_mode(u8 parallel_enable);
void odin_du_enable_trans_key(enum odin_channel ch, bool enable);
void odin_du_set_trans_key(enum odin_channel ch, u32 trans_key);
 void odin_du_set_zorder(enum odin_dma_plane plane,
			enum odin_overlay_zorder zorder);
void odin_du_enable_zorder(enum odin_dma_plane plane, bool enable);
void odin_du_mxd_size_irq_set(enum odin_channel channel);
void odin_du_set_mxd_mux_switch_control(enum odin_channel channel,
	enum odin_dma_plane plane, bool mXD_use, int scenario,
	int crop_w, int crop_h, int out_w, int out_h);
int odin_du_power_on(struct odin_dss_device *dss_dev);
int odin_du_power_off(struct odin_dss_device *dss_dev);
int odin_du_display_init(struct odin_dss_device *dss_dev);
void odin_du_sync_set_act_size(int ch, int vact, int hact);
int odin_du_plane_init(void);
void odin_du_set_channel_out(enum odin_dma_plane plane,
	enum odin_channel channel);
int odin_set_rd_enable_plane(enum odin_channel channel, bool enable);

int odin_du_init_platform_driver(void);
void odin_du_uninit_platform_driver(void);

/* CRG */
#ifdef DSS_PD_CRG_DUMP_LOG
int odin_crg_get_register(u32 offset, bool secure);
#endif
int odin_crg_get_dss_clk_ena(enum odin_crg_clk_gate clk_gate);
int odin_crg_dss_clk_ena(enum odin_crg_clk_gate clk_gate, u32 clk_gate_val,
	bool enable);
int odin_crg_dss_pll_ena(struct clk *pll_clk, bool enable);
int odin_crg_dss_mem_sleep_in(enum odin_crg_mem_deep_sleep mem_sleep,
	bool enable);
int odin_crg_dss_channel_clk_enable(enum odin_channel channel, bool enable);
u32 odin_crg_du_plane_to_clk(u32 plane, bool enable);
int odin_crg_underrun_set_info(enum odin_channel channel, u32 ovl_mask, u32 request_qos);
int odin_crg_underrun_process_thread(enum odin_channel channel,
	bool reset_needed);
void odin_crg_mipi_set_err_crg_mask(enum odin_channel channel);
int odin_crg_set_err_mask(struct odin_dss_device *dss_dev, bool enable);
bool odin_crg_get_error_handling_status(enum odin_channel channel);
int odin_crg_hdmi_div_debug(void);

typedef void (*odin_crg_isr_t) (void *arg, u32 mask, u32 sub_mask);
void odin_crg_set_clock_div(int ch, int val); /* for only hdmi divide now. */
void odin_crg_set_channel_int_mask(struct odin_dss_device *dssdev, bool enable);
void odin_crg_set_interrupt_mask(u32 mask, bool enable);
void odin_crg_set_interrupt_clear(u32 mask);
void odin_crg_clk_update(int i);
int odin_crg_register_isr(odin_crg_isr_t isr, void *arg,
	u32 mask, u32 sub_mask);
int odin_crg_unregister_isr(odin_crg_isr_t isr, void *arg,
	u32 mask, u32 sub_mask);
int odin_crg_wait_for_irq_interruptible_timeout(u32 irqmask, u32 sub_irqmask,
		unsigned long timeout);
int odin_crg_init_platform_driver(void);
void odin_crg_uninit_platform_driver(void);
#if RUNTIME_CLK_GATING
void odin_crg_runtime_clk_enable(enum odin_channel channel, bool enable);
void release_clk_status(int value);
void enable_runtime_clk (struct odin_overlay_manager *mgr, bool mem2mem_mode);
void disable_runtime_clk (enum odin_channel channel);
bool check_clk_gating_status(void);
#endif
/* XD_Engine */
int 	odin_mxd_init_platform_driver(void);
void 	odin_mxd_uninit_platform_driver(void);
/* CABC */
int odin_cabc_init_platform_driver(void);
void odin_cabc_uninit_platform_driver(void);

/* DIPC */
int odin_dipc_resync_hdmi(struct odin_dss_device *dss_dev);
int odin_dipc_power_on(struct odin_dss_device *dss_dev);
int odin_dipc_power_off(struct odin_dss_device *dss_dev);
int odin_dipc_fifo_flush(int dip_ch);
int odin_dipc_get_lcd_cnt(int dip_ch);
int odin_dipc_resync(int dip_ch);
int 	odin_dipc_display_init(struct odin_dss_device *dss_dev);
int 	odin_dipc_init_platform_driver(void);
void 	odin_dipc_uninit_platform_driver(void);
int 	odin_dipc_enable(enum odin_channel ch);
int 	odin_dipc_disable(enum odin_display_type ch);
unsigned int dipc_get_irq_mask(enum odin_channel channel);
unsigned int dipc_get_irq_status(enum odin_channel channel);
unsigned int dipc_clear_irq(enum odin_channel channel, u32 reg_val);
unsigned int dipc_mask_irq(enum odin_channel channel, u32 reg_val);
unsigned int dipc_check_irq(int dip_irq, enum odin_channel channel);
int dipc_fifo_empty_check(int dip_ch);
int dipc_test_fifo_read(int dip_ch);
int odin_dipc_ccr_gamma_config(int dip_ch, u32 src_gamma_data);
bool odin_dipc_ccr_gamma_config_status(int dip_ch);
#ifdef CONFIG_LCD_KCAL
int 	update_screen_dipc_lut(void);
#endif

/* S3D */
int 	odin_formatter_init_platform_driver(void);
void 	odin_formatter_uninit_platform_driver(void);

/* HDMI */
int 	odin_hdmi_init_platform_driver(void);
void 	odin_hdmi_uninit_platform_driver(void);

int 	odin_hdmi_display_check_timing(struct odin_dss_device *dssdev,
					struct odin_video_timings *timings);
int 	odin_hdmi_display_set_mode(struct odin_dss_device *dssdev,
					struct fb_videomode *vm);
void 	odin_hdmi_display_set_timing(struct odin_dss_device *dssdev);
int 	odin_hdmi_display_enable(struct odin_dss_device *dssdev);
void 	odin_hdmi_display_disable(struct odin_dss_device *dssdev);

int 	odin_hdmi_panel_init(void);
void 	odin_hdmi_panel_exit(void);


/* ROTATOR */
int 	odin_rotator_init_platform_driver(void);
void 	odin_rotator_uninit_platform_driver(void);

/* DSSCOMP */
int dsscomp_ion_client_create(void);
void dsscomp_ion_client_destory(void);
struct ion_client* dsscomp_get_ion_client(void);
int dsscomp_complete_workqueue(struct odin_dss_device *dssdev, bool wb_pending_wait, bool release);
void dsscomp_refresh_syncpt_value (int display_num);
int dsscomp_unplug_state_reset(void *ptr);
void dsscomp_release_fence_display(int display_num, bool refresh);
void dsscomp_hdmi_free(void);
#ifdef DSS_CACHE_BACKUP
int odin_dss_cache_log(int mgr_ix);
#endif

u32 odin_crg_dss_plane_to_resetmask(enum odin_dma_plane plane);
int odin_crg_dss_reset(u32 reset_mask, bool enable);

/* DU */
void odin_du_set_channel_sync_enable(u8 sync_number, u8 enable);
void odin_du_set_ovl_sync_enable(enum odin_channel channel, bool sync_enable);
u32 odin_du_get_src_size_width(enum odin_dma_plane plane);
u32 odin_du_get_src_size_height(enum odin_dma_plane plane);

u32 odin_du_get_src_pipsize_width(enum odin_dma_plane plane);
u32 odin_du_get_src_pipsize_height(enum odin_dma_plane plane);

#ifdef DSS_DFS_MUTEX_LOCK
extern struct mutex dfs_mtx;
#endif

#endif



/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "dwc_otg_regs.h"
#include "dwc_otg_hcd.h"
#include "dwc_otg_core_if.h"

#include "odin_dwc_host.h"

#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/odin_pd.h>
#include <linux/usb/hcd.h>
#if defined(CONFIG_IMC_DRV)
#include <linux/mdm_ctrl.h>
#include <linux/gpio.h>
#endif
#define DUMP_IPC_PACKET
#ifdef DUMP_IPC_PACKET
#include <linux/slab.h>
#endif

#if defined(ODIN_DWC_HOST_SET_CPU_AFFINITY)
#include <linux/odin_cpuidle.h>

#define IPC_CPU_AFFINITY_NUM	(2)
#endif


/* To get modem coredump when host wakeup signal isn't received.*/
#define NO_HOSTWAKEUP_DUMP 1

/* To handle corner case of HOST_WAKEUP */
#define PENDING_HW

#if defined(ODIN_DWC_HOST_FLB_FIXED_MEMCLK)
#include <linux/pm_qos.h>

static struct pm_qos_request flb_mem_qos_req;
#define FLB_MEM_FREQ (800000)
#endif

odin_dwc_ddata_t *odin_ddata;
EXPORT_SYMBOL(odin_ddata);
uint32_t g_dbg_lvl = DBG_ERR;
static unsigned int resume_cnt;


extern void mcd_pm_set_force_crash(bool value);


bool no_host_wakeup = false;
EXPORT_SYMBOL(no_host_wakeup);


int mdm_reset = 0;
EXPORT_SYMBOL(mdm_reset);

int enum_is_processing = 0;
EXPORT_SYMBOL(enum_is_processing);

// SYSFS for tracing MUX TX log [S]
int mux_tx_dump_enable = 0;
EXPORT_SYMBOL(mux_tx_dump_enable);
// SYSFS for tracing MUX TX log [E]

// SYSFS for tracing MUX RX log [S]
int mux_rx_dump_enable = 0;
EXPORT_SYMBOL(mux_rx_dump_enable);
// SYSFS for tracing MUX RX log [E]

// SYSFS for tracing ACM TX log [S]
int acm_tx_dump_enable = 0;
EXPORT_SYMBOL(acm_tx_dump_enable);
// SYSFS for tracing ACM TX log [E]

// SYSFS for tracing ACM RX log [S]
int acm_rx_dump_enable = 0;
EXPORT_SYMBOL(acm_rx_dump_enable);
// SYSFS for tracing ACM RX log [E]

// SYSFS for tracing IPC TX log [S]
int ipc_tx_dump_enable = 0;
EXPORT_SYMBOL(ipc_tx_dump_enable);
// SYSFS for tracing IPC TX log [E]

// SYSFS for tracing IPC RX log [S]
int ipc_rx_dump_enable = 0;
EXPORT_SYMBOL(ipc_rx_dump_enable);
// SYSFS for tracing IPC RX log [E]

int ipc_packet_dump = 1;
EXPORT_SYMBOL(ipc_packet_dump);

int cp_force_crash_enable = 1;
EXPORT_SYMBOL(cp_force_crash_enable);

int xmm7260_core_dump = 0;
EXPORT_SYMBOL(xmm7260_core_dump);

int ldisc_connect_status = 0;
EXPORT_SYMBOL(ldisc_connect_status);

#define FEATURE_RECOVERY_SCHEME
#ifdef FEATURE_RECOVERY_SCHEME
int enable_recovery  = 0;
EXPORT_SYMBOL(enable_recovery);
#endif

#define FEATURE_RECOVERY_CP_CRASH
#ifdef FEATURE_RECOVERY_CP_CRASH
int crash_by_gpio_status  = 0;
EXPORT_SYMBOL(crash_by_gpio_status);
#endif

const char *bus_state_str [] =
{
	"DISCONNECT",
	"SUSPENDED DISCONNECT",
	"CHANGING1",
	"CHANGING2",
	"IDLE",
	"RESET",
	"ENUM DONE",
	"SUSPENDING",
	"SUSPENDED",
	"SUSPEND ERROR",
	"RESUMING",
	"RESUMED",
	"RESUME ERROR",
};

#ifdef DUMP_IPC_PACKET
#define CONTROL_PACKET_LEN    20
#define PRINT_PACKET_LEN	  20
#define MAX_DUMP_LEN		  50
typedef struct
{
    u8 buf[CONTROL_PACKET_LEN];
    u32 len;
    int msec;
}
ipc_dump_struct;

static ipc_dump_struct *ipc_tx_dump[MAX_DUMP_LEN];
static ipc_dump_struct *ipc_rx_dump[MAX_DUMP_LEN];
#endif

static const struct odin_udev_id odin_udev_ids[] = {
#if defined(CONFIG_M7450_HSIC)
	{ .vid = 0x04cc, .pid = 0x0500 },
	{ .vid = 0x04cc, .pid = 0x230f },
	{ .vid = 0x0bdb, .pid = 0x100e },
#endif
#if defined(CONFIG_IMC_MODEM) || defined(CONFIG_IMC_DRV)
	{ .vid = 0x8087, .pid = 0x07ef },
	{ .vid = 0x1519, .pid = 0xf000 },
	{ .vid = 0x1519, .pid = 0x0443 },
	{ .vid = 0x1519, .pid = 0x0452 },
#endif
};

#define COMMON_GINTMSK 0xf0010806
#define HOST_GINTMSK 0x07000238
#define NUSED_GINTMSK 0x50010204
#define CLEAR_AN_GINTSTS 0x00200008

#if defined(CONFIG_IMC_MODEM) || defined(CONFIG_IMC_DRV)
extern int mcd_pm_hsic_postsuspend(bool rt_call);
extern int mcd_pm_hsic_preresume(bool rt_call);
extern int mcd_pm_hsic_phy_ready(bool rt_call);
extern int mcd_pm_hsic_phy_off(bool rt_call);
extern int mcd_pm_hsic_phy_off_done(bool rt_call);
extern int mcd_pm_hostw_handshake(int timeout);

#if defined(PENDING_HW)
extern void mcd_pm_enable_hw_irq(void);
bool check_pending_hw_int();
#endif

bool runtime_call = false;
EXPORT_SYMBOL(runtime_call);

#define RUNTIME_EXEC(is_runtime)\
	(is_runtime)
#endif

#define ENABLE_NCM_TPUT

extern int32_t dwc_otg_handle_mode_mismatch_intr(
			dwc_otg_core_if_t * core_if);
extern int32_t dwc_otg_handle_wakeup_detected_intr(
			dwc_otg_core_if_t * core_if);
extern int32_t dwc_otg_handle_usb_suspend_intr(
			dwc_otg_core_if_t * core_if);
extern void kill_all_urbs(dwc_otg_hcd_t *hcd);

static bool odin_dwc_host_supported_uids(
			const struct usb_device_descriptor *ds)

{
	int i;

	for (i = 0; i < ARRAY_SIZE(odin_udev_ids); i++) {
		if (odin_udev_ids[i].vid == ds->idVendor &&
			odin_udev_ids[i].pid == ds->idProduct) {
			return true;
		}
	}
	return false;
}


int odin_dwc_host_usb_notify(struct notifier_block *nb,
		unsigned long action, void *dev)
{
	struct usb_device *udev = dev;
	const struct usb_device_descriptor *desc = &udev->descriptor;
	struct device *root_hub_dev = NULL;

	switch (action) {
	case USB_DEVICE_ADD:
		if (odin_dwc_host_supported_uids(desc)) {
			udbg(UINFO, "Attached USB Dev VID=0x%.4x, PID=0x%.4x\n",
				desc->idVendor, desc->idProduct);

			odin_ddata->b_state = ENUM_DONE;
			enum_is_processing = 0;

#if defined(CONFIG_IMC_MODEM) || defined(CONFIG_IMC_DRV)
			if (desc->idProduct == 0xf000)
				xmm7260_core_dump = 1;
#endif
			if (udev->product)
				udbg(UINFO, "Product : %s\n", udev->product);
			if (udev->manufacturer)
				udbg(UINFO, "Mfr : %s\n", udev->manufacturer);
			if (udev->serial)
				udbg(UINFO, "SN : %s\n", udev->serial);

#if defined(ODIN_DWC_HOST_FLB_FIXED_MEMCLK)
			if (desc->idProduct == 0x07ef)
				pm_qos_update_request(&flb_mem_qos_req, FLB_MEM_FREQ);
#endif

#if defined(CONFIG_IMC_MODEM) || defined(CONFIG_IMC_DRV)
			root_hub_dev = &odin_ddata->hcd->self.root_hub->dev;
			if (root_hub_dev) {
				if ((desc->idVendor == 0x1519) && (desc->idProduct == 0x0452)) {
					pm_runtime_set_autosuspend_delay(
						root_hub_dev, 0);
				} else {
					pm_runtime_set_autosuspend_delay(
						root_hub_dev, -1);
				}
			}
#endif
		}
		break;
	case USB_DEVICE_REMOVE:
		if (odin_dwc_host_supported_uids(desc)) {
			udbg(UINFO, "Detached USB Dev VID=0x%.4x, PID=0x%.4x (%s)\n",
				desc->idVendor, desc->idProduct,
				bus_state_str[odin_ddata->b_state]);

			enum_is_processing = 0;

#if defined(CONFIG_IMC_MODEM) || defined(CONFIG_IMC_DRV)
			xmm7260_core_dump = 0;
#endif
			if (odin_ddata->b_state == SUSPENDED
				|| odin_ddata->b_state == RESUME_ERR)
				odin_ddata->b_state = SUSPENDED_DISCON;
			else
				odin_ddata->b_state = DISCONNECT;

#if defined(ODIN_DWC_HOST_FLB_FIXED_MEMCLK)
			if (desc->idProduct == 0x07ef)
				pm_qos_update_request(&flb_mem_qos_req, 0);
#endif

#if defined(CONFIG_IMC_MODEM) || defined(CONFIG_IMC_DRV)
			root_hub_dev = &odin_ddata->hcd->self.root_hub->dev;
			if (root_hub_dev)
				pm_runtime_set_autosuspend_delay(
					root_hub_dev, -1);
#endif
		}
		break;
	}

	return NOTIFY_DONE;
}

static int odin_host_handle_discon_intr(dwc_otg_core_if_t *core_if)
{
	gintsts_data_t gintsts;
	dwc_otg_hcd_t *dwc_otg_hcd;
	int ch_num, i;
	dwc_hc_t *ch;
	dwc_otg_hc_regs_t *hc_regs;
	hcchar_data_t hcchar;
	gintsts_data_t intmsk = {.d32 = 0};

	if (core_if->hcd_cb && core_if->hcd_cb->disconnect) {
		dwc_otg_hcd = core_if->hcd_cb->p;
		dwc_otg_hcd->flags.b.port_connect_status_change = 1;
		dwc_otg_hcd->flags.b.port_connect_status = 0;
	}

	ch_num = dwc_otg_hcd->core_if->core_params->host_channels;
	for (i = 0; i < ch_num; i++) {
		ch = dwc_otg_hcd->hc_ptr_array[i];
		if (DWC_CIRCLEQ_EMPTY_ENTRY(ch, hc_list_entry)) {
			hc_regs = dwc_otg_hcd->core_if->host_if->hc_regs[i];
			hcchar.d32 = DWC_READ_REG32(&hc_regs->hcchar);
			if (hcchar.b.chen) {
				hcchar.b.chdis = 1;
				DWC_WRITE_REG32(&hc_regs->hcchar, hcchar.d32);
			}

			dwc_otg_hc_cleanup(dwc_otg_hcd->core_if, ch);
			DWC_CIRCLEQ_INSERT_TAIL(&dwc_otg_hcd->free_hc_list, ch,
			     hc_list_entry);
		}
	}

	DWC_WRITE_REG32(&core_if->core_global_regs->gintsts, 0xffffffff);

	intmsk.b.nptxfempty = 1;
	intmsk.b.ptxfempty = 1;
	intmsk.b.hcintr = 1;
	DWC_MODIFY_REG32(&core_if->core_global_regs->gintmsk, intmsk.d32, 0);

	kill_all_urbs(dwc_otg_hcd);

	core_if->lx_state = DWC_OTG_L3;

	gintsts.d32 = 0;
	gintsts.b.disconnect = 1;
	DWC_WRITE_REG32(&core_if->core_global_regs->gintsts, gintsts.d32);

	return 1;
}

static int odin_host_handle_sof_intr(dwc_otg_hcd_t *hcd)
{
	hfnum_data_t hfnum;
	dwc_list_link_t *qh_entry;
	dwc_otg_qh_t *qh;
	dwc_otg_transaction_type_e tr_type;
	gintsts_data_t gintsts = {.d32 = 0 };

	hfnum.d32 =
	    DWC_READ_REG32(&hcd->core_if->host_if->host_global_regs->hfnum);

	hcd->frame_number = hfnum.b.frnum;

	if (hcd->force_reset_state) {
		udbg(UDBG, "%s : Ignore SOF INT\n", __func__);
		goto done;
	}

	qh_entry = DWC_LIST_FIRST(&hcd->periodic_sched_inactive);
	while (qh_entry != &hcd->periodic_sched_inactive) {
		qh = DWC_LIST_ENTRY(qh_entry, dwc_otg_qh_t, qh_list_entry);
		qh_entry = qh_entry->next;
		if (dwc_frame_num_le(qh->sched_frame, hcd->frame_number)) {
			DWC_LIST_MOVE_HEAD(&hcd->periodic_sched_ready,
					   &qh->qh_list_entry);
		}
	}
	tr_type = dwc_otg_hcd_select_transactions(hcd);
	if (tr_type != DWC_OTG_TRANSACTION_NONE) {
		dwc_otg_hcd_queue_transactions(hcd, tr_type);
	}

done:
	/* Clear interrupt */
	gintsts.b.sofintr = 1;
	DWC_WRITE_REG32(&hcd->core_if->core_global_regs->gintsts, gintsts.d32);

	return 1;
}

static int odin_dwc_host_core_irq_handler(dwc_otg_hcd_t *dhcd)
{
	int ret = IRQ_NONE;
	gintsts_data_t gintsts, gintsts_bu;
	gintmsk_data_t gintmsk;
	dwc_otg_core_global_regs_t *gregs = dhcd->core_if->core_global_regs;

	gintsts_bu.d32 = gintsts.d32 = DWC_READ_REG32(&gregs->gintsts);
	gintmsk.d32 = DWC_READ_REG32(&gregs->gintmsk);
	gintsts.d32 &= gintmsk.d32;

	if (gintsts_bu.b.incomplisoout ||
		(!gintsts.b.sofintr && gintsts_bu.b.sofintr)) {
		/* Clear masked interrupts */
		DWC_WRITE_REG32(&gregs->gintsts, (gintsts_bu.d32 & CLEAR_AN_GINTSTS));
		ret = IRQ_HANDLED;
		udbg(UDBG, "%s : Clear masked interrupt = 0x%x\n", __func__, gintsts_bu.d32);
	}

	if (gintsts.d32 & COMMON_GINTMSK) {
		if (gintsts.b.modemismatch) {
			ret |= dwc_otg_handle_mode_mismatch_intr(dhcd->core_if);
			udbg(UINFO, "Mode mismatch INT\n");
		}

		if (gintsts.b.disconnect) {
			spin_lock((spinlock_t *)dhcd->lock);
			ret |= odin_host_handle_discon_intr(dhcd->core_if);
			spin_unlock((spinlock_t *)dhcd->lock);
			udbg(UINFO, "Disconnect INT\n");
		}

		if (gintsts.b.wkupintr) {
			ret |= dwc_otg_handle_wakeup_detected_intr(
				dhcd->core_if);
			udbg(UINFO, "Wakeup detected INT\n");
		}

		if (gintsts.b.usbsuspend) {
			spin_lock((spinlock_t *)dhcd->lock);
			ret |= dwc_otg_handle_usb_suspend_intr(dhcd->core_if);
			spin_unlock((spinlock_t *)dhcd->lock);
			udbg(UINFO, "USB suspend INT\n");
		}

		if (gintsts.d32 & NUSED_GINTMSK) {
			/* Clear unsupported interrupt */
			DWC_WRITE_REG32
				(&gregs->gintsts, (gintsts.d32 & NUSED_GINTMSK));
			udbg(UINFO, "Unsupported Common INT : 0x%x\n", \
				(gintsts.d32 & NUSED_GINTMSK));
		}
	}

	if (gintsts.d32 & HOST_GINTMSK) {
		if (gintsts.b.sofintr) {
			/* TODO: need check spin lock/unlock usage */
			spin_lock((spinlock_t *)dhcd->lock);
			ret |= odin_host_handle_sof_intr(dhcd);
			spin_unlock((spinlock_t *)dhcd->lock);
		}

		if (gintsts.b.rxstsqlvl) {
			ret |= dwc_otg_hcd_handle_rx_status_q_level_intr(dhcd);
			udbg(UINFO, "rxstsqlvl INT (0x%x)\n", gintsts.d32);
		}

		if (gintsts.b.nptxfempty) {
			ret |= dwc_otg_hcd_handle_np_tx_fifo_empty_intr(dhcd);
			udbg(UINFO, "nptxfempty INT (0x%x)\n", gintsts.d32);
		}

		if (gintsts.b.portintr) {
			ret |= dwc_otg_hcd_handle_port_intr(dhcd);
		}

		if (gintsts.b.hcintr) {
			/* TODO: need check spin lock/unlock */
			spin_lock((spinlock_t *)dhcd->lock);
			ret |= dwc_otg_hcd_handle_hc_intr(dhcd);
			spin_unlock((spinlock_t *)dhcd->lock);
		}

		if (gintsts.b.ptxfempty) {
			ret |= dwc_otg_hcd_handle_perio_tx_fifo_empty_intr(dhcd);
			udbg(UINFO, "ptxfempty INT (0x%x)\n", gintsts.d32);
		}

		if (gintsts.b.i2cintr) {
			/* Clear unsupported interrupt */
			DWC_WRITE_REG32
				(&gregs->gintsts,(gintsts.d32 & NUSED_GINTMSK));
			udbg(UINFO, "Unsupported I2C INT\n");
		}
	}

	if (ret != IRQ_HANDLED)
		udbg(UDBG, "IRQ NONE : 0x%x, 0x%x\n", gintsts_bu.d32, gintmsk.d32);

	return ret;
}

static irqreturn_t odin_dwc_host_core_irq(struct usb_hcd *hcd)
{
	int32_t ret;
	dwc_otg_hcd_t *dwc_otg_hcd =
		((struct wrapper_priv_data *)(hcd->hcd_priv))->dwc_otg_hcd;

	ret = odin_dwc_host_core_irq_handler(dwc_otg_hcd);

	return IRQ_RETVAL(ret);
}

static void odin_dwc_host_set_uninitialized(int32_t *p, int size)
{
	int i;

	for (i = 0; i < size; i++)
		p[i] = -1;
}

static int odin_dwc_host_pre_set_params(dwc_otg_core_if_t *core_if)
{
	udbg(UDBG, "%s\n", __func__);

	odin_dwc_host_set_uninitialized((int32_t *)core_if->core_params,
				  sizeof(*core_if->core_params) /
				  sizeof(int32_t));

	dwc_otg_set_param_otg_cap(core_if,
			DWC_OTG_CAP_PARAM_NO_HNP_SRP_CAPABLE);
	dwc_otg_set_param_dma_enable(core_if,
			dwc_param_dma_enable_default);
	dwc_otg_set_param_dma_desc_enable(core_if, 0);
	dwc_otg_set_param_opt(core_if, dwc_param_opt_default);
	dwc_otg_set_param_dma_burst_size(core_if,
			dwc_param_dma_burst_size_default);
	dwc_otg_set_param_host_support_fs_ls_low_power(core_if,
			dwc_param_host_support_fs_ls_low_power_default);
	dwc_otg_set_param_enable_dynamic_fifo(core_if,
			dwc_param_enable_dynamic_fifo_default);
	dwc_otg_set_param_data_fifo_size(core_if,
			dwc_param_data_fifo_size_default);
	dwc_otg_set_param_host_rx_fifo_size(core_if,
			dwc_param_host_rx_fifo_size_default);
	dwc_otg_set_param_host_nperio_tx_fifo_size(core_if,
			dwc_param_host_nperio_tx_fifo_size_default);
	dwc_otg_set_param_max_transfer_size(core_if,
			dwc_param_max_transfer_size_default);
	dwc_otg_set_param_max_packet_count(core_if,
			dwc_param_max_packet_count_default);
	dwc_otg_set_param_host_channels(core_if,
			dwc_param_host_channels_default);
	dwc_otg_set_param_phy_type(core_if,
			dwc_param_phy_type_default);
	dwc_otg_set_param_speed(core_if, dwc_param_speed_default);
	dwc_otg_set_param_host_ls_low_power_phy_clk(core_if,
			dwc_param_host_ls_low_power_phy_clk_default);
	dwc_otg_set_param_phy_ulpi_ddr(core_if,
			dwc_param_phy_ulpi_ddr_default);
	dwc_otg_set_param_phy_ulpi_ext_vbus(core_if,
			dwc_param_phy_ulpi_ext_vbus_default);
	dwc_otg_set_param_phy_utmi_width(core_if,
			dwc_param_phy_utmi_width_default);
	dwc_otg_set_param_ts_dline(core_if,
			dwc_param_ts_dline_default);
	dwc_otg_set_param_i2c_enable(core_if,
			dwc_param_i2c_enable_default);
	dwc_otg_set_param_ulpi_fs_ls(core_if,
			dwc_param_ulpi_fs_ls_default);
	dwc_otg_set_param_en_multiple_tx_fifo(core_if,
			dwc_param_en_multiple_tx_fifo_default);
	dwc_otg_set_param_thr_ctl(core_if,
			dwc_param_thr_ctl_default);
	dwc_otg_set_param_mpi_enable(core_if,
			dwc_param_mpi_enable_default);
	dwc_otg_set_param_pti_enable(core_if,
			dwc_param_pti_enable_default);
	dwc_otg_set_param_lpm_enable(core_if,
			dwc_param_lpm_enable_default);
	dwc_otg_set_param_besl_enable(core_if,
			dwc_param_besl_enable_default);
	dwc_otg_set_param_baseline_besl(core_if,
			dwc_param_baseline_besl_default);
	dwc_otg_set_param_deep_besl(core_if,
			dwc_param_deep_besl_default);
	dwc_otg_set_param_ic_usb_cap(core_if,
			dwc_param_ic_usb_cap_default);
	dwc_otg_set_param_tx_thr_length(core_if,
			dwc_param_tx_thr_length_default);
	dwc_otg_set_param_rx_thr_length(core_if,
			dwc_param_rx_thr_length_default);
	dwc_otg_set_param_ahb_thr_ratio(core_if,
			dwc_param_ahb_thr_ratio_default);
	dwc_otg_set_param_power_down(core_if,
			dwc_param_power_down_default);
	dwc_otg_set_param_reload_ctl(core_if,
			dwc_param_reload_ctl_default);
	dwc_otg_set_param_cont_on_bna(core_if,
			dwc_param_cont_on_bna_default);
	dwc_otg_set_param_ahb_single(core_if,
			dwc_param_ahb_single_default);
	dwc_otg_set_param_otg_ver(core_if,
			dwc_param_otg_ver_default);
	dwc_otg_set_param_adp_enable(core_if,
			dwc_param_adp_enable_default);

	return 0;
}

int odin_dwc_host_post_set_params(dwc_otg_core_if_t *core_if)
{
	int ret;
	ret = dwc_otg_set_param_host_perio_tx_fifo_size(core_if,
			dwc_param_host_perio_tx_fifo_size_default);

	return ret;
}

int odin_dwc_host_cil_init(dwc_otg_core_if_t *core_if,
			const uint32_t *reg_base_addr)
{
	dwc_otg_host_if_t *host_if = 0;
	uint8_t *reg_base = (uint8_t *)reg_base_addr;
	int i, ret = 0;

	udbg(UDBG, "%s\n", __func__);
	core_if->core_global_regs =
			(dwc_otg_core_global_regs_t *)reg_base;

	host_if = DWC_ALLOC(sizeof(dwc_otg_host_if_t));

	if (!host_if) {
		udbg(UERR, "Allocation of dwc_otg_host_if_t failed\n");
		return -ENOMEM;
	}

	host_if->host_global_regs = (dwc_otg_host_global_regs_t *)
	    (reg_base + DWC_OTG_HOST_GLOBAL_REG_OFFSET);

	host_if->hprt0 =
	    (uint32_t *) (reg_base + DWC_OTG_HOST_PORT_REGS_OFFSET);

	for (i = 0; i < MAX_EPS_CHANNELS; i++) {
		host_if->hc_regs[i] = (dwc_otg_hc_regs_t *)
		    (reg_base + DWC_OTG_HOST_CHAN_REGS_OFFSET +
		     (i * DWC_OTG_CHAN_REGS_OFFSET));
		udbg(UDBG, "hc_reg[%d]->hcchar=%p\n",
			    i, &host_if->hc_regs[i]->hcchar);
	}

	host_if->num_host_channels = MAX_EPS_CHANNELS;
	core_if->host_if = host_if;

	for (i = 0; i < MAX_EPS_CHANNELS; i++) {
		core_if->data_fifo[i] =
			(uint32_t *)(reg_base + DWC_OTG_DATA_FIFO_OFFSET +
				  (i * DWC_OTG_DATA_FIFO_SIZE));
		udbg(UDBG, "data_fifo[%d]=0x%08lx\n",
			    i, (unsigned long)core_if->data_fifo[i]);
	}

	core_if->pcgcctl = (uint32_t *)(reg_base + DWC_OTG_PCGCCTL_OFFSET);

	core_if->lx_state = DWC_OTG_L3;

	core_if->hwcfg1.d32 =
	    DWC_READ_REG32(&core_if->core_global_regs->ghwcfg1);
	core_if->hwcfg2.d32 =
	    DWC_READ_REG32(&core_if->core_global_regs->ghwcfg2);
	core_if->hwcfg3.d32 =
	    DWC_READ_REG32(&core_if->core_global_regs->ghwcfg3);
	core_if->hwcfg4.d32 =
	    DWC_READ_REG32(&core_if->core_global_regs->ghwcfg4);

	udbg(UDBG, "hwcfg1=%08x\n", core_if->hwcfg1.d32);
	udbg(UDBG, "hwcfg2=%08x\n", core_if->hwcfg2.d32);
	udbg(UDBG, "hwcfg3=%08x\n", core_if->hwcfg3.d32);
	udbg(UDBG, "hwcfg4=%08x\n", core_if->hwcfg4.d32);

	core_if->hcfg.d32 =
		DWC_READ_REG32(&core_if->host_if->host_global_regs->hcfg);

	udbg(UDBG, "hcfg=%08x\n", core_if->hcfg.d32);
	udbg(UDBG, "op_mode=%0x\n", core_if->hwcfg2.b.op_mode);
	udbg(UDBG, "architecture=%0x\n", core_if->hwcfg2.b.architecture);
	udbg(UDBG, "num_dev_ep=%d\n", core_if->hwcfg2.b.num_dev_ep);
	udbg(UDBG, "num_host_chan=%d\n",
			core_if->hwcfg2.b.num_host_chan);
	udbg(UDBG, "nonperio_tx_q_depth=0x%0x\n",
			core_if->hwcfg2.b.nonperio_tx_q_depth);
	udbg(UDBG, "host_perio_tx_q_depth=0x%0x\n",
			core_if->hwcfg2.b.host_perio_tx_q_depth);
	udbg(UDBG, "Total FIFO SZ=%d\n",
			core_if->hwcfg3.b.dfifo_depth);
	udbg(UDBG, "xfer_size_cntr_width=%0x\n",
			core_if->hwcfg3.b.xfer_size_cntr_width);

	core_if->srp_success = 0;
	core_if->srp_timer_started = 0;

	core_if->wq_otg = DWC_WORKQ_ALLOC("dwc_otg");
	if (core_if->wq_otg == 0) {
		udbg(UERR, "DWC_WORKQ_ALLOC failed\n");
		ret = -ENOMEM;
		goto host_mem_free;
	}

	core_if->snpsid =
		DWC_READ_REG32(&core_if->core_global_regs->gsnpsid);

	udbg(UDBG, "Core Release: %x.%x%x%x\n",
		(core_if->snpsid >> 12 & 0xF), (core_if->snpsid >> 8 & 0xF),
		(core_if->snpsid >> 4 & 0xF), (core_if->snpsid & 0xF));

	core_if->wkp_timer = DWC_TIMER_ALLOC("Wake Up Timer",
			w_wakeup_detected, core_if);
	if (core_if->wkp_timer == 0) {
		udbg(UERR, "DWC_TIMER_ALLOC failed\n");
		ret = -ENOMEM;
		goto wq_otg_free;
	}

	core_if->core_params = DWC_ALLOC(sizeof(dwc_otg_core_params_t));
	if (!core_if->core_params) {
		udbg(UERR, "Allocation of core_params failed\n");
		ret = -ENOMEM;
		goto wkp_timer_free;
	}

	if (odin_dwc_host_pre_set_params(core_if)) {
		udbg(UERR, "Error while setting core params\n");
		ret = -EINVAL;
		goto param_mem_free;
	}

	core_if->hibernation_suspend = 0;
	core_if->test_mode = 0;

#ifdef DUMP_IPC_PACKET
	for(i=0; i < MAX_DUMP_LEN; i++){
        ipc_tx_dump[i] =
            (ipc_dump_struct *) kmalloc(sizeof(ipc_dump_struct), GFP_KERNEL);
        if(ipc_tx_dump[i] == NULL)
            printk("%s error during kmalloc for tx dump\n", __func__);

        ipc_rx_dump[i] =
            (ipc_dump_struct *) kmalloc(sizeof(ipc_dump_struct), GFP_KERNEL);
        if(ipc_rx_dump[i] == NULL)
            printk("%s error during kmalloc for rx dump\n", __func__);
	}
#endif
	return ret;

param_mem_free:
	DWC_FREE(core_if->core_params);
wkp_timer_free:
	DWC_TIMER_FREE(core_if->wkp_timer);
wq_otg_free:
	DWC_WORKQ_FREE(core_if->wq_otg);
host_mem_free:
	DWC_FREE(host_if);

	return ret;
}

static void odin_dwc_host_core_reset(dwc_otg_core_if_t *core_if, int ms)
{
	dwc_otg_core_global_regs_t *global_regs =
			core_if->core_global_regs;
	volatile grstctl_t greset = {.d32 = 0 };
	int count = 0;

	do {
		udelay(10);
		greset.d32 = DWC_READ_REG32(&global_regs->grstctl);
		if (++count > 100000) {
			udbg(UERR, "HANG! AHB Idle GRSTCTL=0x%0x\n",
				 greset.d32);
			return;
		}
	}
	while (greset.b.ahbidle == 0);

	count = 0;
	greset.b.csftrst = 1;
	DWC_WRITE_REG32(&global_regs->grstctl, greset.d32);
	do {
		greset.d32 = DWC_READ_REG32(&global_regs->grstctl);
		if (++count > 10000) {
			udbg(UERR, "HANG! Soft Reset GRSTCTL=%0x\n",
			greset.d32);
			break;
		}
		udelay(1);
	}
	while (greset.b.csftrst == 1);

	if (ms)
		mdelay(ms);
}

void odin_dwc_host_core_init1(dwc_otg_core_if_t *core_if)
{
	dwc_otg_core_global_regs_t *global_regs = core_if->core_global_regs;
	gusbcfg_data_t usbcfg = {.d32 = 0 };

	udbg(UDBG, "%s\n", __func__);

	/* Common Initialization */
	usbcfg.d32 = DWC_READ_REG32(&global_regs->gusbcfg);

	/* Program the ULPI External VBUS bit if needed */
	usbcfg.b.ulpi_ext_vbus_drv =
	    (core_if->core_params->phy_ulpi_ext_vbus ==
	     DWC_PHY_ULPI_EXTERNAL_VBUS) ? 1 : 0;

	/* Set external TS Dline pulsing */
	usbcfg.b.term_sel_dl_pulse =
	    (core_if->core_params->ts_dline == 1) ? 1 : 0;
	DWC_WRITE_REG32(&global_regs->gusbcfg, usbcfg.d32);

	core_if->adp_enable = core_if->core_params->adp_supp_enable;
	core_if->power_down = core_if->core_params->power_down;

	core_if->total_fifo_size = core_if->hwcfg3.b.dfifo_depth;
	core_if->rx_fifo_size = DWC_READ_REG32(&global_regs->grxfsiz);
	core_if->nperio_tx_fifo_size =
	    DWC_READ_REG32(&global_regs->gnptxfsiz) >> 16;

	if (!core_if->phy_init_done) {
		core_if->phy_init_done = 1;
		 if (core_if->core_params->phy_type == 1 &&
			core_if->core_params->phy_utmi_width == 16)
			usbcfg.b.phyif = 1;
		 else
		 	udbg(UINFO, "Not PHY for ODIN-DWC\n");

		 DWC_WRITE_REG32(&global_regs->gusbcfg, usbcfg.d32);

		 odin_dwc_host_core_reset(core_if, 0);
	} else {
		udbg(UINFO, "PHY is already intialized\n");
	}
}

void odin_dwc_host_core_init2(dwc_otg_core_if_t *core_if)
{
	dwc_otg_core_global_regs_t *global_regs =
			core_if->core_global_regs;
	gahbcfg_data_t ahbcfg = {.d32 = 0 };
	gusbcfg_data_t usbcfg = {.d32 = 0 };
	gotgctl_data_t gotgctl = {.d32 = 0 };

	udbg(UDBG, "%s\n", __func__);

	if (core_if->hwcfg2.b.hs_phy_type == 1 &&
		!core_if->core_params->ulpi_fs_ls) {
		usbcfg.d32 = DWC_READ_REG32(&global_regs->gusbcfg);
		usbcfg.b.ulpi_fsls = 0;
		usbcfg.b.ulpi_clk_sus_m = 0;
		DWC_WRITE_REG32(&global_regs->gusbcfg, usbcfg.d32);
	} else {
		udbg(UINFO, "hs/fs phy_type = %d/%d, ulpi_fs_ls = %d\n",
		core_if->hwcfg2.b.hs_phy_type, core_if->hwcfg2.b.fs_phy_type,
		core_if->core_params->ulpi_fs_ls);
	}

	if (core_if->hwcfg2.b.architecture == DWC_INT_DMA_ARCH) {
		ahbcfg.b.hburstlen = DWC_GAHBCFG_INT_DMA_BURST_INCR4;
		core_if->dma_enable = core_if->core_params->dma_enable;
		core_if->dma_desc_enable =
			core_if->core_params->dma_desc_enable;
	} else {
		udbg(UINFO, "arch : %d\n", core_if->hwcfg2.b.architecture);
	}

	if (core_if->core_params->ahb_single) {
		udbg(UINFO, "AHB Single is supported\n");
		ahbcfg.b.ahbsingle = 1;
	}

	ahbcfg.b.dmaenable = core_if->dma_enable;
	DWC_WRITE_REG32(&global_regs->gahbcfg, ahbcfg.d32);

	core_if->en_multiple_tx_fifo = core_if->hwcfg4.b.ded_fifo_en;

	core_if->pti_enh_enable = core_if->core_params->pti_enable;
	core_if->multiproc_int_enable = core_if->core_params->mpi_enable;

	usbcfg.d32 = DWC_READ_REG32(&global_regs->gusbcfg);

	if (core_if->hwcfg2.b.op_mode == DWC_MODE_HNP_SRP_CAPABLE) {
		usbcfg.b.hnpcap = (core_if->core_params->otg_cap ==
				   DWC_OTG_CAP_PARAM_HNP_SRP_CAPABLE);
		usbcfg.b.srpcap = (core_if->core_params->otg_cap !=
				   DWC_OTG_CAP_PARAM_NO_HNP_SRP_CAPABLE);
	} else {
		udbg(UINFO, "op_mode = %d\n", core_if->hwcfg2.b.op_mode);
	}

	DWC_WRITE_REG32(&global_regs->gusbcfg, usbcfg.d32);

	if (core_if->core_params->ic_usb_cap) {
		gusbcfg_data_t gusbcfg = {.d32 = 0 };
		gusbcfg.b.ic_usb_cap = 1;
		udbg(UINFO, "ic_usb_cap is supported\n");
		DWC_MODIFY_REG32(&core_if->core_global_regs->gusbcfg,
			0, gusbcfg.d32);
	}

	gotgctl.b.otgver = core_if->core_params->otg_ver;
	DWC_MODIFY_REG32(&core_if->core_global_regs->gotgctl, 0,
			gotgctl.d32);
	core_if->otg_ver = core_if->core_params->otg_ver;

	udbg(UDBG, "otg version = %d\n", core_if->otg_ver);

	core_if->op_state = A_HOST;
}

void odin_dwc_host_cil_remove(dwc_otg_core_if_t *core_if)
{
#ifdef DUMP_IPC_PACKET
    int i;
#endif
	udbg(UDBG, "%s\n", __func__);

	DWC_MODIFY_REG32(&core_if->core_global_regs->gahbcfg, 1, 0);
	DWC_WRITE_REG32(&core_if->core_global_regs->gintmsk, 0);

	if (core_if->wq_otg) {
		DWC_WORKQ_WAIT_WORK_DONE(core_if->wq_otg, 500);
		DWC_WORKQ_FREE(core_if->wq_otg);
	}

	if (core_if->host_if)
		DWC_FREE(core_if->host_if);

	if (core_if->core_params)
		DWC_FREE(core_if->core_params);

	if (core_if->wkp_timer)
		DWC_TIMER_FREE(core_if->wkp_timer);

	if (core_if->srp_timer)
		DWC_TIMER_FREE(core_if->srp_timer);

	DWC_FREE(core_if);
#ifdef DUMP_IPC_PACKET
	for(i=0; i < MAX_DUMP_LEN; i++){
		kfree(ipc_tx_dump[i]);
		kfree(ipc_rx_dump[i]);
	}
#endif
}

void odin_dwc_host_set_host_mode(dwc_otg_core_if_t *core_if)
{
	gusbcfg_data_t usbcfg;

	udbg(UDBG, "%s\n", __func__);

	usbcfg.d32 = DWC_READ_REG32(&core_if->core_global_regs->gusbcfg);
	usbcfg.b.force_host_mode  = 1;

	DWC_WRITE_REG32(&core_if->core_global_regs->gusbcfg,
			usbcfg.d32);
}

void odin_dwc_host_post_set_param(dwc_otg_core_if_t *core_if)
{
	core_if->hptxfsiz.d32 =
			DWC_READ_REG32(&core_if->core_global_regs->hptxfsiz);

	udbg(UDBG, "%s: hptxfsiz=%x\n", __func__, core_if->hptxfsiz.d32);

	dwc_otg_set_param_host_perio_tx_fifo_size(core_if,
			dwc_param_host_perio_tx_fifo_size_default);
}

static void odin_hsusb_host_suspend(dwc_otg_core_if_t *core_if)
{
	dwc_otg_core_global_regs_t *gregs = core_if->core_global_regs;
	gahbcfg_data_t ahbcfg = {.d32 = 0x1 };


	DWC_MODIFY_REG32(&gregs->gahbcfg, ahbcfg.d32, 0);
	DWC_WRITE_REG32(&gregs->gintmsk, 0);
}

static void odin_hsusb_host_resume(dwc_otg_core_if_t *core_if, bool is_runtime)
{
	int i, num_channels;
	dwc_otg_core_global_regs_t *gregs = core_if->core_global_regs;
	gahbcfg_data_t ahbcfg = {.d32 = 0 };
	gintmsk_data_t intmask = {.d32 = 0 };
	dwc_otg_host_if_t *host_if = core_if->host_if;
	hprt0_data_t hprt0 = {.d32 = 0 };
	hcchar_data_t hcchar;
	dwc_otg_hc_regs_t *hc_regs;
	gusbcfg_data_t usbcfg;

	if (is_runtime)
		goto runtime;

	usbcfg.d32 = DWC_READ_REG32(&gregs->gusbcfg);
	usbcfg.b.force_host_mode  = 1;
	DWC_WRITE_REG32(&gregs->gusbcfg, usbcfg.d32);

	odin_dwc_host_core_reset(core_if, 100);

	ahbcfg.b.hburstlen = DWC_GAHBCFG_INT_DMA_BURST_INCR4;
	ahbcfg.b.dmaenable = core_if->dma_enable;
	DWC_WRITE_REG32(&gregs->gahbcfg, ahbcfg.d32);

	dwc_otg_flush_tx_fifo(core_if, 0x10 /* all TX FIFOs */);
	dwc_otg_flush_rx_fifo(core_if);

	num_channels = core_if->core_params->host_channels;

	for (i = 0; i < num_channels; i++) {
		hc_regs = core_if->host_if->hc_regs[i];
		hcchar.d32 = DWC_READ_REG32(&hc_regs->hcchar);
		hcchar.b.chen = 0;
		hcchar.b.chdis = 1;
		hcchar.b.epdir = 0;
		DWC_WRITE_REG32(&hc_regs->hcchar, hcchar.d32);
	}

	for (i = 0; i < num_channels; i++) {
		int count = 0;
		hc_regs = core_if->host_if->hc_regs[i];
		hcchar.d32 = DWC_READ_REG32(&hc_regs->hcchar);
		hcchar.b.chen = 1;
		hcchar.b.chdis = 1;
		hcchar.b.epdir = 0;
		DWC_WRITE_REG32(&hc_regs->hcchar, hcchar.d32);
		do {
			hcchar.d32 = DWC_READ_REG32(&hc_regs->hcchar);
			if (++count > 1000) {
				udbg(UERR, "Unable to clear halt on channel %d\n", i);
				break;
			}
			udelay(1);
		} while (hcchar.b.chen);
	}

	hprt0.d32 = dwc_otg_read_hprt0(core_if);

	hprt0.b.prtconndet = 1;
	hprt0.b.prtenchng = 1;
	hprt0.b.prtpwr = 1;
	DWC_WRITE_REG32(host_if->hprt0, hprt0.d32);

runtime:

	DWC_WRITE_REG32(&gregs->gintsts, 0xffffffff);

	/* Unmask Interrupts */
	intmask.b.modemismatch = 1;
	intmask.b.wkupintr = 1;
	intmask.b.usbsuspend = 1;
	intmask.b.disconnect = 1;
	intmask.b.portintr = 1;
	intmask.b.hcintr = 1;

	DWC_WRITE_REG32(&gregs->gintmsk, intmask.d32);

	ahbcfg.b.glblintrmsk = 1;
	DWC_MODIFY_REG32(&gregs->gahbcfg, 0, ahbcfg.d32);
}

static int odin_dwc_host_wait_ch_release(dwc_otg_hcd_t *phcd)
{
	int i, ret = -1;

	udbg(UINFO, "Wait until channel is released (%d, %d)\n",
		phcd->non_periodic_channels, phcd->periodic_channels);
	for (i = 0; i < 200 ; i++) { /* timeout 200msec */
		mdelay(1);
		if (!phcd->non_periodic_channels && !phcd->periodic_channels) {
			ret = 0;
			break;
		}
	}
	udbg(UINFO, "%s : %d msec\n", __func__, i+1);

	return ret;
}

static int odin_dwc_host_bus_suspend(struct usb_hcd *hcd)
{
	int ret;

	struct device *dev = hcd->self.controller;
	odin_dwc_ddata_t *ddata = dev_get_drvdata(dev);
	dwc_otg_hcd_t *phcd = ddata->dwc_dev.hcd;
	dwc_otg_core_if_t *core_if;
	bool is_runtime = runtime_call;

	udbg(UDBG, "%s\n", __func__);

	enum_is_processing = 0;

	if (ddata->b_state < ENUM_DONE) {
		udbg(UERR, "%s : Ignore Suspend, current bus is %s\n", __func__,
			bus_state_str[ddata->b_state]);
		return 0;
	}

	ddata->b_state = SUSPENDING;

	if (RUNTIME_EXEC(is_runtime)) {
		udbg(UINFO, "%s RUNTIME suspend.\n", __func__);
	} else {
		udbg(UINFO, "%s SYSTEM suspend.\n", __func__);
	}

	core_if = ddata->dwc_dev.core_if;


#if defined(CONFIG_IMC_MODEM) || defined(CONFIG_IMC_DRV)
	ret = mcd_pm_hsic_phy_off(is_runtime);
	if (ret == -EBUSY) {
		udbg(UERR, "%s : Device is busy.\n", __func__);
		goto error;
	}
#endif

	if (phcd->non_periodic_channels || phcd->periodic_channels) {
		ret = odin_dwc_host_wait_ch_release(phcd);
		if (ret < 0)
			udbg(UERR, "(%s) Channel is still blocked (%d, %d)\n",
				is_runtime ? "Runtime" : "System",
				phcd->non_periodic_channels, phcd->periodic_channels);
	}
	ddata->b_state = SUSPENDING;

#if defined(CONFIG_IMC_MODEM) || defined(CONFIG_IMC_DRV)
	if (RUNTIME_EXEC(is_runtime)) {
		odin_hsusb_host_suspend(core_if);
		disable_irq(hcd->irq);
		odin_dwc_host_clk_disable(ddata);
		ret = odin_pd_off(PD_ICS, 1);
		if (ret < 0) {
			udbg(UERR, "ICS PD1 Off failed : %d", ret);
			goto error;
		}
		mcd_pm_hsic_phy_off_done(is_runtime);
#if defined(PENDING_HW)
		mcd_pm_enable_hw_irq();
#endif
	} else {
		odin_hsusb_host_suspend(core_if);

		disable_irq(hcd->irq);
		printk("%s: Bus Disconnect.\n", __func__);
		dwc_otg_set_hsic_connect(core_if, 0);

		odin_dwc_host_clk_disable(ddata);

		ret = odin_pd_off(PD_ICS, 1);
		if (ret < 0) {
			udbg(UERR, "ICS PD1 Off failed : %d", ret);
			goto error;
		}

		mcd_pm_hsic_phy_off_done(is_runtime);

#if defined(PENDING_HW)
		check_pending_hw_int();
		mcd_pm_enable_hw_irq();
#endif

	}
#else
	odin_hsusb_host_suspend(core_if);

	disable_irq(hcd->irq);

	odin_dwc_host_clk_disable(ddata);

	ret = odin_dwc_host_ldo_disable(ddata);
	if (ret)
		goto error;

	ret = odin_pd_off(PD_ICS, 1);
	if (ret < 0) {
		udbg(UERR, "ICS PD1 Off failed : %d", ret);
		goto error;
	}
#endif

	ddata->b_state = SUSPENDED;
	return 0;

error:
	ddata->b_state = SUSPEND_ERR;
	udbg(UERR, "dwc otg bus suspend failed : %d", ret);

	return ret;
}

static int odin_dwc_hprt_handshake(dwc_otg_core_if_t *core_if, dwc_otg_hcd_t * hcd)
{
	uint32_t i;
	hprt0_data_t hprt0 = {.d32 = 0 };

	for (i = 0 ; i < 100000; i++) { /* Max timeout 1000 msec */
		hprt0.d32 = DWC_READ_REG32(core_if->host_if->hprt0);

		if (hprt0.b.prtconnsts
			&& hcd->flags.b.port_connect_status == 1
			&& hcd->flags.b.port_connect_status_change == 1)
			return 0;

		udelay(10);
	}

	return -1;
}

static int odin_dwc_host_bus_resume(struct usb_hcd *hcd)
{
	int ret;
	struct device *dev = hcd->self.controller;
	odin_dwc_ddata_t *ddata = dev_get_drvdata(dev);
	dwc_otg_core_if_t *core_if;
	dwc_otg_hcd_t *dwc_otg_hcd;
	hprt0_data_t hprt0 = {.d32 = 0 };
#if defined(ODIN_DWC_HOST_SET_CPU_AFFINITY)
	cpumask_var_t cpumask;
#endif

	bool is_runtime;

	enum_is_processing = 0;

	if (ddata->b_state < ENUM_DONE) {
		udbg(UERR, "%s : Ignore Resume, current bus is %s\n", __func__,
			bus_state_str[ddata->b_state]);
		return 0;
	}

	ddata->b_state = RESUMING;

	is_runtime = runtime_call;

	udbg(UINFO, "%s (%d)\n", __func__, resume_cnt++);

	if (!ddata) {
		udbg(UERR, "%s : drvdata is WRONG.\n", __func__);
		ret = -EPERM;
		goto error;
	}

	if (RUNTIME_EXEC(is_runtime)) {
		udbg(UINFO, "%s RUNTIME resume.\n", __func__);
	} else {
		udbg(UINFO, "%s SYSTEM resume.\n", __func__);

#if defined(ODIN_DWC_HOST_SET_CPU_AFFINITY)
		cpumask_clear(cpumask);
		cpumask_set_cpu(IPC_CPU_AFFINITY_NUM, cpumask);
		irq_set_affinity(ddata->odin_dev.irq, cpumask);
#endif
	}

	core_if = ddata->dwc_dev.core_if;
	dwc_otg_hcd = ddata->dwc_dev.hcd;

#if defined(CONFIG_IMC_MODEM) || defined(CONFIG_IMC_DRV)
	ret = mcd_pm_hsic_preresume(is_runtime);
	if (ret) {
		udbg(UERR, "%s : mcd_pm_hsic_preresume Error.\n", __func__);
		goto error;
	}
#endif

#if defined(CONFIG_IMC_MODEM) || defined(CONFIG_IMC_DRV)
	if (RUNTIME_EXEC(is_runtime)) {
		if (mcd_pm_hostw_handshake(1500)) {
			udbg(UERR, "Host wakeup handshake Error.\n");
		}

		ret = odin_pd_on(PD_ICS, 1);
		if (ret < 0) {
			udbg(UERR, "ICS PD1 On failed : %d", ret);
			goto error;
		}

		ret = odin_dwc_host_clk_enable(ddata);
		if (ret)
			goto error;

		odin_hsusb_host_resume(core_if, true);

		enable_irq(hcd->irq);
	} else {
		if (mcd_pm_hostw_handshake(1500)) {
			udbg(UERR, "Host wakeup handshake Error.\n");

		}

		ret = odin_pd_on(PD_ICS, 1);
		if (ret < 0) {
			udbg(UERR, "ICS PD1 On failed : %d", ret);
			goto error;
		}

		ret = odin_dwc_host_clk_enable(ddata);
		if (ret)
			goto error;

		odin_dwc_host_reset(ddata, false);

		odin_hsusb_host_resume(core_if, false);

		enable_irq(hcd->irq);
	}
#else
	ret = odin_dwc_host_ldo_enable(ddata);
	if (ret)
		goto error;

	ret = odin_dwc_host_clk_enable(ddata);
	if (ret)
		goto error;

	ret = odin_pd_on(PD_ICS, 1);
	if (ret < 0) {
		udbg(UERR, "ICS PD1 On failed : %d", ret);
		goto error;
	}

	odin_dwc_host_reset(ddata, false);

	odin_hsusb_host_resume(core_if, false);
	enable_irq(hcd->irq);
#endif

#if defined(CONFIG_IMC_MODEM) || defined(CONFIG_IMC_DRV)

	mcd_pm_hsic_phy_ready(is_runtime);
#endif

	/* Check handshake from L3 state.
	 * In case IPC resume from L2 state, handshake is not needed.
	 */
	if (RUNTIME_EXEC(is_runtime)) {
		/* Doesn't need the handshake for CONNECT signal on Runtime PM. */
	} else {
		udbg(UINFO, "Handshake state. (%d)\n", ret);

		ret = odin_dwc_hprt_handshake(core_if, dwc_otg_hcd);

		hprt0.d32 = DWC_READ_REG32(core_if->host_if->hprt0);
		if (ret) {
			udbg(UERR, "No attached device : 0x%x (%d, %d)\n",
				hprt0.d32, dwc_otg_hcd->flags.b.port_connect_status,
				dwc_otg_hcd->flags.b.port_connect_status_change);
		} else {
			udbg(UINFO, "CONNECT device. 0x%x\n", hprt0.d32);
			enum_is_processing = 1;
		}
	}

	ddata->b_state = RESUMED;

	if(no_host_wakeup) {
#if defined(NO_HOSTWAKEUP_DUMP)
		if (ret == -ENODEV)
			mcd_pm_set_force_crash(true);
#endif
		else {
			no_host_wakeup = false;
			udbg(UERR, "AP Not detects Host Wakeup\n");
		}
	}


	ddata->b_state = RESUMED;
	return 0;

error:
	ddata->b_state = RESUME_ERR;
	udbg(UERR, "dwc otg bus resume failed : %d.\n", ret);

	return ret;
}

static void odin_dwc_host_endpoint_reset
		(struct usb_hcd *hcd, struct usb_host_endpoint *ep)
{
	dwc_irqflags_t flags;
	struct usb_device *udev = hcd->self.root_hub;
	dwc_otg_hcd_t *dwc_otg_hcd =
		((struct wrapper_priv_data *)(hcd->hcd_priv))->dwc_otg_hcd;

	int epnum = usb_endpoint_num(&ep->desc);
	int is_out = usb_endpoint_dir_out(&ep->desc);
	int is_control = usb_endpoint_xfer_control(&ep->desc);

	if (!udev) {
		udbg(UERR, "%s : udev is NULL\n", __func__);
		return ;
	}

	udbg(UDBG, "ODIN DWC HCD EP Reset : EP num = 0x%02d\n", epnum);

	DWC_SPINLOCK_IRQSAVE(dwc_otg_hcd->lock, &flags);

	usb_settoggle(udev, epnum, is_out, 0);
	if (is_control)
		usb_settoggle(udev, epnum, !is_out, 0);

	if (ep->hcpriv)
		dwc_otg_hcd_endpoint_reset(dwc_otg_hcd, ep->hcpriv);

	DWC_SPINUNLOCK_IRQRESTORE(dwc_otg_hcd->lock, flags);
}

int odin_dwc_host_hcd_init(struct device *dev, odin_dwc_ddata_t *ddata)
{
	struct usb_hcd *hcd;
	dwc_otg_hcd_t *dwc_hcd;
	struct usb_device *root_hub;
	struct hc_driver *hc_drv;
	dwc_otg_device_t *dwc_dev = &ddata->dwc_dev;
	int ret = 0;
#if defined(ODIN_DWC_HOST_SET_CPU_AFFINITY)
	cpumask_var_t cpumask;
#endif

	udbg(UDBG, "%s\n", __func__);

	odin_ddata = ddata;

	hc_drv = odin_dwc_host_get_hc_driver();
	if (!hc_drv) {
		udbg(UERR, "host driver is NULL\n");
		return -ENOMEM;
	}

	hc_drv->irq = odin_dwc_host_core_irq;
	hc_drv->bus_suspend = odin_dwc_host_bus_suspend;
	hc_drv->bus_resume = odin_dwc_host_bus_resume;
	hc_drv->endpoint_reset = odin_dwc_host_endpoint_reset;

	hcd = usb_create_hcd(hc_drv, dev, dev_name(dev));
	if (!hcd) {
		udbg(UERR, "failed to create hcd\n");
		return -ENOMEM;
	}

	hcd->has_tt = 1; /* Transaction Translator */
	hcd->regs = dwc_dev->base;

	dwc_hcd = DWC_ALLOC(sizeof(dwc_otg_hcd_t));
	if (!dwc_hcd) {
		udbg(UERR, "failed to allocate hcd\n");
		ret = -ENOMEM;
		goto put_hcd;
	}

	((struct wrapper_priv_data *)(hcd->hcd_priv))->dwc_otg_hcd =
			dwc_hcd;

	dwc_dev->hcd = dwc_hcd;

	ret = dwc_otg_hcd_init(dwc_hcd, dwc_dev->core_if);
	if (ret)
		goto put_hcd;

	dwc_dev->hcd->otg_dev = dwc_dev;

	hcd->self.otg_port = dwc_otg_hcd_otg_port(dwc_hcd);

	ret = usb_add_hcd(hcd, ddata->odin_dev.irq, IRQF_DISABLED);

	if (ret) {
		udbg(UERR, "failed to add hcd (%d)\n", ret);
		goto put_hcd;
	}

	dwc_hcd->priv = hcd;

	root_hub = hcd->self.root_hub;
	pm_runtime_set_autosuspend_delay(&root_hub->dev, 50000);

	/* driver data will be save at init done state with hcd info */
	ddata->hcd = hcd;

#if defined(CONFIG_IMC_MODEM) || defined(CONFIG_IMC_DRV)
	mcd_pm_hsic_phy_ready(false);
#endif

#if defined(ODIN_DWC_HOST_SET_CPU_AFFINITY)
	cpumask_clear(cpumask);
	cpumask_set_cpu(IPC_CPU_AFFINITY_NUM, cpumask);
	irq_set_affinity(ddata->odin_dev.irq, cpumask);
	udbg(UDBG, "Set affinity for HSIC IRQ%d (CPU%d)\n",
			ddata->odin_dev.irq, IPC_CPU_AFFINITY_NUM);
#endif

#if defined(ODIN_DWC_HOST_FLB_FIXED_MEMCLK)
	pm_qos_add_request(&flb_mem_qos_req, PM_QOS_ODIN_MEM_MIN_FREQ, 0);
#endif

	return ret;

put_hcd:
	usb_put_hcd(hcd);

	return ret;
}

void odin_dwc_host_hcd_remove(struct dwc_otg_hcd *dwc_hcd)
{
	struct usb_hcd *hcd;

	udbg(UDBG, "%s\n", __func__);

	if (!dwc_hcd) {
		udbg(UERR, "dwc hcd is NULL\n");
		return;
	}

	hcd = (struct usb_hcd *)dwc_hcd->priv;
	if (!hcd) {
		udbg(UERR, "hcd is NULL\n");
		return;
	}

#if defined(CONFIG_IMC_MODEM) || defined(CONFIG_IMC_DRV)
	mcd_pm_hsic_phy_off(false);
	mcd_pm_hsic_phy_off_done(false);
#endif

	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);
	dwc_otg_hcd_set_priv_data(dwc_hcd, NULL);
	dwc_otg_hcd_remove(dwc_hcd);

#if defined(ODIN_DWC_HOST_FLB_FIXED_MEMCLK)
	pm_qos_remove_request(&flb_mem_qos_req);
#endif
}

/* Resume count */
static ssize_t resume_cnt_show(struct device *pdev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", resume_cnt);
}

static DEVICE_ATTR(resume_cnt, S_IRUGO,
			resume_cnt_show, NULL);

/* DWC debug log level */
static ssize_t dbg_lvl_show(struct device *pdev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%0x\n", g_dbg_lvl);
}

static ssize_t dbg_lvl_store(struct device *pdev,
			struct device_attribute *attr, const char *buf, size_t n)
{
	g_dbg_lvl = simple_strtoul(buf, NULL, 16);

	return n;
}
static DEVICE_ATTR(dbg_lvl, S_IRUGO | S_IWUSR,
			dbg_lvl_show, dbg_lvl_store);

/* Dump global & host register */
static ssize_t regdump_show(struct device *pdev,
			struct device_attribute *attr, char *buf)
{
	dwc_otg_dump_global_registers(odin_ddata->dwc_dev.core_if);
	dwc_otg_dump_host_registers(odin_ddata->dwc_dev.core_if);

	return sprintf(buf, "Register Dump\n");
}
DEVICE_ATTR(regdump, S_IRUGO, regdump_show, NULL);

/* Dump SPRAM */
static ssize_t spramdump_show(struct device *pdev,
			struct device_attribute *attr, char *buf)
{
	dwc_otg_dump_spram(odin_ddata->dwc_dev.core_if);
	return sprintf(buf, "SPRAM Dump\n");
}
DEVICE_ATTR(spramdump, S_IRUGO, spramdump_show, NULL);

/* Dump HCD state */
static ssize_t hcddump_show(struct device *pdev,
			struct device_attribute *attr, char *buf)
{
	dwc_otg_hcd_dump_state(odin_ddata->dwc_dev.hcd);
	return sprintf(buf, "HCD Dump\n");
}
DEVICE_ATTR(hcddump, S_IRUGO, hcddump_show, NULL);

static ssize_t mdm_reset_enable_show(struct device *pdev, struct device_attribute *attr, char *buf){
        return sprintf(buf, "mdm_reset_enable = [%d] \n",mdm_reset);
}

static ssize_t mdm_reset_enable_store(struct device *pdev,struct device_attribute *attr, const char *buf, size_t n){
        int enable = 0;
        if(sscanf(buf, "%d", &enable) != 1)
                return -EINVAL;
        mdm_reset = enable;
        printk("%s] mdm_reset_enable = [%d]\n", __func__, mdm_reset);
        return n;
}
static DEVICE_ATTR(mdm_reset_enable, S_IRUGO | S_IWUSR, mdm_reset_enable_show, mdm_reset_enable_store);

// SYSFS for tracing MUX TX log [S]
static ssize_t mux_tx_enable_show(struct device *pdev, struct device_attribute *attr, char *buf){
    return sprintf(buf, "mux_tx_enable = [%d] \n",mux_tx_dump_enable);
}

static ssize_t mux_tx_enable_store(struct device *pdev,struct device_attribute *attr, const char *buf, size_t n){
    int enable = 0;

	sscanf(buf, "%d", &enable);
    if(enable != 0 && enable != 1)
        return -EINVAL;

    mux_tx_dump_enable = enable;
    printk("acm_tx_enable = [%d]\n", mux_tx_dump_enable);
    return n;
}
static DEVICE_ATTR(mux_tx_enable, S_IRUGO | S_IWUSR, mux_tx_enable_show, mux_tx_enable_store);
// SYSFS for tracing MUX TX log [E]

// SYSFS for tracing MUX RX log [S]
static ssize_t mux_rx_enable_show(struct device *pdev, struct device_attribute *attr, char *buf){
    return sprintf(buf, "mux_rx_enable = [%d] \n",mux_rx_dump_enable);
}

static ssize_t mux_rx_enable_store(struct device *pdev,struct device_attribute *attr, const char *buf, size_t n){
    int enable = 0;

    sscanf(buf, "%d", &enable);
    if(enable != 0 && enable != 1)
        return -EINVAL;

    mux_rx_dump_enable = enable;
    printk("acm_rx_enable = [%d]\n", mux_rx_dump_enable);
    return n;
}
static DEVICE_ATTR(mux_rx_enable, S_IRUGO | S_IWUSR, mux_rx_enable_show, mux_rx_enable_store);
// SYSFS for tracing MUX RX log [E]


// SYSFS for tracing ACM TX log [S]
static ssize_t acm_tx_enable_show(struct device *pdev, struct device_attribute *attr, char *buf){
    return sprintf(buf, "acm_tx_enable = [%d] \n",acm_tx_dump_enable);
}

static ssize_t acm_tx_enable_store(struct device *pdev,struct device_attribute *attr, const char *buf, size_t n){
    int enable = 0;

    sscanf(buf, "%d", &enable);
    if(enable != 0 && enable != 1)
        return -EINVAL;

    acm_tx_dump_enable = enable;
    printk("acm_tx_enable = [%d]\n", acm_tx_dump_enable);
    return n;
}
static DEVICE_ATTR(acm_tx_enable, S_IRUGO | S_IWUSR, acm_tx_enable_show, acm_tx_enable_store);
// SYSFS for tracing ACM TX log [E]

// SYSFS for tracing ACM RX log [S]
static ssize_t acm_rx_enable_show(struct device *pdev, struct device_attribute *attr, char *buf){
    return sprintf(buf, "acm_rx_enable = [%d] \n",acm_rx_dump_enable);
}

static ssize_t acm_rx_enable_store(struct device *pdev,struct device_attribute *attr, const char *buf, size_t n){
    int enable = 0;

    sscanf(buf, "%d", &enable);
    if(enable != 0 && enable != 1)
        return -EINVAL;

    acm_rx_dump_enable = enable;
    printk("acm_rx_enable = [%d]\n", acm_rx_dump_enable);
    return n;
}
static DEVICE_ATTR(acm_rx_enable, S_IRUGO | S_IWUSR, acm_rx_enable_show, acm_rx_enable_store);
// SYSFS for tracing ACM RX log [E]


// SYSFS for tracing IPC TX log [S]
static ssize_t ipc_tx_enable_show(struct device *pdev, struct device_attribute *attr, char *buf){
    return sprintf(buf, "ipc_tx_enable = [%d] \n",ipc_tx_dump_enable);
}

static ssize_t ipc_tx_enable_store(struct device *pdev,struct device_attribute *attr, const char *buf, size_t n){
    int enable = 0;

    sscanf(buf, "%d", &enable);
    if(enable != 0 && enable != 1)
        return -EINVAL;

    ipc_tx_dump_enable = enable;
    printk("acm_tx_enable = [%d]\n", ipc_tx_dump_enable);
    return n;
}
static DEVICE_ATTR(ipc_tx_enable, S_IRUGO | S_IWUSR, ipc_tx_enable_show, ipc_tx_enable_store);
// SYSFS for tracing IPC TX log [E]

// SYSFS for tracing IPC RX log [S]
static ssize_t ipc_rx_enable_show(struct device *pdev, struct device_attribute *attr, char *buf){
    return sprintf(buf, "ipc_rx_enable = [%d] \n",ipc_rx_dump_enable);
}

static ssize_t ipc_rx_enable_store(struct device *pdev,struct device_attribute *attr, const char *buf, size_t n){
    int enable = 0;

    sscanf(buf, "%d", &enable);
    if(enable != 0 && enable != 1)
        return -EINVAL;

    ipc_rx_dump_enable = enable;
    printk("acm_rx_enable = [%d]\n", ipc_rx_dump_enable);
    return n;
}
static DEVICE_ATTR(ipc_rx_enable, S_IRUGO | S_IWUSR, ipc_rx_enable_show, ipc_rx_enable_store);
// SYSFS for tracing IPC RX log [E]

// SYSFS for IPC packet dump[S]
static ssize_t ipc_packet_dump_enable_show(struct device *pdev, struct device_attribute *attr, char *buf){
    return sprintf(buf, "ipc_packet_dump = [%d] \n",ipc_packet_dump);
}

static ssize_t ipc_packet_dump_enable_store(struct device *pdev,struct device_attribute *attr, const char *buf, size_t n){
    int enable = 0;

    sscanf(buf, "%d", &enable);
    if(enable != 0 && enable != 1)
        return -EINVAL;

    ipc_packet_dump = enable;
    printk("ipc_packet_dump = [%d]\n", ipc_packet_dump);
    return n;
}
static DEVICE_ATTR(ipc_packet_dump, S_IRUGO | S_IWUSR, ipc_packet_dump_enable_show, ipc_packet_dump_enable_store);
// SYSFS for IPC packet dump[E]

static ssize_t force_cp_crash_enable_show(struct device *pdev, struct device_attribute *attr, char *buf){
	return sprintf(buf, "force_crash_enable = [%d] \n",cp_force_crash_enable);
}

static ssize_t force_cp_crash_enable_store(struct device *pdev,struct device_attribute *attr, const char *buf, size_t n){

        int enable = 0;

        if(sscanf(buf, "%d", &enable) != 1)
		return -EINVAL;

	cp_force_crash_enable = enable;
	printk("cp_force_crash_enable = [%d]\n", cp_force_crash_enable);

        return n;
}
static DEVICE_ATTR(force_cp_crash, S_IRUGO | S_IWUSR, force_cp_crash_enable_show, force_cp_crash_enable_store);

static ssize_t do_force_cp_crash_show(struct device *pdev, struct device_attribute *attr, char *buf){
	return sprintf(buf, "DO cp_force_crash!!\n");
}
static ssize_t do_force_cp_crash_store(struct device *pdev,struct device_attribute *attr, const char *buf, size_t n){

        int enable = 0;

        if(sscanf(buf, "%d", &enable) != 1)
		return -EINVAL;

	if(enable == 1) {

#ifdef FEATURE_RECOVERY_CP_CRASH
		crash_by_gpio_status = 1;
#endif

		gpio_set_value(98, 1);
		printk("%s] do cp force crash start(1st) %s \n",__func__, gpio_get_value(98)? "HIGH": "LOW");
		msleep(100);
		gpio_set_value(98, 0);
		printk("%s] do cp force crash end(1st)%s \n",__func__, gpio_get_value(98)? "HIGH" : "LOW");
		msleep(100);
		gpio_set_value(98, 1);
		printk("%s] do cp force crash start(2nd) %s \n",__func__, gpio_get_value(98)? "HIGH": "LOW");
		msleep(100);
		gpio_set_value(98, 0);
		printk("%s] do cp force crash end(2nd) %s \n",__func__, gpio_get_value(98)? "HIGH" : "LOW");
	}
        return n;
}
static DEVICE_ATTR(do_force_cp_crash, S_IRWXU | S_IRWXG | S_IRWXO, do_force_cp_crash_show, do_force_cp_crash_store);

static ssize_t throw_at_cmd_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Throw at cmd!!\n");
}

static ssize_t throw_at_cmd_store(struct device *pdev,struct device_attribute *attr, const char *buf, size_t n)
{
	int mux_log_reset = 0;
    int acm_log_reset = 0;
    int mode = 0;

    if (sscanf(buf, "%d", &mode) != 1)
		return -EINVAL;

    if (mode == 0) {
        if (mux_rx_dump_enable)
            mux_log_reset = 1;
        if (acm_rx_dump_enable)
            acm_log_reset = 1;

        mux_rx_dump_enable = 1;
        acm_rx_dump_enable = 1;

        printk("[%s] MODE 2: confirm excution %s \n", __func__, gpio_get_value(98)? "HIGH" : "LOW");
        msleep(1000);

        mux_rx_dump_enable = mux_log_reset;
        acm_rx_dump_enable = acm_log_reset;

    } else if (mode == 1) {
        printk("[%s] MODE 1: confirm excution %s \n", __func__, gpio_get_value(98)? "HIGH" : "LOW");
        msleep(1);
    }

    return n;
}

static DEVICE_ATTR(throw_at_cmd, S_IRWXU | S_IRWXG | S_IRWXO, throw_at_cmd_show, throw_at_cmd_store);

static ssize_t ldisc_connect_show(struct device *pdev,
												struct device_attribute *attr,
												char *buf){
	//printk("ldisc_connect_status [%d]\n", ldisc_connect_status);
	return sprintf(buf, "get ldisc_connect_status [%d]\n", ldisc_connect_status);
}

static ssize_t ldisc_connect_store(struct device *pdev,
												struct device_attribute *attr,
												const char *buf, size_t n) {
	int enable = 0;
	if(sscanf(buf, "%d", &enable) != 1)
		return -EINVAL;

	ldisc_connect_status = enable;
	//printk("set ldisc_connect_status [%d]", ldisc_connect_status);
	return n;
}
static DEVICE_ATTR(ldisc_connect,
							S_IRUGO | S_IWUSR,
							ldisc_connect_show,
							ldisc_connect_store);

#ifdef FEATURE_RECOVERY_SCHEME
static ssize_t enable_recovery_show(struct device *pdev,
												struct device_attribute *attr,
												char *buf){
	printk("enable_recovery [%d]\n", enable_recovery);
	return sprintf(buf, "get enable_recovery [%d]\n", enable_recovery);
}

static ssize_t enable_recovery_store(struct device *pdev,
												struct device_attribute *attr,
												const char *buf, size_t n) {
	int enable = 0;
	if(sscanf(buf, "%d", &enable) != 1)
		return -EINVAL;

	enable_recovery = enable;
	printk("set enable_recovery [%d]", enable_recovery);
	return n;
}
static DEVICE_ATTR(enable_recovery,
							S_IRUGO | S_IWUSR,
							enable_recovery_show,
							enable_recovery_store);
#endif /* FEATURE_RECOVERY_SCHEME */

#ifdef FEATURE_RECOVERY_CP_CRASH
static ssize_t crash_by_gpio_show(struct device *pdev,
												struct device_attribute *attr,
												char *buf){
	printk("%s  [%d]\n", __func__, crash_by_gpio_status);
	return sprintf(buf, "[%d]\n", crash_by_gpio_status);
}
static ssize_t crash_by_gpio_store(struct device *pdev,
												struct device_attribute *attr,
												const char *buf, size_t n) {
	int enable = 0;
	if(sscanf(buf, "%d", &enable) != 1)
		return -EINVAL;

	crash_by_gpio_status = enable;
	printk("set enable_recovery [%d]", crash_by_gpio_status);
	return n;
}

static DEVICE_ATTR(crash_by_gpio,
							S_IRUGO | S_IWUSR,
							crash_by_gpio_show,
							crash_by_gpio_store);
#endif /* FEATURE_RECOVERY_CP_CRASH */

#ifdef ENABLE_NCM_TPUT
extern void register_usbnet_rx_timer(int interval);
extern void register_usbnet_tx_timer(int interval);
extern void unregister_usbnet_rx_timer();
extern void unregister_usbnet_tx_timer();

static ssize_t usbnet_tput_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	int timer;
    sscanf(buf, "%d", &timer);
    if(timer > 0 ){
        printk(KERN_DEBUG "%s : set USBNET tput logging timer as %d sec\n", __func__, timer);
        register_usbnet_rx_timer(timer);
        register_usbnet_tx_timer(timer);
    }else if(timer == 0){
        printk(KERN_DEBUG "%s : disable USBNET tput logging timer\n", __func__);
        unregister_usbnet_rx_timer();
        unregister_usbnet_tx_timer();
    }
	return n;
}
static ssize_t usbnet_tput_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
    printk(KERN_DEBUG "%s : show USBNET rx logging timer\n", __func__);
	return sprintf(buf, "enable_usbnet_rx_tput_show");
}
static DEVICE_ATTR(usbnet_tput, S_IRUGO | S_IWUSR,
			usbnet_tput_show, usbnet_tput_store);
#endif
#define ODIN_DWC_DEVICE_ATTR_R(name, format_str) \
static ssize_t \
name##_show(struct device *dev, struct device_attribute *attr, \
                  char *buf) \
{ \
	unsigned int val; \
	val = dwc_otg_get_##name(odin_ddata->dwc_dev.core_if); \
	return sprintf (buf, format_str, val); \
}

#define ODIN_DWC_DEVICE_ATTR_W(name, format_str) \
static ssize_t \
name##_store(struct device *dev, struct device_attribute *attr, \
		const char *buf, size_t n) \
{ \
	unsigned int val = simple_strtoul(buf, NULL, 16); \
	dwc_otg_set_##name(odin_ddata->dwc_dev.core_if, val); \
	return n; \
} \

#define ODIN_DWC_DEVICE_ATTR_RW(name, format_str) \
ODIN_DWC_DEVICE_ATTR_R(name, format_str)	 \
ODIN_DWC_DEVICE_ATTR_W(name, format_str) \
static DEVICE_ATTR(name, S_IRUGO | S_IWUSR, name##_show, name##_store);

#define ODIN_DWC_DEVICE_ATTR_RO(name, format_str)	 \
ODIN_DWC_DEVICE_ATTR_R(name, format_str)	 \
static DEVICE_ATTR(name, S_IRUGO, name##_show, NULL);

ODIN_DWC_DEVICE_ATTR_RO(mode, "mode=0x%x\n");
ODIN_DWC_DEVICE_ATTR_RW(hsic_connect, "hsic_connect=0x%x\n");
ODIN_DWC_DEVICE_ATTR_RW(inv_sel_hsic, "inv_sel_hsic=0x%x\n");
ODIN_DWC_DEVICE_ATTR_RW(mode_ch_tim, "mode_ch_tim=0x%x\n");
ODIN_DWC_DEVICE_ATTR_RW(fr_interval, "fr_interval=0x%x\n");
ODIN_DWC_DEVICE_ATTR_RW(gusbcfg, "gusbcfg=0x%08x\n");
ODIN_DWC_DEVICE_ATTR_RW(grxfsiz, "grxfsiz=0x%08x\n");
ODIN_DWC_DEVICE_ATTR_RW(gnptxfsiz, "gnptxfsiz=0x%08x\n");
ODIN_DWC_DEVICE_ATTR_RW(gpvndctl, "gpvndctl=0x%08x\n");
ODIN_DWC_DEVICE_ATTR_RW(guid, "guid=0x%08x\n");
ODIN_DWC_DEVICE_ATTR_RO(gsnpsid, "gsnpsid=0x%08x\n");
ODIN_DWC_DEVICE_ATTR_RO(hptxfsiz, "hptxfsiz=0x%08x\n");
ODIN_DWC_DEVICE_ATTR_RW(hprt0, "hprt0=0x%08x\n");
ODIN_DWC_DEVICE_ATTR_RW(prtsuspend, "prtsuspend=0x%x\n");
ODIN_DWC_DEVICE_ATTR_RW(prtpower, "prtpower=0x%x\n");

static struct device_attribute *odin_dwc_host_attrs[] = {
	&dev_attr_mode,
	&dev_attr_hsic_connect,
	&dev_attr_inv_sel_hsic,
	&dev_attr_mode_ch_tim,
	&dev_attr_fr_interval,
	&dev_attr_gusbcfg,
	&dev_attr_grxfsiz,
	&dev_attr_gnptxfsiz,
	&dev_attr_gpvndctl,
	&dev_attr_guid,
	&dev_attr_gsnpsid,
	&dev_attr_hptxfsiz,
	&dev_attr_hprt0,
	&dev_attr_prtsuspend,
	&dev_attr_prtpower,
	&dev_attr_hcddump,
	&dev_attr_spramdump,
	&dev_attr_regdump,
	&dev_attr_dbg_lvl,
	&dev_attr_resume_cnt,
#ifdef ENABLE_NCM_TPUT
	&dev_attr_usbnet_tput,
	&dev_attr_mdm_reset_enable,
	&dev_attr_mux_tx_enable,
	&dev_attr_mux_rx_enable,
	&dev_attr_acm_tx_enable,
	&dev_attr_acm_rx_enable,
	&dev_attr_ipc_tx_enable,
	&dev_attr_ipc_rx_enable,
	&dev_attr_ipc_packet_dump,
	&dev_attr_force_cp_crash,
	&dev_attr_do_force_cp_crash,
	&dev_attr_throw_at_cmd,
#endif
	&dev_attr_ldisc_connect,
	&dev_attr_enable_recovery,
#ifdef 	FEATURE_RECOVERY_CP_CRASH
	&dev_attr_crash_by_gpio,
#endif
	NULL
};

int odin_dwc_host_create_attr(struct platform_device *pdev)
{
	int ret;
	struct device_attribute **attrs = odin_dwc_host_attrs;

	while (*attrs) {
		ret = device_create_file(&pdev->dev, *attrs++);
		if (ret)
			goto err;
	}

	return 0;

err:
	udbg(UERR, "failed to create device file (%d)\n", ret);
	return ret;
}

void odin_dwc_host_remove_attr(struct platform_device *pdev)
{
	struct device_attribute **attrs = odin_dwc_host_attrs;

	while (*attrs)
		device_remove_file(&pdev->dev, *attrs++);
}

void odin_dwc_host_controller_recovery(void)
{
	dwc_otg_core_if_t *core_if = odin_ddata->dwc_dev.core_if;

	odin_dwc_host_reset(odin_ddata, false);
	odin_hsusb_host_resume(core_if, false);
}

static int odin_dwc_host_pre_bus_set(odin_dwc_ddata_t *ddata)
{
	int ret = -1;
	pcgcctl_data_t pcgcctl = {.d32 = 0};
#if !defined(ODIN_DWC_HOST_RECOV_BY_SWRESET)
	gahbcfg_data_t ahbcfg = {.d32 = 0 };
	gintmsk_data_t intmask = {.d32 = 0 };
	dwc_otg_core_if_t *core_if = ddata->dwc_dev.core_if;
	dwc_otg_core_global_regs_t *gregs = core_if->core_global_regs;
#endif

	udbg(UINFO, "%s\n", __func__);

	if (odin_pd_on(PD_ICS, 1) < 0)
		udbg(UERR, "%s : ICS PD1 On failed\n", __func__);
	else if (!odin_dwc_host_clk_enable(ddata)) {
		pcgcctl.b.stoppclk = 1;
		DWC_MODIFY_REG32(ddata->dwc_dev.core_if->pcgcctl, pcgcctl.d32, 0);
		dwc_udelay(10);

#if !defined(ODIN_DWC_HOST_RECOV_BY_SWRESET)
		ahbcfg.b.glblintrmsk = 1;
		DWC_MODIFY_REG32(&gregs->gahbcfg, 0, ahbcfg.d32);

		/* Unmask Interrupts */
		intmask.b.modemismatch = 1;
		intmask.b.wkupintr = 1;
		intmask.b.usbsuspend = 1;
		intmask.b.disconnect = 1;
		intmask.b.portintr = 1;

		DWC_WRITE_REG32(&gregs->gintmsk, intmask.d32);
#endif
		enable_irq(ddata->hcd->irq);
		ret = 0;
	}

	return ret;
}

static void odin_dwc_host_bus_reset(odin_dwc_ddata_t *ddata)
{
	dwc_otg_core_if_t *core_if = ddata->dwc_dev.core_if;
	dwc_otg_hcd_t *hcd = ddata->dwc_dev.hcd;
    pcgcctl_data_t pcgcctl = {.d32 = 0};
	gintsts_data_t intmsk = {.d32 = 0};

	intmsk.b.sofintr = 1;
	DWC_MODIFY_REG32(&core_if->core_global_regs->gintmsk, intmsk.d32, 0);

	hcd->force_reset_state = 1;

	clk_disable_unprepare(ddata->odin_dev.phy_clk);

	pcgcctl.b.stoppclk = 1;
	DWC_MODIFY_REG32(ddata->dwc_dev.core_if->pcgcctl, 0, pcgcctl.d32);

	dwc_otg_set_hsic_connect(ddata->dwc_dev.core_if, 0);
}

static void odin_dwc_host_bus_idle(odin_dwc_ddata_t *ddata)
{
	dwc_otg_core_if_t *core_if = ddata->dwc_dev.core_if;
	dwc_otg_hcd_t *hcd = ddata->dwc_dev.hcd;
#if !defined(ODIN_DWC_HOST_RECOV_BY_SWRESET)
	dwc_otg_core_global_regs_t *gregs = core_if->core_global_regs;
	gintsts_data_t intmsk = {.d32 = 0};
#endif
	pcgcctl_data_t pcgcctl = {.d32 = 0};

	hcd->flags.d32 = 0;
	hcd->non_periodic_channels = 0;
	hcd->periodic_channels = 0;

	if (ddata->b_state == CHANGING2) {
		if (clk_prepare_enable(ddata->odin_dev.phy_clk))
			udbg(UERR, "failed to enable phy clk\n");

		pcgcctl.b.stoppclk = 1;
		DWC_MODIFY_REG32(ddata->dwc_dev.core_if->pcgcctl, pcgcctl.d32, 0);
		dwc_udelay(10);
	}

#if defined(ODIN_DWC_HOST_RECOV_BY_SWRESET)
	odin_dwc_host_reset(ddata, false);

	odin_hsusb_host_resume(core_if, false);

	dwc_otg_set_hsic_connect(ddata->dwc_dev.core_if, 1);
#else
	dwc_otg_set_hsic_connect(ddata->dwc_dev.core_if, 1);

	DWC_WRITE_REG32(&gregs->gintsts, 0xffffffff);
	intmsk.b.hcintr = 1;
	DWC_MODIFY_REG32(&gregs->gintmsk, intmsk.d32, intmsk.d32);
#endif
}

int odin_dwc_host_set_idle(int set)
{
	u32 hs_conn;
	unsigned long flags;

	if (!odin_ddata) {
		udbg(UERR, "odin dev data is NULL\n");
		return 0;
	}

	/* Set this workaround flag to zero here to find better solution */
	enum_is_processing = 0;

	if (odin_ddata->b_state == SUSPENDING ||
		odin_ddata->b_state == RESUMING ||
		odin_ddata->b_state == CHANGING1 ||
		odin_ddata->b_state == CHANGING2) {
		udbg(UERR, "Failed to change BUS (%s, %s)\n",
			set ? "IDLE" : "RESET", bus_state_str[odin_ddata->b_state]);
		return -1;
	}

	udbg(UINFO, "%s : Set %s, current BUS is %s\n", __func__,
		set ? "IDLE" : "RESET", bus_state_str[odin_ddata->b_state]);

	if (odin_ddata->b_state == SUSPENDED ||
		odin_ddata->b_state == RESUME_ERR ||
		odin_ddata->b_state == SUSPENDED_DISCON) {
		odin_ddata->b_state = CHANGING1;
		if (odin_dwc_host_pre_bus_set(odin_ddata))
			udbg(UERR, "Failed to set pre bus state\n");
	} else {
		odin_ddata->b_state = CHANGING2;
	}

	if (odin_ddata->b_state == CHANGING1 ||
		odin_ddata->b_state == CHANGING2) {
		hs_conn = dwc_otg_get_hsic_connect(odin_ddata->dwc_dev.core_if);
		if (set == hs_conn) {
			udbg(UINFO, "Already BUS is %s\n", set ? "IDLE" : "RESET");
			goto done;
		}

		if (set) {
			odin_dwc_host_bus_idle(odin_ddata);
			udbg(UINFO, "Set IDLE state\n");
		} else {
			odin_dwc_host_bus_reset(odin_ddata);
			udbg(UINFO, "Set RESET state\n");
		}
	} else {
		udbg(UERR, "Invalid BUS State (%s, %s)\n",
			set ? "IDLE" : "RESET", bus_state_str[odin_ddata->b_state]);
		return -1;
	}
done:
	odin_ddata->b_state = set ? IDLE : RESET;

	return 0;
}
EXPORT_SYMBOL(odin_dwc_host_set_idle);

void odin_dwc_host_set_force_idle(int set)
{
	if (odin_ddata->b_state == SUSPENDED
		|| odin_ddata->b_state == RESUME_ERR
		|| odin_ddata->b_state == SUSPENDED_DISCON
		|| odin_ddata->b_state == SUSPENDING) {
		udbg(UERR, "%s : Failed to set force idle (%s)\n",
			__func__, bus_state_str[odin_ddata->b_state]);
		return ;
	}

	if (set) {
		udbg(UINFO, "Set Force IDLE state\n");
		dwc_otg_set_hsic_connect(odin_ddata->dwc_dev.core_if, 1);
	} else {
		udbg(UINFO, "Set Force RESET state\n");
		dwc_otg_set_hsic_connect(odin_ddata->dwc_dev.core_if, 0);
	}
}
EXPORT_SYMBOL(odin_dwc_host_set_force_idle);

void odin_dwc_host_set_pre_reset(void)
{
	dwc_otg_hcd_t *hcd = odin_ddata->dwc_dev.hcd;
	dwc_otg_core_if_t *core_if = odin_ddata->dwc_dev.core_if;
	gintsts_data_t intmsk = {.d32 = 0};

	hcd->force_reset_state = 1;

	if (odin_ddata->b_state == SUSPENDED
		|| odin_ddata->b_state == RESUME_ERR
		|| odin_ddata->b_state == SUSPENDED_DISCON
		|| odin_ddata->b_state == SUSPENDING) {
		udbg(UERR, "%s : Failed to set pre-reset (%s)\n",
			__func__, bus_state_str[odin_ddata->b_state]);
		return ;
	}

	udbg(UINFO, "%s\n", __func__);
	intmsk.b.sofintr = 1;
	DWC_MODIFY_REG32(&core_if->core_global_regs->gintmsk, intmsk.d32, 0);
}
EXPORT_SYMBOL(odin_dwc_host_set_pre_reset);

void odin_dwc_otg_dump_hprt0(void)
{
	hprt0_data_t hprt0 = {.d32 = 0 };
	dwc_otg_core_if_t *core_if;

	if (!odin_ddata) {
		udbg(UERR, "%s: ddata error.\n", __func__);
		return;
	}

	if (odin_ddata->b_state == SUSPENDED
		|| odin_ddata->b_state == RESUME_ERR
		|| odin_ddata->b_state == SUSPENDED_DISCON
		|| odin_ddata->b_state == SUSPENDING) {
		udbg(UERR, "%s : not ready to read hprt0's info.\n", __func__);
		return;
	}

	core_if = odin_ddata->dwc_dev.core_if;
	hprt0.d32 = DWC_READ_REG32(core_if->host_if->hprt0);
	udbg(UINFO, "%s: dump hprt : 0x%x\n", __func__, hprt0.d32);
}
EXPORT_SYMBOL(odin_dwc_otg_dump_hprt0);

#ifdef DUMP_IPC_PACKET
static int get_current_time()
{
	ktime_t calltime;
	u64 msecs64;
	int msecs;

	calltime = ktime_get();
	msecs64 = ktime_to_us(calltime);
	do_div(msecs64, MSEC_PER_SEC);
	msecs = msecs64;
	return msecs;
}

static int tx_index = 0;
static int rx_index = 0;
void copy_tx_packet(u8* data, int len)
{
	int copy_len = 0;
	copy_len = (len > PRINT_PACKET_LEN)? PRINT_PACKET_LEN: len;
    ipc_tx_dump[tx_index]->len = len;
    memcpy(&ipc_tx_dump[tx_index]->buf, data, copy_len);
    ipc_tx_dump[tx_index]->msec = get_current_time();
    tx_index = ++tx_index % MAX_DUMP_LEN;
}
EXPORT_SYMBOL(copy_tx_packet);

void copy_rx_packet(u8* data, int len)
{
    int copy_len = 0;
    copy_len = (len > PRINT_PACKET_LEN)? PRINT_PACKET_LEN: len;
    ipc_rx_dump[rx_index]->len = len;
    memcpy(&ipc_rx_dump[rx_index]->buf, data, copy_len);
    ipc_rx_dump[rx_index]->msec = get_current_time();
    rx_index = ++rx_index % MAX_DUMP_LEN;
}
EXPORT_SYMBOL(copy_rx_packet);

#define COL_SIZE PRINT_PACKET_LEN
static void print_dump_char(const unsigned char *buf, int count)
{
	char dump_buf_str[COL_SIZE+1];

	if (buf != NULL)
	{
		int j = 0;
		char *cur_str = dump_buf_str;
		unsigned char ch;
		while((j < COL_SIZE) && (j	< count))
		{
			ch = buf[j];
			if ((ch < 32) || (ch > 126))
			{
				*cur_str = '.';
			} else
			{
				*cur_str = ch;
			}
			cur_str++;
			j++;
		}
		*cur_str = 0;
		printk("text = [%s]\n", dump_buf_str);
	}
	else
	{
		printk("[buffer is NULL]\n");
	}
}

void print_ipc_dump_packet()
{
    int i,j,len;

    printk("*************************** IPC TX PACKET DUMP[S] ***************************************\n");
    printk("Last TX index = %d\n", tx_index);
    for(i=0; i < MAX_DUMP_LEN; i++){
        if(ipc_tx_dump[i] == NULL)
            continue;
        len = (ipc_tx_dump[i]->len > PRINT_PACKET_LEN)? PRINT_PACKET_LEN:ipc_tx_dump[i]->len;
        printk("PACKET %02d th, length = %d, timestamp = %d.%d(sec), data = ",
                   i, ipc_tx_dump[i]->len, ipc_tx_dump[i]->msec / 1000L, ipc_tx_dump[i]->msec % 1000L);
        for(j=0; j < len; j++){
            printk("%02x ", ipc_tx_dump[i]->buf[j]);
        }
        print_dump_char(&ipc_tx_dump[i]->buf, ipc_tx_dump[i]->len);
    }
    tx_index = 0;
    printk("*************************** IPC TX PACKET DUMP[E] ***************************************\n\n");

    printk("*************************** IPC RX PACKET DUMP[S] ***************************************\n");
    printk("Last RX index = %d\n", rx_index);
    for(i=0; i < MAX_DUMP_LEN; i++){
        if(ipc_rx_dump[i] == NULL)
            continue;
        len = (ipc_rx_dump[i]->len > PRINT_PACKET_LEN)? PRINT_PACKET_LEN:ipc_rx_dump[i]->len;
        printk("PACKET %02d th, length = %d, timestamp = %d.%d(sec), data = ",
                   i, ipc_rx_dump[i]->len,  ipc_rx_dump[i]->msec / 1000L, ipc_rx_dump[i]->msec % 1000L);
        for(j=0; j < len; j++){
            printk("%02x ", ipc_rx_dump[i]->buf[j]);
        }
        print_dump_char(&ipc_rx_dump[i]->buf, ipc_rx_dump[i]->len);
    }
    rx_index = 0;
    printk("*************************** IPC RX PACKET DUMP[E] ***************************************\n");
}
EXPORT_SYMBOL(print_ipc_dump_packet);
#endif

/*! \file
    \brief  Declaration of library functions

    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/




/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/


#if CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#define CFG_WMT_WAKELOCK_SUPPORT 1
#endif

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[WMT-PLAT]"


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/* ALPS header files */
#include <mach/mtk_rtc.h>
#include <mach/eint.h>
#include <mach/mtk_rtc.h>
#include <mach/mt_gpio_def.h>
#include <mach/board.h>

/* ALPS and COMBO header files */
#include <mach/mtk_wcn_cmb_stub.h>

/* MTK_WCN_COMBO header files */
#include "wmt_plat.h"
#include "wmt_dev.h"
#include "wmt_lib.h"
#include "mtk_wcn_cmb_hw.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

static VOID wmt_plat_bgf_eirq_cb(VOID);

static INT32 wmt_plat_ldo_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_pmu_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_rtc_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_rst_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_bgf_eint_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_wifi_eint_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_all_eint_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_uart_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_pcm_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_i2s_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_sdio_pin_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_gps_sync_ctrl(ENUM_PIN_STATE state);
static INT32 wmt_plat_gps_lna_ctrl(ENUM_PIN_STATE state);

static INT32 wmt_plat_dump_pin_conf(VOID);




/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
UINT32 gWmtDbgLvl = WMT_LOG_WARN;
INT32 gWmtMergeIfSupport = 0;


/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

static struct mtk_wcn_combo_gpio *g_wmt_pin_info;


#if CFG_WMT_WAKELOCK_SUPPORT
static OSAL_SLEEPABLE_LOCK gOsSLock;
static struct wake_lock wmtWakeLock;
#endif



irq_cb wmt_plat_bgf_irq_cb = NULL;
device_audio_if_cb wmt_plat_audio_if_cb = NULL;



static const fp_set_pin gfp_set_pin_table[] = {
	[PIN_LDO] = wmt_plat_ldo_ctrl,
	[PIN_PMU] = wmt_plat_pmu_ctrl,
	[PIN_RTC] = wmt_plat_rtc_ctrl,
	[PIN_RST] = wmt_plat_rst_ctrl,
	[PIN_BGF_EINT] = wmt_plat_bgf_eint_ctrl,
	[PIN_WIFI_EINT] = wmt_plat_wifi_eint_ctrl,
	[PIN_ALL_EINT] = wmt_plat_all_eint_ctrl,
	[PIN_UART_GRP] = wmt_plat_uart_ctrl,
	[PIN_PCM_GRP] = wmt_plat_pcm_ctrl,
	[PIN_I2S_GRP] = wmt_plat_i2s_ctrl,
	[PIN_SDIO_GRP] = wmt_plat_sdio_pin_ctrl,
	[PIN_GPS_SYNC] = wmt_plat_gps_sync_ctrl,
	[PIN_GPS_LNA] = wmt_plat_gps_lna_ctrl,

};

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*!
 * \brief audio control callback function for CMB_STUB on ALPS
 *
 * A platform function required for dynamic binding with CMB_STUB on ALPS.
 *
 * \param state desired audio interface state to use
 * \param flag audio interface control options
 *
 * \retval 0 operation success
 * \retval -1 invalid parameters
 * \retval < 0 error for operation fail
 */
INT32 wmt_plat_audio_ctrl(CMB_STUB_AIF_X state, CMB_STUB_AIF_CTRL ctrl)
{
	struct mtk_wcn_combo_gpio *pdata = g_wmt_pin_info;
	INT32 iRet = 0;
	UINT32 pinShare = 0;
	UINT32 mergeIfSupport = 0;

	/* input sanity check */
	if ((CMB_STUB_AIF_MAX <= state)
	    || (CMB_STUB_AIF_CTRL_MAX <= ctrl)) {
		return -1;
	}

	iRet = 0;

	/* set host side first */
	switch (state) {
	case CMB_STUB_AIF_0:
		/* BT_PCM_OFF & FM line in/out */
		iRet += wmt_plat_gpio_ctrl(PIN_PCM_GRP, PIN_STA_DEINIT);
		iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_DEINIT);
		break;

	case CMB_STUB_AIF_1:
		iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_DEINIT);
		iRet += wmt_plat_gpio_ctrl(PIN_PCM_GRP, PIN_STA_INIT);
		break;

	case CMB_STUB_AIF_2:
		iRet += wmt_plat_gpio_ctrl(PIN_PCM_GRP, PIN_STA_DEINIT);
		iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_INIT);
		break;

	case CMB_STUB_AIF_3:
		iRet += wmt_plat_gpio_ctrl(PIN_PCM_GRP, PIN_STA_INIT);
		iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_INIT);
		break;

	default:
		/* FIXME: move to cust folder? */
		WMT_ERR_FUNC("invalid state [%d]\n", state);
		return -1;
		break;
	}
	if (0 != wmt_plat_merge_if_flag_get()) {
		if (pdata->conf.merge)
			WMT_INFO_FUNC("[MT6628]<Merge IF> no need to ctrl combo chip side GPIO\n");
		else
			mergeIfSupport = 1;
	} else {
		mergeIfSupport = 1;
	}

	if (0 != mergeIfSupport) {
		if (CMB_STUB_AIF_CTRL_EN == ctrl) {
			WMT_INFO_FUNC("call chip aif setting\n");
			/* need to control chip side GPIO */
			if (NULL != wmt_plat_audio_if_cb) {
				iRet +=
				    (*wmt_plat_audio_if_cb) (state,
							     (pinShare) ? MTK_WCN_BOOL_TRUE :
							     MTK_WCN_BOOL_FALSE);
			} else {
				WMT_WARN_FUNC("wmt_plat_audio_if_cb is not registered\n");
				iRet -= 1;
			}


		} else {
			WMT_INFO_FUNC("skip chip aif setting\n");
		}
	}
	return iRet;

}

static VOID wmt_plat_all_eirq_cb(VOID)
{
}

static VOID wmt_plat_bgf_eirq_cb(VOID)
{
#if CFG_WMT_PS_SUPPORT
/* #error "need to disable EINT here" */
	/* wmt_lib_ps_irq_cb(); */
	if (NULL != wmt_plat_bgf_irq_cb)
		(*(wmt_plat_bgf_irq_cb)) ();
	else
		WMT_WARN_FUNC("WMT-PLAT: wmt_plat_bgf_irq_cb not registered\n");
#else
	return;
#endif

}



VOID wmt_lib_plat_irq_cb_reg(irq_cb bgf_irq_cb)
{
	wmt_plat_bgf_irq_cb = bgf_irq_cb;
}

VOID wmt_lib_plat_aif_cb_reg(device_audio_if_cb aif_ctrl_cb)
{
	wmt_plat_audio_if_cb = aif_ctrl_cb;
}






INT32 wmt_plat_init(struct mtk_combo_data *wmt_data)
{
	INT32 iret = -1;
	/* init cmb_hw */

	iret += mtk_wcn_cmb_hw_init(wmt_data);

	/*init wmt function ctrl wakelock if wake lock is supported by host platform */
#ifdef CFG_WMT_WAKELOCK_SUPPORT
	wake_lock_init(&wmtWakeLock, WAKE_LOCK_SUSPEND, "wmtFuncCtrl");
	osal_sleepable_lock_init(&gOsSLock);
#endif

	WMT_DBG_FUNC("WMT-PLAT: ALPS platform init (%d)\n", iret);

	return 0;
}


INT32 wmt_plat_deinit(VOID)
{
	INT32 iret;

	/* 1. de-init cmb_hw */
	iret = mtk_wcn_cmb_hw_deinit();
	/* 2. unreg to cmb_stub */
	iret += mtk_wcn_cmb_stub_unreg();
	/*3. wmt wakelock deinit */
#ifdef CFG_WMT_WAKELOCK_SUPPORT
	wake_lock_destroy(&wmtWakeLock);
	osal_sleepable_lock_deinit(&gOsSLock);
	WMT_DBG_FUNC("destroy wmtWakeLock\n");
#endif
	WMT_DBG_FUNC("WMT-PLAT: ALPS platform init (%d)\n", iret);

	return 0;
}

INT32 wmt_plat_sdio_ctrl(WMT_SDIO_SLOT_NUM sdioPortType, ENUM_FUNC_STATE on)
{
	return board_sdio_ctrl(sdioPortType, (FUNC_OFF == on) ? 0 : 1);
}

INT32 wmt_plat_irq_ctrl(ENUM_FUNC_STATE state)
{
	return -1;
}


static INT32 wmt_plat_dump_pin_conf(VOID)
{
	const struct mtk_wcn_combo_gpio *pdata = g_wmt_pin_info;

	WMT_INFO_FUNC("[WMT-PLAT]=>dump wmt pin configuration start<=\n");

	if (pdata->pwr.ldo.valid)
		WMT_INFO_FUNC("LDO(GPIO%d)\n", pdata->pwr.ldo.pin);
	else
		WMT_INFO_FUNC("LDO(not defined)\n");

	if (pdata->pwr.pmu.valid)
		WMT_INFO_FUNC("PMU(GPIO%d)\n", pdata->pwr.pmu.pin);
	else
		WMT_INFO_FUNC("PMU(not defined)\n");

	if (pdata->pwr.pmuv28.valid)
		WMT_INFO_FUNC("PMUV28(GPIO%d)\n", pdata->pwr.pmuv28.pin);
	else
		WMT_INFO_FUNC("PMUV28(not defined)\n");

	if (pdata->pwr.rst.valid)
		WMT_INFO_FUNC("RST(GPIO%d)\n", pdata->pwr.rst.pin);
	else
		WMT_INFO_FUNC("RST(not defined)\n");

	if (pdata->pwr.det.valid)
		WMT_INFO_FUNC("DET(GPIO%d)\n", pdata->pwr.det.pin);
	else
		WMT_INFO_FUNC("DET(not defined)\n");

	if (pdata->eint.bgf.valid)
		WMT_INFO_FUNC("BGF_EINT(GPIO%d)\n", pdata->eint.bgf.pin);
	else
		WMT_INFO_FUNC("BGF_EINT(not defined)\n");

	if (pdata->eint.wifi.valid)
		WMT_INFO_FUNC("WIFI_EINT(GPIO%d)\n", pdata->eint.wifi.pin);
	else
		WMT_INFO_FUNC("WIFI_EINT(not defined)\n");

	if (pdata->eint.all.valid)
		WMT_INFO_FUNC("ALL_EINT(GPIO%d)\n", pdata->eint.all.pin);
	else
		WMT_INFO_FUNC("ALL_EINT(not defined)\n");

	if (pdata->uart.rxd.valid)
		WMT_INFO_FUNC("UART_RX(GPIO%d)\n", pdata->uart.rxd.pin);
	else
		WMT_INFO_FUNC("UART_RX(not defined)\n");

	if (pdata->uart.txd.valid)
		WMT_INFO_FUNC("UART_TX(GPIO%d)\n", pdata->uart.txd.pin);
	else
		WMT_INFO_FUNC("UART_TX(not defined)\n");

	if (pdata->pcm.clk.valid)
		WMT_INFO_FUNC("DAICLK(GPIO%d)\n", pdata->pcm.clk.pin);
	else
		WMT_INFO_FUNC("DAICLK(not defined)\n");

	if (pdata->pcm.out.valid)
		WMT_INFO_FUNC("PCMOUT(GPIO%d)\n", pdata->pcm.out.pin);
	else
		WMT_INFO_FUNC("PCMOUT(not defined)\n");

	if (pdata->pcm.in.valid)
		WMT_INFO_FUNC("PCMIN(GPIO%d)\n", pdata->pcm.in.pin);
	else
		WMT_INFO_FUNC("PCMIN(not defined)\n");

	if (pdata->pcm.sync.valid)
		WMT_INFO_FUNC("PCMSYNC(GPIO%d)\n", pdata->pcm.sync.pin);
	else
		WMT_INFO_FUNC("PCMSYNC(not defined)\n");

	if (pdata->conf.fm_digital) {
		if (pdata->i2s.clk.valid)
			WMT_INFO_FUNC("I2S_CK(GPIO%d)\n", pdata->i2s.clk.pin);
		else
			WMT_INFO_FUNC("I2S_CK(not defined)\n");

		if (pdata->i2s.sync.valid)
			WMT_INFO_FUNC("I2S_WS(GPIO%d)\n", pdata->i2s.sync.pin);
		else
			WMT_INFO_FUNC("I2S_WS(not defined)\n");

		if (pdata->i2s.in.valid)
			WMT_INFO_FUNC("I2S_DAT(GPIO%d)\n", pdata->i2s.in.pin);
		else
			WMT_INFO_FUNC("I2S_DAT(not defined)\n");
	} else				/* FM_ANALOG_INPUT || FM_ANALOG_OUTPUT */
		WMT_INFO_FUNC("FM digital mode is not set, no need for I2S GPIOs\n");

	if (pdata->gps.sync.valid)
		WMT_INFO_FUNC("GPS_SYNC(GPIO%d)\n", pdata->gps.sync.pin);
	else
		WMT_INFO_FUNC("GPS_SYNC(not defined)\n");

	if (pdata->gps.lna.valid)
		WMT_INFO_FUNC("GPS_LNA(GPIO%d)\n", pdata->gps.lna.pin);
	else
		WMT_INFO_FUNC("GPS_LNA(not defined)\n");

	WMT_INFO_FUNC("[WMT-PLAT]=>dump wmt pin configuration emds<=\n");
	return 0;
}


INT32 wmt_plat_pwr_ctrl(ENUM_FUNC_STATE state)
{
	INT32 ret = -1;

	switch (state) {
	case FUNC_ON:
		/* TODO:[ChangeFeature][George] always output this or by request throuth /proc or sysfs? */
		wmt_plat_dump_pin_conf();
		ret = mtk_wcn_cmb_hw_pwr_on();
		break;

	case FUNC_OFF:
		ret = mtk_wcn_cmb_hw_pwr_off();
		break;

	case FUNC_RST:
		ret = mtk_wcn_cmb_hw_rst();
		break;
	case FUNC_STAT:
		ret = mtk_wcn_cmb_hw_state_show();
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) in pwr_ctrl\n", state);
		break;
	}

	return ret;
}

INT32 wmt_plat_ps_ctrl(ENUM_FUNC_STATE state)
{
	return -1;
}

INT32 wmt_plat_eirq_ctrl(ENUM_PIN_ID id, ENUM_PIN_STATE state)
{
	const struct mtk_wcn_combo_gpio *pdata = g_wmt_pin_info;
	INT32 iret;

	/* TODO: [ChangeFeature][GeorgeKuo]: use another function to handle this, as done in gpio_ctrls */

	if ((PIN_STA_INIT != state)
	    && (PIN_STA_DEINIT != state)
	    && (PIN_STA_EINT_EN != state)
	    && (PIN_STA_EINT_DIS != state)) {
		WMT_WARN_FUNC("WMT-PLAT:invalid PIN_STATE(%d) in eirq_ctrl for PIN(%d)\n", state,
			      id);
		return -1;
	}

	iret = -2;
	switch (id) {
	case PIN_BGF_EINT:
		if (pdata->eint.bgf.valid) {
			int eint = gpio_to_irq(pdata->eint.bgf.pin) - EINT_IRQ_BASE;
			if (PIN_STA_INIT == state) {
				mt_eint_registration(eint, pdata->eint.bgf.flags,
					wmt_plat_bgf_eirq_cb, 0);
				mt_eint_mask(eint);
				irq_set_irq_wake(gpio_to_irq(pdata->eint.bgf.pin), 1);
			} else if (PIN_STA_EINT_EN == state) {
				mt_eint_unmask(eint);
				WMT_DBG_FUNC("WMT-PLAT:BGFInt (en)\n");
			} else if (PIN_STA_EINT_DIS == state) {
				mt_eint_mask(eint);
				WMT_DBG_FUNC("WMT-PLAT:BGFInt (dis)\n");
			} else {
				irq_set_irq_wake(gpio_to_irq(pdata->eint.bgf.pin), 0);
				mt_eint_mask(eint);
				/* de-init: nothing to do in ALPS, such as un-registration... */
			}
		} else
			WMT_INFO_FUNC("WMT-PLAT:BGF EINT not defined\n");

		iret = 0;
		break;

	case PIN_ALL_EINT:
		if (pdata->eint.all.valid) {
			int eint = gpio_to_irq(pdata->eint.all.pin) - EINT_IRQ_BASE;
			if (PIN_STA_INIT == state) {
				mt_eint_registration(eint,
						     pdata->eint.all.flags, wmt_plat_all_eirq_cb, 0);
				mt_eint_mask(eint);
				WMT_DBG_FUNC("WMT-PLAT:ALLInt (INIT but not used yet)\n");
			} else if (PIN_STA_EINT_EN == state) {
				/*mt_eint_unmask(CUST_EINT_COMBO_ALL_NUM); */
				WMT_DBG_FUNC("WMT-PLAT:ALLInt (EN but not used yet)\n");
			} else if (PIN_STA_EINT_DIS == state) {
				mt_eint_mask(eint);
				WMT_DBG_FUNC("WMT-PLAT:ALLInt (DIS but not used yet)\n");
			} else {
				mt_eint_mask(eint);
				WMT_DBG_FUNC("WMT-PLAT:ALLInt (DEINIT but not used yet)\n");
				/* de-init: nothing to do in ALPS, such as un-registration... */
			}
		} else
			WMT_INFO_FUNC("WMT-PLAT:ALL EINT not defined\n");

		iret = 0;
		break;

	default:
		WMT_WARN_FUNC("WMT-PLAT:unsupported EIRQ(PIN_ID:%d) in eirq_ctrl\n", id);
		iret = -1;
		break;
	}

	return iret;
}

INT32 wmt_plat_gpio_ctrl(ENUM_PIN_ID id, ENUM_PIN_STATE state)
{
	if ((PIN_ID_MAX > id)
	    && (PIN_STA_MAX > state)) {

		/* TODO: [FixMe][GeorgeKuo] do sanity check to const function table when init and skip checking here */
		if (gfp_set_pin_table[id]) {
			return (*(gfp_set_pin_table[id])) (state);	/* .handler */
		} else {
			WMT_WARN_FUNC("WMT-PLAT: null fp for gpio_ctrl(%d)\n", id);
			return -2;
		}
	}
	return -1;
}

INT32 wmt_plat_ldo_ctrl(ENUM_PIN_STATE state)
{
	const struct mt_pin_info *ldo = &g_wmt_pin_info->pwr.ldo;

	if (!ldo->valid) {
		WMT_INFO_FUNC("WMT-PLAT:LDO is not used\n");
		return 0;
	}

	switch (state) {
	case PIN_STA_INIT:
		/*set to gpio output low, disable pull */
		mt_pin_set_pull(ldo->pin, MT_PIN_PULL_DISABLE);
		gpio_direction_output(ldo->pin, 0);
		mt_pin_set_mode_gpio(ldo->pin);
		WMT_DBG_FUNC("WMT-PLAT:LDO init (out 0)\n");
		break;

	case PIN_STA_OUT_H:
		gpio_set_value(ldo->pin, 1);
		WMT_DBG_FUNC("WMT-PLAT:LDO (out 1)\n");
		break;

	case PIN_STA_OUT_L:
		gpio_set_value(ldo->pin, 0);
		WMT_DBG_FUNC("WMT-PLAT:LDO (out 0)\n");
		break;

	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		mt_pin_set_mode_gpio(ldo->pin);
		gpio_direction_input(ldo->pin);
		mt_pin_set_pull(ldo->pin, MT_PIN_PULL_ENABLE_DOWN);
		WMT_DBG_FUNC("WMT-PLAT:LDO deinit (in pd)\n");
		break;

	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on LDO\n",
			state);
		break;
	}

	return 0;
}

INT32 wmt_plat_pmu_ctrl(ENUM_PIN_STATE state)
{
	const struct mt_pin_info *pmu = &g_wmt_pin_info->pwr.pmu;
	const struct mt_pin_info *pmuv28 = &g_wmt_pin_info->pwr.pmuv28;
	bool pull_en, pull_up;

	switch (state) {
	case PIN_STA_INIT:
		/*set to gpio output low, disable pull */
		mt_pin_set_pull(pmu->pin, MT_PIN_PULL_DISABLE);
		gpio_direction_output(pmu->pin, 0);
		mt_pin_set_mode_gpio(pmu->pin);
		if (pmuv28->valid) {
			mt_pin_set_pull(pmuv28->pin, MT_PIN_PULL_DISABLE);
			gpio_direction_output(pmuv28->pin, 0);
			mt_pin_set_mode_gpio(pmuv28->pin);
		}
		WMT_DBG_FUNC("WMT-PLAT:PMU init (out 0)\n");
		break;

	case PIN_STA_OUT_H:
		gpio_set_value(pmu->pin, 1);
		if (pmuv28->valid)
			gpio_set_value(pmuv28->pin, 1);
		WMT_DBG_FUNC("WMT-PLAT:PMU (out 1)\n");
		break;

	case PIN_STA_OUT_L:
		gpio_set_value(pmu->pin, 0);
		if (pmuv28->valid)
			gpio_set_value(pmuv28->pin, 0);
		WMT_DBG_FUNC("WMT-PLAT:PMU (out 0)\n");
		break;

	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		mt_pin_set_mode_gpio(pmu->pin);
		gpio_direction_input(pmu->pin);
		mt_pin_set_pull(pmu->pin, MT_PIN_PULL_ENABLE_DOWN);
		if (-1 != pmuv28->pin) {
			mt_pin_set_mode_gpio(pmuv28->pin);
			gpio_direction_input(pmuv28->pin);
			mt_pin_set_pull(pmuv28->pin,
				MT_PIN_PULL_ENABLE_DOWN);
		}
		WMT_DBG_FUNC("WMT-PLAT:PMU deinit (in pd)\n");
		break;
	case PIN_STA_SHOW:
		mt_pin_get_pull(pmu->pin, &pull_en, &pull_up);
		WMT_INFO_FUNC("WMT-PLAT:PMU PIN_STA_SHOW start\n");
		WMT_INFO_FUNC("WMT-PLAT:PMU Mode(%d)\n",
			mt_pin_get_mode(pmu->pin));
		WMT_INFO_FUNC("WMT-PLAT:PMU Dir(%d)\n",
			mt_pin_get_mode(pmu->pin));
		WMT_INFO_FUNC("WMT-PLAT:PMU Pull(%d, %d)\n",
			pull_en, pull_up);
		WMT_INFO_FUNC("WMT-PLAT:PMU out(%d)\n",
			gpio_get_value(pmu->pin));
		WMT_INFO_FUNC("WMT-PLAT:PMU PIN_STA_SHOW end\n");
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on PMU\n",
			state);
		break;
	}

	return 0;
}

INT32 wmt_plat_rtc_ctrl(ENUM_PIN_STATE state)
{
	switch (state) {
	case PIN_STA_INIT:
		rtc_gpio_enable_32k(RTC_GPIO_USER_GPS);
		WMT_DBG_FUNC("WMT-PLAT:RTC init\n");
		break;
	case PIN_STA_SHOW:
		WMT_INFO_FUNC("WMT-PLAT:RTC PIN_STA_SHOW start\n");
		/* TakMan: Temp. solution for building pass. Hongcheng Xia should check with vend_ownen.chen */
		/* WMT_INFO_FUNC("WMT-PLAT:RTC Status(%d)\n", rtc_gpio_32k_status()); */
		WMT_INFO_FUNC("WMT-PLAT:RTC PIN_STA_SHOW end\n");
		break;
	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on RTC\n",
			state);
		break;
	}
	return 0;
}


INT32 wmt_plat_rst_ctrl(ENUM_PIN_STATE state)
{
	const struct mt_pin_info *rst = &g_wmt_pin_info->pwr.rst;
	bool pull_en, pull_up;

	switch (state) {
	case PIN_STA_INIT:
		/*set to gpio output low, disable pull */
		mt_pin_set_pull(rst->pin, MT_PIN_PULL_DISABLE);
		gpio_direction_output(rst->pin, 0);
		mt_pin_set_mode_gpio(rst->pin);
		WMT_DBG_FUNC("WMT-PLAT:RST init (out 0)\n");
		break;

	case PIN_STA_OUT_H:
		gpio_set_value(rst->pin, 1);
		WMT_DBG_FUNC("WMT-PLAT:RST (out 1)\n");
		break;

	case PIN_STA_OUT_L:
		gpio_set_value(rst->pin, 0);
		WMT_DBG_FUNC("WMT-PLAT:RST (out 0)\n");
		break;

	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		mt_pin_set_mode_gpio(rst->pin);
		gpio_direction_input(rst->pin);
		mt_pin_set_pull(rst->pin, MT_PIN_PULL_ENABLE_DOWN);
		WMT_DBG_FUNC("WMT-PLAT:RST deinit (in pd)\n");
		break;
	case PIN_STA_SHOW:
		mt_pin_get_pull(rst->pin, &pull_en, &pull_up);
		WMT_INFO_FUNC("WMT-PLAT:RST PIN_STA_SHOW start\n");
		WMT_INFO_FUNC("WMT-PLAT:RST Mode(%d)\n",
			mt_pin_get_mode(rst->pin));
		WMT_INFO_FUNC("WMT-PLAT:RST Pull (%d, %d)\n",
			pull_en, pull_up);
		WMT_INFO_FUNC("WMT-PLAT:RST out(%d)\n",
			gpio_get_value(rst->pin));
		WMT_INFO_FUNC("WMT-PLAT:RST PIN_STA_SHOW end\n");
		break;

	default:
		WMT_WARN_FUNC("WMT-PLAT:Warnning, invalid state(%d) on RST\n",
			state);
		break;
	}

	return 0;
}

INT32 wmt_plat_bgf_eint_ctrl(ENUM_PIN_STATE state)
{
	const struct mt_pin_info *bgf = &g_wmt_pin_info->eint.bgf;

	if (!bgf->valid) {
		WMT_INFO_FUNC("WMT-PLAT:BGF EINT not defined\n");
		return 0;
	}

	switch (state) {
	case PIN_STA_INIT:
		/*set to gpio input low, pull down enable */
		mt_pin_set_mode_gpio(bgf->pin);
		gpio_direction_input(bgf->pin);
		mt_pin_set_pull(bgf->pin, MT_PIN_PULL_ENABLE_DOWN);
		WMT_DBG_FUNC("WMT-PLAT:BGFInt init(in pd)\n");
		break;

	case PIN_STA_MUX:
		mt_pin_set_mode_gpio(bgf->pin);
		mt_pin_set_pull(bgf->pin, MT_PIN_PULL_ENABLE_UP);
		mt_pin_set_mode_eint(bgf->pin);
		WMT_DBG_FUNC("WMT-PLAT:BGFInt mux (eint)\n");
		break;

	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		mt_pin_set_mode_gpio(bgf->pin);
		gpio_direction_input(bgf->pin);
		mt_pin_set_pull(bgf->pin, MT_PIN_PULL_ENABLE_DOWN);
		WMT_DBG_FUNC("WMT-PLAT:BGFInt deinit(in pd)\n");
		break;

	default:
		WMT_WARN_FUNC(
			"WMT-PLAT:Warnning, invalid state(%d) on BGF EINT\n",
			state);
		break;
	}

	return 0;
}

INT32 wmt_plat_wifi_eint_ctrl(ENUM_PIN_STATE state)
{
#if 0
	const struct mt_pin_info *wifi = &g_wmt_pin_info->eint.wifi;
	int eint = gpio_to_irq(wifi->pin) - EINT_IRQ_BASE;

	if (!wifi->valid) {
		WMT_INFO_FUNC(
			"WMT-PLAT:WIFI EINT is controlled by MSDC driver\n");
		return 0;
	}

	switch (state) {
	case PIN_STA_INIT:
		mt_pin_set_pull(wifi->pin, MT_PIN_PULL_DISABLE);
		gpio_direction_output(wifi->pin, 0);
		mt_pin_set_mode_gpio(wifi->pin);
		break;
	case PIN_STA_MUX:
		mt_pin_set_mode_gpio(wifi->pin);
		mt_pin_set_pull(wifi->pin, MT_PIN_PULL_ENABLE_UP);
		mt_pin_set_mode_eint(wifi->pin);
		break;
	case PIN_STA_EINT_EN:
		mt_eint_unmask(eint);
		break;
	case PIN_STA_EINT_DIS:
		mt_eint_mask(eint);
		break;
	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		mt_pin_set_mode_gpio(wifi->pin);
		gpio_direction_input(wifi->pin);
		mt_pin_set_pull(wifi->pin, MT_PIN_PULL_ENABLE_DOWN);
		break;
	default:
		WMT_WARN_FUNC(
			"WMT-PLAT:Warnning, invalid state(%d) on WIFI EINT\n",
			state);
		break;
	}
#endif
	return 0;
}


INT32 wmt_plat_all_eint_ctrl(ENUM_PIN_STATE state)
{
	const struct mt_pin_info *all = &g_wmt_pin_info->eint.all;

	if (!all->valid) {
		WMT_INFO_FUNC("WMT-PLAT:ALL EINT not defined\n");
		return 0;
	}

	switch (state) {
	case PIN_STA_INIT:
		mt_pin_set_mode_gpio(all->pin);
		gpio_direction_input(all->pin);
		mt_pin_set_pull(all->pin, MT_PIN_PULL_ENABLE_DOWN);
		WMT_DBG_FUNC("WMT-PLAT:ALLInt init(in pd)\n");
		break;

	case PIN_STA_MUX:
		mt_pin_set_mode_gpio(all->pin);
		mt_pin_set_pull(all->pin, MT_PIN_PULL_ENABLE_UP);
		mt_pin_set_mode_eint(all->pin);
		break;

	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		/*set to gpio input low, pull down enable */
		mt_pin_set_mode_gpio(all->pin);
		gpio_direction_input(all->pin);
		mt_pin_set_pull(all->pin, MT_PIN_PULL_ENABLE_DOWN);
		break;

	default:
		WMT_WARN_FUNC(
			"WMT-PLAT:Warnning, invalid state(%d) on ALL EINT\n",
			state);
		break;
	}
	return 0;
}

INT32 wmt_plat_uart_ctrl(ENUM_PIN_STATE state)
{
	const struct mt_pin_info *rxd = &g_wmt_pin_info->uart.rxd;
	const struct mt_pin_info *txd = &g_wmt_pin_info->uart.txd;

	if (!rxd->valid || !txd->valid)
		return 0;

	switch (state) {
	case PIN_STA_MUX:
	case PIN_STA_INIT:
		mt_pin_set_mode_by_name(rxd->pin, "URXD");
		mt_pin_set_mode_by_name(txd->pin, "UTXD");
		WMT_DBG_FUNC("WMT-PLAT:UART init (UART mode)\n");
		break;
	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		mt_pin_set_mode_gpio(rxd->pin);
		gpio_direction_output(rxd->pin, 0);

		mt_pin_set_mode_gpio(txd->pin);
		gpio_direction_output(txd->pin, 0);
		WMT_DBG_FUNC("WMT-PLAT:UART deinit GPIO mode (out 0)\n");
		break;

	default:
		WMT_WARN_FUNC(
			"WMT-PLAT:Warnning, invalid state(%d) on UART Group\n",
			state);
		break;
	}

	return 0;
}


INT32 wmt_plat_pcm_ctrl(ENUM_PIN_STATE state)
{
	const struct mt_pin_info *clk = &g_wmt_pin_info->pcm.clk;
	const struct mt_pin_info *in = &g_wmt_pin_info->pcm.in;
	const struct mt_pin_info *out = &g_wmt_pin_info->pcm.out;
	const struct mt_pin_info *sync = &g_wmt_pin_info->pcm.sync;
	const struct mtk_combo_conf *conf = &g_wmt_pin_info->conf;
	UINT32 normalPCMFlag = 1;

	const char *merge_mode = "MRG";
	const char *pcm_mode = "PCM";

	if (!clk->valid || !in->valid || !out->valid || !sync->valid)
		return 0;

	/*check if combo chip support merge if or not */
	if (0 == wmt_plat_merge_if_flag_get())
		goto skip_merge_if;

	if (conf->merge) {
		/* Disable normal PCM interface*/
		normalPCMFlag = 0;

		/* Hardware support Merge IF function */
		WMT_DBG_FUNC("WMT-PLAT:<Merge IF>set to Merge PCM function\n");
		/*merge PCM function define */
		switch (state) {
		case PIN_STA_MUX:
		case PIN_STA_INIT:
			mt_pin_set_mode_by_name(clk->pin, merge_mode);
			mt_pin_set_mode_by_name(out->pin, merge_mode);
			mt_pin_set_mode_by_name(in->pin, merge_mode);
			mt_pin_set_mode_by_name(sync->pin, merge_mode);
			WMT_DBG_FUNC("WMT-PLAT:<Merge IF>PCM init (mode: %s)\n", merge_mode);
			break;

		case PIN_STA_IN_L:
		case PIN_STA_DEINIT:
			mt_pin_set_mode_by_name(clk->pin, merge_mode);
			mt_pin_set_mode_by_name(out->pin, merge_mode);
			mt_pin_set_mode_by_name(in->pin, merge_mode);
			mt_pin_set_mode_by_name(sync->pin, merge_mode);
			WMT_DBG_FUNC("WMT-PLAT:<Merge IF>PCM deinit (mode: %s)\n", merge_mode);
			break;

		default:
			WMT_WARN_FUNC(
				"WMT-PLAT:<Merge IF>Warnning, invalid state(%d) "
				"on PCM Group\n", state);
			break;
		}
	} else
		/* Hardware does not support Merge IF function */
		WMT_DBG_FUNC("WMT-PLAT:set to normal PCM function\n");

skip_merge_if:
	if (0 == normalPCMFlag)
		return 0;

	/*normal PCM function define */
	switch (state) {
	case PIN_STA_MUX:
	case PIN_STA_INIT:
		mt_pin_set_mode_by_name(clk->pin, pcm_mode);
		mt_pin_set_mode_by_name(out->pin, pcm_mode);
		mt_pin_set_mode_by_name(in->pin, pcm_mode);
		mt_pin_set_mode_by_name(sync->pin, pcm_mode);
		WMT_DBG_FUNC("WMT-PLAT:MT6589 PCM init (mode: %s)\n", pcm_mode);
		break;

	case PIN_STA_IN_L:
	case PIN_STA_DEINIT:
		mt_pin_set_mode_by_name(clk->pin, pcm_mode);
		mt_pin_set_mode_by_name(out->pin, pcm_mode);
		mt_pin_set_mode_by_name(in->pin, pcm_mode);
		mt_pin_set_mode_by_name(sync->pin, pcm_mode);
		WMT_DBG_FUNC("WMT-PLAT:MT6589 PCM deinit (mode: %s)\n", pcm_mode);
		break;

	default:
		WMT_WARN_FUNC(
			"WMT-PLAT:MT6589 Warnning, invalid state(%d) "
			"on PCM Group\n", state);
		break;
	}

	return 0;
}


INT32 wmt_plat_i2s_ctrl(ENUM_PIN_STATE state)
{
	/* TODO: [NewFeature][GeorgeKuo]: GPIO_I2Sx is changed according to different project. */
	/* TODO: provide a translation table in board_custom.h for different ALPS project customization. */
	const struct mt_pin_info *i2s_ck = &g_wmt_pin_info->i2s.clk;
	const struct mt_pin_info *i2s_ws = &g_wmt_pin_info->i2s.sync;
	const struct mt_pin_info *i2s_dat = &g_wmt_pin_info->i2s.in;
	const struct mt_pin_info *pcm_out = &g_wmt_pin_info->i2s.out;
	const struct mtk_combo_conf *conf = &g_wmt_pin_info->conf;
	const char *merge_mode = "MRG";
	const char *i2s_mode = "I2S";
	UINT32 normalI2SFlag = 1;

	if (!i2s_ck->valid || !i2s_ws->valid ||
		!i2s_dat->valid || !pcm_out->valid)
		return 0;

	/*check if combo chip support merge if or not */
	if (0 == wmt_plat_merge_if_flag_get())
		goto skip_merge_if;

	if (conf->merge) {
		/* Hardware support Merge IF function */
		if (conf->fm_digital) {
			normalI2SFlag = 0;
			switch (state) {
			case PIN_STA_INIT:
			case PIN_STA_MUX:
				mt_pin_set_mode_by_name(i2s_ck->pin, merge_mode);
				mt_pin_set_mode_by_name(i2s_ws->pin, merge_mode);
				mt_pin_set_mode_by_name(i2s_dat->pin, merge_mode);
				mt_pin_set_mode_by_name(pcm_out->pin, merge_mode);
				WMT_DBG_FUNC("WMT-PLAT:<Merge IF>I2S init (I2S0 system)\n");
				break;
			case PIN_STA_IN_L:
			case PIN_STA_DEINIT:
				mt_pin_set_mode_gpio(i2s_ck->pin);
				gpio_direction_output(i2s_ck->pin, 0);

				mt_pin_set_mode_gpio(i2s_ws->pin);
				gpio_direction_output(i2s_ws->pin, 0);

				mt_pin_set_mode_gpio(i2s_dat->pin);
				gpio_direction_output(i2s_dat->pin, 0);

				WMT_DBG_FUNC("WMT-PLAT:<Merge IF>I2S deinit (out 0)\n");
				break;
			default:
				WMT_WARN_FUNC(
					"WMT-PLAT:<Merge IF>Warnning, invalid "
					"state(%d) on I2S Group\n", state);
				break;
			}
		} else
			WMT_INFO_FUNC(
				"[MT662x]<Merge IF>warnning:FM digital mode is not set, "
				"no I2S GPIO settings should be modified by combo driver\n");
	}

skip_merge_if:
	if (0 == normalI2SFlag)
		return 0;

	if (conf->fm_digital) {
		switch (state) {
		case PIN_STA_INIT:
		case PIN_STA_MUX:
			mt_pin_set_mode_by_name(i2s_ck->pin, i2s_mode);
			mt_pin_set_mode_by_name(i2s_ws->pin, i2s_mode);
			mt_pin_set_mode_by_name(i2s_dat->pin, i2s_mode);
			WMT_DBG_FUNC("WMT-PLAT:<I2S IF>I2S init (I2S0 system)\n");
			break;
		case PIN_STA_IN_L:
		case PIN_STA_DEINIT:
			mt_pin_set_mode_gpio(i2s_ck->pin);
			gpio_direction_output(i2s_ck->pin, 0);

			mt_pin_set_mode_gpio(i2s_ws->pin);
			gpio_direction_output(i2s_ws->pin, 0);

			mt_pin_set_mode_gpio(i2s_dat->pin);
			gpio_direction_output(i2s_dat->pin, 0);

			WMT_DBG_FUNC("WMT-PLAT:<I2S IF>I2S deinit (out 0)\n");
			break;
		default:
			WMT_WARN_FUNC(
				"WMT-PLAT:<I2S IF>Warnning, invalid state(%d) "
				"on I2S Group\n", state);
			break;
		}
	} else
		WMT_INFO_FUNC(
			"[MT662x]<I2S IF>warnning:FM digital mode is not set, "
			"no I2S GPIO settings should be modified by combo driver\n");

	return 0;
}

INT32 wmt_plat_sdio_pin_ctrl(ENUM_PIN_STATE state)
{
#if 0
	switch (state) {
	case PIN_STA_INIT:
	case PIN_STA_MUX:
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
#if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
		/* TODO: [FixMe][GeorgeKuo]: below are used for MT6573 only!
		Find a better way to do ALPS customization for different platform. */
		/* WMT_INFO_FUNC( "[mt662x] pull up sd1 bus(gpio62~68)\n"); */
		mt_pin_set_pull(GPIO62, MT_PIN_PULL_ENABLE_UP);
		mt_pin_set_pull(GPIO63, MT_PIN_PULL_ENABLE_UP);
		mt_pin_set_pull(GPIO64, MT_PIN_PULL_ENABLE_UP);
		mt_pin_set_pull(GPIO65, MT_PIN_PULL_ENABLE_UP);
		mt_pin_set_pull(GPIO66, MT_PIN_PULL_ENABLE_UP);
		mt_pin_set_pull(GPIO67, MT_PIN_PULL_ENABLE_UP);
		mt_pin_set_pull(GPIO68, MT_PIN_PULL_ENABLE_UP);
		WMT_DBG_FUNC("WMT-PLAT:SDIO init (pu)\n");
#elif (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 2)
#error "fix sdio2 gpio settings"
#endif
#else
#error "CONFIG_MTK_WCN_CMB_SDIO_SLOT undefined!!!"
#endif

		break;

	case PIN_STA_DEINIT:
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
#if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
		/* TODO: [FixMe][GeorgeKuo]: below are used for MT6573 only!
		Find a better way to do ALPS customization for different platform. */
		/* WMT_INFO_FUNC( "[mt662x] pull down sd1 bus(gpio62~68)\n"); */
		mt_pin_set_pull(GPIO62, MT_PIN_PULL_ENABLE_DOWN);
		mt_pin_set_pull(GPIO63, MT_PIN_PULL_ENABLE_DOWN);
		mt_pin_set_pull(GPIO64, MT_PIN_PULL_ENABLE_DOWN);
		mt_pin_set_pull(GPIO65, MT_PIN_PULL_ENABLE_DOWN);
		mt_pin_set_pull(GPIO66, MT_PIN_PULL_ENABLE_DOWN);
		mt_pin_set_pull(GPIO67, MT_PIN_PULL_ENABLE_DOWN);
		mt_pin_set_pull(GPIO68, MT_PIN_PULL_ENABLE_DOWN);
		WMT_DBG_FUNC("WMT-PLAT:SDIO deinit (pd)\n");
#elif (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 2)
#error "fix sdio2 gpio settings"
#endif
#else
#error "CONFIG_MTK_WCN_CMB_SDIO_SLOT undefined!!!"
#endif
		break;

	default:
		WMT_WARN_FUNC(
			"WMT-PLAT:Warnning, invalid state(%d) "
			"on SDIO Group\n", state);
		break;
	}
#endif
	return 0;
}

static INT32 wmt_plat_gps_sync_ctrl(ENUM_PIN_STATE state)
{
	const struct mt_pin_info *sync = &g_wmt_pin_info->gps.sync;

	if (!sync->valid)
		return 0;

	switch (state) {
	case PIN_STA_INIT:
	case PIN_STA_DEINIT:
		mt_pin_set_mode_gpio(sync->pin);
		gpio_direction_output(sync->pin, 0);
		break;

	case PIN_STA_MUX:
		if (sync->custom)
			mt_pin_set_mode(sync->pin, sync->mode1);
		break;

	default:
		break;
	}

	return 0;
}


static INT32 wmt_plat_gps_lna_ctrl(ENUM_PIN_STATE state)
{
	const struct mt_pin_info *lna = &g_wmt_pin_info->gps.lna;

	if (!lna->valid) {
		WMT_WARN_FUNC("host gps lna pin not defined\n");
		return 0;
	}

	switch (state) {
	case PIN_STA_INIT:
	case PIN_STA_DEINIT:
		mt_pin_set_pull(lna->pin, MT_PIN_PULL_DISABLE);
		gpio_direction_output(lna->pin, 0);
		mt_pin_set_mode_gpio(lna->pin);
		break;

	case PIN_STA_OUT_H:
		gpio_set_value(lna->pin, 1);
		break;
	case PIN_STA_OUT_L:
		gpio_set_value(lna->pin, 0);
		break;

	default:
		WMT_WARN_FUNC("%d mode not defined for  gps lna pin !!!\n",
			state);
		break;
	}

	return 0;
}



INT32 wmt_plat_wake_lock_ctrl(ENUM_WL_OP opId)
{
#ifdef CFG_WMT_WAKELOCK_SUPPORT
	static INT32 counter;
	INT32 ret = 0;


	ret = osal_lock_sleepable_lock(&gOsSLock);
	if (ret) {
		WMT_ERR_FUNC("--->lock gOsSLock failed, ret=%d\n", ret);
		return ret;
	}

	if (WL_OP_GET == opId)
		++counter;
	else if (WL_OP_PUT == opId)
		--counter;

	osal_unlock_sleepable_lock(&gOsSLock);
	if (WL_OP_GET == opId && counter == 1) {
		wake_lock(&wmtWakeLock);
		WMT_DBG_FUNC("WMT-PLAT: after wake_lock(%d), counter(%d)\n",
			     wake_lock_active(&wmtWakeLock), counter);

	} else if (WL_OP_PUT == opId && counter == 0) {
		wake_unlock(&wmtWakeLock);
		WMT_DBG_FUNC("WMT-PLAT: after wake_unlock(%d), counter(%d)\n",
			     wake_lock_active(&wmtWakeLock), counter);
	} else {
		WMT_WARN_FUNC("WMT-PLAT: wakelock status(%d), counter(%d)\n",
			      wake_lock_active(&wmtWakeLock), counter);
	}
	return 0;
#else
	WMT_WARN_FUNC("WMT-PLAT: host awake function is not supported.");
	return 0;

#endif
}


INT32 wmt_plat_merge_if_flag_ctrl(UINT32 enable)
{
	struct mtk_wcn_combo_gpio *pdata = g_wmt_pin_info;
	if (enable) {
		if (pdata->conf.merge)
			gWmtMergeIfSupport = 1;
		else {
			gWmtMergeIfSupport = 0;
			WMT_WARN_FUNC("neither MT6589, MTK_MERGE_INTERFACE_SUPPORT nor MT6628 is not set to true, ");
			WMT_WARN_FUNC("so set gWmtMergeIfSupport to %d\n", gWmtMergeIfSupport);
		}
	} else {
		gWmtMergeIfSupport = 0;
	}

	WMT_INFO_FUNC("set gWmtMergeIfSupport to %d\n", gWmtMergeIfSupport);
	return gWmtMergeIfSupport;
}

INT32 wmt_plat_merge_if_flag_get(VOID)
{
	return gWmtMergeIfSupport;
}

int wmt_plat_alps_init(struct mtk_combo_data *wmt_data)
{
	struct mtk_wcn_combo_gpio *pins = wmt_data->pin_info;

	if (!pins)
		return -EINVAL;

	if (g_wmt_pin_info)
		return -EBUSY;
	WMT_WARN_FUNC ("pins->eint.bgf.valid:%d\n", pins->eint.bgf.valid);
	if (pins->eint.bgf.valid)
		gpio_request(pins->eint.bgf.pin, "alps.eint.bgf");

	if (pins->eint.all.valid && pins->eint.all.w != pins->eint.bgf.w)
		gpio_request(pins->eint.all.pin, "alps.eint.all");

	/* AP:
	 * I'm not attempting to request pins->wifi, because I believe we should not do it here.
	 * All the code referring to sdio pins in this file is 'if 0' by now.
	 * They must be managed by their driver instead. */
	WMT_WARN_FUNC ("pins->pwr.ldo.valid:%d\n", pins->pwr.ldo.valid);
	if (pins->pwr.ldo.valid)
		gpio_request(pins->pwr.ldo.pin, "alps.pwr.ldo");
	WMT_WARN_FUNC ("pins->pmu.ldo.valid:%d\n", pins->pwr.pmu.valid);
	if (pins->pwr.pmu.valid)
		gpio_request(pins->pwr.pmu.pin, "alps.pwr.pmu");
	WMT_WARN_FUNC ("pins->pmuv28.ldo.valid:%d\n", pins->pwr.pmuv28.valid);
	if (pins->pwr.pmuv28.valid)
		gpio_request(pins->pwr.pmuv28.pin, "alps.pwr.pmuv28");
	WMT_WARN_FUNC ("pins->rst.ldo.valid:%d\n", pins->pwr.rst.valid);
	if (pins->pwr.rst.valid)
		gpio_request(pins->pwr.rst.pin, "alps.pwr.rst");
	WMT_WARN_FUNC ("pins->uart.txd.valid:%d\n", pins->uart.txd.valid);
	if (pins->uart.txd.valid)
		gpio_request(pins->uart.txd.pin, "alps.uart.txd");
	WMT_WARN_FUNC ("pins->uart.rxd.valid:%d\n", pins->uart.rxd.valid);
	if (pins->uart.rxd.valid)
		gpio_request(pins->uart.rxd.pin, "alps.uart.rxd");
	WMT_WARN_FUNC ("pins->gps.sync..valid:%d\n", pins->gps.sync.valid);
	if (pins->gps.sync.valid)
		gpio_request(pins->gps.sync.pin, "alps.gps.sync");
	WMT_WARN_FUNC ("pins->gps.lna..valid:%d\n", pins->gps.lna.valid);
	if (pins->gps.lna.valid)
		gpio_request(pins->gps.lna.pin, "alps.gps.lna");

	if (pins->pcm.sync.valid &&
		pins->pcm.clk.valid &&
		pins->pcm.in.valid &&
		pins->pcm.out.valid) {
		gpio_request(pins->pcm.sync.pin, "alps.pcm.sync");
		gpio_request(pins->pcm.clk.pin, "alps.pcm.clk");
		gpio_request(pins->pcm.in.pin, "alps.pcm.in");
		gpio_request(pins->pcm.out.pin, "alps.pcm.out");
	}
	WMT_WARN_FUNC ("pins->i2s.sync.valid:%d\n", pins->i2s.sync.valid);
	if (pins->i2s.sync.w != pins->pcm.sync.w && pins->i2s.sync.valid)
		gpio_request(pins->i2s.sync.pin, "alps.i2s.sync");
	WMT_WARN_FUNC ("pins->i2s.clk.valid:%d\n", pins->i2s.clk.valid);
	if (pins->i2s.clk.w != pins->pcm.clk.w && pins->i2s.clk.valid)
		gpio_request(pins->i2s.clk.pin, "alps.i2s.clk");
	WMT_WARN_FUNC ("pins->i2s.in.valid:%d\n", pins->i2s.in.valid);
	if (pins->i2s.in.w != pins->pcm.in.w && pins->i2s.in.valid)
		gpio_request(pins->i2s.in.pin, "alps.i2s.in");
	WMT_WARN_FUNC ("pins->i2s.out.valid:%d\n", pins->i2s.out.valid);
	if (pins->i2s.out.w != pins->pcm.out.w && pins->i2s.out.valid)
		gpio_request(pins->i2s.out.pin, "alps.i2s.out");

	g_wmt_pin_info = pins;

	return 0;
}

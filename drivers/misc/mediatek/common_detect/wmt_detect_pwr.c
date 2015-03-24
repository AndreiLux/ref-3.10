#include <mach/mtk_rtc.h>
#include <linux/gpio.h>
#include <mach/mt_gpio_def.h>
#include <mach/mtk_wcn_cmb_stub.h>

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[WMT-DETECT]"

#include "wmt_detect.h"

static struct mtk_wifi_power *g_wmt_detect_data;

static int wmt_detect_dump_pin_conf(void)
{
	const struct mtk_wifi_power *pdata = g_wmt_detect_data;

	WMT_DETECT_INFO_FUNC("[WMT-DETECT]=>dump wmt pin configuration start<=\n");

	if (pdata->ldo.valid)
		WMT_DETECT_INFO_FUNC("LDO(GPIO%d)\n", pdata->ldo.pin);
	else
		WMT_DETECT_INFO_FUNC("LDO(not defined)\n");

	if (pdata->pmu.valid)
		WMT_DETECT_INFO_FUNC("PMU(GPIO%d)\n", pdata->pmu.pin);
	else
		WMT_DETECT_INFO_FUNC("PMU(not defined)\n");

	if (pdata->rst.valid)
		WMT_DETECT_INFO_FUNC("RST(GPIO%d)\n", pdata->rst.pin);
	else
		WMT_DETECT_INFO_FUNC("RST(not defined)\n");

	if (pdata->det.valid)
		WMT_DETECT_INFO_FUNC("DET(GPIO%d)\n", pdata->det.pin);
	else
		WMT_DETECT_INFO_FUNC("DET(not defined)\n");

	WMT_DETECT_INFO_FUNC("[WMT-PLAT]=>dump wmt pin configuration emds<=\n");
	return 0;
}

int _wmt_detect_set_output_mode(unsigned int _id)
{
	int id = (int)_id == -1 ? -1 : _id & 0xFFFF;
	if (id == -1)
		return 0;
	mt_pin_set_pull(id, MT_PIN_PULL_DISABLE);
	gpio_direction_output(id, 0);
	mt_pin_set_mode_gpio(id);
	WMT_DETECT_DBG_FUNC("WMT-DETECT: set GPIO%d to output mode\n", id);
	return 0;
}

int _wmt_detect_set_input_mode(unsigned int _id)
{
	int id = (int)_id == -1 ? -1 : _id & 0xFFFF;
	if (id == -1)
		return 0;
	mt_pin_set_pull(id, MT_PIN_PULL_DISABLE);
	gpio_direction_input(id);
	mt_pin_set_mode_gpio(id);
	WMT_DETECT_DBG_FUNC("WMT-DETECT: set GPIO%d to input mode\n", id);
	return 0;
}

int _wmt_detect_output_low(unsigned int _id)
{
	/*_wmt_detect_set_output_mode(id);*/
	int id = (int)_id == -1 ? -1 : _id & 0xFFFF;
	if (id == -1)
		return 0;
	gpio_set_value(id, 0);
	WMT_DETECT_DBG_FUNC("WMT-DETECT: set GPIO%d to output 0\n", id);
	return 0;
}

int _wmt_detect_output_high(unsigned int _id)
{
	/*_wmt_detect_set_output_mode(id);*/
	int id = (int)_id == -1 ? -1 : _id & 0xFFFF;
	if (id == -1)
		return 0;
	gpio_set_value(id, 1);
	WMT_DETECT_DBG_FUNC("WMT-DETECT: set GPIO%d to output 0\n", id);
	return 0;
}

int _wmt_detect_read_gpio_input(unsigned int _id)
{
	int retval = 0;
	int id = (int)_id == -1 ? -1 : _id & 0xFFFF;
	if (id == -1)
		return 0;
	_wmt_detect_set_input_mode(id);
	retval = gpio_get_value(id);
	WMT_DETECT_DBG_FUNC("WMT-DETECT: read GPIO%d input: %d\n", id, retval);
	return retval;
}


/*This power on sequence must support all combo chip's basic power on sequence
	1. LDO control is a must, if external LDO exist
	2. PMU control is a must
	3. RST control is a must
	4. WIFI_EINT pin control is a must, used for GPIO mode for EINT status checkup
	5. RTC32k clock control is a must
*/
static int wmt_detect_chip_pwr_on(void)
{
	const struct mtk_wifi_power *pdata = g_wmt_detect_data;
	int retval = -1;

	if (pdata->ldo.valid) {
		/*set LDO/PMU/RST to output 0, no pull */
		_wmt_detect_set_output_mode(pdata->ldo.pin);
		_wmt_detect_output_low(pdata->ldo.pin);
	}

	_wmt_detect_set_output_mode(pdata->pmu.pin);
	_wmt_detect_output_low(pdata->pmu.pin);

	_wmt_detect_set_output_mode(pdata->rst.pin);
	_wmt_detect_output_low(pdata->rst.pin);

	if (pdata->ldo.valid) {
		/*pull high LDO */
		_wmt_detect_output_high(pdata->ldo.pin);
		/*sleep for LDO stable time */
		msleep(MAX_LDO_STABLE_TIME);
	}

	/*export RTC clock, sleep for RTC stable time */
	rtc_gpio_enable_32k(RTC_GPIO_USER_GPS);
	msleep(MAX_RTC_STABLE_TIME);

	/*PMU output low, RST output low, to make chip power off completely */
	/*always done */

	/*sleep for power off stable time */
	msleep(MAX_OFF_STABLE_TIME);
	/*PMU output high, and sleep for reset stable time */
	_wmt_detect_output_high(pdata->pmu.pin);
	msleep(MAX_RST_STABLE_TIME);
	/*RST output high, and sleep for power on stable time */
	_wmt_detect_output_high(pdata->rst.pin);
	msleep(MAX_ON_STABLE_TIME);

	retval = 0;
	return retval;
}

static int wmt_detect_chip_pwr_off(void)
{
	const struct mtk_wifi_power *pdata = g_wmt_detect_data;

	_wmt_detect_set_output_mode(pdata->rst.pin);
	_wmt_detect_output_low(pdata->rst.pin);

	/*set RST pin to input low status */
	if (pdata->ldo.valid) {
		_wmt_detect_set_output_mode(pdata->ldo.pin);
		_wmt_detect_output_low(pdata->ldo.pin);
	}

	/*set RST pin to input low status */

	/*set RST pin to input low status */
	if (pdata->pmu.valid) {
		_wmt_detect_set_output_mode(pdata->pmu.pin);
		_wmt_detect_output_low(pdata->pmu.pin);
	}

	return 0;
}

int wmt_detect_read_ext_cmb_status(void)
{
	const struct mtk_wifi_power *pdata = g_wmt_detect_data;
	int retval = 0;
	/* AP:
	 * This way of reading gpio pin status is a dangerous solution, because
	 * it belongs to different driver, and we're changing pin mode without
	 * notifying the owner.
	 * I made the detection optional, with default success, to avoid this.
	 */
	if (!pdata->det.valid) {
		retval = 1;
		WMT_DETECT_ERR_FUNC("WMT-DETECT: no DET pin set; assume detected\n");
	} else {
		retval = _wmt_detect_read_gpio_input(pdata->det.pin);
		WMT_DETECT_ERR_FUNC("WMT-DETECT: DET input status:%d\n", retval);
	}
	return retval;
}

int wmt_detect_chip_pwr_ctrl(int on)
{
	int err;
	err = on ? wmt_detect_chip_pwr_on() : wmt_detect_chip_pwr_off();
	if (err)
		wmt_detect_dump_pin_conf();
	return err;
}


int wmt_detect_sdio_pwr_ctrl(int on)
{
	int retval = -1;
#ifdef MTK_WCN_COMBO_CHIP_SUPPORT
	if (0 == on) {
		/*power off SDIO slot */
		retval = board_sdio_ctrl(1, 0);
	} else {
		/*power on SDIO slot */
		retval = board_sdio_ctrl(1, 1);
	}
#else
	WMT_DETECT_WARN_FUNC("WMT-DETECT: MTK_WCN_COMBO_CHIP_SUPPORT is not set\n");
#endif
	return retval;
}

int wmt_detect_power_probe(struct mtk_wifi_power *pdata)
{
	if (!pdata)
		return -ENODEV;

	if (!pdata->rst.valid || !pdata->pmu.valid) {
		WMT_DETECT_ERR_FUNC("WMT-DETECT: either PMU(%d) or RST(%d) is not set\n",
			pdata->pmu.pin, pdata->rst.pin);
		return -EINVAL;
	}

	if (g_wmt_detect_data)
		return -EBUSY;

	gpio_request(pdata->pmu.pin, "COMBO-PMU");
	gpio_request(pdata->rst.pin, "COMBO-RST");

	if (pdata->pmuv28.valid)
		gpio_request(pdata->pmuv28.pin, "COMBO-PMUV28");
	if (pdata->ldo.valid)
		gpio_request(pdata->ldo.pin, "COMBO-LDO");
	if (pdata->det.valid)
		gpio_request(pdata->det.pin, "COMBO-DET");

	g_wmt_detect_data = pdata;
	return 0;
}

int wmt_detect_power_remove(struct mtk_wifi_power *pdata)
{
	if (!pdata)
		return -ENODEV;

	BUG_ON(pdata != g_wmt_detect_data);

	gpio_free(pdata->pmu.pin);
	gpio_free(pdata->rst.pin);

	if (pdata->pmuv28.valid)
		gpio_free(pdata->pmuv28.pin);
	if (pdata->ldo.valid)
		gpio_free(pdata->ldo.pin);
	if (pdata->det.valid)
		gpio_free(pdata->det.pin);

	g_wmt_detect_data = NULL;
	return 0;
}

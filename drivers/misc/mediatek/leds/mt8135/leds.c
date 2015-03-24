/*
 * drivers/leds/leds-mt65xx.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * mt65xx leds driver
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/leds-mt65xx.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
/* #include <cust_leds.h> */
/* #include <cust_leds_def.h> */
#include <mach/mt_pwm.h>
/* #include <mach/mt_gpio.h> */
#include <mach/pmic_mt6329_hw_bank1.h>
#include <mach/pmic_mt6329_sw_bank1.h>
#include <mach/pmic_mt6329_hw.h>
#include <mach/pmic_mt6329_sw.h>
#include <drivers/misc/mediatek/power/mt8135/upmu_common.h>
#include <mach/upmu_hw.h>
#include "leds_sw.h"

/* #include <linux/leds_sw.h> */
/* #include <mach/mt_pmic_feature_api.h> */
/* #include <mach/mt_boot.h> */


static DEFINE_MUTEX(leds_mutex);
/* #define ISINK_CHOP_CLK */
/****************************************************************************
 * variables
 ***************************************************************************/
/* struct cust_mt65xx_led* bl_setting_hal = NULL; */
static unsigned int bl_brightness_hal = 102;
static unsigned int bl_duty_hal = 21;
static unsigned int bl_div_hal = CLK_DIV1;
static unsigned int bl_frequency_hal = 32000;
struct wake_lock leds_suspend_lock;

/****************************************************************************
 * DEBUG MACROS
 ***************************************************************************/
static int debug_enable = 1;
#define LEDS_DEBUG(format, args...) do { \
	if (debug_enable) {\
		pr_warn(format, ##args);\
	} \
} while (0)

/*****************PWM *************************************************/
static int time_array_hal[PWM_DIV_NUM] = { 256, 512, 1024, 2048, 4096, 8192, 16384, 32768 };
static unsigned int div_array_hal[PWM_DIV_NUM] = { 1, 2, 4, 8, 16, 32, 64, 128 };

static unsigned int backlight_PWM_div_hal = CLK_DIV1;	/* this para come from cust_leds. */


/****************************************************************************
 * func:return global variables
 ***************************************************************************/
#define CUST_BLS_PWM
extern int disp_bls_set_backlight(unsigned int level);

unsigned int brightness_mapping(unsigned int level)
{
#if !defined(CUST_BLS_PWM)
	return (level << 6) >> 8;
#else
	unsigned int mapped_level;
	
	mapped_level = level;
	
	return mapped_level;
#endif
}

static struct cust_mt65xx_led cust_led_list[MT65XX_LED_TYPE_TOTAL] = {
		/* have no hw, add as blue for android app need */
		{"red", MT65XX_LED_MODE_PMIC, MT65XX_LED_PMIC_NLED_ISINK0, {0} },
		{"green", MT65XX_LED_MODE_PMIC, MT65XX_LED_PMIC_NLED_ISINK1, {0} },
		{"blue", MT65XX_LED_MODE_PMIC, MT65XX_LED_PMIC_NLED_ISINK0, {0} },
		{"jogball-backlight", MT65XX_LED_MODE_NONE, -1, {0} },
		{"keyboard-backlight", MT65XX_LED_MODE_NONE, -1, {0} },
		{"button-backlight", MT65XX_LED_MODE_PMIC, MT65XX_LED_PMIC_BUTTON, {0} },
#if !defined(CUST_BLS_PWM)
		{"lcd-backlight", MT65XX_LED_MODE_PWM, PWM1, {1, 3, 1, 1, 0} },
#else
		{"lcd-backlight", MT65XX_LED_MODE_CUST_BLS_PWM, (int)disp_bls_set_backlight, {0, 0x11, 0, 0, 0} },
#endif
};

void mt_leds_wake_lock_init(void)
{
	wake_lock_init(&leds_suspend_lock, WAKE_LOCK_SUSPEND, "leds wakelock");
}

unsigned int mt_get_bl_brightness(void)
{
	return bl_brightness_hal;
}

unsigned int mt_get_bl_duty(void)
{
	return bl_duty_hal;
}

unsigned int mt_get_bl_div(void)
{
	return bl_div_hal;
}

unsigned int mt_get_bl_frequency(void)
{
	return bl_frequency_hal;
}

unsigned int *mt_get_div_array(void)
{
	return &div_array_hal[0];
}

void mt_set_bl_duty(unsigned int level)
{
	bl_duty_hal = level;
}

void mt_set_bl_div(unsigned int div)
{
	bl_div_hal = div;
}

void mt_set_bl_frequency(unsigned int freq)
{
	bl_frequency_hal = freq;
}

struct cust_mt65xx_led *mt_get_cust_led_list(void)
{
	return cust_led_list;
}


/****************************************************************************
 * internal functions
 ***************************************************************************/
/* #if 0 */
static int brightness_mapto64(int level)
{
	if (level < 30)
		return (level >> 1) + 7;
	else if (level <= 120)
		return (level >> 2) + 14;
	else if (level <= 160)
		return level / 5 + 20;
	else
		return (level >> 3) + 33;
}



static int find_time_index(int time)
{
	int index = 0;
	while (index < 8) {
		if (time < time_array_hal[index])
			return index;
		else
			index++;
	}
	return PWM_DIV_NUM - 1;
}

int mt_led_set_pwm(int pwm_num, struct nled_setting *led)
{
	/* struct pwm_easy_config pwm_setting; */
	struct pwm_spec_config pwm_setting;
	int time_index = 0;
	pwm_setting.pwm_no = pwm_num;
	pwm_setting.mode = PWM_MODE_OLD;

	LEDS_DEBUG("[LED]led_set_pwm: mode=%d,pwm_no=%d\n", led->nled_mode, pwm_num);
	/* if((pwm_num != PWM3 && pwm_num != PWM4 && pwm_num != PWM5))//AP PWM all support OLD mode in MT6589 */
	pwm_setting.clk_src = PWM_CLK_OLD_MODE_32K;
	/* else */
	/* pwm_setting.clk_src = PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625; */

	switch (led->nled_mode) {
	case NLED_OFF:
		pwm_setting.PWM_MODE_OLD_REGS.THRESH = 0;
		pwm_setting.clk_div = CLK_DIV1;
		pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH = 100;
		break;

	case NLED_ON:
		pwm_setting.PWM_MODE_OLD_REGS.THRESH = 30;
		pwm_setting.clk_div = CLK_DIV1;
		pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH = 100;
		break;

	case NLED_BLINK:
		LEDS_DEBUG("[LED]LED blink on time = %d offtime = %d\n", led->blink_on_time,
			   led->blink_off_time);
		time_index = find_time_index(led->blink_on_time + led->blink_off_time);
		LEDS_DEBUG("[LED]LED div is %d\n", time_index);
		pwm_setting.clk_div = time_index;
		pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH =
		    (led->blink_on_time +
		     led->blink_off_time) * MIN_FRE_OLD_PWM / div_array_hal[time_index];
		pwm_setting.PWM_MODE_OLD_REGS.THRESH =
		    (led->blink_on_time * 100) / (led->blink_on_time + led->blink_off_time);
	}

	pwm_setting.PWM_MODE_FIFO_REGS.IDLE_VALUE = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.GUARD_VALUE = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.GDURATION = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
	pwm_set_spec_config(&pwm_setting);

	return 0;

}


/* ***********************MT6589 led breath funcion******************************* */
#if 0
static int led_breath_pmic(enum mt65xx_led_pmic pmic_type, struct nled_setting *led)
{
	/* int time_index = 0; */
	/* int duty = 0; */
	LEDS_DEBUG("[LED]led_blink_pmic: pmic_type=%d\n", pmic_type);

	if ((pmic_type != MT65XX_LED_PMIC_NLED_ISINK0 && pmic_type != MT65XX_LED_PMIC_NLED_ISINK1)
	    || led->nled_mode != NLED_BLINK) {
		return -1;
	}

	switch (pmic_type) {
	case MT65XX_LED_PMIC_NLED_ISINK0:	/* keypad center 4mA */
		upmu_set_isinks_ch0_mode(PMIC_PWM_0);	/* 6320 spec */
		upmu_set_isinks_ch0_step(0x0);	/* 4mA */
		upmu_set_isinks_breath0_trf_sel(0x04);
		upmu_set_isinks_breath0_ton_sel(0x02);
		upmu_set_isinks_breath0_toff_sel(0x03);
		upmu_set_isink_dim1_duty(15);
		upmu_set_isink_dim1_fsel(11);	/* 6320 0.25KHz */
		upmu_set_rg_spk_div_pdn(0x01);
		upmu_set_rg_spk_pwm_div_pdn(0x01);
		upmu_set_isinks_ch0_en(0x01);
		break;
	case MT65XX_LED_PMIC_NLED_ISINK1:	/* keypad LR  16mA */
		upmu_set_isinks_ch1_mode(PMIC_PWM_1);	/* 6320 spec */
		upmu_set_isinks_ch1_step(0x3);	/* 16mA */
		upmu_set_isinks_breath0_trf_sel(0x04);
		upmu_set_isinks_breath0_ton_sel(0x02);
		upmu_set_isinks_breath0_toff_sel(0x03);
		upmu_set_isink_dim1_duty(15);
		upmu_set_isink_dim1_fsel(11);	/* 6320 0.25KHz */
		upmu_set_rg_spk_div_pdn(0x01);
		upmu_set_rg_spk_pwm_div_pdn(0x01);
		upmu_set_isinks_ch1_en(0x01);
		break;
	default:
		break;
	}
	return 0;

}

#endif

#define PMIC_PERIOD_NUM 13
/* int pmic_period_array[] = {250,500,1000,1250,1666,2000,2500,3333,4000,5000,6666,8000,10000,13333,16000}; */
int pmic_period_array[] = {
	250, 500, 1000, 1250, 1666, 2000, 2500, 3333, 4000, 5000, 6666, 8000, 10000
};

u8 pmic_clksel_array[] = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3 };

/* u8 pmic_freqsel_array[] = {24,26,28,29,30,31,29,30,31,29,30,31,29,30,31}; */
/* u8 pmic_freqsel_array[] = {21,22,23,24,25,26,24,25,26,24,25,26,24,25,26}; */
u8 pmic_freqsel_array[] = { 21, 22, 23, 24, 24, 24, 25, 26, 26, 26, 28, 28, 28 };

static int find_time_index_pmic(int time_ms)
{
	int i;
	for (i = 0; i < PMIC_PERIOD_NUM; i++) {
		if (time_ms <= pmic_period_array[i])
			return i;
		else
			continue;
	}
	return PMIC_PERIOD_NUM - 1;
}

int mt_led_blink_pmic(enum mt65xx_led_pmic pmic_type, struct nled_setting *led)
{
	int time_index = 0;
	int duty = 0;
	LEDS_DEBUG("[LED]led_blink_pmic: pmic_type=%d\n", pmic_type);

	if ((pmic_type != MT65XX_LED_PMIC_NLED_ISINK0 && pmic_type != MT65XX_LED_PMIC_NLED_ISINK1
	     && pmic_type != MT65XX_LED_PMIC_NLED_ISINK2) || led->nled_mode != NLED_BLINK) {
		return -1;
	}

	LEDS_DEBUG("[LED]LED blink on time = %d offtime = %d\n", led->blink_on_time,
		   led->blink_off_time);
	time_index = find_time_index_pmic(led->blink_on_time + led->blink_off_time);
	LEDS_DEBUG("[LED]LED index is %d clksel=%d freqsel=%d\n", time_index,
		   pmic_clksel_array[time_index], pmic_freqsel_array[time_index]);
	duty = 32 * led->blink_on_time / (led->blink_on_time + led->blink_off_time);
	switch (pmic_type) {
	case MT65XX_LED_PMIC_NLED_ISINK0:	/* keypad center 4mA */
		upmu_set_rg_bst_drv_1m_ck_pdn(0x0);
		upmu_set_isinks_ch0_mode(PMIC_PWM_0);	/* 6320 spec */
		upmu_set_isinks_ch0_step(0x0);	/* 4mA */
		upmu_set_isink_dim0_duty(duty);
		upmu_set_isink_dim0_fsel(pmic_freqsel_array[time_index]);
		/* upmu_set_rg_spk_pwm_div_pdn(0x01); */
		/* upmu_set_rg_bst_drv_1m_ck_pdn(0x0); */
#ifdef ISINK_CHOP_CLK
		/* upmu_set_isinks0_chop_en(0x1); */
#endif
		upmu_set_isinks_breath0_trf_sel(0x0);
		upmu_set_isinks_breath0_ton_sel(0x02);
		upmu_set_isinks_breath0_toff_sel(0x05);

		upmu_set_isinks_ch0_en(0x01);
		break;
	case MT65XX_LED_PMIC_NLED_ISINK1:	/* keypad LR  16mA */
		upmu_set_rg_bst_drv_1m_ck_pdn(0x0);
		upmu_set_isinks_ch1_mode(PMIC_PWM_0);	/* 6320 spec */
		upmu_set_isinks_ch1_step(0x3);	/* 16mA */
		upmu_set_isink_dim1_duty(duty);
		upmu_set_isink_dim1_fsel(pmic_freqsel_array[time_index]);
		upmu_set_isinks_breath1_trf_sel(0x0);
		upmu_set_isinks_breath1_ton_sel(0x02);
		upmu_set_isinks_breath1_toff_sel(0x05);
		/* upmu_set_rg_spk_pwm_div_pdn(0x01); */

#ifdef ISINK_CHOP_CLK
		/* upmu_set_isinks1_chop_en(0x1); */
#endif
		upmu_set_isinks_ch1_en(0x01);
		break;
	case MT65XX_LED_PMIC_NLED_ISINK2:	/* keypad LR  16mA */
		upmu_set_rg_bst_drv_1m_ck_pdn(0x0);
		upmu_set_isinks_ch2_mode(PMIC_PWM_0);	/* 6320 spec */
		upmu_set_isinks_ch2_step(0x3);	/* 16mA */
		upmu_set_isink_dim2_duty(duty);
		upmu_set_isink_dim2_fsel(pmic_freqsel_array[time_index]);
		/* upmu_set_rg_spk_pwm_div_pdn(0x01); */
		upmu_set_isinks_breath2_trf_sel(0x0);
		upmu_set_isinks_breath2_ton_sel(0x02);
		upmu_set_isinks_breath2_toff_sel(0x05);
#ifdef ISINK_CHOP_CLK
		/* upmu_set_isinks2_chop_en(0x1); */
#endif
		upmu_set_isinks_ch2_en(0x01);
		break;
	default:
		break;
	}
	return 0;
}


int mt_backlight_set_pwm(int pwm_num, u32 level, u32 div, struct PWM_config *config_data)
{
	struct pwm_spec_config pwm_setting;
	pwm_setting.pwm_no = pwm_num;
	pwm_setting.mode = PWM_MODE_FIFO;	/* new mode fifo and periodical mode */

	pwm_setting.pmic_pad = config_data->pmic_pad;

	if (config_data->div) {
		pwm_setting.clk_div = config_data->div;
		backlight_PWM_div_hal = config_data->div;
	} else
		pwm_setting.clk_div = div;
	if (config_data->clock_source)
		pwm_setting.clk_src = PWM_CLK_NEW_MODE_BLOCK;
	else
		pwm_setting.clk_src = PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625;

	if (config_data->High_duration && config_data->low_duration) {
		pwm_setting.PWM_MODE_FIFO_REGS.HDURATION = config_data->High_duration;
		pwm_setting.PWM_MODE_FIFO_REGS.LDURATION = pwm_setting.PWM_MODE_FIFO_REGS.HDURATION;
	} else {
		pwm_setting.PWM_MODE_FIFO_REGS.HDURATION = 4;
		pwm_setting.PWM_MODE_FIFO_REGS.LDURATION = 4;
	}
	pwm_setting.PWM_MODE_FIFO_REGS.IDLE_VALUE = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.GUARD_VALUE = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 31;
	pwm_setting.PWM_MODE_FIFO_REGS.GDURATION =
	    (pwm_setting.PWM_MODE_FIFO_REGS.HDURATION + 1) * 32 - 1;
	pwm_setting.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;

	LEDS_DEBUG("[LED]backlight_set_pwm:duty is %d\n", level);
	LEDS_DEBUG("[LED]backlight_set_pwm:clk_src/div/high/low is %d%d%d%d\n", pwm_setting.clk_src,
		   pwm_setting.clk_div, pwm_setting.PWM_MODE_FIFO_REGS.HDURATION,
		   pwm_setting.PWM_MODE_FIFO_REGS.LDURATION);
	if (level > 0 && level <= 32) {
		pwm_setting.PWM_MODE_FIFO_REGS.GUARD_VALUE = 0;
		pwm_setting.PWM_MODE_FIFO_REGS.SEND_DATA0 = (1 << level) - 1;
		/* pwm_setting.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0 ; */
		pwm_set_spec_config(&pwm_setting);
	} else if (level > 32 && level <= 64) {
		pwm_setting.PWM_MODE_FIFO_REGS.GUARD_VALUE = 1;
		level -= 32;
		pwm_setting.PWM_MODE_FIFO_REGS.SEND_DATA0 = (1 << level) - 1;
		/* pwm_setting.PWM_MODE_FIFO_REGS.SEND_DATA0 =  0xFFFFFFFF ; */
		/* pwm_setting.PWM_MODE_FIFO_REGS.SEND_DATA1 = (1 << level) - 1; */
		pwm_set_spec_config(&pwm_setting);
	} else {
		LEDS_DEBUG("[LED]Error level in backlight\n");
		/* mt_set_pwm_disable(pwm_setting.pwm_no); */
		/* mt_pwm_power_off(pwm_setting.pwm_no); */
		mt_pwm_disable(pwm_setting.pwm_no, config_data->pmic_pad);
	}
	/* pr_info("[LED]PWM con register is %x\n", INREG32(PWM_BASE + 0x0150)); */
	return 0;
}

void mt_led_pwm_disable(int pwm_num)
{
	struct cust_mt65xx_led *cust_led_list = mt_get_cust_led_list();
	mt_pwm_disable(pwm_num, cust_led_list->config_data.pmic_pad);
}

void mt_backlight_set_pwm_duty(int pwm_num, u32 level, u32 div, struct PWM_config *config_data)
{
	mt_backlight_set_pwm(pwm_num, level, div, config_data);
}

void mt_backlight_set_pwm_div(int pwm_num, u32 level, u32 div, struct PWM_config *config_data)
{
	mt_backlight_set_pwm(pwm_num, level, div, config_data);
}

void mt_backlight_get_pwm_fsel(unsigned int bl_div, unsigned int *bl_frequency)
{

}

void mt_store_pwm_register(unsigned int addr, unsigned int value)
{

}

unsigned int mt_show_pwm_register(unsigned int addr)
{
	return 0;
}

#if 0
static void pmic_isink_power_set(int enable)
{
	int i = 0;
	struct cust_mt65xx_led *cust_led_list = mt_get_cust_led_list();
	if (enable) {
		for (i = 0; i < MT65XX_LED_TYPE_TOTAL; i++) {
			if ((cust_led_list[i].mode == MT65XX_LED_MODE_PMIC)
			    && (cust_led_list[i].data == MT65XX_LED_PMIC_LCD_ISINK0
				|| cust_led_list[i].data == MT65XX_LED_PMIC_LCD_ISINK1
				|| cust_led_list[i].data == MT65XX_LED_PMIC_LCD_ISINK2
				|| cust_led_list[i].data == MT65XX_LED_PMIC_LCD_ISINK3)) {
				upmu_set_rg_drv_2m_ck_pdn(0);
				break;
			}
		}
		for (i = 0; i < MT65XX_LED_TYPE_TOTAL; i++) {
			if ((cust_led_list[i].mode == MT65XX_LED_MODE_PMIC)
			    && (cust_led_list[i].data == MT65XX_LED_PMIC_NLED_ISINK0
				|| cust_led_list[i].data == MT65XX_LED_PMIC_NLED_ISINK1
				|| cust_led_list[i].data == MT65XX_LED_PMIC_NLED_ISINK2
				|| cust_led_list[i].data == MT65XX_LED_PMIC_NLED_ISINK3)) {
				upmu_set_rg_drv_32k_ck_pdn(0);
				break;
			}
		}
	} else {
		upmu_set_rg_drv_2m_ck_pdn(1);
		upmu_set_rg_drv_32k_ck_pdn(1);
	}
}

/* define for 15 level mapping(backlight controlled by PMIC ISINK channel) */

static void brightness_set_pmic_isink_duty(unsigned int level)
{
	if (level < 11) {
		upmu_set_isink_dim0_duty(level);
	} else {
		switch (level) {
		case 11:
			upmu_set_isink_dim0_duty(12);
			break;
		case 12:
			upmu_set_isink_dim0_duty(16);
			break;
		case 13:
			upmu_set_isink_dim0_duty(20);
			break;
		case 14:
			upmu_set_isink_dim0_duty(25);
			break;
		case 15:
			upmu_set_isink_dim0_duty(31);
			break;
		default:
			LEDS_DEBUG("mapping BLK level is error for ISINK!\n");
			break;
		}
	}
}
#endif
int mt_brightness_set_pmic(enum mt65xx_led_pmic pmic_type, u32 level, u32 div)
{
	/* int tmp_level = level; */
	/* static bool backlight_init_flag[2] = {false, false}; */
	static bool led_init_flag[3] = { false, false, false };
	static bool first_time = true;

	LEDS_DEBUG("[LED]PMIC#%d:%d\n", pmic_type, level);

	if (pmic_type == MT65XX_LED_PMIC_LCD_ISINK) {
#if 0
		if (backlight_init_flag[0] == false) {
			hwBacklightISINKTuning(1, PMIC_PWM_0, 0x3, 0);
			hwBacklightISINKTuning(2, PMIC_PWM_0, 0x3, 0);
			hwBacklightISINKTuning(3, PMIC_PWM_0, 0x3, 0);
			backlight_init_flag[0] = true;
		}

		if (level) {
			level = brightness_mapping(tmp_level);
			if (level == ERROR_BL_LEVEL)
				level = tmp_level / 17;
			hwPWMsetting(PMIC_PWM_0, level, div);
			upmu_top2_bst_drv_ck_pdn(0x0);
			hwBacklightISINKTurnOn(1);
			hwBacklightISINKTurnOn(2);
			hwBacklightISINKTurnOn(3);
			bl_duty = level;
		} else {
			hwBacklightISINKTurnOff(1);
			hwBacklightISINKTurnOff(2);
			hwBacklightISINKTurnOff(3);
			bl_duty = level;
		}
		return 0;
	} else if (pmic_type == MT65XX_LED_PMIC_LCD_BOOST) {
		/*
		   if(backlight_init_flag[1] == false)
		   {
		   hwBacklightBoostTuning(PMIC_PWM_0, 0xC, 0);
		   backlight_init_flag[1] = true;
		   }
		 */
		if (level) {
			level = brightness_mapping(tmp_level);
			if (level == ERROR_BL_LEVEL)
				level = tmp_level / 42;

			upmu_boost_isink_hw_sel(0x1);
			upmu_boost_mode(3);
			upmu_boost_cabc_en(0);

			switch (level) {
			case 0:
				upmu_boost_vrsel(0x0);
				/* hwPWMsetting(PMIC_PWM_0, 0, 0x15); */
				break;
			case 1:
				upmu_boost_vrsel(0x1);
				/* hwPWMsetting(PMIC_PWM_0, 4, 0x15); */
				break;
			case 2:
				upmu_boost_vrsel(0x2);
				/* hwPWMsetting(PMIC_PWM_0, 5, 0x15); */
				break;
			case 3:
				upmu_boost_vrsel(0x3);
				/* hwPWMsetting(PMIC_PWM_0, 6, 0x15); */
				break;
			case 4:
				upmu_boost_vrsel(0x5);
				/* hwPWMsetting(PMIC_PWM_0, 7, 0x15); */
				break;
			case 5:
				upmu_boost_vrsel(0x8);
				/* hwPWMsetting(PMIC_PWM_0, 8, 0x15); */
				break;
			case 6:
				upmu_boost_vrsel(0xB);
				/* hwPWMsetting(PMIC_PWM_0, 9, 0x15); */
				break;
			default:
				pr_info("[LED] invalid brightness level %d->%d\n", tmp_level,
					level);
				break;
			}

			upmu_top2_bst_drv_ck_pdn(0x0);
			upmu_boost_en(0x1);
			bl_duty = level;
		} else {
			upmu_boost_en(0x0);
			bl_duty = level;
			/* upmu_top2_bst_drv_ck_pdn(0x1); */
		}
		return 0;
#endif
	}

	else if (pmic_type == MT65XX_LED_PMIC_BUTTON) {

		if (level) {
			upmu_set_kpled_dim_duty(0x9);
			upmu_set_kpled_en(0x1);
		} else {
			upmu_set_kpled_en(0x0);
		}
		return 0;

	} else if (pmic_type == MT65XX_LED_PMIC_NLED_ISINK0) {
		if (first_time == true) {
			upmu_set_isinks_ch1_en(0x0);	/* sw workround for sync leds status */
			upmu_set_isinks_ch2_en(0x0);
			first_time = false;
		}
		/* hwBacklightISINKTuning(0x0, 0x3, 0x0, 0);  //register mode, ch0_step=4ma ,disable CABC */
		/* hwBacklightISINKTurnOn(0x0);  //turn on ISINK0 77 uses ISINK4&5 */


		/* if(led_init_flag[0] == false) */
		{
			/* hwBacklightISINKTuning(MT65XX_LED_PMIC_NLED_ISINK4, PMIC_PWM_1, 0x0, 0); */
			upmu_set_isinks_ch0_mode(PMIC_PWM_0);
			upmu_set_isinks_ch0_step(0x0);	/* 4mA */
			/* hwPWMsetting(PMIC_PWM_1, 15, 8); */
			upmu_set_isink_dim0_duty(15);
			upmu_set_isink_dim0_fsel(11);	/* 6320 0.25KHz */
			led_init_flag[0] = true;
		}

		if (level) {
			/* upmu_top2_bst_drv_ck_pdn(0x0); */
			/* upmu_set_rg_spk_div_pdn(0x01); */
			/* upmu_set_rg_spk_pwm_div_pdn(0x01); */

			upmu_set_rg_bst_drv_1m_ck_pdn(0x0);
#ifdef ISINK_CHOP_CLK
			/* upmu_set_isinks0_chop_en(0x1); */
#endif
			upmu_set_isinks_ch0_en(0x01);

		} else {
			upmu_set_isinks_ch0_en(0x00);
			/* upmu_set_rg_bst_drv_1m_ck_pdn(0x1); */
			/* upmu_top2_bst_drv_ck_pdn(0x1); */
		}
		return 0;
	} else if (pmic_type == MT65XX_LED_PMIC_NLED_ISINK1) {
		if (first_time == true) {
			upmu_set_isinks_ch0_en(0);	/* sw workround for sync leds status */
			upmu_set_isinks_ch2_en(0);
			first_time = false;
		}
		/* hwBacklightISINKTuning(0x0, 0x3, 0x0, 0);  //register mode, ch0_step=4ma ,disable CABC */
		/* hwBacklightISINKTurnOn(0x0);  //turn on ISINK0 */

		/* if(led_init_flag[1] == false) */
		{
			/* hwBacklightISINKTuning(MT65XX_LED_PMIC_NLED_ISINK5, PMIC_PWM_2, 0x0, 0); */
			upmu_set_isinks_ch1_mode(PMIC_PWM_0);
			upmu_set_isinks_ch1_step(0x3);	/* 4mA */
			/* hwPWMsetting(PMIC_PWM_2, 15, 8); */
			upmu_set_isink_dim1_duty(15);
			upmu_set_isink_dim1_fsel(11);	/* 6320 0.25KHz */
			led_init_flag[1] = true;
		}
		if (level) {
			/* upmu_top2_bst_drv_ck_pdn(0x0); */

			/* upmu_set_rg_spk_div_pdn(0x01); */
			/* upmu_set_rg_spk_pwm_div_pdn(0x01); */
			upmu_set_rg_bst_drv_1m_ck_pdn(0x0);
#ifdef ISINK_CHOP_CLK
			/* upmu_set_isinks1_chop_en(0x1); */
#endif
			upmu_set_isinks_ch1_en(0x01);
		} else {
			upmu_set_isinks_ch1_en(0x00);
			/* upmu_set_rg_bst_drv_1m_ck_pdn(0x1); */
			/* upmu_top2_bst_drv_ck_pdn(0x1); */
		}
		return 0;
	} else if (pmic_type == MT65XX_LED_PMIC_NLED_ISINK2) {

		if (first_time == true) {
			upmu_set_isinks_ch0_en(0);	/* sw workround for sync leds status */
			upmu_set_isinks_ch1_en(0);
			first_time = false;
		}
		/* hwBacklightISINKTuning(0x0, 0x3, 0x0, 0);  //register mode, ch0_step=4ma ,disable CABC */
		/* hwBacklightISINKTurnOn(0x0);  //turn on ISINK0 */

		/* if(led_init_flag[1] == false) */
		{
			/* hwBacklightISINKTuning(MT65XX_LED_PMIC_NLED_ISINK5, PMIC_PWM_2, 0x0, 0); */
			upmu_set_isinks_ch2_mode(PMIC_PWM_0);
			upmu_set_isinks_ch2_step(0x3);	/* 16mA */
			/* hwPWMsetting(PMIC_PWM_2, 15, 8); */
			upmu_set_isink_dim2_duty(15);
			upmu_set_isink_dim2_fsel(11);	/* 6320 0.25KHz */
			led_init_flag[2] = true;
		}
		if (level) {
			/* upmu_top2_bst_drv_ck_pdn(0x0); */

			/* upmu_set_rg_spk_div_pdn(0x01); */
			/* upmu_set_rg_spk_pwm_div_pdn(0x01); */
			upmu_set_rg_bst_drv_1m_ck_pdn(0x0);
#ifdef ISINK_CHOP_CLK
			/* upmu_set_isinks2_chop_en(0x1); */
#endif
			upmu_set_isinks_ch2_en(0x01);
		} else {
			upmu_set_isinks_ch2_en(0x00);
			/* upmu_set_rg_bst_drv_1m_ck_pdn(0x1); */
			/* upmu_top2_bst_drv_ck_pdn(0x1); */
		}
		return 0;
	} else if (pmic_type == MT65XX_LED_PMIC_NLED_ISINK01) {

		/* hwBacklightISINKTuning(0x0, 0x3, 0x0, 0);  //register mode, ch0_step=4ma ,disable CABC */
		/* hwBacklightISINKTurnOn(0x0);  //turn on ISINK0 */

		/* if(led_init_flag[1] == false) */
		{

			upmu_set_isinks_ch0_mode(PMIC_PWM_0);
			upmu_set_isinks_ch0_step(0x0);	/* 4mA */
			upmu_set_isink_dim0_duty(1);
			upmu_set_isink_dim0_fsel(1);	/* 6320 1.5KHz */
			/* hwBacklightISINKTuning(MT65XX_LED_PMIC_NLED_ISINK5, PMIC_PWM_2, 0x0, 0); */
			upmu_set_isinks_ch1_mode(PMIC_PWM_0);
			upmu_set_isinks_ch1_step(0x3);	/* 4mA */
			/* hwPWMsetting(PMIC_PWM_2, 15, 8); */
			upmu_set_isink_dim1_duty(15);
			upmu_set_isink_dim1_fsel(11);	/* 6320 0.25KHz */
			led_init_flag[0] = true;
			led_init_flag[1] = true;
		}
		if (level) {
			/* upmu_top2_bst_drv_ck_pdn(0x0); */

			/* upmu_set_rg_spk_div_pdn(0x01); */
			/* upmu_set_rg_spk_pwm_div_pdn(0x01); */
			upmu_set_rg_bst_drv_1m_ck_pdn(0x0);
#ifdef ISINK_CHOP_CLK
			/* upmu_set_isinks0_chop_en(0x1); */
			/* upmu_set_isinks1_chop_en(0x1); */
#endif
			upmu_set_isinks_ch0_en(0x01);
			upmu_set_isinks_ch1_en(0x01);
		} else {
			upmu_set_isinks_ch0_en(0x00);
			upmu_set_isinks_ch1_en(0x00);
			/* upmu_set_rg_bst_drv_1m_ck_pdn(0x1); */
			/* upmu_top2_bst_drv_ck_pdn(0x1); */
		}
		return 0;
	}
	return -1;

}

int mt_brightness_set_pmic_duty_store(u32 level, u32 div)
{
	return -1;
}

int mt_mt65xx_led_set_cust(struct cust_mt65xx_led *cust, int level)
{
	struct nled_setting led_tmp_setting = { 0, 0, 0 };
	int tmp_level = level;
	if (level > LED_FULL)
		level = LED_FULL;
	else if (level < 0)
		level = 0;

	pr_info("mt65xx_leds_set_cust: set brightness, name:%s, mode:%d, level:%d\n",
		cust->name, cust->mode, level);
	switch (cust->mode) {

	case MT65XX_LED_MODE_PWM:
		if (strcmp(cust->name, "lcd-backlight") == 0) {
			bl_brightness_hal = level;
			if (level == 0) {
				/* mt_set_pwm_disable(cust->data); */
				/* mt_pwm_power_off (cust->data); */
				mt_pwm_disable(cust->data, cust->config_data.pmic_pad);

			} else {
				level = brightness_mapping(tmp_level);
				if (level == ERROR_BL_LEVEL)
					level = brightness_mapto64(tmp_level);

				mt_backlight_set_pwm(cust->data, level, bl_div_hal,
						     &cust->config_data);
			}
			bl_duty_hal = level;

		} else {
			if (level == 0)
				led_tmp_setting.nled_mode = NLED_OFF;
			else
				led_tmp_setting.nled_mode = NLED_ON;

			mt_led_set_pwm(cust->data, &led_tmp_setting);
		}
		return 1;

	case MT65XX_LED_MODE_GPIO:
		/* pr_info("brightness_set_cust:go GPIO mode!!!!!\n"); */
#ifdef CONTROL_BL_TEMPERATURE
		mutex_lock(&bl_level_limit_mutex);
		current_level = level;
		/* pr_info("brightness_set_cust:current_level=%d\n", current_level); */
		if (0 == limit_flag) {
			last_level = level;
			/* pr_info("brightness_set_cust:last_level=%d\n", last_level); */
		} else {
			if (limit < current_level) {
				level = limit;
				pr_info("backlight_set_cust: control level=%d\n", level);
			}
		}
		mutex_unlock(&bl_level_limit_mutex);
#endif
		return ((cust_set_brightness) (cust->data)) (level);

	case MT65XX_LED_MODE_PMIC:
		return mt_brightness_set_pmic(cust->data, level, bl_div_hal);

	case MT65XX_LED_MODE_CUST_LCM:
		if (strcmp(cust->name, "lcd-backlight") == 0)
			bl_brightness_hal = level;

#ifdef CONTROL_BL_TEMPERATURE
		mutex_lock(&bl_level_limit_mutex);
		current_level = level;
		/* pr_info("brightness_set_cust:current_level=%d\n", current_level); */
		if (0 == limit_flag) {
			last_level = level;
			/* pr_info("brightness_set_cust:last_level=%d\n", last_level); */
		} else {
			if (limit < current_level) {
				level = limit;
				pr_info("backlight_set_cust: control level=%d\n", level);
			}
		}
		mutex_unlock(&bl_level_limit_mutex);
#endif
		/* pr_info("brightness_set_cust:backlight control by LCM\n"); */
		return ((cust_brightness_set) (cust->data)) (level, bl_div_hal);


	case MT65XX_LED_MODE_CUST_BLS_PWM:
		if (strcmp(cust->name, "lcd-backlight") == 0)
			bl_brightness_hal = level;

#ifdef CONTROL_BL_TEMPERATURE
		mutex_lock(&bl_level_limit_mutex);
		current_level = level;
		/* pr_info("brightness_set_cust:current_level=%d\n", current_level); */
		if (0 == limit_flag) {
			last_level = level;
			/* pr_info("brightness_set_cust:last_level=%d\n", last_level); */
		} else {
			if (limit < current_level) {
				level = limit;
				pr_info("backlight_set_cust: control level=%d\n", level);
			}
		}
		mutex_unlock(&bl_level_limit_mutex);
#endif
		/* pr_info("brightness_set_cust:backlight control by BLS_PWM!!\n"); */
		/* #if !defined (MTK_AAL_SUPPORT) */
		return ((cust_set_brightness) (cust->data)) (level);
		pr_info("brightness_set_cust:backlight control by BLS_PWM done!!\n");
		/* #endif */

	case MT65XX_LED_MODE_NONE:
	default:
		break;
	}
	return -1;
}

void mt_mt65xx_led_work(struct work_struct *work)
{
	struct mt65xx_led_data *led_data = container_of(work, struct mt65xx_led_data, work);

	LEDS_DEBUG("[LED]%s:%d\n", led_data->cust.name, led_data->level);
	mutex_lock(&leds_mutex);
	mt_mt65xx_led_set_cust(&led_data->cust, led_data->level);
	mutex_unlock(&leds_mutex);
}

void mt_mt65xx_led_set(struct led_classdev *led_cdev, enum led_brightness level)
{
	struct mt65xx_led_data *led_data = container_of(led_cdev, struct mt65xx_led_data, cdev);

	/* do something only when level is changed */
	if (led_data->level != level) {
		led_data->level = level;
		if (strcmp(led_data->cust.name, "lcd-backlight")) {
			schedule_work(&led_data->work);
		} else {
			LEDS_DEBUG("[LED]Set Backlight directly %d at time %lu\n", led_data->level,
				   jiffies);
			mt_mt65xx_led_set_cust(&led_data->cust, led_data->level);
		}
	}
}

int mt_mt65xx_blink_set(struct led_classdev *led_cdev,
			unsigned long *delay_on, unsigned long *delay_off)
{
	struct mt65xx_led_data *led_data = container_of(led_cdev, struct mt65xx_led_data, cdev);
	static int got_wake_lock;
	struct nled_setting nled_tmp_setting = { 0, 0, 0 };


	/* only allow software blink when delay_on or delay_off changed */
	if (*delay_on != led_data->delay_on || *delay_off != led_data->delay_off) {
		led_data->delay_on = *delay_on;
		led_data->delay_off = *delay_off;
		if (led_data->delay_on && led_data->delay_off) {	/* enable blink */
			led_data->level = 255;	/* when enable blink  then to set the level  (255) */
			/* if(led_data->cust.mode == MT65XX_LED_MODE_PWM && */
			/* (led_data->cust.data != PWM3 && led_data->cust.data != PWM4
			   && led_data->cust.data != PWM5)) */

			/* AP PWM all support OLD mode in MT6589 */

			if (led_data->cust.mode == MT65XX_LED_MODE_PWM) {
				nled_tmp_setting.nled_mode = NLED_BLINK;
				nled_tmp_setting.blink_off_time = led_data->delay_off;
				nled_tmp_setting.blink_on_time = led_data->delay_on;
				mt_led_set_pwm(led_data->cust.data, &nled_tmp_setting);
				return 0;
			}

			else if ((led_data->cust.mode == MT65XX_LED_MODE_PMIC)
				 && (led_data->cust.data == MT65XX_LED_PMIC_NLED_ISINK0
				     || led_data->cust.data == MT65XX_LED_PMIC_NLED_ISINK1
				     || led_data->cust.data == MT65XX_LED_PMIC_NLED_ISINK2)) {
				/* if(get_chip_eco_ver() == CHIP_E2) { */
				if (1) {

					nled_tmp_setting.nled_mode = NLED_BLINK;
					nled_tmp_setting.blink_off_time = led_data->delay_off;
					nled_tmp_setting.blink_on_time = led_data->delay_on;
					mt_led_blink_pmic(led_data->cust.data, &nled_tmp_setting);
					return 0;
				} else {
					wake_lock(&leds_suspend_lock);
				}
			} else if (!got_wake_lock) {
				wake_lock(&leds_suspend_lock);
				got_wake_lock = 1;
			}
		} else if (!led_data->delay_on && !led_data->delay_off) {	/* disable blink */
			/* if(led_data->cust.mode == MT65XX_LED_MODE_PWM && */
			/* (led_data->cust.data != PWM3 && led_data->cust.data != PWM4
			   && led_data->cust.data != PWM5)) */

			/* AP PWM all support OLD mode in MT6589 */

			if (led_data->cust.mode == MT65XX_LED_MODE_PWM) {
				nled_tmp_setting.nled_mode = NLED_OFF;
				mt_led_set_pwm(led_data->cust.data, &nled_tmp_setting);
				return 0;
			}

			else if ((led_data->cust.mode == MT65XX_LED_MODE_PMIC)
				 && (led_data->cust.data == MT65XX_LED_PMIC_NLED_ISINK0
				     || led_data->cust.data == MT65XX_LED_PMIC_NLED_ISINK1
				     || led_data->cust.data == MT65XX_LED_PMIC_NLED_ISINK2)) {
				/* if(get_chip_eco_ver() == CHIP_E2) { */
				if (1) {
					mt_brightness_set_pmic(led_data->cust.data, 0, 0);
					return 0;
				} else {
					wake_unlock(&leds_suspend_lock);
				}
			} else if (got_wake_lock) {
				wake_unlock(&leds_suspend_lock);
				got_wake_lock = 0;
			}
		}
		return -1;
	}
	/* delay_on and delay_off are not changed */
	return 0;
}

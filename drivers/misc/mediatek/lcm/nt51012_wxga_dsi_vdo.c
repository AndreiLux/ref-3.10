#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/gpio.h>
#else
#include <string.h>
#endif
#include "lcm_drv.h"

#include <lcm_drv_mt8135.h>
#ifdef BUILD_LK
#include <platform/mt_gpio_def.h>
#include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
#else
#include <mach/mt_gpio_def.h>
#include <mach/mt_pm_ldo.h>
#endif

#include <linux/leds.h>
#include <linux/err.h>
#include <linux/lp855x.h>

/* --------------- */
/* Local Constants */
/* --------------- */

#define FRAME_WIDTH  (800)
#define FRAME_HEIGHT (1280)

#define GPIO_LED_TST         65 /* Reserve for panel power switch */
#define GPIO_LCM_PMU_EN      66
#define GPIO_BKLT_EN         76
#define GPIO_LCDPANEL_EN     110 /* Reserve for CABC */

#define UNDEF		0x0		/* undefined */
#define CMI			0x1
#define LGD			0x2
#define PLD			0x3

/* CABC Mode Selection */
#define OFF			0x0
#define UI			0x1
#define STILL		0x2
#define MOVING		0x3

/* ---------------- */
/*  Local Variables */
/* ---------------- */

static unsigned int vendor_id = 0x0;
static unsigned char color_sat_enabled;

static LCM_UTIL_FUNCS lcm_util = {
	.set_reset_pin = NULL,
	.udelay = NULL,
	.mdelay = NULL,
};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

/* --------------- */
/* Local Functions */
/* --------------- */

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	     lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		 lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) \
		 lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		 lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
		 lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		 lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define LCM_DSI_CMD_MODE	0

static char *get_vendor_type(void)
{
	switch (vendor_id) {
	case UNDEF: return "UNDEFINED\0";
	case CMI: return "CMI\0";
	case LGD: return "LGD\0";
	case PLD: return "PLD\0";
	default: return "Unknown\0";
	}
}

static void get_vendor_id(void)
{
	unsigned char buffer[1];
	unsigned int array[16];

	array[0] = 0x00013700; /* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0xFC, buffer, 1);
	vendor_id = buffer[0] >> 4 & 0x03;
#if 0
#ifdef BUILD_LK
	printf("[LK/LCM][nt51012] %s, vendor id = 0x%x\n", __func__, vendor_id);
#else
	pr_info("[nt51012] %s, vendor id = 0x%x\n", __func__, vendor_id);
#endif
#endif
}

static void lcm_regs_config(void)
{
	unsigned int data_array[16];

	data_array[0] = 0x00011500;	/* software reset */
	dsi_set_cmdq(data_array, 1, 1);

	MDELAY(20);

	if (vendor_id < 0x1 || vendor_id > 0x3)
		get_vendor_id();

	#ifdef BUILD_LK
	printf("[LK/LCM] %s, vendor type: %s\n", __func__, get_vendor_type());
	#else
	pr_info("[nt51012] %s, vendor type: %s\n", __func__, get_vendor_type());
	#endif

	switch (vendor_id) {
	case CMI:
		/* set it to 200ohm_200ohm */
		data_array[0] = 0x0DAE1500;
		break;
	case LGD:
		/* set it to open_100ohm */
		data_array[0] = 0x07AE1500;
		break;
	case PLD:
		/* set it to 200ohm_200ohm */
		data_array[0] = 0x0DAE1500;
		break;
	case UNDEF:
	default:
		/* set it to 100ohm_open */
		data_array[0] = 0x0BAE1500;
		break;
	}
	data_array[1] = 0xEAEE1500;
	data_array[2] = 0x5FEF1500;
	data_array[3] = 0x68F21500;
	dsi_set_cmdq(data_array, 4, 1);

	switch (vendor_id) {
	case CMI:
		/* TCLV_OE timing adjust for Gate Pulse
		 * Modulation Function
		 */
		data_array[0] = 0x7FB21500;
		dsi_set_cmdq(data_array, 1, 1);
		break;
	case LGD:
		/* TCLV_OE timing adjust for Gate Pulse
		 * Modulation Function
		 */
		data_array[0] = 0x7DB21500;
		/* Gate OE width control which secures data
		 * charging time margin
		 */
		data_array[1] = 0x18B61500;
		/* AVDDG off */
		data_array[2] = 0x64D21500;
		dsi_set_cmdq(data_array, 3, 1);
		break;
	case PLD:
		/* Selection of amplifier */
		data_array[0] = 0x02BE1500;
		/* Adjust drive timing 1 */
		data_array[1] = 0x90B51500;
		/* Adjust drive timing 2 */
		data_array[2] = 0x09B61500;
		dsi_set_cmdq(data_array, 3, 1);
		break;
	case UNDEF:
	default:
		/* TCLV_OE timing adjust for Gate Pulse
		 * Modulation Function
		 */
		data_array[0] = 0x7DB21500;
		/* Gate OE width control which secures data
		 * charging time margin
		 */
		data_array[1] = 0x18B61500;
		/* AVDDG off */
		data_array[2] = 0x64D21500;
		dsi_set_cmdq(data_array, 3, 1);
		break;
	}

	/* Color saturation */
	if (color_sat_enabled) {
		data_array[0] = 0x03B31500;
		data_array[1] = 0x04C81500;
		dsi_set_cmdq(data_array, 2, 1);
	}

	data_array[0] = 0x00EE1500;
	data_array[1] = 0x00EF1500;
	dsi_set_cmdq(data_array, 2, 1);

	/* BIST */
	/* data_array[0] = 0xEFB11500; */
	/* dsi_set_cmdq(data_array, 1, 1); */
	/* MDELAY(1); */

	/* data_array[0] = 0x00290500;  //display on */
	/* dsi_set_cmdq(data_array, 1, 1); */
}

/* -------------------------- */
/* LCM Driver Implementations */
/* -------------------------- */

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{

	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

#if 0
	/* enable tearing-free */
	params->dbi.te_mode = LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity = LCM_POLARITY_RISING;
#endif

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_EVENT_VDO_MODE;
#endif

	/* DSI */
	/* Command mode setting */
	/* 1 Three lane or Four lane */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	/* Not support in MT6573 */
	params->dsi.packet_size = 256;

	/* Video mode setting */
	params->dsi.intermediat_buffer_num = 0;
	/* because DSI/DPI HW design change,
	 * this parameters should be 0 when video mode in MT658X;
	 * or memory leakage */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count = 720 * 3;

	params->dsi.vertical_sync_active = 1;
	params->dsi.vertical_backporch = 10;
	params->dsi.vertical_frontporch = 10;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 1;
	params->dsi.horizontal_backporch = 37;
	params->dsi.horizontal_frontporch = 23;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	params->dsi.PLL_CLOCK = LCM_DSI_6589_PLL_CLOCK_214_5;

	params->dsi.CLK_ZERO = 47;
	params->dsi.HS_ZERO = 36;
	/*params->dsi.HS_TRAIL = 7;*/

	/* ARIEL-3684 */
	params->dsi.cust_impendence_val = 0x0F;

	/* The values are in mm.
	 * Add these values for reporting corect xdpi and ydpi values
	 * For Aston xdpi and ydpi should be 216. */
	params->active_width = 94;
	params->active_height = 151;
}

static void lcm_reset(void)
{
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	SET_RESET_PIN(1);
}

static void lcm_set_backlight(struct led_classdev *bldev, unsigned int level)
{
	pr_info("[nt51012] %s\n", __func__);
	lp855x_led_set_brightness(bldev, level);
	/* T6: VLED falling slowly, thus wait more time to send lcm sleep cmd */
	MDELAY(40);
}

static void lcm_set_backlight_mode(unsigned int mode)
{
	unsigned int array[16];
	#ifdef BUILD_LK
	/* do nothing */
	#else
	switch (mode) {
	case OFF:
		pr_debug("%s: Switching OFF CABC\n", __func__);
		array[0] = 0xFEB01500;
		break;
	case UI:
		pr_debug("%s: Switching ON CABC UI mode\n", __func__);
		array[0] = 0xBEB01500;
		break;
	case STILL:
		pr_debug("%s: Switching ON CABC STILL mode\n", __func__);
		array[0] = 0x7EB01500;
		break;
	case MOVING:
		pr_debug("%s: Switching ON CABC MOVING mode\n", __func__);
		array[0] = 0x3EB01500;
		break;
	default:
		pr_info("[nt51012] CABC mode not supported\n");
		array[0] = 0xFEB01500;
		break;
	}
	dsi_set_cmdq(array, 1, 1);
	#endif
}

static void lcm_init(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM][Aston] lcm_init() enter\n");

	lcm_pmu_enable();

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	SET_RESET_PIN(1);

	lcm_regs_config();
#else
	get_vendor_id();
	pr_info("[Aston] %s enter, skip power_on & init lcm since it's done by lk\n",
			 __func__);
	pr_info("[nt51012] vendor type: %s\n", get_vendor_type());
#endif
}

static void lcm_res_init(void)
{
	mt_pin_set_mode_gpio(GPIO_LED_TST);
	mt_pin_set_mode_gpio(GPIO_LCM_PMU_EN);
	mt_pin_set_mode_gpio(GPIO_LCDPANEL_EN);

	gpio_request(GPIO_LED_TST, "LCM-RST");
	gpio_request(GPIO_LCM_PMU_EN, "LCM-PMU");
	gpio_request(GPIO_LCDPANEL_EN, "LCM-LED");
}

static void lcm_res_exit(void)
{
	gpio_free(GPIO_LED_TST);
	gpio_free(GPIO_LCM_PMU_EN);
	gpio_free(GPIO_LCDPANEL_EN);
}

static void lcm_suspend(void)
{
	unsigned int data_array[16];

#ifdef BUILD_LK
	printf("[LK/LCM] %s\n", __func__);
#else
	pr_info("[nt51012] %s\n", __func__);
#endif

	/* Display Off */
	data_array[0] = 0x00280500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(20);

	/* Sleep In */
	data_array[0] = 0x00111500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(20);
}


static void lcm_resume(void)
{

	/* unsigned int data_array[16]; */
#ifdef BUILD_LK
	printf("[LK/LCM] %s\n", __func__);
#else
	pr_info("[nt51012] %s\n", __func__);
#endif

	lcm_reset();

	lcm_regs_config();

	/* Sleep Out */
	/* data_array[0] = 0x00101500;
	dsi_set_cmdq(data_array, 1, 1); */
	MDELAY(20);

	/* Display On */
	/* data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1); */

}

/* Seperate lcm_resume_power and lcm_reset from power_on func,
 * to meet turn on MIPI lanes after AVEE ready,
 * before Reset pin Low->High */
static void lcm_resume_power(void)
{
	#ifdef BUILD_LK
		dprintf(ALWAYS, "[LK/LCM] GPIO110/65/66 - Control PANEL/VSYS/PWM\n");
	#else
		pr_info("[nt51012] %s, GPIO110/65/66 - Control PANEL/VSYS/PWM\n",
				__func__);
	#endif
	gpio_direction_output(GPIO_LED_TST, 1);

	gpio_direction_output(GPIO_LCDPANEL_EN, 1);

	gpio_direction_output(GPIO_LCM_PMU_EN, 1);
	MDELAY(60);

}

/* Seperate lcm_suspend_power from lcm_suspend
* to meet turn off MIPI lanes after after sleep in cmd,
* then Reset High->Low, GHGL_EN High->Low
*/
static void lcm_suspend_power(void)
{
	MDELAY(50);
	gpio_direction_output(GPIO_LCM_PMU_EN, 0);

	gpio_direction_output(GPIO_LED_TST, 0);

	gpio_direction_output(GPIO_LCDPANEL_EN, 0);
	MDELAY(10);
}

#if (LCM_DSI_CMD_MODE)
static void lcm_update(
			unsigned int x,
			unsigned int y,
			unsigned int width,
			unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00290508;	/* HW bug, so need send one HS packet */
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

}
#endif

LCM_DRIVER nt51012_wxga_dsi_vdo_lcm_drv = {
	.name = "nt51012_wxga_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.res_init = lcm_res_init,
	.res_exit = lcm_res_exit,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.set_backlight	= lcm_set_backlight,
	.set_backlight_mode	= lcm_set_backlight_mode,
	/* .compare_id     = lcm_compare_id, */
	.resume_power  = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
#if (LCM_DSI_CMD_MODE)
	.update = lcm_update,
#endif
};

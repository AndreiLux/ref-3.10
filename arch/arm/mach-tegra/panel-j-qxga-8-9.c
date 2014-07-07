/*
 * arch/arm/mach-tegra/panel-j-qxga-8-9.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <mach/dc.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/tegra_dsi_backlight.h>
#include <linux/leds.h>
#include <linux/ioport.h>
#include <linux/export.h>
#include <linux/of_gpio.h>

#include <generated/mach-types.h>

#include "board.h"
#include "board-panel.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra11_host1x_devices.h"

#define TEGRA_DSI_GANGED_MODE	1

#define DSI_PANEL_RESET		0

#define DC_CTRL_MODE	(TEGRA_DC_OUT_CONTINUOUS_MODE  |\
			TEGRA_DC_OUT_INITIALIZED_MODE)

enum panel_gpios {
	IOVDD_1V8 = 0,
	AVDD_4V,
	DCDC_EN,
	LCM_RST,
	NUM_PANEL_GPIOS,
};

static bool gpio_requested;
static struct platform_device *disp_device;

static int iovdd_1v8, avdd_4v, dcdc_en, lcm_rst;

static struct gpio panel_init_gpios[] = {
	{TEGRA_GPIO_PQ2,	GPIOF_OUT_INIT_HIGH,    "lcmio_1v8"},
	{TEGRA_GPIO_PR0,	GPIOF_OUT_INIT_HIGH,    "avdd_4v"},
	{TEGRA_GPIO_PEE5,	GPIOF_OUT_INIT_HIGH,    "dcdc_en"},
	{TEGRA_GPIO_PH5,	GPIOF_OUT_INIT_HIGH,	"panel_rst"},
};

static struct tegra_dc_sd_settings dsi_j_qxga_8_9_sd_settings = {
        .enable = 0, /* disabled by default. */
        .use_auto_pwm = false,
        .hw_update_delay = 0,
        .bin_width = -1,
        .aggressiveness = 5,
        .use_vid_luma = false,
        .phase_in_adjustments = 0,
        .k_limit_enable = true,
        .k_limit = 200,
        .sd_window_enable = false,
        .soft_clipping_enable = true,
        /* Low soft clipping threshold to compensate for aggressive k_limit */
        .soft_clipping_threshold = 128,
        .smooth_k_enable = false,
        .smooth_k_incr = 64,
        /* Default video coefficients */
        .coeff = {5, 9, 2},
        .fc = {0, 0},
        /* Immediate backlight changes */
        .blp = {1024, 255},
        /* Gammas: R: 2.2 G: 2.2 B: 2.2 */
        /* Default BL TF */
        .bltf = {
                        {
                                {57, 65, 73, 82},
                                {92, 103, 114, 125},
                                {138, 150, 164, 178},
                                {193, 208, 224, 241},
                        },
                },
        /* Default LUT */
        .lut = {
                        {
                                {255, 255, 255},
                                {199, 199, 199},
                                {153, 153, 153},
                                {116, 116, 116},
                                {85, 85, 85},
                                {59, 59, 59},
                                {36, 36, 36},
                                {17, 17, 17},
                                {0, 0, 0},
                        },
                },
        .sd_brightness = &sd_brightness,
        .use_vpulse2 = true,
};

static struct tegra_dsi_cmd dsi_j_qxga_8_9_init_cmd[] = {
	DSI_CMD_VBLANK_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, 0x0, CMD_NOT_CLUBBED),
	DSI_DLY_MS(120),
	DSI_CMD_VBLANK_SHORT(DSI_DCS_WRITE_1_PARAM, 0x53, 0x24, CMD_CLUBBED),
	DSI_CMD_VBLANK_SHORT(DSI_DCS_WRITE_1_PARAM, 0x55, 0x00, CMD_CLUBBED),
	DSI_CMD_VBLANK_SHORT(DSI_DCS_WRITE_1_PARAM, 0x35, 0x00, CMD_CLUBBED),
	DSI_CMD_VBLANK_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, 0x0, CMD_CLUBBED),
};

static struct tegra_dsi_cmd dsi_j_qxga_8_9_suspend_cmd[] = {
	DSI_CMD_VBLANK_SHORT(DSI_DCS_WRITE_1_PARAM, 0x51, 0x00, CMD_CLUBBED),
	DSI_CMD_VBLANK_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_OFF, 0x0, CMD_CLUBBED),
	DSI_CMD_VBLANK_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_ENTER_SLEEP_MODE, 0x0, CMD_CLUBBED),
	DSI_SEND_FRAME(3),
	DSI_CMD_VBLANK_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xB0, 0x04, CMD_CLUBBED),
	DSI_CMD_VBLANK_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xB1, 0x01, CMD_CLUBBED),
};

static struct tegra_dsi_out dsi_j_qxga_8_9_pdata = {
	.controller_vs = DSI_VS_1,

	.n_data_lanes = 8,

#if DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE,
	.ganged_type = TEGRA_DSI_GANGED_SYMMETRIC_LEFT_RIGHT,
	.suspend_aggr = DSI_HOST_SUSPEND_LV2,
	.refresh_rate = 61,
	.rated_refresh_rate = 60,
	.te_polarity_low = true,
#else
	.ganged_type = TEGRA_DSI_GANGED_SYMMETRIC_LEFT_RIGHT,
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE,
	.refresh_rate = 60,
#endif

	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_TX_ONLY,
	.dsi_init_cmd = dsi_j_qxga_8_9_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_j_qxga_8_9_init_cmd),
	.dsi_suspend_cmd = dsi_j_qxga_8_9_suspend_cmd,
	.n_suspend_cmd = ARRAY_SIZE(dsi_j_qxga_8_9_suspend_cmd),
	.lp00_pre_panel_wakeup = false,
	.ulpm_not_supported = true,
	.no_pkt_seq_hbp = false,
};

static int dsi_j_qxga_8_9_gpio_get(void)
{
	int err;

	if (gpio_requested)
		return 0;

	err = gpio_request_array(panel_init_gpios, ARRAY_SIZE(panel_init_gpios));
	if(err) {
		pr_err("gpio array request failed\n");
		return err;
	}

	gpio_requested = true;

	return 0;
}

static int dsi_j_qxga_8_9_postpoweron(struct device *dev)
{
	int err;

	err = dsi_j_qxga_8_9_gpio_get();
	if (err) {
		pr_err("failed to get panel gpios\n");
		return err;
	}

	gpio_set_value(avdd_4v, 1);
	msleep(1);
	gpio_set_value(dcdc_en, 1);
	msleep(15);
	gpio_set_value(lcm_rst, 1);
	msleep(15);

	return err;
}

static int dsi_j_qxga_8_9_enable(struct device *dev)
{
	gpio_set_value(iovdd_1v8, 1);
	msleep(15);
	return 0;
}

static int dsi_j_qxga_8_9_disable(void)
{

	gpio_set_value(lcm_rst, 0);
	msleep(1);
	gpio_set_value(dcdc_en, 0);
	msleep(15);
	gpio_set_value(avdd_4v, 0);
	gpio_set_value(iovdd_1v8, 0);
	msleep(10);

	return 0;
}

static int dsi_j_qxga_8_9_postsuspend(void)
{
	return 0;
}

static struct tegra_dc_mode dsi_j_qxga_8_9_modes[] = {
	{
#if DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE
		.pclk = 294264000, /* @61Hz */
		.h_ref_to_sync = 0,

		/* dc constraint, min 1 */
		.v_ref_to_sync = 1,

		.h_sync_width = 32,

		/* dc constraint, min 1 */
		.v_sync_width = 1,

		.h_back_porch = 80,

		/* panel constraint, send frame after TE deassert */
		.v_back_porch = 5,

		.h_active = 2560,
		.v_active = 1600,
		.h_front_porch = 328,

		/* dc constraint, min v_ref_to_sync + 1 */
		.v_front_porch = 2,
#else
		.pclk = 257000000, /* @60Hz */
		.h_ref_to_sync = 1,
		.v_ref_to_sync = 1,
		.h_sync_width = 62,
		.v_sync_width = 4,
		.h_back_porch = 152,
		.v_back_porch = 8,
		.h_active = 768 * 2,
		.v_active = 2048,
		.h_front_porch = 322,
		.v_front_porch = 12,
#endif
	},
};

#ifdef CONFIG_TEGRA_DC_CMU
static struct tegra_dc_cmu dsi_j_qxga_8_9_cmu = {
	/* lut1 maps sRGB to linear space. */
	{
	0,    1,    2,    4,    5,    6,    7,    9,
	10,   11,   12,   14,   15,   16,   18,   20,
	21,   23,   25,   27,   29,   31,   33,   35,
	37,   40,   42,   45,   48,   50,   53,   56,
	59,   62,   66,   69,   72,   76,   79,   83,
	87,   91,   95,   99,   103,  107,  112,  116,
	121,  126,  131,  136,  141,  146,  151,  156,
	162,  168,  173,  179,  185,  191,  197,  204,
	210,  216,  223,  230,  237,  244,  251,  258,
	265,  273,  280,  288,  296,  304,  312,  320,
	329,  337,  346,  354,  363,  372,  381,  390,
	400,  409,  419,  428,  438,  448,  458,  469,
	479,  490,  500,  511,  522,  533,  544,  555,
	567,  578,  590,  602,  614,  626,  639,  651,
	664,  676,  689,  702,  715,  728,  742,  755,
	769,  783,  797,  811,  825,  840,  854,  869,
	884,  899,  914,  929,  945,  960,  976,  992,
	1008, 1024, 1041, 1057, 1074, 1091, 1108, 1125,
	1142, 1159, 1177, 1195, 1213, 1231, 1249, 1267,
	1286, 1304, 1323, 1342, 1361, 1381, 1400, 1420,
	1440, 1459, 1480, 1500, 1520, 1541, 1562, 1582,
	1603, 1625, 1646, 1668, 1689, 1711, 1733, 1755,
	1778, 1800, 1823, 1846, 1869, 1892, 1916, 1939,
	1963, 1987, 2011, 2035, 2059, 2084, 2109, 2133,
	2159, 2184, 2209, 2235, 2260, 2286, 2312, 2339,
	2365, 2392, 2419, 2446, 2473, 2500, 2527, 2555,
	2583, 2611, 2639, 2668, 2696, 2725, 2754, 2783,
	2812, 2841, 2871, 2901, 2931, 2961, 2991, 3022,
	3052, 3083, 3114, 3146, 3177, 3209, 3240, 3272,
	3304, 3337, 3369, 3402, 3435, 3468, 3501, 3535,
	3568, 3602, 3636, 3670, 3705, 3739, 3774, 3809,
	3844, 3879, 3915, 3950, 3986, 4022, 4059, 4095,
	},
	/* csc */
	{
		0x105, 0x3D5, 0x024, /* 1.021 -0.164 0.143 */
		0x3EA, 0x121, 0x3C1, /* -0.082 1.128 -0.245 */
		0x002, 0x00A, 0x0F4, /* 0.007 0.038 0.955 */
	},
	/* lut2 maps linear space to sRGB */
	{
		0,    1,    2,    2,    3,    4,    5,    6,
		6,    7,    8,    9,    10,   10,   11,   12,
		13,   13,   14,   15,   15,   16,   16,   17,
		18,   18,   19,   19,   20,   20,   21,   21,
		22,   22,   23,   23,   23,   24,   24,   25,
		25,   25,   26,   26,   27,   27,   27,   28,
		28,   29,   29,   29,   30,   30,   30,   31,
		31,   31,   32,   32,   32,   33,   33,   33,
		34,   34,   34,   34,   35,   35,   35,   36,
		36,   36,   37,   37,   37,   37,   38,   38,
		38,   38,   39,   39,   39,   40,   40,   40,
		40,   41,   41,   41,   41,   42,   42,   42,
		42,   43,   43,   43,   43,   43,   44,   44,
		44,   44,   45,   45,   45,   45,   46,   46,
		46,   46,   46,   47,   47,   47,   47,   48,
		48,   48,   48,   48,   49,   49,   49,   49,
		49,   50,   50,   50,   50,   50,   51,   51,
		51,   51,   51,   52,   52,   52,   52,   52,
		53,   53,   53,   53,   53,   54,   54,   54,
		54,   54,   55,   55,   55,   55,   55,   55,
		56,   56,   56,   56,   56,   57,   57,   57,
		57,   57,   57,   58,   58,   58,   58,   58,
		58,   59,   59,   59,   59,   59,   59,   60,
		60,   60,   60,   60,   60,   61,   61,   61,
		61,   61,   61,   62,   62,   62,   62,   62,
		62,   63,   63,   63,   63,   63,   63,   64,
		64,   64,   64,   64,   64,   64,   65,   65,
		65,   65,   65,   65,   66,   66,   66,   66,
		66,   66,   66,   67,   67,   67,   67,   67,
		67,   67,   68,   68,   68,   68,   68,   68,
		68,   69,   69,   69,   69,   69,   69,   69,
		70,   70,   70,   70,   70,   70,   70,   71,
		71,   71,   71,   71,   71,   71,   72,   72,
		72,   72,   72,   72,   72,   72,   73,   73,
		73,   73,   73,   73,   73,   74,   74,   74,
		74,   74,   74,   74,   74,   75,   75,   75,
		75,   75,   75,   75,   75,   76,   76,   76,
		76,   76,   76,   76,   77,   77,   77,   77,
		77,   77,   77,   77,   78,   78,   78,   78,
		78,   78,   78,   78,   78,   79,   79,   79,
		79,   79,   79,   79,   79,   80,   80,   80,
		80,   80,   80,   80,   80,   81,   81,   81,
		81,   81,   81,   81,   81,   81,   82,   82,
		82,   82,   82,   82,   82,   82,   83,   83,
		83,   83,   83,   83,   83,   83,   83,   84,
		84,   84,   84,   84,   84,   84,   84,   84,
		85,   85,   85,   85,   85,   85,   85,   85,
		85,   86,   86,   86,   86,   86,   86,   86,
		86,   86,   87,   87,   87,   87,   87,   87,
		87,   87,   87,   88,   88,   88,   88,   88,
		88,   88,   88,   88,   88,   89,   89,   89,
		89,   89,   89,   89,   89,   89,   90,   90,
		90,   90,   90,   90,   90,   90,   90,   90,
		91,   91,   91,   91,   91,   91,   91,   91,
		91,   91,   92,   92,   92,   92,   92,   92,
		92,   92,   92,   92,   93,   93,   93,   93,
		93,   93,   93,   93,   93,   93,   94,   94,
		94,   94,   94,   94,   94,   94,   94,   94,
		95,   95,   95,   95,   95,   95,   95,   95,
		95,   95,   96,   96,   96,   96,   96,   96,
		96,   96,   96,   96,   96,   97,   97,   97,
		97,   97,   97,   97,   97,   97,   97,   98,
		98,   98,   98,   98,   98,   98,   98,   98,
		98,   98,   99,   99,   99,   99,   99,   99,
		99,   100,  101,  101,  102,  103,  103,  104,
		105,  105,  106,  107,  107,  108,  109,  109,
		110,  111,  111,  112,  113,  113,  114,  115,
		115,  116,  116,  117,  118,  118,  119,  119,
		120,  120,  121,  122,  122,  123,  123,  124,
		124,  125,  126,  126,  127,  127,  128,  128,
		129,  129,  130,  130,  131,  131,  132,  132,
		133,  133,  134,  134,  135,  135,  136,  136,
		137,  137,  138,  138,  139,  139,  140,  140,
		141,  141,  142,  142,  143,  143,  144,  144,
		145,  145,  145,  146,  146,  147,  147,  148,
		148,  149,  149,  150,  150,  150,  151,  151,
		152,  152,  153,  153,  153,  154,  154,  155,
		155,  156,  156,  156,  157,  157,  158,  158,
		158,  159,  159,  160,  160,  160,  161,  161,
		162,  162,  162,  163,  163,  164,  164,  164,
		165,  165,  166,  166,  166,  167,  167,  167,
		168,  168,  169,  169,  169,  170,  170,  170,
		171,  171,  172,  172,  172,  173,  173,  173,
		174,  174,  174,  175,  175,  176,  176,  176,
		177,  177,  177,  178,  178,  178,  179,  179,
		179,  180,  180,  180,  181,  181,  182,  182,
		182,  183,  183,  183,  184,  184,  184,  185,
		185,  185,  186,  186,  186,  187,  187,  187,
		188,  188,  188,  189,  189,  189,  189,  190,
		190,  190,  191,  191,  191,  192,  192,  192,
		193,  193,  193,  194,  194,  194,  195,  195,
		195,  196,  196,  196,  196,  197,  197,  197,
		198,  198,  198,  199,  199,  199,  200,  200,
		200,  200,  201,  201,  201,  202,  202,  202,
		202,  203,  203,  203,  204,  204,  204,  205,
		205,  205,  205,  206,  206,  206,  207,  207,
		207,  207,  208,  208,  208,  209,  209,  209,
		209,  210,  210,  210,  211,  211,  211,  211,
		212,  212,  212,  213,  213,  213,  213,  214,
		214,  214,  214,  215,  215,  215,  216,  216,
		216,  216,  217,  217,  217,  217,  218,  218,
		218,  219,  219,  219,  219,  220,  220,  220,
		220,  221,  221,  221,  221,  222,  222,  222,
		223,  223,  223,  223,  224,  224,  224,  224,
		225,  225,  225,  225,  226,  226,  226,  226,
		227,  227,  227,  227,  228,  228,  228,  228,
		229,  229,  229,  229,  230,  230,  230,  230,
		231,  231,  231,  231,  232,  232,  232,  232,
		233,  233,  233,  233,  234,  234,  234,  234,
		235,  235,  235,  235,  236,  236,  236,  236,
		237,  237,  237,  237,  238,  238,  238,  238,
		239,  239,  239,  239,  240,  240,  240,  240,
		240,  241,  241,  241,  241,  242,  242,  242,
		242,  243,  243,  243,  243,  244,  244,  244,
		244,  244,  245,  245,  245,  245,  246,  246,
		246,  246,  247,  247,  247,  247,  247,  248,
		248,  248,  248,  249,  249,  249,  249,  249,
		250,  250,  250,  250,  251,  251,  251,  251,
		251,  252,  252,  252,  252,  253,  253,  253,
		253,  253,  254,  254,  254,  254,  255,  255,
	},
};
#endif

#define ORIG_PWM_MAX 255
#define ORIG_PWM_DEF 142
#define ORIG_PWM_MIN 30

#define MAP_PWM_MAX     255
#define MAP_PWM_DEF     90
#define MAP_PWM_MIN     13

static unsigned char shrink_pwm(int val)
{
	unsigned char shrink_br;

	/* define line segments */
	if (val <= ORIG_PWM_MIN)
		shrink_br = MAP_PWM_MIN;
	else if (val > ORIG_PWM_MIN && val <= ORIG_PWM_DEF)
		shrink_br = MAP_PWM_MIN +
			(val-ORIG_PWM_MIN)*(MAP_PWM_DEF-MAP_PWM_MIN)/(ORIG_PWM_DEF-ORIG_PWM_MIN);
	else
	shrink_br = MAP_PWM_DEF +
	(val-ORIG_PWM_DEF)*(MAP_PWM_MAX-MAP_PWM_DEF)/(ORIG_PWM_MAX-ORIG_PWM_DEF);

	return shrink_br;
}

static int dsi_j_qxga_8_9_bl_notify(struct device *unused, int brightness)
{
	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	else if (brightness > 0 && brightness <= 255)
		brightness = shrink_pwm(brightness);

	return brightness;
}

static int dsi_j_qxga_8_9_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &disp_device->dev;
}

static struct tegra_dsi_cmd dsi_j_qxga_8_9_backlight_cmd[] = {
       DSI_CMD_VBLANK_SHORT(DSI_DCS_WRITE_1_PARAM, 0x51, 0xFF, CMD_NOT_CLUBBED),
};

static struct tegra_dsi_bl_platform_data dsi_j_qxga_8_9_bl_data = {
	.dsi_backlight_cmd = dsi_j_qxga_8_9_backlight_cmd,
	.n_backlight_cmd = ARRAY_SIZE(dsi_j_qxga_8_9_backlight_cmd),
	.dft_brightness	= 224,
	.notify		= dsi_j_qxga_8_9_bl_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= dsi_j_qxga_8_9_check_fb,
};

static struct platform_device __maybe_unused
		dsi_j_qxga_8_9_bl_device = {
	.name	= "tegra-dsi-backlight",
	.dev	= {
		.platform_data = &dsi_j_qxga_8_9_bl_data,
	},
};

static struct platform_device __maybe_unused
			*dsi_j_qxga_8_9_bl_devices[] __initdata = {
	&dsi_j_qxga_8_9_bl_device,
};

static int __init dsi_j_qxga_8_9_register_bl_dev(void)
{
	int err = 0;
	err = platform_add_devices(dsi_j_qxga_8_9_bl_devices,
				ARRAY_SIZE(dsi_j_qxga_8_9_bl_devices));
	if (err) {
		pr_err("disp1 bl device registration failed");
		return err;
	}
	return err;
}

static void dsi_j_qxga_8_9_set_disp_device(
	struct platform_device *dalmore_display_device)
{
	disp_device = dalmore_display_device;
}

static void dsi_j_qxga_8_9_dc_out_init(struct tegra_dc_out *dc)
{
	int i;
	struct device_node *np;

	np = of_find_node_by_name(NULL, "panel_jdi_qxga_8_9");
	if (np == NULL) {
		pr_info("can't find device node\n");
	} else {
		for (i=0; i<NUM_PANEL_GPIOS; i++) {
			panel_init_gpios[i].gpio =
				of_get_gpio_flags(np, i, NULL);
			pr_info("gpio pin = %d\n", panel_init_gpios[i].gpio);
		}
	}

	iovdd_1v8 = panel_init_gpios[IOVDD_1V8].gpio;
	avdd_4v = panel_init_gpios[AVDD_4V].gpio;
	dcdc_en = panel_init_gpios[DCDC_EN].gpio;
	lcm_rst = panel_init_gpios[LCM_RST].gpio;

	dc->dsi = &dsi_j_qxga_8_9_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_j_qxga_8_9_modes;
	dc->n_modes = ARRAY_SIZE(dsi_j_qxga_8_9_modes);
	dc->enable = dsi_j_qxga_8_9_enable;
	dc->postpoweron = dsi_j_qxga_8_9_postpoweron;
	dc->disable = dsi_j_qxga_8_9_disable;
	dc->postsuspend	= dsi_j_qxga_8_9_postsuspend,
	dc->width = 135;
	dc->height = 180;
	dc->flags = DC_CTRL_MODE;
}

static void dsi_j_qxga_8_9_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_j_qxga_8_9_modes[0].h_active;
	fb->yres = dsi_j_qxga_8_9_modes[0].v_active;
}

static void
dsi_j_qxga_8_9_sd_settings_init(struct tegra_dc_sd_settings *settings)
{
	*settings = dsi_j_qxga_8_9_sd_settings;
	settings->bl_device_name = "tegra-dsi-backlight";
}

static void dsi_j_qxga_8_9_cmu_init(struct tegra_dc_platform_data *pdata)
{
	pdata->cmu = &dsi_j_qxga_8_9_cmu;
}

struct tegra_panel __initdata dsi_j_qxga_8_9 = {
	.init_sd_settings = dsi_j_qxga_8_9_sd_settings_init,
	.init_dc_out = dsi_j_qxga_8_9_dc_out_init,
	.init_fb_data = dsi_j_qxga_8_9_fb_data_init,
	.register_bl_dev = dsi_j_qxga_8_9_register_bl_dev,
	.init_cmu_data = dsi_j_qxga_8_9_cmu_init,
	.set_disp_device = dsi_j_qxga_8_9_set_disp_device,
};
EXPORT_SYMBOL(dsi_j_qxga_8_9);

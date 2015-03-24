#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/lp855x.h>
#else
#include <string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/mt_gpio_def.h>
	#include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
#else
	#include <mach/mt_gpio_def.h>
	#include <mach/mt_pm_ldo.h>
#endif

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#endif

/* ---------------------------------------------------
 *  Local Constants
 * --------------------------------------------------- */

#define FRAME_WIDTH					(800)
#define FRAME_HEIGHT				(1280)

#define REGFLAG_DELAY				0xFE
#define REGFLAG_END_OF_TABLE		0x00   /* END OF REGISTERS MARKER */

#define LCM_DSI_CMD_MODE			0

#define GPIO_LCD_RST_EN      65
#define GPIO_LCD_STB_EN      66
#define GPIO_LCD_BL_EN       76
#define GPIO_LCD_LED_EN      110

/* ID1 DAh - Vendor and Build Designation
 * Bit[7:5] - Vendor Code (Innolux, LGD, Samsung)
 * Bit[4:2] - Reserved
 * Bit[2:0] - Build (proto, evt, dvt, etc.)
 */
#define PROTO		0x0
#define EVT1		0x1
#define EVT2		0x2
#define DVT			0x4
#define PVT			0x7
#define MP			0x6
#define BUILD_MASK	0x7

#define UNDEF		0x0		/* undefined */
#define INX			0x1		/* Innolux */
#define LGD			0x2		/* LGD */
#define SDC			0x3		/* Samsung */

/* CABC Mode Selection */
#define OFF			0x0
#define UI			0x1
#define STILL		0x2
#define MOVING		0x3

/* ---------------------------------------------------
 *  Local Variables
 * --------------------------------------------------- */

static unsigned int vendor_id = 0x1;
static unsigned int build_id = 0x1;
static int is_first = 0;

static LCM_UTIL_FUNCS lcm_util = {
	.set_reset_pin = NULL,
	.udelay = NULL,
	.mdelay = NULL,
};

#define SET_RESET_PIN(v)		(lcm_util.set_reset_pin((v)))

#define UDELAY(n)				(lcm_util.udelay(n))
#define MDELAY(n)				(lcm_util.mdelay(n))

/* ---------------------------------------------------
 *  Local Functions
 * --------------------------------------------------- */

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
		 (lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update))
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		 (lcm_util.dsi_set_cmdq(pdata, queue_size, force_update))
#define wrtie_cmd(cmd) \
		 (lcm_util.dsi_write_cmd(cmd))
#define write_regs(addr, pdata, byte_nums) \
		 (lcm_util.dsi_write_regs(addr, pdata, byte_nums))
#define read_reg \
		 (lcm_util.dsi_read_reg())
#define read_reg_v2(cmd, buffer, buffer_size) \
		 (lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size))
#define dsi_cabc_send(pdata, queue_size, force_update) \
		 (lcm_util.dsi_cabc_send(pdata, queue_size, force_update))

static void lcm_reset(void)
{
	/* GPIO65 - Reset NT35521 */
	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] GPIO65 - Reset\n");
	#else
		pr_info("[nt35521] %s, GPIO65 - Reset\n", __func__);
	#endif
	mt_pin_set_mode_gpio(GPIO_LCD_RST_EN);
	gpio_direction_output(GPIO_LCD_RST_EN, 1);
	MDELAY(10);
	gpio_set_value(GPIO_LCD_RST_EN, 0);
	MDELAY(1);
	gpio_set_value(GPIO_LCD_RST_EN, 1);
	MDELAY(20);
}

#if 0
/*
 * Created by Novatek's initial code for Proto2.0/2.1 INX panel using
 * (20131014 Initial code_for Compal_Ariel v0.04.xls)
 */
static void init_proto_lcm(void)
{
	unsigned int data_array[64];

	/* ==========Internal setting========== */
	data_array[0] = 0x00053902;
	data_array[1] = 0xA555AAFF;
	data_array[2] = 0x00000080;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x0000116F;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000020F7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x066F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xA0F71500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x196F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x12F71500;
	dsi_set_cmdq(data_array, 1, 1);

	/* MIPI AC setting */
	data_array[0] = 0x016F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x20F71500;
	dsi_set_cmdq(data_array, 1, 1);

	/* ==========Page0 relative========== */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000008;
	dsi_set_cmdq(data_array, 3, 1);

	/* LEDPWM frequency selection for CABC 13.02 KHz */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000301D9;
	dsi_set_cmdq(data_array, 2, 1);

	/* Set WXGA resolution & CTB */
	/* Enhance ESD is needed: REGW 0xB1 0x64 0x01
	 * But when the 0xB1 set to 0x64, 0x01
	 * It would gate HS mode in CMD1 for CABC
	 * Need MTK support send CABC cmd by LP mode
	 * during HS video stream, temporary set 0xB1
	 * 0x60, 0x01 */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000164B1; /* 0x000160B1; */
	dsi_set_cmdq(data_array, 2, 1);

	/* Set source output hold time */
	data_array[0] = 0x08B61500;
	dsi_set_cmdq(data_array, 1, 1);

	/* EQ control function */
	data_array[0] = 0x026F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x08B81500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Set bias current for GOP and SOP */
	data_array[0] = 0x00033902;
	data_array[1] = 0x004474BB;
	dsi_set_cmdq(data_array, 2, 1);

	/* Inversion setting: Column */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000000BC;
	dsi_set_cmdq(data_array, 2, 1);

	/* DSP Timing Settings update for BIST */
	data_array[0] = 0x00063902;
	data_array[1] = 0x0CB002BD;
	data_array[2] = 0x0000000A;
	dsi_set_cmdq(data_array, 3, 1);

	/* ==========Page1 relative========== */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000108;
	dsi_set_cmdq(data_array, 3, 1);

	/* Setting AVDD, AVEE clamp */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000505B0;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0] = 0x00033902;
	data_array[1] = 0x000505B1;
	dsi_set_cmdq(data_array, 2, 1);

	/* VGMP, VGMN, VGSP, VGSN setting 4.7V */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000188BC;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0] = 0x00033902;
	data_array[1] = 0x000188BD;
	dsi_set_cmdq(data_array, 2, 1);

	/* Gate signal control */
	data_array[0] = 0x00CA1500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Power IC control */
	data_array[0] = 0x04C01500;
	dsi_set_cmdq(data_array, 1, 1);

	/* VCOM = -1.4875V */
	data_array[0] = 0x60BE1500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Setting VGH = 16V, VGL = -12V */
	data_array[0] = 0x00033902;
	data_array[1] = 0x002D2DB3;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0] = 0x00033902;
	data_array[1] = 0x001919B4;
	dsi_set_cmdq(data_array, 2, 1);

	/* Power control for VGH, VGL */
	data_array[0] = 0x00033902;
	data_array[1] = 0x004646B9;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0] = 0x00033902;
	data_array[1] = 0x002424BA;
	dsi_set_cmdq(data_array, 2, 1);

	/* ==========Page2 relative========== */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000208;
	dsi_set_cmdq(data_array, 3, 1);

	/* Gamma control register control */
	data_array[0] = 0x01EE1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00053902;
	data_array[1] = 0x150609EF;
	data_array[2] = 0x00000018;
	dsi_set_cmdq(data_array, 3, 1);

	/* Postive Red */
	data_array[0] = 0x00073902;
	data_array[1] = 0x000000B0;
	data_array[2] = 0x00550024;
	dsi_set_cmdq(data_array, 3, 1);
	data_array[0] = 0x066F1500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Postive Red (cont'd) */
	data_array[0] = 0x00073902;
	data_array[1] = 0x007700B0;
	data_array[2] = 0x00C00094;
	dsi_set_cmdq(data_array, 3, 1);
	data_array[0] = 0x0C6F1500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Postive Red (cont'd) */
	data_array[0] = 0x00053902;
	data_array[1] = 0x01E300B0;
	data_array[2] = 0x0000001A;
	dsi_set_cmdq(data_array, 3, 1);
	data_array[0] = 0x00073902;
	data_array[1] = 0x014601B1;
	data_array[2] = 0x00BC0188;
	dsi_set_cmdq(data_array, 3, 1);
	data_array[0] = 0x066F1500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Postive Red (cont'd) */
	data_array[0] = 0x00073902;
	data_array[1] = 0x020B02B1;
	data_array[2] = 0x004D024B;
	dsi_set_cmdq(data_array, 3, 1);
	data_array[0] = 0x0C6F1500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Postive Red (cont'd) */
	data_array[0] = 0x00053902;
	data_array[1] = 0x028802B1;
	data_array[2] = 0x000000C9;
	dsi_set_cmdq(data_array, 3, 1);
	data_array[0] = 0x00073902;
	data_array[1] = 0x03F302B2;
	data_array[2] = 0x004E0329;
	dsi_set_cmdq(data_array, 3, 1);

	/* Postive Red (cont'd) */
	data_array[0] = 0x066F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00073902;
	data_array[1] = 0x037D03B2;
	data_array[2] = 0x00BE039B;
	dsi_set_cmdq(data_array, 3, 1);
	data_array[0] = 0x0C6F1500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Postive Red (cont'd) */
	data_array[0] = 0x00053902;
	data_array[1] = 0x03D303B2;
	data_array[2] = 0x000000E9;
	dsi_set_cmdq(data_array, 3, 1);

	/* Postive Red (cont'd) */
	data_array[0] = 0x00053902;
	data_array[1] = 0x03FB03B3;
	data_array[2] = 0x000000FF;
	dsi_set_cmdq(data_array, 3, 1);

	/* ==========GOA relative========== */
	/* Page6:GOUT Mapping, VGLO select */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000608;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E0BB0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00092EB1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00292AB2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00191BB3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001517B4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001113B5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E01B6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EB7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EB8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EB9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EBA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EBB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EBC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00002EBD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001210BE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001614BF;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001A18C0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002A29C1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E08C2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000A2EC3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EE5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E0AC4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00002EC5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00292AC6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001210C7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001614C8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001A18C9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E08CA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2ECB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2ECC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2ECD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2ECE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2ECF;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2ED0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00092ED1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00191BD2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001517D3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001113D4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002A29D5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E01D6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000B2ED7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EE6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000000D8;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000000D9;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00E71500;
	dsi_set_cmdq(data_array, 1, 1);

	/* ==========Page3 relative========== */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000308;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000020B0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000020B1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000005B2;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000005B6;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000005B7;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000057BA;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000057BB;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x000000C0;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x000000C1;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x60C41500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x40C51500;
	dsi_set_cmdq(data_array, 1, 1);

	/* ==========Page5 relative========== */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000508;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x030103BD;
	data_array[2] = 0x00000303;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00B81500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00B91500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00BA1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x02BB1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00BC1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x07C01500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x81C41500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xA3C51500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003005C8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003101C9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x3C0000CC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x3C0000CD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x090500D1;
	data_array[2] = 0x00001007;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x0E0500D2;
	data_array[2] = 0x00001007;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x06E51500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06E61500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06E71500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06E81500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06E91500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06EA1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x30ED1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x116F1500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Reload setting: reload setting to internal circuit */
	data_array[0] = 0x01F31500;
	dsi_set_cmdq(data_array, 1, 1);

#if 1
	/* Normal display */
	data_array[0] = 0x00351500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Set BCTRL for CABC
	 * Enable Dimming & LEDPWM Output */
	data_array[0] = 0x2C531500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Set WRDISBV - PWM duty 50/50 duty
	 * for Maximum, when CABC on */
	data_array[0] = 0xFF511500;
	dsi_set_cmdq(data_array, 1, 1);

	/* CABC Mode Selection - Off by default */
	data_array[0] = 0x00551500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
#else
	/* BIST pattern output */
	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] BIST pattern\n");
	#endif
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000008;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00FF07EF;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x027887EE;
	data_array[2] = 0x00000040;
	dsi_set_cmdq(data_array, 3, 1);
#endif
	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM][INX][Proto] Initial OK\n");
	#else
		pr_info("[nt35521][INX][Proto] %s, Initial OK\n",
				 __func__);
	#endif
}
#endif

/*
 * Created by Novatek's initial code for Pre-EVT1/EVT1 INX panel using
 * (Initial code_for Compal_6 inch Ariel_20131114.xls)
 * Change Note: Remove some of initial code which is programmed on IC as below.
 * MIPI AC Setting
 * VGMP, VGMN, VGSP, VGSN setting 4.7V
 * VCOM = -1.4875V
 * Gamma Setting
 */
static void init_inx_lcm(void)
{
	unsigned int data_array[64];

	/* ==========Internal setting========== */
	data_array[0] = 0x00053902;
	data_array[1] = 0xA555AAFF;
	data_array[2] = 0x00000080;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x0000116F;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000020F7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x066F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xA0F71500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x196F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x12F71500;
	dsi_set_cmdq(data_array, 1, 1);

	/* ==========Page0 relative========== */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000008;
	dsi_set_cmdq(data_array, 3, 1);

	/* LEDPWM frequency selection for CABC 13.02 KHz */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000301D9;
	dsi_set_cmdq(data_array, 2, 1);

	/* Set WXGA resolution & CTB */
	/* Enhance ESD is needed: REGW 0xB1 0x64 0x01
	 * But when the 0xB1 set to 0x64, 0x01
	 * It would gate HS mode in CMD1 for CABC
	 * Need MTK support send CABC cmd by LP mode
	 * during HS video stream, temporary set 0xB1
	 * 0x60, 0x01 */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000164B1; /* 0x000160B1; */
	dsi_set_cmdq(data_array, 2, 1);

	/* Set source output hold time */
	data_array[0] = 0x08B61500;
	dsi_set_cmdq(data_array, 1, 1);

	/* EQ control function */
	data_array[0] = 0x026F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x08B81500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Set bias current for GOP and SOP */
	data_array[0] = 0x00033902;
	data_array[1] = 0x004474BB;
	dsi_set_cmdq(data_array, 2, 1);

	/* Inversion setting: Column */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000000BC;
	dsi_set_cmdq(data_array, 2, 1);

	/* DSP Timing Settings update for BIST */
	/* (ARIELHW-621) Since Ariel DVT2.1,
	 * update for improve ESD performance */
	data_array[0] = 0x00063902;
	data_array[1] = 0x1EB002BD;
	data_array[2] = 0x0000001E;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000701C5;
	dsi_set_cmdq(data_array, 2, 1);

	/* Release Date: 2014/1/15
	 * Initial code_for Compal_6 inch Ariel_EVT1.xls
	 * Only one change for ARIEL-2060: Monitor the MIPI
	 * Video mode H-sync start packet
	 * is detected or not on specific time base */
	data_array[0] = 0x83C81500;
	dsi_set_cmdq(data_array, 1, 1);
	/* ==================================*/

	/* ==========Page1 relative========== */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000108;
	dsi_set_cmdq(data_array, 3, 1);

	/* Setting AVDD, AVEE clamp */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000505B0;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0] = 0x00033902;
	data_array[1] = 0x000505B1;
	dsi_set_cmdq(data_array, 2, 1);

	/* Gate signal control */
	data_array[0] = 0x00CA1500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Power IC control */
	data_array[0] = 0x04C01500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Setting VGH = 16V, VGL = -12V */
	data_array[0] = 0x00033902;
	data_array[1] = 0x002D2DB3;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0] = 0x00033902;
	data_array[1] = 0x001919B4;
	dsi_set_cmdq(data_array, 2, 1);

	/* (ARIELHW-621) Since Ariel DVT2.1,
	 * faster charge-pump clock frequency
	 * for improve ESD performance */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000505B6; /* VDDA pump colck control */
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0] = 0x00033902;
	data_array[1] = 0x000505B8; /* VCL pump clock control */
	dsi_set_cmdq(data_array, 2, 1);

	/* Power control for VGH, VGL */
	/* (ARIELHW-621) Since Ariel DVT2.1,
	 * faster charge-pump clock frequency
	 * for improve ESD performance */
	data_array[0] = 0x00033902;
	data_array[1] = 0x004646B9;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0] = 0x00033902;
	data_array[1] = 0x002525BA; /* VGL pump clock control */
	dsi_set_cmdq(data_array, 2, 1);

	/* ==========GOA relative========== */
	/* Page6:GOUT Mapping, VGLO select */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000608;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E0BB0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00092EB1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00292AB2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00191BB3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001517B4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001113B5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E01B6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EB7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EB8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EB9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EBA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EBB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EBC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00002EBD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001210BE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001614BF;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001A18C0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002A29C1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E08C2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000A2EC3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EE5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E0AC4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00002EC5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00292AC6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001210C7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001614C8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001A18C9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E08CA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2ECB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2ECC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2ECD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2ECE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2ECF;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2ED0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00092ED1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00191BD2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001517D3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001113D4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002A29D5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E01D6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000B2ED7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2EE6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000000D8;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000000D9;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00E71500;
	dsi_set_cmdq(data_array, 1, 1);

	/* ==========Page3 relative========== */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000308;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000020B0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000020B1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000005B2;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000005B6;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000005B7;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000057BA;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000057BB;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x000000C0;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x000000C1;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x60C41500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x40C51500;
	dsi_set_cmdq(data_array, 1, 1);

	/* ==========Page5 relative========== */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000508;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x030103BD;
	data_array[2] = 0x00000303;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00B81500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00B91500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00BA1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x02BB1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00BC1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x07C01500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x80C41500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xA4C51500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003005C8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003101C9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x3C0000CC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x3C0000CD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x090500D1;
	data_array[2] = 0x00001007;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x0E0500D2;
	data_array[2] = 0x00001007;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x06E51500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06E61500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06E71500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06E81500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06E91500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06EA1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x30ED1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x116F1500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Reload setting: reload setting to internal circuit */
	data_array[0] = 0x01F31500;
	dsi_set_cmdq(data_array, 1, 1);

#if 1
	/* Normal display */
	data_array[0] = 0x00351500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Set BCTRL for CABC
	 * Enable Dimming & LEDPWM Output */
	data_array[0] = 0x2C531500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Set WRDISBV - PWM duty 50/50 duty
	 * for Maximum, when CABC on */
	data_array[0] = 0xFF511500;
	dsi_set_cmdq(data_array, 1, 1);

	/* CABC Mode Selection - Off by default */
	data_array[0] = 0x00551500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);

	/* For meeting spec, sleep out -> display on */
	MDELAY(120);

	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
#else
	/* BIST pattern output */
	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] BIST pattern cmd...\n");
	#endif
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000008;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00FF07EF;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x027887EE;
	data_array[2] = 0x00000040;
	dsi_set_cmdq(data_array, 3, 1);
#endif
	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM][INX] Initial OK\n");
	#else
		pr_info("[nt35521][INX] %s, Initial OK\n",
				 __func__);
	#endif
}

/*
 * Created by LG's initial code for Pre-EVT1/EVT1 LGD panel using
 * ([LGD] Ariel_EVT1_V01_140116.txt)
 * Change Note: update for Gamma, VGL, Power consumption and GIP Control
 *
 * Updated for EVT2 LGD panel
 * ([LGD] Ariel_EVT2_V03_140325.txt)
 * Change Note: (Ariel_EVT2_Initial_Code_update_140325.pdf)
 * Power Consumption:VGH 15V->13V, VRGH 12V, VGH_REG Off
 * Deleted MTP programming for VCOM
 * Gamma Setting
 * CABC Setting
 * GIP Setting: STV1/STV2 Falling Time 0->5us, H-line
 */
static void init_lg_lcm(void)
{
	unsigned int data_array[64];

	/* ==========CMD3 Enable========== */
	data_array[0] = 0x00053902;
	data_array[1] = 0xA555AAFF;
	data_array[2] = 0x00000080;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x026F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000047F7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x176F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000060F4;
	dsi_set_cmdq(data_array, 2, 1);

	/* ==========Page0 Enable========== */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000008;
	dsi_set_cmdq(data_array, 3, 1);

	/* LEDPWM frequency selection for CABC 13.02 KHz */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000301D9;
	dsi_set_cmdq(data_array, 2, 1);

	/* Set WXGA resolution & CTB */
	/* Enhance ESD is needed: REGW 0xB1 0x64 0x01
	 * But when the 0xB1 set to 0x64, 0x01
	 * It would gate HS mode in CMD1 for CABC
	 * Need MTK support send CABC cmd by LP mode
	 * during HS video stream, temporary set 0xB1
	 * 0x60, 0x01 */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000164B1; /* 0x000160B1; */
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x0000C8B5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x00000AB6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x0C0201B8;
	data_array[2] = 0x00000002;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003333BB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000505BC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x206702BD;
	data_array[2] = 0x00000020;
	dsi_set_cmdq(data_array, 3, 1);

	/* temporary mask 0xD0 set to 0x01
	 * LGD's equipment need low active in LEDPWM
	 * so it set 0xD0, 0x01, but that's against our system
	 * need LGD update the latest code */
	/* data_array[0] = 0x01D01500;
	   dsi_set_cmdq(data_array, 1, 1); */ /* It'd deleted in V03 */

	/* ARIEL-2060: Avoid EVT1 LGD panel impdeance issue
	 * Symptom: After MTK logo become white screen gradually */
	data_array[0] = 0x83C81500;
	dsi_set_cmdq(data_array, 1, 1);

	/* ==========Page1 Enable========== */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000108;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001E1EB3; /* V03 update for power consumption */
	dsi_set_cmdq(data_array, 2, 1); /* VGH 15V -> 13V */

	data_array[0] = 0x00033902;
	data_array[1] = 0x001919B4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434B9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002424BA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000720BC; /* V03 update for Gamma */
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000720BD; /* V03 update for Gamma */
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002D2DC3; /* V03 update for power consumption */
	dsi_set_cmdq(data_array, 2, 1); /* VRGH 12V */

	data_array[0] = 0x00023902;
	data_array[1] = 0x000001CA; /* V03 update for power consumption */
	dsi_set_cmdq(data_array, 2, 1); /* VGH_REG Off */

	/* V03 update
	 * Deleted VCOM setting value in Initial code
	 * Initial code -> MTP programming for VCOM */

	/*==========Page2 Enable========== */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000208;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x01EE1500;
	dsi_set_cmdq(data_array, 1, 1);   /* update for Gamma */

	data_array[0] = 0x00113902;
	data_array[1] = 0x000000B0;
	data_array[2] = 0x0040000C;
	data_array[3] = 0x006D005B;
	data_array[4] = 0x00B80099;
	data_array[5] = 0x000000E3;
	dsi_set_cmdq(data_array, 6, 1);   /* V03 update for Gamma */

	data_array[0] = 0x00113902;
	data_array[1] = 0x010901B1;
	data_array[2] = 0x01750146;
	data_array[3] = 0x02FE01C1;
	data_array[4] = 0x02360200;
	data_array[5] = 0x0000006F;
	dsi_set_cmdq(data_array, 6, 1);   /* V03 update for Gamma */

	data_array[0] = 0x00113902;
	data_array[1] = 0x029202B2;
	data_array[2] = 0x03ED02C8;
	data_array[3] = 0x034E0329;
	data_array[4] = 0x03A6038D;
	data_array[5] = 0x000000C8;
	dsi_set_cmdq(data_array, 6, 1);   /* V03 update for Gamma */

	data_array[0] = 0x00053902;
	data_array[1] = 0x03EC03B3;
	data_array[2] = 0x000000F9;
	dsi_set_cmdq(data_array, 3, 1);   /* V03 update for Gamma */

	/* =========== Page3 Enable =========== */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000308;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000060B0;   /* update for Power consumption */
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000020B1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x170015B2;   /* V03 update for GIP */
	data_array[2] = 0x0000FA00;   /* STV1 Falling Time 0->5us */
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x170015B3;   /* V03 update for GIP */
	data_array[2] = 0x0000FA00;   /* STV2 Falling Time 0->5us */
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x191053BA;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x191053BB;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x1A1053BC;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x1A1053BD;   /* update for GIP Control */
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x340000C0;   /* update for GIP Control */
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x340000C1;   /* update for GIP Control */
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x340000C2;   /* update for GIP Control */
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x340000C3;   /* update for GIP Control */
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	/* =========== Page5 Enable =========== */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000508;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000303BD;
	data_array[2] = 0x00000303;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x07C01500;   /* V03 update for GIP */
	dsi_set_cmdq(data_array, 1, 1);   /* STV01_RISE_START_POS[4:0]=6->7(H-line) */

	data_array[0] = 0x06C11500;   /* V03 update for GIP */
	dsi_set_cmdq(data_array, 1, 1);   /* STV01_RISE_START_POS[4:0]=5->6(H-line) */

	data_array[0] = 0xA6C41500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xA6C51500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xA6C61500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xA6C71500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002005C8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002004C9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x006001CA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x006001CB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x030500D1;
	data_array[2] = 0x00001007;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x040510D2;
	data_array[2] = 0x00001003;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x430020D3;
	data_array[2] = 0x00001007;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x430030D4;
	data_array[2] = 0x00001007;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00083902;
	data_array[1] = 0x000000D0;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x000C3902;
	data_array[1] = 0x000000D5;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	dsi_set_cmdq(data_array, 4, 1);

	data_array[0] = 0x000C3902;
	data_array[1] = 0x000000D6;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	dsi_set_cmdq(data_array, 4, 1);

	data_array[0] = 0x000C3902;
	data_array[1] = 0x000000D7;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	dsi_set_cmdq(data_array, 4, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000000D8;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x020000CC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x020000CD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x020000CE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x020000CF;
	dsi_set_cmdq(data_array, 2, 1);

	/* =========== Page6 Enable =========== */

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000608;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131B0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131B1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2DB2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00342EB3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002A29B4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001916B5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001718B6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000302B7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E08B8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003334B9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003433BA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00082EBB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000001BC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001211BD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001013BE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00292ABF;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E34C0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002D2EC1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131C2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131C3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000000D8;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000000D9;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	/* CMD3 disable */
	data_array[0] = 0x00053902;
	data_array[1] = 0xA555AAFF;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	/* CMD2 page0 disable */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	/* Set BCTRL for CABC
	 * Enable Dimming & LEDPWM Output */
	data_array[0] = 0x2C531500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Set WRDISBV - PWM duty 50/50 duty
	 * for Maximum, when CABC on */
	data_array[0] = 0xFF511500;
	dsi_set_cmdq(data_array, 1, 1);

	/* CABC Mode Selection - Off by default */
	data_array[0] = 0x00551500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Sleep Out */
	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);

	/* Display On */
	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);

	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM][LGD] Initial OK\n");
	#else
		pr_info("[nt35521][LGD] %s, Initial OK\n",
				__func__);
	#endif
}

/*
 * Created for EVT1.1 Samsung panel using
 * (LTL060-test1_SHS_140128_OTP_GMA TUNE.TMD)
 */
static void init_sdc_lcm(void)
{
	unsigned int data_array[64];

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000008;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0xA555AAFF;
	data_array[2] = 0x00000080;
	dsi_set_cmdq(data_array, 3, 1);

	/* LEDPWM frequency selection for CABC 13.02 KHz */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000301D9;
	dsi_set_cmdq(data_array, 2, 1);

	/* Set WXGA resolution & CTB */
	/* Enhance ESD is needed: REGW 0xB1 0x64 0x01
	 * But when the 0xB1 set to 0x64, 0x01
	 * It would gate HS mode in CMD1 for CABC
	 * Need MTK support send CABC cmd by LP mode
	 * during HS video stream, temporary set 0xB1
	 * 0x60, 0x01 */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000164B1; /* 0x000160B1; */
	dsi_set_cmdq(data_array, 2, 1);

	/* DATA OUT CHANGE */
	data_array[0] = 0x00023902;
	data_array[1] = 0x00000BB6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x0C0201B8;
	data_array[2] = 0x00000002;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000505BC;
	dsi_set_cmdq(data_array, 2, 1);

	/* ARIEL-2060: Monitor the MIPI
	 * Video mode H-sync start packet
	 * is detected or not on specific time base */
	data_array[0] = 0x83C81500;
	dsi_set_cmdq(data_array, 1, 1);

	/*---------------------------------*/

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000108;
	dsi_set_cmdq(data_array, 3, 1);

	/* VGH 15.6V */
	data_array[0] = 0x00033902;
	data_array[1] = 0x002B2BB3;
	dsi_set_cmdq(data_array, 2, 1);

	/* VGL  -12.8V */
	data_array[0] = 0x00033902;
	data_array[1] = 0x001D1DB4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x004444B9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434BA;
	dsi_set_cmdq(data_array, 2, 1);

	/* VGMP 3.6V, VGSP 0.3V */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000130BC;
	dsi_set_cmdq(data_array, 2, 1);

	/* VGMN -3.61V, VGSN -0.3V */
	data_array[0] = 0x00033902;
	data_array[1] = 0x000131BD;
	dsi_set_cmdq(data_array, 2, 1);

	/* VCOM Setting (need to be optimized after LCD sample out) */
	data_array[0] = 0x00023902;
	data_array[1] = 0x000026BE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000030BF;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000000CA;
	dsi_set_cmdq(data_array, 2, 1);

	/*---------------------------------*/

	/* Gamma Setting (need to be optimized after LCD sample out) */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000208;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000001EE;
	dsi_set_cmdq(data_array, 2, 1);

	/* Grey Setting */
	data_array[0] = 0x00113902;
	data_array[1] = 0x000000B0;
	data_array[2] = 0x001F000C;
	data_array[3] = 0x00420031;
	data_array[4] = 0x00850064;
	data_array[5] = 0x000000B2;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0] = 0x00113902;
	data_array[1] = 0x01DC00B1;
	data_array[2] = 0x015A0122;
	data_array[3] = 0x01F901B2;
	data_array[4] = 0x023702FB;
	data_array[5] = 0x00000078;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0] = 0x00113902;
	data_array[1] = 0x029F02B2;
	data_array[2] = 0x030303D1;
	data_array[3] = 0x03620358;
	data_array[4] = 0x038C0376;
	data_array[5] = 0x000000A7;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x03C003B3;
	data_array[2] = 0x000000FF;
	dsi_set_cmdq(data_array, 3, 1);

	/*---------------------------------*/

	/* ASG timing setting */
	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000308;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000020B0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000000B1;
	dsi_set_cmdq(data_array, 2, 1);

	/* STVF */
	data_array[0] = 0x00063902;
	data_array[1] = 0x000005B2;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	/* STVB */
	data_array[0] = 0x00063902;
	data_array[1] = 0x000005B6;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	/* CK */
	data_array[0] = 0x00063902;
	data_array[1] = 0x000053BA;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	/* CKB */
	data_array[0] = 0x00063902;
	data_array[1] = 0x000053BB;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000060C4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000040C5;
	dsi_set_cmdq(data_array, 2, 1);

	/*---------------------------------*/

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000508;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000000B8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000000B9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000000BA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000000BC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x010103BD;
	data_array[2] = 0x00000300;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000007C0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x0000A0C4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002003C8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002101C9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000000D0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0xFD0400D1;
	data_array[2] = 0x00001007;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x010500D2;
	data_array[2] = 0x00001007;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000006E5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000006E6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000006E7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000006E9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000006EA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000033ED;
	dsi_set_cmdq(data_array, 2, 1);

	/*---------------------------------*/

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000608;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E08B0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00182DB1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001216B2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000010B3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434B4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003134B5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131B6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434B7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434B8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434B9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434BA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434BB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434BC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131BD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003431BE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434BF;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001101C0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001713C1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002D19C2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00092EC3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002D01C4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00112EC5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001713C6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000919C7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434C8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003134C9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131CA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434CB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434CC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434CD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434CE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434CF;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434D0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131D1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003431D2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434D3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001808D4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001216D5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E10D6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00002DD7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000000D8;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000000D9;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434E5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434E6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000000E7;
	dsi_set_cmdq(data_array, 2, 1);

	/* Set BCTRL for CABC
	 * Enable Dimming & LEDPWM Output */
	data_array[0] = 0x2C531500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Set WRDISBV - PWM duty 50/50 duty
	 * for Maximum, when CABC on */
	data_array[0] = 0xFF511500;
	dsi_set_cmdq(data_array, 1, 1);

	/* CABC Mode Selection - Off by default */
	data_array[0] = 0x00551500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Sleep Out */
	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);

	/* Display On */
	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);

	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM][SDC] Initial OK\n");
	#else
		pr_info("[nt35521][SDC] %s, Initial OK\n",
				 __func__);
	#endif
}

/* -----------------------------------------
 *  LCM Driver Implementations
 * ----------------------------------------- */
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->dsi.mode   = SYNC_EVENT_VDO_MODE;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM				= LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	/* Not support in MT6573 */
	params->dsi.packet_size = 256;

	/* Video mode setting */
	params->dsi.intermediat_buffer_num = 0;

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	/* Provided by Innolux */
	params->dsi.vertical_sync_active			= 4;
	params->dsi.vertical_backporch			= 40;
	params->dsi.vertical_frontporch			= 40;
	params->dsi.vertical_active_line			= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active		= 4;
	params->dsi.horizontal_backporch			= 32;
	params->dsi.horizontal_frontporch		= 32;
	params->dsi.horizontal_active_pixel		= FRAME_WIDTH;

	/* Bit rate calculation */
	/* params->dsi.pll_div1= 30; //32
		// fref=26MHz, fvco=fref*(div1+1)
		// (div1=0~63, fvco=500MHZ~1GHz)
	 * params->dsi.pll_div2= 1;		// div2=0~15: fout=fvo/(2*div2)
	 * params->dsi.PLL_CLOCK = LCM_DSI_6589_PLL_CLOCK_221; */
	params->dsi.PLL_CLOCK = LCM_DSI_6589_PLL_CLOCK_234;

	params->dsi.CLK_ZERO = 47;
	params->dsi.HS_ZERO = 36;

	/* The values are in mm.
	 * Add these values for reporting corect xdpi and ydpi values
	 * For Ariel xdpi and ydpi should be 252. */
	params->active_width = 81;
	params->active_height = 129;
}

static void get_lcm_id(void)
{
	unsigned char buffer[1];
	unsigned int array[16];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0xDA, buffer, 1);
	build_id = (buffer[0] & BUILD_MASK);
	vendor_id = buffer[0] >> 5;
#ifdef BUILD_LK
	dprintf(INFO, "[LK/LCM] %s, vendor id = 0x%x, board id = 0x%x\n",
			__func__, vendor_id, build_id);
#else
	pr_info("[nt35521] %s, vendor id = 0x%x, board id = 0x%x\n",
			__func__, vendor_id, build_id);
#endif
}

static char *lcm_get_build_type(void)
{
	switch (build_id) {
	case PROTO: return "PROTO\0";
	case EVT1: return "EVT1\0";
	case EVT2: return "EVT2\0";
	case DVT: return "DVT\0";
	case PVT: return "PVT\0";
	case MP: return "MP\0";
	default: return "Unknown\0";
	}
}

static char *lcm_get_vendor_type(void)
{
	switch (vendor_id) {
	case UNDEF: return "UNDEFINED\0";
	case INX: return "INX\0";
	case LGD: return "LGD\0";
	case SDC: return "SDC\0";
	default: return "Unknown\0";
	}
}

#if 0
static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0;
	unsigned char buffer[3];
	unsigned int array[16];

	SET_RESET_PIN(1); /* NOTE:should reset LCM firstly */
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(120);

	array[0] = 0x00033700; /* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0x04, buffer, 3);
	id = buffer[1]; /* we only need ID */

#ifdef BUILD_LK
	/*The Default Value should be 0x00,0x80,0x00*/
	dprintf(INFO, "[LK/LCM] %s, id0 = 0x%08x, id1 = 0x%08x, id2 = 0x%08x\n",
			 __func__, buffer[0], buffer[1], buffer[2]);
#else
	pr_info("[nt35521] %s, id0 = 0x%08x, id1 = 0x%08x, id2 = 0x%08x\n",
			 __func__, buffer[0], buffer[1], buffer[2]);
#endif
	return (id == 0x80) ? 1 : 0;
}
#endif

static void lcm_set_backlight(struct led_classdev *bldev, unsigned int level)
{
	pr_info("[nt35521] %s\n", __func__);
	lp855x_led_set_brightness(bldev, level);
	/* T14: VLED falling slowly, thus wait more time to send lcm sleep cmd */
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
		array[0] = 0x00551500;
		break;
	case UI:
		pr_debug("%s: Switching ON CABC UI mode\n", __func__);
		array[0] = 0x01551500;
		break;
	case STILL:
		pr_debug("%s: Switching ON CABC STILL mode\n", __func__);
		array[0] = 0x02551500;
		break;
	case MOVING:
		pr_debug("%s: Swithcing ON CABC MOVING mode\n", __func__);
		array[0] = 0x03551500;
		break;
	default:
		pr_info("[nt35521] CABC mode not supported\n");
		array[0] = 0x00551500;
		break;
	}
	/* dsi_set_cmdq(array, 1, 1); */
	dsi_cabc_send(array, 1, 1);
	#endif
}

static void lcm_res_init(void)
{
	gpio_request(GPIO_LCD_RST_EN, "LCM-RST");
	gpio_request(GPIO_LCD_LED_EN, "LCM-LED");
}

static void lcm_res_exit(void)
{
	gpio_free(GPIO_LCD_RST_EN);
	gpio_free(GPIO_LCD_LED_EN);
}

static void lcm_init(void)
{
#ifdef BUILD_LK
	power_on();
#endif

	get_lcm_id();

#ifdef BUILD_LK

	dprintf(CRITICAL,
			 "[LK/LCM] %s enter, build type: %s, vendor type: %s\n",
			 __func__,
			 lcm_get_build_type(),
			 lcm_get_vendor_type());

	switch (vendor_id) {
	/* proto2.0/proto2.1 */
	case UNDEF:
		/* workaround since vid isn't ready */
		init_sdc_lcm();
		/* init_proto_lcm(); */
		break;
	/* since pre-evt1 */
	case INX:
		init_inx_lcm();
		break;
	/* since evt1 */
	case LGD:
		init_lg_lcm();
		break;
	/* since evt1.1 */
	case SDC:
		init_sdc_lcm();
		break;
	default:
		dprintf(INFO, "[LK/LCM] the vendor not supported\n");
		break;
	}

#else
	pr_info("[nt35521] %s enter, skip power_on & init lcm since it's done by lk\n",
			 __func__);
	pr_info("[nt35521] build type: %s, vendor type: %s\n",
			 lcm_get_build_type(),
			 lcm_get_vendor_type());
#endif
}

static void lcm_suspend(void)
{
	unsigned int data_array[16];
	#ifdef CONFIG_AMAZON_METRICS_LOG
		char buf[128];
		snprintf(buf, sizeof(buf), "%s:lcd:suspend=1;CT;1:NR", __func__);
		log_to_metrics(ANDROID_LOG_INFO, "LCDEvent", buf);
	#endif

	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] %s\n", __func__);
	#else
		pr_info("[nt35521] %s\n", __func__);
	#endif

	data_array[0] = 0x00280500; /* Display Off */
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(20);

	data_array[0] = 0x00100500; /* Sleep In */
	dsi_set_cmdq(data_array, 1, 1);

	MDELAY(120);

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1); /* T13: remove redundance delay*/
	SET_RESET_PIN(1);
}

static void lcm_resume(void)
{
	#ifdef CONFIG_AMAZON_METRICS_LOG
		char buf[128];
		snprintf(buf, sizeof(buf), "%s:lcd:resume=1;CT;1:NR", __func__);
		log_to_metrics(ANDROID_LOG_INFO, "LCDEvent", buf);
	#endif

	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] %s\n", __func__);
	#else
		pr_info("[nt35521] %s\n", __func__);
	#endif

	lcm_reset();

	/* FIXME:workaround for read wrong vendor id at lcm_init */
	if (is_first == 0) {
		get_lcm_id();
		is_first = 1;
	}

	switch (vendor_id) {
	/* proto2.0/proto2.1 */
	case UNDEF:
		/* workaround since vid isn't ready */
		init_sdc_lcm();
		/* init_proto_lcm(); */
		break;
	/* since pre-evt1 */
	case INX:
		init_inx_lcm();
		break;
	/* since evt1 */
	case LGD:
		init_lg_lcm();
		break;
	/* since evt1.1 */
	case SDC:
		init_sdc_lcm();
		break;
	default:
	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] the vendor not supported yet\n");
	#else
		pr_info("[nt35521] the vendor not supported yet\n");
	#endif
		break;
	}
}

/* Seperate lcm_resume_power and lcm_reset from power_on func,
 * to meet turn on MIPI lanes after AVEE ready,
 * before Reset pin Low->High */
static void lcm_resume_power(void)
{
	/* GHGL_EN/GPIO110 - Control AVDD/AVEE/VDD,
	 * their sequence rests entirely on NT50357 */
	#ifdef BUILD_LK
		dprintf(ALWAYS, "[LK/LCM] GPIO110 - Control AVDD/AVEE/VDD\n");
	#else
		pr_info("[nt35521] %s, GPIO110 - Control AVDD/AVEE/VDD\n",
				 __func__);
	#endif
	mt_pin_set_mode_gpio(GPIO_LCD_LED_EN);
	gpio_direction_output(GPIO_LCD_LED_EN, 1);

}

/* Seperate lcm_suspend_power from lcm_suspend
 * to meet turn off MIPI lanes after after sleep in cmd,
 * then Reset High->Low, GHGL_EN High->Low
 */
static void lcm_suspend_power(void)
{
	mt_pin_set_mode_gpio(GPIO_LCD_RST_EN);
	gpio_direction_output(GPIO_LCD_RST_EN, 0);

	MDELAY(10);

	mt_pin_set_mode_gpio(GPIO_LCD_LED_EN);
	gpio_direction_output(GPIO_LCD_LED_EN, 0);

	MDELAY(10);
}

LCM_DRIVER nt35521_wxga_dsi_vdo_hh060ia_lcm_drv = {
	.name			= "nt35521_wxga_dsi_vdo_hh060ia",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.res_init       = lcm_res_init,
	.res_exit       = lcm_res_exit,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.set_backlight	= lcm_set_backlight,
	.set_backlight_mode	= lcm_set_backlight_mode,
	/* .compare_id    = lcm_compare_id, */
	.resume_power	= lcm_resume_power,
	.suspend_power	= lcm_suspend_power,
};

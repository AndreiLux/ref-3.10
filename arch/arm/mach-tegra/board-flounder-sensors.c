/*
 * arch/arm/mach-tegra/board-flounder-sensors.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#include <asm/atomic.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/mpu.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/nct1008.h>
#include <linux/of_platform.h>
#include <linux/pid_thermal_gov.h>
#include <linux/tegra-fuse.h>
#include <mach/edp.h>
#include <mach/pinmux-t12.h>
#include <mach/pinmux.h>
#include <mach/io_dpd.h>
#include <media/camera.h>
#include <media/ar0261.h>
#include <media/imx135.h>
#include <media/dw9718.h>
#include <media/as364x.h>
#include <media/ov5693.h>
#include <media/ov7695.h>
#include <media/mt9m114.h>
#include <media/ad5823.h>
#include <media/max77387.h>
#include <media/imx219.h>
#include <media/ov9760.h>
#include <media/drv201.h>
#include <media/tps61310.h>

#include <linux/platform_device.h>
#include <linux/input/cy8c_sar.h>
#include <media/soc_camera.h>
#include <media/soc_camera_platform.h>
#include <media/tegra_v4l2_camera.h>
#include <linux/generic_adc_thermal.h>
#include <mach/board_htc.h>

#include "cpu-tegra.h"
#include "devices.h"
#include "board.h"
#include "board-common.h"
#include "board-flounder.h"
#include "tegra-board-id.h"
#ifdef CONFIG_THERMAL_GOV_ADAPTIVE_SKIN
#include <linux/adaptive_skin.h>
#endif

int cy8c_sar1_reset(void)
{
	pr_debug("[SAR] %s Enter\n", __func__);
	gpio_set_value_cansleep(TEGRA_GPIO_PG6, 1);
	msleep(5);
	gpio_set_value_cansleep(TEGRA_GPIO_PG6, 0);
	msleep(50);/*wait chip reset finish time*/
	return 0;
}

int cy8c_sar_reset(void)
{
	pr_debug("[SAR] %s Enter\n", __func__);
	gpio_set_value_cansleep(TEGRA_GPIO_PG7, 1);
	msleep(5);
	gpio_set_value_cansleep(TEGRA_GPIO_PG7, 0);
	msleep(50);/*wait chip reset finish time*/
	return 0;
}

int cy8c_sar_powerdown(int activate)
{
	int gpio = TEGRA_GPIO_PO5;
	int ret = 0;

	if (!is_mdm_modem()) {
		pr_debug("[SAR]%s:!is_mdm_modem()\n", __func__);
		return ret;
	}

	if (activate) {
		pr_debug("[SAR]:%s:gpio high,activate=%d\n",
			__func__, activate);
		ret = gpio_direction_output(gpio, 1);
		if (ret < 0)
			pr_debug("[SAR]%s: calling gpio_free(sar_modem)\n",
				__func__);
	} else {
		pr_debug("[SAR]:%s:gpio low,activate=%d\n", __func__, activate);
		ret = gpio_direction_output(gpio, 0);
		if (ret < 0)
			pr_debug("[SAR]%s: calling gpio_free(sar_modem)\n",
				__func__);
	}
	return ret;
}

static struct i2c_board_info flounder_i2c_board_info_cm32181[] = {
	{
		I2C_BOARD_INFO("cm32181", 0x48),
	},
};

struct cy8c_i2c_sar_platform_data sar1_cy8c_data[] = {
	{
		.gpio_irq = TEGRA_GPIO_PCC5,
		.reset    = cy8c_sar1_reset,
		.position_id = 1,
		.bl_addr = 0x61,
		.ap_addr = 0x5d,
		.powerdown    = cy8c_sar_powerdown,
	},
};

struct cy8c_i2c_sar_platform_data sar_cy8c_data[] = {
	{
		.gpio_irq = TEGRA_GPIO_PC7,
		.reset    = cy8c_sar_reset,
		.position_id = 0,
		.bl_addr = 0x60,
		.ap_addr = 0x5c,
		.powerdown    = cy8c_sar_powerdown,
	},
};

struct i2c_board_info flounder_i2c_board_info_cypress_sar[] = {
	{
		I2C_BOARD_INFO("CYPRESS_SAR", 0xB8 >> 1),
		.platform_data = &sar_cy8c_data,
		.irq = -1,
	},
};

struct i2c_board_info flounder_i2c_board_info_cypress_sar1[] = {
	{
		I2C_BOARD_INFO("CYPRESS_SAR1", 0xBA >> 1),
		.platform_data = &sar1_cy8c_data,
		.irq = -1,
	},
};

/*
 * Soc Camera platform driver for testing
 */
#if IS_ENABLED(CONFIG_SOC_CAMERA_PLATFORM)
static int flounder_soc_camera_add(struct soc_camera_device *icd);
static void flounder_soc_camera_del(struct soc_camera_device *icd);

static int flounder_soc_camera_set_capture(struct soc_camera_platform_info *info,
		int enable)
{
	/* TODO: probably add clk opertaion here */
	return 0; /* camera sensor always enabled */
}

static struct soc_camera_platform_info flounder_soc_camera_info = {
	.format_name = "RGB4",
	.format_depth = 32,
	.format = {
		.code = V4L2_MBUS_FMT_RGBA8888_4X8_LE,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.field = V4L2_FIELD_NONE,
		.width = 1280,
		.height = 720,
	},
	.set_capture = flounder_soc_camera_set_capture,
};

static struct tegra_camera_platform_data flounder_camera_platform_data = {
	.flip_v                 = 0,
	.flip_h                 = 0,
	.port                   = TEGRA_CAMERA_PORT_CSI_A,
	.lanes                  = 4,
	.continuous_clk         = 0,
};

static struct soc_camera_link flounder_soc_camera_link = {
	.bus_id         = 0, /* This must match the .id of tegra_vi01_device */
	.add_device     = flounder_soc_camera_add,
	.del_device     = flounder_soc_camera_del,
	.module_name    = "soc_camera_platform",
	.priv		= &flounder_camera_platform_data,
	.dev_priv	= &flounder_soc_camera_info,
};

static struct platform_device *flounder_pdev;

static void flounder_soc_camera_release(struct device *dev)
{
	soc_camera_platform_release(&flounder_pdev);
}

static int flounder_soc_camera_add(struct soc_camera_device *icd)
{
	return soc_camera_platform_add(icd, &flounder_pdev,
			&flounder_soc_camera_link,
			flounder_soc_camera_release, 0);
}

static void flounder_soc_camera_del(struct soc_camera_device *icd)
{
	soc_camera_platform_del(icd, flounder_pdev, &flounder_soc_camera_link);
}

static struct platform_device flounder_soc_camera_device = {
	.name   = "soc-camera-pdrv",
	.id     = 0,
	.dev    = {
		.platform_data = &flounder_soc_camera_link,
	},
};
#endif

static struct tegra_io_dpd csia_io = {
	.name			= "CSIA",
	.io_dpd_reg_index	= 0,
	.io_dpd_bit		= 0,
};

static struct tegra_io_dpd csib_io = {
	.name			= "CSIB",
	.io_dpd_reg_index	= 0,
	.io_dpd_bit		= 1,
};

static struct tegra_io_dpd csie_io = {
	.name			= "CSIE",
	.io_dpd_reg_index	= 1,
	.io_dpd_bit		= 12,
};

static atomic_t shared_gpios_refcnt = ATOMIC_INIT(0);

static void flounder_enable_shared_gpios(void)
{
	if (1 == atomic_add_return(1, &shared_gpios_refcnt)) {
		gpio_set_value(CAM_VCM2V85_EN, 1);
		usleep_range(100, 120);
		gpio_set_value(CAM_1V2_EN, 1);
		gpio_set_value(CAM_A2V85_EN, 1);
		gpio_set_value(CAM_1V8_EN, 1);
		pr_debug("%s\n", __func__);
	}
}

static void flounder_disable_shared_gpios(void)
{
	if (atomic_dec_and_test(&shared_gpios_refcnt)) {
		gpio_set_value(CAM_1V8_EN, 0);
		gpio_set_value(CAM_A2V85_EN, 0);
		gpio_set_value(CAM_1V2_EN, 0);
		gpio_set_value(CAM_VCM2V85_EN, 0);
		pr_debug("%s\n", __func__);
	}
}

static int flounder_imx219_power_on(struct imx219_power_rail *pw)
{
	/* disable CSIA/B IOs DPD mode to turn on camera for flounder */
	tegra_io_dpd_disable(&csia_io);
	tegra_io_dpd_disable(&csib_io);

	gpio_set_value(CAM_PWDN, 0);

	flounder_enable_shared_gpios();

	usleep_range(1, 2);
	gpio_set_value(CAM_PWDN, 1);

	usleep_range(300, 310);
	pr_debug("%s\n", __func__);
	return 1;
}

static int flounder_imx219_power_off(struct imx219_power_rail *pw)
{
	gpio_set_value(CAM_PWDN, 0);
	usleep_range(100, 120);
	pr_debug("%s\n", __func__);

	flounder_disable_shared_gpios();

	/* put CSIA/B IOs into DPD mode to save additional power for flounder */
	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);
	return 0;
}

static struct imx219_platform_data flounder_imx219_pdata = {
	.power_on = flounder_imx219_power_on,
	.power_off = flounder_imx219_power_off,
};

static int flounder_ov9760_power_on(struct ov9760_power_rail *pw)
{
	/* disable CSIE IO DPD mode to turn on camera for flounder */
	tegra_io_dpd_disable(&csie_io);

	gpio_set_value(CAM2_RST, 0);

	flounder_enable_shared_gpios();

	usleep_range(100, 120);
	gpio_set_value(CAM2_RST, 1);
	pr_debug("%s\n", __func__);

	return 1;
}

static int flounder_ov9760_power_off(struct ov9760_power_rail *pw)
{
	gpio_set_value(CAM2_RST, 0);
	usleep_range(100, 120);
	pr_debug("%s\n", __func__);

	flounder_disable_shared_gpios();

	/* put CSIE IOs into DPD mode to save additional power for flounder */
	tegra_io_dpd_enable(&csie_io);

	return 0;
}

static struct ov9760_platform_data flounder_ov9760_pdata = {
	.power_on = flounder_ov9760_power_on,
	.power_off = flounder_ov9760_power_off,
	.mclk_name = "mclk2",
};

static int flounder_drv201_power_on(struct drv201_power_rail *pw)
{
	gpio_set_value(CAM_VCM_PWDN, 0);

	flounder_enable_shared_gpios();

	gpio_set_value(CAM_VCM_PWDN, 1);
	usleep_range(100, 120);
	pr_debug("%s\n", __func__);

	return 1;
}

static int flounder_drv201_power_off(struct drv201_power_rail *pw)
{
	gpio_set_value(CAM_VCM_PWDN, 0);
	usleep_range(100, 120);
	pr_debug("%s\n", __func__);

	flounder_disable_shared_gpios();

	return 1;
}

static struct nvc_focus_cap flounder_drv201_cap = {
	.version = NVC_FOCUS_CAP_VER2,
	.settle_time = 15,
	.focus_macro = 810,
	.focus_infinity = 50,
	.focus_hyper = 50,
};

static struct drv201_platform_data flounder_drv201_pdata = {
	.cfg = 0,
	.num = 0,
	.sync = 0,
	.dev_name = "focuser",
	.cap = &flounder_drv201_cap,
	.power_on	= flounder_drv201_power_on,
	.power_off	= flounder_drv201_power_off,
};

static struct nvc_torch_pin_state flounder_tps61310_pinstate = {
	.mask		= 1 << (CAM_FLASH_STROBE - TEGRA_GPIO_PBB0), /* VGP4 */
	.values		= 1 << (CAM_FLASH_STROBE - TEGRA_GPIO_PBB0),
};

static struct tps61310_platform_data flounder_tps61310_pdata = {
	.dev_name	= "torch",
	.pinstate	= &flounder_tps61310_pinstate,
};

static struct camera_data_blob flounder_camera_lut[] = {
	{"flounder_imx219_pdata", &flounder_imx219_pdata},
	{"flounder_drv201_pdata", &flounder_drv201_pdata},
	{"flounder_tps61310_pdata", &flounder_tps61310_pdata},
	{"flounder_ov9760_pdata", &flounder_ov9760_pdata},
	{},
};

void __init flounder_camera_auxdata(void *data)
{
	struct of_dev_auxdata *aux_lut = data;
	while (aux_lut && aux_lut->compatible) {
		if (!strcmp(aux_lut->compatible, "nvidia,tegra124-camera")) {
			pr_info("%s: update camera lookup table.\n", __func__);
			aux_lut->platform_data = flounder_camera_lut;
		}
		aux_lut++;
	}
}

static int flounder_camera_init(void)
{
	pr_debug("%s: ++\n", __func__);

	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);
	tegra_io_dpd_enable(&csie_io);
	tegra_gpio_disable(TEGRA_GPIO_PBB0);
	tegra_gpio_disable(TEGRA_GPIO_PCC0);

#if IS_ENABLED(CONFIG_SOC_CAMERA_PLATFORM)
	platform_device_register(&flounder_soc_camera_device);
#endif
	return 0;
}

static struct pid_thermal_gov_params cpu_pid_params = {
	.max_err_temp = 4000,
	.max_err_gain = 1000,

	.gain_p = 1000,
	.gain_d = 0,

	.up_compensation = 15,
	.down_compensation = 15,
};

static struct thermal_zone_params cpu_tzp = {
	.governor_name = "pid_thermal_gov",
	.governor_params = &cpu_pid_params,
};

static struct thermal_zone_params therm_est_activ_tzp = {
	.governor_name = "step_wise"
};

static struct throttle_table cpu_throttle_table[] = {
	/* CPU_THROT_LOW cannot be used by other than CPU */
	/*      CPU,    GPU,  C2BUS,  C3BUS,   SCLK,    EMC   */
	{ { 2295000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 2269500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 2244000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 2218500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 2193000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 2167500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 2142000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 2116500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 2091000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 2065500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 2040000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 2014500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1989000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1963500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1938000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1912500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1887000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1861500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1836000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1810500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1785000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1759500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1734000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1708500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1683000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1657500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1632000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1606500, 790000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1581000, 776000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1555500, 762000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1530000, 749000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1504500, 735000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1479000, 721000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1453500, 707000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1428000, 693000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1402500, 679000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1377000, 666000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1351500, 652000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1326000, 638000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1300500, 624000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1275000, 610000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1249500, 596000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1224000, 582000, NO_CAP, NO_CAP, NO_CAP, 792000 } },
	{ { 1198500, 569000, NO_CAP, NO_CAP, NO_CAP, 792000 } },
	{ { 1173000, 555000, NO_CAP, NO_CAP, 360000, 792000 } },
	{ { 1147500, 541000, NO_CAP, NO_CAP, 360000, 792000 } },
	{ { 1122000, 527000, NO_CAP, 684000, 360000, 792000 } },
	{ { 1096500, 513000, 444000, 684000, 360000, 792000 } },
	{ { 1071000, 499000, 444000, 684000, 360000, 792000 } },
	{ { 1045500, 486000, 444000, 684000, 360000, 792000 } },
	{ { 1020000, 472000, 444000, 684000, 324000, 792000 } },
	{ {  994500, 458000, 444000, 684000, 324000, 792000 } },
	{ {  969000, 444000, 444000, 600000, 324000, 792000 } },
	{ {  943500, 430000, 444000, 600000, 324000, 792000 } },
	{ {  918000, 416000, 396000, 600000, 324000, 792000 } },
	{ {  892500, 402000, 396000, 600000, 324000, 792000 } },
	{ {  867000, 389000, 396000, 600000, 324000, 792000 } },
	{ {  841500, 375000, 396000, 600000, 288000, 792000 } },
	{ {  816000, 361000, 396000, 600000, 288000, 792000 } },
	{ {  790500, 347000, 396000, 600000, 288000, 792000 } },
	{ {  765000, 333000, 396000, 504000, 288000, 792000 } },
	{ {  739500, 319000, 348000, 504000, 288000, 792000 } },
	{ {  714000, 306000, 348000, 504000, 288000, 624000 } },
	{ {  688500, 292000, 348000, 504000, 288000, 624000 } },
	{ {  663000, 278000, 348000, 504000, 288000, 624000 } },
	{ {  637500, 264000, 348000, 504000, 288000, 624000 } },
	{ {  612000, 250000, 348000, 504000, 252000, 624000 } },
	{ {  586500, 236000, 348000, 504000, 252000, 624000 } },
	{ {  561000, 222000, 348000, 420000, 252000, 624000 } },
	{ {  535500, 209000, 288000, 420000, 252000, 624000 } },
	{ {  510000, 195000, 288000, 420000, 252000, 624000 } },
	{ {  484500, 181000, 288000, 420000, 252000, 624000 } },
	{ {  459000, 167000, 288000, 420000, 252000, 624000 } },
	{ {  433500, 153000, 288000, 420000, 252000, 396000 } },
	{ {  408000, 139000, 288000, 420000, 252000, 396000 } },
	{ {  382500, 126000, 288000, 420000, 252000, 396000 } },
	{ {  357000, 112000, 288000, 420000, 252000, 396000 } },
	{ {  331500,  98000, 288000, 420000, 252000, 396000 } },
	{ {  306000,  84000, 288000, 420000, 252000, 396000 } },
	{ {  280500,  84000, 288000, 420000, 252000, 396000 } },
	{ {  255000,  84000, 288000, 420000, 252000, 396000 } },
	{ {  229500,  84000, 288000, 420000, 252000, 396000 } },
	{ {  204000,  84000, 288000, 420000, 252000, 396000 } },
};

static struct balanced_throttle cpu_throttle = {
	.throt_tab_size = ARRAY_SIZE(cpu_throttle_table),
	.throt_tab = cpu_throttle_table,
};

static struct throttle_table gpu_throttle_table[] = {
	/* CPU_THROT_LOW cannot be used by other than CPU */
	/*      CPU,    GPU,  C2BUS,  C3BUS,   SCLK,    EMC   */
	{ { 2295000, 782800, 480000, 756000, 384000, 924000 } },
	{ { 2269500, 772200, 480000, 756000, 384000, 924000 } },
	{ { 2244000, 761600, 480000, 756000, 384000, 924000 } },
	{ { 2218500, 751100, 480000, 756000, 384000, 924000 } },
	{ { 2193000, 740500, 480000, 756000, 384000, 924000 } },
	{ { 2167500, 729900, 480000, 756000, 384000, 924000 } },
	{ { 2142000, 719300, 480000, 756000, 384000, 924000 } },
	{ { 2116500, 708700, 480000, 756000, 384000, 924000 } },
	{ { 2091000, 698100, 480000, 756000, 384000, 924000 } },
	{ { 2065500, 687500, 480000, 756000, 384000, 924000 } },
	{ { 2040000, 676900, 480000, 756000, 384000, 924000 } },
	{ { 2014500, 666000, 480000, 756000, 384000, 924000 } },
	{ { 1989000, 656000, 480000, 756000, 384000, 924000 } },
	{ { 1963500, 645000, 480000, 756000, 384000, 924000 } },
	{ { 1938000, 635000, 480000, 756000, 384000, 924000 } },
	{ { 1912500, 624000, 480000, 756000, 384000, 924000 } },
	{ { 1887000, 613000, 480000, 756000, 384000, 924000 } },
	{ { 1861500, 603000, 480000, 756000, 384000, 924000 } },
	{ { 1836000, 592000, 480000, 756000, 384000, 924000 } },
	{ { 1810500, 582000, 480000, 756000, 384000, 924000 } },
	{ { 1785000, 571000, 480000, 756000, 384000, 924000 } },
	{ { 1759500, 560000, 480000, 756000, 384000, 924000 } },
	{ { 1734000, 550000, 480000, 756000, 384000, 924000 } },
	{ { 1708500, 539000, 480000, 756000, 384000, 924000 } },
	{ { 1683000, 529000, 480000, 756000, 384000, 924000 } },
	{ { 1657500, 518000, 480000, 756000, 384000, 924000 } },
	{ { 1632000, 508000, 480000, 756000, 384000, 924000 } },
	{ { 1606500, 497000, 480000, 756000, 384000, 924000 } },
	{ { 1581000, 486000, 480000, 756000, 384000, 924000 } },
	{ { 1555500, 476000, 480000, 756000, 384000, 924000 } },
	{ { 1530000, 465000, 480000, 756000, 384000, 924000 } },
	{ { 1504500, 455000, 480000, 756000, 384000, 924000 } },
	{ { 1479000, 444000, 480000, 756000, 384000, 924000 } },
	{ { 1453500, 433000, 480000, 756000, 384000, 924000 } },
	{ { 1428000, 423000, 480000, 756000, 384000, 924000 } },
	{ { 1402500, 412000, 480000, 756000, 384000, 924000 } },
	{ { 1377000, 402000, 480000, 756000, 384000, 924000 } },
	{ { 1351500, 391000, 480000, 756000, 384000, 924000 } },
	{ { 1326000, 380000, 480000, 756000, 384000, 924000 } },
	{ { 1300500, 370000, 480000, 756000, 384000, 924000 } },
	{ { 1275000, 359000, 480000, 756000, 384000, 924000 } },
	{ { 1249500, 349000, 480000, 756000, 384000, 924000 } },
	{ { 1224000, 338000, 480000, 756000, 384000, 792000 } },
	{ { 1198500, 328000, 480000, 756000, 384000, 792000 } },
	{ { 1173000, 317000, 480000, 756000, 360000, 792000 } },
	{ { 1147500, 306000, 480000, 756000, 360000, 792000 } },
	{ { 1122000, 296000, 480000, 684000, 360000, 792000 } },
	{ { 1096500, 285000, 444000, 684000, 360000, 792000 } },
	{ { 1071000, 275000, 444000, 684000, 360000, 792000 } },
	{ { 1045500, 264000, 444000, 684000, 360000, 792000 } },
	{ { 1020000, 253000, 444000, 684000, 324000, 792000 } },
	{ {  994500, 243000, 444000, 684000, 324000, 792000 } },
	{ {  969000, 232000, 444000, 600000, 324000, 792000 } },
	{ {  943500, 222000, 444000, 600000, 324000, 792000 } },
	{ {  918000, 211000, 396000, 600000, 324000, 792000 } },
	{ {  892500, 200000, 396000, 600000, 324000, 792000 } },
	{ {  867000, 190000, 396000, 600000, 324000, 792000 } },
	{ {  841500, 179000, 396000, 600000, 288000, 792000 } },
	{ {  816000, 169000, 396000, 600000, 288000, 792000 } },
	{ {  790500, 158000, 396000, 600000, 288000, 792000 } },
	{ {  765000, 148000, 396000, 504000, 288000, 792000 } },
	{ {  739500, 137000, 348000, 504000, 288000, 792000 } },
	{ {  714000, 126000, 348000, 504000, 288000, 624000 } },
	{ {  688500, 116000, 348000, 504000, 288000, 624000 } },
	{ {  663000, 105000, 348000, 504000, 288000, 624000 } },
	{ {  637500,  95000, 348000, 504000, 288000, 624000 } },
	{ {  612000,  84000, 348000, 504000, 252000, 624000 } },
	{ {  586500,  84000, 348000, 504000, 252000, 624000 } },
	{ {  561000,  84000, 348000, 420000, 252000, 624000 } },
	{ {  535500,  84000, 288000, 420000, 252000, 624000 } },
	{ {  510000,  84000, 288000, 420000, 252000, 624000 } },
	{ {  484500,  84000, 288000, 420000, 252000, 624000 } },
	{ {  459000,  84000, 288000, 420000, 252000, 624000 } },
	{ {  433500,  84000, 288000, 420000, 252000, 396000 } },
	{ {  408000,  84000, 288000, 420000, 252000, 396000 } },
	{ {  382500,  84000, 288000, 420000, 252000, 396000 } },
	{ {  357000,  84000, 288000, 420000, 252000, 396000 } },
	{ {  331500,  84000, 288000, 420000, 252000, 396000 } },
	{ {  306000,  84000, 288000, 420000, 252000, 396000 } },
	{ {  280500,  84000, 288000, 420000, 252000, 396000 } },
	{ {  255000,  84000, 288000, 420000, 252000, 396000 } },
	{ {  229500,  84000, 288000, 420000, 252000, 396000 } },
	{ {  204000,  84000, 288000, 420000, 252000, 396000 } },
};

static struct balanced_throttle gpu_throttle = {
	.throt_tab_size = ARRAY_SIZE(gpu_throttle_table),
	.throt_tab = gpu_throttle_table,
};

static int __init flounder_tj_throttle_init(void)
{
	if (of_machine_is_compatible("google,flounder") ||
	    of_machine_is_compatible("google,flounder_lte") ||
	    of_machine_is_compatible("google,flounder64") ||
	    of_machine_is_compatible("google,flounder64_lte")) {
		balanced_throttle_register(&cpu_throttle, "cpu-balanced");
		balanced_throttle_register(&gpu_throttle, "gpu-balanced");
	}

	return 0;
}
late_initcall(flounder_tj_throttle_init);

#ifdef CONFIG_TEGRA_SKIN_THROTTLE
static struct thermal_trip_info skin_trips[] = {
	{
		.cdev_type = "skin-balanced",
		.trip_temp = 45000,
		.trip_type = THERMAL_TRIP_PASSIVE,
		.upper = THERMAL_NO_LIMIT,
		.lower = THERMAL_NO_LIMIT,
		.hysteresis = 0,
	}
};

static struct therm_est_subdevice skin_devs_wifi[] = {
	{
		.dev_data = "Tdiode_tegra",
		.coeffs = {
			3, 0, 0, 0,
			-1, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, -1
		},
	},
	{
		.dev_data = "Tboard_tegra",
		.coeffs = {
			7, 6, 5, 3,
			3, 4, 4, 4,
			4, 4, 4, 4,
			3, 4, 3, 3,
			4, 6, 9, 15
		},
	},
};

static struct therm_est_subdevice skin_devs_lte[] = {
	{
		.dev_data = "Tdiode_tegra",
		.coeffs = {
			2, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, -1, -2
		},
	},
	{
		.dev_data = "Tboard_tegra",
		.coeffs = {
			7, 5, 4, 3,
			3, 3, 4, 4,
			3, 3, 3, 3,
			4, 4, 3, 3,
			3, 5, 10, 16
		},
	},
};

#ifdef CONFIG_THERMAL_GOV_ADAPTIVE_SKIN
static struct adaptive_skin_thermal_gov_params skin_astg_params = {
	.tj_tran_threshold = 2000,
	.tj_std_threshold = 3000,
	.tj_std_fup_threshold = 5000,

	.tskin_tran_threshold = 500,
	.tskin_std_threshold = 1000,

	.target_state_tdp = 12,
};

static struct thermal_zone_params skin_astg_tzp = {
	.governor_name = "adaptive_skin",
	.governor_params = &skin_astg_params,
};
#endif

static struct pid_thermal_gov_params skin_pid_params = {
	.max_err_temp = 4000,
	.max_err_gain = 1000,

	.gain_p = 1000,
	.gain_d = 0,

	.up_compensation = 15,
	.down_compensation = 15,
};

static struct thermal_zone_params skin_tzp = {
	.governor_name = "pid_thermal_gov",
	.governor_params = &skin_pid_params,
};

static struct thermal_zone_params skin_step_wise_tzp = {
	.governor_name = "step_wise",
};

static struct therm_est_data skin_data = {
	.num_trips = ARRAY_SIZE(skin_trips),
	.trips = skin_trips,
	.polling_period = 1100,
	.passive_delay = 15000,
	.tc1 = 10,
	.tc2 = 1,
	.tzp = &skin_step_wise_tzp,
};

static struct throttle_table skin_throttle_table[] = {
	/* CPU_THROT_LOW cannot be used by other than CPU */
	/*      CPU,    GPU,  C2BUS,  C3BUS,   SCLK,    EMC   */
       { { 2000000, 804000, 480000, 756000, NO_CAP, NO_CAP } },
       { { 1900000, 756000, 480000, 648000, NO_CAP, NO_CAP } },
       { { 1800000, 708000, 444000, 648000, NO_CAP, NO_CAP } },
       { { 1700000, 648000, 444000, 600000, NO_CAP, NO_CAP } },
       { { 1600000, 648000, 444000, 600000, NO_CAP, NO_CAP } },
       { { 1500000, 612000, 444000, 600000, NO_CAP, NO_CAP } },
       { { 1450000, 612000, 444000, 600000, NO_CAP, NO_CAP } },
       { { 1400000, 540000, 444000, 600000, NO_CAP, NO_CAP } },
       { { 1350000, 540000, 444000, 600000, NO_CAP, NO_CAP } },
       { { 1300000, 540000, 444000, 600000, NO_CAP, NO_CAP } },
       { { 1250000, 540000, 444000, 600000, NO_CAP, NO_CAP } },
       { { 1200000, 540000, 444000, 600000, NO_CAP, NO_CAP } },
       { { 1150000, 468000, 444000, 600000, 240000, NO_CAP } },
       { { 1100000, 468000, 444000, 600000, 240000, NO_CAP } },
       { { 1050000, 468000, 396000, 600000, 240000, NO_CAP } },
       { { 1000000, 468000, 396000, 504000, 204000, NO_CAP } },
       { {  975000, 468000, 396000, 504000, 204000, 792000 } },
       { {  950000, 468000, 396000, 504000, 204000, 792000 } },
       { {  925000, 468000, 396000, 504000, 204000, 792000 } },
       { {  900000, 468000, 348000, 504000, 204000, 792000 } },
       { {  875000, 468000, 348000, 504000, 136000, 600000 } },
       { {  850000, 468000, 348000, 420000, 136000, 600000 } },
       { {  825000, 396000, 348000, 420000, 136000, 600000 } },
       { {  800000, 396000, 348000, 420000, 136000, 528000 } },
       { {  775000, 396000, 348000, 420000, 136000, 528000 } },
};

static struct balanced_throttle skin_throttle = {
	.throt_tab_size = ARRAY_SIZE(skin_throttle_table),
	.throt_tab = skin_throttle_table,
};

static int __init flounder_skin_init(void)
{
	if (of_machine_is_compatible("google,flounder") ||
	    of_machine_is_compatible("google,flounder64")) {
		/* turn on tskin only on XE (DVT2) and later revision */
		if (flounder_get_hw_revision() >= FLOUNDER_REV_DVT2 ) {
			skin_data.ndevs = ARRAY_SIZE(skin_devs_wifi);
			skin_data.devs = skin_devs_wifi;
			skin_data.toffset = -4746;
#ifdef CONFIG_THERMAL_GOV_ADAPTIVE_SKIN
			skin_data.tzp = &skin_astg_tzp;
			skin_data.passive_delay = 6000;
#endif

			balanced_throttle_register(&skin_throttle,
							"skin-balanced");
			tegra_skin_therm_est_device.dev.platform_data =
							&skin_data;
			platform_device_register(&tegra_skin_therm_est_device);
		}
	}
	else if (of_machine_is_compatible("google,flounder_lte") ||
	    of_machine_is_compatible("google,flounder64_lte")) {
		/* turn on tskin only on LTE XD (DVT1) and later revision  */
		if (flounder_get_hw_revision() >= FLOUNDER_REV_DVT1 ) {
			skin_data.ndevs = ARRAY_SIZE(skin_devs_lte);
			skin_data.devs = skin_devs_lte;
			skin_data.toffset = -1625;
#ifdef CONFIG_THERMAL_GOV_ADAPTIVE_SKIN
			skin_data.tzp = &skin_astg_tzp;
			skin_data.passive_delay = 6000;
#endif

			balanced_throttle_register(&skin_throttle,
							"skin-balanced");
			tegra_skin_therm_est_device.dev.platform_data =
							&skin_data;
			platform_device_register(&tegra_skin_therm_est_device);
		}

	}
	return 0;
}
late_initcall(flounder_skin_init);
#endif

static struct nct1008_platform_data flounder_nct72_pdata = {
	.loc_name = "tegra",
	.supported_hwrev = true,
	.conv_rate = 0x06, /* 4Hz conversion rate */
	.offset = 0,
	.extended_range = true,

	.sensors = {
		[LOC] = {
			.tzp = &therm_est_activ_tzp,
			.shutdown_limit = 120, /* C */
			.passive_delay = 1000,
			.num_trips = 1,
			.trips = {
				{
					.cdev_type = "therm_est_activ",
					.trip_temp = 26000,
					.trip_type = THERMAL_TRIP_ACTIVE,
					.hysteresis = 1000,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
					.mask = 1,
				},
			},
		},
		[EXT] = {
			.tzp = &cpu_tzp,
			.shutdown_limit = 95, /* C */
			.passive_delay = 1000,
			.num_trips = 2,
			.trips = {
				{
					.cdev_type = "shutdown_warning",
					.trip_temp = 93000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
					.mask = 0,
				},
				{
					.cdev_type = "cpu-balanced",
					.trip_temp = 83000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
					.hysteresis = 1000,
					.mask = 1,
				},
			}
		}
	}
};

#ifdef CONFIG_TEGRA_SKIN_THROTTLE
static struct nct1008_platform_data flounder_nct72_tskin_pdata = {
	.loc_name = "skin",

	.supported_hwrev = true,
	.conv_rate = 0x06, /* 4Hz conversion rate */
	.offset = 0,
	.extended_range = true,

	.sensors = {
		[LOC] = {
			.shutdown_limit = 95, /* C */
			.num_trips = 0,
			.tzp = NULL,
		},
		[EXT] = {
			.shutdown_limit = 85, /* C */
			.passive_delay = 10000,
			.polling_delay = 1000,
			.tzp = &skin_tzp,
			.num_trips = 1,
			.trips = {
				{
					.cdev_type = "skin-balanced",
					.trip_temp = 50000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
					.mask = 1,
				},
			},
		}
	}
};
#endif

static struct i2c_board_info flounder_i2c_nct72_board_info[] = {
	{
		I2C_BOARD_INFO("nct72", 0x4c),
		.platform_data = &flounder_nct72_pdata,
		.irq = -1,
	},
#ifdef CONFIG_TEGRA_SKIN_THROTTLE
	{
		I2C_BOARD_INFO("nct72", 0x4d),
		.platform_data = &flounder_nct72_tskin_pdata,
		.irq = -1,
	}
#endif
};

static int flounder_nct72_init(void)
{
	s32 base_cp, shft_cp;
	u32 base_ft, shft_ft;
	int nct72_port = TEGRA_GPIO_PI6;
	int ret = 0;
	int i;
	struct thermal_trip_info *trip_state;

	/* raise NCT's thresholds if soctherm CP,FT fuses are ok */
	if ((tegra_fuse_calib_base_get_cp(&base_cp, &shft_cp) >= 0) &&
	    (tegra_fuse_calib_base_get_ft(&base_ft, &shft_ft) >= 0)) {
		flounder_nct72_pdata.sensors[EXT].shutdown_limit += 20;
		for (i = 0; i < flounder_nct72_pdata.sensors[EXT].num_trips;
			 i++) {
			trip_state = &flounder_nct72_pdata.sensors[EXT].trips[i];
			if (!strncmp(trip_state->cdev_type, "cpu-balanced",
					THERMAL_NAME_LENGTH)) {
				trip_state->cdev_type = "_none_";
				break;
			}
		}
	} else {
		tegra_platform_edp_init(
			flounder_nct72_pdata.sensors[EXT].trips,
			&flounder_nct72_pdata.sensors[EXT].num_trips,
					12000); /* edp temperature margin */
		tegra_add_cpu_vmax_trips(
			flounder_nct72_pdata.sensors[EXT].trips,
			&flounder_nct72_pdata.sensors[EXT].num_trips);
		tegra_add_tgpu_trips(
			flounder_nct72_pdata.sensors[EXT].trips,
			&flounder_nct72_pdata.sensors[EXT].num_trips);
		tegra_add_vc_trips(
			flounder_nct72_pdata.sensors[EXT].trips,
			&flounder_nct72_pdata.sensors[EXT].num_trips);
		tegra_add_core_vmax_trips(
			flounder_nct72_pdata.sensors[EXT].trips,
			&flounder_nct72_pdata.sensors[EXT].num_trips);
	}

	tegra_add_all_vmin_trips(flounder_nct72_pdata.sensors[EXT].trips,
		&flounder_nct72_pdata.sensors[EXT].num_trips);

	flounder_i2c_nct72_board_info[0].irq = gpio_to_irq(nct72_port);

	ret = gpio_request(nct72_port, "temp_alert");
	if (ret < 0)
		return ret;

	ret = gpio_direction_input(nct72_port);
	if (ret < 0) {
		pr_info("%s: calling gpio_free(nct72_port)", __func__);
		gpio_free(nct72_port);
	}

	i2c_register_board_info(0, flounder_i2c_nct72_board_info,
	ARRAY_SIZE(flounder_i2c_nct72_board_info));

	return ret;
}

static int powerdown_gpio_init(void){
	int ret = 0;
	static int done;
	if (!is_mdm_modem()) {
		pr_debug("[SAR]%s:!is_mdm_modem()\n", __func__);
		return ret;
	}

	if (done == 0) {
		if (!gpio_request(TEGRA_GPIO_PO5, "sar_modem")) {
			pr_debug("[SAR]%s:gpio_request success\n", __func__);
			ret = gpio_direction_output(TEGRA_GPIO_PO5, 0);
			if (ret < 0) {
				pr_debug(
					"[SAR]%s: calling gpio_free(sar_modem)",
					__func__);
				gpio_free(TEGRA_GPIO_PO5);
			}
			done = 1;
		}
	}
	return ret;
}

static int flounder_sar_init(void){
	int sar_intr = TEGRA_GPIO_PC7;
	int ret;
	pr_info("%s: GPIO pin:%d\n", __func__, sar_intr);
	flounder_i2c_board_info_cypress_sar[0].irq = gpio_to_irq(sar_intr);
	ret = gpio_request(sar_intr, "sar_interrupt");
	if (ret < 0)
		return ret;
	ret = gpio_direction_input(sar_intr);
	if (ret < 0) {
		pr_info("%s: calling gpio_free(sar_intr)", __func__);
		gpio_free(sar_intr);
	}
	powerdown_gpio_init();
	i2c_register_board_info(1, flounder_i2c_board_info_cypress_sar,
			ARRAY_SIZE(flounder_i2c_board_info_cypress_sar));
	return 0;
}

static int flounder_sar1_init(void){
	int sar1_intr = TEGRA_GPIO_PCC5;
	int ret;
	pr_info("%s: GPIO pin:%d\n", __func__, sar1_intr);
	flounder_i2c_board_info_cypress_sar1[0].irq = gpio_to_irq(sar1_intr);
	ret  = gpio_request(sar1_intr, "sar1_interrupt");
	if (ret < 0)
		return ret;
	ret = gpio_direction_input(sar1_intr);
	if (ret < 0) {
		pr_info("%s: calling gpio_free(sar1_intr)", __func__);
		gpio_free(sar1_intr);
	}
	powerdown_gpio_init();
	i2c_register_board_info(1, flounder_i2c_board_info_cypress_sar1,
			ARRAY_SIZE(flounder_i2c_board_info_cypress_sar1));
	return 0;
}

int __init flounder_sensors_init(void)
{
	flounder_camera_init();
	flounder_nct72_init();
	flounder_sar_init();
	flounder_sar1_init();

	i2c_register_board_info(0, flounder_i2c_board_info_cm32181,
		ARRAY_SIZE(flounder_i2c_board_info_cm32181));

	return 0;
}

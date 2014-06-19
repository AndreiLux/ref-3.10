/*
 * arch/arm/mach-tegra/board-flounder.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/serial_8250.h>
#include <linux/i2c.h>
#include <linux/i2c/i2c-hid.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/spi/spi.h>
#include <linux/spi/rm31080a_ts.h>
#include <linux/maxim_sti.h>
#include <linux/memblock.h>
#include <linux/spi/spi-tegra.h>
#include <linux/rfkill-gpio.h>
#include <linux/nfc/bcm2079x.h>
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>
#include <linux/regulator/consumer.h>
#include <linux/smb349-charger.h>
#include <linux/max17048_battery.h>
#include <linux/leds.h>
#include <linux/i2c/at24.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra.h>
#include <linux/platform_data/serial-tegra.h>
#include <linux/edp.h>
#include <linux/usb/tegra_usb_phy.h>
#include <linux/mfd/palmas.h>
#include <linux/clk/tegra.h>
#include <media/tegra_dtv.h>
#include <linux/clocksource.h>
#include <linux/irqchip.h>
#include <linux/irqchip/tegra.h>
#include <linux/pci-tegra.h>
#include <linux/max1187x.h>
#include <linux/tegra-soc.h>

#include <mach/irqs.h>
#include <linux/tegra_fiq_debugger.h>

#include <mach/pinmux.h>
#include <mach/pinmux-t12.h>
#include <mach/io_dpd.h>
#include <mach/i2s.h>
#include <mach/isomgr.h>
#include <mach/tegra_asoc_pdata.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/gpio-tegra.h>
#include <mach/xusb.h>
#include <linux/platform_data/tegra_ahci.h>
#include <linux/irqchip/tegra.h>
#include <linux/htc_headset_mgr.h>
#include <linux/htc_headset_pmic.h>
#include <linux/htc_headset_one_wire.h>
#include <../../../drivers/staging/android/timed_gpio.h>

#include <mach/flounder-bdaddress.h>
#include "board.h"
#include "board-flounder.h"
#include "board-common.h"
#include "board-touch-raydium.h"
#include "board-touch-maxim_sti.h"
#include "clock.h"
#include "common.h"
#include "devices.h"
#include "gpio-names.h"
#include "iomap.h"
#include "pm.h"
#include "tegra-board-id.h"
#include "../../../sound/soc/codecs/rt5506.h"
#include "../../../sound/soc/codecs/rt5677.h"
#include "../../../sound/soc/codecs/tfa9895.h"
#include "../../../sound/soc/codecs/rt5677-spi.h"

static unsigned int flounder_hw_rev;
static unsigned int flounder_eng_id;

static int __init flounder_hw_revision(char *id)
{
	int ret;
	char *hw_rev;

	hw_rev = strsep(&id, ".");

	ret = kstrtouint(hw_rev, 10, &flounder_hw_rev);
	if (ret < 0) {
		pr_err("Failed to parse flounder hw_revision=%s\n", hw_rev);
		return ret;
	}

	if (id) {
		ret = kstrtouint(id, 10, &flounder_eng_id);
		if (ret < 0) {
			pr_err("Failed to parse flounder eng_id=%s\n", id);
			return ret;
		}
	}

	pr_info("Flounder hardware revision = %d, engineer id = %d\n",
            flounder_hw_rev, flounder_eng_id);

	return 0;
}
early_param("hw_revision", flounder_hw_revision);

int flounder_get_hw_revision(void)
{
	return flounder_hw_rev;
}

int flounder_get_eng_id(void)
{
	return flounder_eng_id;
}

struct aud_sfio_data {
	const char *name;
	int id;
};

static struct aud_sfio_data audio_sfio_pins[] = {
	[0] = {
		.name = "I2S1_LRCK",
		.id   = TEGRA_GPIO_PA2,
	},
	[1] = {
		.name = "I2S1_SCLK",
		.id   = TEGRA_GPIO_PA3,
	},
	[2] = {
		.name = "I2S1_SDATA_IN",
		.id   = TEGRA_GPIO_PA4,
	},
	[3] = {
		.name = "I2S1_SDATA_OUT",
		.id   = TEGRA_GPIO_PA5,
	},
	[4] = {
		.name = "I2S2_LRCK",
		.id   = TEGRA_GPIO_PP0,
	},
	[5] = {
		.name = "I2S2_SDATA_IN",
		.id   = TEGRA_GPIO_PP1,
	},
	[6] = {
		.name = "I2S2_SDATA_OUT",
		.id   = TEGRA_GPIO_PP2,
	},
	[7] = {
		.name = "I2S2_SCLK",
		.id   = TEGRA_GPIO_PP3,
	},
	[8] = {
		.name = "extperiph1_clk",
		.id   = TEGRA_GPIO_PW4,
	},
	[9] = {
		.name = "SPI_MOSI",
		.id   = TEGRA_GPIO_PY0,
	},
	[10] = {
		.name = "SPI_MISO",
		.id   = TEGRA_GPIO_PY1,
	},
	[11] = {
		.name = "SPI_SCLK",
		.id   = TEGRA_GPIO_PY2,
	},
	[12] = {
		.name = "SPI_CS",
		.id   = TEGRA_GPIO_PY3,
	},
};

static struct resource flounder_bluedroid_pm_resources[] = {
	[0] = {
		.name   = "shutdown_gpio",
		.start  = TEGRA_GPIO_PR1,
		.end    = TEGRA_GPIO_PR1,
		.flags  = IORESOURCE_IO,
	},
	[1] = {
		.name = "host_wake",
		.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
	[2] = {
		.name = "gpio_ext_wake",
		.start  = TEGRA_GPIO_PEE1,
		.end    = TEGRA_GPIO_PEE1,
		.flags  = IORESOURCE_IO,
	},
	[3] = {
		.name = "gpio_host_wake",
		.start  = TEGRA_GPIO_PU6,
		.end    = TEGRA_GPIO_PU6,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device flounder_bluedroid_pm_device = {
	.name = "bluedroid_pm",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(flounder_bluedroid_pm_resources),
	.resource       = flounder_bluedroid_pm_resources,
};

static noinline void __init flounder_setup_bluedroid_pm(void)
{
	flounder_bluedroid_pm_resources[1].start =
		flounder_bluedroid_pm_resources[1].end =
				gpio_to_irq(TEGRA_GPIO_PU6);
	platform_device_register(&flounder_bluedroid_pm_device);
}

static struct tfa9895_platform_data tfa9895_data = {
	.tfa9895_power_enable = TEGRA_GPIO_PX5,
};
struct rt5677_priv rt5677_data = {
	.vad_clock_en = TEGRA_GPIO_PX3,
};

static struct i2c_board_info __initdata rt5677_board_info = {
	I2C_BOARD_INFO("rt5677", 0x2d),
	.platform_data = &rt5677_data,
};
static struct i2c_board_info __initdata tfa9895_board_info = {
	I2C_BOARD_INFO("tfa9895", 0x34),
	.platform_data = &tfa9895_data,
};
static struct i2c_board_info __initdata tfa9895l_board_info = {
	I2C_BOARD_INFO("tfa9895l", 0x35),
};

static struct bcm2079x_platform_data bcm2079x_pdata = {
	.irq_gpio = TEGRA_GPIO_PR7,
	.en_gpio = TEGRA_GPIO_PB1,
	.wake_gpio= TEGRA_GPIO_PS1,
};

static struct i2c_board_info __initdata bcm2079x_board_info = {
	I2C_BOARD_INFO("bcm2079x-i2c", 0x77),
	.platform_data = &bcm2079x_pdata,
};

static __initdata struct tegra_clk_init_table flounder_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "pll_m",	NULL,		0,		false},
	{ "hda",	"pll_p",	108000000,	false},
	{ "hda2codec_2x", "pll_p",	48000000,	false},
	{ "pwm",	"pll_p",	3187500,	false},
	{ "i2s1",	"pll_a_out0",	0,		false},
	{ "i2s2",	"pll_a_out0",	0,		false},
	{ "i2s3",	"pll_a_out0",	0,		false},
	{ "i2s4",	"pll_a_out0",	0,		false},
	{ "spdif_out",	"pll_a_out0",	0,		false},
	{ "d_audio",	"clk_m",	12000000,	false},
	{ "dam0",	"clk_m",	12000000,	false},
	{ "dam1",	"clk_m",	12000000,	false},
	{ "dam2",	"clk_m",	12000000,	false},
	{ "audio1",	"i2s1_sync",	0,		false},
	{ "audio2",	"i2s2_sync",	0,		false},
	{ "audio3",	"i2s3_sync",	0,		false},
	{ "vi_sensor",	"pll_p",	150000000,	false},
	{ "vi_sensor2",	"pll_p",	150000000,	false},
	{ "cilab",	"pll_p",	150000000,	false},
	{ "cilcd",	"pll_p",	150000000,	false},
	{ "cile",	"pll_p",	150000000,	false},
	{ "i2c1",	"pll_p",	3200000,	false},
	{ "i2c2",	"pll_p",	3200000,	false},
	{ "i2c3",	"pll_p",	3200000,	false},
	{ "i2c4",	"pll_p",	3200000,	false},
	{ "i2c5",	"pll_p",	3200000,	false},
	{ "sbc1",	"pll_p",	25000000,	false},
	{ "sbc2",	"pll_p",	25000000,	false},
	{ "sbc3",	"pll_p",	25000000,	false},
	{ "sbc4",	"pll_p",	25000000,	false},
	{ "sbc5",	"pll_p",	25000000,	false},
	{ "sbc6",	"pll_p",	25000000,	false},
	{ "uarta",	"pll_p",	408000000,	false},
	{ "uartb",	"pll_p",	408000000,	false},
	{ "uartc",	"pll_p",	408000000,	false},
	{ "uartd",	"pll_p",	408000000,	false},
	{ NULL,		NULL,		0,		0},
};

struct max1187x_board_config max1187x_config_data[] = {
	{
		.config_id = 0x03E7,
		.chip_id = 0x75,
		.major_ver = 1,
		.minor_ver = 10,
		.protocol_ver = 8,
		.config_touch = {
			0x03E7, 0x141F, 0x0078, 0x001E, 0x0A01, 0x0100, 0x0302, 0x0504,
			0x0706, 0x0908, 0x0B0A, 0x0D0C, 0x0F0E, 0x1110, 0x1312, 0x1514,
			0x1716, 0x1918, 0x1B1A, 0x1D1C, 0xFF1E, 0xFFFF, 0xFFFF, 0xFFFF,
			0xFFFF, 0x0100, 0x0302, 0x0504, 0x0706, 0x0908, 0x0B0A, 0x0D0C,
			0x0F0E, 0x1110, 0x1312, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
			0xFFFF, 0x0EFF, 0x895F, 0xFF13, 0x0000, 0x1402, 0x04B0, 0x04B0,
			0x04B0, 0x0514, 0x00B4, 0x1A00, 0x0A08, 0x00B4, 0x0082, 0xFFFF,
			0xFFFF, 0x03E8, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
			0x5046
		},
		.config_cal = {
			0xFFF5, 0xFFEA, 0xFFDF, 0x001E, 0x001E, 0x001E, 0x001E, 0x001E,
			0x001E, 0x001E, 0x001E, 0x001E, 0x001E, 0x001E, 0x001E, 0x001E,
			0x001E, 0x001E, 0x001E, 0x001E, 0x001E, 0x001E, 0x001E, 0x001E,
			0x001E, 0x001E, 0x001E, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F,
			0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F,
			0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F,
			0x000F, 0x000F, 0x000F, 0xFFFF, 0xFF1E, 0x00FF, 0x00FF, 0x00FF,
			0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x000A, 0x0001,
			0x0001, 0x0002, 0x0002, 0x0003, 0x0001, 0x0001, 0x0002, 0x0002,
			0x0003, 0x0C26
		},
		.config_private = {
			0x0118, 0x0069, 0x0082, 0x0038, 0xF0FF, 0x1428, 0x001E, 0x0190,
			0x03B6, 0x00AA, 0x00C8, 0x0018, 0x04E2, 0x003C, 0x0000, 0xB232,
			0xFEFE, 0xFFFF, 0xFFFF, 0xFFFF, 0x00FF, 0xFF64, 0x4E21, 0x1403,
			0x78C8, 0x524C, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
			0xFFFF, 0xF22F
		},
		.config_lin_x = {
			0x002B, 0x3016, 0x644C, 0x8876, 0xAA99, 0xCBBB, 0xF0E0, 0x8437
		},
		.config_lin_y = {
			0x0030, 0x2E17, 0x664B, 0x8F7D, 0xAE9F, 0xCABC, 0xEADA, 0x8844
		},
	},
	{
		.config_id = 0,
		.chip_id = 0x75,
		.major_ver = 0,
		.minor_ver = 0,
	},
};

struct max1187x_pdata max1187x_platdata = {
	.fw_config = max1187x_config_data,
	.gpio_tirq = TEGRA_GPIO_PK2,
	.gpio_reset = TEGRA_GPIO_PX6,
	.num_fw_mappings = 1,
	.fw_mapping[0] = {.chip_id = 0x75, .filename = "max11876.bin", .filesize = 0xC000, .file_codesize = 0xC000},
	.defaults_allow = 1,
	.default_config_id = 0x5800,
	.default_chip_id = 0x75,
	.i2c_words = 128,
	.coordinate_settings = 0,
	.panel_min_x = 0,
	.panel_max_x = 3840,
	.panel_min_y = 0,
	.panel_max_y = 2600,
	.lcd_x = 2560,
	.lcd_y = 1600,
	.num_rows = 32,
	.num_cols = 20,
	.input_protocol = MAX1187X_PROTOCOL_B,
	.update_feature = MAX1187X_UPDATE_CONFIG,
	.tw_mask = 0xF,
	.button_code0 = KEY_HOME,
	.button_code1 = KEY_BACK,
	.button_code2 = KEY_RESERVED,
	.button_code3 = KEY_RESERVED,
	.report_mode = MAX1187X_REPORT_MODE_EXTEND,
};

static void flounder_i2c_init(void)
{
	i2c_register_board_info(1, &rt5677_board_info, 1);
	i2c_register_board_info(1, &tfa9895_board_info, 1);
	i2c_register_board_info(1, &tfa9895l_board_info, 1);

	bcm2079x_board_info.irq = gpio_to_irq(TEGRA_GPIO_PR7),
	i2c_register_board_info(0, &bcm2079x_board_info, 1);
}

#ifndef CONFIG_USE_OF
static struct platform_device *flounder_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
};

static struct tegra_serial_platform_data flounder_uarta_pdata = {
	.dma_req_selector = 8,
	.modem_interrupt = false,
};

static struct tegra_serial_platform_data flounder_uartb_pdata = {
	.dma_req_selector = 9,
	.modem_interrupt = false,
};

static struct tegra_serial_platform_data flounder_uartc_pdata = {
	.dma_req_selector = 10,
	.modem_interrupt = false,
};

static struct tegra_serial_platform_data flounder_uartd_pdata = {
	.dma_req_selsctor = 19,
	.modem_interrupt = false,
};
#endif
static struct tegra_serial_platform_data flounder_uarta_pdata = {
	.dma_req_selector = 8,
	.modem_interrupt = false,
};

static struct tegra_asoc_platform_data flounder_audio_pdata_rt5677 = {
	.gpio_hp_det = -1,
	.gpio_ldo1_en = TEGRA_GPIO_PK0,
	.gpio_ldo2_en = TEGRA_GPIO_PQ3,
	.gpio_reset = TEGRA_GPIO_PX4,
	.gpio_irq1 = TEGRA_GPIO_PS4,
	.gpio_wakeup = TEGRA_GPIO_PO0,
	.gpio_spkr_en = -1,
	.gpio_spkr_ldo_en = TEGRA_GPIO_PX5,
	.gpio_int_mic_en = TEGRA_GPIO_PV3,
	.gpio_ext_mic_en = TEGRA_GPIO_PS3,
	.gpio_hp_mute = -1,
	.gpio_hp_en = TEGRA_GPIO_PX1,
	.gpio_hp_ldo_en = -1,
	.gpio_codec1 = -1,
	.gpio_codec2 = -1,
	.gpio_codec3 = -1,
	.i2s_param[HIFI_CODEC]       = {
		.audio_port_id = 1,
		.is_i2s_master = 1,
		.i2s_mode = TEGRA_DAIFMT_I2S,
	},
	.i2s_param[SPEAKER]       = {
		.audio_port_id = 2,
		.is_i2s_master = 1,
		.i2s_mode = TEGRA_DAIFMT_I2S,
	},
	.i2s_param[BT_SCO] = {
		.audio_port_id = 3,
		.is_i2s_master = 1,
		.i2s_mode = TEGRA_DAIFMT_DSP_A,
	},
	/* Add for MI2S driver to get GPIO */
	.i2s_set[HIFI_CODEC*4 + 0] = {
		.name = "I2S1_LRCK",
		.id   = TEGRA_GPIO_PA2,
	},
	.i2s_set[HIFI_CODEC*4 + 1] = {
		.name = "I2S1_SCLK",
		.id   = TEGRA_GPIO_PA3,
	},
	.i2s_set[HIFI_CODEC*4 + 2] = {
		.name = "I2S1_SDATA_IN",
		.id   = TEGRA_GPIO_PA4,
		.dir_in = 1,
		.pg = TEGRA_PINGROUP_DAP2_DIN,
	},
	.i2s_set[HIFI_CODEC*4 + 3] = {
		.name = "I2S1_SDATA_OUT",
		.id   = TEGRA_GPIO_PA5,
	},
	.i2s_set[SPEAKER*4 + 0] = {
		.name = "I2S2_LRCK",
		.id   = TEGRA_GPIO_PP0,
	},
	.i2s_set[SPEAKER*4 + 1] = {
		.name = "I2S2_SDATA_IN",
		.id   = TEGRA_GPIO_PP1,
		.dir_in = 1,
		.pg   = TEGRA_PINGROUP_DAP3_DIN,
	},
	.i2s_set[SPEAKER*4 + 2] = {
		.name = "I2S2_SDATA_OUT",
		.id   = TEGRA_GPIO_PP2,
	},
	.i2s_set[SPEAKER*4 + 3] = {
		.name = "I2S2_SCLK",
		.id   = TEGRA_GPIO_PP3,
	},
	.first_time_free[HIFI_CODEC] = 1,
	.first_time_free[SPEAKER] = 1,
	.codec_mclk = {
		.name = "extperiph1_clk",
		.id   = TEGRA_GPIO_PW4,
	}
};

static struct tegra_spi_device_controller_data dev_cdata_rt5677 = {
	.rx_clk_tap_delay = 0,
	.tx_clk_tap_delay = 16,
};

static struct gpio_config flounder_spi_pdata_rt5677[] = {
	[0] = {
		.name = "SPI5_MOSI",
		.id = TEGRA_GPIO_PY0,
	},
	[1] = {
		.name = "SPI5_MISO",
		.id = TEGRA_GPIO_PY1,
		.dir_in = 1,
		.pg = TEGRA_PINGROUP_ULPI_DIR,
	},
	[2] = {
		.name = "SPI5_SCLK",
		.id = TEGRA_GPIO_PY2,
	},
	[3] = {
		.name = "SPI5_CS#",
		.id = TEGRA_GPIO_PY3,
	},
};

void flounder_rt5677_spi_suspend(bool on)
{
	int i, ret;
	if (on) {
		pr_info("%s: suspend", __func__);
		for (i = 0; i < ARRAY_SIZE(flounder_spi_pdata_rt5677); i++) {
			ret = gpio_request(flounder_spi_pdata_rt5677[i].id,
								flounder_spi_pdata_rt5677[i].name);
			if (ret < 0) {
				pr_err("%s: gpio_request failed for gpio[%d] %s, return %d\n",
				__func__, flounder_spi_pdata_rt5677[i].id, flounder_spi_pdata_rt5677[i].name, ret);
				continue;
			}
			if (!flounder_spi_pdata_rt5677[i].dir_in) {
				gpio_direction_output(flounder_spi_pdata_rt5677[i].id, 0);
			} else {
				tegra_pinctrl_pg_set_pullupdown(flounder_spi_pdata_rt5677[i].pg,
												TEGRA_PUPD_PULL_DOWN);
				gpio_direction_input(flounder_spi_pdata_rt5677[i].id);
			}

		}
	} else {
		pr_info("%s: resume", __func__);
		for (i = 0; i < ARRAY_SIZE(flounder_spi_pdata_rt5677); i++) {
			gpio_free(flounder_spi_pdata_rt5677[i].id);
		}
	}
}

static struct rt5677_spi_platform_data rt5677_spi_pdata = {
	.spi_suspend = flounder_rt5677_spi_suspend
};

struct spi_board_info rt5677_flounder_spi_board[1] = {
	{
	 .modalias = "rt5677_spidev",
	 .bus_num = 4,
	 .chip_select = 0,
	 .max_speed_hz = 12 * 1000 * 1000,
	 .mode = SPI_MODE_0,
	 .controller_data = &dev_cdata_rt5677,
	 .platform_data = &rt5677_spi_pdata,
	 },
};

static void flounder_audio_init(void)
{
	int i;
	flounder_audio_pdata_rt5677.codec_name = "rt5677.1-002d";
	flounder_audio_pdata_rt5677.codec_dai_name = "rt5677-aif1";

	spi_register_board_info(&rt5677_flounder_spi_board[0],
	ARRAY_SIZE(rt5677_flounder_spi_board));
	/* To prevent power leakage */
	gpio_request(TEGRA_GPIO_PN1, "I2S0_SDATA_IN");
	tegra_pinctrl_pg_set_pullupdown(TEGRA_PINGROUP_DAP1_DIN, TEGRA_PUPD_PULL_DOWN);

	/* To config SFIO */
	for (i = 0; i < ARRAY_SIZE(audio_sfio_pins); i++)
		if (tegra_is_gpio(audio_sfio_pins[i].id)) {
			gpio_request(audio_sfio_pins[i].id, audio_sfio_pins[i].name);
			gpio_free(audio_sfio_pins[i].id);
			pr_info("%s: gpio_free for gpio[%d] %s\n",
				__func__, audio_sfio_pins[i].id, audio_sfio_pins[i].name);
		}
}

static struct platform_device flounder_audio_device_rt5677 = {
	.name = "tegra-snd-rt5677",
	.id = 0,
	.dev = {
		.platform_data = &flounder_audio_pdata_rt5677,
	},
};

static void __init flounder_uart_init(void)
{

#ifndef CONFIG_USE_OF
	tegra_uarta_device.dev.platform_data = &flounder_uarta_pdata;
	tegra_uartb_device.dev.platform_data = &flounder_uartb_pdata;
	tegra_uartc_device.dev.platform_data = &flounder_uartc_pdata;
	tegra_uartd_device.dev.platform_data = &flounder_uartd_pdata;
	platform_add_devices(flounder_uart_devices,
			ARRAY_SIZE(flounder_uart_devices));
#endif
	tegra_uarta_device.dev.platform_data = &flounder_uarta_pdata;
	if (!is_tegra_debug_uartport_hs()) {
		int debug_port_id = uart_console_debug_init(0);
		if (debug_port_id < 0)
			return;

#ifdef CONFIG_TEGRA_FIQ_DEBUGGER
#ifndef CONFIG_TRUSTY_FIQ
		tegra_serial_debug_init_irq_mode(TEGRA_UARTA_BASE, INT_UARTA, NULL, -1, -1);
#endif
#else
		platform_device_register(uart_console_debug_device);
#endif
	} else {
		tegra_uarta_device.dev.platform_data = &flounder_uarta_pdata;
		platform_device_register(&tegra_uarta_device);
	}
}

static struct resource tegra_rtc_resources[] = {
	[0] = {
		.start = TEGRA_RTC_BASE,
		.end = TEGRA_RTC_BASE + TEGRA_RTC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_RTC,
		.end = INT_RTC,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device tegra_rtc_device = {
	.name = "tegra_rtc",
	.id   = -1,
	.resource = tegra_rtc_resources,
	.num_resources = ARRAY_SIZE(tegra_rtc_resources),
};

static struct timed_gpio flounder_vib_timed_gpios[] = {
	{
		.name = "vibrator",
		.gpio = TEGRA_GPIO_PU3,
		.max_timeout = 15000,
	},
};

static struct timed_gpio_platform_data flounder_vib_pdata = {
	.num_gpios = ARRAY_SIZE(flounder_vib_timed_gpios),
	.gpios     = flounder_vib_timed_gpios,
};

static struct platform_device flounder_vib_device = {
	.name = TIMED_GPIO_NAME,
	.id   = -1,
	.dev  = {
		.platform_data = &flounder_vib_pdata,
	},
};

static struct platform_device *flounder_devices[] __initdata = {
	&tegra_pmu_device,
	&tegra_rtc_device,
	&tegra_udc_device,
#if defined(CONFIG_TEGRA_WATCHDOG)
#ifndef CONFIG_TRUSTY_FIQ
	&tegra_wdt0_device,
#endif
#endif
#if defined(CONFIG_TEGRA_AVP)
	&tegra_avp_device,
#endif
	&tegra_ahub_device,
	&tegra_dam_device0,
	&tegra_dam_device1,
	&tegra_dam_device2,
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&tegra_i2s_device3,
	&tegra_i2s_device4,
	&flounder_audio_device_rt5677,
	&tegra_spdif_device,
	&spdif_dit_device,
	&bluetooth_dit_device,
	&tegra_hda_device,
#if IS_ENABLED(CONFIG_SND_SOC_TEGRA_OFFLOAD)
	&tegra_offload_device,
#endif
#if IS_ENABLED(CONFIG_SND_SOC_TEGRA30_AVP)
	&tegra30_avp_audio_device,
#endif

#if defined(CONFIG_CRYPTO_DEV_TEGRA_AES)
	&tegra_aes_device,
#endif
	&flounder_vib_device,
};

static struct tegra_usb_platform_data tegra_udc_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_DEVICE,
	.u_data.dev = {
		.vbus_pmu_irq = 0,
		.vbus_gpio = -1,
		.charging_supported = true,
		.remote_wakeup_supported = false,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 64,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_hsslew_lsb = 0,
		.xcvr_hsslew_msb = 0,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 0,
	},
};

static struct tegra_usb_platform_data tegra_ehci1_utmi_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 15,
		.xcvr_lsfslew = 0,
		.xcvr_lsrslew = 3,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
		.vbus_oc_map = 0x4,
		.xcvr_hsslew_lsb = 2,
	},
};

static struct tegra_usb_platform_data tegra_ehci2_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
		.vbus_oc_map = 0x5,
	},
};

static struct tegra_usb_platform_data tegra_ehci3_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
	.u_cfg.utmi = {
	.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
		.vbus_oc_map = 0x5,
	},
};

static struct tegra_usb_otg_data tegra_otg_pdata = {
	.ehci_device = &tegra_ehci1_device,
	.ehci_pdata = &tegra_ehci1_utmi_pdata,
	.id_det_gpio = TEGRA_GPIO_PW2,
};

static void flounder_usb_init(void)
{
	int usb_port_owner_info = tegra_get_usb_port_owner_info();

	/* Device cable is detected through PMU Interrupt */
	tegra_udc_pdata.support_pmu_vbus = true;
	tegra_ehci1_utmi_pdata.support_pmu_vbus = true;
	tegra_ehci1_utmi_pdata.vbus_extcon_dev_name = "palmas-extcon";
	/* Host cable is detected through GPIO Interrupt */
	tegra_udc_pdata.id_det_type = TEGRA_USB_GPIO_ID;
	tegra_udc_pdata.vbus_extcon_dev_name = "palmas-extcon";
	tegra_ehci1_utmi_pdata.id_det_type = TEGRA_USB_GPIO_ID;
	tegra_ehci1_utmi_pdata.id_extcon_dev_name = "palmas-extcon";

	if (!(usb_port_owner_info & UTMI1_PORT_OWNER_XUSB)) {
		tegra_otg_pdata.is_xhci = false;
		tegra_udc_pdata.u_data.dev.is_xhci = false;
	} else {
		tegra_otg_pdata.is_xhci = true;
		tegra_udc_pdata.u_data.dev.is_xhci = true;
	}
	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);
	/* Setup the udc platform data */
	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;

	if ((!(usb_port_owner_info & UTMI2_PORT_OWNER_XUSB))
		/* tegra_ehci2_device will reserve for mdm9x25 modem */
		&& (!is_mdm_modem()))
	{
		tegra_ehci2_device.dev.platform_data =
			&tegra_ehci2_utmi_pdata;
		platform_device_register(&tegra_ehci2_device);
	}
	if (!(usb_port_owner_info & UTMI2_PORT_OWNER_XUSB)) {
		tegra_ehci3_device.dev.platform_data = &tegra_ehci3_utmi_pdata;
		platform_device_register(&tegra_ehci3_device);
	}
}

static struct tegra_xusb_platform_data xusb_pdata = {
	.portmap = TEGRA_XUSB_SS_P0 | TEGRA_XUSB_USB2_P0 | TEGRA_XUSB_SS_P1 |
			TEGRA_XUSB_USB2_P1 | TEGRA_XUSB_USB2_P2,
};

static void flounder_xusb_init(void)
{
	int usb_port_owner_info = tegra_get_usb_port_owner_info();

	xusb_pdata.lane_owner = (u8) tegra_get_lane_owner_info();

	if (!(usb_port_owner_info & UTMI1_PORT_OWNER_XUSB))
		xusb_pdata.portmap &= ~(TEGRA_XUSB_USB2_P0 |
			TEGRA_XUSB_SS_P0);

	if (!(usb_port_owner_info & UTMI2_PORT_OWNER_XUSB))
		xusb_pdata.portmap &= ~(TEGRA_XUSB_USB2_P1 |
			TEGRA_XUSB_USB2_P2 | TEGRA_XUSB_SS_P1);

	if (usb_port_owner_info & HSIC1_PORT_OWNER_XUSB)
		xusb_pdata.portmap |= TEGRA_XUSB_HSIC_P0;

	if (usb_port_owner_info & HSIC2_PORT_OWNER_XUSB)
		xusb_pdata.portmap |= TEGRA_XUSB_HSIC_P1;
}

#ifndef CONFIG_USE_OF
static struct platform_device *flounder_spi_devices[] __initdata = {
	&tegra11_spi_device1,
	&tegra11_spi_device4,
};

static struct tegra_spi_platform_data flounder_spi1_pdata = {
	.dma_req_sel		= 15,
	.spi_max_frequency	= 25000000,
	.clock_always_on	= false,
};

static struct tegra_spi_platform_data flounder_spi4_pdata = {
	.dma_req_sel		= 18,
	.spi_max_frequency	= 25000000,
	.clock_always_on	= false,
};
static void __init flounder_spi_init(void)
{
	tegra11_spi_device1.dev.platform_data = &flounder_spi1_pdata;
	tegra11_spi_device4.dev.platform_data = &flounder_spi4_pdata;
	platform_add_devices(flounder_spi_devices,
			ARRAY_SIZE(flounder_spi_devices));
}
#else
static void __init flounder_spi_init(void)
{
}
#endif

#ifdef CONFIG_USE_OF
static struct of_dev_auxdata flounder_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000d400, "spi-tegra114.0",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000d600, "spi-tegra114.1",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000d800, "spi-tegra114.2",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000da00, "spi-tegra114.3",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000dc00, "spi-tegra114.4",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000de00, "spi-tegra114.5",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-apbdma", 0x60020000, "tegra-apbdma",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-se", 0x70012000, "tegra12-se", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-host1x", TEGRA_HOST1X_BASE, "host1x",
		NULL),
		OF_DEV_AUXDATA("nvidia,tegra124-gk20a", TEGRA_GK20A_BAR0_BASE,
			"gk20a.0", NULL),
#ifdef CONFIG_ARCH_TEGRA_VIC
	OF_DEV_AUXDATA("nvidia,tegra124-vic", TEGRA_VIC_BASE, "vic03.0", NULL),
#endif
	OF_DEV_AUXDATA("nvidia,tegra124-msenc", TEGRA_MSENC_BASE, "msenc",
		NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-vi", TEGRA_VI_BASE, "vi.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-isp", TEGRA_ISP_BASE, "isp.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-isp", TEGRA_ISPB_BASE, "isp.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-tsec", TEGRA_TSEC_BASE, "tsec", NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-hsuart", 0x70006000, "serial-tegra.0",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-hsuart", 0x70006040, "serial-tegra.1",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-hsuart", 0x70006200, "serial-tegra.2",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-hsuart", 0x70006300, "serial-tegra.3",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-i2c", 0x7000c000, "tegra12-i2c.0",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-i2c", 0x7000c400, "tegra12-i2c.1",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-i2c", 0x7000c500, "tegra12-i2c.2",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-i2c", 0x7000c700, "tegra12-i2c.3",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-i2c", 0x7000d000, "tegra12-i2c.4",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-i2c", 0x7000d100, "tegra12-i2c.5",
				NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-xhci", 0x70090000, "tegra-xhci",
				&xusb_pdata),
	OF_DEV_AUXDATA("nvidia,tegra124-camera", 0, "pcl-generic",
				NULL),
	{}
};
#endif

static struct maxim_sti_pdata maxim_sti_pdata = {
	.touch_fusion         = "/vendor/bin/touch_fusion",
	.config_file          = "/vendor/firmware/touch_fusion.cfg",
	.fw_name              = "maxim_fp35.bin",
	.nl_family            = TF_FAMILY_NAME,
	.nl_mc_groups         = 5,
	.chip_access_method   = 2,
	.default_reset_state  = 0,
	.tx_buf_size          = 4100,
	.rx_buf_size          = 4100,
	.gpio_reset           = TOUCH_GPIO_RST_MAXIM_STI_SPI,
	.gpio_irq             = TOUCH_GPIO_IRQ_MAXIM_STI_SPI
};

static struct tegra_spi_device_controller_data maxim_dev_cdata = {
	.rx_clk_tap_delay = 0,
	.is_hw_based_cs = true,
	.tx_clk_tap_delay = 0,
};

static struct spi_board_info maxim_sti_spi_board = {
	.modalias = MAXIM_STI_NAME,
	.bus_num = TOUCH_SPI_ID,
	.chip_select = TOUCH_SPI_CS,
	.max_speed_hz = 12 * 1000 * 1000,
	.mode = SPI_MODE_0,
	.platform_data = &maxim_sti_pdata,
	.controller_data = &maxim_dev_cdata,
};

static int __init flounder_touch_init(void)
{
	pr_info("%s init synaptics spi touch\n", __func__);

	if (of_find_node_by_path("/spi@7000d800/synaptics_dsx@0") == NULL) {
		pr_info("[TP] %s init maxim spi touch\n", __func__);
		(void)touch_init_maxim_sti(&maxim_sti_spi_board);
	} else {
		pr_info("[TP] synaptics device tree found\n");
	}
	return 0;
}

#define	EARPHONE_DET TEGRA_GPIO_PW3
#define	HSMIC_2V85_EN TEGRA_GPIO_PS3
#define AUD_REMO_PRES TEGRA_GPIO_PS2
#define	AUD_REMO_TX_OE TEGRA_GPIO_PQ4
#define	AUD_REMO_TX TEGRA_GPIO_PJ7
#define	AUD_REMO_RX TEGRA_GPIO_PB0

static void headset_init(void)
{
	int ret ;

	ret = gpio_request(HSMIC_2V85_EN, "HSMIC_2V85_EN");
	if (ret < 0){
		pr_err("[HS] %s: gpio_request failed for gpio %s\n",
                        __func__, "HSMIC_2V85_EN");
	}

	ret = gpio_request(AUD_REMO_TX_OE, "AUD_REMO_TX_OE");
	if (ret < 0){
		pr_err("[HS] %s: gpio_request failed for gpio %s\n",
                        __func__, "AUD_REMO_TX_OE");
	}

	ret = gpio_request(AUD_REMO_TX, "AUD_REMO_TX");
	if (ret < 0){
		pr_err("[HS] %s: gpio_request failed for gpio %s\n",
                        __func__, "AUD_REMO_TX");
	}

	ret = gpio_request(AUD_REMO_RX, "AUD_REMO_RX");
	if (ret < 0){
		pr_err("[HS] %s: gpio_request failed for gpio %s\n",
                        __func__, "AUD_REMO_RX");
	}

	gpio_direction_output(HSMIC_2V85_EN, 0);
	gpio_direction_output(AUD_REMO_TX_OE, 1);

}

static void headset_power(int enable)
{
	pr_info("[HS_BOARD] (%s) Set MIC bias %d\n", __func__, enable);

	if (enable)
		gpio_set_value(HSMIC_2V85_EN, 1);
	else {
		gpio_set_value(HSMIC_2V85_EN, 0);
	}
}

#ifdef CONFIG_HEADSET_DEBUG_UART
#define	AUD_DEBUG_EN TEGRA_GPIO_PK5
static int headset_get_debug(void)
{
	int ret = 0;
	ret = gpio_get_value(AUD_DEBUG_EN);
	pr_info("[HS_BOARD] (%s) AUD_DEBUG_EN=%d\n", __func__, ret);

	return ret;
}
#endif

/* HTC_HEADSET_PMIC Driver */
static struct htc_headset_pmic_platform_data htc_headset_pmic_data = {
	.driver_flag		= DRIVER_HS_PMIC_ADC,
	.hpin_gpio		= EARPHONE_DET,
	.hpin_irq		= 0,
	.key_gpio		= AUD_REMO_PRES,
	.key_irq		= 0,
	.key_enable_gpio	= 0,
	.adc_mic		= 0,
	.adc_remote 	= {0, 117, 118, 262, 263, 414, 415, 829},
	.hs_controller		= 0,
	.hs_switch		= 0,
	.iio_channel_name = "hs_channel",
#ifdef CONFIG_HEADSET_DEBUG_UART
	.debug_gpio		= AUD_DEBUG_EN,
	.debug_irq		= 0,
	.headset_get_debug	= headset_get_debug,
#endif
};

static struct platform_device htc_headset_pmic = {
	.name	= "HTC_HEADSET_PMIC",
	.id	= -1,
	.dev	= {
		.platform_data	= &htc_headset_pmic_data,
	},
};

static struct htc_headset_1wire_platform_data htc_headset_1wire_data = {
	.tx_level_shift_en	= AUD_REMO_TX_OE,
	.uart_sw		= 0,
	.one_wire_remote	={0x7E, 0x7F, 0x7D, 0x7F, 0x7B, 0x7F},
	.remote_press		= 0,
	.onewire_tty_dev	= "/dev/ttyTHS3",
};

static struct platform_device htc_headset_one_wire = {
	.name	= "HTC_HEADSET_1WIRE",
	.id	= -1,
	.dev	= {
		.platform_data	= &htc_headset_1wire_data,
	},
};

static void uart_tx_gpo(int mode)
{
	pr_info("[HS_BOARD] (%s) Set uart_tx_gpo mode = %d\n", __func__, mode);
	switch (mode) {
		case 0:
			gpio_direction_output(AUD_REMO_TX, 0);
			break;
		case 1:
			gpio_direction_output(AUD_REMO_TX, 1);
			break;
		case 2:
			tegra_gpio_disable(AUD_REMO_TX);
			break;
	}
}

static void uart_lv_shift_en(int enable)
{
	pr_info("[HS_BOARD] (%s) Set uart_lv_shift_en %d\n", __func__, enable);
	gpio_direction_output(AUD_REMO_TX_OE, enable);
}

/* HTC_HEADSET_MGR Driver */
static struct platform_device *headset_devices[] = {
	&htc_headset_pmic,
	&htc_headset_one_wire,
	/* Please put the headset detection driver on the last */
};

static struct headset_adc_config htc_headset_mgr_config[] = {
	{
		.type = HEADSET_MIC,
		.adc_max = 3680,
		.adc_min = 621,
	},
	{
		.type = HEADSET_NO_MIC,
		.adc_max = 620,
		.adc_min = 0,
	},
};

static struct htc_headset_mgr_platform_data htc_headset_mgr_data = {
	.driver_flag		= DRIVER_HS_MGR_FLOAT_DET,
	.headset_devices_num	= ARRAY_SIZE(headset_devices),
	.headset_devices	= headset_devices,
	.headset_config_num	= ARRAY_SIZE(htc_headset_mgr_config),
	.headset_config		= htc_headset_mgr_config,
	.headset_init		= headset_init,
	.headset_power		= headset_power,
	.uart_tx_gpo		= uart_tx_gpo,
	.uart_lv_shift_en	= uart_lv_shift_en,
};

static struct platform_device htc_headset_mgr = {
	.name	= "HTC_HEADSET_MGR",
	.id	= -1,
	.dev	= {
		.platform_data	= &htc_headset_mgr_data,
	},
};

static int __init flounder_headset_init(void)
{
	pr_info("[HS]%s Headset device register enter\n", __func__);
	platform_device_register(&htc_headset_mgr);
	return 0;
}

static struct device *gps_dev;
static struct class *gps_class;

extern int tegra_get_hw_rev(void);


#define PRJ_F	302
static int __init flounder_gps_init(void)
{

	int ret;
	int gps_onoff;
	int product_id;

	pr_info("[GPS]%s init gps onoff\n", __func__);
	of_property_read_u32(
		of_find_node_by_path("/chosen/board_info"),
		"pid",
		&product_id);

	if (product_id == PRJ_F && flounder_get_hw_revision() <= FLOUNDER_REV_EVT1_1  ){
		gps_onoff = TEGRA_GPIO_PH5; // XB
	} else {
		gps_onoff = TEGRA_GPIO_PB4; // XC
	}

	gps_class = class_create(THIS_MODULE, "gps");
	if (IS_ERR(gps_class)){
		pr_err("[GPS] %s: gps class create fail \n", __func__);
		return PTR_ERR(gps_class);
	}

	gps_dev = device_create(gps_class, NULL, 0, NULL, "bcm47521");
	if (IS_ERR(gps_dev)){
		pr_err("[GPS] %s: gps device create fail \n", __func__);
		return PTR_ERR(gps_dev);
	}

	ret = gpio_request(gps_onoff, "gps_onoff");
	if (ret < 0){
		pr_err("[GPS] %s: gpio_request failed for gpio %s\n",
			__func__, "gps_onoff");
	}

	gpio_direction_output(gps_onoff, 0);
	gpio_export (gps_onoff, 1);
	gpio_export_link(gps_dev,"gps_onoff", gps_onoff);

	return 0;
}
#undef PRJ_F

static void __init sysedp_init(void)
{
	flounder_new_sysedp_init();
}

static void __init edp_init(void)
{
	flounder_edp_init();
}

static void __init sysedp_dynamic_capping_init(void)
{
	flounder_sysedp_dynamic_capping_init();
}

static void __init tegra_flounder_early_init(void)
{
	sysedp_init();
	tegra_clk_init_from_table(flounder_clk_init_table);
	tegra_clk_verify_parents();
	tegra_soc_device_init("flounder");
}

static struct tegra_dtv_platform_data flounder_dtv_pdata = {
	.dma_req_selector = 11,
};

static void __init flounder_dtv_init(void)
{
	tegra_dtv_device.dev.platform_data = &flounder_dtv_pdata;
	platform_device_register(&tegra_dtv_device);
}

static struct tegra_io_dpd pexbias_io = {
	.name			= "PEX_BIAS",
	.io_dpd_reg_index	= 0,
	.io_dpd_bit		= 4,
};
static struct tegra_io_dpd pexclk1_io = {
	.name			= "PEX_CLK1",
	.io_dpd_reg_index	= 0,
	.io_dpd_bit		= 5,
};
static struct tegra_io_dpd pexclk2_io = {
	.name			= "PEX_CLK2",
	.io_dpd_reg_index	= 0,
	.io_dpd_bit		= 6,
};

static void __init tegra_flounder_late_init(void)
{
	platform_device_register(&tegra124_pinctrl_device);
	flounder_pinmux_init();

	flounder_display_init();
	flounder_uart_init();
	flounder_usb_init();
	if(is_mdm_modem())
		flounder_mdm_9k_init();
	flounder_xusb_init();
	flounder_i2c_init();
	flounder_spi_init();
	flounder_audio_init();
	platform_add_devices(flounder_devices, ARRAY_SIZE(flounder_devices));
	tegra_io_dpd_init();
	flounder_sdhci_init();
	flounder_regulator_init();
	flounder_dtv_init();
	flounder_suspend_init();

	flounder_emc_init();
	flounder_edp_init();
	isomgr_init();
	flounder_touch_init();
	flounder_headset_init();
	flounder_panel_init();
	flounder_kbc_init();
	flounder_gps_init();

	/* put PEX pads into DPD mode to save additional power */
	tegra_io_dpd_enable(&pexbias_io);
	tegra_io_dpd_enable(&pexclk1_io);
	tegra_io_dpd_enable(&pexclk2_io);

#ifdef CONFIG_TEGRA_WDT_RECOVERY
	tegra_wdt_recovery_init();
#endif

	flounder_sensors_init();

	flounder_soctherm_init();

	flounder_setup_bluedroid_pm();
	tegra_register_fuse();
	sysedp_dynamic_capping_init();
}

static void __init tegra_flounder_init_early(void)
{
	flounder_rail_alignment_init();
	tegra12x_init_early();
}

static void __init tegra_flounder_dt_init(void)
{
	tegra_flounder_early_init();
#ifdef CONFIG_USE_OF
	flounder_camera_auxdata(flounder_auxdata_lookup);
	of_platform_populate(NULL,
		of_default_bus_match_table, flounder_auxdata_lookup,
		&platform_bus);
#endif

	tegra_flounder_late_init();
        bt_export_bd_address();
}

static void __init tegra_flounder_reserve(void)
{
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM) || \
		defined(CONFIG_TEGRA_NO_CARVEOUT)
	/* 1536*2048*4*2 = 25165824 bytes */
	tegra_reserve(0, SZ_16M + SZ_8M, SZ_16M);
#else
	tegra_reserve(SZ_1G, SZ_16M + SZ_8M, SZ_4M);
#endif
}

static const char * const flounder_dt_board_compat[] = {
	"google,flounder",
	"google,flounder_lte",
	"google,flounder64",
	"google,flounder64_lte",
	NULL
};

DT_MACHINE_START(FLOUNDER, "flounder")
	.atag_offset	= 0x100,
	.smp		= smp_ops(tegra_smp_ops),
	.map_io		= tegra_map_common_io,
	.reserve	= tegra_flounder_reserve,
	.init_early	= tegra_flounder_init_early,
	.init_irq	= irqchip_init,
	.init_time	= clocksource_of_init,
	.init_machine	= tegra_flounder_dt_init,
	.restart	= tegra_assert_system_reset,
	.dt_compat	= flounder_dt_board_compat,
	.init_late      = tegra_init_late
MACHINE_END

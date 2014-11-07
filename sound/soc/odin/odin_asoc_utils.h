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

#include <linux/device.h>
#include <sound/soc.h>

#define AUD_IO_IN    0
#define AUD_IO_OUT   1
#define AUD_IO_IP0   2
#define AUD_IO_IP1   3

#define AUD_IO_FUNC0    0
#define AUD_IO_FUNC1    1
#define AUD_IO_FUNC2    2
#define AUD_IO_FUNC3    3

#define OUT_L    0
#define OUT_H    1

enum {
	             /* func2=ip0    */ /* func3=ip1   */
	AUD_GPIO_00, /* MCLK_I2S0    */ /*             */
	AUD_GPIO_01, /* SCLK_I2S0    */ /*             */
	AUD_GPIO_02, /* WS_I2S0      */ /*             */
	AUD_GPIO_03, /* SDI_I2S0     */ /*             */
	AUD_GPIO_04, /* SDO_I2S0     */ /*             */
	AUD_GPIO_05, /* MCLK_I2S1    */ /*             */
	AUD_GPIO_06, /* SCLK_I2S1    */ /*             */
	AUD_GPIO_07, /* WS_I2S1      */ /*             */
	AUD_GPIO_08, /* SDI_I2S1     */ /*             */
	AUD_GPIO_09, /* SDO_I2S1     */ /*             */
	AUD_GPIO_10, /* MCLK_I2S2    */ /*             */
	AUD_GPIO_11, /* SCLK_I2S2    */ /*             */
	AUD_GPIO_12, /* WS_I2S2      */ /*             */
	AUD_GPIO_13, /* SDI_I2S2     */ /*             */
	AUD_GPIO_14, /* SDO_I2S2     */ /*             */
	AUD_GPIO_15, /* MCLK_I2S3    */ /* MCLK_MI2S   */
	AUD_GPIO_16, /* SCLK_I2S3    */ /* SCLK_MI2S   */
	AUD_GPIO_17, /* WS_I2S3      */ /* WS_MI2S     */
	AUD_GPIO_18, /* SDI_I2S3     */ /* SDI0_MI2S   */
	AUD_GPIO_19, /* SDO_I2S3     */ /* SDI1_MI2S   */
	AUD_GPIO_20, /* MCLK_I2S4    */ /* SDI2_MI2S   */
	AUD_GPIO_21, /* SCLK_I2S4    */ /* SDO0_MI2S   */
	AUD_GPIO_22, /* WS_I2S4      */ /* SDO1_MI2S   */
	AUD_GPIO_23, /* SDI_I2S4     */ /* SDO2_MI2S   */
	AUD_GPIO_24, /* SDO_I2S4     */ /* SDO3_MI2S   */
	AUD_GPIO_25, /* SCL_I2C0     */ /*             */
	AUD_GPIO_26, /* SDA_I2C0     */ /*             */
	AUD_GPIO_27, /* SCL_I2C1     */ /*             */
	AUD_GPIO_28, /* SDA_I2C1     */ /*             */
	AUD_GPIO_29, /* CLK_SB       */ /*             */
	AUD_GPIO_30, /* DATA_SB      */ /*             */
	AUD_GPIO_31, /* AUD_TCK      */ /*             */
	AUD_GPIO_32, /* AUD_NTRST    */ /* SCK_SPI     */
	AUD_GPIO_33, /* AUD_TDI      */ /* CS_SPI      */
	AUD_GPIO_34, /* AUD_TDO      */ /* MOSI_SPI    */
	AUD_GPIO_35, /* AUD_TMS      */ /* MISO_SPI    */
	AUD_GPIO_36, /* AUD_DSP_RSTN */ /* CS1_out_SPI */
	AUD_GPIO_37, /* SPDIF        */ /* SDI3_MI2S   */
	AUD_GPIO_38, /* GPIO input   */ /* Aud_PLL_OUT */
	AUD_GPIO_39, /* GPIO input   */ /* GPIO output */
	AUD_GPIO_40, /* GPIO input   */ /* GPIO output */
	AUD_GPIO_41, /* GPIO input   */ /* GPIO output */
	AUD_GPIO_42, /* GPIO input   */ /* GPIO output */
	AUD_GPIO_43, /* GPIO input   */ /* GPIO output */
};

/* Next functions must be called by machine driver in ASoC */
int odin_get_aud_io_func(int base_num, int aud_gpio, int *value);
int odin_set_aud_io_func(int base_num, int aud_gpio, int func, int value);
int odin_of_get_base_bank_num(struct device *dev, int *aud_gpio_base);
int odin_of_dai_property_get(struct device *dev, struct snd_soc_card *card);
#if defined(CONFIG_DEBUG_FS)
void odin_aud_io_debugfs_init(int aud_gpio_base);
void odin_aud_io_debugfs_exit(void);
#else
void odin_aud_io_debugfs_init(int aud_gpio_base)
{
	return;
}
void odin_aud_io_debugfs_exit(void)
{
	return;
}
#endif

/**
 * Some ALSA internal helper functions
 */
int snd_interval_refine_set(struct snd_interval *i, unsigned int val);
int _snd_pcm_hw_param_set(struct snd_pcm_hw_params *params,
				 snd_pcm_hw_param_t var, unsigned int val,
				 int dir);


/*
 * Definitions for tfa9895 speaker amp chip.
 */
#ifndef TFA9895_H
#define TFA9895_H

#include <linux/ioctl.h>

#define TFA9895_I2C_NAME "tfa9895"
#define TFA9895L_I2C_NAME "tfa9895l"
struct tfa9895_platform_data {
	uint32_t tfa9895_power_enable;
};

int set_tfa9895_spkamp(int en, int dsp_mode);
int set_tfa9895l_spkamp(int en, int dsp_mode);
int tfa9895_l_write(char *txdata, int length);
int tfa9895_l_read(char *rxdata, int length);
#endif

/*
 * Definitions for tfa9895 speaker amp chip.
 */
#ifndef TFA9895_H
#define TFA9895_H

#include <linux/ioctl.h>

#define TFA9895_I2C_NAME "tfa9895"
#define TFA9895L_I2C_NAME "tfa9895l"

#define TFA9895_IOCTL_MAGIC 'a'
#define TFA9895_WRITE_CONFIG_NR     0x01
#define TFA9895_READ_CONFIG_NR      0x02
#define TFA9895_ENABLE_DSP_NR       0x03
#define TFA9895_WRITE_L_CONFIG_NR   0x04
#define TFA9895_READ_L_CONFIG_NR    0x05

#define TFA9895_WRITE_CONFIG(size)  _IOW(TFA9895_IOCTL_MAGIC, TFA9895_WRITE_CONFIG_NR, (size))
#define TFA9895_READ_CONFIG(size)   _IOWR(TFA9895_IOCTL_MAGIC, TFA9895_READ_CONFIG_NR, (size))
#define TFA9895_ENABLE_DSP(size)    _IOW(TFA9895_IOCTL_MAGIC, TFA9895_ENABLE_DSP_NR, (size))
#define TFA9895_WRITE_L_CONFIG(size) _IOW(TFA9895_IOCTL_MAGIC, TFA9895_WRITE_L_CONFIG_NR , (size))
#define TFA9895_READ_L_CONFIG(size) _IOWR(TFA9895_IOCTL_MAGIC, TFA9895_READ_L_CONFIG_NR, (size))

struct tfa9895_platform_data {
	uint32_t tfa9895_power_enable;
};

struct tfa9895_i2c_buffer {
	int size;
	unsigned char buffer[255];
};

int set_tfa9895_spkamp(int en, int dsp_mode);
int set_tfa9895l_spkamp(int en, int dsp_mode);
int tfa9895_l_write(char *txdata, int length);
int tfa9895_l_read(char *rxdata, int length);
int tfa9895_disable(bool disable);
int tfa9895l_disable(bool disable);
#endif

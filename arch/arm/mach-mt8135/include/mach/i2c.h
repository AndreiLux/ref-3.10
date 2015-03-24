#ifndef __ARCH_I2C_H_
#define __ARCH_I2C_H_

#include <mach/mt_platform.h>

/* //36000	kHz for wrapper I2C work frequency  6397 should be 24000K*/
#define I2C_CLK_WRAPPER_RATE	24000
#if (defined(CONFIG_MT8135_FPGA))
#define	CONFIG_MT_I2C_FPGA_ENABLE
#endif

#if (defined(CONFIG_MT_I2C_FPGA_ENABLE))
#define I2C_CLK_RATE	12000	/* kHz for FPGA I2C work frequency */
#else
#define I2C_CLK_RATE	I2C_CLK_WRAPPER_RATE
#endif

struct mt_i2c_data {
	u16 pdn;	/* MTK clock id */
	u16 pmic_ch;	/* pmic channel */
	u16 speed;	/* bus speed in kHz */
	u16 delay_len;	/* idle cycles (min 2) */
	u32 flags;
	int (*enable_clk)(struct mt_i2c_data *, bool);
	int (*enable_dma_clk)(struct mt_i2c_data *, bool);
	u32 (*get_func_clk)(struct mt_i2c_data *);
	u16 *need_wrrd;
};
#endif /* __ARCH_I2C_H_ */

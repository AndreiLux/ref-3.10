#ifndef __ARCH_MT_DDP_BLS_H
#define __ARCH_MT_DDP_BLS_H

struct PWM_config {
	int clock_source;
	int div;
	int low_duration;
	int High_duration;
	BOOL pmic_pad;
};

/*
 * MTK back-light driver platform data
 */

struct mt_bls_data {
	struct PWM_config pwm_config;
};

#endif /* __ARCH_MT_DDP_BLS_H */

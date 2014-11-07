/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __MAX77665_VIBRATOR_H__
#define __MAX77665_VIBRATOR_H__

#define MAX77665_SLAVE_ADDR	0x90

enum max77665_vib_freq
{
	MAX8957_VIB_0_75KHZ,
	MAX8957_VIB_1_50KHZ,
	MAX8957_VIB_2_30KHZ,
	MAX8957_VIB_3_00KHZ,
	MAX8957_VIB_4_50KHZ,
	MAX8957_VIB_6_70KHZ,
	MAX8957_VIB_11_70KHZ,
	MAX8957_VIB_23_40KHZ,
};

enum max77665_vib_htype
{
	INPUT_MPWM,
	INPUT_IPWM,
};

enum max77665_motor_type
{
	MODE_ERM,
	MODE_LRA,
};

enum max77665_scf
{
	SCF_1CLOCK,
	SCF_2CLOCKS,
	SCF_3CLOCKS,
	SCF_4CLOCKS,
	SCF_5CLOCKS,
	SCF_6CLOCKS,
	SCF_7CLOCKS,
	SCF_8CLOCKS,
};

struct max77665_vib_platform_data
{
	enum max77665_vib_htype input;
	enum max77665_motor_type motor;
	enum max77665_scf        scf;        // scale frequence parameter
	u8 pwmdc[4];   // PWM Duty Cycle Pattern 0 ~ 3 (0~63) (1/64(1.56%) ~ 64/64(100%))
	u8 sigp[4];    // Signal Period Pattern 0 ~3 (0~255) ((sigp + 1) * 32 PWM pulses)
	u8 sigdc[4];   // Signal Duty Cycle Pattern 0 ~ 3 (0~15) (1/16(6.25%) ~ 16/16(100%))
	u8 cycle[4];   // Number of Cycle Pattern 0~ 3 (0~15) (0~15 cycles)

	int initial_vibrate_ms;
	int max_timeout_ms;
	int level_mV;
	char *regulator_name;
};

#endif /* __MAX77665_VIBRATOR_H__ */

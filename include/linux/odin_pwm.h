#ifndef _ODIN_PWM_H
#define _ODIN_PWM_H

#define ODIN_NR_PWMS 2
#define DIVIDER		0x00
#define PRESCALE0	0x04
#define PRESCALE1	0x08
#define CONTROL		0x0C		/* Timer Control Register */
#define DZ_LEN0		0x10		/* PWM Timer Dead-zone Control0 */
#define DZ_LEN1		0x14		/* PWM Timer Dead-zone Control1 */
#define TCMPB0		0x18		/* Timer 0 compare buffer register */
#define TCNTB0		0x1C
#define TCNTO0		0x20
#define TGCNT0		0x24
#define TCMPB1		0x28
#define TCNTB1		0x2C
#define TCNTO1		0x30
#define TGCNT1		0x34
#define NS_IN_HZ (1000000000UL)

struct odin_chip {
	struct pwm_chip chip;
	void __iomem *base;
	struct clk *clk;
	unsigned int  duty_ns[2];
	spinlock_t lock;
};

#define to_odin_chip(chip)	container_of(chip, struct odin_chip, chip)

#endif

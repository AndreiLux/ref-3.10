#include <asm/mach/time.h>
#include <mach/mt_timer.h>
#include <linux/version.h>

extern struct mt_clock mt8135_gpt;
extern int generic_timer_register(void);


struct mt_clock *mt8135_clocks[] = {
	&mt8135_gpt,
};

struct mt_clock_init __initdata *mt8135_clocks_init[] = {
	&mt8135_gpt_init,
};

void __init mt8135_timer_init(void)
{
	int i;
	struct mt_clock *clock;
	struct mt_clock_init *clock_init;
	int err;

	for (i = 0; i < ARRAY_SIZE(mt8135_clocks); i++) {
		clock = mt8135_clocks[i];
		clock_init = mt8135_clocks_init[i];
		clock_init->init_func();

		if (clock->clocksource.name) {
			err = clocksource_register(&(clock->clocksource));
			if (err) {
				pr_err("mt8135_timer_init: clocksource_register failed for %s\n",
				       clock->clocksource.name);
			}
		}

		err = setup_irq(clock->irq.irq, &(clock->irq));
		if (err) {
			pr_err("mt8135_timer_init: setup_irq failed for %s\n", clock->irq.name);
		}

		if (clock->clockevent.name)
			clockevents_register_device(&(clock->clockevent));
	}

#ifndef CONFIG_MT8135_FPGA
	err = generic_timer_register();
	if (err) {
		pr_err("generic_timer_register failed, err=%d\n", err);
	}
	/* pr_info("fwq no generic timer"); */
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
struct sys_timer mt8135_timer = {
	.init = mt8135_timer_init,
};
#endif

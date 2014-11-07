/*
 * Copyright 2010-2011 Calxeda, Inc.
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
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/reboot.h>
#include <linux/smp.h>

#include <asm/cacheflush.h>
#include <asm/smp_plat.h>
#include <asm/hardware/arm_timer.h>
#include <asm/hardware/timer-sp.h>
#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include "core.h"
#include "pm.h"
#if defined(CONFIG_SMP)
extern void __init odin_smp_map_io(void);
#endif
extern struct smp_operations __initdata odin_smp_ops;

static struct map_desc odin_io_desc[] __initdata = {
	{
		.virtual    = ODIN_DEBUG_LLUART_VIRT_BASE,
		.pfn        = __phys_to_pfn(ODIN_DEBUG_LLUART_PHYS_BASE),
		.length     = SZ_4K,
		.type       = MT_DEVICE,
	},
	{
		.virtual    = ODIN_SCR_VIRT_BASE,
		.pfn        = __phys_to_pfn(ODIN_SCR_PHYS_BASE),
		.length     = SZ_4K,
		.type       = MT_DEVICE,
	},
};

static void __init odin_map_io(void)
{
	iotable_init(odin_io_desc, ARRAY_SIZE(odin_io_desc));
#if defined(CONFIG_SMP)
	odin_smp_map_io();
#endif
}

const static struct of_device_id irq_match[] = {
	{ .compatible = "arm,cortex-a15-gic", .data = gic_of_init, },
	{}
};

static void __init odin_init_irq(void)
{
	of_irq_init(irq_match);
}

static struct clk_lookup lookup = {
	.dev_id = "sp804",
	.con_id = NULL,
};

static const struct of_device_id clk_match[] = {
        { .compatible = "fixed-clock", .data = of_fixed_clk_setup, },
        {},
};

static void __init odin_clocks_init(void)
{
        of_clk_init(clk_match);
}

static void __init odin_timer_init(void)
{
	int irq;
	struct device_node *np;
	void __iomem *timer_base;

	/* Map system registers */
	np = of_find_compatible_node(NULL, NULL, "arm,sp804");
	timer_base = of_iomap(np, 0);
	WARN_ON(!timer_base);
	irq = irq_of_parse_and_map(np, 0);

	odin_clocks_init();
	lookup.clk = of_clk_get(np, 0);
	clkdev_add(&lookup);

	sp804_clocksource_and_sched_clock_init(timer_base + 0x20, "timer1");
	sp804_clockevents_init(timer_base, irq, "timer0");
}

static struct sys_timer odin_timer = {
	.init = odin_timer_init,
};

static void odin_power_off(void)
{
	while (1)
		cpu_do_idle();
}

static void __init odin_init(void)
{
	pm_power_off = odin_power_off;
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
}

static const char *odin_match[] __initconst = {
	"lge,odin-fpga",
	NULL,
};

extern struct smp_operations bL_smp_ops;

DT_MACHINE_START(ODIN, "Odin FPGA")
	.dt_compat	= odin_match,
	.smp        = smp_ops(odin_smp_ops),
	.map_io		= odin_map_io,
	.init_irq	= odin_init_irq,
	.timer		= &odin_timer,
	.handle_irq	= gic_handle_irq,
	.init_machine	= odin_init,
	.restart	= odin_restart,
MACHINE_END

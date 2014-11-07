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
#include <linux/irqchip/arm-gic.h>
#include <linux/irqchip.h>
#include <linux/platform_device.h>
#include <linux/odin_pmic.h>
#include <linux/clocksource.h>
#ifdef CONFIG_ODIN_THERMAL_MONITOR
#include <linux/odin_thermal.h>
#include <linux/odin_thermal_monitor.h>
#endif
#include <linux/odin_pd.h>
#include <linux/board_lge.h>

/*
               
                              
                                    
 */
#if 0//def CONFIG_PSTORE_RAM
#include <linux/pstore_ram.h>
#include <linux/memblock.h>
#include <asm/setup.h>
#endif
/*              */

#include <asm/cacheflush.h>
#include <asm/smp_plat.h>
#include <asm/hardware/arm_timer.h>
#include <asm/hardware/timer-sp.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/system_info.h>

#include "core.h"
#include "pm.h"
#if defined(CONFIG_SMP)
extern void __init odin_smp_map_io(void);
#endif
extern struct smp_operations __initdata odin_smp_ops;

extern void odin_clk_init();

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

static struct platform_device generic_bl_platdev = {
	.name = "arm-bL-cpufreq-dt",
};

static const struct of_device_id clk_match[] = {
	{ .compatible = "fixed-clock", .data = of_fixed_clk_setup, },
	{},
};

static void __init odin_clocks_init(void)
{
	of_clk_init(clk_match);
}

static void odin_power_off(void)
{
	odin_set_pwr_shutdown();
}

/*
               
                              
                                    
 */
#if 0//def CONFIG_PSTORE_RAM
static struct ramoops_platform_data ramoops_data = {
//	.mem_address    = 0xDCE00000~0xDD000000,
//	.mem_address select in ramoops_init
	.mem_address	= 0xB7900000,
	.mem_size       = 0x100000,		// 1MB
	.console_size   = 0x80000,		// 512K
	.record_size    = 2048,
	.dump_oops      = 1,
};

static struct platform_device ramoops_dev = {
	.name = "ramoops",
	.dev = {
		.platform_data = &ramoops_data,
	},
};

extern struct meminfo meminfo;

static void odin_ramoops_init(void)
{
	int ret;
	struct membank * bank = &meminfo.bank[1];

	printk(KERN_INFO "%s: memblock_reserve for (%s)\n", __func__, ramoops_dev.name);

//	ramoops_data.mem_address = (phys_addr_t)(bank->start + bank->size - ramoops_data.mem_size);
	memblock_reserve(ramoops_data.mem_address, ramoops_data.mem_size);
	printk(KERN_INFO "%s: reverve memmory addr 0x%x, size %d\n",
			__func__, ramoops_data.mem_address, ramoops_data.mem_size);
	ret = platform_device_register(&ramoops_dev);
	if (ret) {
		memblock_free(ramoops_data.mem_address, ramoops_data.mem_size);
		printk(KERN_ERR "unable to register platform device : %s\n", ramoops_dev.name);
		return ret;
	}
}
#endif
/*              */

#if defined(CONFIG_BCMDHD)
extern void init_bcm_wifi(void);
#endif

static void __init odin_timer_init(void)
{
	odin_clk_init();

	clocksource_of_init();
}

#ifdef CONFIG_IMC_DRV
extern void mdm_platform_dev_init(void);
#endif

#ifdef CONFIG_LGE_BOOT_TIME_CHECK
extern void lge_add_boot_time_checker(void);
#endif

#ifdef CONFIG_ODIN_TEE
extern void odin_sram_map(void);
#endif
#if defined(CONFIG_PRE_SELF_DIAGNOSIS)
int pre_selfd_set_values(int kcal_r, int kcal_g, int kcal_b)
{
	return 0;
}

static int pre_selfd_get_values(int *kcal_r, int *kcal_g, int *kcal_b)
{
	return 0;
}

static struct pre_selfd_platform_data pre_selfd_pdata = {
	.set_values = pre_selfd_set_values,
	.get_values = pre_selfd_get_values,
};


static struct platform_device pre_selfd_platrom_device = {
	.name   = "pre_selfd_ctrl",
	.dev = {
		.platform_data = &pre_selfd_pdata,
	}
};

void __init lge_add_pre_selfd_devices(void)
{
	pr_info(" PRE_SELFD_DEBUG : %s\n", __func__);
	platform_device_register(&pre_selfd_platrom_device);
}
#endif /* CONFIG_PRE_SELF_DIAGNOSIS */

static void __init odin_init(void)
{
	int board_rev = odin_pmic_revision_get();

/*
               
                                                            
                                      
                                                                     
                                                          
                                  
   */
#ifdef CONFIG_MACH_ODIN_LIGER
	if(board_rev == 0xF){
		board_rev = 0xE;
		printk("Temporal Board Revision Matching to MC & SIC Board \n");
	}

	system_rev = ((0x03 << 16) & 0xf0000) | (CONFIG_ARCH_ODIN_REVISION << 8)
		| (board_rev & 0xff);
#else
	system_rev = (board_rev & 0xf0000) | (CONFIG_ARCH_ODIN_REVISION << 8)
		| (board_rev & 0xff);
#endif

	pm_power_off = odin_power_off;
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
	platform_device_register(&generic_bl_platdev);
/*
               
                              
                                    
 */
#if 0//def CONFIG_PSTORE_RAM
	odin_ramoops_init();
#endif
/*              */

#ifdef CONFIG_IMC_DRV
	mdm_platform_dev_init();
#endif

#ifdef CONFIG_ODIN_THERMAL_MONITOR
	odin_tsens_driver_arch_init();
	odin_tmon_driver_init();
#endif
#ifdef CONFIG_ODIN_TEE
	odin_sram_map();
#endif

#ifdef CONFIG_LGE_BOOT_TIME_CHECK
	lge_add_boot_time_checker();
#endif

#if defined(CONFIG_PRE_SELF_DIAGNOSIS)
	lge_add_pre_selfd_devices();
#endif
}

static const char *odin_match[] __initconst = {
	"lge,odin",
	NULL,
};

extern void odin_timer_init(void);
extern void odin_smp_init(void);
extern void odin_irq_init(void);

DT_MACHINE_START(odin, "Odin")
	.dt_compat	= odin_match,
	.smp_init   = smp_init_ops(odin_smp_init),
	.map_io		= odin_map_io,
	.init_irq	= odin_irq_init,
	.init_time	= odin_timer_init,
	.init_machine	= odin_init,
	.restart	= odin_restart,
MACHINE_END

/*
 * Spin Table SMP initialisation
 *
 * Copyright (C) 2013 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/init.h>
#include <linux/of.h>
#include <linux/smp.h>

#include <asm/cacheflush.h>

#include <linux/io.h>
#include <linux/of_address.h>


static phys_addr_t cpu_release_addr[NR_CPUS];


int __init smp_spin_table_init_cpu(struct device_node *dn, int cpu)
{
	/*
	 * Determine the address from which the CPU is polling.
	 */
	if (of_property_read_u64(dn, "cpu-release-addr",
				 &cpu_release_addr[cpu])) {
		pr_err("CPU %d: missing or invalid cpu-release-addr property\n",
		       cpu);

		return -1;
	}

	return 0;
}

/*MTK only*/
#define CCI400_SI4_BASE                                 0x5000
#define CCI400_SI4_SNOOP_CONTROL           CCI400_SI4_BASE
#define DVM_MSG_REQ                                     (1U << 1)
#define SNOOP_REQ                                       (1U << 0)
#define CCI400_STATUS                                   0x000C
#define CHANGE_PENDING                                  (1U << 0)

int __init smp_spin_table_prepare_cpu(int cpu)
{
	void **release_addr;

    struct device_node *node;
    void __iomem *cci400_base;

	if (!cpu_release_addr[cpu])
		return -ENODEV;

    /*MTK only. Setup coherence interface*/
    node = of_find_compatible_node(NULL, NULL, "mediatek,CCI400");
    if(node)
    {
        cci400_base = of_iomap(node, 0);
                
        printk(KERN_EMERG "1.CCI400_SI4_SNOOP_CONTROL:0x%x, 0x%08x\n", cci400_base + CCI400_SI4_SNOOP_CONTROL, readl(cci400_base + CCI400_SI4_SNOOP_CONTROL));
        /* Enable snoop requests and DVM message requests*/
        writel(readl(cci400_base + CCI400_SI4_SNOOP_CONTROL) | (SNOOP_REQ | DVM_MSG_REQ), cci400_base + CCI400_SI4_SNOOP_CONTROL);
        while (readl(cci400_base + CCI400_STATUS) & CHANGE_PENDING);
        printk(KERN_EMERG "2.CCI400_SI4_SNOOP_CONTROL:0x%x, 0x%08x\n", cci400_base + CCI400_SI4_SNOOP_CONTROL,readl(cci400_base + CCI400_SI4_SNOOP_CONTROL));
    }

	release_addr = __va(cpu_release_addr[cpu]);
	release_addr[0] = (void *)__pa(secondary_holding_pen);
	__flush_dcache_area(release_addr, sizeof(release_addr[0]));

	/*
	 * Send an event to wake up the secondary CPU.
	 */
	sev();

	return 0;
}

const struct smp_enable_ops smp_spin_table_ops __initconst = {
	.name		= "spin-table",
	.init_cpu 	= smp_spin_table_init_cpu,
	.prepare_cpu	= smp_spin_table_prepare_cpu,
};

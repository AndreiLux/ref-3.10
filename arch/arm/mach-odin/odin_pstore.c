/*
 * arch/arm/mach-odin/odin_pstore.c
 *
 * ODIN Pstore device
 *
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/pstore_ram.h>

#define ODIN_PSTORE_CONSOLE_SIZE   0x00100000
#define ODIN_PSTORE_RECORD_SIZE    (1 << CONFIG_LOG_BUF_SHIFT)
#define ODIN_PSTORE_FTRACE_SIZE    0x002F8000

static int __init odin_pstore_init(void)
{
	struct device_node *np;
	unsigned long mem_base, mem_size, console_size;
	struct ramoops_platform_data pstore_data;

	memset(&pstore_data, 0, sizeof(pstore_data));

	np = of_find_compatible_node(NULL, NULL, "lge,pstore");
	if (!np)
		return -ENODEV;

	if(of_property_read_u32(np, "base", &pstore_data.mem_address))
		return -EINVAL;

	if(of_property_read_u32(np, "size", &pstore_data.mem_size))
		return -EINVAL;

	if(pstore_data.mem_size < ODIN_PSTORE_CONSOLE_SIZE)
		return -ENODEV;

	pstore_data.console_size = ODIN_PSTORE_CONSOLE_SIZE;
	pstore_data.record_size = ODIN_PSTORE_RECORD_SIZE;
	pstore_data.ftrace_size = ODIN_PSTORE_FTRACE_SIZE;
	pstore_data.dump_oops = 1;

	printk(KERN_INFO "odin_pstore: base=0x%x, size=0x%x, record_size=0x%x, ftrace_size=0x%x\n",
		       pstore_data.mem_address, pstore_data.mem_size,
		       pstore_data.record_size, pstore_data.ftrace_size);

	platform_device_register_data(NULL, "ramoops", 0, &pstore_data, sizeof(pstore_data));

	return 0;
}

module_init(odin_pstore_init);

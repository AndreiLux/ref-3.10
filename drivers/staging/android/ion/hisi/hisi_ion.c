/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/export.h>
#include <linux/err.h>
#include <linux/hisi_ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/rwsem.h>
#include <linux/uaccess.h>
#include "../ion_priv.h"

static struct ion_device *idev;
static int num_heaps;
static struct ion_heap **heaps;

struct ion_client *hisi_ion_client_create(const char *name)
{
	return ion_client_create(idev, name);
}

EXPORT_SYMBOL(hisi_ion_client_create);

static void free_pdata(const struct ion_platform_data *pdata)
{
	kfree(pdata);
}

static struct ion_platform_heap hisi_ion_heaps[] = {
	[ION_SYSTEM_HEAP_ID] = {
		.type = ION_HEAP_TYPE_SYSTEM,
		.id = ION_SYSTEM_HEAP_ID,
		.name = "system-heap",
	},
	[ION_SYSTEM_CONTIG_HEAP_ID] = {
		.type = ION_HEAP_TYPE_SYSTEM_CONTIG,
		.id = ION_SYSTEM_CONTIG_HEAP_ID,
		.name = "system-contig-heap",
	},
	[ION_GRALLOC_HEAP_ID] = {
		.type = ION_HEAP_TYPE_CARVEOUT,
		.id = ION_GRALLOC_HEAP_ID,
		.name = "carveout-heap",
		.base = 0,
		.size = 0,
	},
	[ION_DMA_HEAP_ID] = {
		.type = ION_HEAP_TYPE_DMA,
		.id = ION_DMA_HEAP_ID,
		.name = "ion-dma-heap",
	},
	[ION_DMA_POOL_HEAP_ID] = {
		.type = ION_HEAP_TYPE_DMA_POOL,
		.id = ION_DMA_POOL_HEAP_ID,
		.name = "ion-dma-pool-heap",
	},
	[ION_CPU_DRAW_HEAP_ID] = {
		.type = ION_HEAP_TYPE_CPUDRAW,
		.id = ION_CPU_DRAW_HEAP_ID,
		.name = "ion-cpu-draw-heap",
		.base = 0,
		.size = 0,
	},
};

#define MAX_ION_CPU_DRAW_HEAP_SIZE SZ_32M
extern unsigned long hisi_reserved_graphic_phymem;
extern unsigned long hisi_reserved_smmu_phymem;

void hisi_ionsysinfo(struct sysinfo *si)
{
	__kernel_ulong_t ion_free_memory = 0;
	struct ion_heap *heap = NULL;
	int i;
	for (i = 0; i < num_heaps; i++) {
		heap = heaps[i];
		if (heap->free_memory)
			ion_free_memory += heap->free_memory(heap);
	}
	si->freeram += ion_free_memory;
}

static void hisi_ion_get_heap(void)
{
	int i;
	size_t size;
	ion_phys_addr_t cpu_draw_heap_base, gralloc_heap_base;
	size_t cpu_draw_heap_size, gralloc_heap_size;

	if (hisi_reserved_smmu_phymem > hisi_reserved_graphic_phymem)
		size = hisi_reserved_smmu_phymem - hisi_reserved_graphic_phymem;
	else
		size = 0;

	if (size > MAX_ION_CPU_DRAW_HEAP_SIZE) {
		cpu_draw_heap_base = hisi_reserved_graphic_phymem;
		gralloc_heap_base = hisi_reserved_graphic_phymem + MAX_ION_CPU_DRAW_HEAP_SIZE;
		cpu_draw_heap_size = MAX_ION_CPU_DRAW_HEAP_SIZE;
		gralloc_heap_size = size - MAX_ION_CPU_DRAW_HEAP_SIZE;
	} else {
		cpu_draw_heap_base = hisi_reserved_graphic_phymem;
		gralloc_heap_base = 0;
		cpu_draw_heap_size = size;
		gralloc_heap_size = 0;
	}

	printk(KERN_INFO "[ION]cpu buf heap: base = 0x%lx, size = 0x%x; "
		"gralloc heap: base = 0x%lx, size = 0x%x\n",
		cpu_draw_heap_base, cpu_draw_heap_size,
		gralloc_heap_base, gralloc_heap_size);

	for (i = 0; i < ARRAY_SIZE(hisi_ion_heaps); i++) {
		if (hisi_ion_heaps[i].id == ION_GRALLOC_HEAP_ID) {
			hisi_ion_heaps[i].base = gralloc_heap_base;
			hisi_ion_heaps[i].size = gralloc_heap_size;
		}
		if (hisi_ion_heaps[i].id == ION_CPU_DRAW_HEAP_ID) {
			hisi_ion_heaps[i].base = cpu_draw_heap_base;
			hisi_ion_heaps[i].size = cpu_draw_heap_size;
		}
		printk(KERN_INFO "[ION]heap: %s, id: %d, type: %d, base: 0x%lx, size: 0x%x\n",
			hisi_ion_heaps[i].name, hisi_ion_heaps[i].id, hisi_ion_heaps[i].type,
			hisi_ion_heaps[i].base, hisi_ion_heaps[i].size);
	}
}

static long hisi_ion_custom_ioctl(struct ion_client *client,
				unsigned int cmd,
				unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case ION_HISI_CUSTOM_PHYS:
	{
		struct ion_phys_data data;
		struct ion_handle *handle;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			return -EFAULT;
		}

		handle = ion_import_dma_buf(client, data.fd_buffer);
		if (IS_ERR(handle))
		{
			ion_free(client, handle);
			return PTR_ERR(handle);
	        }
		ret = ion_phys(client, handle, &data.phys, &data.size);
		if (ret)
		{
			ion_free(client, handle);
			return ret;
	        }
		if (copy_to_user((void __user *)arg, &data, sizeof(data)))
		{
			ion_free(client, handle);
			return -EFAULT;
		}
		ion_free(client, handle);

		break;
	}
	default:
		return -ENOTTY;
	}

	return ret;
}

static int hisi_ion_probe(struct platform_device *pdev)
{
	struct ion_platform_data *pdata;
	int err = -1;
	int i;

	hisi_ion_get_heap();

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (IS_ERR_OR_NULL(pdata)) {
		err = PTR_ERR(pdata);
		goto out;
	}

	pdata->nr = ARRAY_SIZE(hisi_ion_heaps);
	pdata->heaps = hisi_ion_heaps;

	num_heaps = pdata->nr;
	dev_info(&pdev->dev, "num_heaps: %d\n", num_heaps);

	heaps = kcalloc(pdata->nr, sizeof(struct ion_heap *), GFP_KERNEL);

	if (!heaps) {
		err = -ENOMEM;
		free_pdata(pdata);
		goto out;
	}

	idev = ion_device_create(hisi_ion_custom_ioctl);
	if (IS_ERR_OR_NULL(idev)) {
		err = PTR_ERR(idev);
		goto freeheaps;
	}
	dev_info(&pdev->dev, "idev: %p\n", idev);

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];

		dev_info(&pdev->dev, "create heap %d "
			" ---> type: %d, id: %d, name: %s,"
			" base: 0x%lx, size: 0%x\n", i,
			heap_data->type, heap_data->id, heap_data->name,
			heap_data->base, heap_data->size);

		heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			heaps[i] = 0;
			continue;
		} else {
			if (heap_data->size)
				pr_info("ION heap %s created at %lx "
					"with size %x\n", heap_data->name,
							  heap_data->base,
							  heap_data->size);
			else
				pr_info("ION heap %s created\n",
							  heap_data->name);
		}

		ion_device_add_heap(idev, heaps[i]);
	}

	free_pdata(pdata);

	platform_set_drvdata(pdev, idev);
	return 0;

freeheaps:
	kfree(heaps);
	free_pdata(pdata);
out:
	return err;
}

static int hisi_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < num_heaps; i++)
		ion_heap_destroy(heaps[i]);
	if (idev)
		ion_device_destroy(idev);
	kfree(heaps);
	return 0;
}

static struct of_device_id hisi_ion_match_table[] = {
	{.compatible = "hisi,k3v2-ion"},
	{.compatible = "hisi,k3v3-ion"},
	{},
};


static struct platform_driver hisi_ion_driver = {
	.probe = hisi_ion_probe,
	.remove = hisi_ion_remove,
	.driver = {
		.name = "ion-hisi",
		.of_match_table = hisi_ion_match_table,
	},
};

static int __init hisi_ion_init(void)
{
	return platform_driver_register(&hisi_ion_driver);
}

static void __exit hisi_ion_exit(void)
{
	platform_driver_unregister(&hisi_ion_driver);
}

subsys_initcall(hisi_ion_init);
module_exit(hisi_ion_exit);

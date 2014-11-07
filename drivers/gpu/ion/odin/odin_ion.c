/*
 * drivers/gpu/ion/odin/odin_ion.c
 *
 * Copyright(c) 2011 by LG Electronics Inc.
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of.h>

#include <linux/ion.h>
#include <linux/iommu.h>
#include <linux/odin_iommu.h>


struct ion_device *idev;
int num_heaps;
struct ion_heap **heaps;

static struct ion_platform_data odin_ion_data = {
#ifndef CONFIG_ODIN_ION_I_TUNER
	/* .nr = 3, */
	.nr = 2,
#else
	/* .nr = 4, */
	.nr = 3,
#endif
	.heaps = {
		{
			.type 	= ION_HEAP_TYPE_CARVEOUT_SMMU,
			.id 	= ODIN_ION_CARVEOUT_FB,
			.name 	= "fb",
			.base 	= ODIN_ION_CARVEOUT_FB_BASE_PA,
			.size 	= ODIN_ION_CARVEOUT_FB_SIZE,
		},
		{
			.type	= ION_HEAP_TYPE_SYSTEM,
			.id 	= ODIN_ION_SYSTEM_HEAP1,
			.name	= "sys",
		},
		/*
		{
			.type 	= ION_HEAP_TYPE_CARVEOUT_SMMU,
			.id 	= ODIN_ION_CARVEOUT_AUD,
			.name 	= "memory for audio DSP",
			.base 	= ODIN_ION_CARVEOUT_AUD_BASE_PA,
			.size 	= ODIN_ION_CARVEOUT_AUD_SIZE,
		},
		*/
#ifdef CONFIG_ODIN_ION_I_TUNER
		{
			.type 	= ION_HEAP_TYPE_CARVEOUT_SMMU,
			.id 	= ODIN_ION_CARVEOUT_I_TUNER,
			.name 	= "memory for CSS i-tuner",
			.base 	= ODIN_ION_CARVEOUT_I_TUNER_BASE_PA,
			.size 	= ODIN_ION_CARVEOUT_I_TUNER_SIZE,
		},
#endif
	},
};

/* only for sysfs */
struct ion_heap * odin_ion_get_sys_heap( void )
{
	return heaps[ODIN_ION_SYSTEM_HEAP1];
}

static void odin_ion_prepare_cached_mem( void )
{

	struct ion_client *client;
	struct ion_handle *handle;
	int remind;


	if ( odin_has_dram_3gbyte()==true )
		remind = ODIN_3GDDR_ION_CACHE_SIZE/SZ_2M;
	else
		remind = ODIN_2GDDR_ION_CACHE_SIZE/SZ_2M;
	remind -= odin_ion_get_page_cnt(SZ_2M);

	client = odin_ion_client_create("startup_temp");
	for ( ;remind>0; remind--){
		handle = ion_alloc( client, SZ_2M, 0,						\
							(1<<ODIN_ION_SYSTEM_HEAP1),				\
							(ION_FLAG_OUT_BUFFER|ION_FLAG_DONT_INV), 0 );
		if ( IS_ERR(handle) ){
			printk("[II] fill pool fail\n");
			break;
		}
	}
	odin_ion_client_destroy(client);

	odin_ion_prepare_pool_protection(							\
							odin_ion_get_sys_heap_pool(SZ_2M));

	printk("[II] odin memory prepare finished\n");

}

int odin_ion_probe(struct platform_device *pdev)
{
	struct ion_platform_data *pdata = pdev->dev.platform_data;
	int err;
	int i;

	of_property_read_string( pdev->dev.of_node, "name", &(pdev->name) );
	of_property_read_u32( pdev->dev.of_node, "id", &(pdev->id) );

	platform_device_add_data( pdev, &odin_ion_data,
			( sizeof(int) + sizeof(struct ion_platform_heap)*odin_ion_data.nr ) );
	pdata = pdev->dev.platform_data;

	num_heaps = pdata->nr;

	heaps = kzalloc(sizeof(struct ion_heap *) * pdata->nr, GFP_KERNEL);

	idev = ion_device_create(NULL);
	if (IS_ERR_OR_NULL(idev)) {
		kfree(heaps);
		return PTR_ERR(idev);
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];

		heaps[i] = ion_heap_create(heap_data);

		if (IS_ERR_OR_NULL(heaps[i])) {
			err = PTR_ERR(heaps[i]);
			goto err;
		}
		ion_device_add_heap(idev, heaps[i]);
	}
	platform_set_drvdata(pdev, idev);

	odin_ion_prepare_cached_mem();

	return 0;

err:

	for (i = 0; i < num_heaps; i++) {
		if (heaps[i])
			ion_heap_destroy(heaps[i]);
	}

	kfree(heaps);
	return err;

}

int odin_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	ion_device_destroy(idev);

	for (i = 0; i < num_heaps; i++)
		ion_heap_destroy(heaps[i]);

	kfree(heaps);

	return 0;
}

struct ion_client *odin_ion_client_create(const char *name)
{
	return ion_client_create(idev, name);
}
EXPORT_SYMBOL(odin_ion_client_create);

void odin_ion_client_destroy( struct ion_client * client )
{
	ion_client_destroy( client );
}
EXPORT_SYMBOL(odin_ion_client_destroy);

static struct of_device_id odin_ion_smmu_heap_match[] = {
	{ .compatible = "google,ion_smmu_heap" },
	{},
};

static struct platform_driver ion_driver = {
	.probe = odin_ion_probe,
	.remove = odin_ion_remove,
	.driver = {
		.name = "ion_smmu_heap",
		.of_match_table = of_match_ptr( odin_ion_smmu_heap_match ),
	},
};

static int __init ion_init(void)
{
	return platform_driver_register(&ion_driver);
}

static void __exit ion_exit(void)
{
	platform_driver_unregister(&ion_driver);
}

module_init(ion_init);
module_exit(ion_exit);



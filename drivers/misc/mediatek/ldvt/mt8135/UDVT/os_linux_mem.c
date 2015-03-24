#include <linux/module.h>
#include <linux/slab.h>
#include "x_os.h"
#include "x_assert.h"

#define CONFIG_SYS_MEM_WASTE_WARNING 0
#define GFP_MEM     (GFP_KERNEL | __GFP_REPEAT  | __GFP_NOFAIL)



VOID *x_mem_alloc(SIZE_T z_size)
{

#if CONFIG_SYS_MEM_WASTE_WARNING
	if (z_size >= PAGE_SIZE) {
		size_t size = z_size;
		void *caller = __builtin_return_address(0);

		size_t z_waste = ((1 << get_order(size)) << PAGE_SHIFT) - size;
		if ((size + (512 * 1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
			printk("[%s] size = %d, waste(%d) > 512K, LR = %p\n", __func__, size,
			       z_waste, caller);
		else if ((size + (256 * 1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
			printk("[%s] size = %d, waste(%d) > 256K, LR = %p\n", __func__, size,
			       z_waste, caller);
		else if ((size + (128 * 1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
			printk("[%s] size = %d, waste(%d) > 128K, LR = %p\n", __func__, size,
			       z_waste, caller);
		else if ((size + (64 * 1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
			printk("[%s] size = %d, waste(%d) > 64K, LR = %p\n", __func__, size,
			       z_waste, caller);
		else if ((size + (32 * 1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
			printk("[%s] size = %d, waste(%d) > 32K, LR = %p\n", __func__, size,
			       z_waste, caller);
		else if ((size + (16 * 1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
			printk("[%s] size = %d, waste(%d) > 16K, LR = %p\n", __func__, size,
			       z_waste, caller);
		else if ((size + (8 * 1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
			printk("[%s] size = %d, waste(%d) > 8K, LR = %p\n", __func__, size,
			       z_waste, caller);
		else if ((size + (4 * 1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
			printk("[%s] size = %d, waste(%d) > 4K, LR = %p\n", __func__, size,
			       z_waste, caller);

	}
#endif

	return kmalloc(z_size, GFP_MEM);
}

VOID *x_mem_calloc(UINT32 ui4_num_element, SIZE_T z_size_element)
{
	return kcalloc(ui4_num_element, z_size_element, GFP_MEM);
}


VOID *x_mem_realloc(VOID *pv_mem_block, SIZE_T z_new_size)
{
	return krealloc(pv_mem_block, z_new_size, GFP_MEM);
}


VOID x_mem_free(VOID *pv_mem_block)
{
	kfree(pv_mem_block);
}


VOID *x_mem_ch2_alloc(SIZE_T z_size)
{
	return kmalloc(z_size, GFP_MEM);
}


VOID *x_mem_ch2_calloc(UINT32 ui4_num_element, SIZE_T z_size_element)
{
	return kcalloc(ui4_num_element, z_size_element, GFP_MEM);
}


VOID *x_mem_ch2_realloc(VOID *pv_mem_block, SIZE_T z_new_size)
{
	return krealloc(pv_mem_block, z_new_size, GFP_MEM);
}


INT32 x_mem_part_create(HANDLE_T *ph_part_hdl,
			const CHAR *ps_name, VOID *pv_addr, SIZE_T z_size, SIZE_T z_alloc_size)
{
	return OSR_OK;
}


INT32 x_mem_part_delete(HANDLE_T h_part_hdl)
{
	return OSR_OK;
}


INT32 x_mem_part_attach(HANDLE_T *ph_part_hdl, const CHAR *ps_name)
{
	return OSR_OK;
}


VOID *x_mem_part_alloc(HANDLE_T h_part_hdl, SIZE_T z_size)
{
	return kmalloc(z_size, GFP_MEM);
}


VOID *x_mem_part_calloc(HANDLE_T h_part_hdl, UINT32 ui4_num_element, SIZE_T z_size_element)
{
	return kcalloc(ui4_num_element, z_size_element, GFP_MEM);
}


VOID *x_mem_part_realloc(HANDLE_T h_part_hdl, VOID *pv_mem_block, SIZE_T z_new_size)
{
	return krealloc(pv_mem_block, z_new_size, GFP_MEM);
}
EXPORT_SYMBOL(x_mem_alloc);
EXPORT_SYMBOL(x_mem_calloc);
EXPORT_SYMBOL(x_mem_realloc);
EXPORT_SYMBOL(x_mem_free);
EXPORT_SYMBOL(x_mem_ch2_alloc);
EXPORT_SYMBOL(x_mem_ch2_calloc);
EXPORT_SYMBOL(x_mem_ch2_realloc);
EXPORT_SYMBOL(x_mem_part_create);
EXPORT_SYMBOL(x_mem_part_delete);
EXPORT_SYMBOL(x_mem_part_attach);
EXPORT_SYMBOL(x_mem_part_alloc);
EXPORT_SYMBOL(x_mem_part_calloc);
EXPORT_SYMBOL(x_mem_part_realloc);

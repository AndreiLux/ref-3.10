/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See th
 * GNU General Public License for more details.
 */
/*       
  
                                          
                                                                         
  
                                                                 
                
                    
                                 
  
                          
     
 */

/*-------------------------------------------------------------------
	Control Constants
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	File Inclusions
--------------------------------------------------------------------*/
#include "gfx_impl.h"
#include "gfx_cfg.h"
/* #include "base_device.h" */
/* #include "misc_util.h" */

/*-------------------------------------------------------------------
	Constant Definitions
--------------------------------------------------------------------*/
#define	LX_CHIP_L9		1
#define	LX_CHIP_ODIN	2

/*-------------------------------------------------------------------
	Macro Definitions
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Type Definitions
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	External Function Prototype Declarations
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	External Variables
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	global Variables
--------------------------------------------------------------------*/

/** GFX memory description table
 *
 *	if base value is zero, base address is dynamicalled assigned by devmem
 *	manager ( dynamic address )
 *	if base value is non zero, devmem manager doesn't modify its address
 *	( static address )
 *	@see gfx_initcfg
 */
LX_MEMCFG_T gfx_mem_desc_table[] =
{
	[0] = {.name = "gfx_surf",.base = 0x0,.size = 137<<20},
	[1] = {.name = "gfx_surf",.base = 0x33A00000,.size = 0x00100000 },
	[2] = {.name = "gfx_surf",.base = 0x0,.size = 0x08000000},
};
/* L9 GP */
/* L9 COSMO */
/* H13 = 128M default */

/** GFX H/W configuratin table
 *
 */
LX_GFX_REG_CFG_T	gfx_reg_cfg_desc_table[] =
{
	/* L9 */
    [0] = {
		.chip		= LX_CHIP_L9,
        .chip_name	= "GFX-L9",
        .reg_base_addr	= 0xc001d000, /* L9_GFX_BASE */
        .reg_size	= 0x200, 	/* real size is '0x1EC' */
        .irq_num	= 0x0,		/* L9_IRQ_GFX */
    },

	/* H13 */
    [1] = {
		.chip		= LX_CHIP_ODIN,
        .chip_name	= "ODIN-GFX0",
        .reg_base_addr	= 0x31300000, /* ODIN_GFX0_BASE, */
        .reg_size	= 0x200,	/* real size is '0x1EC' */
        .irq_num	= (32/*H13_IRQ_GIC_START*/ + 54),/* H13_IRQ_GPU_GFX */
    },
};

LX_GFX_REG_CFG_T *gpgfxregcfg = NULL;
LX_GFX_MEM_CFG_T gmemcfggfx;

/*-------------------------------------------------------------------
	Static Function Prototypes Declarations
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Static Variables
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Implementation Group
--------------------------------------------------------------------*/
void gfx_initcfg ( void )
{
	/* get chip configuration */
	memcpy( &g_gfx_cfg, g_gfx_hal.getcfg(), sizeof(LX_GFX_CFG_T));

	/*-------------------------------------------------------------------
	 * [H13] configuration
	--------------------------------------------------------------------*/
	gpgfxregcfg = &gfx_reg_cfg_desc_table[1];
	memcpy( &gmemcfggfx.surface, &gfx_mem_desc_table[2], sizeof(LX_MEMCFG_T));
}

/** @} */


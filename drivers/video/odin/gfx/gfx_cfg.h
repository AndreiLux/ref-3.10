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
 * MERCANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See th
 * GNU General Public License for more details.
 */

/*       
  
                                          
                                                                         
  
                                         
                
                    
                                 
  
                          
     
 */

#ifndef	_GFX_CFG_H_
#define	_GFX_CFG_H_

/*-------------------------------------------------------------------
	Control Constants
--------------------------------------------------------------------*/
#define	GFX_USE_NOISY_ERR_DEBUG
/* print nosiy ouput when GFX error detected */

/*-------------------------------------------------------------------
    File Inclusions
--------------------------------------------------------------------*/

#ifdef	__cplusplus
extern "C"
{
#endif /* __cplusplus */

/*-------------------------------------------------------------------
	Constant Definitions
--------------------------------------------------------------------*/
#define GFX0_MODULE			"gfx"
#ifdef CONFIG_ODIN_DUAL_GFX
#define GFX1_MODULE			"dualgfx"
#endif
#define GFX_MAX_DEVICE		1

/*-------------------------------------------------------------------
	Macro Definitions
--------------------------------------------------------------------*/


/*-------------------------------------------------------------------
    Type Definitions
--------------------------------------------------------------------*/


/**
 * Common Memory Entry for each module/sub-module.
 * if base is zero then, it shall be calculated automatically
 * from previous end address.
 */
typedef struct
{
	char *name;	/* name of memory chunk : for debugging & module param */
	UINT32	base;	/* physical base address in BYTE!!! of media ip */
	UINT32	size;	/* size in BYTE!!! */
} LX_MEMCFG_T;

typedef struct
{
	LX_MEMCFG_T	surface;
}
LX_GFX_MEM_CFG_T;

typedef struct
{
	UINT32		chip;
	char *chip_name;
	UINT32		reg_base_addr;
	UINT32		reg_size;
	UINT32		irq_num;
}
LX_GFX_REG_CFG_T;

/*-------------------------------------------------------------------
	Extern Function Prototype Declaration
--------------------------------------------------------------------*/
extern LX_GFX_MEM_CFG_T gmemcfggfx;
extern LX_GFX_REG_CFG_T *gpgfxregcfg;
#ifdef CONFIG_ODIN_DUAL_GFX
extern LX_GFX_MEM_CFG_T gmemcfggfx1;
extern LX_GFX_REG_CFG_T *gpgfxregcfg1;
#endif

/*-------------------------------------------------------------------
	Extern Variables
--------------------------------------------------------------------*/

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _GFX_CFG_H_ */

/** @} */


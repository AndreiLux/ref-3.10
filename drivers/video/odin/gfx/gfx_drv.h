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
#ifndef __GFX_DRV_H__
#define __GFX_DRV_H__

/*-------------------------------------------------------------------
	Control Constants
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
    File Inclusions
--------------------------------------------------------------------*/
#include "base_types.h"
#include "gfx_kapi.h"

#ifdef	__cplusplus
extern "C"
{
#endif /* __cplusplus */

/*-------------------------------------------------------------------
	Constant Definitions
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Macro Definitions
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
    Type Definitions
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Extern Function Prototype Declaration
--------------------------------------------------------------------*/
extern void gfx_preinit(void);
extern int gfx_init (void);
extern void gfx_cleanup(void);

extern void gfx_lockdevice(void);
extern void gfx_unlockdevice(void);

extern int gfx_getsurfacememorystat(LX_GFX_MEM_STAT_T* pMemStat );

#ifdef	__cplusplus
}
#endif /* __cplusplus */




#endif	/* __GFX_DRV_H__ */

/** @} */

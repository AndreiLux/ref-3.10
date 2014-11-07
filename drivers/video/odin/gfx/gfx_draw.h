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
#ifndef	_GFX_DRV_H_
#define	_GFX_DRV_H_

/*----------------------------------------------------------------
	Control Constants
-----------------------------------------------------------------*/

/*----------------------------------------------------------------
    File Inclusions
-----------------------------------------------------------------*/
#include "base_types.h"
#include "gfx_kapi.h"

#ifdef	__cplusplus
extern "C"
{
#endif /* __cplusplus */

/*----------------------------------------------------------------
	Constant Definitions
-----------------------------------------------------------------*/

/*----------------------------------------------------------------
	Macro Definitions
-----------------------------------------------------------------*/

/*----------------------------------------------------------------
    Type Definitions
-----------------------------------------------------------------*/

/*----------------------------------------------------------------
	Extern Function Prototype Declaration ( GFX Basic Renderer )
-----------------------------------------------------------------*/
extern void gfx_lockdevice(void);
extern void gfx_unlockdevice(void);

#define GFX_AllocARGBSurface(w,h,phys_addr)  gfx_allocargbsurfaceex(w,h,phys_addr,(phys_addr)?(w)*4:0x0)
extern int gfx_allocargbsurfaceex( int w, int h, UINT32 phys_addr, UINT32 stride );
extern int gfx_alloc8bppsurfaceex( int w, int h, UINT32 phys_addr, UINT32 stride );
extern int gfx_setsurfacepalette( int dst_fd, int size , UINT32 *data);
extern int gfx_freesurface( int dst_fd );
extern int gfx_getsurfaceinfo( int dst_fd, LX_GFX_SURFACE_SETTING_T* surface_info );
extern void gfx_blitsurface( int src0_fd, int src1_fd, int dst_fd, int sx, int sy, int sw, int wh, int dx, int dy, BOOLEAN fBlend );
extern void gfx_stretchsurface( int src_fd, int dst_fd, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh );
extern void gfx_clearsurface( int dst_fd, int w, int h );
extern void gfx_fillsurface( int dst_fd, int x, int y, int w, int h, UINT32 color );
extern void gfx_fadesurface( int src_fd, int dst_fd, int w, int h, UINT32 alpha );
#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _GFX_DRV_H_ */

/** @} */

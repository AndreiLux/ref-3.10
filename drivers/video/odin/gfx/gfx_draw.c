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

/*-------------------------------------------------------------------
	Control Constants
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	File Inclusions
--------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/memory.h>
#include <asm/uaccess.h>            /* to verify pointers from user space */
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>

#include "gfx_impl.h"

/*-------------------------------------------------------------------
	Constant Definitions
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Macro Definitions
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

/*-------------------------------------------------------------------
	Static Function Prototypes Declarations
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Static Variables
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Implementation Group
--------------------------------------------------------------------*/
static void gfx_waitgfxsync( LX_GFX_MANUAL_BLEND_CTRL_T* blend_ctrl,
	BOOLEAN fAutoReset );

/*-------------------------------------------------------------------
	Static Variables
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
    Implementation Group
--------------------------------------------------------------------*/
int gfx_allocargbsurfaceex( int w, int h, UINT32 phys_addr, UINT32 stride )
{
    LX_GFX_SURFACE_SETTING_T            surface_settings;
    LX_GFX_SURFACE_SETTING_PARAM_T      surface_param;

    memset( &surface_settings, 0x0, sizeof(LX_GFX_SURFACE_SETTING_T) );

    if ( phys_addr )
    {
        surface_settings.type       = LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER;
        surface_settings.phys_addr  = (void*)phys_addr;
    }
    else
    {
        surface_settings.type       = LX_GFX_SURFACE_TYPE_MEM_BUFFER;
    }
    surface_settings.pixel_format   = LX_GFX_PIXEL_FORMAT_ARGB8888;
    surface_settings.width          = w;
    surface_settings.height         = h;
    surface_settings.alignment      = 4;
    surface_settings.stride         = stride;

    memcpy( &surface_param.surface_setting, &surface_settings,
    	sizeof(LX_GFX_SURFACE_SETTING_T) );

    gfx_allocsurface( &surface_param );

    return surface_param.surface_fd;
}

int gfx_alloc8bppsurfaceex( int w, int h, UINT32 phys_addr, UINT32 stride )
{
    LX_GFX_SURFACE_SETTING_T            surface_settings;
    LX_GFX_SURFACE_SETTING_PARAM_T      surface_param;

    memset( &surface_settings, 0x0, sizeof(LX_GFX_SURFACE_SETTING_T) );

    if ( phys_addr )
    {
        surface_settings.type       = LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER;
        surface_settings.phys_addr  = (void*)phys_addr;
    }
    else
    {
        surface_settings.type       = LX_GFX_SURFACE_TYPE_MEM_BUFFER;
    }
    surface_settings.pixel_format   = LX_GFX_PIXEL_FORMAT_INDEX_8;
    surface_settings.width          = w;
    surface_settings.height         = h;
    surface_settings.alignment      = 4;
    surface_settings.stride         = stride;

    memcpy( &surface_param.surface_setting, &surface_settings,
    	sizeof(LX_GFX_SURFACE_SETTING_T) );

    gfx_allocsurface( &surface_param );

    return surface_param.surface_fd;
}

void gfx_fillsurface( int fd, int x, int y, int w, int h, UINT32 color )
{
	LX_GFX_PORT_T	src0_port;
	LX_GFX_PORT_T	src1_port;
	LX_GFX_PORT_T	dst_port;

	LX_GFX_MANUAL_BLEND_CTRL_PARAM_T	blend_ctrl;
	memset( &blend_ctrl, 0x0, sizeof(LX_GFX_MANUAL_BLEND_CTRL_PARAM_T) );

	/* basic error check */
	if ( w <= 0 || h <= 0 ) return;

	memset( &src0_port, 0x0, sizeof(LX_GFX_PORT_T));
	memset( &dst_port,  0x0, sizeof(LX_GFX_PORT_T));

	dst_port.surface_type	= LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER;
	dst_port.surface_fd 	= fd;
	dst_port.rect.x			= x;
	dst_port.rect.y			= y;
	dst_port.rect.w			= w;
	dst_port.rect.h			= h;

	src1_port.surface_type  = LX_GFX_SURFACE_TYPE_COLOR;
	src1_port.surface_color.pixel_format = LX_GFX_PIXEL_FORMAT_INDEX_0;
	src1_port.surface_color.pixel_value  = color;
	src1_port.rect.x        = 0;
	src1_port.rect.y        = 0;
	src1_port.rect.w        = w;
	src1_port.rect.h        = h;

	src0_port.surface_type	= LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER;
	src0_port.surface_fd 	= fd;
	src0_port.rect.x		= x;
	src0_port.rect.y		= y;
	src0_port.rect.w		= w;
	src0_port.rect.h		= h;

	blend_ctrl.op_mode	= LX_GFX_OP_MODE_TWO_SRC_PORT;
	blend_ctrl.out_sel	= LX_GFX_OUT_SEL_ROP;
	blend_ctrl.rop		= LX_GFX_ROP_DST;
	blend_ctrl.constant_color = 0x0;

	memcpy( &blend_ctrl.src0, &src0_port, sizeof(LX_GFX_PORT_T));
	memcpy( &blend_ctrl.src1, &src1_port, sizeof(LX_GFX_PORT_T));

	memcpy( &blend_ctrl.dst,  &dst_port, sizeof(LX_GFX_PORT_T));
	gfx_runblendop(&blend_ctrl);
	gfx_waitgfxsync ( &blend_ctrl, FALSE );
}

void gfx_clearsurface	( int fd, int w, int h )
{
	gfx_fillsurface( fd, 0, 0, w, h, 0x0 );
}

void gfx_stretchsurface ( int src, int dst, int sx, int sy, int sw,
				int sh, int dx, int dy, int dw, int dh )
{
	LX_GFX_PORT_T	src0_port;
	LX_GFX_PORT_T	src1_port;
	LX_GFX_PORT_T	dst_port;

	LX_GFX_MANUAL_BLEND_CTRL_PARAM_T	blend_ctrl;
	memset( &blend_ctrl, 0x0, sizeof(LX_GFX_MANUAL_BLEND_CTRL_PARAM_T) );

	memset( &src0_port, 0x0, sizeof(LX_GFX_PORT_T));
	memset( &src1_port, 0x0, sizeof(LX_GFX_PORT_T));
	memset( &dst_port,  0x0, sizeof(LX_GFX_PORT_T));

	src0_port.surface_type	= LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER;
	src0_port.surface_fd 	= src;
	src0_port.rect.x		= sx;
	src0_port.rect.y		= sy;
	src0_port.rect.w		= sw;
	src0_port.rect.h		= sh;

	src1_port.surface_type  = LX_GFX_SURFACE_TYPE_COLOR;
	src1_port.surface_color.pixel_format = LX_GFX_PIXEL_FORMAT_INDEX_0;
	src1_port.surface_color.pixel_value  = 0x00000000;
	src1_port.rect.x        = 0;
	src1_port.rect.y        = 0;
	src1_port.rect.w        = sw;
	src1_port.rect.h        = sh;

	dst_port.surface_type	= LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER;
	dst_port.surface_fd 	= dst;
	dst_port.rect.x			= dx;
	dst_port.rect.y			= dy;
	dst_port.rect.w			= sw;
	dst_port.rect.h			= sh;

	blend_ctrl.scaler.mode 	= LX_GFX_SCALER_MODE_BILINEAR;
	blend_ctrl.scaler.input.w	= sw;
	blend_ctrl.scaler.input.h	= sh;
	blend_ctrl.scaler.output.w	= dw;
	blend_ctrl.scaler.output.h	= dh;

	blend_ctrl.op_mode		= LX_GFX_OP_MODE_TWO_SRC_PORT;
	blend_ctrl.out_sel		= LX_GFX_OUT_SEL_ROP;
	blend_ctrl.rop			= LX_GFX_ROP_SRC;
	blend_ctrl.constant_color	= 0x0;

	memcpy( &blend_ctrl.src0, &src0_port, sizeof(LX_GFX_PORT_T));
	memcpy( &blend_ctrl.src1, &src1_port, sizeof(LX_GFX_PORT_T));
	memcpy( &blend_ctrl.dst, &dst_port, sizeof(LX_GFX_PORT_T));

	gfx_runblendop(&blend_ctrl);
	gfx_waitgfxsync ( &blend_ctrl, TRUE );
}

void gfx_blitsurface( int src0, int src1, int dst, int sx, int sy,
				int sw, int sh, int dx, int dy, BOOLEAN fBlend )
{
	LX_GFX_PORT_T	src0_port;
	LX_GFX_PORT_T	src1_port;
	LX_GFX_PORT_T	dst_port;

	LX_GFX_MANUAL_BLEND_CTRL_PARAM_T	blend_ctrl;
	memset( &blend_ctrl, 0x0, sizeof(LX_GFX_MANUAL_BLEND_CTRL_PARAM_T) );

	memset( &src0_port, 0x0, sizeof(LX_GFX_PORT_T));
	memset( &src1_port, 0x0, sizeof(LX_GFX_PORT_T));
	memset( &dst_port,  0x0, sizeof(LX_GFX_PORT_T));

	src0_port.surface_type = LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER;
	src0_port.surface_fd = src0;
	src0_port.rect.x	= sx;
	src0_port.rect.y	= sy;
	src0_port.rect.w	= sw;
	src0_port.rect.h	= sh;

	if ( src1 > 0 )
	{
		src1_port.surface_type = LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER;
		src1_port.surface_fd = src1;
	}
	else
	{
		src1_port.surface_type = LX_GFX_SURFACE_TYPE_COLOR;
	    src1_port.surface_color.pixel_format= LX_GFX_PIXEL_FORMAT_INDEX_0;
  		src1_port.surface_color.pixel_value = 0x0;
	}
	src1_port.rect.x		= dx;
	src1_port.rect.y		= dy;
	src1_port.rect.w		= sw;
	src1_port.rect.h		= sh;

	dst_port.surface_type	= LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER;
	dst_port.surface_fd 	= dst;
	dst_port.rect.x			= dx;
	dst_port.rect.y			= dy;
	dst_port.rect.w			= sw;
	dst_port.rect.h			= sh;

	if ( fBlend )
	{
		src0_port.port_flag	|= LX_GFX_PORT_FLAG_PREMULTIPLY;
		src1_port.port_flag	|= LX_GFX_PORT_FLAG_PREMULTIPLY;
		dst_port.port_flag	|= LX_GFX_PORT_FLAG_DEMULTIPLY;

		blend_ctrl.xor_alpha_en 	= FALSE;
		blend_ctrl.xor_color_en		= FALSE;
		blend_ctrl.mux_alpha_m0		= LX_GFX_MUX_ALPHA_M0_ALPHA0;
		blend_ctrl.mux_color_m0		= LX_GFX_MUX_COLOR_M0_COLOR0;
		blend_ctrl.comp_rule_alpha	= LX_GFX_COMP_RULE_FACTOR;
		blend_ctrl.comp_rule_color	= LX_GFX_COMP_RULE_FACTOR;
		blend_ctrl.mux_factor0_alpha= LX_GFX_MUX_FACTOR_ONE;
		blend_ctrl.mux_factor1_alpha= LX_GFX_MUX_FACTOR_ONE_MINUS_SRC0_ALPHA;
		blend_ctrl.mux_factor0_color= LX_GFX_MUX_FACTOR_ONE;
		blend_ctrl.mux_factor1_color= LX_GFX_MUX_FACTOR_ONE_MINUS_SRC0_ALPHA;

		blend_ctrl.op_mode	= LX_GFX_OP_MODE_TWO_SRC_PORT;
		blend_ctrl.out_sel	= LX_GFX_OUT_SEL_BLEND;
		blend_ctrl.rop		= LX_GFX_ROP_NONE;
		blend_ctrl.constant_color = 0x0;
	}
	else
	{
		blend_ctrl.op_mode	= LX_GFX_OP_MODE_TWO_SRC_PORT;
		blend_ctrl.out_sel	= LX_GFX_OUT_SEL_ROP;
		blend_ctrl.rop		= LX_GFX_ROP_SRC;
		blend_ctrl.constant_color = 0x0;
	}

	memcpy( &blend_ctrl.src0, &src0_port, sizeof(LX_GFX_PORT_T));
	memcpy( &blend_ctrl.src1, &src1_port, sizeof(LX_GFX_PORT_T));
	memcpy( &blend_ctrl.dst,  &dst_port, sizeof(LX_GFX_PORT_T));

	gfx_runblendop(&blend_ctrl);

	gfx_waitgfxsync ( &blend_ctrl, FALSE );
}

/** emulate global alpha modulation for POSD.
 *	POSD should made to GFX surface
 *
 *	@param surf [IN] POSD surface
 *	@param w [IN] width of POSD
 *	@param h [IN] height of POSD
 *	@param alpha [IN] global alpha for POSD
 *
 */
void gfx_fadesurface( int src, int dst, int w, int h, UINT32 alpha )
{
	LX_GFX_PORT_T	src0_port;
	LX_GFX_PORT_T	src1_port;
	LX_GFX_PORT_T	dst_port;

	LX_GFX_MANUAL_BLEND_CTRL_PARAM_T	blend_ctrl;
	memset( &blend_ctrl, 0x0, sizeof(LX_GFX_MANUAL_BLEND_CTRL_PARAM_T) );

	/* alpha with 0xff doesn't need to support FADE */
	if ( alpha >= 0xff ) return;

	memset( &src0_port, 0x0, sizeof(LX_GFX_PORT_T));
	memset( &src1_port, 0x0, sizeof(LX_GFX_PORT_T));
	memset( &dst_port,  0x0, sizeof(LX_GFX_PORT_T));

	src0_port.surface_type	= LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER;
	src0_port.surface_fd 	= src;
	src0_port.rect.x		= 0;
	src0_port.rect.y		= 0;
	src0_port.rect.w		= w;
	src0_port.rect.h		= h;

	src1_port.surface_type				= LX_GFX_SURFACE_TYPE_COLOR;
	src1_port.surface_color.pixel_format= LX_GFX_PIXEL_FORMAT_INDEX_0;
	src1_port.surface_color.pixel_value	= 0x0;

	dst_port.surface_type	= LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER;
	dst_port.surface_fd 	= dst;
	dst_port.rect.x			= 0;
	dst_port.rect.y			= 0;
	dst_port.rect.w			= w;
	dst_port.rect.h			= h;

#if 0
	src0_port.port_flag	|= LX_GFX_PORT_FLAG_PREMULTIPLY;
	src1_port.port_flag	|= LX_GFX_PORT_FLAG_PREMULTIPLY;
	dst_port.port_flag	|= LX_GFX_PORT_FLAG_DEMULTIPLY;
#endif

	blend_ctrl.xor_alpha_en = FALSE;
	blend_ctrl.xor_color_en= FALSE;
	blend_ctrl.mux_alpha_m0= LX_GFX_MUX_ALPHA_M0_ALPHA0_X_CONST_ALPHA;
	blend_ctrl.mux_color_m0 = LX_GFX_MUX_COLOR_M0_COLOR0;
	blend_ctrl.comp_rule_alpha = LX_GFX_COMP_RULE_FACTOR;
	blend_ctrl.comp_rule_color = LX_GFX_COMP_RULE_FACTOR;
	blend_ctrl.mux_factor0_alpha= LX_GFX_MUX_FACTOR_ONE;
	blend_ctrl.mux_factor1_alpha= LX_GFX_MUX_FACTOR_ZERO;
							/*ONE_MINUS_SRC0_ALPHA; */
	blend_ctrl.mux_factor0_color= LX_GFX_MUX_FACTOR_ONE;
	blend_ctrl.mux_factor1_color= LX_GFX_MUX_FACTOR_ZERO;
							/* ONE_MINUS_SRC0_ALPHA; */
	blend_ctrl.op_mode			= LX_GFX_OP_MODE_TWO_SRC_PORT;
	blend_ctrl.out_sel			= LX_GFX_OUT_SEL_BLEND;
	blend_ctrl.rop				= LX_GFX_ROP_NONE;
	blend_ctrl.constant_color	= (alpha<<24) | 0xffffff;

	memcpy( &blend_ctrl.src0, &src0_port, sizeof(LX_GFX_PORT_T));
	memcpy( &blend_ctrl.src1, &src1_port, sizeof(LX_GFX_PORT_T));
	memcpy( &blend_ctrl.dst,  &dst_port,  sizeof(LX_GFX_PORT_T));

	gfx_runblendop(&blend_ctrl);

	gfx_waitgfxsync ( &blend_ctrl, FALSE );
}


static void gfx_waitgfxsync( LX_GFX_MANUAL_BLEND_CTRL_T* blend_ctrl,
						BOOLEAN fAutoReset )
{
	int ret;
	UINT32 wait_time = gfx_calcsynctimeout( blend_ctrl,
						g_gfx_cfg.sync_wait_timeout );

	gfx_runflushcommand();
	ret = gfx_waitsynccommand( wait_time );
	if (ret != RET_OK)
	{
		GFXERR("GFX kdriver draw failed\n");
		gfx_dumpblitparam( blend_ctrl );
	}

/*	if ( fAutoReset ) gfx_swresetcommand(); */
}


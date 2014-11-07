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

/*--------------------------------------------------------------------
	Control Constants
---------------------------------------------------------------------*/

/*--------------------------------------------------------------------
	File Inclusions
---------------------------------------------------------------------*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>		 /**< printk() */
#include <linux/slab.h>			 /**< kmalloc() */
#include <linux/vmalloc.h>		 /**< kmalloc() */
#include <linux/fs.h>			 /**< everything\ldots{} */
#include <linux/types.h>		 /**< size_t */
#include <linux/fcntl.h>		 /**< O_ACCMODE */
#include <asm/uaccess.h>
#include <linux/ioport.h>		 /**< For request_region, check_region etc */
#include <asm/io.h>				 /**< For ioremap_nocache */
#include <linux/workqueue.h>	 /**< For working queue */
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>

#include "gfx_impl.h"

/*--------------------------------------------------------------------
	Constant Definitions
---------------------------------------------------------------------*/

/*--------------------------------------------------------------------
	Macro Definitions
---------------------------------------------------------------------*/
#define ROUND32(width) ((((width) + 31) / 32) * 32 )

/*--------------------------------------------------------------------
	Type Definitions
---------------------------------------------------------------------*/

/*--------------------------------------------------------------------
	External Function Prototype Declarations
---------------------------------------------------------------------*/

/*--------------------------------------------------------------------
	External Variables
---------------------------------------------------------------------*/

/*--------------------------------------------------------------------
	global Variables
---------------------------------------------------------------------*/
GFX_SURFACE_OBJ_T *g_gfx_surf_list;

/* int GFX_AllocMemory(u32 surface_fd); */
/* int GFX_FreeMemory(u32 surface_fd); */

/*--------------------------------------------------------------------
	Static Function Prototypes Declarations
---------------------------------------------------------------------*/
/* static int GFX_AllocSurfaceMem(UINT32 surface_fd); */
/* static int GFX_FreeSurfaceMem(UINT32 surface_fd); */

/*--------------------------------------------------------------------
	Static Variables
---------------------------------------------------------------------*/
static u32	g_cr_idx;	/* create index */
static LX_GFX_PHYS_MEM_INFO_T	g_gfx_mem_pool_info;

/*--------------------------------------------------------------------
	Implementation Group
---------------------------------------------------------------------*/

/**
 *
 * initialize GFX memory for surface
 *
 * @param	void
 * @return	int	0 : OK , -1 : NOT OK
 *
 */
int gfx_initsurfacememory(void)
{
	int	ret =0;
	g_cr_idx = 0;

	/* partition gfx memory into two part. first part is non-cached attributed
		and second part is cached attributed */
	g_gfx_mem_pool_info.phys_addr 	= (void*)gmemcfggfx.surface.base;
	g_gfx_mem_pool_info.length	 	= gmemcfggfx.surface.size;
	g_gfx_mem_pool_info.offset	 	= 0;

	GFX_TRACE_BEGIN();
	g_gfx_surf_list = (GFX_SURFACE_OBJ_T *)
		vmalloc(sizeof(GFX_SURFACE_OBJ_T)*GFX_MAX_SURFACE);
	GFX_CHECK_ERROR( NULL == g_gfx_surf_list, return RET_ERROR,
		"can't alloc surf_list\n");

	memset((void *)g_gfx_surf_list, 0x0 ,
		sizeof(GFX_SURFACE_OBJ_T)*GFX_MAX_SURFACE);

	GFX_CHECK_ERROR( ret<0 , /* nop */, "can't alloc surf region\n" );
	GFX_TRACE_END();

	return ret;
}

/**
 *
 * allocate surface memory as imcoming param-LX_GFX_SURFACE_SETTING_PARAM_T
 *
 * @param	LX_GFX_SURFACE_SETTING_PARAM_T
 * @return	int	0 : OK , -1 : NOT OK
 *
 */
int gfx_allocsurface(LX_GFX_SURFACE_SETTING_PARAM_T *surface)
{
	int i;
	int slot = -1;
	int ret = RET_ERROR;

	GFX_TRACE_BEGIN();

	/* I would like to give the special fd to the external memory based surface */
	if ( (ULONG)surface->surface_setting.phys_addr ) { i = 0; }
	else 								{ i = 0x80; }

	/* search empty slot */
	for ( i = 0  ; i < GFX_MAX_SURFACE; i++ )
	{
		if ( !g_gfx_surf_list[i].balloc ) { slot = i; break; }
	}

	GFX_CHECK_ERROR( slot < 0, goto func_exit,
		"surface pool(max %d) is full\n", GFX_MAX_SURFACE );
/*	GFXINFO("%s : empty slot = %d\n", __F__, slot );	*/

	surface->surface_fd = slot;
	g_gfx_surf_list[slot].balloc = 1;
	g_gfx_surf_list[slot].surf.surface_fd = slot;
	g_gfx_surf_list[slot].surf.type = surface->surface_setting.type;
	g_gfx_surf_list[slot].surf.pixel_format =
				surface->surface_setting.pixel_format;
	g_gfx_surf_list[slot].surf.width = surface->surface_setting.width;
	g_gfx_surf_list[slot].surf.height = surface->surface_setting.height;
	g_gfx_surf_list[slot].surf.alignment = surface->surface_setting.alignment;
	g_gfx_surf_list[slot].cidx = g_cr_idx++;
	g_gfx_surf_list[slot].ctick = OS_GETMSECTICKS();

	/* get task name */
	get_task_comm( g_gfx_surf_list[slot].psname, current );
	g_gfx_surf_list[slot].psname[TASK_COMM_LEN] = 0x0;

	GFX_TRACE("%s:%d -- fd %d, dim %dx%d\n", __F__, __L__, slot,
		g_gfx_surf_list[slot].surf.width, g_gfx_surf_list[slot].surf.height );

	/* if application doesn't know stride, caculate it here */
	if (surface->surface_setting.stride == 0)
	{
		switch (surface->surface_setting.pixel_format)
		{
			case LX_GFX_PIXEL_FORMAT_INDEX_0:
				g_gfx_surf_list[slot].surf.stride = 0;
				break;

			case LX_GFX_PIXEL_FORMAT_INDEX_1:
				g_gfx_surf_list[slot].surf.stride =
					ROUND32(surface->surface_setting.width*1)/8;
				break;
			case LX_GFX_PIXEL_FORMAT_INDEX_2 :
				g_gfx_surf_list[slot].surf.stride =
					ROUND32(surface->surface_setting.width*2)/8;
				break;
			case LX_GFX_PIXEL_FORMAT_INDEX_4 :
				g_gfx_surf_list[slot].surf.stride =
					ROUND32(surface->surface_setting.width*4)/8;
				break;
			case LX_GFX_PIXEL_FORMAT_INDEX_8 :
			case LX_GFX_PIXEL_FORMAT_ALPHA_8 :
				g_gfx_surf_list[slot].surf.stride =
					ROUND32(surface->surface_setting.width*8)/8;
				break;

			case LX_GFX_PIXEL_FORMAT_Y8__CB8_444__CR8_444 :
			case LX_GFX_PIXEL_FORMAT_CB8_420__CR8_420:
			case LX_GFX_PIXEL_FORMAT_CB8_422__CR8_422:
				g_gfx_surf_list[slot].surf.stride =
					ROUND32(surface->surface_setting.width*8)/8;
				break;

			case LX_GFX_PIXEL_FORMAT_CBCR_420:		/* 16 */
			case LX_GFX_PIXEL_FORMAT_CBCR_422:		/* 16 */
			case LX_GFX_PIXEL_FORMAT_CBCR_444:		/* 16 */
			case LX_GFX_PIXEL_FORMAT_Y0CB0Y1CR0_422:/* 16 */
				g_gfx_surf_list[slot].surf.stride =
					ROUND32(surface->surface_setting.width*16)/8;
				break;

			case LX_GFX_PIXEL_FORMAT_YCBCR655:
			case LX_GFX_PIXEL_FORMAT_AYCBCR2644:
			case LX_GFX_PIXEL_FORMAT_AYCBCR4633:
			case LX_GFX_PIXEL_FORMAT_AYCBCR6433:
			case LX_GFX_PIXEL_FORMAT_RGB565 :
			case LX_GFX_PIXEL_FORMAT_ARGB1555:
			case LX_GFX_PIXEL_FORMAT_ARGB4444 :
			case LX_GFX_PIXEL_FORMAT_ARGB6343 :
				g_gfx_surf_list[slot].surf.stride =
					ROUND32(surface->surface_setting.width*16)/8;
				break;

			case LX_GFX_PIXEL_FORMAT_AYCBCR8888 :
			case LX_GFX_PIXEL_FORMAT_ARGB8888 :
				g_gfx_surf_list[slot].surf.stride =
					ROUND32(surface->surface_setting.width*32)/8;
				break;
		}
	}
	else
	{
		g_gfx_surf_list[slot].surf.stride = surface->surface_setting.stride;
	}

	/*	raxis.lim (2010/05/13)
	 *	-- 	we need fill surface memory info even though it is from
	 *		external memory
	 *		since application can query surface memory regardless of
	 *		surface type.
	 *		but it is important that valid information may be only physical
	 *		address or length.
	 */
	if ( surface->surface_setting.phys_addr )
	{
		g_gfx_surf_list[slot].surf.phys_addr=
			surface->surface_setting.phys_addr;
		g_gfx_surf_list[slot].mem.phys_addr =
			surface->surface_setting.phys_addr;
		g_gfx_surf_list[slot].mem.length =
			surface->surface_setting.stride * surface->surface_setting.height;
		g_gfx_surf_list[slot].mem.offset =
			(UINT32)surface->surface_setting.phys_addr -
			(UINT32)g_gfx_mem_pool_info.phys_addr;

		ret = RET_OK;
	}
	else
	{
		GFXDBG("%s : Error because of memory addr invalidity\n", __F__ );
		ret = RET_ERROR;
/*		ret = GFX_AllocSurfaceMem(surface->surface_fd); */
	}

	GFX_CHECK_ERROR( ret != RET_OK, g_gfx_surf_list[slot].balloc = 0;
		goto func_exit, "fail to alloc surface (%d)\n", surface->surface_fd );

	g_gfx_surf_list[slot].bpalette = 0;

	/* return surface_settings data */
	memcpy( &surface->surface_setting, &g_gfx_surf_list[slot].surf,
		sizeof(LX_GFX_SURFACE_SETTING_T));

	ret = RET_OK;
func_exit:
	if ( ret != RET_OK )
	{
		/* raxis.lim (2012/11/12) print memory pool info for debug */
		LX_GFX_MEM_STAT_T	gfx_mem_stat;
		int					surface_cnt = 0;

		(void)gfx_getsurfacememorystat( &gfx_mem_stat );
		printk("gfx memory pool info = ptr %p, len %d KB, alloc %d KB, \
		free %d KB\n",
		gfx_mem_stat.surface_mem_base, gfx_mem_stat.surface_mem_length>>10,
		gfx_mem_stat.surface_mem_alloc_size>>10,
		gfx_mem_stat.surface_mem_free_size>>10 );

		for ( i=0; i< GFX_MAX_SURFACE; i++ )
		{
			if ( g_gfx_surf_list[i].balloc ) surface_cnt++;
		}
		printk("total %d surface(s) allocated\n", surface_cnt );
	}
	GFX_TRACE_END();
	return ret;
}

/** free allocated surface memory which got from gfx_allocsurface
 *
 * @param	int surface_fd
 * @return	int	0 : OK , -1 : NOT OK
 */
int gfx_freesurface(UINT32 surface_fd)
{
	UINT32 phys_addr;
	int ret = RET_ERROR;
	GFX_TRACE_BEGIN();

	/* check error condition */
	GFX_CHECK_ERROR( surface_fd >= GFX_MAX_SURFACE,
		goto func_exit, "invalid fd 0x%x(%d)\n", surface_fd, surface_fd );
	GFX_CHECK_ERROR( !g_gfx_surf_list[surface_fd].balloc, goto func_exit,
		"not allocated fd 0x%x(%d)\n", surface_fd, surface_fd );

	phys_addr = (UINT32)g_gfx_surf_list[surface_fd].surf.phys_addr;

	/* since surface is allocated with the external memory,
		we should check phys_addr range */
	if ( g_gfx_surf_list[surface_fd].surf.type == LX_GFX_SURFACE_TYPE_MEM_BUFFER
		&& ( phys_addr >= (UINT32)g_gfx_mem_pool_info.phys_addr
		&& phys_addr < (UINT32)g_gfx_mem_pool_info.phys_addr +
		g_gfx_mem_pool_info.length ) )
	{
/*       GFX_FreeSurfaceMem(surface_fd); */
	}

	/*	raxis.lim (2010/07/01)
	 *	-- palettte memory should be free when paleltte data is available.
	 *	-- bpalette variable seems not enough to free palette data.
	 */
	if ( g_gfx_surf_list[surface_fd].pal )
	{
		OS_Free( g_gfx_surf_list[surface_fd].pal );
	}

	memset( &g_gfx_surf_list[surface_fd], 0x0, sizeof(GFX_SURFACE_OBJ_T) );

	ret = RET_OK; /* all work done */
func_exit:
	GFX_TRACE_END();
	return ret;
}


/**
 *
 * get surface's image memory inform
 *
 * @param	LX_GFX_SURFACE_MEM_INFO_PARAM_T *mem
 * @return	int	0 : OK , -1 : NOT OK
 *
 */
int gfx_getsurfacememory(LX_GFX_SURFACE_MEM_INFO_PARAM_T *mem)
{
	GFX_TRACE_BEGIN();
#if 1
	memcpy( &mem->surface_mem, &(g_gfx_surf_list[mem->surface_fd].mem),
	sizeof(LX_GFX_PHYS_MEM_INFO_T));
#else
	mem->surface_mem.phys_addr  = g_gfx_surf_list[mem->surface_fd].mem.phys_addr;
	mem->surface_mem.offset     = g_gfx_surf_list[mem->surface_fd].mem.offset;
	mem->surface_mem.length     = g_gfx_surf_list[mem->surface_fd].mem.length;
#endif

	GFX_TRACE("%s:%d -- fd:%d, phys_addr:%p, offset:%d, length:%d\n", __F__,
		__L__, mem->surface_fd, mem->surface_mem.phys_addr,
		mem->surface_mem.offset, mem->surface_mem.length );
	GFX_TRACE_END();
	return 0;
}


/** save the pallete date to surface
 *
 */
int gfx_setsurfacepalette(int surface_fd, int size , UINT32 *data)
{
	int ret = RET_OK;

	GFX_TRACE_BEGIN();

	if ( (g_gfx_surf_list[surface_fd].surf.width == 0) ||
		(g_gfx_surf_list[surface_fd].surf.height == 0) )
	{
		GFXERR("ERROR : tried to save palette data on free surface [%d]\n" ,
			surface_fd);
		GFX_TRACE_END();
		return RET_ERROR;
	}

	/*	raxis.lim (2010/07/01) -- bug fix
	 *	-- 	the real palette memory is allocated when it is set
	 *		for the first time.
	 *	-- 	We know the maximum palette size is 256 for 8bpp surface,
	 *		so it is very normal to allocate the maximum 256 palette entries.
	 */
	if ( !g_gfx_surf_list[surface_fd].pal )
	{
		g_gfx_surf_list[surface_fd].pal = (UINT32*)OS_Malloc( sizeof(UINT32)*256 );
	}

	GFX_CHECK_ERROR( !g_gfx_surf_list[surface_fd].pal, goto func_exit,
		"out of resource. check memory usage\n");

	memcpy( (void *)g_gfx_surf_list[surface_fd].pal , (void *)data ,
			sizeof(UINT32)*size);

	/* 	raxis.lim (2010/07/01) -- bug fix
	 *	-- palsize should be number of palette entry not byte size of palette data
	 *	-- bpaldownload field should be removed
	 *	( GFX should always download palette when blit time )
	 */
	g_gfx_surf_list[surface_fd].bpalette 	= 1;
	g_gfx_surf_list[surface_fd].bpaldownload= 0;
	g_gfx_surf_list[surface_fd].palsize 	= size;

	GFXINFO("%s:%d -- fd %d, palSize %d, palData %p\n", __F__, __L__,
				surface_fd, g_gfx_surf_list[surface_fd].palsize,
				g_gfx_surf_list[surface_fd].pal );
func_exit:
	GFX_TRACE_END();

	return ret;
}

/** get the pallete date from surface
 *
 */
int gfx_getsurfacepalette(int surface_fd, int size , UINT32 *data)
{
	int ret = RET_OK;

	GFX_TRACE_BEGIN();

	if ( (g_gfx_surf_list[surface_fd].surf.width == 0) ||
		(g_gfx_surf_list[surface_fd].surf.height == 0) ||
		(g_gfx_surf_list[surface_fd].pal == NULL ) )
	{
		GFXERR("ERROR : tried to save palette data on free surface [%d]\n",
			surface_fd);
		GFX_TRACE_END();
		return RET_ERROR;
	}

	memcpy( (void*)data, (void *)g_gfx_surf_list[surface_fd].pal,
		sizeof(UINT32)*size);

	GFXINFO("%s:%d -- fd %d, palSize %d, palData %p\n", __F__, __L__,
				surface_fd, g_gfx_surf_list[surface_fd].palsize,
				g_gfx_surf_list[surface_fd].pal );

	GFX_TRACE_END();

	return ret;
}

/**	get memory statistics for GFX surface memory
 *	this function is used in IOCTL for debug, and in gfx proc.
 *	application can use this information to monitor memroy usage.
 *
 *	@param 	pMemStat [IN] pointer to LX_GFX_MEM_STAT_T ( surface memory info )
 *	@return RET_OK
 *
 */
int gfx_getsurfacememorystat ( LX_GFX_MEM_STAT_T* pMemStat )
{
	int	i;
	int	surf_cnt = 0;

	OS_RGN_INFO_T   mem_info;

	GFX_TRACE_BEGIN();

	pMemStat->surface_max_num = GFX_MAX_SURFACE;

	/* get memory region info */

/*	(void)OS_GetRegionInfo ( &g_rgn, &mem_info ); */

	pMemStat->surface_mem_base 		= (void*)mem_info.phys_mem_addr;
	pMemStat->surface_mem_length	= mem_info.length;
	pMemStat->surface_mem_alloc_size= mem_info.mem_alloc_size;
	pMemStat->surface_mem_free_size = mem_info.mem_free_size;

/*	pMemStat->surface_alloc_num		= mem_info.mem_alloc_cnt; */

	/* surface count should include both internal surface and external surface */
	for ( i=0; i< GFX_MAX_SURFACE; i++ )
	{
		if ( g_gfx_surf_list[i].balloc ) surf_cnt++;
	}
	pMemStat->surface_alloc_num	= surf_cnt;

	GFX_TRACE_END();

	return RET_OK;
}
/** @} */

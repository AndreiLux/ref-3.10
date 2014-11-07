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
#define	GFX_IGNORE_DUPLICATE_PALTTE

/*-------------------------------------------------------------------
	File Inclusions
--------------------------------------------------------------------*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>	/**< printk() */
#include <linux/slab.h> 	/**< kmalloc() */
#include <linux/fs.h> 		/**< everything\ldots{} */
#include <linux/types.h> 	/**< size_t */
#include <linux/fcntl.h>	/**< O_ACCMODE */
#include <asm/uaccess.h>
#include <linux/ioport.h>	/**< For request_region, check_region etc */
#include <asm/io.h>			/**< For ioremap_nocache */
#include <linux/delay.h>
#include <linux/workqueue.h>/**< For working queue */
#include <linux/interrupt.h>
#include <linux/sched.h>	/* for schedule_timeout */
#include <linux/dma-mapping.h>
#include <linux/version.h>

#include "gfx_impl.h"
#include "gfx_core_reg.h"
	/* For using g_gfx_reg->gfx_intr_clear.gfx_intr_clear */

/*-------------------------------------------------------------------
	Constant Definitions
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Macro Definitions
--------------------------------------------------------------------*/


/*-------------------------------------------------------------------
	External Function Prototype Declarations
--------------------------------------------------------------------*/
extern volatile GFX_REG_T *g_gfx_reg;


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
static LX_GFX_BATCH_RUN_MODE_T g_gfx_run_mode = LX_GFX_BATCH_RUN_MODE_AUTO;
static LX_GFX_GRAPHIC_SYNC_MODE_T g_gfx_sync_mode=
	LX_GFX_GRAPHIC_SYNC_MODE_AUTO;
static DECLARE_WAIT_QUEUE_HEAD(g_gfx_wq_done);

#ifdef GFX_IGNORE_DUPLICATE_PALTTE
static	UINT32	g_palette_0[256];
static	UINT32	g_palette_1[256];
#endif

static u32 cscyuv2argb_coef[8] = {	0x129FFC98,
								0xF775129F,
								0x21D70000,
								0x129F0000,
								0x1CB00000,
								0xFFF0FF80,
								0xFF800000,
								0x00000000};

/*-------------------------------------------------------------------
	Implementation Group
--------------------------------------------------------------------*/
/**
 * register the pallete date to HW IP
 *
 * @param	int port_num , int size , UINT32 *data
 *	port_num : input port#(0~1)
 *	size : palette data size
 *	data : palette data
 * @return	int	0 : OK , -1 : NOT OK
 *
 */
int gfx_downloadpalette(int port_num , int size , UINT32 *data)
{
	int ret = RET_OK;

	GFX_CMD_OP_T cmd_op;

	GFXINFO("%s:%d -- downloading palette port %d, palSize %d, palData %p\n",
		__F__, __L__, port_num, size, data );
	GFX_TRACE_BEGIN();

	memset( &cmd_op, 0x0, sizeof(GFX_CMD_OP_T));

	switch (port_num)
	{
		case 0:
		{
#ifdef GFX_IGNORE_DUPLICATE_PALTTE
			if ( !( memcmp( g_palette_0, data, sizeof(UINT32)* size )) )
			{
				GFXINFO("<gfx> same palette 0 .. ignore\n"); goto func_exit;
			}
			memcpy( g_palette_0, data, sizeof(UINT32)* size );
#endif
			cmd_op.sport = EREADONEPORT;
			cmd_op.bupdateclut = 0x0;
			/* raxis.lim (2010/07/01) -- bugf fix . write should be zero */
		}
		break;

		case 1:
		{
#ifdef GFX_IGNORE_DUPLICATE_PALTTE
			if ( !( memcmp( g_palette_1, data, sizeof(UINT32)* size )) )
			{
				GFXINFO("<gfx> same palette 1 .. ignore\n"); goto func_exit;
			}
			memcpy( g_palette_1, data, sizeof(UINT32)* size );
#endif
			cmd_op.sport = EREADTWOPORTS;
			cmd_op.bupdateclut = 0x0;
			/* raxis.lim (2010/07/01) -- bugf fix . write should be zero */
		}
		break;

		default:
			ret = RET_ERROR; goto func_exit;
		break;
	}

	gfx_setoperationstatus(&cmd_op);

	if (gfx_setclut(port_num , size , data) < 0)
	{
		GFXERR("gfx_setclut error\n"); ret = RET_ERROR; goto func_exit;
	}

func_exit:
	GFX_TRACE_END();
	return ret;
}

/** draw image on memory by application layer's setting
 *
 * @param	LX_GFX_MANUAL_BLEND_CTRL_PARAM_T
 * @return	int	0 : OK , -1 : NOT OK
 *
 *	raxis.lim (2010/05/15)
 *	--	bug fix in determine port->uladdr
 *	--	port->uladdr should be calculated by
 *	(base_addr + stride * y + byets_per_pixel * x )
 */
int gfx_runblendop(LX_GFX_MANUAL_BLEND_CTRL_PARAM_T *param)
{
	GFX_PORT_CONFIGURE_T 	*iport0 = NULL, *iport1 = NULL, *iport2 = NULL;
	GFX_OUT_CONFIGURE_T  	*oport;
	GFX_BLEND_CONFIGURE_T 	*blend;
	GFX_CMD_OP_T 			*operate;
	GFX_SCALER_CONFIGURE_T	*scaler;

	int ret=RET_OK;

	GFX_SURFACE_OBJ_T	*src0_surface;
	GFX_SURFACE_OBJ_T	*src1_surface;
	GFX_SURFACE_OBJ_T	*src2_surface;
	GFX_SURFACE_OBJ_T	*dst_surface;

	/* initialize premultiply/demultiply flag to be FALSE by default */
	BOOLEAN	fsrc0premultiply = FALSE;
	BOOLEAN	fsrc1premultiply = FALSE;
	BOOLEAN	fdstdemultiply = FALSE;

#if 0
	/* custom blend doesn't support LX_GFX_OP_CMD_WRITE */
	if ( param->op_mode == LX_GFX_OP_MODE_WRITE )
	{
		GFX_WARN("%s:%d -- WRITE mode is supported in GFX_IOW_BLEND\n",
			__F__, __L__ );
		return RET_ERROR;
	}

	/* custom blend doesn't support LX_GFX_OP_ONE_SRC_PORT */
	if ( param->op_mode == LX_GFX_OP_MODE_ONE_SRC_PORT )
	{
		GFX_WARN("%s:%d -- SCRBLT mode is supported in GFX_IOW_BLEND\n",
			__F__, __L__ );
		return RET_ERROR;
	}
#endif

#if 0
	/* get the dummy spin lock to sync memory */
	ULONG flags;
	spin_lock_irqsave(&_g_draw_spinlock, flags);
	spin_unlock_irqrestore(&_g_draw_spinlock, flags);
#endif

	GFX_TRACE_BEGIN();

	/* get surface handle for each GFX port */
	src0_surface = &(g_gfx_surf_list[param->src0.surface_fd]);
	src1_surface = &(g_gfx_surf_list[param->src1.surface_fd]);
	src2_surface = &(g_gfx_surf_list[param->src2.surface_fd]);
	dst_surface = &(g_gfx_surf_list[param->dst.surface_fd]);

#ifdef GFX_HEX_DUMP
	OS_HexDump((UINT32)param, param, sizeof(LX_GFX_MANUAL_BLEND_CTRL_PARAM_T));
#endif

	iport0 = (GFX_PORT_CONFIGURE_T*) OS_Malloc(sizeof(GFX_PORT_CONFIGURE_T));
	iport1 = (GFX_PORT_CONFIGURE_T*) OS_Malloc(sizeof(GFX_PORT_CONFIGURE_T));
	oport = (GFX_OUT_CONFIGURE_T*)  OS_Malloc(sizeof(GFX_OUT_CONFIGURE_T));
	blend = (GFX_BLEND_CONFIGURE_T*)OS_Malloc(sizeof(GFX_BLEND_CONFIGURE_T));
	scaler = (GFX_SCALER_CONFIGURE_T*)OS_Malloc(sizeof(GFX_SCALER_CONFIGURE_T));
	operate = (GFX_CMD_OP_T *)OS_Malloc(sizeof(GFX_CMD_OP_T));

	GFX_CHECK_ERROR( iport0 == NULL, goto func_exit, "out of memory\n");
	GFX_CHECK_ERROR( iport1 == NULL, goto func_exit, "out of memory\n");
	GFX_CHECK_ERROR( oport  == NULL, goto func_exit, "out of memory\n");
	GFX_CHECK_ERROR( blend  == NULL, goto func_exit, "out of memory\n");
	GFX_CHECK_ERROR( scaler == NULL, goto func_exit, "out of memory\n");
	GFX_CHECK_ERROR( operate== NULL, goto func_exit, "out of memory\n");

	memset( iport0, 0x0, sizeof(GFX_PORT_CONFIGURE_T));
	memset( iport1, 0x0, sizeof(GFX_PORT_CONFIGURE_T));
	memset( oport , 0x0, sizeof(GFX_OUT_CONFIGURE_T));
	memset( blend , 0x0, sizeof(GFX_BLEND_CONFIGURE_T));
	memset( operate,0x0, sizeof(GFX_CMD_OP_T));

	/* iport2 may not be used. so initialize it to NULL */
	iport2 = NULL;


	/* raxis.lim (2012/11/02)
	 * reset scaler H/W block for prevent some GFX H/W bugs.
	 * it's platform dependent.
	 * in H13B0, scaler block should be reset. otherwise output image
	 * is shifted to right by 10-20 pixels.
	 */
	memset( scaler, 0x0, sizeof(GFX_SCALER_CONFIGURE_T));
	scaler->cmd = GFX_SCALER_CMD_SOFT_RESET;
	gfx_setscalerconfigure( scaler );

	/* raxis.lim (2011/05/19)
	 * when scaler enabled, we should modify dst rect to be the
	 * same dimension as src rect.
	 * GFX blender block requires the same dimension in both src and dst port.
	 * the real scaling dimension should be set after blender block
	 *
	 * since we are using only very simple scaler from src0 to dst port,
	 * we can refer to src0 dimension without any check code.
	 */
	if ( param->scaler.mode )
	{
		param->dst.rect.w = param->src0.rect.w;
		param->dst.rect.h = param->src0.rect.h;
	}

	/*	parse src0 port configuration
	 *	-- alomost GFX operations needs read port0 (src0)
	 *	except OP_MODE_WRITE and OP_MODE_PACK_Cb_Cr
	 *
	 *	raxis.lim (2010/05/28)
	 *	[TODO] need some code to convert 8bpp or 16bpp surface to 32bpp ????
	 */
	{
		iport0->sport 	= EPORT0;
		iport0->sendian	= ELITTLE;

		if ( param->src0.surface_type == LX_GFX_SURFACE_TYPE_COLOR )
		{
			iport0->usstride		= 0x0;
			iport0->uladdr			= 0x0;
			iport0->sfmt			= LX_GFX_PIXEL_FORMAT_INDEX_0;
			iport0->uiglobalalpha 	= param->src0.surface_color.pixel_value;
		}
		else
		{
			iport0->usstride		= (UINT16)src0_surface->surf.stride;
			iport0->uladdr			= gfx_getportbaseaddr( src0_surface,
										&param->src0.rect );
			iport0->sfmt 			= src0_surface->surf.pixel_format;
			iport0->uiglobalalpha 	= 0xffffffff;

			/* download palette if surface is index color format */
			if (src0_surface->surf.pixel_format <= LX_GFX_PIXEL_FORMAT_INDEX_8)
			{
				if ( src0_surface->pal )
				{
					gfx_downloadpalette( 0 , src0_surface->palsize,
						src0_surface->pal);
				}
			}
		}

		iport0->bbitmaskenable 	= FALSE;
		iport0->bcocenable 		= FALSE;
		iport0->bcscenable 		= FALSE;
		iport0->bcolorkeyenable = FALSE;

		/* enable/disable premulitication of source. */
		if ( param->src0.port_flag & LX_GFX_PORT_FLAG_PREMULTIPLY )
		{
			fsrc0premultiply = TRUE;
		}

		/* enable/disable colorkey when input color is
			in inside color key range. */
		if ( param->src0.port_flag & LX_GFX_PORT_FLAG_COLORKEY_INSIDE )
		{
			iport0->bcolorkeyenable = 0x1;
			iport0->bcolorkeymode 	= 0x0;
		}
		/* enable/disable colorkey when input color is
			outside color key range. */
		else if ( param->src0.port_flag & LX_GFX_PORT_FLAG_COLORKEY_OUTSIDE )
		{
			iport0->bcolorkeyenable = 0x1;
			iport0->bcolorkeymode	= 0x1;
		}

		/* enable/disable bitmask color */
		if ( param->src0.port_flag & LX_GFX_PORT_FLAG_BITMASK )
		{
			iport0->bbitmaskenable 	= 0x1;
			iport0->ulbitmask 		= param->src0.bitmask;
		}

		if ( param->src0.port_flag & LX_GFX_PORT_FLAG_COC_CTRL )
		{
			iport0->bcocenable		= 0x1;
			memcpy( &iport0->ulcocctrl, &param->src0.coc_ctrl, 0x4 );
		}

		if ( param->src0.port_flag & LX_GFX_PORT_FLAG_CSC_CTRL )
		{
			iport0->bcscenable		= 0x1;
			iport0->scscsel			= param->src0.csc_ctrl;
		}

		iport0->ulreplacecolor 	= param->src0.colorkey_tgt_color;
		iport0->ulkeylow 		= param->src0.colorkey_lower_limit;
		iport0->ulkeyhigh 		= param->src0.colorkey_upper_limit;
	}

	/*	parse src1 port configuration
	 *	-- alomost GFX operations needs read port0 (src1)
	 *	except OP_MODE_ONE_SRC_PORT (SRCBLT) mode.
	 *	raxis.lim (2010/05/28)
	 *	[TODO] need some code to convert 8bpp or 16bpp surface to 32bpp ????
	 *
	 *
	 */
	if ( param->op_mode == LX_GFX_OP_MODE_TWO_SRC_PORT ||
		 param->op_mode == LX_GFX_OP_MODE_THREE_SRC_PORT ||
		 param->op_mode == LX_GFX_OP_MODE_PACK_Y_CBCR ||
		 param->op_mode == LX_GFX_OP_MODE_PACK_Y_CB_CR ||
		 param->op_mode == LX_GFX_OP_MODE_PACK_CB_CR )
	{
		iport1->sport 	= EPORT1;
		iport1->sendian	= ELITTLE;

		if ( param->src1.surface_type == LX_GFX_SURFACE_TYPE_COLOR )
		{
			iport1->usstride		= 0x0;
			iport1->uladdr			= 0x0;
			iport1->sfmt			= LX_GFX_PIXEL_FORMAT_INDEX_0;
			iport1->uiglobalalpha 	= param->src1.surface_color.pixel_value;
		}
		else
		{
			iport1->usstride		= (UINT16)src1_surface->surf.stride;
			iport1->uladdr			= gfx_getportbaseaddr( src1_surface,
										&param->src1.rect );
			iport1->sfmt 			= src1_surface->surf.pixel_format;
			iport1->uiglobalalpha 	= 0xffffffff;

			/* download palette if surface is index color format */
			if (src1_surface->surf.pixel_format <= LX_GFX_PIXEL_FORMAT_INDEX_8)
			{
				if ( src1_surface->pal )
				{
					gfx_downloadpalette( 1 , src1_surface->palsize,
						src1_surface->pal);
				}
			}
		}

		iport1->bbitmaskenable 	= FALSE;
		iport1->bcocenable 		= FALSE;
		iport1->bcolorkeyenable = FALSE;
		iport1->bcscenable 		= FALSE;

		/* enable/disable premulitication of source. */
		if ( param->src1.port_flag & LX_GFX_PORT_FLAG_PREMULTIPLY )
		{
			fsrc1premultiply = TRUE;
		}

		/* enable/disable colorkey when input color is
			in inside color key range. */
		if ( param->src1.port_flag & LX_GFX_PORT_FLAG_COLORKEY_INSIDE )
		{
			iport1->bcolorkeyenable = 0x1;
			iport1->bcolorkeymode 	= 0x0;
		}
		/* enable/disable colorkey when input color is
			outside color key range. */
		else if ( param->src1.port_flag & LX_GFX_PORT_FLAG_COLORKEY_OUTSIDE )
		{
			iport1->bcolorkeyenable = 0x1;
			iport1->bcolorkeymode	= 0x1;
		}

		/* enable/disable bitmask color */
		if ( param->src1.port_flag & LX_GFX_PORT_FLAG_BITMASK )
		{
			iport1->bbitmaskenable 	= 0x1;
			iport1->ulbitmask 		= param->src1.bitmask;
		}

		if ( param->src1.port_flag & LX_GFX_PORT_FLAG_COC_CTRL )
		{
			iport1->bcocenable		= 0x1;
			memcpy( &iport1->ulcocctrl, &param->src1.coc_ctrl, 0x4 );
		}

		if ( param->src1.port_flag & LX_GFX_PORT_FLAG_CSC_CTRL )
		{
			iport1->bcscenable		= 0x1;
			iport1->scscsel			= param->src1.csc_ctrl;
		}

		iport1->ulreplacecolor 	= param->src1.colorkey_tgt_color;
		iport1->ulkeylow 		= param->src1.colorkey_lower_limit;
		iport1->ulkeyhigh 		= param->src1.colorkey_upper_limit;
	}

	/*	parse src2 port configuration
	 *	-- 	alomost GFX operations doesn't needs read2 port (src2)
	 *	except OP_MODE_THREE_SRC_PORT, OP_MODE_PACK_Y_Cb_Cr
	 *		and OP_MODE_PACK_Cb_Cr
	 *
	 *	raxis.lim (2010/05/28)
	 *	[TODO] need some code to convert 8bpp or 16bpp surface to 32bpp ????
	 *
	 */
	if ( param->op_mode == LX_GFX_OP_MODE_THREE_SRC_PORT ||
		 param->op_mode == LX_GFX_OP_MODE_PACK_Y_CB_CR ||
		 param->op_mode == LX_GFX_OP_MODE_PACK_CB_CR )
	{
		iport2	= (GFX_PORT_CONFIGURE_T *)OS_Malloc(sizeof(GFX_PORT_CONFIGURE_T));
		memset( iport2, 0x0, sizeof(GFX_PORT_CONFIGURE_T));

		iport2->sport 	= EPORT2;
		iport2->sendian	= ELITTLE;

		if ( param->src2.surface_type == LX_GFX_SURFACE_TYPE_COLOR )
		{
			iport2->usstride		= 0x0;
			iport2->uladdr			= 0x0;
			iport2->sfmt			= LX_GFX_PIXEL_FORMAT_INDEX_0;
			iport2->uiglobalalpha 	= param->src2.surface_color.pixel_value;
		}
		else
		{
			iport2->usstride		= (UINT16)src2_surface->surf.stride;
			iport2->uladdr			= gfx_getportbaseaddr( src2_surface,
										&param->src2.rect );
			iport2->sfmt 			= src2_surface->surf.pixel_format;
			iport2->uiglobalalpha 	= 0xffffffff;
		}
	}

#if 0
	/* get current dst port configuration */
	gfx_getoutputconfigure(oport);
#endif

	/*	parse dst port configuration
	 *
	 *	-- dst port should always be surface !!
	 **/
	{
		if ( param->dst.port_flag & LX_GFX_PORT_FLAG_DEMULTIPLY )
		{
			fdstdemultiply = TRUE;
		}

		if ( param->dst.port_flag & LX_GFX_PORT_FLAG_COC_CTRL )
		{
			oport->bcocenable = 0x1;
			memcpy( &oport->ulcocctrl, &param->dst.coc_ctrl, 0x4 );
		}

		if ( param->dst.port_flag  & LX_GFX_PORT_FLAG_CSC_CTRL )
		{
			oport->bcscenable		= 0x1;

			/* coefficient for YUV -> RGB */
			if ( param->dst.csc_ctrl == LX_CSC_COEF_SEL_0 ||
				param->dst.csc_ctrl == LX_CSC_COEF_SEL_1 )
			{
				oport->uicsccoef[0] = cscyuv2argb_coef[0];
				oport->uicsccoef[1] = cscyuv2argb_coef[1];
				oport->uicsccoef[2] = cscyuv2argb_coef[2];
				oport->uicsccoef[3] = cscyuv2argb_coef[3];
				oport->uicsccoef[4] = cscyuv2argb_coef[4];
				oport->uicsccoef[5] = cscyuv2argb_coef[5];
				oport->uicsccoef[6] = cscyuv2argb_coef[6];
				oport->uicsccoef[7] = cscyuv2argb_coef[7];
			}
			/* coefficient for RGB -> YUV */
			else
			{
/*				// BT.709
				oport->uicsccoef[0] = 0x09D200FD;
				oport->uicsccoef[1] = 0x02EDFA98;
				oport->uicsccoef[2] = 0x0706FE63;
				oport->uicsccoef[3] = 0xF99EFF5D;
				oport->uicsccoef[4] = 0x07060000;
				oport->uicsccoef[5] = 0x00000000;
				oport->uicsccoef[6] = 0x00000010;
				oport->uicsccoef[7] = 0x00800080;

				// BT.601 full
				oport->uicsccoef[0] = 0x08100191;
				oport->uicsccoef[1] = 0x041CFB59;
				oport->uicsccoef[2] = 0x0706FDA2;
				oport->uicsccoef[3] = 0xFA1DFEDE;
				oport->uicsccoef[4] = 0x07060000;
				oport->uicsccoef[5] = 0x00000000;
				oport->uicsccoef[6] = 0x00000010;
				oport->uicsccoef[7] = 0x00800080;
*/
	    			 // BT.601 comformant
				// BT.601 conformance
				oport->uicsccoef[0] = 0x08100191;
				oport->uicsccoef[1] = 0x041CFB59;
				oport->uicsccoef[2] = 0x0706FDA2;
				oport->uicsccoef[3] = 0xFA1DFEDE;
				oport->uicsccoef[4] = 0x07060000;
				oport->uicsccoef[5] = 0x00000000;
				oport->uicsccoef[6] = 0x00000010;
				oport->uicsccoef[7] = 0x00800080;
			}
		}
		else /* no CSC. bypass ? */
		{
			oport->uicsccoef[0]		= 0x10000000;
			oport->uicsccoef[1]		= 0x00000000;
			oport->uicsccoef[2]		= 0x10000000;
			oport->uicsccoef[3]		= 0x00000000;
			oport->uicsccoef[4]		= 0x10000000;
			oport->uicsccoef[5]		= 0x00000000;
			oport->uicsccoef[6]		= 0x00000000;
			oport->uicsccoef[7]		= 0x00000000;
		}

 		/* [TODO] need to verify oport configuration */
		oport->usstride = (UINT16)dst_surface->surf.stride;

		oport->uladdr = gfx_getportbaseaddr( dst_surface, &param->dst.rect );
		oport->ushsize = param->dst.rect.w;	/* sub region display */
		oport->usvsize = param->dst.rect.h;

		if ( dst_surface->surf.pixel_format == LX_GFX_PIXEL_FORMAT_CBCR_420 )
		{
			oport->ushsize 	*= 2;
			oport->usvsize 	*= 2;
		}
		if ( dst_surface->surf.pixel_format == LX_GFX_PIXEL_FORMAT_CBCR_422 )
		{
			oport->ushsize 	*= 2;
		}

		/* if current OP mode is 'WRITE ONLY',
			set pixel format and global alpha based on src0 port */
		if ( param->op_mode == LX_GFX_OP_MODE_WRITE )
		{
			oport->sfmt 	= LX_GFX_PIXEL_FORMAT_INDEX_0;
			oport->ulgalpha = param->src0.surface_color.pixel_value;
		}
		else
		{
			oport->sfmt 	= dst_surface->surf.pixel_format;
			oport->ulgalpha = 0xFFFFFFFF;
		}

		oport->sendian	= ELITTLE;
	}

	/* configure operation */
#if 0
	gfx_getoperationstatus(operate);
#endif
	operate->sport = param->op_mode;
	operate->uscoef[0] = 0x080;		/* test value ? */
	operate->uscoef[1] = 0x0C0;		/* test value ? */
	operate->uscoef[2] = 0x100;		/* test value ? */
	operate->uscoef[3] = 0x040;		/* test value ? */

	/* configure blend parameter */
#if 0
	gfx_getblendconfigure(blend);
#endif

	/* set blend param only for BLEND request */
	blend->sout 		= param->out_sel;
	blend->sraster 		= param->rop;

	/* blend factor is valid only for BLEND mode */
	if ( param->out_sel == LX_GFX_OUT_SEL_BLEND )
	{
		blend->bpma0enable	= fsrc0premultiply;
			/* src0 port should be premultiplied ? */
		blend->bpma1enable	= fsrc1premultiply;
			/* src1 port should be premultiplied ? */
		blend->bxor0enable	= param->xor_color_en;
		blend->bxor1enable	= param->xor_alpha_en;
		blend->bdivenable	=  fdstdemultiply;
			/* dst port should be demultiplied ? */
		blend->usalpha_m0	= param->mux_alpha_m0;
		blend->usc_m0		= param->mux_color_m0;
		blend->salphablend	= param->comp_rule_alpha;
		blend->srblend		= param->comp_rule_color;
		blend->sgblend		= param->comp_rule_color;
		blend->sbblend		= param->comp_rule_color;

		blend->usa0_alpha 	= param->mux_factor0_alpha;
		blend->usb0_alpha 	= param->mux_factor1_alpha;
		blend->usa1_r		= param->mux_factor0_color;
		blend->usa2_g		= param->mux_factor0_color;
		blend->usa3_b		= param->mux_factor0_color;
		blend->usb1_r		= param->mux_factor1_color;
		blend->usb2_g 		= param->mux_factor1_color;
		blend->usb3_b 		= param->mux_factor1_color;

		blend->ulblendconstant = param->constant_color;
	}

	if (iport0)	gfx_setinputconfigure(iport0);
	if (iport1)	gfx_setinputconfigure(iport1);
	if (iport2)	gfx_setinputconfigure(iport2);

	gfx_setoutputconfigure(oport);
	gfx_setblendconfigure(blend);
	gfx_setoperationstatus(operate);

	memset( scaler, 0x0, sizeof(GFX_SCALER_CONFIGURE_T));

	/* make scaler command after the normal GFX command set */
	if ( param->scaler.mode == LX_GFX_SCALER_MODE_NONE )
	{
		scaler->cmd = GFX_SCALER_CMD_BYPASS;
		gfx_setscalerconfigure( scaler );
	}
	else
	{
		param->dst.rect.w	= param->src0.rect.w;
		param->dst.rect.h	= param->src0.rect.h;

		scaler->cmd 		= GFX_SCALER_CMD_START;
		scaler->filter 		= GFX_SCALER_FILTER_CUSTOM;
		scaler->in_dim.w	= param->scaler.input.w;
		scaler->in_dim.h	= param->scaler.input.h;
		scaler->out_dim.w	= param->scaler.output.w;
		scaler->out_dim.h	= param->scaler.output.h;

		gfx_setscalerconfigure( scaler );
	}

	/* raxis.lim (2010/05/25)
	 * GFX START command should be issued regardless of batch run mode.
	 * Application controls only "Batch Run" (flush) command.
	 */
	gfx_runstartcommand();

func_exit:
	if (iport0)	OS_Free(iport0);
	if (iport1)	OS_Free(iport1);
	if (iport2)	OS_Free(iport2);
	if (oport)	OS_Free(oport);
	if (operate)	OS_Free(operate);
	if (scaler)	OS_Free(scaler);
	if (blend)	OS_Free(blend);

	GFX_TRACE_END();

	return ret;
}

/** dump blend param for debug
 *
 * @param param [IN] GFX blend param to request
 *
 */
void gfx_dumpblitparam(LX_GFX_MANUAL_BLEND_CTRL_PARAM_T *param)
{
	printk( "[r0] 0x%08x,0x%x,0x%04x,0x%08x (%4d,%4d,%4d,%4d) [r1]  \
			0x%08x,0x%x,0x%04x,0x%08x (%4d,%4d,%4d,%4d) "
			"[wr] 0x%08x,0x%x,0x%04x,0x%08x (%4d,%4d,%4d,%4d)	\
			[op] %d %d 0x%08x [sc] %d (%4d,%4d) (%4d,%4d)\n",
			param->src0.port_flag,param->src0.surface_type,
			param->src0.surface_fd, param->src0.surface_color.pixel_value,
			param->src0.rect.x, param->src0.rect.y,
			param->src0.rect.w, param->src0.rect.h,
			param->src1.port_flag,param->src1.surface_type,
			param->src1.surface_fd, param->src1.surface_color.pixel_value,
			param->src1.rect.x, param->src1.rect.y,
			param->src1.rect.w, param->src1.rect.h,
			param->dst.port_flag, param->dst.surface_type,
			param->dst.surface_fd, param->dst.surface_color.pixel_value,
			param->dst.rect.x, param->dst.rect.y,
			param->dst.rect.w, param->dst.rect.h,
			param->op_mode, param->out_sel, param->constant_color,
			param->scaler.mode, param->scaler.input.w,
			param->scaler.input.h, param->scaler.output.w,param->scaler.output.h );
}

/**
 *
 * color space setting on out port
 *
 * @param	UINT8 *coef
 * @return	int	0 : OK , -1 : NOT OK
 *
 */
int gfx_setcolorspace(UINT32 *coef)
{
	GFX_OUT_CONFIGURE_T *oport;
	int ret = 0;
	int i;

	GFX_TRACE_BEGIN();
	oport = (GFX_OUT_CONFIGURE_T *)kmalloc(sizeof(GFX_OUT_CONFIGURE_T),
		GFP_KERNEL);

	gfx_getoutputconfigure(oport);

	for (i=0 ; i<8 ;i++) {
		cscyuv2argb_coef[i] = *coef;
		oport->uicsccoef[i] = *coef++;
	}
	gfx_setoutputconfigure(oport);
	GFX_TRACE_END();

	kfree(oport);

	return ret;
}

/**
 *
 * get command delay parameter which mean the execution time gap
 * between two commands
 *
 * @param	UINT32 *cmd_delay
 * @return	int	0 : OK , -1 : NOT OK
 *
 */
int gfx_getcommanddelay(UINT32 *cmd_delay)
{
	int ret = 0;

	GFX_TRACE_BEGIN();
	*cmd_delay = gfx_getcommanddelayreg();
	GFX_TRACE_END();

	return ret;
}


/**
 *
 * set command delay parameter which mean the execution time gap
 * between two commands
 *
 * @param	UINT32 cmd_delay
 * @return	int	0 : OK , -1 : NOT OK
 *
 */
int gfx_setcommanddelay(UINT32 cmd_delay)
{
	int ret = 0;

	GFX_TRACE_BEGIN();

	if (cmd_delay > 0x2ff)
	{
		GFXWARN("WARNING : the cmd delay is too big. should be under 0x2ff\n");
		cmd_delay = 0x2ff;
	}

	gfx_setcommanddelayreg((UINT16)(cmd_delay));

	GFX_TRACE_END();

	return ret;
}


/** set command run type
 *
 * @param	UINT32 LX_GFX_BATCH_RUN_MODE_T
 * @return	int	0 : OK , -1 : NOT OK
 *
 */
int gfx_setruncommand(LX_GFX_BATCH_RUN_MODE_T *cmd)
{
	int ret = RET_OK;

	GFX_TRACE_BEGIN();
	g_gfx_run_mode = *cmd;
	GFX_TRACE_END();

	return ret;
}

int	gfx_setgraphicsyncmode( LX_GFX_GRAPHIC_SYNC_MODE_T	mode)
{
	int ret = RET_OK;
	GFX_TRACE_BEGIN();
	g_gfx_sync_mode = mode;
	GFX_TRACE_END();

	return ret;
}

LX_GFX_GRAPHIC_SYNC_MODE_T	gfx_getgraphicsyncmode(void)
{
	return g_gfx_sync_mode;
}

/** get command run type
 *
 * @param	void
 * @return	LX_GFX_BATCH_RUN_MODE_T
 *
 */
LX_GFX_BATCH_RUN_MODE_T gfx_getruncommand(void)
{
	return g_gfx_run_mode;
}

/** calculate GFX sync timeout with very heuristic method.
 *
 *	default timeout value will be enough for almost GFX operation.
 *	but some big image requires more timeout value.
 */
UINT32 gfx_calcsynctimeout   ( LX_GFX_MANUAL_BLEND_CTRL_PARAM_T* blend_ctrl,
							UINT32 def_tm_out )
{
	SINT16	src_w, src_h, dst_w, dst_h;
	UINT32	src_area, dst_area;

	if ( blend_ctrl->scaler.mode == LX_GFX_SCALER_MODE_NONE )
	{
		src_w = 0;
		src_h = 0;
		dst_w = blend_ctrl->dst.rect.w;
		dst_h = blend_ctrl->dst.rect.h;
	}
	else
	{
		src_w = blend_ctrl->scaler.input.w;
		src_h = blend_ctrl->scaler.input.h;
		dst_w = blend_ctrl->scaler.output.w;
		dst_h = blend_ctrl->scaler.output.h;
	}

	src_area = src_w * src_h;
	dst_area = dst_w * dst_h;

#if 0
	if ( src_area <= 0xe1000 && dst_area <= 0xe1000 )
		{ /* do noting */ }
	else
		{ printk("[gfx] tm calc : 1 - %dx%d - %dx%d\n",
			src_w, src_h, dst_w, dst_h ); }
#endif

	/* 0xe1000 means 1280*720 */
	if ( src_area <= 0xe1000 && dst_area <= 0xe1000 )
		return def_tm_out;
	else
		return (def_tm_out<<2);
}

/**	wait until gfx core completes all CQs ( action handler for
 *	GFX_IOW_WAIT_FOR_SYNC )
 *	-- 	this function will be invoked by application since application shouild
 *		wait for GFX sync to access surface memory by CPU operation.
 *	--	application can call this function multiple times regardless of the
 *		real request count.
 *	--	driver should return this function when GFX core is idle
 *		( all CQs are processed ),
 *		otherwise driver should block application for msec_tm interval.
 *
 *	raxis.lim (2010/05/22) -- bug fix
 *	--	the objective of this function is not holding/resuming GFX operation
 *		function code should be re-implemented !!
 *
 *	raxis.lim (2010/05/27) -- more revision
 *	--	I decided not to use giGfxIntDone, instead read GFX register.
 *		register itself is the exact status information.
 *	--	gfx_isgfxidle() function created to hide the decision algorithm.
 *	--	more analysis is needed if decision algorithm is 100% correct or not.
 *	-- wait_event_xxx function may return RET_ERROR or RET_TIMEOUT
 *
 * @param	void
 * @return	int	0 : OK , -1 : NOT OK
 */
int gfx_waitsynccommand(UINT32 msec_tm )
{
	int	rc;
	int	i = 0;
/*	UINT32	tk0 = OS_GetHighResTicks(); */
#ifdef SW_WORKAROUND_INTERRUPT
	rc = 1;
	do {
		i++;
		mdelay(1);
		/* GFXINFO("[gfx] %d time is elapsed\n", i); */
		if (i==100){
			rc = -1;
			GFXINFO("[gfx] %d times is elapsed, so Timeout happened!!\n", i);
			break;
		}
	} while (gfx_isgfxidle()==FALSE);
#else
/*	GFX_RdFL( gfx_status2 );
	printk( "Before GFX status : 0x%08x\n", g_gfx_reg_cache->gfx_status2 );
*/
	rc = wait_event_interruptible_timeout(g_gfx_wq_done, gfx_isgfxidle(),
			msecs_to_jiffies(msec_tm) );

/*	GFX_RdFL( gfx_status2 );
	printk( "After GFX status : 0x%08x\n", g_gfx_reg_cache->gfx_status2 );
*/
#endif
	GFX_ASSERT( rc > 0 );
/*	printk("<gfx> %s : rc = %d. us = %d\n", __F__, rc,
	(UINT32)OS_GetHighResTimeElapsed(tk0)/1000 ); */

	/* print warning message when timeout detected.
	 * it's very critical error for GFX h/w and 'reset' recovery should
	 * be executed.
	 *
	 * raxis.lim (2012/12/27)
	 * Reset GFX HW after every GFX operation, this will prevent the possible
	 * GFX invalid output.  this is very rare case,
	 * but it's very critical cuz all GFX output image will be wrong.
	 */
	if ( rc <= 0 )
	{
		int	i;
		GFX_CMD_QUEUE_CTRL_T st;
		gfx_getcomqueuestatus(&st);
		GFXERR("gfx sync timeout. status=%d-%d-%d\n", st.bstatus,
			st.bcmdqstatus, st.bfull );

		gfx_stopcommand( );

		for ( i=100; i>0; i-- )
		{
			if ( gfx_isgfxidle() ) break;
			OS_MsecSleep(1);
		}
		if ( i == 0 ) GFXERR("crtical error !! GFX recovery failed !!\n");
	}
	else
	{
		/* GFXINFO("[GFX] GFX operation is done\n"); */

		gfx_stopcommand( );

		/*  TODO : Is the below code necessary? or where it is? */
		g_gfx_reg->gfx_intr_clear.gfx_intr_clear = 0x1;
	}

	/* the caller should check return value and do some recovery
	 * based on return value.
	 * for example, if return value is RET_ERROR,
	 * the caller should retry rendering again
	 */
	return (rc>0)? RET_OK: RET_ERROR;
}

/** wake up function
 *
 * @param	void
 * @return	int	0 : OK , -1 : NOT OK
 */
int gfx_wakeupwaitsync(void)
{
	if ( gfx_isgfxidle() ) wake_up_interruptible(&g_gfx_wq_done);
	return RET_OK;
}

/** get bits per pixel information from GFX pixel format
 *
 *	@param pxl_fmt [IN] GFX pixel format
 *	return bits per pixel
 */
UINT32 gfx_pxlfmt2bpp( LX_GFX_PIXEL_FORMAT_T pxl_fmt )
{
	switch ( pxl_fmt )
	{
/*		case LX_GFX_PIXEL_FORMAT_INDEX_0: */

		case LX_GFX_PIXEL_FORMAT_INDEX_1: 		return 1;
		case LX_GFX_PIXEL_FORMAT_INDEX_2: 		return 2;
		case LX_GFX_PIXEL_FORMAT_INDEX_4: 		return 4;
		case LX_GFX_PIXEL_FORMAT_INDEX_8:
    	case LX_GFX_PIXEL_FORMAT_ALPHA_8:

   		case LX_GFX_PIXEL_FORMAT_CB8_420__CR8_420:
    	case LX_GFX_PIXEL_FORMAT_CB8_422__CR8_422:
    	case LX_GFX_PIXEL_FORMAT_Y8__CB8_444__CR8_444:
												return 8;

		case LX_GFX_PIXEL_FORMAT_CBCR_420:
		case LX_GFX_PIXEL_FORMAT_CBCR_422:
		case LX_GFX_PIXEL_FORMAT_CBCR_444:

    	case LX_GFX_PIXEL_FORMAT_YCBCR655:
		case LX_GFX_PIXEL_FORMAT_AYCBCR2644:
		case LX_GFX_PIXEL_FORMAT_AYCBCR4633:
		case LX_GFX_PIXEL_FORMAT_AYCBCR6433:

		case LX_GFX_PIXEL_FORMAT_RGB565:
		case LX_GFX_PIXEL_FORMAT_ARGB1555:
		case LX_GFX_PIXEL_FORMAT_ARGB4444:
		case LX_GFX_PIXEL_FORMAT_ARGB6343:	return 16;

		case LX_GFX_PIXEL_FORMAT_Y0CB0Y1CR0_422:
		case LX_GFX_PIXEL_FORMAT_AYCBCR8888:
		case LX_GFX_PIXEL_FORMAT_ARGB8888:
		default:						 	return 32;
	}
}

/** get the physical base address for GFX operation.
 *
 *	@param surface [IN] GFX surface handle
 *	@param rect [IN] rendering area
 *	@return physical address for GFX operation
 */
UINT32 gfx_getportbaseaddr( GFX_SURFACE_OBJ_T* surface, LX_RECT_T* rect )
{
	UINT32	base_addr;
	int		x = rect->x;
	int		y = rect->y;

	base_addr  = (UINT32)surface->surf.phys_addr + y * surface->surf.stride;
	base_addr += x * ( gfx_pxlfmt2bpp( surface->surf.pixel_format ) >> 3 );

	return base_addr;
}

/*-------------------------------------------------------------------
	Implementation Group (HW HAL)
============================================================*/
int gfx_inithw (void)
{
    return g_gfx_hal.inithw( );
}

int gfx_runflushcommand(void)
{
	return g_gfx_hal.runflushcommand( );
}

int gfx_runstartcommand(void)
{
	return g_gfx_hal.runstartcommand( );
}

int gfx_pausecommand(void)
{
	return g_gfx_hal.runpausecommand( );
}

int gfx_resumecommand(void)
{
	return g_gfx_hal.runresumecommand( );
}

int gfx_stopcommand(void)
{
	return g_gfx_hal.runstopcommand( );
}

int gfx_swresetcommand(void)
{
	return g_gfx_hal.runswresetcommand();
}

void gfx_getcomqueuestatus(GFX_CMD_QUEUE_CTRL_T *queue)
{
	g_gfx_hal.getcomqueuestatus(queue);
}

bool gfx_isgfxidle(void)
{
	return g_gfx_hal.isgfxidle( );
}

void gfx_getoperationstatus(GFX_CMD_OP_T *operate)
{
	g_gfx_hal.getoperationstatus(operate);
}

void gfx_setoperationstatus(GFX_CMD_OP_T *operate)
{
	g_gfx_hal.setoperationstatus(operate);
}

void gfx_getinputconfigure(int iPort, GFX_PORT_CONFIGURE_T *port)
{
	g_gfx_hal.getinputconfigure( iPort, port );
}

void gfx_setinputconfigure(GFX_PORT_CONFIGURE_T *port)
{
	g_gfx_hal.setinputconfigure(port);
}

void gfx_setblendingout(GFX_ENUM_OUT_T type)
{
	g_gfx_hal.setblendingout(type);
}

void gfx_getblendconfigure( GFX_BLEND_CONFIGURE_T *blend)
{
	g_gfx_hal.getblendconfigure(blend);
}

void gfx_setblendconfigure( GFX_BLEND_CONFIGURE_T *blend)
{
	g_gfx_hal.setblendconfigure(blend);
}

void gfx_getoutputconfigure( GFX_OUT_CONFIGURE_T *port)
{
	g_gfx_hal.getoutputconfigure(port);
}

void gfx_setoutputconfigure(GFX_OUT_CONFIGURE_T *port)
{
	g_gfx_hal.setoutputconfigure(port);
}

void gfx_setscalerconfigure(GFX_SCALER_CONFIGURE_T* scaler)
{
	if ( g_gfx_hal.setscalerconfigure ) g_gfx_hal.setscalerconfigure(scaler);
}

int gfx_setclut(int port, int size , u32 *data)
{
	return g_gfx_hal.setclut(port,size,data);
}

void gfx_setcommanddelayreg(u16 delay)
{
	g_gfx_hal.setcommanddelayreg(delay);
}

u16 gfx_getcommanddelayreg(void)
{
	return g_gfx_hal.getcommanddelayreg( );
}

void gfx_setregister(u32 addr, u32 val)
{
	g_gfx_hal.setregister(addr, val);
}

u32 gfx_getregister(u32 addr)
{
	return g_gfx_hal.getregister(addr);
}

void gfx_dumpregister(void)
{
	return g_gfx_hal.dumpregister( );
}

int gfx_runsuspend(void)
{
	return g_gfx_hal.runsuspend( );
}

int gfx_runresume(void)
{
	return g_gfx_hal.runresume( );
}

/** @} */


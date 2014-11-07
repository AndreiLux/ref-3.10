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

#ifndef	_GFX_KAPI_H_
#define	_GFX_KAPI_H_

/*-------------------------------------------------------------------
	Control Constants
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
    File Inclusions
--------------------------------------------------------------------*/
#include "base_types.h"

#ifdef	__cplusplus
extern "C"
{
#endif /* __cplusplus */

/*-------------------------------------------------------------------
	Constant Definitions
--------------------------------------------------------------------*/
#define	GFX_IOC_MAGIC		'G'

/** maximum number of surface managed by GFX driver
 * raxis.lim (2010/05/13) -- GP2/3 requires minimum 2000 surface to be managed
 *raxis.lim (2010/11/03) -- GFX_MAX_SURFACE is moved from gfx_cfg.h to
 * gfx_kapi.h
 */
/* #define	GFX_MAX_SURFACE		2000 */
#define	GFX_MAX_SURFACE		100 /*                                   */

/*-------------------------------------------------------------------
	IO comand naming rule  : MODULE_IO[R][W]_COMMAND
--------------------------------------------------------------------*/
/**
@name GFX-SURFACE IOCTL List
ioctl list for gfx surface

@{
*/

/**
@def GFX_IO_RESET
	reset GFX devvices

*/

#define GFX_IO_RESET	_IO(GFX_IOC_MAGIC,  0 )

/**
 @def GFX_IOR_CHIP_REV_INFO
 	get gfx firmware version or software revision info
 	@param  version_info [OUT] pointer to LX_CHIP_REV_INFO_T
 	@return OK
*/
#define GFX_IOR_CHIP_REV_INFO	_IOR(GFX_IOC_MAGIC, 1, LX_CHIP_REV_INFO_T )

/**
 @def GFX_IOR_GET_CFG
	get current configuration for GFX device
 	@param  gfx_cfg [OUT] pointer to LX_GFX_CFG_T
	@return OK
*/
#define GFX_IOR_GET_CFG		_IOR(GFX_IOC_MAGIC, 2, LX_GFX_CFG_T )

/**
 @def GFX_IORW_ALLOC_SURFACE
 	allocate surface dynamically.
	@param surface_create_param [IN/OUT] pointer to
		LX_GFX_SURFACE_SETTING_PARAM_T
	@return RET_OK if success, none zero if otherwise.

 	This IOCTL allocates a surface dynamically based on the
 	requested surface setting.
	According to LG1150 document, engine should align width by 32 byte.
	So application should always check stride information when it read/write
	the surface directly.

 @def GFX_IOW_FREE_SURFACE
 	free surface.
	@param surface_fd [IN] pointer to surface id
	@return RET_OK if success, none zero if otherwise.

	All resources with the surface should be deallocated.
	Application shoule confirm the surface is note used anymore
	when this IOCTL called.
	So engine may return RET_ERROR when the surface is under busy state.

 @def GFX_IOW_SET_SURFACE_PALETTE
	set palette to surface
 	@param palette [IN] pointer to LX_GFX_PALETTE_T
	@return RET_OK if success, none zero if otherwise.

	set palette when surface is indexed color format.

	@todo Palette is used for surface with indexed color pixel format.
	This functon may return RET_ERROR when platte data contains
	different pixel format from
	surface or current surface is not configured to index color format.
	This restriction to be discussed later.
*/
#define GFX_IORW_ALLOC_SURFACE		\
	_IOWR(GFX_IOC_MAGIC, 3, LX_GFX_SURFACE_SETTING_PARAM_T )
#define GFX_IOW_FREE_SURFACE _IOW (GFX_IOC_MAGIC, 4, u32 )
#define GFX_IOW_SET_SURFACE_PALETTE	\
	_IOW (GFX_IOC_MAGIC, 6, LX_GFX_SURFACE_PALETTE_PARAM_T )
#define GFX_IOR_GET_SURFACE_PALETTE		\
	_IOR (GFX_IOC_MAGIC, 7, LX_GFX_SURFACE_PALETTE_PARAM_T )

/**
 @def GFX_IOW_SYNC_SURFACE
	(obsolete) flush cache change related to the surface
	@note DO NOT use this ioctl

 @def GFX_IORW_GET_SURFACE_MEM_INFO
	get the memory information of the surface.

	@param mem_info_param [IN] pointer to LX_GFX_SURFACE_MEM_INFO_PARAM_T
	@return RET_OK if success, none zero if otherwise.

	This ioctl gets the physical memory information, offset from base address
	of surface memory area.
	Application should know the offset information to call mmap() system call.\n
	Remember that physical memory and offset information is never used
	in application. It is used for another ioctl or system call.\n
	DTV software doesn't need to know this information, so this ioctl should
	be called at the internal kadaptor module.

	@note kadaptor should support surface_mmap, surface_munmap.
*/
#define GFX_IOW_SYNC_SURFACE		\
	_IOW ( GFX_IOC_MAGIC, 10, u32 )
#define GFX_IORW_GET_SURFACE_MEM_INFO		\
	_IOWR( GFX_IOC_MAGIC, 11, LX_GFX_SURFACE_MEM_INFO_PARAM_T )


/*  GFX Power Gating */
#define GFX_IOR_GET_POWERSTATUS		_IOR( GFX_IOC_MAGIC, 12, u32 )
#define GFX_IOW_SET_POWERSTATUS		_IOW( GFX_IOC_MAGIC, 13, u32 )

/* For Dual GFX Use */
#define GFX_IOW_SET_DUAL_GFX		_IOW( GFX_IOC_MAGIC, 14, u32 )

/*  GFX Operation mode */
#define GFX_IOR_GET_OPER_MODE		_IOR( GFX_IOC_MAGIC, 15, u32 )
#define GFX_IOW_SET_OPER_MODE		_IOW( GFX_IOC_MAGIC, 16, u32 )


/** @} */

/*-------------------------------------------------------------------
	IO comand naming rule  : MODULE_IO[R][W]_COMMAND
--------------------------------------------------------------------*/

/**
@name GFX-BLITTER IOCTL List
ioctl list for gfx blitter
@{
*/

/** execute the manual blend
 *
*/
#define GFX_IOW_BLEND		\
	_IOW( GFX_IOC_MAGIC, 30, LX_GFX_MANUAL_BLEND_CTRL_PARAM_T )
#define GFX_IOW_MANUAL_BLEND	\
	_IOW( GFX_IOC_MAGIC, 31, LX_GFX_MANUAL_BLEND_CTRL_PARAM_T )

/** set color space conversion
 *
 *	This IOCTL set new color space conversion parameters for GFX engine.
 *
 * @note This IOCTL seems rarely used.
*/
#define GFX_IOW_SET_CSC_CONV_TBL	\
	_IOW( GFX_IOC_MAGIC, 33, LX_GFX_CSC_TBL_T )

/** get command delay between command queue request
 *
 */
#define GFX_IOR_GET_CMD_DELAY		_IOR( GFX_IOC_MAGIC, 34, u32 )

/** set command delay between command queue request
 *
 * Command delay means the delay in unit of clock. \n
 * Large value makes gfx engine slow. Small value makes gfx engine fast but
 * the whole system perfomance will be degraded since gfx engine
 * consumes large amount of memory bandwidth.
 *
 * @note Valid range is between 0 and 0x7ff (11bit).
 *		Device driver will accept maximum 0x7ff even though application
 *		requests the big delay value.\n
 * 		Device driver sould have the default command delay value
 *		and initialized with that value.\n
 *
 * @todo We should make the adaptive method to imporve the performance
 *		of both gfx engine and system.
 */
#define GFX_IOW_SET_CMD_DELAY		_IOW( GFX_IOC_MAGIC, 35, u32 )

/** set batch run mode
 *
 *	This IOCTL controls which module issues 'GFX_BATCH_RUN' command.
 *	If auto mode is set, driver generates GFX_BATCH_RUN whenever it is
 *	necessary. If manual mode is set, driver makes CQ and GFX_START
 *	for each GFX operation but GFX_BATCH_RUN is
 *	generated by application with GFX_IO_START_BATCH_RUN ioctl.
 *
 *	Application will change batch run mode to 'auto' when it generates
 *	many GFX operation in sequence.
 *	I expect that auto mode will be reduce CPU interrupt.
 *
 *	@note Driver shoule be started with default 'auto' mode \n
 *		Batch run mode will be changed in run time and driver shold
 *		meet this requirement.
 *
 *	@param batch_run_mode [IN] batch run mode
 *	@return OK
 */
#define	GFX_IOW_SET_BATCH_RUN_MODE	\
	_IOW( GFX_IOC_MAGIC, 36, LX_GFX_BATCH_RUN_MODE_T )

/** get batch run mode
 *	@param batch_run_mode [OUT] batch run mode
 *	@return OK
 */
#define GFX_IOR_GET_BATCH_RUN_MODE	\
	_IOR( GFX_IOC_MAGIC, 37, LX_GFX_BATCH_RUN_MODE_T )

/** generate GFX_BATCH_RUN command.
 *	@param none
 *	@return OK if success, NOT_OK otherwise
 *
 *	@note This IOCTL should be called only when batch run mode is 'auto'.
 */
#define	GFX_IO_START_BATCH_RUN			_IO(  GFX_IOC_MAGIC, 38 )

/** set graphic sync mode
 *
 *
 */
#define	GFX_IOW_SET_GRAPHIC_SYNC_MODE	\
	_IOW( GFX_IOC_MAGIC, 39, LX_GFX_GRAPHIC_SYNC_MODE_T )

/** check if all CQs are processed and wait until ALL CQs are processed.
 *
 *	This IOCTL should block the calling process when timeout value is greater
 *	than zero and GFX are not processed all CQs.
 *	When timeout value is 0, driver should not block the calling process
 *	and just return current status ( OK or ERR_TIMEOUT )
 *
 *	@param timeout [IN] waiting time in unit of msec time.
 *	@return OK if All CQs processed, RET_TIMEOUT if timeout elapsed,
 *	NOT_OK otherwise
 *
 *	@note
 */
#define GFX_IOW_WAIT_FOR_SYNC			_IOW( GFX_IOC_MAGIC, 40, u32 )

/** get gfx memory information
 *
 *
 */
#define GFX_IOR_GET_MEM_STAT	_IOR( GFX_IOC_MAGIC, 50, LX_GFX_MEM_STAT_T)

/*-------------------------------------------------------------------
	Macro Definitions
--------------------------------------------------------------------*/
#define GFX_IOC_MAXNR					100

/*-------------------------------------------------------------------
    Type Definitions (Common)
--------------------------------------------------------------------*/

/** surface position definition
 *
 *	position is defined as the coordinate point or offset from zero point
 *	such as x, y, z.\n
 */
typedef SINT16	LX_GFX_POSITION_T;

/** surface dimension definition
 *
 *	dimension is defined as the length information such as width, height.\n
 */
typedef SINT16	LX_GFX_DIMENSION_T;

/** GFX CFG
 *
 *	current GFX device configuration
 *
 */
typedef struct
{
	/** H/W supports scaler or not ?
	 *
	 */
	u32		b_hw_scaler:1,
			xxx:31;

	/** default cmd delay for surface to surface blit
	 *	if POSD bandwidth problem occur during GFX blit, you should
	 *	adjust cmd delay value
	 */
	u16		surface_blit_cmd_delay;

	/** default cmd delay for surface to POSD blit.
	 *	if POSD bandwidth problem occur during GFX blit, you should adjust
	 *	cmd delay value
	 */
	u16		screen_blit_cmd_delay;

	/** default GFX sync wait timeout
	 *	during GFX H/W read, process & writes data to memory,
	 *	application should wait for its completion.
	 *	this value should be tuned by ad doc test for the best perfomance.
	 *	too low value may lead to the frequent "gfx sync timeout" problem.
	 * 	too long value may lead to some graphic still(stuck)
	 *	during heady memory bandwidth.
	 */
	u32		sync_wait_timeout;

	/** retry count when GFX timoeut occur
	 *	currently, this value is fixed to 2
	 */
	u8		sync_fail_retry_count;
	u8		rsvd[7];

	u32		power_status;
	struct
	{
		/** maximum surface width supported by GFX H/W. minimum value is 1 */
		u16	max_surface_width;
		/** maximum surface stride supported by GFX H/W. minimum value is 1 */
		u16	max_surface_stride;
		/** minimum width of scaler H/W */
		u16	min_scaler_input_width;
		u32	rsvd[6];	/** reserved field = 24 byte */
	}
	hw_limit;

	struct
	{
		u8	bad_dst_addr_stuck;
		u8	scaler_read_buf_stuck;
		u8	srcblt_op_stuck;
		u8	write_op_stuck;
		u8	rsvd[12];	/** reserved field = 12 byte */
	}
	workaround;
}
LX_GFX_CFG_T;


/** GFX color space
 *
 */
typedef enum
{
	LX_GFX_COLOR_SPACE_RGB	= 0x0,		/** RGB color space */
	LX_GFX_COLOR_SPACE_YUV	= 0x1,		/** YUV(YCbCr) color space */
}
LX_GFX_COLOR_SPACE_T;

typedef enum
{
	LX_GFX_SURFACE_CACHE_OP_INVAL = 0x0,
	LX_GFX_SURFACE_CACHE_OP_CLEAN = 0x1,
}
LX_GFX_SURFACE_CACHE_OP_T;

/**	GFX pixel format
 *
 * Belows are the detail information\n
 *
 * @li @b LX_GFX_PIXEL_FORMAT_INDEX_0 \n\n
 * This pixel format is not real but virtual to be used for the blit operation.
 * If you want to fill the contstant color to the surface, you need to
 * set this pixel format to GFX src port.
 * @see LG_GFX_SURFACE_TYPE_T::LX_GFX_SURFACE_TYPE_NONE \n
 *      LG_GFX_BLEND_CMD_T::LX_GFX_BLEND_CMD_WRITE
 *
 * @li @b LX_GFX_PIXEL_FORMAT_INDEX_1 \n
 * @li @b LX_GFX_PIXEL_FORMAT_INDEX_2 \n
 * @li @b LX_GFX_PIXEL_FORMAT_INDEX_4 \n
 * @li @b LX_GFX_PIXEL_FORMAT_INDEX_8 \n\n
 * These pixel formats represent indexed color.
 * 1bit is occuplied to represent one pixel for INDEX_1.
 * 2bit for INDEX_2, 4bit for INDEX_4 and 1byte(8bit) for INDEX_8.\n
 * Recently, INDEX_1, INDEX_2 and INDEX_4 seems to be rarely used.
 *
 * @li @b LG_GFX_PIXEL_FORMAT_ALPHA_8 \n\n
 * This pixel format is used to describe the surface containg only
 * alpha channel.
 * @todo Let's think about the usage of this pixel format.
 *
 *
 * @li @b LX_GFX_PIXEL_FORMAT_YCbCr_444
 * @li @b LX_GFX_PIXEL_FORMAT_Cb8Cr8_422
 * @li @b LX_GFX_PIXEL_FORMAT_Cb8Cr8_420
 *
 * @li @b LX_GFX_PIXEL_FORMAT_YCBCR655
 * @li @b LX_GFX_PIXEL_FORMAT_AYCBCR2644
 * @li @b LX_GFX_PIXEL_FORMAT_AYCBCR4633
 * @li @b LX_GFX_PIXEL_FORMAT_AYCBCR6433
 * @li @b LX_GFX_PIXEL_FORMAT_CBCR_420
 *
 * @li @b LX_GFX_PIXEL_FORMAT_AYCBCR8888
 * @li @b LX_GFX_PIXEL_FORMAT_Y0Cb0Y1Cr_422
 *
 * @li @b LX_GFX_PIXEL_FORMAT_RGB565
 * @li @b LX_GFX_PIXEL_FORMAT_ARGB1555
 * @li @b LX_GFX_PIXEL_FORMAT_ARGB4444
 * @li @b LX_GFX_PIXEL_FORMAT_ARGB6343
 * @li @b LX_GFX_PIXEL_FORMAT_CBCR_422\n\n
 * TBD
 *
 *
 * @li @b LX_GFX_PIXEL_FORMAT_ARGB8888 \n\n
 * Each pixel is express as 4byte (1byte alpha, 1byte red, 1byte gree,
 * 1byte blue)
 *
 * @note
 *
 *  @li Defined value in GFX is different from OSD. So this type should be
 *	used in only surface operation.
 *  But I think the real pixel format is the same as that of OSD.
 *  For example ARGB888 in GFX is ARGB8888 in OSD.\n
 *
 * @li If you plan to use YCbCr color, please contact to the driver developer.
 *   He/She will help you how to treat YCbCr color space.
 *
*/
typedef enum
{
	LX_GFX_PIXEL_FORMAT_INDEX_0		= 0,
		/**< virtual pixel format indicating the constant color */

	LX_GFX_PIXEL_FORMAT_INDEX_1 	= 0x1,
		/**< 1bpp indexed color */
	LX_GFX_PIXEL_FORMAT_INDEX_2 	= 0x2,
		/**< 2bpp indexed color */
	LX_GFX_PIXEL_FORMAT_INDEX_4 	= 0x3,
		/**< 4bpp indexed color */
	LX_GFX_PIXEL_FORMAT_INDEX_8 	= 0x4,
		/**< 8bpp indexed color */
	LX_GFX_PIXEL_FORMAT_ALPHA_8 	= 0x5,
		/**< 8bpp, only alpha plane ?? */

	LX_GFX_PIXEL_FORMAT_Y8__CB8_444__CR8_444	= 0x6,
		/**< 8bpp, unpacked (Y8 / Cb8 for 444 / Cr8 for 444 )*/
	LX_GFX_PIXEL_FORMAT_CB8_422__CR8_422 		= 0x7,
		/**< 8bpp, unpacked (Cb8 for 422 / Cr8 for 422 ) */
	LX_GFX_PIXEL_FORMAT_CB8_420__CR8_420 		= 0x8,
		/**< 8bpp, unpacked (Cb8 for 420 / Cr8 for 420 )  */

	LX_GFX_PIXEL_FORMAT_YCBCR655		= 0x9,
		/**< 16bpp, Y6 Cb5 Cr5 */
	LX_GFX_PIXEL_FORMAT_AYCBCR2644 		= 0xa,
		/**< 16bpp, A2 Y6 Cb4 Cr4 */
	LX_GFX_PIXEL_FORMAT_AYCBCR4633 		= 0xb,
		/**< 16bpp, A4 Y6 Cb3 Cr3 */
	LX_GFX_PIXEL_FORMAT_AYCBCR6433 		= 0xc,
		/**< 16bpp, A6 Y4 Cb3 Cr3 */

	LX_GFX_PIXEL_FORMAT_CBCR_420 		= 0xd,
		/**< 16bpp, CbCr interleaved */
	LX_GFX_PIXEL_FORMAT_CBCR_422 		= 0x1d,
		/**< 16bpp, CbCr interleaved */
	LX_GFX_PIXEL_FORMAT_CBCR_444 		= 0x1f,
		/**< 16bpp, CbCr interleaved */

	LX_GFX_PIXEL_FORMAT_RGB565 			= 0x19,
		/**< 16bpp, R5 G6 B 5 */
	LX_GFX_PIXEL_FORMAT_ARGB1555		= 0x1a,
		/**< 16bpp, A1 R5 G5 B5 */
	LX_GFX_PIXEL_FORMAT_ARGB4444 		= 0x1b,
		/**< 16bpp, A4 R4 G4 B4 */
	LX_GFX_PIXEL_FORMAT_ARGB6343 		= 0x1c,
		/**< 16bpp, A6 R3 G4 B3 */

	LX_GFX_PIXEL_FORMAT_AYCBCR8888	 	= 0xe,
		/**< 32bpp, A8 Y8 Cb8 Cr8 */
	LX_GFX_PIXEL_FORMAT_Y0CB0Y1CR0_422	= 0xf,
		/**< 32bpp, YCbCr-422 interleaved (YUYV) */
	LX_GFX_PIXEL_FORMAT_ARGB8888 		= 0x1e,
		/**< 32bpp, A8 R8 G8 B8 */
}
LX_GFX_PIXEL_FORMAT_T;

/** GFX color representation
 *
 *	This struct is to used to pass color data between application
 *	and device driver.
 *
 * @note
 * @li Device driver should convert automatically when color data is different
 *	from the pixel format of surface.\n
 * @li It is stronly recommended color data has the same pixel format
 *	with surface.\n
*/
typedef struct
{
	LX_GFX_PIXEL_FORMAT_T	pixel_format;	/**< pixel format of color */
	u32						pixel_value;	/**< real color data */
}
LX_GFX_COLOR_T;

#if 0
/** general rectangle
 *
 */
typedef struct
{
	u16		x;		/*  x ( offset ) */
	u16		y;		/* y ( offset ) */
	u16		w;		/* width */
	u16		h;		/* height */
}
LX_RECTANGLE_T /* long form */, LX_RECT_T /* short form */ ;
#endif


/** structure for CLUT palette.
 *
 * Palette is used only when FB device/surface is configured to
 *indexed colors.\n
 * For 256 indexed colors, you should pass 256 palette data.
 * But it is possible for you to use less than 256 colors.\n
 * It is stronly recommended that pixel format in palette should be
 * the same as pixel format of FB device.
 * Each entry in palette_data is expressed as ARGB8888 or AYCbCr8888.
 *
 * @note\n
 * -# When insufficient palette data are provided, device driver should
 * preserve the other index data which is not covered by this palette data.\n
 * -# Device driver should copy palette_data field to local storage.
 *
 * @todo\n
 * -# Check what pixel format is for palette_data. Is it only ARGB8888 ?
 * Yes. Only ARGB8888 acceptible.
*/
typedef struct
{
#if 0 /* SPEC OUT */
	LX_GFX_PIXEL_FORMAT_T pixel_format; /**< pixel format for palette entry */
#endif
	int	palette_num;	/**< total number of palette entry */
	u32 *palette_data;	/**< pointer to palette data */
}
LX_GFX_PALETTE_T;

/** structure for GFX_IOW_SET_SURFACE_PALETTE ioctl.
 *
 *	@see GFX_IOW_SET_SURFACE_PALETTE
 */
typedef struct
{
	u32					surface_fd;			/**< surface handle */
	LX_GFX_PALETTE_T	palette;			/**< palette data */
}
LX_GFX_SURFACE_PALETTE_PARAM_T;


/*-------------------------------------------------------------------
    Type Definitions (Surface)
--------------------------------------------------------------------*/

/** Surface type information
 *
 * @li @b LX_GFX_SURFACE_TYPE_NONE \n
 * It means that surface doesn't have any physical buffer and it should not
 * treated as the normal sourface.
 * This type is used for blitting/wirting constant color such as fill,
 * blended_fill.
 *
 * @li @b LX_GFX_SURFACE_TYPE_MEM_BUFFER \n
 * Normal surface type. Real memory buffer is allocated in gfx engine.
 * So gfx engine should free memory buffer when free operation requested
 * by application. Almost surface operation will use this type.
 *
 * @li @b LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER \n
 * This surface type is supported to pack color from the
 * unpacked Y/C, Y/Cb/Cr, Cb/Cr data.
 * Real memory buffer is provided by the other module such as the
 * video decoder.
 * So device driver should not allocate/free this memory buffer.
 * <b>2010/02/23 : </b> OSD as target surface also uses this surface type
 * since OSD buffer is already allocated.\n
 *
 * Please refer to external_mem_ptr field in LX_GFX_SURFACE_SETTING_T.
 *
 * @note If surface type is NONE or EXTERN_MEM_BUFFER,
 *	GFX_IORW_MEM_MAP_SURFACE is not supported.
 */
typedef enum
{
	LX_GFX_SURFACE_TYPE_NONE				= 0,
		/**< -- virtual surface type (constant color) */
	LX_GFX_SURFACE_TYPE_COLOR				= 0,
		/**< -- virtual surface type (constant color) */
	LX_GFX_SURFACE_TYPE_MEM_BUFFER			= 1,
		/**< -- surface buffer is allocated in gfx engine */
	LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER	= 2,
		/**< -- surface buffer is preallocated */
}
LX_GFX_SURFACE_TYPE_T;

/** surface information for the creation and query.
 *
 *
 */
typedef struct
{
	/** surface fd
	 *
	 *	this value is needed when application searches surface
	 *	without surface_fd
	 */
	u32					surface_fd;

	/** surface type
	 *
	 * Only LX_GFX_SURFACE_TYPE_MEM_BUFFER and
	 *	LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER are allowed.\n
	 * DO NOT use LX_GFX_SURFACE_TYPE_CONST_NONE.
	 */
	LX_GFX_SURFACE_TYPE_T	type;

	/** pixel format for the surface.
	 *
	 *  @note DO NOT use LX_GFX_PIXEL_FORMAT_INDEX_0
	 *	( this is used in the internal GFX gine )
	 */
	LX_GFX_PIXEL_FORMAT_T	pixel_format;

	/** surface width
	 *
	 *  @note According to LG1150 document, available range is up to 8192.\n
	 */
	LX_GFX_DIMENSION_T		width;

	/** surface width
	 *
	 *  @note According to LG1150 document, available range is up to 8192.\n
	 *
	 */
	LX_GFX_DIMENSION_T		height;

	/** buffer alignment
	 *
	 *	GFX buffer should be aligned according to its pixel format.
	 *  GFX allows one byte alignment for 8bpp color, 2 byte for 16bpp color
	 *	and 4 byte for 32bpp color.
	 *
	 *	Alignment is expressed as power of 2.\n
	 *	For example, alignment value 0 means 1 byte aligned, value 1 means
	 *	2 byte aligend, value 2 means 4 byt aligned and so on.
	 *
	 *	@note Device driver may not use alignment information while creating
	 *	new surface.\n
	 *	But application should set this field accroding the pixel format.
	 *
	 *  <b>2010/02/23 : </b> alignment seems useless.
	 *	please DO NOT use this field.
	 */
	u32					alignment;

	/** buffer stride
	 *
	 * 	Normally stride means that byte size per line.
	 *	As you know, stride is calculated as width multiplied by byte size of
	 *	pixel format.
	 *	But system may require N byte aligend width. So application should
	 *	use stride information when reading/writing surface buffer directly.
	 *
	 *	@note This field is not used at surface creation.
	 *	So let this value to be initialized
	 *  to zero when creating surface.\n
	 *	This field is set to the exact value when querying the
	 *	surface information.
	 *
	 *  <b>2010/02/23 : </b> when surface type is
	 			LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER,
	 *	You should set the exact stride with the help of system engineer.
	 *	( In VDEC case, when width is 720 but stride may be 1024 since
	 *	vdec memory should be allocated by power of 2 rules )\n
	 * 	When surface type is LX_GFX_SURFACE_NONE, this field is not used\n
	 */
	u32					stride;

	/** preallocated memory buffer to be used as surface buffer.
	 *
	 *	This field is only valid when surface type is
	 *	LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER.
	 *	This field contains the preallocated memory buffer used as
	 *	surfface buffer.
	 *	So let this field to zero for other surface type.
	 *	Device driver should not allocate new buffer
	 *	but use this buffer as surface buffer.
	 *
	 *	Real value in memory pointer is "physical" memory address.
	 *	So application cannot use this field and should just pass to
	 *	gfx engine without any operation or modification.
	 *
	 *	This field is set to the exact value when querying the
	 *	surface information even though this field is not used in application.
	 *	Application may print surface information in debug code and
	 *	it will be helpful to device driver developer.\n
	 *
	 *	If surface type is LX_GFX_SURFACE_TYPE_NONE,
	 *	phy_memptr is always set to 0 ( NULL )
	 */
	void *phys_addr;

	/** preallocated mmap pointer
	 *	[note] this information is processed at kadaptor layer not passed
	 *	to the kernel driver.
 	 * 	[note] this information is only valid when surface memory is from
 	 *	the external memory area.
	 */
	void *mmap_ptr;
}
LX_GFX_SURFACE_SETTING_T;

/** surface creation/querying command parameter for IOCTL.
 *
 * @see GFX_IORW_ALLOC_SURFACE
 */
typedef struct
{
	u32			surface_fd;			/**< surface handle */
	LX_GFX_SURFACE_SETTING_T surface_setting; /**< surface information */
}
LX_GFX_SURFACE_SETTING_PARAM_T;

#if 0
typedef struct
{
#define	LX_GFX_SURFACE_QUERY_TYPE_SURFACE_FD		0
/* query by surface_fd */
#define	LX_GFX_SURFACE_QUERY_TYPE_SURFACE_OFFSET	1
/* query by surface_offset */
#define	LX_GFX_SURFACE_QUERY_TYPE_SURFACE_PHYS_ADDR	2
/* query by surface_phys_addr */
	u32						query_type;

	union
	{
		u32					surface_fd;
		u32					surface_offset;
		u32					surface_phys_addr;
	}
	query_data;

	LX_GFX_SURFACE_SETTING_T	surface_setting;/**< surface information */
}
LX_GFX_SURFACE_QUERY_PARAM_T;
#endif


/** surface memory information.
 *
 *	Remember that offset and length field are valid only
 *	when the surface is allocated
 *	at the surface memory area. In other words, if surface is virtually
 *	created using the external memory
 *	buffer, offset and length field may not contain the wrong information.
 *
 * @see GFX_IORW_GET_SURFACE_MEM_INFO \n
 * 		LX_GFX_SURFACE_MEM_INFO_PARAM_T
*/
typedef struct
{
	void 	*phys_addr;	/**< physical address of the surface */
	u32		offset;
			/**< memory offset information from the surface base address */
	u32		length;
			/**< allocated byte size of the surface (total buffer length) */
}
LX_GFX_PHYS_MEM_INFO_T;

/** surface memory parameter for GFX_IORW_GET_SURFACE_MEM_INFO
 *
 *
 * @see GFX_IORW_GET_SURFACE_MEM_INFO
 */
typedef struct
{
	u32						surface_fd;	/**< surface handle */
	LX_GFX_PHYS_MEM_INFO_T		surface_mem; /**< memory info */
}
LX_GFX_SURFACE_MEM_INFO_PARAM_T;


#if 0
/** surface deivce file control
 *
 *
 */
typedef struct
{
#define	LX_GFX_DEV_CTRL_NONE				0x00000
#define	LX_GFX_DEV_CTRL_MMAP_CACHED			0x00001
	u32	opmask;

	UINT8	resvd[8];
}
LX_GFX_DEV_FILE_CTRL_T;

/** cache control for each GFX surface
 *
 */
typedef struct
{
	LX_GFX_SURFACE_CACHE_OP_T op;

	u32	phys_addr;		/**< physical address */
	u32	virt_addr;		/**< virtual address returned by mmap */
	u32	length;			/**< memory length */
}
LX_GFX_SURFACE_CACHE_CTRL_T;
#endif

/*-------------------------------------------------------------------
    Type Definitions (graphics)
--------------------------------------------------------------------*/
/** GFX batch run mode
 *
 *	This type is used to control the behavior of GFX command queue.
 *
 *	@note Device driver is initialized with AUTO mode.
 *	@todo How to control GFX batch operation when manual mode ?
 */
typedef enum
{
	LX_GFX_BATCH_RUN_MODE_AUTO	= 0,
		/**< command queue is controlled by device driver */
	LX_GFX_BATCH_RUN_MODE_MANUAL
		/**< application has the right to control command queue */
}
LX_GFX_BATCH_RUN_MODE_T;

typedef enum
{
	LX_GFX_GRAPHIC_SYNC_MODE_AUTO = 0,
		/**< graphic sync is done inside device driver */
	LX_GFX_GRAPHIC_SYNC_MODE_MANUAL,
		/**< application should call GFX_IOW_WAIT_FOR_SYNC for graphic sync */
}
LX_GFX_GRAPHIC_SYNC_MODE_T;

/** port id defnition
 *
 *	Each port is the same as the one described in LG1150 GFX manual.\n
 *	Application should use the designated port for the graphic operation.
 *	Please refer to the LG1150 GFX manual.
 *
 *	In manual, there are three read port ( read0 ~ read2 ) and one write port.
 *	I will use "src" instead of "read" and "dst" instead of "write".
 *
 */
typedef enum
{
	LX_GFX_PORT_SRC0	= (1<<0),	/**< 1st src port */
	LX_GFX_PORT_SRC1	= (1<<1),	/**< 2nd src port */
	LX_GFX_PORT_SRC2	= (1<<2),	/**< 3rd src port */
	LX_GFX_PORT_DST		= (1<<3),	/**< dst port */
}
LX_GFX_PORT_ID_T;

/** GFX raster operation
 *
 * @note\n
 * -# Raster engine treats color value as just 32bit data.
 *    In other words, raster operation is the bit-wise operation.\n\n
 * -# When blitting, you can't combine raster operation with  operation
 *	( This is hardware restriction ).
 *	So if you need the raster operation with blend, you should request
 *	two blit operation.
 *	One is for raster operation, the other is blend operation.
 *	But I think it seems very difficult to get the desired result.\n
 * -# : LX_GFX_ROP_NONE is not real GFX value.
 *	Application will use this value when it doesn't need any ROP operation.
*/
typedef enum
{
	LX_GFX_ROP_NONE = 0,	/* logical symbol for NON ROP definition */

	LX_GFX_ROP_ZERO	= 0,			/*  0 */
	LX_GFX_ROP_SRC_AND_DST,			/*  src AND dst */
	LX_GFX_ROP_SRC_AND_NOT_DST,		/*  src AND ~dst */
	LX_GFX_ROP_SRC,					/*  src */
	LX_GFX_ROP_NOT_SRC_AND_DST,		/*  ~src AND dst */
	LX_GFX_ROP_DST,					/*  dst */
	LX_GFX_ROP_SRC_XOR_DST,			/*  src XOR dst */
	LX_GFX_ROP_SRC_OR_DST,			/*  src OR dst */
	LX_GFX_ROP_NOT_SRC_AND_NOT_DST,		/*  ~src AND ~dst = ~(src OR dst) */
	LX_GFX_ROP_NOT_SRC_XOR_DST,		/*  ~src XOR dst = ~(src XOR dst ) */
	LX_GFX_ROP_NOT_DST,				/*  ~dst */
	LX_GFX_ROP_SRC_OR_NOT_DST,		/*  src OR ~dst */
	LX_GFX_ROP_NOT_SRC,				/*  ~src */
	LX_GFX_ROP_NOT_SRC_OR_DST,		/*  ~src OR dst */
	LX_GFX_ROP_NOT_SRC_OR_NOT_DST,		/*  ~src OR ~dst = ~( src AND dst ) */
	LX_GFX_ROP_ONE,							/*  1 */

}
LX_GFX_ROP_T;

/** GFX port configuration.
 *
 * @li @b LX_GFX_PORT_FLAG_PREMULTIPLY \n\n
 *	This flag is used to convert source surface to premultiplied format
 *	during the blit operation.\n
 *	LG1150 performs premultification in the internal blend block and doesn't
 *	update surface itself.\n
 *	This flag should be used for the source port and the blend operation.
 *	In other words, this flag isn't effective for the raster operation or
 *	simple write, copy operation.\n
 *
 *
 * @li @b LX_GFX_PORT_FLAG_DEMULTIPLY \n\n
 *	This flag is used to convert premultipled blended image to
 *	non-premultiplied image.
 *	So this flag doesn't modify source surface.
 *	This flag should be used for the destination port and the blend operation.
 *	In other words, this flag isn't effective for the raster operation or
 *	simple write, copy operation.\n
 *
 *  @li @b LX_GFX_PORT_FLAG_COLORKEY_INSIDE
 *	@li @b LX_GFX_PORT_FLAG_COLORKEY_OUTSIDE \n\n
 *	These flags control color keying for the source surface.
 *	When this flag enabled, GFX engine checks whether every pixel is inside/
 *	outside color key range
 *	and replace input color with desired replace color if necessary.\n
 *	This flag should be used for the source surface.\n
 *\n
 *  belows is LG1150 color key algorithm.
 *\n
 * @code
 * if ( src_color(x,y) is inside/outside range )
 *    out_color(x,y) = replace_color;
 * else
 *    out_color(x,y) = src_color(x,y)
 *
 * @endcode
 * @see LX_GFX_PORT_T::colorkey_tgt_color \n
 * 		LX_GFX_PORT_T::colorkey_lower_limit \n
 * 		LX_GFX_PORT_T::colorkey_lower_limit \n
 *
 *
 * @li @b LX_GFX_PORT_FLAG_BITMASK \n\n
 * This flag controls the modification of each color bit with bitmask data.\n
 * LG1150 supports 32bit bitmask while reading 32bit pixel. So you can clear
 * the specified bits of
 * source color using bitmask data pattern.\n
 * This flag should be used for the source surface.\n
 * \n
 * Belows is LG1150 bitmask algorithm how to modify single source color.
 * \n
 * @code
 * for ( int i=0; i<32 ; i++ )
 * {
 *		if ( bitmask[i] ) new_src_pixel[i] = 0;
 *		else              new_src_pixel[i] = src_pixel[i];
 * }
 * @endcode
 * @see LX_GFX_PORT_T::bitmask\n
 *
 *
 * @note\n
 * -# DO not mix LX_GFX_PORT_FLAG_PREMULTIPLY and
 *	LX_GFX_PORT_FLAG_DEMULTIPLY to the same port.
 *	As described above, premultify is for the source port and demultifly is
 *	fot the destination port. \n
 * -# DO not mix LX_GFX_PORT_FLAG_COLORKEY_INSIDE and
 *	LX_GFX_PORT_FLAG_COLORKEY_OUTSIDE. \n
 * -# For hardware restriction, All flags for source port should not
 *	be set for src2 port.
 *	As you can see, src2 port doesn't have color key processor,
 *	bitmask processor, COC processor and CSC processor.
 *	Accordint to GFX manual, src2 port is used only for mask surface and
 *	YC interleaving.
 *
 */
typedef enum
{
	/** empty flag */
	LX_GFX_PORT_FLAG_NONE					= 0,

	/** enable/disable premulitication of source. */
	LX_GFX_PORT_FLAG_PREMULTIPLY			= (1<<0),

	/** enable/disable demultification.  */
	LX_GFX_PORT_FLAG_DEMULTIPLY				= (1<<1),

	/** enable/disable colorkey when input color is in inside color key range. */
	LX_GFX_PORT_FLAG_COLORKEY_INSIDE		= (1<<2),

	/** enable/disable colorkey when input color is outside color key range. */
	LX_GFX_PORT_FLAG_COLORKEY_OUTSIDE		= (1<<3),

	/** enable/disable bitmask color */
	LX_GFX_PORT_FLAG_BITMASK				= (1<<4),

	/** enable/disable color re-reorder (COC) */
	LX_GFX_PORT_FLAG_COLOR_ORDER_CTRL		= (1<<5),
	LX_GFX_PORT_FLAG_COC_CTRL				= (1<<5),

	/** enable/disable color space conversion (CSC) */
	LX_GFX_PORT_FLAG_COLOR_SPACE_CONV_CTRL	= (1<<6),
	LX_GFX_PORT_FLAG_CSC_CTRL				= (1<<6),
}
LX_GFX_PORT_FLAG_T;

/** GFX port configration
 *
 *	@note Device driver should perform the color space conversion
 *	automatically.\n
 *	 Even though LG1150 support the color order converson,
 *	we will not use that feature.  If you need the color space conversion,
 *	please contact the driver developer.\n
 */
typedef struct
{
	/** port flag to control the color key processor, bitmask processor
	 * and multiplying operation  in the blender block.
	 */
	LX_GFX_PORT_FLAG_T	port_flag;

	/** surface type ( one of constant color, gfx allocated source and
		externally allocated source */
	LX_GFX_SURFACE_TYPE_T	surface_type;

	/** surface handle
 	 *
  	 * This field is used when surface type is
  	 * LX_GFX_SURFACE_TYPE_MEM_BUFFER or
  	 * LX_GFX_SURFACE_TYPE_EXTERN_MEM_BUFFER.
  	 * Otherwise, this field should be set to 0x0.
	 */
	u32					surface_fd;

	/** surface color
	 *
	 * This field is used when surface type is LX_GFX_SURFACE_TYPE_NONE.
	 * Otherwise, this field should be set to 0x0.
	 */
	LX_GFX_COLOR_T			surface_color;

	/** Rectanlge sub-region inside surface.
	 *
	 *	Becuase LG1150 GFX doesn't support hardware clipping,
	 *  application should process software
	 *	clipping. So device driver may not check any clipping.\n
	 *
	 *	And LG1150 gfx doesn't support stretch blit, dst_rect.w and dst_rect.h
	 *	shoule be same as src_rect.w and src_rect.h.
	 * 	Device driver may return error when dst_rect.w and dst_rect.h is
	 *	different from src_rect.w and src_rect.h.\n
	 */
	LX_RECT_T				rect;

	/** replace color data if source pixel is inside/outside color key range */
	u32					colorkey_tgt_color;
	/** lower limit(range) for color key */
	u32					colorkey_lower_limit;
	/** upper limit(range) for color key */
	u32					colorkey_upper_limit;

	/** bitmask pattern when LX_GFX_PORT_FLAG_BITMASK enabled
	 *	@see LX_GFX_PORT_FLAG_BITMASK
	 */
	u32					bitmask;

	/** CSC control value is only valid for src0, src1 port */
#define	LX_CSC_COEF_SEL_0	0x0 /* YC(16~235) -> RGB	*/
#define	LX_CSC_COEF_SEL_1	0x1 /* YC(0~255) -> RGB		*/
#define	LX_CSC_COEF_SEL_2	0x2 /* RGB -> YC(16~235)	*/
#define	LX_CSC_COEF_SEL_3	0x3 /* RGB -> YC( 0~255)	*/
	u32					csc_ctrl;

	/** color order control data
	 *
	 *	in(24:31)	-- 0x0
	 * 	in(16:23)	-- 0x1
 	 *	in(08:15)	-- 0x2
	 *	in(00:07)	-- 0x3
	 *
	 *  raxis.lim (2010/12/16) GFX regiseter manuals seems to be wrong
	 *	... swap field order !!!!!!!
	 */
	struct
	{
#define	LX_COC_24_31	0x0
#define	LX_COC_16_23	0x1
#define	LX_COC_08_15	0x2
#define	LX_COC_00_07	0x3
		u32
			coc_00_07_after	:2,
			coc_08_15_after	:2,
			coc_16_23_after	:2,
			coc_24_31_after	:2,
					   	:8,
			coc_00_07_before:2,
			coc_08_15_before:2,
			coc_16_23_before:2,
			coc_24_31_before:2;	/* before pixel format conversion. only valid at L9 */
	}
	coc_ctrl;
}
LX_GFX_PORT_T;

/** Optional flag for blend operation
 *
 */
typedef enum
{
	/** no flag */
	LX_GFX_BLEND_FLAG_NONE				= 0x0,

	/** mask plane flag
	 *
	 * This flag controls whether GFX engine uses mask plane or not.
	 * When this flag enabled, mask plane is set to src2 port.\n
	 * With mask plane, You can modify source source pixel by pixel.\n
	 * As you know, both color key and bitmask processor in src0, src1 port is
	 * very simple but difficult to modify surface to your desired form.\n
	 * Mask plane enables you to do advanced control over source surface.\n
	 *
	 * Mask plane in src2 port is only combined with src0 port.
	 * combined surface is feeded to GFX internal raster/blend block.\n
	 *
	 * @todo mask flag seems to be a high technique for graphic operation.
	 *		 We should find what scenes need mask surface.
	 *
	 * @see LX_GFX_BLEND_T::src2
	 */
	LX_GFX_BLEND_FLAG_MASK_PLANE		= (1<<0),

	/** global alpha flag
	 *
	 * By enabling this flag, alpha channel in source surface should be
	 * ignored and use attached global alpha value.\n
	 * In other words, during blend operation device driver should use
	 * global_alpha value for all calculation.
	 *
	 * @note Global alpha is very command operation in the porter duff .
	 *	For example, Yahoo! TV Widget uses global alpha when
	 *	it requests SRC_OVER .\n
	 *	Global alpha enables us to implement the fade in, fade out with ease.
	 *
	 * @see LX_GFX_PORT_T::global_alpha
	 */
	LX_GFX_BLEND_FLAG_GLOBAL_ALPHA			= (1<<1),
}
LX_GFX_BLEND_FLAG_T;


/*-------------------------------------------------------------------
    Type Definitions (advanced graphics)
--------------------------------------------------------------------*/

/** structure defnition for blender control
 *
 *	Before using this structure port configuration needed. Refer to
 *	LX_GFX_PORT_T;
 *
 *	Each field of this structure is matched to GFX register.
 *	Please refer to LG1150 GFX register manual.
 *
 * @note This structure is used to control blender device directly
 *	by application.\n
 *	Use this structure for functional test and update device driver if you
 *	finished test.
 */
typedef struct
{
	LX_GFX_PORT_T		src0;			/**< 1st port of source surface */
	LX_GFX_PORT_T		src1;			/**< 2nd port of source surface */
	LX_GFX_PORT_T		src2;			/**< 3rd port of source surface */
	LX_GFX_PORT_T		dst;			/**< destination port */

	#define LX_GFX_OP_MODE_WRITE			0x0
	/* write such as simple fillrect */
	#define LX_GFX_OP_MODE_ONE_SRC_PORT		0x1
	/* srcblt such as simple surface copy */
	#define LX_GFX_OP_MODE_TWO_SRC_PORT		0x2
	/* rop or blend with two src port */
	#define LX_GFX_OP_MODE_THREE_SRC_PORT	0x3
	/* rop or blend with three src port */
	#define LX_GFX_OP_MODE_PACK_Y_CBCR		0x4
	#define LX_GFX_OP_MODE_PACK_Y_CB_CR		0x5
	#define LX_GFX_OP_MODE_PACK_CB_CR		0x6

	/** operationg mode
	 *
	 * @see LX_GFX_OP_MODE_T
	 */
	u32					op_mode:3,

	#define LX_GFX_OUT_SEL_SRC0_PORT		0x0		/* debug only */
	#define LX_GFX_OUT_SEL_SRC1_PORT		0x1		/* debug only */
	#define LX_GFX_OUT_SEL_SRC2_PORT		0x2		/* debug only */
	#define LX_GFX_OUT_SEL_CONST_COLOR		0x3
	#define LX_GFX_OUT_SEL_BLEND			0x4
	#define LX_GFX_OUT_SEL_ROP				0x5
	#define LX_GFX_OUT_SEL_INTERLEAVE		0x6

	/** output selection for dst alpha, color
	 *
	 * 	@note LX_GFX_OUT_SEL_SRC0_PORT ~ LX_GFX_OUT_SEL_SRC2_PORT is not used
	 *	in application.  This value is used only for SOC debugging.
	 */
							out_sel:3,

	/** raster operation mode
	 *
	 * 	@see LX_GFX_ROP_T
	 * 	@note please set LX_GFX_ROP_NONE when rop is not necessary
	 */
							rop:4,

	/** flag for XORing muxed output of src0 alpha and src1 alpha
	 *
	 * 	@note refer to xor0_en field in LG1150 GFX document
	 */
							xor_alpha_en:1,
	/** flag for XORing muxed output of src0 color and src1 color
	 *
	 * 	@note refer to xor1_en field in LG1150 GFX document
	 */
							xor_color_en:1,

	#define LX_GFX_MUX_ALPHA_M0_ALPHA0					0x0
	#define LX_GFX_MUX_ALPHA_M0_ALPHA0_X_CONST_ALPHA	0x1
	#define LX_GFX_MUX_ALPHA_M0_ALPHA0_X_MASK_ALPHA		0x2
	#define LX_GFX_MUX_ALPHA_M0_CONST_ALPHA				0x3

	/** mux output selection for alpha_mo
	 *
	 */
							mux_alpha_m0:2,

	#define LX_GFX_MUX_COLOR_M0_COLOR0					0x0
	#define LX_GFX_MUX_COLOR_M0_COLOR0_X_CONST_ALPHA	0x1
	#define LX_GFX_MUX_COLOR_M0_COLOR0_X_MASK_COLOR		0x2
	#define LX_GFX_MUX_COLOR_M0_COLOR0_X_CONST_COLOR	0x3

	/** mux output selection for color_m0
	 *
	 *
	 */
							mux_color_m0:2,

	#define LX_GFX_COMP_RULE_FACTOR						0x0
	#define LX_GFX_COMP_RULE_MIN						0x1
	#define LX_GFX_COMP_RULE_MAX						0x2
	#define LX_GFX_COMP_RULE_CONST						0x3

	/** composition rule selection
	 *
	 *	@note (raxis.lim 2010/03/23)
	 *	L8 support separate composition rule for each A,R,G,B field.
	 *	But normally we should apply the same composition rule for
	 *	each color field.
	 *	So comp_rule_color will be applied to the all color field.
	 *	If you need to assign the different composition rule, modify the
	 *	below data field. Refer LG1150 GFX manual.
	 */
							comp_rule_alpha:2,
							comp_rule_color:2,
	/* 12bit reserved */
							:10;

	#define LX_GFX_MUX_FACTOR_ZERO						0x0
	#define LX_GFX_MUX_FACTOR_ONE						0x1

	#define LX_GFX_MUX_FACTOR_SRC0_ALPHA				0x2
	#define LX_GFX_MUX_FACTOR_SRC0_COLOR				0x3
	#define LX_GFX_MUX_FACTOR_SRC1_ALPHA				0x4
	#define LX_GFX_MUX_FACTOR_SRC1_COLOR				0x5
	#define LX_GFX_MUX_FACTOR_CONST_ALPHA				0x6
	#define LX_GFX_MUX_FACTOR_CONST_COLOR				0x7

	#define LX_GFX_MUX_FACTOR_ONE_MINUS_SRC0_ALPHA		0x8
	#define LX_GFX_MUX_FACTOR_ONE_MINUS_SRC0_COLOR		0x9
	#define LX_GFX_MUX_FACTOR_ONE_MINUS_SRC1_ALPHA		0xa
	#define LX_GFX_MUX_FACTOR_ONE_MINUS_SRC1_COLOR		0xb
	#define LX_GFX_MUX_FACTOR_ONE_MINUS_CONST_ALPHA		0xc
	#define LX_GFX_MUX_FACTOR_ONE_MINUS_CONST_COLOR		0xd

	/** input data selection for the composition rule
	 *
	 *	@note (raxis.lim 2010/03/23)
	 *		L8 support separate mux factor for eatch A,R,G,B field.
	 *		But normally we should apply the same mux factor for
	 *		each color field.
	 *		So mux_factorX_color will be applied to the all color field.
	 *		If you need to assign the different mux factor, modify the
	 *		below data field. Refer LG1150 GFX manual.
	 */
	u32		mux_factor0_alpha:4,
			mux_factor1_alpha:4,
			mux_factor0_color:4,
			mux_factor1_color:4,

	/* 16bit reserved */
							:16;
	/** constant color for
	 *
	 * @note Refer to BLEND_CTRL_CONT (0x014c) register
	 */
	u32					constant_color;

	struct
	{
	/** stretch mode selection
	 *
	 *	if stretch mdoe is non-zero, stretch block is always working
	 *
	 *	@note (raxis.lim 2011/05/11)
	 *		stretch mode is only valid for L9 or higher chip revisioin.
	 *		since stretch block is located at the end of blend/rop block,
	 *		stretch is done after the gfx operation.
	 *		for simple scenario, stretch is only supported for the simple
	 *		copy operation.	refer to gfx_kadp.c
	 */
	#define LX_GFX_SCALER_MODE_NONE			0x0
	#define LX_GFX_SCALER_MODE_BILINEAR		0x1
		u8				mode;
		LX_DIMENSION_T		input;
		LX_DIMENSION_T		output;
	}
	scaler;
}
LX_GFX_MANUAL_BLEND_CTRL_T, LX_GFX_MANUAL_BLEND_CTRL_PARAM_T;

/** alpha conversion table between 16bpp and 32bpp.
 *
 *	LG1150 uses four level of alpha value for converting
 *	between 16bpp and 32bpp.
 *
 *	- alpha[0] : alpha value for level 0
 *	- alpha[1] : alpha value for level 1
 *	- alpha[2] : alpha value for level 2
 *	- alpha[3] : alpha value for level 3
 *
 *	@note You need to discuss the real alpha value with design team or
 *	chip engineer.\n
 *		I think this table seems not used since GP doesn't use
 *		16bpp surface anymore.\n
 *		Device driver should have the default table for alpha conversion
 *		and initialized with the default value.
 *
 *	@see GFX_IOW_SET_ALPHA_CONV_TBL
*/
typedef struct
{
	u8					alpha[4];		/* 4 data value */
}
LX_GFX_ALPHA_CONV_TBL_T;

/** coefficent table for color space conversion.
 *
 *	- coef[0] : coefficient value for [0]
 *	- coef[1] : coefficient value for [1]
 *	- coef[2] : coefficient value for [2]
 *	- coef[3] : coefficient value for [3]
 *	- coef[4] : coefficient value for [4]
 *	- coef[5] : coefficient value for [5]
 *	- coef[6] : coefficient value for [6]
 *	- coef[7] : coefficient value for [7]
 *
 * 	@note Device driver should have the default table for
 *	color space conversion and initialized with the default value.
 *
 * @see GFX_IOW_SET_CSC_CONV_TBL
 */
typedef struct
{
	u32					coef[8];		/**< 8 data value */
}
LX_GFX_CSC_TBL_T;

/** chroma filter mode
 *
 *
 *
 * @todo More reivew needed
 */
typedef enum
{
	LX_GFX_CHROMA_FILTER_MODE_REPEAT 		= 0,
	LX_GFX_CHROMA_FILTER_MODE_2_TAP_AVG 	= 1,
	LX_GFX_CHROMA_FILTER_MODE_4_TAP_FILTER 	= 2,
}
LX_GFX_CHROMA_FILTER_MODE_T;

/** coefficient table for chroma filter
 *
 *	This data is used only when filter mode is
 *	LX_GFX_CHROMA_FILTER_MODE_4_TAP_FILTER.
 *	Device driver uses only 12bit data for each coefficient.
 *
 * @note Device driver should have the default table for the chroma filter
 *	and initialized with the default value.
 *
 * @see GFX_IOW_SET_CHROMA_FILTER_TBL
 *
 * @todo More reivew needed
*/
typedef struct
{
	u16				coef[4];
}
LX_GFX_CHROMA_FILTER_TBL_T;

/**	brief information about GFX memory usage
 *
 *	Application can use this information for tuing memory size or someting
 *	like that...
 *
 *	@see GFX_IOR_GET_MEM_STAT
 *
 */
typedef struct
{
	u32	surface_max_num;/**< maximum number which is managed by kdriver */
	u32	surface_alloc_num;		/**< current allocated surface num */

	void*surface_mem_base;		/**< base address of surface memory */
	u32	surface_mem_length;		/**< length of surface memory */

	u32 surface_mem_alloc_size;	/**< byte size of allocated surface */
	u32 surface_mem_free_size;	/**< byte size of free memory */

	/* add something...*/
}
LX_GFX_MEM_STAT_T;

typedef enum
{
	GFX_NONE_FLAG	= 0,		/** empty flag */
	GFX_COMP_FLAG	= (1<<0),	/* Composition	*/
	GFX_WFD_FLAG	= (1<<1),	/* WiFi Direct	*/
	GFX_CAM_FLAG	= (1<<2),	/* CAMERA recording	*/
	GFX_CSC_FLAG	= (1<<3),	/* YUV -> ARGB	*/
}
LX_GFX_OPER_STATUS_T;

typedef struct
{
	u32 enable;		/* if enable == 1, set to 1 else if enable == 0, set to 0 */
	LX_GFX_OPER_STATUS_T operation_flag;	/* LX_GFX_OPER_STATUS_T sets */
}
LX_GFX_OPER_MODE_T;

/*-------------------------------------------------------------------
	Extern Function Prototype Declaration
-------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Extern Variables
-------------------------------------------------------------------*/

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _GFX_KAPI_H_ */

/** @} */

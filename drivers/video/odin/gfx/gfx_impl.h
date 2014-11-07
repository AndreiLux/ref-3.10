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

#ifndef	_GFX_IMPL_H_
#define	_GFX_IMPL_H_

/*-------------------------------------------------------------------
	Control Constants
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
    File Inclusions
--------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "base_types.h"
#include "os_util.h"
#include "gfx_proc.h"
/* #include "debug_util.h" */
/* #include "misc_util.h" */

#include "gfx_cfg.h"
#include "gfx_kapi.h"
#include "gfx_hal.h"

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

#ifdef CONFIG_ODIN_GFX_DEBUG_SUPPORT
#define DEBUG
#endif

#ifdef DEBUG
#define GFXDBG(format, ...) \
		printk(KERN_ERR "[GFX]: " format, ## __VA_ARGS__)
#define DSSDBGF(format, ...) \
		printk(KERN_ERR "[GFX]: " \
				": %s(" format ")\n", \
				__func__, \
				## __VA_ARGS__)
#define DBG_PRINT(S...)	printk(KERN_DEBUG S)
#else /* DEBUG */
#define GFXDBG(format, ...)
#define DSSDBGF(format, ...)
#define DBG_PRINT(S...)
#endif

#define GFXERR(format, ...)	\
	printk(KERN_ERR "[GFX] error: " format, ## __VA_ARGS__)
#define GFXINFO(format, ...)	\
	printk(KERN_INFO "[GFX] info: " format, ## __VA_ARGS__)
#define GFXWARN(format, ...)	\
	printk(KERN_WARNING "[GFX] warn: " format, ## __VA_ARGS__)

#define GFX_TRACE(format, args...) DBG_PRINT(format, ##args)
#define GFX_TRACE_BEGIN() GFX_TRACE("[GFXDEV:%d] BEGIN -- %s:%d\n",	\
	g_gfx_trace_depth++, __FUNCTION__, __LINE__ )
#define GFX_TRACE_END()	GFX_TRACE("[GFXDEV:%d] END -- %s:%d\n",	\
	--g_gfx_trace_depth, __FUNCTION__, __LINE__ )
#define GFX_TRACE_MARK() GFX_TRACE("[GFXDEV] LOGGING -- %s:%d\n",	\
	__FUNCTION__, __LINE__ )

#define GFX_CHECK_CODE(__checker,__if_action,fmt,args...)   \
			 __CHECK_IF_ERROR(__checker, GFXINFO, __if_action , fmt, ##args )

#define GFX_CHECK_ERROR(__checker,__if_action,fmt,args...)   \
			 __CHECK_IF_ERROR(__checker, GFXERR, __if_action , fmt, ##args )

#define GFX_ASSERT(__checker)	\
			__CHECK_IF_ERROR( !(__checker), GFXWARN, /* nop */,	\
			"[GFXDEV] ASSERT FAILED -- %s:%d\n", __FUNCTION__, __LINE__ )


#define	GFX_WADesc(param)				g_gfx_cfg.workaround.param
#ifdef CONFIG_ODIN_DUAL_GFX
#define	GFX_WADesc1(param)				g_gfx_cfg1.workaround.param
#endif

#define GFX_WRITE_REG(reg,val)	\
    __raw_writel(val , (ULONG)(g_gfx_reg) + (ULONG)OFFSET(GFX_REG_T,reg))

#define GFX_READ_REG(reg)	\
    __raw_readl( (ULONG)(g_gfx_reg) + (ULONG)OFFSET(GFX_REG_T,reg))


#define SW_WORKAROUND_INTERRUPT
/* if SW_WORKAROUND_INTERRUPT is defined then polling scheme is applied
 if SW_WORKAROUND_INTERRUPT is not defined then interrupt scheme is applied */

#define GH14_INTERRUPT
/* if GH15_INTERRUPT is defined,
	GFX is under CRG interrupt circumstancial operation */

/*-------------------------------------------------------------------
    Type Definitions
--------------------------------------------------------------------*/
/** GFX device context for each open request.
 *
 */

typedef struct
{
    UINT8	cached_mmap;	/* enable cached mmap if non zero */
	UINT8	rsvd[7];
}
GFX_DEV_CTX_T;

typedef struct
{
	char	psname[TASK_COMM_LEN+1];
	u32	balloc:1,       /* surface is allocated or not */
    	bpalette:1,     /* palette can be used or not */
		bpaldownload:1,  /* download to GFX IP */
		:1,
		palsize:12;     /* palette size ( max value 256 ) */

	u32	*pal;           /* palette data max is 256*4 byte */
	LX_GFX_SURFACE_SETTING_T    surf;
	LX_GFX_PHYS_MEM_INFO_T      mem;

	u32	cidx;			/* creation idex */
	u32	ctick;			/* createion tick */
}
GFX_SURFACE_OBJ_T;

/*-------------------------------------------------------------------
	Extern Function Prototype Declaration
--------------------------------------------------------------------*/
extern void gfx_initcfg(void);

extern int gfx_inithw(void);
/* extern int GFX_ShutdownHW (void); */

extern int gfx_initsurfacememory	(void);

extern int gfx_allocsurface(LX_GFX_SURFACE_SETTING_PARAM_T *surface);
extern int gfx_freesurface(UINT32 surface_fd);
extern int gfx_surfacemmap(struct file *file, struct vm_area_struct *vma);

extern int gfx_getsurfaceinfo(int surface_fd,
		LX_GFX_SURFACE_SETTING_T * surface_info );

extern int gfx_getsurfacememory(LX_GFX_SURFACE_MEM_INFO_PARAM_T *mem);
extern int gfx_getsurfacememorystat(LX_GFX_MEM_STAT_T* pMemStat );

extern int gfx_setsurfacepalette(int port_num, int size , UINT32 *data);
extern int gfx_getsurfacepalette(int port_num, int size , UINT32 *data);
extern int gfx_setcolorspace(UINT32 *coef);

extern int gfx_runblendop(LX_GFX_MANUAL_BLEND_CTRL_PARAM_T *param);
extern void gfx_dumpblitparam(LX_GFX_MANUAL_BLEND_CTRL_PARAM_T *param);
/*-------------------------------------------------------------------
	Extern Function Prototype Declaration (Utility)
--------------------------------------------------------------------*/
extern UINT32 gfx_pxlfmt2bpp( LX_GFX_PIXEL_FORMAT_T pxl_fmt );
extern UINT32 gfx_getportbaseaddr( GFX_SURFACE_OBJ_T* surface,
		LX_RECT_T* rect );

/*-------------------------------------------------------------------
	Extern Function Prototype Declaration (Extra)
--------------------------------------------------------------------*/
extern UINT32 gfx_calcsynctimeout( LX_GFX_MANUAL_BLEND_CTRL_PARAM_T *param,
			UINT32 default_msec_tmout );
extern int gfx_waitsynccommand	( UINT32 msec_tmout );
extern int gfx_wakeupwaitsync(void);
extern int gfx_getcommanddelay(UINT32 *cmd_delay);
extern int gfx_setcommanddelay(UINT32 cmd_delay);
extern LX_GFX_BATCH_RUN_MODE_T gfx_getruncommand(void);
extern int gfx_setruncommand (LX_GFX_BATCH_RUN_MODE_T *cmd);
extern int gfx_setgraphicsyncmode	(LX_GFX_GRAPHIC_SYNC_MODE_T mode);
extern LX_GFX_GRAPHIC_SYNC_MODE_T gfx_getgraphicsyncmode	(void);
extern void gfx_proc_init(void);
extern void gfx_proc_cleanup(void);

int gfx_odin_runresume(void);
unsigned int odin_gfx0_get_irq_status(void);
unsigned int odin_gfx1_get_irq_status(void);
#ifndef SW_WORKAROUND_INTERRUPT
int gfx_enable_isr(void);
void gfx_disable_isr(void);
#endif

#ifdef CONFIG_ODIN_DUAL_GFX
extern UINT32 gfx_calcsynctimeout1( LX_GFX_MANUAL_BLEND_CTRL_PARAM_T *param,
			UINT32 default_msec_tmout );
extern int gfx_waitsynccommand1	( UINT32 msec_tmout );
extern int gfx_wakeupwaitsync1(void);

extern int gfx_getcommanddelay1(UINT32 *cmd_delay);
extern int gfx_setcommanddelay1(UINT32 cmd_delay);
extern LX_GFX_BATCH_RUN_MODE_T gfx_getruncommand1(void);
extern int gfx_setruncommand1 (LX_GFX_BATCH_RUN_MODE_T *cmd);
extern int gfx_setgraphicsyncmode1	(LX_GFX_GRAPHIC_SYNC_MODE_T mode);
extern LX_GFX_GRAPHIC_SYNC_MODE_T gfx_getgraphicsyncmode1	(void);

extern int gfx_initsurfacememory1	(void);
extern int gfx_allocsurface1(LX_GFX_SURFACE_SETTING_PARAM_T *surface);
extern int gfx_freesurface1(UINT32 surface_fd);
extern int gfx_surfacemmap1(struct file *file, struct vm_area_struct *vma);
extern int gfx_getsurfacememory1(LX_GFX_SURFACE_MEM_INFO_PARAM_T *mem);
extern int gfx_getsurfacememorystat1(LX_GFX_MEM_STAT_T* pMemStat );
extern int gfx_setsurfacepalette1(int port_num, int size , UINT32 *data);
extern int gfx_getsurfacepalette1(int port_num, int size , UINT32 *data);
extern int gfx_setcolorspace1(UINT32 *coef);
extern int gfx_runblendop1(LX_GFX_MANUAL_BLEND_CTRL_PARAM_T *param);
extern void gfx_dumpblitparam1(LX_GFX_MANUAL_BLEND_CTRL_PARAM_T *param);
extern void gfx_initcfg1(void);
extern int gfx_inithw1(void);
extern UINT32 gfx_pxlfmt2bpp1( LX_GFX_PIXEL_FORMAT_T pxl_fmt );
extern UINT32 gfx_getportbaseaddr1( GFX_SURFACE_OBJ_T* surface,
		LX_RECT_T* rect );
extern int gfx_getcommanddelay1(UINT32 *cmd_delay);
extern int gfx_setcommanddelay1(UINT32 cmd_delay);
extern LX_GFX_BATCH_RUN_MODE_T gfx_getruncommand1(void);
extern UINT32 gfx_calcsynctimeout1( LX_GFX_MANUAL_BLEND_CTRL_PARAM_T *param,
			UINT32 default_msec_tmout );
extern LX_GFX_GRAPHIC_SYNC_MODE_T gfx_getgraphicsyncmode1	(void);
extern UINT32 gfx_pxlfmt2bpp1( LX_GFX_PIXEL_FORMAT_T pxl_fmt );
extern int gfx_wakeupwaitsync1(void);
extern int gfx_runblendop1(LX_GFX_MANUAL_BLEND_CTRL_PARAM_T *param);
extern int gfx_allocsurface1(LX_GFX_SURFACE_SETTING_PARAM_T *surface);
#endif


/*-------------------------------------------------------------------
	Extern Variables
--------------------------------------------------------------------*/
/* extern	int	g_gfx_debug_fd; */
extern	int					g_gfx_trace_depth;
extern	LX_GFX_CFG_T		g_gfx_cfg;
extern	GFX_HAL_T			g_gfx_hal;
extern	GFX_SURFACE_OBJ_T *g_gfx_surf_list;

#ifdef CONFIG_ODIN_DUAL_GFX
extern	LX_GFX_CFG_T		g_gfx_cfg1;
extern	GFX_HAL_T1			g_gfx_hal1;
extern	GFX_SURFACE_OBJ_T *g_gfx_surf_list1;
#endif

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _GFX_IMPL_H_ */

/** @} */

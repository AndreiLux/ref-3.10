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

/** @file
 *
 *  Brief description.
 *  Detailed description starts here.
 *
 *  @author		raxis.lim
 *  @version	1.0
 *  @date		2011-04-03
 *  @note		Additional information.
 */

/*-------------------------------------------------------------------
	Control Constants
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	File Inclusions
--------------------------------------------------------------------*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h> 		/* everything\ldots{} */
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <asm/uaccess.h>
#include <linux/ioport.h>	/* For request_region, check_region etc */
#include <asm/io.h>			/* For ioremap_nocache */
#include <linux/workqueue.h>	 /* For working queue */
#include <linux/interrupt.h>
#include <linux/irq.h>
#include "gfx_impl.h"
#include "gfx_core_reg.h"
#include "./dualgfx/gfx_core_reg1.h"
#include "../dss/dss.h"
#include <video/odindss.h>

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
	External Function Prototype Declarations
--------------------------------------------------------------------*/
/* extern void	GFX_Odin_stop(void); */
extern int	gfx_odin_runflushcommand(void);
extern int	gfx_odin_runstartcommand(void);
extern int	gfx_odin_pausecommand(void);
extern int	gfx_odin_resumecommand(void);
extern int	gfx_odin_stopcommand(void);
extern int	gfx_odin_swresetcommand(void);
extern void	gfx_odin_getcomqueuestatus(GFX_CMD_QUEUE_CTRL_T *queue);
extern bool	gfx_odin_isgfxidle(void);
extern void	gfx_odin_getoperationstatus(GFX_CMD_OP_T *operate);
extern void	gfx_odin_setoperationstatus(GFX_CMD_OP_T *operate);
extern void	gfx_odin_getinputconfigure(int iPort, GFX_PORT_CONFIGURE_T *port);
extern void	gfx_odin_setinputconfigure(GFX_PORT_CONFIGURE_T *port);
extern void	gfx_odin_setblendingout(GFX_ENUM_OUT_T type);
extern void	gfx_odin_getblendconfigure( GFX_BLEND_CONFIGURE_T *blend);
extern void	gfx_odin_setblendconfigure( GFX_BLEND_CONFIGURE_T *blend);
extern void	gfx_odin_getoutputconfigure(GFX_OUT_CONFIGURE_T *port);
extern void	gfx_odin_setoutputconfigure(GFX_OUT_CONFIGURE_T *port);
extern void	gfx_odin_setscalerconfigure(GFX_SCALER_CONFIGURE_T* scaler);
extern void	gfx_odin_setinterruptmode(u32 uiMode);
extern void	gfx_odin_setackmode(u32 *mode);
extern void	gfx_odin_getackmode(u32 *mode);
extern int	gfx_odin_setclut(int port, int size , u32 *data);
extern void	gfx_odin_setcommanddelayreg(u16 delay);
extern u16	gfx_odin_getcommanddelayreg(void);
extern void	gfx_odin_setregister(u32 addr, u32 val);
extern u32	gfx_odin_getregister(u32 addr);
extern void	gfx_odin_dumpregister(void);
extern int	gfx_odin_runsuspend(void);
extern int	gfx_odin_runresume(void);
extern void	gfx_odin_initscaler(void);
extern void	gfx_restart_function(void);

/*-------------------------------------------------------------------
	External Variables
--------------------------------------------------------------------*/
/* extern GFX_SCALER_FILTER_DATA_T g_gfx_scaler_filter; */

/*-------------------------------------------------------------------
	Global Variables
--------------------------------------------------------------------*/
volatile GFX_REG_T *g_gfx_reg;
GFX_REG_T *g_gfx_reg_cache;

/*-------------------------------------------------------------------
	Static Function Prototypes Declarations
--------------------------------------------------------------------*/
const LX_GFX_CFG_T* gfx_odin_getcfg	(void);
int gfx_odin_inithw	(void);
int gfx_odin_shutdownhw	(void);

#ifndef SW_WORKAROUND_INTERRUPT
#ifdef GH14_INTERRUPT
void gfx_odin_isrhandler(void *tossed_data,u32 irqmask,u32 sub_irqmask);
#else
irqreturn_t gfx_odin_isrhandler	(int irq, void *dev_id, struct pt_regs *regs);
#endif
#endif

/*-------------------------------------------------------------------
	Static Variables
--------------------------------------------------------------------*/
const static GFX_HAL_T	g_gfx_hal_odin =
{
	.getcfg				= gfx_odin_getcfg,
	.inithw				= gfx_odin_inithw,
	.shutdownhw			= gfx_odin_shutdownhw,
	.runsuspend			= gfx_odin_runsuspend,
	.runresume			= gfx_odin_runresume,
	.runflushcommand	= gfx_odin_runflushcommand,
	.runstartcommand	= gfx_odin_runstartcommand,
	.runpausecommand	= gfx_odin_pausecommand,
	.runresumecommand	= gfx_odin_resumecommand,
	.runstopcommand		= gfx_odin_stopcommand,
	.runswresetcommand	= gfx_odin_swresetcommand,
	.getcomqueuestatus	= gfx_odin_getcomqueuestatus,
	.getoperationstatus	= gfx_odin_getoperationstatus,
	.setoperationstatus	= gfx_odin_setoperationstatus,
	.getinputconfigure	= gfx_odin_getinputconfigure,
	.setinputconfigure	= gfx_odin_setinputconfigure,
	.getblendconfigure	= gfx_odin_getblendconfigure,
	.setblendconfigure	= gfx_odin_setblendconfigure,
	.setblendingout		= gfx_odin_setblendingout,
	.getoutputconfigure	= gfx_odin_getoutputconfigure,
	.setoutputconfigure	= gfx_odin_setoutputconfigure,
	.setscalerconfigure	= gfx_odin_setscalerconfigure,
	.setclut			= gfx_odin_setclut,
	.setcommanddelayreg	= gfx_odin_setcommanddelayreg,
	.getcommanddelayreg	= gfx_odin_getcommanddelayreg,
	.isgfxidle			= gfx_odin_isgfxidle,
	.setregister		= gfx_odin_setregister,
	.getregister		= gfx_odin_getregister,
	.dumpregister		= gfx_odin_dumpregister,
};

static LX_GFX_CFG_T	g_gfx_cfg_odin =
{
    .b_hw_scaler = TRUE,
    .surface_blit_cmd_delay= 0x0,	/* 0x40 */
    .screen_blit_cmd_delay = 0x0,	/* 0x20 */
	.sync_wait_timeout	= 40,	/* 20, for FPGA verification */
	.sync_fail_retry_count = 2,
	.power_status = 1,

    .hw_limit = {
        .max_surface_width	= 8191, /* L9 has 13 bit width field */
        .max_surface_stride	= 32767, /* L9 has 15 bit stride field */
        .min_scaler_input_width = 12,
        /* L9 doesn't strech below input width <= 12 */
    },
};


/*-------------------------------------------------------------------
    Implementation Group
--------------------------------------------------------------------*/

/** get Odin specific configuration
 *
 *  @return LX_GFX_CFG_T
 */
const LX_GFX_CFG_T* gfx_odin_getcfg(void)
{
    return &g_gfx_cfg_odin;
}

void gfx_odin_inithal( GFX_HAL_T*	hal )
{
	memcpy( hal, &g_gfx_hal_odin, sizeof(GFX_HAL_T));

	g_gfx_cfg_odin.workaround.bad_dst_addr_stuck      = 0;
	g_gfx_cfg_odin.workaround.scaler_read_buf_stuck   = 0;
	g_gfx_cfg_odin.workaround.srcblt_op_stuck         = 0;
	g_gfx_cfg_odin.workaround.write_op_stuck          = 0;
}

/*-------------------------------------------------------------------
	Static Function Prototypes Declarations
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Static Variables
--------------------------------------------------------------------*/


/*-------------------------------------------------------------------
	Implementation Group
--------------------------------------------------------------------*/

unsigned int odin_gfx0_get_irq_status(void)
{
	u32 val = 0;
	GFX_RdFL( gfx_status2 );

	if (g_gfx_reg->gfx_status2.intr_st)	val = 0x1;

	GFXDBG("[%s.%d] GFX0 interrupt status value = %d\n", __F__, __L__, val );

	return val;
}

#ifndef SW_WORKAROUND_INTERRUPT
#ifdef GH14_INTERRUPT
/* GFX interrupt handler  */
void gfx_odin_isrhandler(void *tossed_data,u32 irqmask,u32 sub_irqmask)
{
	/*
	 * TODO.. need GFX1 interrupt action handler
	 */
	gfx_wakeupwaitsync();

	g_gfx_reg->gfx_intr_clear.gfx_intr_clear = 0x1;

	return IRQ_HANDLED;
}
#if 0
int gfx_enable_isr(void)
{
	int r;

	r = odin_crg_register_isr(gfx_odin_isrhandler, NULL, CRG_IRQ_GFX, 0);

	if (r)	GFXDBG("fail :  gfx_enable_isr\n");
	else	GFXDBG("sucess : gfx_enable_isr\n");

	return r;
}

void gfx_disable_isr(void)
{
	odin_crg_unregister_isr(gfx_odin_isrhandler, NULL, CRG_IRQ_GFX, 0);
}
#endif
#else
irqreturn_t gfx_odin_isrhandler(int irq, void *dev_id, struct pt_regs *regs)
{
/*	if ( gfx_isgfxidle() ) */
	{
/*		GFXDBG("#### GFX ISR - IDLE OK ####\n"); */
		gfx_wakeupwaitsync();
	}

	g_gfx_reg->gfx_intr_clear.gfx_intr_clear = 0x1;
	printk("#### GFX ISR ####\n");

	return IRQ_HANDLED;
}
#endif
#endif

/** initialize Odin hardware
*
* @return RET_OK when success, RET_ERROR otherwise
*/

int gfx_odin_inithw(void)
{
	/* do ioremap */
	g_gfx_reg =(GFX_REG_T *)ioremap( gpgfxregcfg->reg_base_addr,
		gpgfxregcfg->reg_size);

	GFX_CHECK_CODE( g_gfx_reg == NULL, return RET_ERROR,
		"can't alloc memory for regs\n");

	g_gfx_reg_cache = (GFX_REG_T *)OS_Malloc( gpgfxregcfg->reg_size );
	GFX_CHECK_CODE( g_gfx_reg_cache == NULL, return RET_ERROR,
		"out of memory\n");

	memset( g_gfx_reg_cache, 0x0, gpgfxregcfg->reg_size );

	/* Reset GFX H/W */
	gfx_odin_swresetcommand( );

#ifndef SW_WORKAROUND_INTERRUPT
#ifdef GH14_INTERRUPT
	/* register GFX interrupt handler */
	odin_crg_register_isr(gfx_odin_isrhandler, NULL, CRG_IRQ_GFX, 0);
	printk("GFX ISR register\n");
#else
	/* register GFX interrupt handler */
	GFX_CHECK_CODE( request_irq( gpgfxregcfg->irq_num,
	(irq_handler_t)gfx_odin_isrhandler, 0, "gfx_irq", NULL), /* nop */,
	"request irq failed\n" );
#endif
#endif
	g_gfx_reg->gfx_intr_ctrl.intr_gen_mode = 0; /* batch command finish */
	g_gfx_reg->gfx_intr_ctrl.intr_en = 1; /* interrupt enable */

	gfx_odin_initscaler( );

	return 0;
}

/** shutdown Odin GFX hardware
 *
 * @return RET_OK when success, RET_ERROR otherwise
 */
int	gfx_odin_shutdownhw(void)
{
	g_gfx_reg->gfx_intr_ctrl.intr_en = 0; /* interrupt disable */

	/* [TODO] more cleanup !!! */

    iounmap((void *)g_gfx_reg);

	return 0;
}

/** @} */


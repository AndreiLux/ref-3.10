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
#include "../gfx_impl.h"
#include "gfx_core_reg1.h"

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
extern int	gfx_odin_runflushcommand1(void);
extern int	gfx_odin_runstartcommand1(void);
extern int	gfx_odin_pausecommand1(void);
extern int	gfx_odin_resumecommand1(void);
extern int	gfx_odin_stopcommand1(void);
extern int	gfx_odin_swresetcommand1(void);
extern void	gfx_odin_getcomqueuestatus1(GFX_CMD_QUEUE_CTRL_T *queue);
extern bool	gfx_odin_isgfxidle1(void);
extern void	gfx_odin_getoperationstatus1(GFX_CMD_OP_T *operate);
extern void	gfx_odin_setoperationstatus1(GFX_CMD_OP_T *operate);
extern void	gfx_odin_getinputconfigure1(int iPort, GFX_PORT_CONFIGURE_T *port);
extern void	gfx_odin_setinputconfigure1(GFX_PORT_CONFIGURE_T *port);
extern void	gfx_odin_setblendingout1(GFX_ENUM_OUT_T type);
extern void	gfx_odin_getblendconfigure1( GFX_BLEND_CONFIGURE_T *blend);
extern void	gfx_odin_setblendconfigure1( GFX_BLEND_CONFIGURE_T *blend);
extern void	gfx_odin_getoutputconfigure1(GFX_OUT_CONFIGURE_T *port);
extern void	gfx_odin_setoutputconfigure1(GFX_OUT_CONFIGURE_T *port);
extern void	gfx_odin_setscalerconfigure1(GFX_SCALER_CONFIGURE_T* scaler);
extern void	gfx_odin_setinterruptmode1(u32 uiMode);
extern void	gfx_odin_setackmode1(u32 *mode);
extern void	gfx_odin_getackmode1(u32 *mode);
extern int	gfx_odin_setclut1(int port, int size , u32 *data);
extern void	gfx_odin_setcommanddelayreg1(u16 delay);
extern u16	gfx_odin_getcommanddelayreg1(void);
extern void	gfx_odin_setregister1(u32 addr, u32 val);
extern u32	gfx_odin_getregister1(u32 addr);
extern void	gfx_odin_dumpregister1(void);
extern int	gfx_odin_runsuspend1(void);
extern int	gfx_odin_runresume1(void);
extern void	gfx_odin_initscaler1(void);

/*-------------------------------------------------------------------
	External Variables
--------------------------------------------------------------------*/
/* extern GFX_SCALER_FILTER_DATA_T g_gfx_scaler_filter; */

/*-------------------------------------------------------------------
	Global Variables
--------------------------------------------------------------------*/
volatile GFX_REG_T1 *g_gfx_reg1;
GFX_REG_T1 *g_gfx_reg_cache1;

/*-------------------------------------------------------------------
	Static Function Prototypes Declarations
--------------------------------------------------------------------*/
const LX_GFX_CFG_T * gfx_odin_getcfg1 (void);
int gfx_odin_inithw1 (void);
int gfx_odin_shutdownhw1	(void);
irqreturn_t gfx_odin_isrhandler1 (int irq, void *dev_id, struct pt_regs *regs);

/*-------------------------------------------------------------------
	Static Variables
--------------------------------------------------------------------*/
const static GFX_HAL_T1	g_gfx_hal_odin1 =
{
	.getcfg				= gfx_odin_getcfg1,
	.inithw				= gfx_odin_inithw1,
	.shutdownhw			= gfx_odin_shutdownhw1,
	.runsuspend			= gfx_odin_runsuspend1,
	.runresume			= gfx_odin_runresume1,
	.runflushcommand	= gfx_odin_runflushcommand1,
	.runstartcommand	= gfx_odin_runstartcommand1,
	.runpausecommand	= gfx_odin_pausecommand1,
	.runresumecommand	= gfx_odin_resumecommand1,
	.runstopcommand		= gfx_odin_stopcommand1,
	.runswresetcommand	= gfx_odin_swresetcommand1,
	.getcomqueuestatus	= gfx_odin_getcomqueuestatus1,
	.getoperationstatus	= gfx_odin_getoperationstatus1,
	.setoperationstatus	= gfx_odin_setoperationstatus1,
	.getinputconfigure	= gfx_odin_getinputconfigure1,
	.setinputconfigure	= gfx_odin_setinputconfigure1,
	.getblendconfigure	= gfx_odin_getblendconfigure1,
	.setblendconfigure	= gfx_odin_setblendconfigure1,
	.setblendingout		= gfx_odin_setblendingout1,
	.getoutputconfigure	= gfx_odin_getoutputconfigure1,
	.setoutputconfigure	= gfx_odin_setoutputconfigure1,
	.setscalerconfigure	= gfx_odin_setscalerconfigure1,
	.setclut			= gfx_odin_setclut1,
	.setcommanddelayreg	= gfx_odin_setcommanddelayreg1,
	.getcommanddelayreg	= gfx_odin_getcommanddelayreg1,
	.isgfxidle			= gfx_odin_isgfxidle1,
	.setregister		= gfx_odin_setregister1,
	.getregister		= gfx_odin_getregister1,
	.dumpregister		= gfx_odin_dumpregister1,
};

static LX_GFX_CFG_T g_gfx_cfg_odin1 =
{
	.b_hw_scaler = TRUE,
	.surface_blit_cmd_delay= 0x0,	/* 0x40 */
	.screen_blit_cmd_delay = 0x0,	/* 0x20 */
	.sync_wait_timeout	= 40,	/* 20, for FPGA verification */
	.sync_fail_retry_count = 2,

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
const LX_GFX_CFG_T * gfx_odin_getcfg1(void)
{
	return &g_gfx_cfg_odin1;
}

void gfx_odin_inithal1 ( GFX_HAL_T1 * hal )
{
	memcpy( hal, &g_gfx_hal_odin1, sizeof(GFX_HAL_T1));

	g_gfx_cfg_odin1.workaround.bad_dst_addr_stuck      = 0;
	g_gfx_cfg_odin1.workaround.scaler_read_buf_stuck   = 0;
	g_gfx_cfg_odin1.workaround.srcblt_op_stuck         = 0;
	g_gfx_cfg_odin1.workaround.write_op_stuck          = 0;
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

unsigned int odin_gfx1_get_irq_status(void)
{
	u32 val = 0;

	GFX_RdFL1( gfx_status2 );
	if (g_gfx_reg1->gfx_status2.intr_st)	val = 0x2;

	GFXDBG("[%s.%d] GFX1 interrupt status value = %d\n", __F__, __L__, val );

	return val;
}

/** GFX interrupt handler
 *
 */
irqreturn_t gfx_odin_isrhandler1(int irq, void *dev_id, struct pt_regs *regs)
{
/*	if ( gfx_isgfxidle() ) */
	{
/*		GFXDBG("#### GFX ISR - IDLE OK ####\n"); */
		gfx_wakeupwaitsync1();
	}

	g_gfx_reg1->gfx_intr_clear.gfx_intr_clear = 0x1;
	GFXDBG("#### GFX ISR ####\n");

	return IRQ_HANDLED;
}

/** initialize Odin hardware
*
* @return RET_OK when success, RET_ERROR otherwise
*/

int gfx_odin_inithw1(void)
{
	/* do ioremap */
	g_gfx_reg1 =(GFX_REG_T1 *)ioremap( gpgfxregcfg1->reg_base_addr,
		gpgfxregcfg1->reg_size);

	GFX_CHECK_CODE( g_gfx_reg1 == NULL, return RET_ERROR,
		"can't alloc memory for regs\n");

	g_gfx_reg_cache1 = (GFX_REG_T1 *)OS_Malloc( gpgfxregcfg1->reg_size );
	GFX_CHECK_CODE( g_gfx_reg_cache1 == NULL, return RET_ERROR,
		"out of memory\n");

	memset( g_gfx_reg_cache1, 0x0, gpgfxregcfg1->reg_size );

	/* Reset GFX H/W */
	gfx_odin_swresetcommand1( );

#ifndef SW_WORKAROUND_INTERRUPT
#ifdef GH14_INTERRUPT
	/* register GFX interrupt handler */
	/* TODO */
#else
	GFX_CHECK_CODE( request_irq( gpgfxregcfg1->irq_num,
	(irq_handler_t)gfx_odin_isrhandler1, 0, "gfx_irq", NULL), /* nop */,
	"request irq failed\n" );
#endif
#endif
	g_gfx_reg1->gfx_intr_ctrl.intr_gen_mode = 0; /* batch command finish */
	g_gfx_reg1->gfx_intr_ctrl.intr_en = 1; /* interrupt enable */

	gfx_odin_initscaler1( );

	return 0;
}

/** shutdown Odin GFX hardware
 *
 * @return RET_OK when success, RET_ERROR otherwise
 */
int gfx_odin_shutdownhw1(void)
{
	g_gfx_reg1->gfx_intr_ctrl.intr_en = 0; /* interrupt disable */

	/* [TODO] more cleanup !!! */

    iounmap((void *)g_gfx_reg1);

	return 0;
}

/** @} */


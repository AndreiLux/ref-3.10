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
 *  @author     raxis.lim
 *  @version    1.0
 *  @date       2011-04-03
 *  @note       Additional information.
 */

#ifndef	_GFX_HAL_H_
#define	_GFX_HAL_H_

/*-------------------------------------------------------------------
	Control Constants
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
    File Inclusions
--------------------------------------------------------------------*/
#include "gfx_hw.h"

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
typedef	struct
{
	const LX_GFX_CFG_T *(*getcfg)	(void);

	int		(*inithw)				(void);
	int		(*shutdownhw)			(void);
	int		(*runsuspend)			(void);
	int		(*runresume)			(void);
	int		(*runflushcommand)		(void);
	int		(*runstartcommand)		(void);
	int		(*runpausecommand)		(void);
	int		(*runresumecommand)		(void);
	int		(*runstopcommand)		(void);
	int		(*runswresetcommand)	(void);
	void	(*getcomqueuestatus)	(GFX_CMD_QUEUE_CTRL_T *queue);
	void 	(*getoperationstatus)	(GFX_CMD_OP_T *operate);
	void 	(*setoperationstatus)	(GFX_CMD_OP_T *operate);
	void 	(*getinputconfigure)	(int iPort, GFX_PORT_CONFIGURE_T *port);
	void 	(*setinputconfigure)	(GFX_PORT_CONFIGURE_T *port);
	void 	(*getblendconfigure)	(GFX_BLEND_CONFIGURE_T *blend);
	void 	(*setblendconfigure)	(GFX_BLEND_CONFIGURE_T *blend);
	void 	(*setblendingout)		(GFX_ENUM_OUT_T type);
	void 	(*getoutputconfigure)	(GFX_OUT_CONFIGURE_T *port);
	void 	(*setoutputconfigure)	(GFX_OUT_CONFIGURE_T *port);
	void	(*setscalerconfigure)	(GFX_SCALER_CONFIGURE_T *scaler);
	int 	(*setclut)				(int port, int size , u32 *data);
	void	(*setcommanddelayreg)	(u16 delay);
	u16		(*getcommanddelayreg)	(void);
	bool	(*isgfxidle)			(void);
	void	(*setregister)			(u32 addr, u32 val);
	u32		(*getregister)			(u32 addr );
	void	(*dumpregister)			(void);
}
GFX_HAL_T;

#ifdef CONFIG_ODIN_DUAL_GFX
typedef	struct
{
	const LX_GFX_CFG_T *(*getcfg)	(void);

	int		(*inithw)				(void);
	int		(*shutdownhw)			(void);
	int		(*runsuspend)			(void);
	int		(*runresume)			(void);
	int		(*runflushcommand)		(void);
	int		(*runstartcommand)		(void);
	int		(*runpausecommand)		(void);
	int		(*runresumecommand)		(void);
	int		(*runstopcommand)		(void);
	int		(*runswresetcommand)	(void);
	void	(*getcomqueuestatus)	(GFX_CMD_QUEUE_CTRL_T *queue);
	void 	(*getoperationstatus)	(GFX_CMD_OP_T *operate);
	void 	(*setoperationstatus)	(GFX_CMD_OP_T *operate);
	void 	(*getinputconfigure)	(int iPort, GFX_PORT_CONFIGURE_T *port);
	void 	(*setinputconfigure)	(GFX_PORT_CONFIGURE_T *port);
	void 	(*getblendconfigure)	(GFX_BLEND_CONFIGURE_T *blend);
	void 	(*setblendconfigure)	(GFX_BLEND_CONFIGURE_T *blend);
	void 	(*setblendingout)		(GFX_ENUM_OUT_T type);
	void 	(*getoutputconfigure)	(GFX_OUT_CONFIGURE_T *port);
	void 	(*setoutputconfigure)	(GFX_OUT_CONFIGURE_T *port);
	void	(*setscalerconfigure)	(GFX_SCALER_CONFIGURE_T *scaler);
	int 	(*setclut)				(int port, int size , u32 *data);
	void	(*setcommanddelayreg)	(u16 delay);
	u16		(*getcommanddelayreg)	(void);
	bool	(*isgfxidle)			(void);
	void	(*setregister)			(u32 addr, u32 val);
	u32		(*getregister)			(u32 addr );
	void	(*dumpregister)			(void);
}
GFX_HAL_T1;
#endif

/*-------------------------------------------------------------------
	Extern Function Prototype Declaration
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Extern Variables
--------------------------------------------------------------------*/
extern	GFX_HAL_T	g_gfx_hal;
#ifdef CONFIG_ODIN_DUAL_GFX
extern	GFX_HAL_T1	g_gfx_hal1;
#endif

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _GFX_HAL_H_ */


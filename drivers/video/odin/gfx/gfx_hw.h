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

#ifndef	_GFX_HW_H_
#define	_GFX_HW_H_

/*-------------------------------------------------------------------
	Control Constants
--------------------------------------------------------------------*/
/* #define __ARM__ */

/*-------------------------------------------------------------------
    File Inclusions
--------------------------------------------------------------------*/

#ifdef	__cplusplus
extern "C"
#endif

#if 0
/*-------------------------------------------------------------------
	Macro Definitions
--------------------------------------------------------------------*/
#define	GFX_REG_MEMCPY(dst,src,cnt)						\
{														\
	register int i;										\
	volatile u32* dst_ptr = (volatile u32*)dst;			\
			 u32* src_ptr = (u32*)src;					\
	for ( i=cnt; i>0; i-- )	 *dst_ptr++ = *src_ptr++;	\
}
#endif

/*-------------------------------------------------------------------
    Type Definitions (enum)
--------------------------------------------------------------------*/
#if 0
typedef enum
{
    GFX_INT_BATCH = 0,
    GFX_INT_SINGLECMD,
    GFX_INT_SPECIFIEDCMD,
}
GFX_INT_MODE_T;
#endif

typedef enum
{
	EWRITEONLY = 0,
	EREADONEPORT,
	EREADTWOPORTS,
	EREADTHREEPORTS,
	EREADTWOPORTS_YC,
	EREADTHREEPORTS_YC
}
GFX_ENUM_INPUT_T;

typedef enum
{
	ENONE = 0,
	EPORT0,
	EPORT1,
	EPORT2
}
GFX_ENUM_PORT_T;

typedef enum
{
	EINDEX_0 = 0,
	EINDEX_1 = 0x1,
	EINDEX_2 = 0x2,
	EINDEX_4 = 0x3,
	EINDEX_8 = 0x4,
	EALPHA_8 = 0x5,
	EYCBCR444 = 0x6,
	ECB8CR8422 = 0x7,
	ECB8CR8420 = 0x8,
	EYCBCR655 = 0x9,
	EAYCBCR2644 = 0xa,
	EAYCBCR4633 = 0xb,
	EAYCBCR6433 = 0xc,
	ECBCR420 = 0xd,
	EAYCBCR8888 = 0xe,
	EY0CB0Y1CR422 = 0xf,
	ERGB565 = 0x19,
	EARGB1555 = 0x1a,
	EARGB4444 = 0x1b,
	EARGB6343 = 0x1c,
	ECBCR422 = 0x1d,
	EARGB8888 = 0x1e,
}
GFX_ENUM_PIXEL_FORMAT_T;

/*	raxis.lim (2010/05/31)	-- bug fix
 *	-- 	swap endian value
 *		according to GFX manual, little endian is 0 and big endian is 1
 *		( system default is little endian )
 */
typedef enum
{
	ELITTLE = 0,
	EBIG 	= 1,
}
GFX_ENUM_ENDIAN_T;

typedef enum
{
	EPORT_0	= 0,
	EPORT_1,
	EPORT_2,
	ECONSTANT,
	EBLEND,
	ERASTER,
	EYC_INTERLEAVING
}
GFX_ENUM_OUT_T;

typedef enum
{
	ECOMP_LINEAR = 0,
	ECOMP_MIN,
	ECOMP_MAX
}
GFX_ENUM_BLEND_COMP_T;

typedef enum
{
	EZERO	= 0,
	ESRC_AND_DST,
	ESRC_AND_NOT_DST,
	ESRC,
	ENOT_SRC_AND_DST,
	EDST,
	ESRC_XOR_DST,
	ESRC_OR_DST,
	ENOT_SRC_AND_NOT_DST,
	ENOT_SRC_XOR_DST,
	ENOT_DST,
	ESRC_OR_NOT_DST,
	ENOT_SRC,
	ENOT_SRC_OR_DST,
	ENOT_SRC_OR_NOT_DST,
	EONE
}
GFX_ENUM_RASTER_T;

typedef enum
{
	EREPEAT =0,
	ETWO_TAP_AVG,
	EOTHERS
}
GFX_ENUM_CH_FILTER_MODE_T;

typedef enum
{
	EYC_TO_RGB0 =0,
	EYC_TO_RGB1,
	ERGB_TO_YC0,
	ERGB_TO_YC1,
}
GFX_ENUM_CSC_COEF_SEL_T;

/*-------------------------------------------------------------------
    Type Definitions (struct)
--------------------------------------------------------------------*/
typedef struct
{
	u8 	bstatus;
	u8 	bfull;
	u8 	bbatchstatus;
	u8	bcmdqstatus;

	u16	usline;
	u16	usremainspace;
	u16	usremainparam;

	u32	status[3];
}
GFX_CMD_QUEUE_CTRL_T;

typedef struct
{
	GFX_ENUM_INPUT_T sport;	/* pixel format */

	u8 bburstmodeenable;		/* burst mode enable */
	u8 bupdateclut;
	u8 bchromafilterenable;
	GFX_ENUM_CH_FILTER_MODE_T sfmode;

	u16 uscoef[4];
/*
	u8 bRun;
	u8 bStart;
	u8 bPause;
	u8 bHalt;
*/
}
GFX_CMD_OP_T;

typedef struct
{
	GFX_ENUM_PORT_T 		sport;

	u32 					uladdr;			/* base address */
	u16 					usstride;		/* stride */
	GFX_ENUM_ENDIAN_T 		sendian;		/* endian */
	GFX_ENUM_PIXEL_FORMAT_T sfmt;			/* pixel format */
	u32 					uiglobalalpha;

	u32 					uldataclut;
	u16 					usaddrclut;
	u16 					brw_clut:1,
							bauto_inc_clut:1,
							bcolorkeyenable:1,
							bbitmaskenable:1,
							bcocenable:1,
							bcscenable:1,
							bcolorkeymode:1,
							xxx:9;
	GFX_ENUM_CSC_COEF_SEL_T scscsel;

	u32 					ulkeylow;
	u32 					ulkeyhigh;
	u32 					ulreplacecolor;
	u32 					ulbitmask;
	u32 					ulcocctrl;
}
GFX_PORT_CONFIGURE_T;

typedef struct
{
	GFX_ENUM_OUT_T 			sout;
	GFX_ENUM_BLEND_COMP_T 	salphablend;
	GFX_ENUM_BLEND_COMP_T 	srblend;
	GFX_ENUM_BLEND_COMP_T 	sgblend;
	GFX_ENUM_BLEND_COMP_T 	sbblend;

	u16 					usalpha_m0;
	u16 					usc_m0;

	u32 					bdivenable:1,
							bxor0enable:1,
							bxor1enable:1,
							bpma0enable:1,
							bpma1enable:1,
							xxx:27;

	u16 					usa0_alpha;
	u16 					usb0_alpha;
	u16 					usa1_r;
	u16 					usb1_r;
	u16 					usa2_g;
	u16 					usb2_g;
	u16 					usa3_b;
	u16 					usb3_b;

	u32 					ulblendconstant;
	GFX_ENUM_RASTER_T 		sraster;
}
GFX_BLEND_CONFIGURE_T;

typedef struct
{
	u32 uladdr;		/* out base address */
	u16 usstride;	/* out stride. bytes per line */

	GFX_ENUM_ENDIAN_T sendian;	/* endian */
	GFX_ENUM_PIXEL_FORMAT_T sfmt;	/* pixel format */

	u16 usvsize;
	u16 ushsize;

	u32 ulgalpha;

	u8 bcscenable;
	u8 bcocenable;

	u32 ulcocctrl;

	u32 uicsccoef[8];
}
GFX_OUT_CONFIGURE_T;

typedef struct
{
	u32	phase_offset;
	u32	boundary_mode;
	u32	sampling_mode;
	u32	numerator;
	u32	denomidator;
	u32	filter_bank;
}
GFX_SCALER_FILTER_DATA_T;

typedef struct
{
#define GFX_SCALER_CMD_BYPASS			0
#define GFX_SCALER_CMD_SOFT_RESET		1
#define GFX_SCALER_CMD_START			2
	u8			cmd;

#define GFX_SCALER_FILTER_BILINEAR		0
	/** bilinear filter (GFX default filter) */
#define GFX_SCALER_FILTER_CUSTOM		1
	/** polyphase filter (custom filter) */
#define GFX_SCALER_FILTER_MAX			2
	u8			filter;

	LX_DIMENSION_T	in_dim;			/* scaler input dimension */
	LX_DIMENSION_T	out_dim;		/* scaler output dimension */
}
GFX_SCALER_CONFIGURE_T;

/*-------------------------------------------------------------------
	Extern Function Prototype Declaration
--------------------------------------------------------------------*/
void gfx_initialize(void);
int gfx_runflushcommand(void);
int gfx_runstartcommand(void);
int gfx_pausecommand(void);
int gfx_resumecommand(void);
int gfx_stopcommand(void);
int gfx_swresetcommand(void);
void gfx_getcomqueuestatus(GFX_CMD_QUEUE_CTRL_T *queue);
void gfx_getoperationstatus(GFX_CMD_OP_T *operate);
void gfx_setoperationstatus(GFX_CMD_OP_T *operate);
void gfx_getinputconfigure(int iPort, GFX_PORT_CONFIGURE_T *port);
void gfx_setinputconfigure( GFX_PORT_CONFIGURE_T *port);
void gfx_setinputbasicconfigure( GFX_PORT_CONFIGURE_T *port);
void gfx_getblendconfigure( GFX_BLEND_CONFIGURE_T *blend);
void gfx_setblendconfigure( GFX_BLEND_CONFIGURE_T *blend);
void gfx_setblendingout(GFX_ENUM_OUT_T type);
void gfx_getoutputconfigure(GFX_OUT_CONFIGURE_T *port);
void gfx_setoutputconfigure(GFX_OUT_CONFIGURE_T *port);
void gfx_setoutputbasicconfigure(GFX_OUT_CONFIGURE_T *port);
void gfx_setscalerconfigure(GFX_SCALER_CONFIGURE_T* scaler);
int gfx_setclut(int port , int size , u32 *data);
void gfx_setcommanddelayreg(u16 delay);
u16 gfx_getcommanddelayreg(void);
void gfx_setregister(u32 addr, u32 val);
u32 gfx_getregister(u32 addr);
void gfx_dumpregister(void);
bool gfx_isgfxidle(void);
int gfx_runsuspend(void);
int gfx_runresume(void);

#ifdef CONFIG_ODIN_DUAL_GFX
void gfx_initialize1(void);
int gfx_runflushcommand1(void);
int gfx_runstartcommand1(void);
int gfx_pausecommand1(void);
int gfx_resumecommand1(void);
int gfx_stopcommand1(void);
int gfx_swresetcommand1(void);
void gfx_getcomqueuestatus1(GFX_CMD_QUEUE_CTRL_T *queue);
void gfx_getoperationstatus1(GFX_CMD_OP_T *operate);
void gfx_setoperationstatus1(GFX_CMD_OP_T *operate);
void gfx_getinputconfigure1(int iPort, GFX_PORT_CONFIGURE_T *port);
void gfx_setinputconfigure1( GFX_PORT_CONFIGURE_T *port);
void gfx_setinputbasicconfigure1( GFX_PORT_CONFIGURE_T *port);
void gfx_getblendconfigure1( GFX_BLEND_CONFIGURE_T *blend);
void gfx_setblendconfigure1( GFX_BLEND_CONFIGURE_T *blend);
void gfx_setblendingout1(GFX_ENUM_OUT_T type);
void gfx_getoutputconfigure1(GFX_OUT_CONFIGURE_T *port);
void gfx_setoutputconfigure1(GFX_OUT_CONFIGURE_T *port);
void gfx_setoutputbasicconfigure1(GFX_OUT_CONFIGURE_T *port);
void gfx_setscalerconfigure1(GFX_SCALER_CONFIGURE_T* scaler);
int gfx_setclut1(int port , int size , u32 *data);
void gfx_setcommanddelayreg1(u16 delay);
u16 gfx_getcommanddelayreg1(void);
void gfx_setregister1(u32 addr, u32 val);
u32 gfx_getregister1(u32 addr);
void gfx_dumpregister1(void);
bool gfx_isgfxidle1(void);
int gfx_runsuspend1(void);
int gfx_runresume1(void);
#endif

/*-------------------------------------------------------------------
	Extern Variables
--------------------------------------------------------------------*/

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _GFX_HW_H_ */

/** @} */


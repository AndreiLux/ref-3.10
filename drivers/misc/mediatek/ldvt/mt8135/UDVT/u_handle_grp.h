/*-----------------------------------------------------------------------------
 * $RCSfile: u_handle_grp.h,v $
 * $Revision: #5 $
 * $Date: 2010/02/03 $
 * $Author: yun.zhou $
 *
 * Description:
 *         This header file contains handle group definitions which are known
 *         to the whole Middleware, Drivers and Applications.
 *---------------------------------------------------------------------------*/

#ifndef _U_HANDLE_GRP_H_
#define _U_HANDLE_GRP_H_


/*-----------------------------------------------------------------------------
		    include files
 ----------------------------------------------------------------------------*/

#include "u_handle.h"


/*-----------------------------------------------------------------------------
		    macros, defines, typedefs, enums
 ----------------------------------------------------------------------------*/

/* Specify handle groups */
#define HT_GROUP_SIZE     ((HANDLE_TYPE_T) 64)

/* A handle type of value '0' will be reserved as invalid handle. */
/* The first HT_GROUP_SIZE handle type values are reserved by the */
/* handle library. The first group starts at value HT_GROUP_SIZE. */
#define HT_RES_BY_HANDLE_LIB  HT_GROUP_SIZE

#define HT_GROUP_RM       ((HANDLE_TYPE_T) (1 * HT_GROUP_SIZE))	/* Resource Manager */
#define HT_GROUP_OS       ((HANDLE_TYPE_T) (2 * HT_GROUP_SIZE))	/* OS */
#define HT_GROUP_CM       ((HANDLE_TYPE_T) (3 * HT_GROUP_SIZE))	/* Connection Manager */
#define HT_GROUP_SM       ((HANDLE_TYPE_T) (4 * HT_GROUP_SIZE))	/* Stream Manager */
#define HT_GROUP_FM       ((HANDLE_TYPE_T) (5 * HT_GROUP_SIZE))	/* File Manager */
#define HT_GROUP_TM       ((HANDLE_TYPE_T) (6 * HT_GROUP_SIZE))	/* Table Manager RESERVE 128 TYPES. */
#define HT_GROUP_COMMON   ((HANDLE_TYPE_T) (8 * HT_GROUP_SIZE))	/* Common libraries */
#define HT_GROUP_CDB      ((HANDLE_TYPE_T) (9 * HT_GROUP_SIZE))	/* Core DB library */
#define HT_GROUP_PM       ((HANDLE_TYPE_T) (10 * HT_GROUP_SIZE))	/* Process Manager */
#define HT_GROUP_GL       ((HANDLE_TYPE_T) (11 * HT_GROUP_SIZE))	/* Graphic library */
#define HT_GROUP_BRDCST   ((HANDLE_TYPE_T) (12 * HT_GROUP_SIZE))	/* Broadcast handler */
#define HT_GROUP_IOM      ((HANDLE_TYPE_T) (13 * HT_GROUP_SIZE))	/* IO Manager */
#define HT_GROUP_CL       ((HANDLE_TYPE_T) (14 * HT_GROUP_SIZE))	/* Compression / Decompression library */
#define HT_GROUP_WGL      ((HANDLE_TYPE_T) (15 * HT_GROUP_SIZE))	/* Widget library. RESERVE 256 TYPES. */
#define HT_GROUP_EVCTX    ((HANDLE_TYPE_T) (19 * HT_GROUP_SIZE))	/* Event context */
#define HT_GROUP_SVCTX    ((HANDLE_TYPE_T) (20 * HT_GROUP_SIZE))	/* Service context */
#define HT_GROUP_FLM      ((HANDLE_TYPE_T) (21 * HT_GROUP_SIZE))	/* Favorites List Manager context */
#define HT_GROUP_SVL_BLDR ((HANDLE_TYPE_T) (22 * HT_GROUP_SIZE))	/* SVL builder. */
#define HT_GROUP_AEE      ((HANDLE_TYPE_T) (23 * HT_GROUP_SIZE))	/* AEE Manager */
#define HT_GROUP_FONT     ((HANDLE_TYPE_T) (24 * HT_GROUP_SIZE))	/* Font engine */
#define HT_GROUP_ATV      ((HANDLE_TYPE_T) (25 * HT_GROUP_SIZE))	/* ATV engine */
#define HT_GROUP_CFG      ((HANDLE_TYPE_T) (26 * HT_GROUP_SIZE))	/* Configuration Manager */
#define HT_GROUP_IMG      ((HANDLE_TYPE_T) (27 * HT_GROUP_SIZE))	/* Image library */
#define HT_GROUP_SCDB     ((HANDLE_TYPE_T) (28 * HT_GROUP_SIZE))	/* Stream Component database module */
#define HT_GROUP_SVL      ((HANDLE_TYPE_T) (29 * HT_GROUP_SIZE))	/* Service List library */
#define HT_GROUP_TSL      ((HANDLE_TYPE_T) (30 * HT_GROUP_SIZE))	/* Transport Stream List library */
#define HT_GROUP_DT       ((HANDLE_TYPE_T) (31 * HT_GROUP_SIZE))	/* DateTime library */
#define HT_GROUP_OOB      ((HANDLE_TYPE_T) (32 * HT_GROUP_SIZE))	/* Out-of-band handler */
#define HT_GROUP_1394     ((HANDLE_TYPE_T) (33 * HT_GROUP_SIZE))	/* 1394 handler */
#define HT_GROUP_POD      ((HANDLE_TYPE_T) (34 * HT_GROUP_SIZE))	/* POD stack */
#define HT_GROUP_DSM      ((HANDLE_TYPE_T) (35 * HT_GROUP_SIZE))	/* Device Status Manager */
#define HT_GROUP_ABRDCST  ((HANDLE_TYPE_T) (36 * HT_GROUP_SIZE))	/* Analog Broadcast handler */
#define HT_GROUP_AVC      ((HANDLE_TYPE_T) (37 * HT_GROUP_SIZE))	/* AVC handler */
#define HT_GROUP_RRCTX    ((HANDLE_TYPE_T) (38 * HT_GROUP_SIZE))	/* Rating Recion Context */
#define HT_GROUP_PCL      ((HANDLE_TYPE_T) (39 * HT_GROUP_SIZE))	/* Power Control Library */
#define HT_GROUP_SOCK     ((HANDLE_TYPE_T) (40 * HT_GROUP_SIZE))	/* X-Socket Library */
#define HT_GROUP_VBIF     ((HANDLE_TYPE_T) (41 * HT_GROUP_SIZE))	/* VBI Filter */
#define HT_GROUP_MHEG_5   ((HANDLE_TYPE_T) (42 * HT_GROUP_SIZE))	/* MHEG5 */
#define HT_GROUP_PLAYBACK ((HANDLE_TYPE_T) (43 * HT_GROUP_SIZE))	/* Playback handler */
#define HT_GROUP_MSVCTX   ((HANDLE_TYPE_T) (44 * HT_GROUP_SIZE))	/* Media service context handler */
#define HT_GROUP_MINFO    ((HANDLE_TYPE_T) (45 * HT_GROUP_SIZE))	/* Media info handler */
#define HT_GROUP_MIDXBULD ((HANDLE_TYPE_T) (46 * HT_GROUP_SIZE))	/* Media index builder handler */
#define HT_GROUP_CECM     ((HANDLE_TYPE_T) (47 * HT_GROUP_SIZE))	/* CEC Manager */
#define HT_GROUP_DM       ((HANDLE_TYPE_T) (48 * HT_GROUP_SIZE))	/* Device Manager */
#define HT_GROUP_HBIDEC   ((HANDLE_TYPE_T) (49 * HT_GROUP_SIZE))	/* HBI decode */
#define HT_GROUP_LPCH     ((HANDLE_TYPE_T) (50 * HT_GROUP_SIZE))	/* Legacy Playback Connection Handler */
#define HT_GROUP_CPSM     ((HANDLE_TYPE_T) (51 * HT_GROUP_SIZE))	/* Content Protection Manager */
#define HT_GROUP_SCOM     ((HANDLE_TYPE_T) (52 * HT_GROUP_SIZE))	/* Stream Component Oupput Manager Handler */
#define HT_GROUP_MTP      ((HANDLE_TYPE_T) (53 * HT_GROUP_SIZE))	/* MTP Manager */
#define HT_GROUP_LIBC     ((HANDLE_TYPE_T) (54 * HT_GROUP_SIZE))	/* LIBC */
#define HT_GROUP_AIH      ((HANDLE_TYPE_T) (55 * HT_GROUP_SIZE))	/* Audio In */
#define HT_GROUP_AWAGENT  ((HANDLE_TYPE_T) (56 * HT_GROUP_SIZE))	/* AW Agent */
#define HT_GROUP_NFM      ((HANDLE_TYPE_T) (57 * HT_GROUP_SIZE))	/* NF_Mngr */
#define HT_GROUP_MTKPBLIB ((HANDLE_TYPE_T) (58 * HT_GROUP_SIZE))	/* Network common interface MtkPb_Ctrl */

#define HT_NB_GROUPS  (56)


#endif				/* _U_HANDLE_GRP_H_ */

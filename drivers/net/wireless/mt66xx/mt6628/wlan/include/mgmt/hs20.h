/*
** Id: //Department/DaVinci/BRANCHES/HS2_DEV_SW/MT6620_WIFI_DRIVER_V2_1_HS_2_0/include/mgmt/hs20.h#2
*/

/*! \file   hs20.h
    \brief This file contains the function declaration for hs20.c.
*/

/*******************************************************************************
* Copyright (c) 2013 MediaTek Inc.
*
* All rights reserved. Copying, compilation, modification, distribution
* or any other use whatsoever of this material is strictly prohibited
* except in accordance with a Software License Agreement with
* MediaTek Inc.
********************************************************************************
*/

/*******************************************************************************
* LEGAL DISCLAIMER
*
* BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND
* AGREES THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK
* SOFTWARE") RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE
* PROVIDED TO BUYER ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY
* DISCLAIMS ANY AND ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT
* LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
* PARTICULAR PURPOSE OR NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE
* ANY WARRANTY WHATSOEVER WITH RESPECT TO THE SOFTWARE OF ANY THIRD PARTY
* WHICH MAY BE USED BY, INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK
* SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY
* WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE
* FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S SPECIFICATION OR TO
* CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
* BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
* LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL
* BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT
* ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY
* BUYER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
* THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
* WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT
* OF LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING
* THEREOF AND RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN
* FRANCISCO, CA, UNDER THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE
* (ICC).
********************************************************************************
*/

/*
** Log:
 *
 */

#ifndef _HS20_H
#define _HS20_H

#if CFG_SUPPORT_HOTSPOT_2_0
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define BSSID_POOL_MAX_SIZE             8
#define HS20_SIGMA_SCAN_RESULT_TIMEOUT  30  /*sec*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

#if CFG_ENABLE_GTK_FRAME_FILTER
/*For GTK Frame Filter*/
struct IPV4_NETWORK_ADDRESS_LIST {
	UINT_8                  ucAddrCount;
	IPV4_NETWORK_ADDRESS    arNetAddr[1];
};
#endif

/* Entry of BSSID Pool - For SIGMA Test */
struct BSSID_ENTRY_T {
	UINT_8                  aucBSSID[MAC_ADDR_LEN];
};


struct HS20_INFO_T {

	/*Hotspot 2.0 Information*/
	UINT_8                  aucHESSID[MAC_ADDR_LEN];
	UINT_8                  ucAccessNetworkOptions;
	UINT_8                  ucVenueGroup;                 /* VenueInfo - Group*/
	UINT_8                  ucVenueType;
	UINT_8                  ucHotspotConfig;

	/*Roaming Consortium Information*/
	/*PARAM_HS20_ROAMING_CONSORTIUM_INFO rRCInfo;*/

	/*Hotspot 2.0 dummy AP Info*/

	/*Time Advertisement Information*/
	/*UINT_32                 u4UTCOffsetTime;*/
	/*UINT_8                  aucTimeZone[ELEM_MAX_LEN_TIME_ZONE];*/
	/*UINT_8                  ucLenTimeZone;*/

	/* For SIGMA Test */
	/* BSSID Pool */
	struct BSSID_ENTRY_T	arBssidPool[BSSID_POOL_MAX_SIZE];
	UINT_8					ucNumBssidPoolEntry;
	BOOLEAN					fgIsHS2SigmaMode;

};


/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*For GTK Frame Filter*/
#if DBG
	#define FREE_IPV4_NETWORK_ADDR_LIST(_prAddrList)    \
		{   \
			UINT_32 u4Size = OFFSET_OF(struct IPV4_NETWORK_ADDRESS_LIST, arNetAddr) +  \
					(((_prAddrList)->ucAddrCount) * sizeof(IPV4_NETWORK_ADDRESS));  \
			kalMemFree((_prAddrList), VIR_MEM_TYPE, u4Size);    \
			(_prAddrList) = NULL;   \
		}
#else
	#define FREE_IPV4_NETWORK_ADDR_LIST(_prAddrList)    \
		{   \
			kalMemFree((_prAddrList), VIR_MEM_TYPE, 0);    \
			(_prAddrList) = NULL;   \
		}
#endif

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

VOID
hs20GenerateInterworkingIE(
	IN P_ADAPTER_T     prAdapter,
	OUT P_MSDU_INFO_T   prMsduInfo
	);

VOID
hs20GenerateRoamingConsortiumIE(
	IN P_ADAPTER_T     prAdapter,
	OUT P_MSDU_INFO_T   prMsduInfo
	);

VOID
hs20GenerateHS20IE(
	IN P_ADAPTER_T     prAdapter,
	OUT P_MSDU_INFO_T   prMsduInfo
	);

VOID
hs20FillExtCapIE(
	P_ADAPTER_T     prAdapter,
	P_BSS_INFO_T    prBssInfo,
	P_MSDU_INFO_T   prMsduInfo
	);

VOID
hs20FillProreqExtCapIE(
	IN P_ADAPTER_T          prAdapter,
	OUT PUINT_8             pucIE
	);

VOID
hs20FillHS20IE(
	IN P_ADAPTER_T          prAdapter,
	OUT PUINT_8             pucIE
	);

UINT_32
hs20CalculateHS20RelatedIEForProbeReq(
	IN P_ADAPTER_T          prAdapter,
	IN PUINT_8              pucTargetBSSID
	);

WLAN_STATUS
hs20GenerateHS20RelatedIEForProbeReq(
	IN P_ADAPTER_T          prAdapter,
	IN PUINT_8              pucTargetBSSID,
	OUT PUINT_8             prIE
	);

BOOLEAN
hs20IsGratuitousArp(
	IN P_ADAPTER_T prAdapter,
	IN P_SW_RFB_T prCurrSwRfb
	);

BOOLEAN
hs20IsUnsolicitedNeighborAdv(
	IN P_ADAPTER_T prAdapter,
	IN P_SW_RFB_T prCurrSwRfb
	);

#if CFG_ENABLE_GTK_FRAME_FILTER
BOOLEAN
hs20IsForgedGTKFrame(
	IN P_ADAPTER_T prAdapter,
	IN P_BSS_INFO_T prBssInfo,
	IN P_SW_RFB_T prCurrSwRfb
	);
#endif

BOOLEAN
hs20IsUnsecuredFrame(
	IN P_ADAPTER_T prAdapter,
	IN P_BSS_INFO_T prBssInfo,
	IN P_SW_RFB_T prCurrSwRfb
	);

BOOLEAN
hs20IsFrameFilterEnabled(
	IN P_ADAPTER_T prAdapter,
	IN P_BSS_INFO_T prBssInfo
	);

WLAN_STATUS
hs20SetBssidPool(
	IN P_ADAPTER_T                  prAdapter,
	IN PVOID                        pvBuffer,
	IN ENUM_NETWORK_TYPE_INDEX_T    eNetTypeIdx
	);


#endif
#endif


/*
** $Id: stats.h#1 $
*/

/*! \file stats.h
    \brief This file includes statistics support.
*/


/*
** $Log: stats.h $
 *
 * 07 17 2014 samp.lin
 * NULL
 * Initial version.
 */

/*******************************************************************************
 *						C O M P I L E R	 F L A G S
 ********************************************************************************
 */
#define CFG_SUPPORT_STATS_TX_ENV_ESTIMATION			1

 
/*******************************************************************************
 *						E X T E R N A L	R E F E R E N C E S
 ********************************************************************************
 */

/*******************************************************************************
*						C O N S T A N T S
********************************************************************************
*/

/* Command to TDLS core module */
typedef enum _STATS_CMD_CORE_ID {
	STATS_CORE_CMD_ENV_REQUEST = 0x00
} STATS_CMD_CORE_ID;

typedef enum _STATS_EVENT_HOST_ID {
	STATS_HOST_EVENT_ENV_REPORT = 0x00
} STATS_EVENT_HOST_ID;

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef struct _STATS_CMD_CORE_T {

	UINT32					u4Command; /* STATS_CMD_CORE_ID */

	UINT8					ucStaRecIdx;
	UINT8					ucReserved[3];

	UINT32					u4Reserved[4];

#define STATS_CMD_CORE_RESERVED_SIZE					50
	union{
		UINT8 Reserved[STATS_CMD_CORE_RESERVED_SIZE];
	} Content;

} STATS_CMD_CORE_T;

typedef struct _STATS_INFO_ENV_T {

	BOOLEAN fgIsUsed; /* TRUE: used */

	BOOLEAN fgTxIsRtsUsed; /* TRUE: we use RTS/CTS currently */
	BOOLEAN fgTxIsRtsEverUsed; /* TRUE: we ever use RTS/CTS */
	BOOLEAN fgTxIsCtsSelfUsed; /* TRUE: we use CTS-self */

#define STATS_INFO_TX_PARAM_HW_BW40_OFFSET			0
#define STATS_INFO_TX_PARAM_HW_SHORT_GI20_OFFSET	1
#define STATS_INFO_TX_PARAM_HW_SHORT_GI40_OFFSET	2
#define STATS_INFO_TX_PARAM_USE_BW40_OFFSET			3
#define STATS_INFO_TX_PARAM_USE_SHORT_GI_OFFSET		4
#define STATS_INFO_TX_PARAM_NO_ACK_OFFSET			5
	UINT_8	ucTxParam;

	UINT_8  ucStaRecIdx;
	UINT_8  ucReserved1[2];

	UINT32 u4TxDataCntAll; /* total tx count from host */
	UINT32 u4TxDataCntOK; /* total tx ok count to air */
	UINT32 u4TxDataCntErr; /* total tx err count to air */

	/* WLAN_STATUS_BUFFER_RETAINED ~ WLAN_STATUS_PACKET_LIFETIME_ERROR */
	UINT32 u4TxDataCntErrType[6]; /* total tx err count for different type to air */

	UINT_8 ucTxRate1NonHTMax;
	UINT_8 ucTxRate1HTMax;
	UINT32 u4TxRateCntNonHT[16]; /* tx done rate */
	UINT32 u4TxRateCntHT[16]; /* tx done rate */

	UINT_8 ucTxAggBitmap; /* TX BA sessions TID0 ~ TID7 */
	UINT_8 ucTxPeerAggMaxSize;


	BOOLEAN fgRxIsRtsUsed; /* TRUE: peer uses RTS/CTS currently */
	BOOLEAN fgRxIsRtsEverUsed; /* TRUE: peer ever uses RTS/CTS */

	UINT_8 ucRcvRcpi;
	UINT_8 ucHwChanNum;
	BOOLEAN fgRxIsShortGI;
	UINT_8 ucReserved2[1];

	UINT32 u4RxDataCntAll; /* total rx count from peer */
	UINT32 u4RxDataCntErr; /* total rx err count */
	UINT32 u4RxRateCnt[3][16]; /* [0]:CCK, [1]:OFDM, [2]:MIXED (skip green mode) */

	UINT_8 ucRxAggBitmap; /* RX BA sessions TID0 ~ TID7 */
	UINT_8 ucRxAggMaxSize;

#define STATS_INFO_PHY_MODE_CCK					0
#define STATS_INFO_PHY_MODE_OFDM				1
#define STATS_INFO_PHY_MODE_HT					2
#define STATS_INFO_PHY_MODE_VHT					3
	UINT_8 ucBssSupPhyMode; /* CCK, OFDM, HT, or VHT BSS */

	UINT_8 ucReserved3[5];
} STATS_INFO_ENV_T;


/*******************************************************************************
*						F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*						P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*						P R I V A T E  F U N C T I O N S
********************************************************************************
*/

/*******************************************************************************
*						P U B L I C  F U N C T I O N S
********************************************************************************
*/

WLAN_STATUS
statsInfoEnvRequest(
	ADAPTER_T 	  						*prAdapter,
	VOID			  					*pvSetBuffer,
	UINT_32 		  					u4SetBufferLen,
	UINT_32		  						*pu4SetInfoLen
	);

VOID
statsEnvReportDetect(
	ADAPTER_T							*prAdapter,
	UINT8								ucStaRecIndex
	);

VOID
statsEventHandle(
	GLUE_INFO_T							*prGlueInfo,
	UINT8								*prInBuf,
	UINT32 								u4InBufLen
	);

/* End of stats.h */



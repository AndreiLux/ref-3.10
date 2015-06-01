/*
** $Id: stats.c#1 $
*/

/*! \file stats.c
    \brief This file includes statistics support.
*/


/*
** $Log: stats.c $
 *
 * 07 17 2014 samp.lin
 * NULL
 * Initial version.
 */

/*******************************************************************************
 *						C O M P I L E R	 F L A G S
 ********************************************************************************
 */
 
/*******************************************************************************
 *						E X T E R N A L	R E F E R E N C E S
 ********************************************************************************
 */
#include "precomp.h"

#if (CFG_SUPPORT_STATISTICS == 1)

/*******************************************************************************
*						C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*						F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
#if (CFG_SUPPORT_STATS_TX_ENV_ESTIMATION == 1)
static void
statsInfoEnvDisplay(
	GLUE_INFO_T							*prGlueInfo,
	UINT8								*prInBuf,
	UINT32 								u4InBufLen
	);
#endif /* CFG_SUPPORT_STATS_TX_ENV_ESTIMATION */

/*******************************************************************************
*						P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*						P R I V A T E  F U N C T I O N S
********************************************************************************
*/

#if (CFG_SUPPORT_STATS_TX_ENV_ESTIMATION == 1)
/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to display environment log.
*
* \param[in] prGlueInfo		Pointer to the Adapter structure
* \param[in] prInBuf		A pointer to the command string buffer, from u4EventSubId
* \param[in] u4InBufLen	The length of the buffer
* \param[out] None
*
* \retval None
*
*/
/*----------------------------------------------------------------------------*/
static void
statsInfoEnvDisplay(
	GLUE_INFO_T							*prGlueInfo,
	UINT8								*prInBuf,
	UINT32 								u4InBufLen
	)
{
	P_ADAPTER_T prAdapter;
	STA_RECORD_T *prStaRec;
	UINT32 u4NumOfInfo, u4InfoId;
	UINT32 u4RxErrBitmap;
	STATS_INFO_ENV_T rStatsInfoEnv, *prInfo;


	/* init */
	prAdapter = prGlueInfo->prAdapter;
	prInfo = &rStatsInfoEnv;

	/* parse */
	u4NumOfInfo = *(UINT32 *)prInBuf;
	u4RxErrBitmap = *(UINT32 *)(prInBuf+4);

	/* print */
	for(u4InfoId=0; u4InfoId<u4NumOfInfo; u4InfoId++)
	{
		kalMemCopy(&rStatsInfoEnv, prInBuf+8, sizeof(STATS_INFO_ENV_T));

		prStaRec = cnmGetStaRecByIndex(prAdapter, rStatsInfoEnv.ucStaRecIdx);
		if (prStaRec == NULL)
			continue;

		DBGLOG(INIT, INFO, ("<stats> Display stats for ["MACSTR"]:\n",
			MAC2STR(prStaRec->aucMacAddr)));
		DBGLOG(INIT, INFO, ("<stats> TPAM(0x%x) RTS(%d %d) BA(0x%x %d) "
			"OK(%d %d) ERR(%d %d %d %d %d %d %d)\n",
			prInfo->ucTxParam,
			prInfo->fgTxIsRtsUsed, prInfo->fgTxIsRtsEverUsed,
			prInfo->ucTxAggBitmap, prInfo->ucTxPeerAggMaxSize,
			prInfo->u4TxDataCntAll, prInfo->u4TxDataCntOK,
			prInfo->u4TxDataCntErr,	prInfo->u4TxDataCntErrType[0],
			prInfo->u4TxDataCntErrType[1], prInfo->u4TxDataCntErrType[2],
			prInfo->u4TxDataCntErrType[3], prInfo->u4TxDataCntErrType[4],
			prInfo->u4TxDataCntErrType[5]));

		DBGLOG(INIT, INFO, ("<stats> TRATE "
			"(%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d) "
			"(%d %d %d %d %d %d %d %d)\n",
			prInfo->u4TxRateCntNonHT[0], prInfo->u4TxRateCntNonHT[1],
			prInfo->u4TxRateCntNonHT[2], prInfo->u4TxRateCntNonHT[3],
			prInfo->u4TxRateCntNonHT[4], prInfo->u4TxRateCntNonHT[5],
			prInfo->u4TxRateCntNonHT[6], prInfo->u4TxRateCntNonHT[7],
			prInfo->u4TxRateCntNonHT[8], prInfo->u4TxRateCntNonHT[9],
			prInfo->u4TxRateCntNonHT[10], prInfo->u4TxRateCntNonHT[11],
			prInfo->u4TxRateCntNonHT[12], prInfo->u4TxRateCntNonHT[13],
			prInfo->u4TxRateCntNonHT[14], prInfo->u4TxRateCntNonHT[15],
			prInfo->u4TxRateCntHT[0], prInfo->u4TxRateCntHT[1],
			prInfo->u4TxRateCntHT[2], prInfo->u4TxRateCntHT[3],
			prInfo->u4TxRateCntHT[4], prInfo->u4TxRateCntHT[5],
			prInfo->u4TxRateCntHT[6], prInfo->u4TxRateCntHT[7]));

		DBGLOG(INIT, INFO, ("<stats> RX(%d %d %d) BA(0x%x %d) OK(%d) ERR(%d)\n",
			prInfo->ucRcvRcpi, prInfo->ucHwChanNum, prInfo->fgRxIsShortGI,
			prInfo->ucRxAggBitmap, prInfo->ucRxAggMaxSize,
			prInfo->u4RxDataCntAll, prInfo->u4RxDataCntErr));

		DBGLOG(INIT, INFO, ("<stats> RCCK "
			"(%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d)\n",
			prInfo->u4RxRateCnt[0][0], prInfo->u4RxRateCnt[0][1],
			prInfo->u4RxRateCnt[0][2], prInfo->u4RxRateCnt[0][3],
			prInfo->u4RxRateCnt[0][4], prInfo->u4RxRateCnt[0][5],
			prInfo->u4RxRateCnt[0][6], prInfo->u4RxRateCnt[0][7],
			prInfo->u4RxRateCnt[0][8], prInfo->u4RxRateCnt[0][9],
			prInfo->u4RxRateCnt[0][10], prInfo->u4RxRateCnt[0][11],
			prInfo->u4RxRateCnt[0][12], prInfo->u4RxRateCnt[0][13],
			prInfo->u4RxRateCnt[0][14], prInfo->u4RxRateCnt[0][15]));
		DBGLOG(INIT, INFO, ("<stats> ROFDM "
			"(%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d)\n",
			prInfo->u4RxRateCnt[1][0], prInfo->u4RxRateCnt[1][1],
			prInfo->u4RxRateCnt[1][2], prInfo->u4RxRateCnt[1][3],
			prInfo->u4RxRateCnt[1][4], prInfo->u4RxRateCnt[1][5],
			prInfo->u4RxRateCnt[1][6], prInfo->u4RxRateCnt[1][7],
			prInfo->u4RxRateCnt[1][8], prInfo->u4RxRateCnt[1][9],
			prInfo->u4RxRateCnt[1][10], prInfo->u4RxRateCnt[1][11],
			prInfo->u4RxRateCnt[1][12], prInfo->u4RxRateCnt[1][13],
			prInfo->u4RxRateCnt[1][14], prInfo->u4RxRateCnt[1][15]));
		DBGLOG(INIT, INFO, ("<stats> RHT "
			"(%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d)\n",
			prInfo->u4RxRateCnt[2][0], prInfo->u4RxRateCnt[2][1],
			prInfo->u4RxRateCnt[2][2], prInfo->u4RxRateCnt[2][3],
			prInfo->u4RxRateCnt[2][4], prInfo->u4RxRateCnt[2][5],
			prInfo->u4RxRateCnt[2][6], prInfo->u4RxRateCnt[2][7],
			prInfo->u4RxRateCnt[2][8], prInfo->u4RxRateCnt[2][9],
			prInfo->u4RxRateCnt[2][10], prInfo->u4RxRateCnt[2][11],
			prInfo->u4RxRateCnt[2][12], prInfo->u4RxRateCnt[2][13],
			prInfo->u4RxRateCnt[2][14], prInfo->u4RxRateCnt[2][15]));
	}
}
#endif /* CFG_SUPPORT_STATS_TX_ENV_ESTIMATION */


/*******************************************************************************
*						P U B L I C  F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to configure channel switch parameters.
*
* \param[in] prAdapter			Pointer to the Adapter structure
* \param[in] pvSetBuffer		A pointer to the buffer that holds the data to be set
* \param[in] u4SetBufferLen		The length of the set buffer
* \param[out] pu4SetInfoLen	If the call is successful, returns the number of
*	bytes read from the set buffer. If the call failed due to invalid length of
*	the set buffer, returns the amount of storage needed.
*
* \retval TDLS_STATUS_xx
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
statsInfoEnvRequest(
	ADAPTER_T 	  						*prAdapter,
	VOID			  					*pvSetBuffer,
	UINT_32 		  					u4SetBufferLen,
	UINT_32		  						*pu4SetInfoLen
	)
{
#if (CFG_SUPPORT_STATS_TX_ENV_ESTIMATION == 1)
	STATS_CMD_CORE_T *prCmdContent;
	WLAN_STATUS rStatus;


	/* init command buffer */
	prCmdContent = (STATS_CMD_CORE_T *)pvSetBuffer;
	prCmdContent->u4Command = STATS_CORE_CMD_ENV_REQUEST;

	/* send the command */
	rStatus = wlanSendSetQueryCmd (
		prAdapter,					/* prAdapter */
		CMD_ID_STATS,				/* ucCID */
		TRUE,						/* fgSetQuery */
		FALSE, 			   			/* fgNeedResp */
		FALSE,						/* fgIsOid */
		NULL,
		NULL,						/* pfCmdTimeoutHandler */
		sizeof(STATS_CMD_CORE_T),	/* u4SetQueryInfoLen */
		(PUINT_8) prCmdContent, 	/* pucInfoBuffer */
		NULL,						/* pvSetQueryBuffer */
		0							/* u4SetQueryBufferLen */
		);

	if (rStatus != WLAN_STATUS_PENDING)
	{
		DBGLOG(INIT, ERROR, ("%s wlanSendSetQueryCmd allocation fail!\n",
			__FUNCTION__));
		return WLAN_STATUS_RESOURCES;
	}

	DBGLOG(INIT, INFO, ("%s cmd ok.\n", __FUNCTION__));
#endif /* CFG_SUPPORT_STATS_TX_ENV_ESTIMATION */
	return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to send a command to TDLS module.
*
* \param[in] prGlueInfo		Pointer to the Adapter structure
* \param[in] prInBuf		A pointer to the command string buffer
* \param[in] u4InBufLen	The length of the buffer
* \param[out] None
*
* \retval None
*/
/*----------------------------------------------------------------------------*/
VOID
statsEventHandle(
	GLUE_INFO_T							*prGlueInfo,
	UINT8								*prInBuf,
	UINT32 								u4InBufLen
	)
{
	UINT32 u4EventId;


	/* sanity check */
//	DBGLOG(INIT, INFO,
//		("<stats> %s: Rcv a event\n", __FUNCTION__));

	if ((prGlueInfo == NULL) || (prInBuf == NULL))
		return; /* shall not be here */

	/* handle */
	u4EventId = *(UINT32 *)prInBuf;
	u4InBufLen -= 4;

//	DBGLOG(INIT, INFO,
//		("<stats> %s: Rcv a event: %d\n", __FUNCTION__, u4EventId));

	switch(u4EventId)
	{
#if (CFG_SUPPORT_STATS_TX_ENV_ESTIMATION == 1)
		case STATS_HOST_EVENT_ENV_REPORT:
			statsInfoEnvDisplay(prGlueInfo, prInBuf+4, u4InBufLen);
			break;
#endif /* CFG_SUPPORT_STATS_TX_ENV_ESTIMATION */

		default:
			break;
	}
}


/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to detect if we need to get a report.
*
* \param[in] prGlueInfo		Pointer to the Adapter structure
* \param[in] prInBuf		A pointer to the command string buffer
* \param[in] u4InBufLen	The length of the buffer
* \param[out] None
*
* \retval None
*/
/*----------------------------------------------------------------------------*/
VOID
statsEnvReportDetect(
	ADAPTER_T							*prAdapter,
	UINT8								ucStaRecIndex
	)
{
	STA_RECORD_T *prStaRec;
	OS_SYSTIME rCurTime;
	STATS_CMD_CORE_T rCmd;


	prStaRec = cnmGetStaRecByIndex(prAdapter, ucStaRecIndex);
	if (prStaRec == NULL)
		return;

	prStaRec->u4StatsEnvTxCnt ++;
	GET_CURRENT_SYSTIME(&rCurTime);

	if (prStaRec->rStatsEnvTxPeriodLastTime == 0)
	{
		prStaRec->rStatsEnvTxLastTime = rCurTime;
		prStaRec->rStatsEnvTxPeriodLastTime = rCurTime;
		return;
	}

	if (prStaRec->u4StatsEnvTxCnt > STATS_ENV_TX_CNT_REPORT_TRIGGER)
	{
		if (CHECK_FOR_TIMEOUT(rCurTime, prStaRec->rStatsEnvTxLastTime,
			SEC_TO_SYSTIME(STATS_ENV_TX_CNT_REPORT_TRIGGER_SEC)))
		{
			rCmd.ucStaRecIdx = ucStaRecIndex;
			statsInfoEnvRequest(prAdapter, &rCmd, 0, NULL);

			prStaRec->rStatsEnvTxLastTime = rCurTime;
			prStaRec->rStatsEnvTxPeriodLastTime = rCurTime;
			prStaRec->u4StatsEnvTxCnt = 0;
			return;
		}
	}

	if (CHECK_FOR_TIMEOUT(rCurTime, prStaRec->rStatsEnvTxPeriodLastTime,
		SEC_TO_SYSTIME(STATS_ENV_TIMEOUT_SEC)))
	{
		rCmd.ucStaRecIdx = ucStaRecIndex;
		statsInfoEnvRequest(prAdapter, &rCmd, 0, NULL);

		prStaRec->rStatsEnvTxPeriodLastTime = rCurTime;
		return;
	}
}

#endif /* CFG_SUPPORT_STATISTICS */

/* End of stats.c */



#include "UDVT.h"
#include "UDVT_IF.h"



void UDVT_IF_SendResult(UDVT_TEST_RESULT_e TestResult)
{
	if (TestResult == UDVT_TEST_PASS) {
		UDVT_IF_TestProgress(100);
	} else {
		UDVT_IF_TestProgress(0xFFFF);
	}
}


void UDVT_IF_TestProgress(UINT32 u4Percent)
{
	HANDLE_T hTestCaseThread;
	UDVT_CompletePacket_s *pPck;
	if (OSR_OK == x_thread_self(&hTestCaseThread)) {
		pPck =
		    UDVT_GetTestCasePacketByThreadHandle(hTestCaseThread,
							 UDVT_GET_TESTCASE_PACKET_OUTPUT);
		if (pPck != 0) {
			pPck->lData[0] = u4Percent;
			pPck->sPacketHeader.PacketLength = 12 + 4;
			pPck->sPacketHeader.lPacketChecksum = 0;
			pPck->sPacketHeader.PacketHeader =
			    UDVT_FrameTheHeader(UDVT_DATA_PACKET, UDVT_ACK, FALSE);
			pPck->lCommand = UDVT_CMD_TEST_STATUS;
			pPck->sPacketHeader.lPacketChecksum = UDVT_CalculateCheckSum(pPck);
			UDVT_PostMessageToHost(pPck);
		}
	}
}



UINT32 UDVT_IF_OpenFile(const char *filename, const char *mode)
{
	HANDLE_T hTestCaseThread;
	UINT32 n = 0;
	UDVT_CompletePacket_s *pPck;
	UDVT_OpenFile_s *pOpenFileS;
	UINT32 tstCaseIndex;
	UINT32 fileHandle = 0;
	char *fname;
	char *fmode;
	fname = (char *)filename;
	fmode = (char *)mode;



	if (OSR_OK == x_thread_self(&hTestCaseThread)) {
		pPck =
		    UDVT_GetTestCasePacketByThreadHandle(hTestCaseThread,
							 UDVT_GET_TESTCASE_PACKET_OUTPUT);
		if (pPck != 0) {
			pOpenFileS = (UDVT_OpenFile_s *) (pPck->lData);
			while ((*fmode) && (n < UDVT_OPENFILE_MODE_DATASIZE)) {
				pOpenFileS->fmode[n] = *fmode;
				fmode++;
				n++;
			}
			pOpenFileS->fmode[n] = 0;

			n = 0;
			while ((*fname) && (n < UDVT_OPENFILE_FILENAME_LEN)) {
				pOpenFileS->fname[n] = *fname;
				fname++;
				n++;
			}
			pOpenFileS->fname[n] = 0;
			n = (n + 1 + 3) / 4;
			pPck->sPacketHeader.PacketLength = 12 + UDVT_OPENFILE_MODE_DATASIZE + n * 4;
			pPck->sPacketHeader.lPacketChecksum = 0;
			pPck->sPacketHeader.PacketHeader =
			    UDVT_FrameTheHeader(UDVT_DATA_PACKET, UDVT_ACK, FALSE);
			pPck->lCommand = UDVT_CMD_TEST_OPENFILE;
			pPck->sPacketHeader.lPacketChecksum = UDVT_CalculateCheckSum(pPck);
			UDVT_LOG(3, "[UDVT]Open file %s,mode %s\n", filename, mode);
			UDVT_PostMessageToHost(pPck);
			tstCaseIndex = UDVT_FindWorkTestCaseByThreadHandle(hTestCaseThread);
			UDVT_WaitPCResponse(tstCaseIndex, 0);

			fileHandle = UDVT_GetTestCaseOpenFileHandle(tstCaseIndex);
		}



	}

	return fileHandle;
}


UINT32 UDVT_IF_GetFileLength(UINT32 stream)
{
	UINT32 fileLength;
	HANDLE_T hTestCaseThread;
	UINT32 tstCaseIndex;
	UDVT_CompletePacket_s *pPck;

	fileLength = 0;
	if (OSR_OK == x_thread_self(&hTestCaseThread)) {
		pPck =
		    UDVT_GetTestCasePacketByThreadHandle(hTestCaseThread,
							 UDVT_GET_TESTCASE_PACKET_OUTPUT);
		if (pPck != 0) {

			pPck->sPacketHeader.PacketLength = 12 + 4;
			pPck->sPacketHeader.lPacketChecksum = 0;
			pPck->sPacketHeader.PacketHeader =
			    UDVT_FrameTheHeader(UDVT_DATA_PACKET, UDVT_ACK, FALSE);
			pPck->lData[0] = stream;

			pPck->lCommand = UDVT_CMD_TEST_GET_FILELENGTH;
			pPck->sPacketHeader.lPacketChecksum = UDVT_CalculateCheckSum(pPck);
			UDVT_PostMessageToHost(pPck);
			tstCaseIndex = UDVT_FindWorkTestCaseByThreadHandle(hTestCaseThread);
			UDVT_WaitPCResponse(tstCaseIndex, 0);

			fileLength = UDVT_GetFileLength(tstCaseIndex);
			UDVT_LOG(3, "[UDVT]The file length is %d\n", fileLength);

		}
	}

	return fileLength;
}


UINT32 FATS_IF_WaitResponse(const char *pMsgStr, UINT32 timeout)
{
	UDVT_CompletePacket_s *pPck;
	HANDLE_T hTestCaseThread;
	UINT32 tstCaseIndex;
	UINT32 msgN;
	char *pStr;
	if (OSR_OK == x_thread_self(&hTestCaseThread)) {
		pPck =
		    UDVT_GetTestCasePacketByThreadHandle(hTestCaseThread,
							 UDVT_GET_TESTCASE_PACKET_OUTPUT);
		if (pPck != 0) {
			pStr = (char *)(&pPck->lData[1]);
			msgN = 0;
			while (*pMsgStr != 0) {
				*pStr = *pMsgStr;
				pStr++;
				pMsgStr++;
				msgN++;
			}
			*pStr = 0;
			msgN = (msgN + 1 + 3) / 4;
			pPck->sPacketHeader.PacketLength = 12 + 4 + (msgN << 2);
			pPck->sPacketHeader.lPacketChecksum = 0;
			pPck->sPacketHeader.PacketHeader =
			    UDVT_FrameTheHeader(UDVT_DATA_PACKET, UDVT_ACK, FALSE);
			pPck->lData[0] = timeout;

			pPck->lCommand = UDVT_CMD_TEST_WAIT;
			pPck->sPacketHeader.lPacketChecksum = UDVT_CalculateCheckSum(pPck);
			UDVT_PostMessageToHost(pPck);
			UDVT_LOG(3, "[FATS]Wait response\n");
			tstCaseIndex = UDVT_FindWorkTestCaseByThreadHandle(hTestCaseThread);
			UDVT_WaitPCResponse(tstCaseIndex, 0);
			return UDVT_GetWaitResponse(tstCaseIndex);


		}
	}
	return 0;
}


UINT32 UDVT_IF_CloseFile(UINT32 stream)
{
	HANDLE_T hTestCaseThread;
	UDVT_CompletePacket_s *pPck;
	UINT32 tstCaseIndex;

	if (OSR_OK == x_thread_self(&hTestCaseThread)) {
		pPck =
		    UDVT_GetTestCasePacketByThreadHandle(hTestCaseThread,
							 UDVT_GET_TESTCASE_PACKET_OUTPUT);
		if (pPck != 0) {

			pPck->sPacketHeader.PacketLength = 12 + 4;
			pPck->sPacketHeader.lPacketChecksum = 0;
			pPck->sPacketHeader.PacketHeader =
			    UDVT_FrameTheHeader(UDVT_DATA_PACKET, UDVT_ACK, FALSE);
			pPck->lData[0] = stream;

			pPck->lCommand = UDVT_CMD_TEST_CLOSEFILE;
			pPck->sPacketHeader.lPacketChecksum = UDVT_CalculateCheckSum(pPck);
			UDVT_PostMessageToHost(pPck);
			tstCaseIndex = UDVT_FindWorkTestCaseByThreadHandle(hTestCaseThread);
			UDVT_WaitPCResponse(tstCaseIndex, 0);


			UDVT_LOG(3, "[UDVT]close file\n");

		}
	}

	return 0;
}


UINT32 UDVT_IF_SeekFile(UINT32 stream, INT32 offset, UINT32 origin)
{
	HANDLE_T hTestCaseThread;
	UDVT_CompletePacket_s *pPck;
	UINT32 tstCaseIndex;
	UINT32 ret = 1;
	if (OSR_OK == x_thread_self(&hTestCaseThread)) {
		pPck =
		    UDVT_GetTestCasePacketByThreadHandle(hTestCaseThread,
							 UDVT_GET_TESTCASE_PACKET_OUTPUT);
		if (pPck != 0) {

			pPck->sPacketHeader.PacketLength = 12 + 12;
			pPck->sPacketHeader.lPacketChecksum = 0;
			pPck->sPacketHeader.PacketHeader =
			    UDVT_FrameTheHeader(UDVT_DATA_PACKET, UDVT_ACK, FALSE);
			pPck->lData[0] = stream;
			pPck->lData[1] = (UINT32) offset;
			pPck->lData[2] = origin;

			pPck->lCommand = UDVT_CMD_TEST_SEEKFILE;
			pPck->sPacketHeader.lPacketChecksum = UDVT_CalculateCheckSum(pPck);
			UDVT_PostMessageToHost(pPck);
			tstCaseIndex = UDVT_FindWorkTestCaseByThreadHandle(hTestCaseThread);
			UDVT_WaitPCResponse(tstCaseIndex, 0);
			ret = UDVT_GetFileResponse(tstCaseIndex);

			UDVT_LOG(3, "[UDVT]The seek file response %d\n", ret);

		}
	}
	return ret;
}


UINT32 UDVT_IF_DebugCheckSum(void *ptr, UINT32 count)
{
	UINT8 *pData;
	UINT32 n;
	UINT32 checkSum = 0;
	UINT32 crcValue = 0;
	UINT32 oneCRC = 0;
	pData = (UINT8 *) ptr;

	for (n = 0; n < count; n++) {
		checkSum += pData[n];
		if ((n & 0x03) == 0) {
			crcValue ^= oneCRC;
			oneCRC = ((pData[n]) << 24);

		} else {
			oneCRC = (oneCRC >> 8);
			oneCRC += ((pData[n]) << 24);
		}
	}


	crcValue ^= oneCRC;
	UDVT_LOG(3, "[UDVT] CheckSum is %x,crc is %x\n", checkSum, crcValue);

	return checkSum;
}


UINT32 FATS_IF_SaveLog(UINT32 logLvl, const char *pformat, ...)
{
	HANDLE_T hTestCaseThread;
	UDVT_CompletePacket_s *pPck;
	UINT32 tmpN;
	char *pLogStr;
	va_list t_ap;
	UINT32 strn;
	if (OSR_OK == x_thread_self(&hTestCaseThread)) {
		pPck =
		    UDVT_GetTestCasePacketByThreadHandle(hTestCaseThread,
							 UDVT_GET_TESTCASE_PACKET_OUTPUT);
		if (pPck != 0) {

			pPck->sPacketHeader.PacketLength = 12 + 4;
			pPck->sPacketHeader.lPacketChecksum = 0;
			pPck->sPacketHeader.PacketHeader =
			    UDVT_FrameTheHeader(UDVT_DATA_PACKET, UDVT_ACK, FALSE);
			pPck->lData[0] = logLvl;
			pLogStr = (char *)(&pPck->lData[1]);
			va_start(t_ap, pformat);
			vsprintf(pLogStr, (const char *)pformat, t_ap);
			va_end(t_ap);
			UDVT_LOG(3, "[UDVT] vsprintf:%s", pLogStr);
			strn = 0;
			while (*pLogStr != 0) {
				strn++;
				pLogStr++;
			}
			strn++;
			pPck->sPacketHeader.PacketLength += strn;
			tmpN = ((pPck->sPacketHeader.PacketLength + 3) >> 2);
			pPck->sPacketHeader.PacketLength = (tmpN << 2);
			pPck->lCommand = UDVT_CMD_TEST_SAVELOG;
			pPck->sPacketHeader.lPacketChecksum = UDVT_CalculateCheckSum(pPck);
			UDVT_PostMessageToHost(pPck);



		}
	}
	return 0;
}

UINT32 UDVT_IF_WriteFile(void *ptr, UINT32 size, UINT32 count, UINT32 stream)
{
	HANDLE_T hTestCaseThread;
	UINT32 BuffSize;
	UDVT_CompletePacket_s *pPck;
	UINT32 tstCaseIndex;

	UINT32 checksum;

	BuffSize = size * count;
	if (ptr == 0 || BuffSize == 0) {
		UDVT_LOG(3, "[UDVT] Write Buff is not prepare\n");
		return 0;
	}

	if (OSR_OK == x_thread_self(&hTestCaseThread)) {

		pPck =
		    UDVT_GetTestCasePacketByThreadHandle(hTestCaseThread,
							 UDVT_GET_TESTCASE_PACKET_OUTPUT);
		if (pPck != 0) {
			pPck->sPacketHeader.PacketLength = 12 + 8;
			pPck->sPacketHeader.lPacketChecksum = 0;
			pPck->sPacketHeader.PacketHeader =
			    UDVT_FrameTheHeader(UDVT_DATA_PACKET, UDVT_ACK, FALSE);
			pPck->lData[0] = stream;
			pPck->lData[1] = BuffSize;
			pPck->lCommand = UDVT_CMD_TEST_WRITEFILE;
			pPck->sPacketHeader.lPacketChecksum = UDVT_CalculateCheckSum(pPck);
			UDVT_PostMessageToHost(pPck);
			tstCaseIndex = UDVT_FindWorkTestCaseByThreadHandle(hTestCaseThread);
			UDVT_WaitPCResponse(tstCaseIndex, 0);

			if (UDVT_IsFileWriteable(tstCaseIndex) == UDVT_WRITEFILE_WRITABLE) {
				UDVT_WriteFiletoPort(ptr, BuffSize);

				UDVT_LOG(3, "[UDVT] Write file complete\n");

				/* tstCaseIndex = UDVT_FindWorkTestCaseByThreadHandle(hTestCaseThread); */
				/* UDVT_WaitPCResponse(tstCaseIndex,0); */
				checksum = 0;


				checksum = UDVT_IF_DebugCheckSum(ptr, BuffSize);

				UDVT_LOG(3, "[UDVT] Write file complete,checksum is 0x%x\n",
					 checksum);
			} else {
				BuffSize = 0;
				UDVT_LOG(3, "[UDVT] Unwritable file");
			}
		}
	}
	/* HalFlushInvalidateDCache(); */
	return BuffSize;
}



UINT32 UDVT_IF_ReadFile(void *ptr, UINT32 size, UINT32 count, UINT32 stream)
{
	HANDLE_T hTestCaseThread;
	UINT32 BuffSize;
	UDVT_CompletePacket_s *pPck;
	UINT32 tstCaseIndex;
	UINT32 restFileLength;
	UINT32 checksum;

	restFileLength = 0;
	BuffSize = size * count;
	if (ptr == 0 || BuffSize == 0) {
		UDVT_LOG(3, "[UDVT] Read Buff is not prepare\n");
		return 0;
	}

	if (OSR_OK == x_thread_self(&hTestCaseThread)) {
		pPck =
		    UDVT_GetTestCasePacketByThreadHandle(hTestCaseThread,
							 UDVT_GET_TESTCASE_PACKET_OUTPUT);
		if (pPck != 0) {

			pPck->sPacketHeader.PacketLength = 12 + 8;
			pPck->sPacketHeader.lPacketChecksum = 0;
			pPck->sPacketHeader.PacketHeader =
			    UDVT_FrameTheHeader(UDVT_DATA_PACKET, UDVT_ACK, FALSE);
			pPck->lData[0] = stream;
			pPck->lData[1] = BuffSize;

			pPck->lCommand = UDVT_CMD_TEST_GET_RESTFILELENGTH;
			pPck->sPacketHeader.lPacketChecksum = UDVT_CalculateCheckSum(pPck);
			UDVT_PostMessageToHost(pPck);
			tstCaseIndex = UDVT_FindWorkTestCaseByThreadHandle(hTestCaseThread);
			UDVT_WaitPCResponse(tstCaseIndex, 0);

			restFileLength = UDVT_GetFileLength(tstCaseIndex);
			UDVT_LOG(3, "[UDVT]The rest file length is %d\n", restFileLength);

		}

		if (restFileLength > 0) {
			if (restFileLength > BuffSize) {
				restFileLength = BuffSize;
			}
			pPck =
			    UDVT_GetTestCasePacketByThreadHandle(hTestCaseThread,
								 UDVT_GET_TESTCASE_PACKET_OUTPUT);
			if (pPck != 0) {
				pPck->sPacketHeader.PacketLength = 12 + 8;
				pPck->sPacketHeader.lPacketChecksum = 0;
				pPck->sPacketHeader.PacketHeader =
				    UDVT_FrameTheHeader(UDVT_DATA_PACKET, UDVT_ACK, FALSE);
				pPck->lData[0] = stream;
				pPck->lData[1] = restFileLength;
				pPck->lCommand = UDVT_CMD_TEST_READFILE;
				pPck->sPacketHeader.lPacketChecksum = UDVT_CalculateCheckSum(pPck);
				UDVT_PostMessageToHost(pPck);
				UDVT_ReadFileFromPort(ptr, restFileLength);

				UDVT_LOG(1, "[UDVT] Read file complete\n");
				/* tstCaseIndex = UDVT_FindWorkTestCaseByThreadHandle(hTestCaseThread); */
				/* UDVT_WaitPCResponse(tstCaseIndex,0); */
				checksum = 0;


				checksum = UDVT_IF_DebugCheckSum(ptr, restFileLength);

				UDVT_LOG(3, "[UDVT] Read file complete,checksum is 0x%x\n",
					 checksum);
			}
		}
	}
	/* HalFlushInvalidateDCache(); */
	return restFileLength;
}


#if 0
void UDVT_IF_TestProgress(UINT32 u4Percent)
{
	HANDLE_T hTestCaseThread;
	if (u4Percent > 100) {
		return;
	}

	if (OSR_OK == x_thread_self(&hTestCaseThread)) {

		UDVT_ReportTestCaseProgress(hTestCaseThread, u4Percent);
	}
}

#endif

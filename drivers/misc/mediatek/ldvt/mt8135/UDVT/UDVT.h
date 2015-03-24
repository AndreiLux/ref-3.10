#ifndef _UDVT_H_
#define _UDVT_H_

#include <linux/completion.h>
#include "x_os.h"
#include "x_assert.h"


#define UDVT_LOG(level, fmt...)		UDVT_LOG_##level(fmt)
#define UDVT_LOG_0(fmt...)			printk(fmt)	/* info */
#define UDVT_LOG_1(fmt...)			printk(fmt)	/* error */
#define UDVT_LOG_2(fmt...)          printk(fmt)	/* warning */
#define UDVT_LOG_3(fmt...)          printk(fmt)	/* debug */

#define UDVT_PACKET_MAX_DATA_LENGTH   250
#define UDVT_CHECKSUM_OK    1
#define UDVT_CHECKSUM_ERR   0

#define UDVT_GETDATA_TIMEOUT    200
#define UDVT_SENDDATA_TIMEOUT   200

#define UDVT_MAX_WORK_TESTCASE    10
#define UDVT_MAX_TESTCASE_MSG_NUM 10

#define UDVT_MAX_UDVT_INPUT_MSG_NUM    100
#define UDVT_MAX_UDVT_OUTPUT_MSG_NUM    100

#define UDVT_WRITEFILE_WRITABLE       1
#define UDVT_WRITEFILE_UNWRITABLE     0

typedef enum {
	UDVT_CMD_MODULE_INIT_REQUEST = 0,
	UDVT_CMD_MODULE_INIT_RESPONSE,
	UDVT_CMD_VERSION_NUM_REQUEST,
	UDVT_CMD_VERSION_NUM_RESPONSE,
	UDVT_CMD_TEST_INIT_REQUEST,
	UDVT_CMD_TEST_INIT_RESPONSE,
	UDVT_CMD_TEST_START_REQUEST,
	UDVT_CMD_TEST_STATUS,
	UDVT_CMD_DATA_REQUEST,
	UDVT_CMD_DATA_RESPONSE,
	UDVT_CMD_TEST_DEINIT_REQUEST,
	UDVT_CMD_TEST_DEINIT_RESPONSE,
	UDVT_CMD_MODULE_DEINIT_REQUEST,
	UDVT_CMD_MODULE_DEINIT_RESPONSE,
	UDVT_CMD_TEST_SKIP_REQUEST,
	UDVT_CMD_TEST_SKIP_RESPONSE,
	UDVT_CMD_TEST_STOP_REQUEST,
	UDVT_CMD_TEST_STOP_RESPONSE,
	UDVT_CMD_TEST_OPENFILE,
	UDVT_CMD_TEST_OPENFILE_RESPONSE,
	UDVT_CMD_TEST_GET_RESTFILELENGTH,
	UDVT_CMD_TEST_GET_RESTFILELENGTH_RESPONSE,
	UDVT_CMD_TEST_READFILE,
	UDVT_CMD_TEST_GET_FILELENGTH,
	UDVT_CMD_TEST_GET_FILELENGTH_RESPONSE,
	UDVT_CMD_TEST_CLOSEFILE,
	UDVT_CMD_TEST_CLOSEFILE_RESPONSE,
	UDVT_CMD_TEST_SEEKFILE,
	UDVT_CMD_TEST_SEEKFILE_RESPONSE,
	UDVT_CMD_TEST_WAIT,
	UDVT_CMD_TEST_WAIT_RESPONSE,
	UDVT_CMD_TEST_WRITEFILE,
	UDVT_CMD_TEST_WRITEFILE_RESPONSE,
	UDVT_CMD_TEST_SAVELOG,
	UDVT_CMD_NUM
} UDVT_CMD_TYPE_e;

#define UDVT_GET_HANDLE_INPUT_QUEUE   1
#define UDVT_GET_HANDLE_OUTPUT_QUEUE  2

#define UDVT_GET_TESTCASE_PACKET_INPUT  1
#define UDVT_GET_TESTCASE_PACKET_OUTPUT 2

#define UDVT_CLEAR_STATUS_BIT         0xFEFF
#define UDVT_SET_STATUS_BIT           0x0100
#define UDVT_STATUS_BIT_MASK          0x0100
#define UDVT_CLEAR_RETRANSMIT_BIT     0xFDFF
#define UDVT_SET_RETRANSMIT_BIT       0x0200
#define UDVT_RETRANSMIT_BIT_MASK      0x0200
#define UDVT_MESSAGE_TYPE_MASK        0x00FF


#define UDVT_DATA_PACKET             0x00
#define UDVT_LARGE_DATA_PACKET       0x01
#define UDVT_STATUS_PACKET           0x02
#define UDVT_REQUEST_PACKET          0x03


#define UDVT_RESEND_REQUEST         0x04


typedef enum {
	UDVT_ACK,
	UDVT_NACK
} UDVT_ACK_e;

typedef enum {
	UDVT_FUN_TIMEOUT = 2,
	UDVT_FUN_OK = 1,
	UDVT_FUN_ERR = 0
} UDVT_FUN_RET_E;


#define UDVT_PACKAGE_SWITCH_RESEND    3

#define UDVT_FUN_TIMEOUT    2
#define UDVT_FUN_OK         1
#define UDVT_FUN_ERR        0

typedef struct _UDVT_TAG_PacketHeader {
	UINT16 PacketHeader;
	UINT16 PacketLength;
	UINT32 lPacketChecksum;
} UDVT_PacketHeader_s;

#define UDVT_PACKET_HEADER_SIZE   8

typedef struct _UDVT_TAG_CompletePacket {
	UDVT_PacketHeader_s sPacketHeader;
	UINT32 lSourceId;
	UINT32 lDestinationId;
	UINT32 lCommand;
	UINT32 lData[UDVT_PACKET_MAX_DATA_LENGTH];
} UDVT_CompletePacket_s;

#define UDVT_PACKET_SIZE  (UDVT_PACKET_HEADER_SIZE + 12 + UDVT_PACKET_MAX_DATA_LENGTH)

#define UDVT_PORT_SIZE  0x4000

typedef struct _UDVT_Configuration {
	UINT32 PacketHeaderSize;
	void (*InitPortFun) (void);
	 UINT32(*GetDataFun) (UINT8 *pDataBuff, UINT32 DataLength, UINT32 TimeOut);
	 UINT32(*SendDataFun) (UINT8 *pDataBuff, UINT32 DataLength, UINT32 TimeOut);
	 UINT32(*GetDataDirect) (UINT8 *pDataBuff, UINT32 DataLength, UINT32 TimeOut);
	 UINT32(*SendDataDirect) (UINT8 *pDataBuff, UINT32 DataLength, UINT32 TimeOut);
	HANDLE_T hInMsgQueue;
	HANDLE_T hOutMsgQueue;
} UDVT_Configuration_s;

typedef struct _UDVT_TestCase_Env {
	UINT32 case_id;
	UINT32 host_id;
	HANDLE_T msg_queue;
	HANDLE_T work_thread;
	HANDLE_T exit_sema;
	HANDLE_T wait_sema;

	struct completion exit_complete;
	UDVT_CompletePacket_s InPacket;
	UDVT_CompletePacket_s OutPacket;
	UINT32 u4TestProgress;
	struct task_struct *pTask;
	int pThread_id;
	UINT32 pcFileHandle;
	UINT32 pcFileLength;
	UINT32 pcFileResponse;
	UINT32 waitResponse;
	UINT32 fileWritable;
} UDVT_TestCase_Env_s;

#define UDVT_OPENFILE_MODE_DATASIZE   12
#define UDVT_OPENFILE_FILENAME_LEN    500

typedef struct _UDVT_OpenFile {
	char fmode[UDVT_OPENFILE_MODE_DATASIZE];
	char fname[UDVT_OPENFILE_FILENAME_LEN];
} UDVT_OpenFile_s;

void UDVT_SendACK(void);


UINT32 UDVT_PacketSwitch(UDVT_CompletePacket_s *pPacket);
UDVT_FUN_RET_E UDVT_PostMessageToTestCase(UINT32 TestCaseIndex, UDVT_CompletePacket_s *pPacket);
UINT32 UDVT_FindWorkTestCaseByDestID(UINT32 caseID);
UINT32 UDVT_AddWorkTestCase(UINT32 caseID, UINT32 hostID);
UDVT_FUN_RET_E UDVT_IsCheckSumOk(UDVT_CompletePacket_s *pPacket);
UINT32 UDVT_FindWorkTestCaseByThreadHandle(HANDLE_T hThread);
void UDVT_TestCaseThread(void *pv_arg);
void UDVT_TaskFun(void *pv_arg);
void UDVT_ResetTestCaseEnv(UINT32 TestCaseIndex);

UDVT_FUN_RET_E UDVT_SendResponse(UINT32 TestCaseIndex, UINT32 lCmd);

extern int UDVT_Init(void);
extern UDVT_CompletePacket_s *UDVT_GetTestCasePacketByThreadHandle(HANDLE_T hThread,
								   UINT32 packetType);
extern void UDVT_Config(UDVT_Configuration_s *pcfg);
extern void *UDVT_GetUDVTHandle(UINT32 type);
extern UDVT_FUN_RET_E UDVT_PostMessageToHost(UDVT_CompletePacket_s *pPacket);
extern UINT32 UDVT_CalculateCheckSum(UDVT_CompletePacket_s *pPacket);
extern UDVT_FUN_RET_E UDVT_ReportTestCaseProgress(HANDLE_T hThread, UINT32 u4Percent);
extern UINT16 UDVT_FrameTheHeader(UINT8 messageType, UDVT_ACK_e statusBit, BOOL retransmitBit);
extern UDVT_FUN_RET_E UDVT_WaitPCResponse(UINT32 TestCaseIndex, UINT32 timeout);
extern UINT32 UDVT_GetTestCaseOpenFileHandle(UINT32 caseID);
extern UINT32 UDVT_GetFileLength(UINT32 caseID);
extern UINT32 UDVT_ReadFileFromPort(UINT8 *pData, UINT32 DataSize);
extern UINT32 UDVT_WriteFiletoPort(UINT8 *pData, UINT32 DataSize);
extern UINT32 UDVT_GetFileResponse(UINT32 caseID);
extern UINT32 UDVT_GetWaitResponse(UINT32 caseID);
extern UINT32 UDVT_IsFileWriteable(UINT32 caseID);

#endif

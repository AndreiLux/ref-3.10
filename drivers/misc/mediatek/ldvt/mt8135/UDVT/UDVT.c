#include <linux/types.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/time.h>

#include "UDVT.h"
#define CONFIG_SYS_MEM_PHASE2 (1)
#define CONFIG_SYS_MEM_PHASE3 (0)
/* extern INT32 CLI_Parser(const CHAR *szCmdBuf); */
#define x_cli_parser(par) printk(par)
#define x_alloc_aligned_dma_mem(size, align) x_mem_alloc(size)
#define x_free_aligned_dma_mem(addr)  x_mem_free(addr)
/*
#if CONFIG_SYS_MEM_PHASE2
#include "x_mem_phase2.h"
#elif CONFIG_SYS_MEM_PHASE3
#include "x_kmem.h"
#endif
*/
extern int UDVT_open(int portn);

extern void vVdecTest(void);


static HANDLE_T hUDVT_Task;

static UDVT_Configuration_s UDVT_Conf;

static UDVT_CompletePacket_s UDVT_CompletePacket;


static UDVT_TestCase_Env_s UDVT_WorkCase[UDVT_MAX_WORK_TESTCASE];

UINT32 UDVT_ActiveTestCase;
/*
void vTestMapUnUnit(void)
{
	printk("Unit Test Start!\n");

	UINT8 *pTestData = NULL;
	UINT8 *pTest_tmp = NULL;

	pTestData = (UINT8 *)x_alloc_aligned_dma_mem(0x8000, 32);

	memset((void *)pTestData, 0xFF, 0x8000);

	pTest_tmp = (UINT8 *)BSP_dma_map_single(pTestData, 0x8000, TO_DEVICE);

	Printf("pTest_tmp = %x!\n", pTest_tmp);

	BSP_dma_unmap_single(PHYSICAL(pTestData), 0x8000, TO_DEVICE);

	x_free_aligned_dma_mem(pTestData);

	Printf("Unit Test End!\n");
}
*/
static void calc_time_diff(struct timeval *tv1, struct timeval *tv2)
{
	tv2->tv_sec -= tv1->tv_sec;
	if (tv2->tv_usec < tv1->tv_usec) {
		tv2->tv_sec--;
		tv2->tv_usec = tv2->tv_usec + 1000000 - tv1->tv_usec;
	} else {
		tv2->tv_usec -= tv1->tv_usec;
	}
}

void UDVT_TaskFun(void *pv_arg)
{
	UDVT_FUN_RET_E res;
	UINT16 validMsg;
	UINT16 msgqIndex;
	UINT8 *tmpReceived;
	UINT8 *tmpSend;
	UINT32 receivedN;
	UINT32 tmpReceivedN;
	SIZE_T msgSize;
	UINT32 receiveErr;
	/* HAL_TIME_T oldTime; */
	/* HAL_TIME_T newTime; */
	/* HAL_TIME_T daltaTime; */
	struct timeval tv1;
	struct timeval tv2;

	UDVT_LOG(0, "[UDVT] TaskFun is running!\n");
	/* HAL_GetTime(&oldTime); */
	receivedN = 0;
	tmpReceivedN = 0;
	receiveErr = 0;

#if CONFIG_SYS_MEM_PHASE2
	tmpReceived = (UINT8 *) x_alloc_aligned_dma_mem(UDVT_PORT_SIZE, 32);
#elif CONFIG_SYS_MEM_PHASE3
	tmpReceived = x_mem_aligned_alloc(UDVT_PORT_SIZE, 32);
#endif

	VERIFY(tmpReceived != NULL);
#if CONFIG_SYS_MEM_PHASE2
	tmpSend = (UINT8 *) x_alloc_aligned_dma_mem(UDVT_PORT_SIZE, 32);
#elif CONFIG_SYS_MEM_PHASE3
	tmpSend = x_mem_aligned_alloc(UDVT_PORT_SIZE, 32);
#endif
	VERIFY(tmpSend != NULL);

	while (1) {
		validMsg = 0;
		x_thread_delay(3);
		/* UDVT_LOG(3,"[UDVT] Receive Data\n"); */
		tmpReceivedN =
		    (*UDVT_Conf.GetDataFun) (tmpReceived + receivedN, UDVT_PORT_SIZE - receivedN,
					     0);
		/* UDVT_LOG(3,"[UDVT] Receive Data End\n"); */
		if (tmpReceivedN > 0) {

			/* HAL_GetTime(&oldTime); */
			do_gettimeofday(&tv1);
			receivedN += tmpReceivedN;
			if (receivedN > UDVT_PORT_SIZE) {
				receivedN = 0;
				receiveErr = 1;
				UDVT_LOG(3, "[UDVT] xxxxxxxxxxxxxxx\n");
			}
		} else {
			/* HAL_GetTime(&newTime); */
			do_gettimeofday(&tv2);
			calc_time_diff(&tv1, &tv2);
			/* HAL_GetDeltaTime(&daltaTime,&oldTime,&newTime); */
			if (receivedN == UDVT_PORT_SIZE && receiveErr == 0) {
				x_msg_q_send(UDVT_Conf.hInMsgQueue, tmpReceived, UDVT_PACKET_SIZE,
					     100);
				receivedN = 0;
			} else if (tv2.tv_usec > 10) {
				/* if(receivedN == UDVT_PORT_SIZE && receiveErr == 0) */
				/* { */
				/* x_msg_q_send(UDVT_Conf.hInMsgQueue,tmpReceived,UDVT_PACKET_SIZE,100); */
				/* receivedN = 0; */
				/* } */
				/* else */
				/* { */
				receivedN = 0;
				receiveErr = 0;
				/* } */
			}
		}

		if (x_msg_q_num_msgs(UDVT_Conf.hInMsgQueue, &validMsg) == OSR_OK) {

			if (validMsg > 0) {
				UDVT_LOG(3, "[UDVT] Get %d input packet\n", validMsg);
				msgSize = UDVT_PACKET_SIZE;
				VERIFY(OSR_OK ==
				       x_msg_q_receive(&msgqIndex, &UDVT_CompletePacket, &msgSize,
						       &(UDVT_Conf.hInMsgQueue), 1,
						       X_MSGQ_OPTION_NOWAIT));
				res = UDVT_IsCheckSumOk(&UDVT_CompletePacket);
				if (res == UDVT_FUN_ERR) {
				}


				res = UDVT_PacketSwitch(&UDVT_CompletePacket);
				if (res == UDVT_FUN_OK) {

				} else if (res == UDVT_PACKAGE_SWITCH_RESEND) {
					(*UDVT_Conf.SendDataFun) (tmpSend, UDVT_PORT_SIZE, 0);
				}
			}
		}

		if (x_msg_q_num_msgs(UDVT_Conf.hOutMsgQueue, &validMsg) == OSR_OK) {
			if (validMsg > 0) {
				UDVT_LOG(3, "[UDVT] Put %d output packet\n", validMsg);
				msgSize = UDVT_PACKET_SIZE;
				VERIFY(OSR_OK ==
				       x_msg_q_receive(&msgqIndex, tmpSend, &msgSize,
						       &(UDVT_Conf.hOutMsgQueue), 1,
						       X_MSGQ_OPTION_NOWAIT));
				UDVT_LOG(3, "[UDVT] Output packet header is 0x%x\n",
					 UDVT_CompletePacket.sPacketHeader.PacketHeader);
				(*UDVT_Conf.SendDataFun) (tmpSend, UDVT_PORT_SIZE, 0);
			}
		}


	}

}


extern INT32 _i4UDVTAccessFileTest(void);
void UDVT_TestCaseThread(void *pv_arg)
{
	UINT32 TestCaseIndex;
	UINT16 validInputMsg;
	UINT16 msgqIndex;
	SIZE_T msgSize;
	UINT32 IsWork;
	UDVT_TestCase_Env_s *pCaseEnv;

	/* TestCaseIndex = *((UINT32 *)pv_arg); */
	TestCaseIndex = ((UINT32) pv_arg);
	UDVT_LOG(3, "[UDVT] TestCase %x is Created!\n", TestCaseIndex);
	pCaseEnv = &(UDVT_WorkCase[TestCaseIndex]);
	/* lock_kernel(); */
	/* current->flags |= PF_NOFREEZE; */
	/* daemonize("UDVT"); */
	allow_signal(SIGKILL);
	allow_signal(SIGTERM);
	allow_signal(SIGSTOP);
	/* pCaseEnv->pTask = get_current(); */
	/* ASSERT(x_thread_self((HANDLE_T *)&(pCaseEnv->work_thread)) == OSR_OK); */
	/* unlock_kernel(); */
	IsWork = 1;
	while (IsWork) {
		validInputMsg = 0;
		x_thread_delay(100);
		if (x_msg_q_num_msgs(pCaseEnv->msg_queue, &validInputMsg) == OSR_OK) {
			if (validInputMsg > 0) {
				UDVT_LOG(3, "[UDVT] Thread of TestCase %d got a message\n",
					 TestCaseIndex);
				msgSize = UDVT_PACKET_SIZE - UDVT_PACKET_HEADER_SIZE;
				if (OSR_OK ==
				    x_msg_q_receive(&msgqIndex,
						    (UINT8 *) (&(pCaseEnv->InPacket)) +
						    UDVT_PACKET_HEADER_SIZE, &msgSize,
						    &(pCaseEnv->msg_queue), 1,
						    X_MSGQ_OPTION_NOWAIT)) {
					UDVT_LOG(3, "[UDVT] TestCase %d carry out cmd %s\n",
						 TestCaseIndex, (char *)(pCaseEnv->InPacket.lData));
					pCaseEnv->OutPacket.lSourceId =
					    pCaseEnv->InPacket.lDestinationId;
					pCaseEnv->OutPacket.lDestinationId =
					    pCaseEnv->InPacket.lSourceId;
					switch (pCaseEnv->InPacket.lCommand) {
					case UDVT_CMD_TEST_START_REQUEST:
						/* _i4UDVTAccessFileTest(); */
						vVdecTest();
						/* x_cli_parser((char *)(pCaseEnv->InPacket.lData)); */
						break;
					case UDVT_CMD_MODULE_DEINIT_REQUEST:


						IsWork = 0;
						break;

					}

				}
			}
		}
	}





	x_sema_unlock(pCaseEnv->exit_sema);
	/* complete_and_exit(&(pCaseEnv->exit_complete),0); */
	UDVT_LOG(3, "[UDVT] TestCase %x exit sema unlock!\n", TestCaseIndex);
	x_thread_exit();
	UDVT_LOG(3, "[UDVT] TestCase %x is terminated!\n", TestCaseIndex);
	/* return 0; */
}

extern int UDVT_open(int portn);
int UDVT_Init(void)
{

	UINT32 tmpN;
	UDVT_Conf.PacketHeaderSize = sizeof(UDVT_PacketHeader_s);

	for (tmpN = 0; tmpN < UDVT_MAX_WORK_TESTCASE; tmpN++) {
		UDVT_WorkCase[tmpN].host_id = 0;
		UDVT_WorkCase[tmpN].case_id = 0;
		UDVT_WorkCase[tmpN].msg_queue = 0;
		UDVT_WorkCase[tmpN].work_thread = 0;
		UDVT_WorkCase[tmpN].u4TestProgress = 0;
	}

	VERIFY(OSR_OK ==
	       x_msg_q_create(&(UDVT_Conf.hInMsgQueue), "InputMsg", UDVT_PACKET_SIZE,
			      UDVT_MAX_UDVT_INPUT_MSG_NUM));
	VERIFY(OSR_OK ==
	       x_msg_q_create(&(UDVT_Conf.hOutMsgQueue), "OutputMsg", UDVT_PACKET_SIZE,
			      UDVT_MAX_UDVT_OUTPUT_MSG_NUM));
	VERIFY(OSR_OK == x_thread_create(&hUDVT_Task, "UDVT_Task", 10240, 100, UDVT_TaskFun, 0, 0));

	UDVT_open(0);
	(*UDVT_Conf.InitPortFun) ();

	return 0;
}

UINT16 UDVT_FrameTheHeader(UINT8 messageType, UDVT_ACK_e statusBit, BOOL retransmitBit)
{
	UINT16 framedHeader = 0;
	framedHeader = messageType & UDVT_MESSAGE_TYPE_MASK;
	if (statusBit == UDVT_NACK) {
		framedHeader = framedHeader & UDVT_CLEAR_STATUS_BIT;
		framedHeader = framedHeader | UDVT_SET_STATUS_BIT;
	} else {
		framedHeader = framedHeader & UDVT_CLEAR_STATUS_BIT;
	}

	if (retransmitBit == TRUE) {
		framedHeader = framedHeader & UDVT_CLEAR_RETRANSMIT_BIT;
		framedHeader = framedHeader | UDVT_SET_RETRANSMIT_BIT;
	} else {
		framedHeader = framedHeader & UDVT_CLEAR_RETRANSMIT_BIT;
	}
	return framedHeader;
}

void UDVT_SendACK(void)
{
	UDVT_CompletePacket_s ackPacket;
	ackPacket.sPacketHeader.PacketHeader =
	    UDVT_FrameTheHeader(UDVT_STATUS_PACKET, UDVT_ACK, FALSE);
	ackPacket.sPacketHeader.PacketLength = 0;
	ackPacket.sPacketHeader.lPacketChecksum = 0;
	ackPacket.sPacketHeader.lPacketChecksum = UDVT_CalculateCheckSum(&ackPacket);
	UDVT_LOG(3, "[UDVT]Send ACK\n");
	UDVT_PostMessageToHost(&ackPacket);
}

void *UDVT_GetUDVTHandle(UINT32 type)
{
	void *pHandle;

	pHandle = 0;
	switch (type) {
	case UDVT_GET_HANDLE_INPUT_QUEUE:
		pHandle = &(UDVT_Conf.hInMsgQueue);
		break;
	case UDVT_GET_HANDLE_OUTPUT_QUEUE:
		pHandle = &(UDVT_Conf.hOutMsgQueue);
		break;

	}

	return pHandle;

}


UINT32 UDVT_CalculateCheckSum(UDVT_CompletePacket_s *pPacket)
{
	UINT32 Checksum = 0;
	UINT32 *pData;
	UINT32 u4Tmp;
	UINT32 DataLength;
	pData = (UINT32 *) pPacket;
	DataLength = (UDVT_PACKET_HEADER_SIZE + pPacket->sPacketHeader.PacketLength + 3) / 4;
	u4Tmp = pPacket->sPacketHeader.lPacketChecksum;
	pPacket->sPacketHeader.lPacketChecksum = 0;
	while (DataLength) {
		Checksum = Checksum ^ (*pData);
		pData++;
		DataLength--;
	}

	pPacket->sPacketHeader.lPacketChecksum = u4Tmp;

	return Checksum;
}

#if 0
UDVT_FUN_RET_E UDVT_PacketSwitch(UDVT_CompletePacket_s *pPacket)
{
	UINT32 retN;

	switch (pPacket->sPacketHeader.PacketHeader & UDVT_MESSAGE_TYPE_MASK) {
	case UDVT_STATUS_PACKET:
		Printf("[UDVT] Get ACK packet\n");
		break;
	case UDVT_DATA_PACKET:
		Printf("[UDVT] Get Data packet\n");
		retN = UDVT_FindWorkTestCaseByDestID(pPacket->lDestinationId);
		if (retN == UDVT_MAX_WORK_TESTCASE) {

			retN = UDVT_AddWorkTestCase(pPacket->lDestinationId);
			if (retN == UDVT_MAX_WORK_TESTCASE) {
				return UDVT_FUN_ERR;
			} else {
				Printf("[UDVT] Create new test case thread\n");
				if (UDVT_PostMessageToTestCase(retN, pPacket) == UDVT_FUN_OK) {
					return UDVT_FUN_OK;
				} else {

				}
			}
		} else {
			if (UDVT_PostMessageToTestCase(retN, pPacket) == UDVT_FUN_OK) {
				return UDVT_FUN_OK;
			} else {

			}
		}
		break;
	}

	return UDVT_FUN_OK;
}

#endif


UDVT_FUN_RET_E UDVT_SendResponse(UINT32 TestCaseIndex, UINT32 lCmd)
{
	if (TestCaseIndex < UDVT_MAX_WORK_TESTCASE) {
		UDVT_WorkCase[TestCaseIndex].OutPacket.lCommand = lCmd;
		UDVT_WorkCase[TestCaseIndex].OutPacket.lDestinationId =
		    UDVT_WorkCase[TestCaseIndex].host_id;
		UDVT_WorkCase[TestCaseIndex].OutPacket.lSourceId =
		    UDVT_WorkCase[TestCaseIndex].case_id;
		UDVT_WorkCase[TestCaseIndex].OutPacket.sPacketHeader.PacketHeader =
		    UDVT_FrameTheHeader(UDVT_DATA_PACKET, UDVT_ACK, FALSE);
		UDVT_WorkCase[TestCaseIndex].OutPacket.sPacketHeader.lPacketChecksum = 0;
		UDVT_WorkCase[TestCaseIndex].OutPacket.sPacketHeader.PacketLength = 12;
		UDVT_WorkCase[TestCaseIndex].OutPacket.sPacketHeader.lPacketChecksum =
		    UDVT_CalculateCheckSum(&(UDVT_WorkCase[TestCaseIndex].OutPacket));
		UDVT_PostMessageToHost(&(UDVT_WorkCase[TestCaseIndex].OutPacket));
	}

	return UDVT_FUN_OK;
}

UINT32 UDVT_PacketSwitch(UDVT_CompletePacket_s *pPacket)
{
	UINT32 retN;
	UDVT_LOG(2, "pPacket->sPacketHeader.PacketHeader %d\n",
		 pPacket->sPacketHeader.PacketHeader);
	switch (pPacket->sPacketHeader.PacketHeader & UDVT_MESSAGE_TYPE_MASK) {

	case UDVT_DATA_PACKET:
		/* UDVT_SendACK(); */
		UDVT_LOG(3, "[UDVT] %d Request\n", pPacket->lCommand);
		retN = UDVT_FindWorkTestCaseByDestID(pPacket->lDestinationId);
		if (retN == UDVT_MAX_WORK_TESTCASE) {
			switch (pPacket->lCommand) {
			case UDVT_CMD_MODULE_INIT_REQUEST:
				UDVT_LOG(3, "[UDVT] Module Init Request\n");
				/* vVdecTest(); */
				retN =
				    UDVT_AddWorkTestCase(pPacket->lDestinationId,
							 pPacket->lSourceId);
				if (retN == UDVT_MAX_WORK_TESTCASE) {
					return UDVT_FUN_ERR;
				} else {
					UDVT_LOG(1, "[UDVT] Create new test case thread\n");
					/* if(UDVT_PostMessageToTestCase(retN,pPacket)==UDVT_FUN_OK) */
					/* { */
					/* return UDVT_FUN_OK; */
					/* } */
					/* else */
					/* { */
					/*  */
					/* } */
					UDVT_ActiveTestCase = retN;
					x_sema_create(&(UDVT_WorkCase[retN].exit_sema),
						      X_SEMA_TYPE_BINARY, X_SEMA_STATE_LOCK);
					x_sema_create(&(UDVT_WorkCase[retN].wait_sema),
						      X_SEMA_TYPE_BINARY, X_SEMA_STATE_LOCK);
					/* init_completion(&(UDVT_WorkCase[retN].exit_complete)); */
				}
				UDVT_SendResponse(retN, UDVT_CMD_MODULE_INIT_RESPONSE);
				break;
			default:

				break;
			}
		} else {

			switch (pPacket->lCommand) {
			case UDVT_CMD_MODULE_INIT_REQUEST:
				UDVT_LOG(1, "[UDVT] Repeat Modules Init Request\n");
				UDVT_SendResponse(retN, UDVT_CMD_MODULE_INIT_RESPONSE);
				break;
			case UDVT_CMD_TEST_INIT_REQUEST:
				UDVT_LOG(3, "[UDVT] Test Init Request\n");
				x_cli_parser("cd \\");
				UDVT_SendResponse(retN, UDVT_CMD_TEST_INIT_RESPONSE);
				break;
			case UDVT_CMD_TEST_START_REQUEST:
				UDVT_LOG(3, "[UDVT] Test Start Request\n");
				if (UDVT_PostMessageToTestCase(retN, pPacket) == UDVT_FUN_OK) {
					return UDVT_FUN_OK;
				} else {

				}

				break;
			case UDVT_CMD_TEST_STOP_REQUEST:
				UDVT_LOG(3, "[UDVT] Test Stop Request\n");

				force_sig(SIGSTOP, UDVT_WorkCase[retN].pTask);
				force_sig(SIGKILL, UDVT_WorkCase[retN].pTask);
				send_sig(SIGTERM, UDVT_WorkCase[retN].pTask, 1);
				/* wait_for_completion(&(UDVT_WorkCase[retN].)); */
				/* x_thread_kill((UDVT_WorkCase[retN].work_thread)); */
				break;
			case UDVT_CMD_TEST_DEINIT_REQUEST:
				UDVT_LOG(3, "[UDVT] Test DeInit Request\n");
				UDVT_SendResponse(retN, UDVT_CMD_TEST_DEINIT_RESPONSE);
				break;
			case UDVT_CMD_MODULE_DEINIT_REQUEST:
				UDVT_LOG(3, "[UDVT] Module DeInit Request\n");
				if (UDVT_PostMessageToTestCase(retN, pPacket) == UDVT_FUN_OK) {
					x_sema_lock(UDVT_WorkCase[retN].exit_sema,
						    X_SEMA_OPTION_WAIT);
					/* wait_for_completion(&(UDVT_WorkCase[retN].exit_complete)); */
					UDVT_SendResponse(retN, UDVT_CMD_MODULE_DEINIT_RESPONSE);
					UDVT_ResetTestCaseEnv(retN);

				} else {

				}


				break;
			case UDVT_CMD_TEST_OPENFILE_RESPONSE:
				UDVT_LOG(3, "[UDVT] Open File Response\n");
				if (UDVT_PostMessageToTestCase(retN, pPacket) == UDVT_FUN_OK) {
					return UDVT_FUN_OK;
				}
				break;
			case UDVT_CMD_TEST_CLOSEFILE_RESPONSE:
				UDVT_LOG(3, "[UDVT] Close file Response\n");
				if (UDVT_PostMessageToTestCase(retN, pPacket) == UDVT_FUN_OK) {
					return UDVT_FUN_OK;
				}
				break;
			case UDVT_CMD_TEST_GET_FILELENGTH_RESPONSE:
				UDVT_LOG(3, "[UDVT] Get file length response\n");
				if (UDVT_PostMessageToTestCase(retN, pPacket) == UDVT_FUN_OK) {
					return UDVT_FUN_OK;
				}
				break;
			case UDVT_CMD_TEST_GET_RESTFILELENGTH_RESPONSE:
				UDVT_LOG(3, "[UDVT] Get rest file length response\n");
				if (UDVT_PostMessageToTestCase(retN, pPacket) == UDVT_FUN_OK) {
					return UDVT_FUN_OK;
				}
				break;
			case UDVT_CMD_TEST_SEEKFILE_RESPONSE:
				UDVT_LOG(3, "[UDVT] seek file response\n");
				if (UDVT_PostMessageToTestCase(retN, pPacket) == UDVT_FUN_OK) {
					return UDVT_FUN_OK;
				}
				break;
			case UDVT_CMD_TEST_WAIT_RESPONSE:
				UDVT_LOG(3, "[UDVT] Wait response\n");
				if (UDVT_PostMessageToTestCase(retN, pPacket) == UDVT_FUN_OK) {
					return UDVT_FUN_OK;
				}
				break;
			case UDVT_CMD_TEST_WRITEFILE_RESPONSE:
				UDVT_LOG(3, "[UDVT] WriteFile response\n");
				if (UDVT_PostMessageToTestCase(retN, pPacket) == UDVT_FUN_OK) {
					return UDVT_FUN_OK;
				}
				break;
			default:
				UDVT_LOG(3, "[UDVT] Get Unknow Cmd\n");
				break;
			}
		}


		break;
	case UDVT_REQUEST_PACKET:
		switch (pPacket->lCommand) {
		case UDVT_RESEND_REQUEST:
			UDVT_LOG(1, "[UDVT] Get Resend Request\n");
			return UDVT_PACKAGE_SWITCH_RESEND;
			break;
		}
		break;

	}

	return UDVT_FUN_OK;
}


/* void UDVT_Log(UINT32 type, */
void UDVT_Config(UDVT_Configuration_s *pcfg)
{
	UDVT_Conf.GetDataFun = pcfg->GetDataFun;
	UDVT_Conf.SendDataFun = pcfg->SendDataFun;
	UDVT_Conf.InitPortFun = pcfg->InitPortFun;
	UDVT_Conf.GetDataDirect = pcfg->GetDataDirect;
	UDVT_Conf.SendDataDirect = pcfg->SendDataDirect;


}

void UDVT_ResetTestCaseEnv(UINT32 TestCaseIndex)
{
	UDVT_LOG(3, "[UDVT]Test Case %d Env Reset\n", TestCaseIndex);
	if (TestCaseIndex < UDVT_MAX_WORK_TESTCASE) {

		UDVT_WorkCase[TestCaseIndex].case_id = 0;
		UDVT_WorkCase[TestCaseIndex].host_id = 0;
		if (UDVT_WorkCase[TestCaseIndex].exit_sema != 0) {
			x_sema_delete(UDVT_WorkCase[TestCaseIndex].exit_sema);
			UDVT_WorkCase[TestCaseIndex].exit_sema = 0;

		}
		if (UDVT_WorkCase[TestCaseIndex].wait_sema != 0) {
			x_sema_delete(UDVT_WorkCase[TestCaseIndex].wait_sema);
			UDVT_WorkCase[TestCaseIndex].wait_sema = 0;
		}
		if (UDVT_WorkCase[TestCaseIndex].msg_queue != 0) {
			x_msg_q_delete(UDVT_WorkCase[TestCaseIndex].msg_queue);
			UDVT_WorkCase[TestCaseIndex].msg_queue = 0;
		}

		UDVT_WorkCase[TestCaseIndex].work_thread = 0;

	}
}

UDVT_FUN_RET_E UDVT_PostMessageToTestCase(UINT32 TestCaseIndex, UDVT_CompletePacket_s *pPacket)
{
	HANDLE_T msg_q_handle = UDVT_WorkCase[TestCaseIndex].msg_queue;


	switch (pPacket->lCommand) {
	case UDVT_CMD_TEST_OPENFILE_RESPONSE:
		UDVT_WorkCase[TestCaseIndex].pcFileHandle = pPacket->lData[0];
		UDVT_WorkCase[TestCaseIndex].pcFileLength = pPacket->lData[1];
		UDVT_LOG(3, "[UDVT] OpenFile Response:0x%x,%d\n",
			 UDVT_WorkCase[TestCaseIndex].pcFileHandle,
			 UDVT_WorkCase[TestCaseIndex].pcFileLength);
		x_sema_unlock(UDVT_WorkCase[TestCaseIndex].wait_sema);
		break;
	case UDVT_CMD_TEST_GET_RESTFILELENGTH_RESPONSE:
		UDVT_WorkCase[TestCaseIndex].pcFileHandle = pPacket->lData[0];
		UDVT_WorkCase[TestCaseIndex].pcFileLength = pPacket->lData[1];
		UDVT_LOG(3, "[UDVT] GetRestFileLength Response:0x%x,%d\n",
			 UDVT_WorkCase[TestCaseIndex].pcFileHandle,
			 UDVT_WorkCase[TestCaseIndex].pcFileLength);
		x_sema_unlock(UDVT_WorkCase[TestCaseIndex].wait_sema);
		break;
	case UDVT_CMD_TEST_GET_FILELENGTH_RESPONSE:
		UDVT_WorkCase[TestCaseIndex].pcFileHandle = pPacket->lData[0];
		UDVT_WorkCase[TestCaseIndex].pcFileLength = pPacket->lData[1];
		UDVT_LOG(3, "[UDVT] GetFileLength Response:0x%x,%d\n",
			 UDVT_WorkCase[TestCaseIndex].pcFileHandle,
			 UDVT_WorkCase[TestCaseIndex].pcFileLength);
		x_sema_unlock(UDVT_WorkCase[TestCaseIndex].wait_sema);
		break;
	case UDVT_CMD_TEST_CLOSEFILE_RESPONSE:
		UDVT_WorkCase[TestCaseIndex].pcFileHandle = 0;
		UDVT_WorkCase[TestCaseIndex].pcFileLength = 0;
		UDVT_LOG(3, "[UDVT] CloseFile Response\n");
		x_sema_unlock(UDVT_WorkCase[TestCaseIndex].wait_sema);
		break;
	case UDVT_CMD_TEST_SEEKFILE_RESPONSE:
		UDVT_WorkCase[TestCaseIndex].pcFileHandle = pPacket->lData[0];
		UDVT_WorkCase[TestCaseIndex].pcFileResponse = pPacket->lData[1];
		UDVT_LOG(3, "[UDVT] SeekFile Response\n");
		x_sema_unlock(UDVT_WorkCase[TestCaseIndex].wait_sema);
		break;
	case UDVT_CMD_TEST_WAIT_RESPONSE:
		UDVT_WorkCase[TestCaseIndex].waitResponse = pPacket->lData[0];
		UDVT_LOG(3, "[UDVT] Wait Response %d\n", UDVT_WorkCase[TestCaseIndex].waitResponse);
		x_sema_unlock(UDVT_WorkCase[TestCaseIndex].wait_sema);
		break;
	case UDVT_CMD_TEST_WRITEFILE_RESPONSE:
		UDVT_WorkCase[TestCaseIndex].pcFileHandle = pPacket->lData[0];
		UDVT_WorkCase[TestCaseIndex].fileWritable = pPacket->lData[1];
		UDVT_LOG(3, "[UDVT] WriteFile writable %d\n",
			 UDVT_WorkCase[TestCaseIndex].fileWritable);
		x_sema_unlock(UDVT_WorkCase[TestCaseIndex].wait_sema);
		break;
	default:
		if (OSR_OK ==
		    x_msg_q_send(msg_q_handle, ((UINT8 *) pPacket) + UDVT_PACKET_HEADER_SIZE,
				 (SIZE_T) (pPacket->sPacketHeader.PacketLength), 200)) {
			UDVT_LOG(3, "[UDVT] Post msg(%d) to test case(%d)\n", pPacket->lCommand,
				 TestCaseIndex);
			return UDVT_FUN_OK;
		}
		break;
	}



	return UDVT_FUN_ERR;
}

UDVT_FUN_RET_E UDVT_WaitPCResponse(UINT32 TestCaseIndex, UINT32 timeout)
{

	x_sema_lock(UDVT_WorkCase[TestCaseIndex].wait_sema, X_SEMA_OPTION_WAIT);
	return UDVT_FUN_OK;
}

UDVT_FUN_RET_E UDVT_PostMessageToHost(UDVT_CompletePacket_s *pPacket)
{

	UINT32 retryN = 50;
	UINT16 validMsg = 0;

	do {
		validMsg = 0;
		if (x_msg_q_num_msgs(UDVT_Conf.hOutMsgQueue, &validMsg) == OSR_OK) {
			if (UDVT_MAX_UDVT_INPUT_MSG_NUM > validMsg + 10) {
				x_msg_q_send(UDVT_Conf.hOutMsgQueue, pPacket,
					     pPacket->sPacketHeader.PacketLength +
					     UDVT_PACKET_HEADER_SIZE, 1);
				break;
			}
		}
		x_thread_delay(10);
		retryN--;
	} while (retryN > 0);

	if (retryN == 0) {
		UDVT_LOG(1, "[UDVT] msg queue to host is full\n");
	}

	return UDVT_FUN_OK;
}

UINT32 UDVT_FindWorkTestCaseByDestID(UINT32 caseID)
{
	UINT32 tmpN;
	for (tmpN = 0; tmpN < UDVT_MAX_WORK_TESTCASE; tmpN++) {
		if (UDVT_WorkCase[tmpN].case_id == caseID) {
			break;
		}

	}
	return tmpN;
}

UDVT_FUN_RET_E UDVT_ReportTestCaseProgress(HANDLE_T hThread, UINT32 u4Percent)
{
	UINT32 u4CaseID;
	u4CaseID = UDVT_FindWorkTestCaseByThreadHandle(hThread);
	if (u4CaseID != UDVT_MAX_WORK_TESTCASE) {
		UDVT_WorkCase[u4CaseID].u4TestProgress += u4Percent;
		if (UDVT_WorkCase[u4CaseID].u4TestProgress > 100) {
			UDVT_WorkCase[u4CaseID].u4TestProgress = 100;
		}
	} else {
		return UDVT_FUN_ERR;
	}

	return UDVT_FUN_OK;
}

UINT32 UDVT_FindWorkTestCaseByThreadHandle(HANDLE_T hThread)
{
	UINT32 tmpN;
	for (tmpN = 0; tmpN < UDVT_MAX_WORK_TESTCASE; tmpN++) {
		if (UDVT_WorkCase[tmpN].work_thread == hThread) {
			break;
		}

	}
	tmpN = UDVT_ActiveTestCase;
	return tmpN;
}

UINT32 UDVT_GetFileResponse(UINT32 caseID)
{
	return UDVT_WorkCase[caseID].pcFileResponse;
}

UINT32 UDVT_GetFileLength(UINT32 caseID)
{
	return UDVT_WorkCase[caseID].pcFileLength;
}

UINT32 UDVT_IsFileWriteable(UINT32 caseID)
{
	return UDVT_WorkCase[caseID].fileWritable;
}

UINT32 UDVT_GetWaitResponse(UINT32 caseID)
{
	return UDVT_WorkCase[caseID].waitResponse;
}

UINT32 UDVT_GetTestCaseOpenFileHandle(UINT32 caseID)
{
	return UDVT_WorkCase[caseID].pcFileHandle;
}

UINT32 UDVT_ReadFileFromPort(UINT8 *pData, UINT32 DataSize)
{

	(*UDVT_Conf.GetDataDirect) (pData, DataSize, 1000000);
	return 0;
}

UINT32 UDVT_WriteFiletoPort(UINT8 *pData, UINT32 DataSize)
{
	(*UDVT_Conf.SendDataDirect) (pData, DataSize, 1000000);
	return 0;
}

UDVT_CompletePacket_s *UDVT_GetTestCasePacketByThreadHandle(HANDLE_T hThread, UINT32 packetType)
{
	UDVT_CompletePacket_s *pPck;
	UINT32 caseID;
	pPck = 0;
	caseID = UDVT_FindWorkTestCaseByThreadHandle(hThread);

	if (caseID != UDVT_MAX_WORK_TESTCASE) {
		switch (packetType) {
		case UDVT_GET_TESTCASE_PACKET_INPUT:
			pPck = &(UDVT_WorkCase[caseID].InPacket);
			break;
		case UDVT_GET_TESTCASE_PACKET_OUTPUT:
			pPck = &(UDVT_WorkCase[caseID].OutPacket);
			break;

		}
	}
	return pPck;
}

UINT32 UDVT_AddWorkTestCase(UINT32 caseID, UINT32 hostID)
{
	UINT32 tmpN;
	char nameStr[100];
	tmpN = UDVT_FindWorkTestCaseByDestID(caseID);

	if (tmpN == UDVT_MAX_WORK_TESTCASE) {
		for (tmpN = 0; tmpN < UDVT_MAX_WORK_TESTCASE; tmpN++) {
			if (UDVT_WorkCase[tmpN].case_id == 0) {
				x_sprintf(nameStr, "TC_MsgQ_%u", (unsigned int)tmpN);
				if (OSR_OK ==
				    x_msg_q_create(&(UDVT_WorkCase[tmpN].msg_queue), nameStr,
						   UDVT_PACKET_SIZE - sizeof(UDVT_PacketHeader_s),
						   UDVT_MAX_TESTCASE_MSG_NUM)) {
					x_sprintf(nameStr, "TC_Thread_%u", (unsigned int)tmpN);
					/* UDVT_WorkCase[tmpN].pTask = kthread_run(UDVT_TestCaseThread,(VOID *)tmpN,nameStr); */
					/* UDVT_WorkCase[tmpN].pThread_id = kernel_thread(UDVT_TestCaseThread,(VOID *)tmpN,CLONE_KERNEL); */
					/* UDVT_WorkCase[tmpN].pTask = find_task_by_vpid(UDVT_WorkCase[tmpN].pThread_id); */
					/* UDVT_WorkCase[tmpN].case_id = caseID; */
					/* break; */
					if (OSR_OK ==
					    x_thread_create(&(UDVT_WorkCase[tmpN].work_thread),
							    nameStr, 1000, 100, UDVT_TestCaseThread,
							    4, (void *)tmpN)) {
						UDVT_WorkCase[tmpN].case_id = caseID;
						UDVT_WorkCase[tmpN].host_id = hostID;
						break;
					}
				}

			}
		}
	}
	return tmpN;
}


UDVT_FUN_RET_E UDVT_IsCheckSumOk(UDVT_CompletePacket_s *pPacket)
{

	if (pPacket->sPacketHeader.lPacketChecksum == UDVT_CalculateCheckSum(pPacket)) {

		return UDVT_FUN_OK;
	}
	UDVT_LOG(1, "[UDVT]Checksum received is %d,Calculate Checksum is %d\n",
		 pPacket->sPacketHeader.lPacketChecksum, UDVT_CalculateCheckSum(pPacket));
	return UDVT_FUN_ERR;
}

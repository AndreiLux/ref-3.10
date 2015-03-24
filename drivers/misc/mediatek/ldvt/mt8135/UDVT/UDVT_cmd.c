
#include "x_os.h"
/* #include <linux/usb/udvt_serial.h> */
#include <linux/init.h>
#include <linux/module.h>
#include "UDVT.h"
#include "UDVT_IF.h"


static HANDLE_T *phSendQueue;
static HANDLE_T *phGetQueue;

UDVT_Configuration_s udvt_if_cfg;

#define x_alloc_aligned_dma_mem(size, align) x_mem_alloc(size)
#define x_free_aligned_dma_mem(addr)  x_mem_free(addr)

extern int UDVT_Buffed_Write(int portn, const unsigned char *buf, int count);
extern int UDVT_Buffed_Read(int portn, unsigned char *buf, int count);
extern int UDVT_Direct_Read(int portn, unsigned char *buf, int count, int *completesem);
extern UINT32 UDVT_Direct_Write(int portn, unsigned char *buf, int count, int *completesem);
extern unsigned long UDVT_IsDirectReadComplete(int portn);
extern unsigned long UDVT_IsDirectWriteComplete(int portn);

/* extern int k_uart_open(void *inode, void *flip); */
/* extern int k_uart_ioctl(void *inode, void *file, unsigned int cmd,unsigned long arg); */
/* extern UINT32 k_uart_write(void *file, const char *Buf, size_t size, void * loff); */
/* extern UINT32 k_uart_read(void *file, const char *Buf, size_t size, void * loff); */


UINT32 GetDataFun(UINT8 * pDataBuff, UINT32 DataLength, UINT32 TimeOut)
{
/* return k_uart_read(0,(char *)pDataBuff,DataLength,0); */
	return UDVT_Buffed_Read(0, (char *)pDataBuff, DataLength);

}

UINT32 SendDataFun(UINT8 *pDataBuff, UINT32 DataLength, UINT32 TimeOut)
{
/* return k_uart_write(0,(char *)pDataBuff,DataLength,0); */
	UINT32 writeN;
	do {
		writeN = UDVT_Buffed_Write(0, (char *)pDataBuff, DataLength);
		if (writeN < DataLength) {
			x_thread_delay(10);
			DataLength = DataLength - writeN;
			writeN = 0;
		}
	} while (writeN != DataLength);
	return 0;
}

UINT32 GetDataDirect(UINT8 *pDataBuff, UINT32 DataLength, UINT32 TimeOut)
{
	UINT32 n;
	int complete;

	complete = 0;
	UDVT_Direct_Read(0, pDataBuff, DataLength, &complete);
	for (n = 0; n < TimeOut; n++) {
		/* if(complete == 1) */
		if (UDVT_IsDirectReadComplete(0) == 1) {

			break;
		} else {
			x_thread_delay(10);
		}
	}

	return 0;
}


UINT32 SendDataDirect(UINT8 *pDataBuff, UINT32 DataLength, UINT32 TimeOut)
{
	UINT32 n;
	int complete;

	complete = 0;
	UDVT_Direct_Write(0, pDataBuff, DataLength, &complete);
	for (n = 0; n < TimeOut; n++) {
		/* if(complete == 1) */
		if (UDVT_IsDirectWriteComplete(0) == 1) {

			break;
		} else {
			x_thread_delay(100);
		}
	}

	return 0;
}


void _vPortInit(void)
{
/* k_uart_open(0,0); */
/* k_uart_ioctl(0,0,UART_CMD_SET_PORT, UART_PORT_2); */
/* k_uart_ioctl(0,0,UART_CMD_SET_MODE, 1); */
/* k_uart_ioctl(0,0,UART_CMD_SET_BAUD, RS_232_SPEED_115200); */
	/* UDVT_open(0); */



}

INT32 _i4UDVTInit(void)
{
	udvt_if_cfg.GetDataFun = GetDataFun;
	udvt_if_cfg.SendDataFun = SendDataFun;
	udvt_if_cfg.InitPortFun = _vPortInit;
	udvt_if_cfg.GetDataDirect = GetDataDirect;
	udvt_if_cfg.SendDataDirect = SendDataDirect;
	UDVT_Config(&udvt_if_cfg);

	UDVT_Init();

	phGetQueue = (HANDLE_T *) (UDVT_GetUDVTHandle(UDVT_GET_HANDLE_INPUT_QUEUE));
	phSendQueue = (HANDLE_T *) (UDVT_GetUDVTHandle(UDVT_GET_HANDLE_OUTPUT_QUEUE));
	return 0;
}

/*
INT32 _i4UDVTSendVirtualPacket(INT32 i4Argc, const CHAR ** szArgv)
{
  UINT32 icmd;
  UINT32 isrc;
  UINT32 idst;
  UINT32 tmpN;
  UINT8 *tmpD;

  UDVT_CompletePacket_s pck;

  if(i4Argc < 5)
  {
    printk("[UDVT]Send Virtual Packet [command] [ISource] [IDestination] [Data]\n");
    return -1;
  }

  icmd = StrToInt(szArgv[1]);
  isrc = StrToInt(szArgv[2]);
  idst = StrToInt(szArgv[3]);

  pck.lCommand = icmd;
  pck.lDestinationId = idst;
  pck.lSourceId = isrc;

  tmpN = 0;
  tmpD = (UINT8 *)(pck.lData);
  while(szArgv[4][tmpN])
  {
    tmpD[tmpN] = szArgv[4][tmpN];
    tmpN++;
  }
  tmpD[tmpN] = 0;

  pck.sPacketHeader.PacketHeader = 0;
  pck.sPacketHeader.PacketLength = tmpN+12;
  pck.sPacketHeader.lPacketChecksum = 0;

  x_msg_q_send(*phGetQueue,&pck,tmpN+20,100);
  return 0;
}
*/

INT32 _i4UDVTTargetSimulator(INT32 i4Argc, const CHAR **szArgv)
{
	UINT32 step;
	char tmpStr[] = "[UDVT]Simulator is running....\n";
	step = 0;
	while (step < 100) {
		x_thread_delay(1000);
		step += 10;
		UDVT_IF_TestProgress(step);
		printk(tmpStr);

	}
	printk("[UDVT]Simulator run end!\n");
	UDVT_IF_SendResult(UDVT_TEST_PASS);
	return 0;
}

INT32 _i4UDVTWriteFileTest(INT32 i4Argc, const CHAR **szArgv)
{
	UINT8 *pData;
	UINT8 *wstr;
	UINT32 fHandle;
	UINT32 WriteLength;


	UINT32 n;
	UINT32 checksum = 0;

	pData = x_alloc_aligned_dma_mem(0x100000, 4);
	if (i4Argc < 3) {
		printk("[UDVT]fa [filename] [mode] [wstring]\n");

		return -1;
	}

	fHandle = UDVT_IF_OpenFile(szArgv[1], szArgv[2]);
	if (fHandle == 0) {
		printk("[UDVT] Open pc file %s fail!\n", szArgv[1]);
	} else {
		printk("[UDVT] Open pc file %s success!\n", szArgv[1]);
	}

	wstr = (UINT8 *) (szArgv[3]);
	n = 0;
	while (*wstr) {
		pData[n] = *wstr;
		n++;
		wstr++;
	}

	pData[n] = 0;
	WriteLength = n;
	UDVT_IF_WriteFile(pData, 1, WriteLength, fHandle);
	UDVT_IF_CloseFile(fHandle);

	for (n = 0; n < WriteLength; n++) {

		checksum += pData[n];
	}
	FATS_IF_SaveLog(0, "\n[UDVT] Write %d btyes,checksum is %d\n", WriteLength, checksum);
	printk("\n[UDVT] Write %d btyes,checksum is %d\n", WriteLength, checksum);
	x_free_aligned_dma_mem(pData);
	UDVT_IF_SendResult(UDVT_TEST_PASS);
	return 0;
}

/*
INT32 _i4UDVTSaveLogTest(INT32 i4Argc, const CHAR ** szArgv)
{

  UINT32 lognum;
	UINT32 logn;


  if(i4Argc < 2)
  {
    printk("[UDVT]sl [log num]\n");

    return -1;
  }
  lognum = StrToInt(szArgv[1]);

  for(logn=0;logn<lognum;logn++)
  {
	FATS_IF_SaveLog(0,"[UDVT] log save test %d",logn);
  }
  return 0;
}
*/
INT32 _i4UDVTAccessFileTest(void)
{
	UINT8 *pData;
	UINT32 fHandle;
	UINT32 ReadLength;
	UINT32 fileLength;

	UINT32 n;
	UINT32 checksum = 0;

	pData = x_alloc_aligned_dma_mem(0x100000, 4);

	printk("Test Start\n");
	fHandle = UDVT_IF_OpenFile("D://1.txt", "rb");
	if (fHandle == 0) {
		printk("[UDVT] Open pc filefail!\n");
	} else {
		printk("[UDVT] Open pc file success!\n");
	}

	fileLength = UDVT_IF_GetFileLength(fHandle);
	printk("file length is %d\n", fileLength);
	while (fileLength > 0) {
		if (fileLength > 0x100000) {

			ReadLength = UDVT_IF_ReadFile(pData, 1, 0x100000, fHandle);
			if (ReadLength != 0x100000) {
				printk("error occur at %d\n", fileLength);
				break;
			} else {
				fileLength -= ReadLength;
			}
		} else {
			ReadLength = UDVT_IF_ReadFile(pData, 1, fileLength, fHandle);
			if (ReadLength != fileLength) {
				printk("error occur at %d\n", fileLength);
				break;
			} else {
				fileLength -= ReadLength;
			}
		}

		for (n = 0; n < ReadLength; n++) {

			checksum += pData[n];
		}
	}

	UDVT_IF_CloseFile(fHandle);
	printk("\n[UDVT] Read %d btyes,checksum is %d\n", fileLength, checksum);
	x_free_aligned_dma_mem(pData);
	UDVT_IF_SendResult(UDVT_TEST_PASS);
	return 0;
}

static int __init uvvp_udvt_init(void)
{
	printk("\r\n******** uvvp_udvt_init  ********\r\n");
	_i4UDVTInit();
	return 0;
}
late_initcall(uvvp_udvt_init);

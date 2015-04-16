
#ifndef __HIFI_LPP_H__
#define __HIFI_LPP_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define PAGE_MAX_SIZE		 0x1000

#ifndef OK
#define OK			 0
#endif
#define ERROR		(-1)
#define BUSY		(-2)
#define NOMEM		(-3)
#define INVAILD 	(-4)

/* IOCTL入参和出参的SIZE限制 */
#define SIZE_LIMIT_PARAM		(256)

/* HIFI专用区 */
/* |0x37700000~~~~~~~~~~ |0x37a00000~~~~~~~~~~~~|0x37d80000~~~~~~~~~|0x37d82000~~~~~~~~~|0x37da0000~~~~~~~~~~|0x37de0000~~~~~~~~|0x37df5000~~~~~~~~~~|0x37e00000~~~~~~~~~~~~| */
/* |~~~~~Code run 3M~~~~~|~~~Music data 3.5M~~~~|~~~~~~socp 8K~~~~~~|~~~~reserv1 120K~~~|~~~~~OCRAM 256K~~~~~|~~~~TCM 84K~~~~~~~|~~~~~reserv2 44K~~~~|~~~~Code backup 2M~~~~| */
/* |~~~~~~~~no sec~~~~~~~|~~~~~~~~no sec~~~~~~~~|~~~~~~~no sec~~~~~~|~~~~~~~no sec~~~~~~|~~~~~~~nosec~~~~~~~~|~~~~~~nosec~~~~~~~|~~~~~~~~nosec~~~~~~~|~~~~~~~~~sec~~~~~~~~~~| */
/* |0x37700000~~~~~~~~~~ |0x37a00000~~~~~~~~~~~~|0x37d80000~~~~~~~~~|0x37d82000~~~~~~~~~|0x37da0000~~~~~~~~~~|0x37de0000~~~~~~~~|0x37df5000~~~~~~~~~~|0x37e00000~~~~~~~~~~~~| */
#define HIFI_RUN_SIZE					(0x300000)
#define HIFI_MUSIC_DATA_SIZE			(0x380000)
#define HIFI_SOCP_SIZE					(0x2000)
#define HIFI_RESERVE1_SIZE				(0x1e000)
#define HIFI_IMAGE_OCRAMBAK_SIZE		(0x40000)
#define HIFI_IMAGE_TCMBAK_SIZE			(0x15000)
#define HIFI_RESERVE2_SIZE				(0xb000)
#define HIFI_IMAGE_SIZE 				(0x200000)
#define HIFI_SIZE						(0x900000)

#define HIFI_BASE_ADDR					(0x37700000)
#define HIFI_RUN_LOCATION				(HIFI_BASE_ADDR)											/*Code run*/
#define HIFI_MUSIC_DATA_LOCATION		(HIFI_RUN_LOCATION + HIFI_RUN_SIZE) 						/*Music Data*/
#define HIFI_SOCP_LOCATION				(HIFI_MUSIC_DATA_LOCATION + HIFI_MUSIC_DATA_SIZE)			/*SOCP*/
#define HIFI_RESERVE1_LOCATION			(HIFI_SOCP_LOCATION + HIFI_SOCP_SIZE)						/*reserv1*/
#define HIFI_IMAGE_OCRAMBAK_LOCATION	(HIFI_RESERVE1_LOCATION + HIFI_RESERVE1_SIZE)				/*OCRAM*/
#define HIFI_IMAGE_TCMBAK_LOCATION		(HIFI_IMAGE_OCRAMBAK_LOCATION + HIFI_IMAGE_OCRAMBAK_SIZE)	/*TCM*/
#define HIFI_RESERVE2_LOCATION			(HIFI_IMAGE_TCMBAK_LOCATION + HIFI_IMAGE_TCMBAK_SIZE)		/*reserv2*/
#define HIFI_IMAGE_LOCATION 			(HIFI_RESERVE2_LOCATION + HIFI_RESERVE2_SIZE)				/*Code backup*/

#define HIFI_OFFSET_MUSIC_DATA			(HIFI_RUN_SIZE)
#define HIFI_OFFSET_IMG 				(HIFI_SIZE - HIFI_IMAGE_SIZE)
#define HIFI_OFFSET_RUN 				(0x0)

#if 1 /*hifi define*/
#define DRV_HIFI_DDR_BASE_ADDR			(HIFI_BASE_ADDR)
#define DRV_HIFI_SOCP_DDR_BASE_ADDR 	(HIFI_SOCP_LOCATION)
#define DRV_DSP_PANIC_MARK				(HIFI_RESERVE1_LOCATION)
#define DRV_DSP_UART_LOG_LEVEL			(HIFI_RESERVE1_LOCATION + 4)
#define DRV_DSP_UART_TO_MEM_CUR_ADDR	(DRV_DSP_UART_LOG_LEVEL + 4)
#define DRV_DSP_EXCEPTION_NO			(DRV_DSP_UART_TO_MEM_CUR_ADDR + 4)
#define DRV_DSP_IDLE_COUNT_ADDR			(DRV_DSP_EXCEPTION_NO + 4)
#define DRV_DSP_WRITE_MEM_PRINT_ADDR	(DRV_DSP_IDLE_COUNT_ADDR + 4)
#define DRV_DSP_KILLME_ADDR             (DRV_DSP_WRITE_MEM_PRINT_ADDR + 4) /*only for debug, 0x37d82018*/
#define DRV_DSP_KILLME_VALUE            (0xA5A55A5A)
#endif

#define DRV_DSP_UART_TO_MEM 			(0x3FE80000)
#define DRV_DSP_UART_TO_MEM_SIZE		(512*1024)
#define DRV_DSP_UART_TO_MEM_RESERVE_SIZE (10*1024)

#define HIFI_SIZE_MUSIC_DATA			(HIFI_MUSIC_DATA_SIZE)
#define SIZE_PARAM_PRIV 				(100 * 1024)

#define SYS_TIME_STAMP_REG				(0xFFF0A534)

/* 接收HIFI消息，前部cmd_id占用的字节数 */
#define SIZE_CMD_ID 	   (8)

/* notice主动上报一次缓冲 */
#define REV_MSG_NOTICE_ID_MAX		2

#define ACPU_TO_HIFI_ASYNC_CMD	  0xFFFFFFFF

#define BUFFER_NUM	8

/*****************************************************************************
  3 枚举定义
*****************************************************************************/
typedef enum {
	HIFI_CLOSE,
	HIFI_OPENED,
}HIFI_STATUS;

typedef enum {
	DUMP_DSP_LOG,
	DUMP_DSP_BIN
}DUMP_DSP_TYPE;

typedef enum {
	DSP_NORMAL,
	DSP_PANIC
}DSP_ERROR_TYPE;

typedef enum {
	NORMAL_LOG = 0,
	NORMAL_BIN,
	PANIC_LOG,
	PANIC_BIN
}DUMP_DSP_INDEX;

typedef enum HIFI_MSG_ID_ {
	ID_AP_AUDIO_PLAY_START_REQ			= 0xDD51,/* AP启动Hifi audio player request命令 */
	ID_AUDIO_AP_PLAY_START_CNF			= 0xDD52,/* Hifi启动audio player后回复AP confirm命令 */
	ID_AP_AUDIO_PLAY_PAUSE_REQ			= 0xDD53,/* AP停止Hifi audio player request命令 */
	ID_AUDIO_AP_PLAY_PAUSE_CNF			= 0xDD54,/* Hifi停止audio player后回复AP confirm命令 */
	ID_AUDIO_AP_PLAY_DONE_IND			= 0xDD56,/* Hifi通知AP audio player一块数据播放完毕或者播放中断indication */
	ID_AP_AUDIO_PLAY_UPDATE_BUF_CMD 	= 0xDD57,/* AP通知Hifi新数据块更新command */
	ID_AP_AUDIO_PLAY_QUERY_TIME_REQ 	= 0xDD59,/* AP查询Hifi audio player播放进度request命令 */
	ID_AP_AUDIO_PLAY_WAKEUPTHREAD_REQ	= 0xDD5A,
	ID_AUDIO_AP_PLAY_QUERY_TIME_CNF 	= 0xDD60,/* Hifi回复AP audio player播放进度confirm命令 */
	ID_AP_AUDIO_PLAY_QUERY_STATUS_REQ	= 0xDD61,/* AP查询Hifi audio player播放状态request命令 */
	ID_AUDIO_AP_PLAY_QUERY_STATUS_CNF	= 0xDD62,/* Hifi回复AP audio player播放状态confirm命令 */
	ID_AP_AUDIO_PLAY_SEEK_REQ			= 0xDD63,/* AP seek Hifi audio player到某一位置request命令 */
	ID_AUDIO_AP_PLAY_SEEK_CNF			= 0xDD64,/* Hifi回复AP seek结果confirm命令 */
	ID_AP_AUDIO_PLAY_SET_VOL_CMD		= 0xDD70,/* AP设置音量命令 */

	/* enhance msgid between ap and hifi */
	ID_AP_HIFI_ENHANCE_START_REQ		= 0xDD81,
	ID_HIFI_AP_ENHANCE_START_CNF		= 0xDD82,
	ID_AP_HIFI_ENHANCE_STOP_REQ 		= 0xDD83,
	ID_HIFI_AP_ENHANCE_STOP_CNF 		= 0xDD84,
	ID_AP_HIFI_ENHANCE_SET_DEVICE_REQ	= 0xDD85,
	ID_HIFI_AP_ENHANCE_SET_DEVICE_CNF	= 0xDD86,

	/* audio enhance msgid between ap and hifi */
	ID_AP_AUDIO_ENHANCE_SET_DEVICE_IND	= 0xDD91,
	ID_AP_AUDIO_MLIB_SET_PARA_IND		= 0xDD92,

	/*Voice Record*/
	ID_AP_HIFI_VOICE_RECORD_START_CMD	= 0xDD40,
	ID_AP_HIFI_VOICE_RECORD_STOP_CMD	= 0xDD41,

} HIFI_MSG_ID;

/*处理hifi回复消息，记录cmd_id和数据*/
typedef struct {
	unsigned char *mail_buff;		/* 邮箱数据接收的buff */
	unsigned int mail_buff_len;
	unsigned int cmd_id;			/* 邮箱接收数据前4个字节是cmd_id */
	unsigned char *out_buff_ptr;	/* 指向mail_buff cmd_id后的位置 */
	unsigned int out_buff_len;
} rev_msg_buff;

struct recv_request {
	struct list_head recv_node;
	rev_msg_buff rev_msg;
};

struct misc_recmsg_param {
	unsigned short	msgID;
	unsigned short	playStatus;
};

struct temp_hifi_cmd{
	unsigned short	msgID;
	unsigned short	value;
};

struct hifi_dsp_dump_info{
	DSP_ERROR_TYPE error_type;
	DUMP_DSP_TYPE dump_type;
	char* file_name;
	char* data_addr;
	unsigned int data_len;
};

#ifndef LOG_TAG
#define LOG_TAG "hifi_misc "
#endif

extern struct hifi_misc_priv g_misc_data;

#define logd(fmt, ...) \
do {\
	if (g_misc_data.debug_level >= 3) {\
		printk(LOG_TAG"[D][%u]:%s:%d: "fmt, (unsigned int)readl(g_misc_data.dsp_time_stamp), __FUNCTION__, __LINE__, ##__VA_ARGS__);\
	}\
} while(0);

#define logi(fmt, ...) \
do {\
	if (g_misc_data.debug_level >= 2) {\
		printk(LOG_TAG"[I][%u]:%s:%d: "fmt, (unsigned int)readl(g_misc_data.dsp_time_stamp), __FUNCTION__, __LINE__, ##__VA_ARGS__);\
	}\
} while(0);


#define logw(fmt, ...) \
do {\
	if (g_misc_data.debug_level >= 1) {\
		printk(LOG_TAG"[W][%u]:%s:%d: "fmt, (unsigned int)readl(g_misc_data.dsp_time_stamp), __FUNCTION__, __LINE__, ##__VA_ARGS__);\
	}\
} while(0);

#define loge(fmt, ...) \
do {\
	if (g_misc_data.debug_level >= 0) {\
		printk(LOG_TAG"[E][%u]:%s:%d: "fmt, (unsigned int)readl(g_misc_data.dsp_time_stamp), __FUNCTION__, __LINE__, ##__VA_ARGS__);\
	}\
} while(0);

#define IN_FUNCTION   logd("begin.\n");
#define OUT_FUNCTION  logd("end.\n");

extern void hifi_dump_panic_log(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* end of hifi_lpp.h */


#ifndef __HIFI_LOADER_H__
#define __HIFI_LOADER_H__

/* |0x37700000~~~~~~~~~~ |0x37a00000~~~~~~~~~~~~|0x37d80000~~~~~~~~~|0x37d82000~~~~~~~~~|0x37da0000~~~~~~~~~~|0x37de0000~~~~~~~~|0x37df5000~~~~~~~~~~|0x37e00000~~~~~~~~~~~~| */
/* |~~~~~Code run 3M~~~~~|~~~Music data 3.5M~~~~|~~~~~~socp 8K~~~~~~|~~~~reserv1 120K~~~|~~~~~OCRAM 256K~~~~~|~~~~TCM 84K~~~~~~~|~~~SEC HEAD 1K~~~~~~|~~~WTD FLAG 16byte~~|~~~~~reserv2 43K~~~~|~~~~Code backup 2M~~~~| */
/* |~~~~~~~~no sec~~~~~~~|~~~~~~~~no sec~~~~~~~~|~~~~~~~no sec~~~~~~|~~~~~~~no sec~~~~~~|~~~~~~~nosec~~~~~~~~|~~~~~~nosec~~~~~~~|~~~~~~~~nosec~~~~~~~|~~~~~~~~nosec~~~~~~~|~~~~~~~no sec~~~~~~~|~~~~~~~~~sec~~~~~~~~~~| */
/* |0x37700000~~~~~~~~~~ |0x37a00000~~~~~~~~~~~~|0x37d80000~~~~~~~~~|0x37d82000~~~~~~~~~|0x37da0000~~~~~~~~~~|0x37de0000~~~~~~~~|0x37df5000~~~~~~~~~~|0x37df5400~~~~~~~~~~|0x37df5410~~~~~~~~~~|0x37e00000~~~~~~~~~~~~| */
#define HIFI_RUN_SIZE					(0x300000)
#define HIFI_MUSIC_DATA_SIZE			(0x380000)
#define HIFI_SOCP_SIZE					(0x2000)
#define HIFI_RESERVE1_SIZE				(0x1e000)
#define HIFI_IMAGE_OCRAMBAK_SIZE		(0x40000)
#define HIFI_IMAGE_TCMBAK_SIZE			(0x15000)
#define HIFI_RESERVE2_SIZE				(0xABF0)
#define HIFI_IMAGE_SIZE 				(0x200000)
#define HIFI_SIZE						(0x900000)
#define HIFI_SEC_HEAD_SIZE				(0x400)
#define HIFI_WTD_FLAG_SIZE				(0x10)

#define HIFI_BASE_ADDR					(0x37700000)
#define HIFI_RUN_LOCATION				(HIFI_BASE_ADDR)											/*Code run*/
#define HIFI_MUSIC_DATA_LOCATION		(HIFI_RUN_LOCATION + HIFI_RUN_SIZE) 						/*Music Data*/
#define HIFI_SOCP_LOCATION				(HIFI_MUSIC_DATA_LOCATION + HIFI_MUSIC_DATA_SIZE)			/*SOCP*/
#define HIFI_RESERVE1_LOCATION			(HIFI_SOCP_LOCATION + HIFI_SOCP_SIZE)						/*reserv1*/
#define HIFI_IMAGE_OCRAMBAK_LOCATION	(HIFI_RESERVE1_LOCATION + HIFI_RESERVE1_SIZE)				/*OCRAM*/
#define HIFI_IMAGE_TCMBAK_LOCATION		(HIFI_IMAGE_OCRAMBAK_LOCATION + HIFI_IMAGE_OCRAMBAK_SIZE)	/*TCM*/
#define HIFI_SEC_HEAD_BACKUP            (HIFI_IMAGE_TCMBAK_LOCATION + HIFI_IMAGE_TCMBAK_SIZE)		/*SEC HEAD backup*/
#define HIFI_WTD_FLAG_BASE				(HIFI_SEC_HEAD_BACKUP + HIFI_SEC_HEAD_SIZE)					/*Watchdog flag*/
#define HIFI_RESERVE2_LOCATION			(HIFI_WTD_FLAG_BASE + HIFI_WTD_FLAG_SIZE)					/*reserv2*/
#define HIFI_IMAGE_LOCATION 			(HIFI_RESERVE2_LOCATION + HIFI_RESERVE2_SIZE)				/*Code backup*/

#if 1 /*hifi define*/
#define DRV_HIFI_DDR_BASE_ADDR			(HIFI_BASE_ADDR)
#define DRV_HIFI_SOCP_DDR_BASE_ADDR 	(HIFI_SOCP_LOCATION)
#define DRV_DSP_PANIC_MARK				(HIFI_RESERVE1_LOCATION)
#define DRV_DSP_UART_LOG_LEVEL			(HIFI_RESERVE1_LOCATION + 4)
#define DRV_DSP_UART_TO_MEM_CUR_ADDR	(DRV_DSP_UART_LOG_LEVEL + 4)
#define DRV_DSP_EXCEPTION_NO			(DRV_DSP_UART_TO_MEM_CUR_ADDR + 4)
#define DRV_DSP_IDLE_COUNT_ADDR			(DRV_DSP_EXCEPTION_NO + 4)
#define DRV_DSP_WRITE_MEM_PRINT_ADDR	(DRV_DSP_IDLE_COUNT_ADDR + 4) /*only for debug, 0x37d82014*/
#define DRV_DSP_KILLME_ADDR             (DRV_DSP_WRITE_MEM_PRINT_ADDR + 4) /*only for debug, 0x37d82018*/
#define DRV_DSP_KILLME_VALUE            (0xA5A55A5A)
#endif

#define DRV_DSP_UART_TO_MEM 			(0x3FE80000)
#define DRV_DSP_UART_TO_MEM_SIZE		(512*1024)
#define DRV_DSP_UART_TO_MEM_RESERVE_SIZE (10*1024)

#define MEM_BEGIN_CHECK32_DATA			(0x55AA55AA)
#define MEM_END_CHECK32_DATA			(0xAA55AA55)
#define HIFI_TIME_STAMP_LEN 			(24)
#define HIFI_SEC_MAX_NUM				(32)
#define HIFI_DYNAMIC_SEC_MAX_NUM		(HIFI_SEC_MAX_NUM)

#define HIFI_DDR_PHY_BEGIN_ADDR			(0xC0000000)
#define HIFI_DDR_PHY_END_ADDR			(HIFI_DDR_PHY_BEGIN_ADDR + HIFI_RUN_SIZE - 1)
#define HIFI_DDR_RUN_BASE_ADDR			(HIFI_RUN_LOCATION)
#define HIFI_DDR_SIZE					(HIFI_RUN_SIZE)

#define HIFI_TCM_PHY_BEGIN_ADDR			(0xE8058000)
#define HIFI_TCM_PHY_END_ADDR			(0xE806CFFF)
#define HIFI_TCM_SIZE					(0x15000)
#define HIFI_TCMBAK_OFFSET				(0x80000)

#define HIFI_OCRAM_PHY_BEGIN_ADDR		(0xE8030000)
#define HIFI_OCRAM_PHY_END_ADDR			(0xE803FFFF)
#define HIFI_OCRAM_SIZE					(0x10000)
#define HIFI_OCRAMBAK_OFFSET			(0x95000)

#define HIFI_SHARE_ADDR_LENGTH			(4*1024)

enum DRV_HIFI_IMAGE_SEC_TYPE_ENUM {
	DRV_HIFI_IMAGE_SEC_TYPE_CODE = 0,		 /* section type code */
	DRV_HIFI_IMAGE_SEC_TYPE_DATA,			 /* section type data */
	DRV_HIFI_IMAGE_SEC_TYPE_BUTT,
};

enum DRV_HIFI_IMAGE_SEC_LOAD_ENUM {
	DRV_HIFI_IMAGE_SEC_LOAD_STATIC = 0,
	DRV_HIFI_IMAGE_SEC_LOAD_DYNAMIC,
	DRV_HIFI_IMAGE_SEC_UNLOAD,
	DRV_HIFI_IMAGE_SEC_LOAD_BUTT,
};

struct drv_hifi_image_sec {
	unsigned short	sn; 			 /* serial num		 8*2 bits */
	unsigned char	type;			 /* type			 8	 bits */
	unsigned char	load_attib; 	 /* load attribution 8	 bits */
	unsigned int	src_offset; 	 /* offset			 8*4 bits */
	unsigned int	des_addr;		 /* addr			 8*4 bits */
	unsigned int	size;			 /* section length	 8*4 bits */

};

struct drv_hifi_image_head {
	char							time_stamp[HIFI_TIME_STAMP_LEN]; /* image time stamp 24*8 bits*/
	unsigned int					image_size; 	/* image len 32 bits */
	unsigned int					sections_num;	/* section count 32 bits*/
	struct drv_hifi_image_sec		sections[HIFI_SEC_MAX_NUM];    /* section info sum is sections_num*/
};
/*****************************************************************************
 实 体 名  : drv_hifi_sec_addr_info
 功能描述  : Hifi动态加载段地址结构
*****************************************************************************/
struct drv_hifi_sec_addr {
	unsigned int  sec_source_addr;	/*段的源地址*/
	unsigned int  sec_length;		/*段的长度*/
	unsigned int  sec_dest_addr;	/*段的目的地址*/
};
#define HIFI_SEC_DATA_LENGTH (HIFI_SHARE_ADDR_LENGTH \
							   - HIFI_DYNAMIC_SEC_MAX_NUM*sizeof(struct drv_hifi_sec_addr) \
							   - 3*sizeof(unsigned int))



struct drv_hifi_sec_load_info {
	unsigned int			 sec_magic; 		/*段信息开始的保护字*/
	unsigned int			 sec_num;			/*段的个数*/
	struct drv_hifi_sec_addr sec_addr_info[HIFI_DYNAMIC_SEC_MAX_NUM]; /*段的地址信息*/
	char					 sec_data[HIFI_SEC_DATA_LENGTH];		  /*段信息*/
};

extern int drv_hifi_load_sec(u32 hifi_base_virt);
extern int drv_hifi_load_dynamic_sec(u32 hifi_base_virt);
#endif /* end of hifi_loader.h */


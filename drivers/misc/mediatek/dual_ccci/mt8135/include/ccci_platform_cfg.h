#ifndef __CCCI_PLATFORM_CFG_H__
#define __CCCI_PLATFORM_CFG_H__

/* Common configuration */
#define MD_EX_LOG_SIZE					(512)


/* MD SYS1 configuration */
#define CCCI1_PCM_SMEM_SIZE				(16 * 2 * 1024)	/* PCM */
#define CCCI1_MD_LOG_SIZE				(137*1024*4+64*1024+112*1024)	/* MD Log */

#define RPC1_MAX_BUF_SIZE				2048	/* (6*1024) */
#define RPC1_REQ_BUFFER_NUM				2	/* support 2 concurrently request */

#define CCCI1_TTY_BUFFER_SIZE			(16 * 1024)
#define CCCI1_CCMNI_BUFFER_SIZE			(3*1024)
#define CCCI1_TTY_PORT_COUNT			(3)
#define CCCI1_CCMNI_PORT_COUNT			(3)	/* For V1 CCMNI */

/* MD SYS2 configuration */
#define CCCI2_PCM_SMEM_SIZE				(16 * 1024)	/* PCM */
#define CCCI2_MD_LOG_SIZE				(137*1024*4+64*1024+112*1024)	/* MD Log */

#define RPC2_MAX_BUF_SIZE				2048	/* (6*1024) */
#define RPC2_REQ_BUFFER_NUM				2	/* support 2 concurrently request */

#define CCCI2_TTY_BUFFER_SIZE			(16 * 1024)
#define CCCI2_CCMNI_BUFFER_SIZE			(3*1024)
#define CCCI2_TTY_PORT_COUNT			(3)
#define CCCI2_CCMNI_PORT_COUNT			(3)	/* For V1 CCMNI */


#define CCCI_SHARED_MEM_SIZE			UL(0x200000)	/* 2M align for share memory */

#define MD_IMG_DUMP_SIZE				(1<<8)
#define DSP_IMG_DUMP_SIZE				(1<<9)

/* Feature option */
/* #define CCCI_STATIC_SHARED_MEM */

#endif

/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <asm/atomic.h>
#include "dsp_drv.h"
#include <linux/odin_mailbox.h>
#include <linux/odin_pd.h>
#include <linux/odin_lpa.h>
#include "mailbox/mailbox_hal.h"
#include "dsp_lpa_drv.h"
#include "dsp_ioctl_cmd.h"

#include <linux/delay.h>

#if defined(CONFIG_WODEN_DSP)
#include "woden/dsp_coredrv_woden.h"
#elif defined(CONFIG_ODIN_DSP)
#include "odin/dsp_coredrv_odin.h"
#endif

#define	CHECK_DSP_DEBUG	1
#define NEARSTOP_THRESHOLD 2

#if PLAY_PP

#define NUMBER_OF_BUFFER	4
//#define BUFFER_SIZE			(2580480)
#define BUFFER_SIZE			(774144) //756*1024

#elif PLAY_ENTIRE

#define NUMBER_OF_BUFFER	1
#define BUFFER_SIZE			10*1024*1024

#endif

#define SEND_SMS_MAILBOX	1

int buffer_number;

typedef struct LPA_BUFFER_LIST
{
	struct list_head list;
	spinlock_t lock;
	unsigned int buffer_num;
	unsigned int eos_flag;
} LPA_BUFFER_LIST;

LPA_BUFFER_LIST *lpa_buffer_list;

EVENT_LPA *event_lpa;
unsigned int lpa_buffer_is_empty[NUMBER_OF_BUFFER];

static void workqueue_lpa(struct work_struct *unused);
struct workqueue_struct *lpa_workqueue;
DECLARE_WORK(lpa_work, workqueue_lpa);
struct work_struct lpa_work;
wait_queue_head_t lpa_poll_waitqueue;

unsigned int old_codec_type;
				
LPA_AUDIO_INFO audio_info;


atomic_t lpa_status = ATOMIC_INIT(LPA_OFF);

static int
dsp_lpa_open(struct inode *inode, struct file *filp)
{
	struct cdev *cdev;
	DSP_DEVICE_T *dsp_dev;
	int i;	

	struct clk *cclk_dsp;
	struct clk *aclk_dsp;
	struct clk *pclk_dsp;
	struct clk *async_aclk_dsp;

	void __iomem	*aud_crg_base;
	int ret;

	aud_crg_base = ioremap(0x34670000, 0x100);
	if (!aud_crg_base) {
		printk("audio_crg ioremap failed\n");
		return -ENOMEM;
	}
	__raw_writel(0xffffff80, aud_crg_base + 0x100);

	odin_pd_on(PD_AUD, 0);

	__raw_writel(0xffffffff, aud_crg_base + 0x100);

	iounmap(aud_crg_base);

#if 1
	{
		void __iomem	*aud_crg_base;

		aud_crg_base = ioremap(0x34670000, 0x100);
		if (!aud_crg_base) {
			printk("audio_crg ioremap failed\n");
			return -ENOMEM;
		}

		cclk_dsp = clk_get(NULL, "p_cclk_dsp");
		aclk_dsp = clk_get(NULL, "aclk_dsp");
		pclk_dsp = clk_get(NULL, "pclk_dsp");
		async_aclk_dsp = clk_get(NULL, "p_pclk_async");
	
		__raw_writel(0x0, aud_crg_base + 0x10);
		
		clk_set_rate(cclk_dsp, 24000000);
		ret = clk_prepare_enable(cclk_dsp);
		if (ret)
		{
			printk("cclk_dsp enable failed : %d\n", ret);
		}

		clk_set_rate(aclk_dsp, 12000000);
		ret = clk_prepare_enable(aclk_dsp);
		if (ret)
		{
			printk("aclk_aud enable failed : %d\n", ret);
		}
		
		__raw_writel(0x1, aud_crg_base + 0x14);

		clk_set_rate(pclk_dsp, 12000000);
		ret = clk_prepare_enable(pclk_dsp);
		if (ret)
		{
			printk("pclk_aud enable failed : %d\n", ret);
		}

		__raw_writel(0x0, aud_crg_base + 0x30);
		
		clk_set_rate(async_aclk_dsp, 12000000);
		ret = clk_prepare_enable(async_aclk_dsp);
		if (ret)
		{
			printk("pclk_aud enable failed : %d\n", ret);
		}
		ret = clk_prepare_enable(async_aclk_dsp);
		if (ret)
		{
			printk("async_aclk_dsp enable failed : %d\n", ret);
		}
		__raw_writel(0x1, aud_crg_base + 0x34);


		iounmap(aud_crg_base);
	}
#endif


#if 0
	{
		void __iomem	*aud_crg_base;

		aud_crg_base = ioremap(0x34670000, 0x100);
		if (!aud_crg_base) {
			printk("audio_crg ioremap failed\n");
			return -ENOMEM;
		}

		__raw_writel(0x0, aud_crg_base + 0x10);
		__raw_writel(0x0, aud_crg_base + 0x10);
		__raw_writel(0x0, aud_crg_base + 0x18);
		__raw_writel(0x0, aud_crg_base + 0x18);
		__raw_writel(0x1, aud_crg_base + 0x1C);
		__raw_writel(0x1, aud_crg_base + 0x1C);

		__raw_writel(0x1, aud_crg_base + 0x20);
		__raw_writel(0x1, aud_crg_base + 0x20);
		__raw_writel(0x1, aud_crg_base + 0x28);
		__raw_writel(0x1, aud_crg_base + 0x28);

		__raw_writel(0x1, aud_crg_base + 0x14);
		__raw_writel(0x1, aud_crg_base + 0x14);

		__raw_writel(0x0, aud_crg_base + 0x30);
		__raw_writel(0x0, aud_crg_base + 0x30);
		__raw_writel(0x0, aud_crg_base + 0x38);
		__raw_writel(0x0, aud_crg_base + 0x38);
		__raw_writel(0x1, aud_crg_base + 0x3c);
		__raw_writel(0x1, aud_crg_base + 0x3c);
		__raw_writel(0x1, aud_crg_base + 0x34);
		__raw_writel(0x1, aud_crg_base + 0x34);

		__raw_writel(0x0, aud_crg_base + 0x50);
		__raw_writel(0x0, aud_crg_base + 0x50);
		__raw_writel(0x0, aud_crg_base + 0x58);
		__raw_writel(0x0, aud_crg_base + 0x58);
		__raw_writel(0x1, aud_crg_base + 0x5c);
		__raw_writel(0x1, aud_crg_base + 0x5c);
		__raw_writel(0x1, aud_crg_base + 0x54);
		__raw_writel(0x1, aud_crg_base + 0x54);

		iounmap(aud_crg_base);

#if 0
		aud_crg_base = ioremap(0x34705000, 0x100);
		if (!aud_crg_base) {
			printk("audio_crg ioremap failed\n");
			return -ENOMEM;
		}
		__raw_writel(0x01000031, aud_crg_base + 0x4);
		__raw_writel(0x31, aud_crg_base + 0x8);
		__raw_writel(0x01000031, aud_crg_base + 0x8);

		iounmap(aud_crg_base);
#endif
	}
#endif

#if 1
	printk( "call lpa_dsp open start\n");
#endif
	cdev = inode->i_cdev;
	dsp_dev = container_of(cdev, DSP_DEVICE_T, cdev);

	if (dsp_dev->dev_open_count == 0)
	{
		dsp_dev->is_initialized = 0;
	}

	buffer_number = 0;

	event_lpa = kmalloc(sizeof(EVENT_LPA),GFP_KERNEL);
	if (event_lpa == NULL)
	{
		printk(KERN_ERR "Can't allocate kernel memory\n");
		return RET_OUT_OF_MEMORY;
	}
	memset(event_lpa, 0, sizeof(EVENT_LPA));
	init_waitqueue_head(&event_lpa->wait);
	event_lpa->lpa_readqueue_count = 0;

	lpa_workqueue = create_workqueue("lpa_work");
	if (lpa_workqueue == NULL)
		return -ENOMEM;

	dsp_mode->is_opened_lpa++;

	for (i=0;i<NUMBER_OF_BUFFER;i++)
	{
		lpa_buffer_is_empty[i] = 1;
	}

	lpa_buffer_list = kmalloc(sizeof(LPA_BUFFER_LIST), GFP_KERNEL);
	spin_lock_init(&lpa_buffer_list->lock);
	INIT_LIST_HEAD(&lpa_buffer_list->list);

	old_codec_type = 0;

	init_waitqueue_head(&lpa_poll_waitqueue);

	atomic_set(&lpa_status, LPA_OFF);

	workqueue_flag.lpa_workqueue_flag= 0;
	workqueue_flag.pcm_workqueue_flag= 0;

	atomic_set(&status, 1);

#if CHECK_DSP_DEBUG
	printk( "call lpa_dsp open finish\n");
#endif
	return RET_OK;
}

static int
dsp_lpa_close(struct inode *inode, struct file *filp)
{
	struct clk *cclk_dsp;
	struct clk *aclk_dsp;
	struct clk *pclk_dsp;
	struct clk *async_aclk_dsp;
	struct cdev *cdev;
	DSP_DEVICE_T *dsp_dev;
	MAILBOX_DATAREGISTER send_data_register;
	unsigned int transaction_number;
	int ret;

#if CHECK_DSP_DEBUG
	printk( "call dsp_lpa close start\n");
#endif	

#if PLAY_PP
	switch (atomic_read(&lpa_status))
	{
		case LPA_PLAY:
		case LPA_PAUSE:
			{

				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number = hal_set_command(&send_data_register,
						CONTROL_STOP,
						REQUEST);
				hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
				printk( "wait CONTROL_STOP RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == CONTROL_STOP,
							msecs_to_jiffies(100)))
				{
					printk( "Can't received CONTROL_STOP in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk( "receive CONTROL_STOP RES command\n");
#endif

				atomic_set(&lpa_status, LPA_STOP);
			}
		case LPA_STOP:
			{
				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number = hal_set_command(&send_data_register, CLOSE, REQUEST);
				hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
				printk( "wait CLOSE RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == CLOSE,
							msecs_to_jiffies(100)))
				{
					printk( "Can't received CLOSE in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk( "receive CLOSE RES command\n");
#endif
				
				atomic_set(&lpa_status, LPA_CREATE);
			}
#if !defined(WITH_BINARY)
		case LPA_CREATE :
			{
				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number = hal_set_command(&send_data_register, DELETE, REQUEST);
				hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
				printk( "wait DELETE RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == DELETE,
							msecs_to_jiffies(100)))
				{
					printk( "Can't received DELETE in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk( "receive DELETE RES command\n");
#endif
				atomic_set(&lpa_status, LPA_READY);
			}
			break;
#endif
		default:
			break;
	}

#if !defined(WITH_BINARY)
	dsp_power_off();
				
	atomic_set(&lpa_status, LPA_OFF);
#endif
#endif /* PLAY_PP */

	cdev = inode->i_cdev;
	dsp_dev = container_of(cdev, DSP_DEVICE_T, cdev);

	if (dsp_dev->dev_open_count > 0)
	{
		--dsp_dev->dev_open_count;
	}

#if PLAY_PP

	workqueue_flag.lpa_workqueue_flag = -1;

	wake_up(&lpa_poll_waitqueue);

	if (lpa_workqueue)
	{
		destroy_workqueue(lpa_workqueue);
		lpa_workqueue = NULL;
	}

	kfree(lpa_buffer_list);
	kfree(event_lpa);
#endif
	dsp_mode->is_opened_lpa--;

	atomic_set(&status, 0);

	cclk_dsp = clk_get(NULL, "p_cclk_dsp");
	aclk_dsp = clk_get(NULL, "aclk_dsp");
	pclk_dsp = clk_get(NULL, "pclk_dsp");
	async_aclk_dsp = clk_get(NULL, "p_pclk_async");

	clk_disable_unprepare(async_aclk_dsp);
	clk_disable_unprepare(pclk_dsp);
	clk_disable_unprepare(aclk_dsp);
	clk_disable_unprepare(cclk_dsp);

#if 1
	printk( "call dsp_lpa close finish\n");
#endif
	return 0;
}
	
static long
dsp_lpa_ioctl(struct file *filp, unsigned int cmd, unsigned long arg )
{
	int ret;
	unsigned int transaction_number;

	switch (cmd)
	{
#ifndef WITH_BINARY
		case DSP_IO_POWER_ON :
			{
#if CHECK_DSP_DEBUG
				printk("DSP_IO_POWER_ON\n");
#endif
				ret = dsp_power_on();

				atomic_set(&lpa_status, LPA_ON);
				if (ret<0)
				{
					printk("dsp power can't open\n");
					return RET_ERROR;
				}
			}
			break;
		case DSP_IOW_FIRMWARE_DOWNLOAD_DATA :
			{
				DSP_BUFFER firmware_buffer;

#if CHECK_DSP_DEBUG
				printk("DSP_IOW_FIRMWARE_DOWNLOAD_DATA\n");
#endif
				if (copy_from_user(&firmware_buffer,
							(void __user *)arg,
							sizeof(DSP_BUFFER)))
				{
					printk("Can't recieve data firmware\n");
					return RET_ERROR;
				}

				ret = dsp_download_dram(&firmware_buffer);

			}
			break;
		case DSP_IOW_FIRMWARE_DOWNLOAD_INST :
			{
				DSP_BUFFER firmware_buffer;
#if CHECK_DSP_DEBUG
				printk("DSP_IOW_FIRMWARE_DOWNLOAD_INST\n");
#endif
				if (copy_from_user(&firmware_buffer,
							(void __user *)arg,
							sizeof(DSP_BUFFER)))
				{
					printk("Can't recieve data firmware\n");
					return RET_ERROR;
				}

				ret = dsp_download_iram(&firmware_buffer);
			}
			break;
#endif
		case DSP_LPA_IO_READY:
			{
				struct clk *dsp_clk;
				unsigned int dsp_clk_rate;
				unsigned int axi_ahb_clk;
				unsigned int sync_apb_clk;
				unsigned int async_apb_clk;
				MAILBOX_DATAREGISTER send_data_register;
#if CHECK_DSP_DEBUG
				printk("ioctl(DSP_LPA_IO_READY) called\n");
#endif
				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));

#if 0
				dsp_clk = clk_get(NULL, "cclk_dsp");
				dsp_clk_rate = (unsigned int)clk_get_rate(dsp_clk);
				dsp_clk_rate /= 1000000;
				dsp_clk = clk_get(NULL, "aclk_aud");
				axi_ahb_clk = (unsigned int)clk_get_rate(dsp_clk);
#if CHECK_DSP_DEBUG
				printk("axi_ahb_clk = %d\n", axi_ahb_clk);
#endif
				axi_ahb_clk /= 1000000;
				dsp_clk = clk_get(NULL, "pclk_aud");
				sync_apb_clk = (unsigned int)clk_get_rate(dsp_clk);
#if CHECK_DSP_DEBUG
				printk("sync_apb_clk = %d\n", sync_apb_clk);
#endif
				sync_apb_clk /= 1000000;
				dsp_clk = clk_get(NULL, "pclk_async");
				async_apb_clk = (unsigned int)clk_get_rate(dsp_clk);
#if CHECK_DSP_DEBUG
				printk("async_apb_clk = %d\n", async_apb_clk);
#endif
				async_apb_clk /= 1000000;
#endif

				transaction_number = hal_set_command(&send_data_register, READY, REQUEST);
#if 0
				hal_set_parameter(8,
						(void *)&send_data_register,
						READY, dsp_clk_rate,
						axi_ahb_clk,
						sync_apb_clk,
						async_apb_clk,
						dsp_lpa_mem_cfg.sys_table_addr,
						dsp_lpa_mem_cfg.sys_table_size);
#endif

#if 1
				hal_set_parameter(8,
						(void *)&send_data_register,
						READY,
						24,
						12,
						12,
						12,
						pst_dsp_mem_cfg->sys_table_addr,
						pst_dsp_mem_cfg->sys_table_size);
#endif
				hal_send_mailbox(CH0_REQ_RES, &send_data_register);

#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
				printk("wait READY RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == READY,
							HZ))
				{
					printk("Can't received READY in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk("receive READY RES command\n");
#endif
				atomic_set(&lpa_status, LPA_READY);

				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number = hal_set_command(&send_data_register, CREATE, REQUEST);

				hal_set_parameter(4, &send_data_register, CREATE, DECODE, LPA);
				hal_send_mailbox(CH0_REQ_RES, &send_data_register);

#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
				printk("wait CREATE RES command\n");
#endif

				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == CREATE,HZ))
				{
					printk("Can't received CREATE in timeout\n");
					return RET_ERROR;
				}

#if CHECK_DSP_DEBUG
				printk("receive CREATE RES command\n");
#endif
				atomic_set(&lpa_status, LPA_CREATE);
			}
			break;
		case DSP_LPA_IOW_INIT:
			{
				MAILBOX_DATAREGISTER send_data_register;
				unsigned int stereo;
				unsigned int woofer;

#if CHECK_DSP_DEBUG
				printk("ioctl(DSP_LPA_IOW_INIT) called\n");
#endif
				if (copy_from_user(&audio_info,
							(void __user *)arg,
							sizeof(LPA_AUDIO_INFO)))
				{
#if CHECK_DSP_DEBUG
					printk("Can't recieve audio_info\n");
#endif
					return RET_ERROR;
				}

				/* 예전 곡이랑 코덱타입이 다르면 
				 * 다시 OPEN을 호출해야함
				 * 처음에는 무조건 OPEN 날림 */
				if (old_codec_type != audio_info.codec_type)
				{
					/* old_codec_type이 0이 아니라는 
					 * 것은 이미 노래가 한번 플레이되었다는 의미
					 * 따라서 코덱 change를 하려면 OPEN Req 전에 
					 * CLOSE Req를 보내야 함*/
					if (old_codec_type != 0)
					{
						memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
						transaction_number =
							hal_set_command(&send_data_register, CLOSE, REQUEST);
						hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
						wait_event_interruptible(dsp_wait->wait,
								received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
						printk( "wait CLOSE RES command\n");
#endif
						if (!wait_event_interruptible_timeout(event_lpa->wait,
									event_lpa->command == CLOSE,HZ))
						{
							printk(KERN_ERR "Can't received CLOSE in timeout\n");
							return RET_ERROR;
						}
#if CHECK_DSP_DEBUG
						printk( "receive CLOSE RES command\n");
#endif
						atomic_set(&lpa_status, LPA_CREATE);
					}

					memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
					transaction_number = hal_set_command(&send_data_register, OPEN, REQUEST);
					hal_set_parameter(3, &send_data_register, OPEN, audio_info.codec_type);
					hal_send_mailbox(CH0_REQ_RES, &send_data_register);

					old_codec_type = audio_info.codec_type;
#if RECEIVE_SEND2
					wait_event_interruptible(dsp_wait->wait,
							received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
					printk("wait OPEN RES command\n");
#endif
					if (!wait_event_interruptible_timeout(event_lpa->wait,
								event_lpa->command == OPEN,HZ))
					{
						printk("Can't received OPEN in timeout\n");
						return RET_ERROR;
					}
#if CHECK_DSP_DEBUG
					printk("receive OPEN RES command\n");
#endif

					atomic_set(&lpa_status, LPA_OPEN);
				}

				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number =
					hal_set_command(&send_data_register,
						CONTROL_INIT,
						REQUEST);

				stereo = 0;
				woofer = 0;

				switch (audio_info.channel_number)
				{
					case CHANNEL_1 :
						{
							stereo = 1;
							woofer = 0;
						} break;
					case CHANNEL_2 :
						{
							stereo = 2;
							woofer = 0;
						} break;
					case CHANNEL_3 :
						{
							stereo = 3;
							woofer = 0;
						} break;
					case CHANNEL_4 :
						{
							stereo = 4;
							woofer = 0;
						} break;
					case CHANNEL_5 :
						{
							stereo = 5;
							woofer = 0;
						} break;
					case CHANNEL_51 :
						{
							stereo = 5;
							woofer = 1;
						} break;
					case CHANNEL_61 :
						{
							stereo = 6;
							woofer = 1;
						} break;
				}

				hal_set_parameter(7,
						&send_data_register,
						CONTROL_INIT,
						stereo,
						woofer,
						audio_info.sampling_freq,
						audio_info.bit_per_sample,
						audio_info.bit_rate);

				hal_send_mailbox(CH0_REQ_RES, &send_data_register);

#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif				

#if CHECK_DSP_DEBUG
				printk("wait CONTROL_INIT RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == CONTROL_INIT,HZ))
				{
					printk("Can't received CONTROL_INIT in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk("receive CONTROL_INIT RES command\n");
#endif

				atomic_set(&lpa_status, LPA_INIT);
			}
			break;
		case DSP_LPA_IO_START:
			{
#if CHECK_DSP_DEBUG
				printk("ioctl(DSP_LPA_IO_START) called\n");
#endif			
			}
			break;
		case DSP_LPA_IOW_PLAY:
			{
				unsigned int nearstop_threshold;
				BUFFER_INFO buffer_info;
				LPA_BUFFER_LIST *send_buffer_list;
				MAILBOX_DATAREGISTER send_data_register;
				int i;

#if SEND_SMS_MAILBOX
				if (atomic_read(&lpa_status) == LPA_PLAY)
				{
#if CHECK_DSP_DEBUG
					printk("odin_lpa_write_done\n");
#endif
					odin_lpa_write_done(PD_AUD, 0);
				}
#endif

				send_buffer_list = kmalloc(sizeof(LPA_BUFFER_LIST), GFP_KERNEL);

#if CHECK_DSP_DEBUG
				printk("ioctl(DSP_LPA_IOW_PLAY) called\n");
#endif
				if (copy_from_user(&buffer_info, (void __user *)arg, sizeof(BUFFER_INFO)))
				{
					printk("Can't recieve buffer_info\n");
					return RET_ERROR;
				}

#if CHECK_DSP_DEBUG
				printk("phys_addr = 0x%08X\n", buffer_info.buffer);
				printk("size = 0x%08X\n", buffer_info.size);
				printk("eos_flag = %d\n", buffer_info.eos_flag);
#endif				

				if (buffer_info.eos_flag)
				{
					memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
					transaction_number =
						hal_set_command(&send_data_register,
								CONTROL_PLAY_PARTIAL,
								REQUEST);

					hal_set_parameter(4,
							&send_data_register,
							CONTROL_PLAY_PARTIAL_END,
							buffer_info.buffer,
							buffer_info.size);

					hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
					wait_event_interruptible(dsp_wait->wait,
							received_transaction_number==transaction_number);
#endif
					for (i=0;i<NUMBER_OF_BUFFER;i++)
					{
						if (buffer_info.buffer ==
								pst_dsp_mem_cfg->stream_buffer_addr+
								i*pst_dsp_mem_cfg->stream_buffer_size)
						{
#if CHECK_DSP_DEBUG
							printk("write buffer number = %d\n", i);
#endif
							lpa_buffer_is_empty[i] = 0;
							break;
						}
					}

					event_lpa->eos_flag = buffer_info.eos_flag;

					send_buffer_list->buffer_num = i;
					send_buffer_list->eos_flag = buffer_info.eos_flag;
					INIT_LIST_HEAD(&send_buffer_list->list);

					spin_lock(&lpa_buffer_list->lock);
					list_add_tail(&send_buffer_list->list, &lpa_buffer_list->list);
					spin_unlock(&lpa_buffer_list->lock);

#if SEND_SMS_MAILBOX
					if (atomic_read(&lpa_status) != LPA_PLAY)
					{
#if CHECK_DSP_DEBUG
						printk("odin_lpa_start\n");
#endif
						odin_lpa_start(PD_AUD, 0);
					}
#endif

					atomic_set(&lpa_status, LPA_PLAY);
				}
				else
				{
					memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
#if PLAY_PP
					transaction_number =
						hal_set_command(&send_data_register,
							CONTROL_PLAY_PARTIAL,
							REQUEST);
					
					/* 지금은 남은 %를 보내지만 stream의 size로 바꿀 예정 */
					nearstop_threshold = NEARSTOP_THRESHOLD;
					hal_set_parameter(5,
							&send_data_register,
							CONTROL_PLAY_PARTIAL,
							buffer_info.buffer,
							buffer_info.size,
							nearstop_threshold);

					hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
					wait_event_interruptible(dsp_wait->wait,
							received_transaction_number==transaction_number);
#endif /* #if RECEIVE_SEND2 */

					for (i=0;i<NUMBER_OF_BUFFER;i++)
					{
						if (buffer_info.buffer ==
								pst_dsp_mem_cfg->stream_buffer_addr+
								i*pst_dsp_mem_cfg->stream_buffer_size)
						{
#if CHECK_DSP_DEBUG
							printk("write buffer number = %d\n", i);
#endif
							lpa_buffer_is_empty[i] = 0;
							break;
						}
					}

					send_buffer_list->buffer_num = i;
					send_buffer_list->eos_flag = buffer_info.eos_flag;

					INIT_LIST_HEAD(&send_buffer_list->list);

					spin_lock(&lpa_buffer_list->lock);
					list_add_tail(&send_buffer_list->list, &lpa_buffer_list->list);
					spin_unlock(&lpa_buffer_list->lock);

#if SEND_SMS_MAILBOX
					if (atomic_read(&lpa_status) != LPA_PLAY)
					{
#if CHECK_DSP_DEBUG
						printk("odin_lpa_start\n");
#endif
						odin_lpa_start(PD_AUD, 0);
					}
#endif

					atomic_set(&lpa_status, LPA_PLAY);
					atomic_set(&status, 2);

#endif /* #if PLAY_PP */

#if PLAY_ENTIRE
					transaction_number =
						hal_set_command(&send_data_register,
								CONTROL_PLAY_ENTIRE,
								REQUEST);

					/* 지금은 남은 %를 보내지만 stream의 size로 바꿀 예정 */
					nearstop_threshold = 20;

					hal_set_parameter(5,
							&send_data_register,
							CONTROL_PLAY_ENTIRE,
							buffer_info.buffer,
							buffer_info.size,
							nearstop_threshold);

					hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
					wait_event_interruptible(dsp_wait->wait,
							received_transaction_number==transaction_number);
#endif	/* #if RECEIVE_SEND2 */

#if 0

#if CHECK_DSP_DEBUG
					printk("wait CONTROL_PLAY_ENTIRE RES command\n");
#endif
					if (!wait_event_interruptible_timeout(event_lpa->wait,
								event_lpa->command == CONTROL_PLAY_ENTIRE,
								msecs_to_jiffies(100)))
					{
						printk("Can't received CONTROL_PLAY_ENTIRE in timeout\n");
						return RET_ERROR;
					}
#if CHECK_DSP_DEBUG
					printk("receive CONTROL_PLAY_ENTIRE RES command\n");
#endif
#endif

#endif 	/* #if PLAY_ENTIRE */
				}

#if defined(CONFIG_WODEN_DSP)
				if (*(dsp_woden_mem_ctl->dsp_control_register+15)==0xEEEE0001)
				{
					return -1;
				}
				else if (*(dsp_woden_mem_ctl->dsp_control_register+15)==0xEEEE0002)
				{
					return -1;
				}
				else if (*(dsp_woden_mem_ctl->dsp_control_register+15)==0xEEEE0003)
				{
					return -1;
				}
#endif


#ifdef CONFIG_PM_WAKELOCKS
				if (wake_lock_active(&odin_wake_lock))
				{
#if CHECK_DSP_DEBUG
					printk("lpa_wake_unlock\n");
#endif
					wake_unlock(&odin_wake_lock);
				}
#endif
			}
			break;
		case DSP_LPA_IOR_BUFFER_NUMBER_INFO:
			{
				int i;

				for (i=0;i<NUMBER_OF_BUFFER;i++)
				{
					/* 이 방식으로 하면 버퍼 2개만 있어도 됨*/
					if (lpa_buffer_is_empty[i] == 1)
					{
						buffer_number=(unsigned int)i;
						if (copy_to_user((void *)arg, (void *)&buffer_number, sizeof(int)))
						{
							printk("Can't send buffer_bumber_info\n");
							return RET_ERROR;
						}
						break;
					}
				}
				printk("buffer_number = %d\n", buffer_number);
			}
			break;
		case DSP_LPA_IO_FLUSH : /* 모든 버퍼 비움 */
			{

				MAILBOX_DATAREGISTER send_data_register;
				LPA_BUFFER_LIST buffer_list;
				unsigned int stereo;
				unsigned int woofer;
				int i;

				/* CONTROL_STOP -> CONTROL_INIT */
				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number =
					hal_set_command(&send_data_register,
							CONTROL_STOP,
							REQUEST);

				hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
				printk("wait CONTROL_STOP RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == CONTROL_STOP,
							msecs_to_jiffies(100)))
				{
					printk("Can't received CONTROL_STOP in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk("receive CONTROL_STOP RES command\n");
#endif

#if SEND_SMS_MAILBOX
#if CHECK_DSP_DEBUG
				printk("odin_lpa_stop\n");
#endif
				odin_lpa_stop(PD_AUD, 0);
#endif
				atomic_set(&lpa_status, LPA_STOP);

				while (!list_empty(&lpa_buffer_list->list))
				{
					spin_lock(&lpa_buffer_list->lock);
					memcpy(&buffer_list,
							list_first_entry(&lpa_buffer_list->list,
								LPA_BUFFER_LIST,
								list),
							sizeof(LPA_BUFFER_LIST));

					list_del(&buffer_list.list);
					spin_unlock(&lpa_buffer_list->lock);
				}

				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number =
					hal_set_command(&send_data_register,
						CONTROL_INIT,
						REQUEST);

				stereo = 0;
				woofer = 0;

				switch (audio_info.channel_number)
				{
					case CHANNEL_1 :
						{
							stereo = 1;
							woofer = 0;
						} break;
					case CHANNEL_2 :
						{
							stereo = 2;
							woofer = 0;
						} break;
					case CHANNEL_3 :
						{
							stereo = 3;
							woofer = 0;
						} break;
					case CHANNEL_4 :
						{
							stereo = 4;
							woofer = 0;
						} break;
					case CHANNEL_5 :
						{
							stereo = 5;
							woofer = 0;
						} break;
					case CHANNEL_51 :
						{
							stereo = 5;
							woofer = 1;
						} break;
					case CHANNEL_61 :
						{
							stereo = 6;
							woofer = 1;
						} break;
				}

				hal_set_parameter(7,
						&send_data_register,
						CONTROL_INIT,
						stereo,
						woofer,
						audio_info.sampling_freq,
						audio_info.bit_per_sample,
						audio_info.bit_rate);

				hal_send_mailbox(CH0_REQ_RES, &send_data_register);

#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif				

#if CHECK_DSP_DEBUG
				printk("wait CONTROL_INIT RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == CONTROL_INIT,
							msecs_to_jiffies(100)))
				{
					printk("Can't received CONTROL_INIT in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk("receive CONTROL_INIT RES command\n");
#endif

				atomic_set(&lpa_status, LPA_INIT);

				for (i=0;i<NUMBER_OF_BUFFER;i++)
				{
					lpa_buffer_is_empty[i] = 1;  /* Set empty */
				}
			}
			break;
		case DSP_LPA_IO_PAUSE :
			{
				MAILBOX_DATAREGISTER send_data_register;
				
				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number =
					hal_set_command(&send_data_register,
							CONTROL_PAUSE,
							REQUEST);

				hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
				printk("wait CONTROL_PAUSE RES command\n");
#endif

				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == CONTROL_PAUSE,HZ))
				{
					printk("Can't received CONTROL_PAUSE in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk("receive CONTROL_PAUSE RES command\n");
#endif

#if 0
#if SEND_SMS_MAILBOX
#if CHECK_DSP_DEBUG
				printk("odin_lpa_stop\n");
#endif
				odin_lpa_stop(PD_AUD, 0);
#endif
#endif

				atomic_set(&lpa_status, LPA_PAUSE);
				atomic_set(&status, 3);
			}
			break;
		case DSP_LPA_IO_RESUME :
			{
				MAILBOX_DATAREGISTER send_data_register;

				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number =
					hal_set_command(&send_data_register,
						CONTROL_RESUME,
						REQUEST);

				hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
				printk("wait CONTROL_RESUME RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == CONTROL_RESUME,
							msecs_to_jiffies(100)))
				{
					printk("Can't received CONTROL_RESUME in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk("receive CONTROL_RESUME RES command\n");
#endif

#if 0
#if SEND_SMS_MAILBOX
#if CHECK_DSP_DEBUG
				printk("odin_lpa_start\n");
#endif
				odin_lpa_start(PD_AUD, 0);
#endif
#endif

				atomic_set(&lpa_status, LPA_PLAY);
				atomic_set(&status, 2);
			}
			break;
		case DSP_LPA_IO_MUTE :
			{
				MAILBOX_DATAREGISTER send_data_register;
				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number =
					hal_set_command(&send_data_register,
							CONTROL_MUTE,
							REQUEST);

				hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
				printk("wait CONTROL_MUTE RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == CONTROL_MUTE,
							msecs_to_jiffies(100)))
				{
					printk("Can't received CONTROL_MUTE in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk("receive CONTROL_MUTE RES command\n");
#endif
			}
			break;
		case DSP_LPA_IO_UNMUTE :
			{
				MAILBOX_DATAREGISTER send_data_register;
				
				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number =
					hal_set_command(&send_data_register,
							CONTROL_UNMUTE,
							REQUEST);

				hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
				printk("wait CONTROL_UNMUTE RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == CONTROL_UNMUTE,
							msecs_to_jiffies(100)))
				{
					printk("Can't received CONTROL_UNMUTE in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk("receive CONTROL_UNMUTE RES command\n");
#endif
			}
			break;
		case DSP_LPA_IO_STOP :
			{
				MAILBOX_DATAREGISTER send_data_register;
				
				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number =
					hal_set_command(&send_data_register,
							CONTROL_STOP,
							REQUEST);
				hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
				printk("wait CONTROL_STOP RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == CONTROL_STOP,
							msecs_to_jiffies(100)))
				{
					printk("Can't received CONTROL_STOP in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk("receive CONTROL_STOP RES command\n");
#endif

#if SEND_SMS_MAILBOX
#if CHECK_DSP_DEBUG
				printk("odin_lpa_stop\n");
#endif
				odin_lpa_stop(PD_AUD, 0);
#endif
				atomic_set(&lpa_status, LPA_STOP);
				atomic_set(&status, 4);
			}
			break;
		case DSP_LPA_IO_CLOSE :
			{
				MAILBOX_DATAREGISTER send_data_register;
				
				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number =
					hal_set_command(&send_data_register,
							CLOSE,
							REQUEST);

				hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
				printk("wait CLOSE RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == CLOSE,
							msecs_to_jiffies(100)))
				{
					printk("Can't received CLOSE in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk("receive CLOSE RES command\n");
#endif
				atomic_set(&lpa_status, LPA_CREATE);
			}
			break;
		case DSP_LPA_IO_DELETE :
			{
				MAILBOX_DATAREGISTER send_data_register;
				
				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number =
					hal_set_command(&send_data_register,
							DELETE,
							REQUEST);

				hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
				printk("wait DELETE RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == DELETE,
							msecs_to_jiffies(100)))
				{
					printk("Can't received DELETE in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk("receive DELETE RES command\n");
#endif
				atomic_set(&lpa_status, LPA_READY);
			}
			break;
		case DSP_LPA_IOW_VOLUME :
			{
				MAILBOX_DATAREGISTER send_data_register;
				int vol_value;

				if (copy_from_user(&vol_value, (void __user *)arg, sizeof(int)))
				{
					printk("Can't recieve vol_value\n");
					return RET_ERROR;
				}

				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number =
					hal_set_command(&send_data_register,
							CONTROL_VOLUME,
							REQUEST);

				hal_set_parameter(3,
						&send_data_register,
						CONTROL_VOLUME,
						(unsigned int)vol_value);

				hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if CHECK_DSP_DEBUG
				printk("wait CONTROL_VOLUME RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_lpa->wait,
							event_lpa->command == CONTROL_VOLUME,
							msecs_to_jiffies(100)))
				{
					printk("Can't received CONTROL_VOLUME in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk("receive CONTROL_VOLUME RES command\n");
#endif
			}
			break;
		case DSP_LPA_IOR_GETTIMESTAMP :
			{
				int i;
				MAILBOX_DATAREGISTER send_data_register;
				long long timestamp;

				for (i=0;i<3;i++)
				{
					memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
					transaction_number =
						hal_set_command(&send_data_register,
								CONTROL_GET_TIMESTAMP,
								REQUEST);

					hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
					wait_event_interruptible(dsp_wait->wait,
							received_transaction_number==transaction_number);
#endif

					if (!wait_event_interruptible_timeout(event_lpa->wait,
								event_lpa->command == CONTROL_GET_TIMESTAMP,
								msecs_to_jiffies(100)))
					{
						printk("Can't received CONTROL_GET_TIMESTAMP in timeout\n");
						continue;
					}
#if CHECK_DSP_DEBUG
					printk("receive CONTROL_GET_TIMESTAMP RES command\n");
#endif
					break;
				}
				
				if (i==3)
				{
					return RET_ERROR;
				}

				timestamp = event_lpa->timestamp.value;

				if (copy_to_user((void *)arg, (void *)&timestamp, sizeof(long long)))
				{
					printk("Can't send timestamp\n");
					return RET_ERROR;
				}						
			}
			break;
#if 0
		case DSP_LPA_IOR_GETAUDIOINFO :
			{
				AUDIO_INFO audio_info;

				transaction_number =
					HalSendCommandMailbox(CONTROL_GET_AUDIO_INFO,
							REQUEST);
#if RECEIVE_MAILBOX
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if RECEIVE_MAILBOX
				printk("wait CONTROL_GET_AUDIO_INFO RES command\n");
				wait_event_interruptible(event_lpa->wait,
						event_lpa->command == CONTROL_GET_AUDIO_INFO);
				printk("receive CONTROL_GET_AUDIO_INFO RES command\n");
#endif				

				memcpy(&audio_info, &event_lpa->audio_info, sizeof(AUDIO_INFO));

				if (copy_to_user((void *)arg, (void *)&audio_info, sizeof(AUDIO_INFO)))
				{
					printk("Can't send timestamp\n");
					return RET_ERROR;
				}						

			}
			break;
#endif
	}

	return RET_OK;
}

static int
dsp_lpa_mmap(struct file *filp, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;

	vma->vm_flags |= VM_IO;
	vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);

	vma->vm_page_prot =
		phys_mem_access_prot(filp,
				vma->vm_pgoff,
				size,
				vma->vm_page_prot);

	if (remap_pfn_range(vma,
				vma->vm_start,
				vma->vm_pgoff,
				size,
				vma->vm_page_prot))
	{
		return -EAGAIN;
	}
	
	return RET_OK;
}

static unsigned int
dsp_lpa_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &lpa_poll_waitqueue, wait);

	if (event_lpa->lpa_readqueue_count)
	{
#if CHECK_DSP_DEBUG
		printk("poll event wakeup : %s\n", __FUNCTION__);
#endif
		event_lpa->lpa_readqueue_count = 0;
		mask |= POLLIN | POLLRDNORM;
	}
	
	return mask;
}

ssize_t
dsp_lpa_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	unsigned long flags;

	local_save_flags(flags);
	local_irq_disable();

		
	put_user((char)event_lpa->command, (char *)buf);
#if 0
	if (event_lpa->lpa_readqueue_count)
	{	
		put_user((char)event_lpa->command, (char *)buf);

	}
#endif
	local_irq_restore(flags);

	return event_lpa->lpa_readqueue_count;
}

static void
workqueue_lpa(struct work_struct *unused)
{
	MAILBOX_DATAREGISTER received_data_regsiter;

	memcpy(&received_data_regsiter,
			pst_received_data_register,
			sizeof(MAILBOX_DATAREGISTER));

#if CHECK_DSP_DEBUG
		printk("Entered lpa workqueue\n");
#endif

#if 0
		mutex_lock(&mutex);
		if (mutex_lockinterruptible(&mutex))
			return -ERESTARTSYS;
#endif

		if (workqueue_flag.lpa_workqueue_flag == -1)
		{
			workqueue_flag.lpa_workqueue_flag = 0;
			return;
		}	
		/* if REQ/RES mailbox(channel 0) */
		else if (workqueue_flag.lpa_workqueue_flag == 1)
		{
			MAILBOX_DATA_REGISTER0_CHANNEL0 data_register0;

			data_register0 = (MAILBOX_DATA_REGISTER0_CHANNEL0)
				(received_data_regsiter.data_register[0]);

			event_lpa->command = data_register0.field.command;
			event_lpa->type = data_register0.field.type;

			/* If RES Mailbox */
			if (event_lpa->type == RESPONSE)
			{
				if ((event_lpa->command == CONTROL_PLAY_PARTIAL)||
						(event_lpa->command==CONTROL_PLAY_PARTIAL_END))
				{
					LPA_BUFFER_LIST received_buffer_list;
					unsigned int eos_flag;
					
					eos_flag = 0;
#if CHECK_DSP_DEBUG
					printk("Recieved CONTROL_PLAY_PARTIAL RES\n");
#endif
					spin_lock(&lpa_buffer_list->lock);
					memcpy(&received_buffer_list,
						list_first_entry(&lpa_buffer_list->list,
								LPA_BUFFER_LIST,
								list),
						sizeof(LPA_BUFFER_LIST));
					spin_unlock(&lpa_buffer_list->lock);

#if CHECK_DSP_DEBUG
					printk("received_buffer_list->buffer_num = %d\n",
							received_buffer_list.buffer_num);
#endif
					/* Set this buffer enable */
					lpa_buffer_is_empty[received_buffer_list.buffer_num] = 1;

					eos_flag = received_buffer_list.eos_flag;

#if CHECK_DSP_DEBUG
					printk("received_buffer_list->eos_flag = %d\n",
							received_buffer_list.eos_flag);
#endif

					spin_lock(&lpa_buffer_list->lock);
					list_del(&received_buffer_list.list);
					spin_unlock(&lpa_buffer_list->lock);

					if (eos_flag == 1)
					{
#if CHECK_DSP_DEBUG
						printk("Received last CONTROL_PLAY_PARTIAL RES\n");
#endif

#if LPA_NEAR_STOP
						event_lpa->lpa_readqueue_count = 1;
						eos_flag = 0;
						wake_up(&lpa_poll_waitqueue);
#endif

#if LPA_DOUBLE_BUFFER
						event_lpa->command = CONTROL_PLAY_PARTIAL_END;
						event_lpa->lpa_readqueue_count = 1;
						eos_flag = 0;
						wake_up(&lpa_poll_waitqueue);
#endif
					}
#if LPA_DOUBLE_BUFFER
					else if (eos_flag == 0)
					{
						if (!event_lpa->eos_flag)
						{
							event_lpa->eos_flag = 0;
							event_lpa->lpa_readqueue_count = 1;
							wake_up(&lpa_poll_waitqueue);
						}
					}
#endif
				}
				else if (event_lpa->command == CONTROL_GET_TIMESTAMP)
				{
					MAILBOX_DATA_REGISTER1_CHANNEL0 data_register1;
					MAILBOX_DATA_REGISTER2_CHANNEL0 data_register2;
#if CHECK_DSP_DEBUG
	printk("Received CONTROL_GET_TIMESTAMP\n");
#endif

					data_register1 = (MAILBOX_DATA_REGISTER1_CHANNEL0)
						(received_data_regsiter.data_register[1]);
					data_register2 = (MAILBOX_DATA_REGISTER2_CHANNEL0)
						(received_data_regsiter.data_register[2]);
				
					event_lpa->timestamp.field.timestamp_upper =
						data_register1.field_timestamp.timestamp_upper;
					event_lpa->timestamp.field.timestamp_lower =
						data_register2.field_timestamp.timestamp_lower;
					printk("event_lpa->timestamp.timestamp_upper = %d\n", event_lpa->timestamp.field.timestamp_upper);
					printk("event_lpa->timestamp.timestamp_lower = %d\n", event_lpa->timestamp.field.timestamp_lower);


					printk("event_lpa->timestamp = %lld\n", event_lpa->timestamp.value);

					wake_up_interruptible(&event_lpa->wait);
				}
				else
				{
					wake_up_interruptible(&event_lpa->wait);
				}
			}
			/* If REQ Mailbox */
			else if (event_lpa->type == REQUEST)
			{
			
			}

			workqueue_flag.lpa_workqueue_flag = 0;
			return;
		}
		/* if IND mailbox(channel 1) */
		else if (workqueue_flag.lpa_workqueue_flag == 2)
		{
			workqueue_flag.lpa_workqueue_flag = 0;
			return;
		}
		/* if NTF mailbox(channel 2) */
		else if (workqueue_flag.lpa_workqueue_flag == 3)
		{
			MAILBOX_DATA_REGISTER0_CHANNEL2	data_register0;
			MAILBOX_DATA_REGISTER4_CHANNEL2	data_register4;

			data_register0 = (MAILBOX_DATA_REGISTER0_CHANNEL2)
				(received_data_regsiter.data_register[0]);
			data_register4 = (MAILBOX_DATA_REGISTER4_CHANNEL2)
				(received_data_regsiter.data_register[4]);

			event_lpa->command = data_register0.field.command;

#if LPA_NEAR_STOP
			if (event_lpa->command == NEAR_CONSUMED)
			{
#if CHECK_DSP_DEBUG
				printk("recieved NEAR_CONSUMED NTF command\n");
				printk("remain byte = %d bytes\n",
						data_register4.field_nearconsumed.remain_byte);
#endif
#if PLAY_PP
				event_lpa->lpa_readqueue_count = 1;
				wake_up(&lpa_poll_waitqueue);
#endif
			}
#endif
			workqueue_flag.lpa_workqueue_flag = 0;
			return;
		}

#if 0
		mutex_unlock(&mutex);
#endif
}

struct
file_operations gst_dsp_lpa_fops =
{
	.owner = THIS_MODULE,
	.open 	= dsp_lpa_open,
	.release= dsp_lpa_close,
	.unlocked_ioctl	= dsp_lpa_ioctl,
	.mmap = dsp_lpa_mmap,
	.poll = dsp_lpa_poll,
	.read = dsp_lpa_read,
};

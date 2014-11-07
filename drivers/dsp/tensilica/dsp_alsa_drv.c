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
#include <asm/io.h>			/**< For ioremap_nocache */
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/odin_mailbox.h>
#include <linux/odin_pd.h>
#include "dsp_drv.h"
#include "dsp_alsa_drv.h"
#include "mailbox/mailbox_hal.h"
#include "dsp_ioctl_cmd.h"
	
#if defined(CONFIG_WODEN_DSP)
#include "woden/dsp_coredrv_woden.h"
#elif defined(CONFIG_ODIN_DSP)
#include "odin/dsp_coredrv_odin.h"
#endif

#define	CHECK_DSP_DEBUG	1

#define PCM_PLAYBACK	1
#define PCM_CAPTURE		2

#define ALSA_OFF		0
#define ALSA_ON			1
#define ALSA_READY		2
#define ALSA_CREATE		3
#define ALSA_OPEN		4
#define ALSA_INIT		5
#define ALSA_PLAY		6
#define ALSA_CAPTURE	7



atomic_t alsa_status = ATOMIC_INIT(ALSA_OFF);

struct timer_list *timer;

void dsp_kerneltimer(unsigned long arg);

static void workqueue_alsa(struct work_struct *unused);
struct workqueue_struct *alsa_workqueue;
DECLARE_WORK(alsa_work, workqueue_alsa);
struct work_struct alsa_work;

struct mutex mutex;

typedef struct ALSA_BUFFER_LIST
{
	struct list_head list;
	spinlock_t lock;
	unsigned int buffer_num;
} ALSA_BUFFER_LIST;

ALSA_BUFFER_LIST *alsa_buffer_list;

EVENT_ALSA *event_alsa;

ALSA_RUN_INFO alsa_run_info;

static int
dsp_alsa_open(struct inode *inode, struct file *filp)
{
	struct cdev *cdev;
	DSP_DEVICE_T *dsp_dev;
	
#if	CHECK_DSP_DEBUG
	printk("call dsp_alsa open start\n");
#endif

#if 1 /*LPA TEST 나중에 제거해야함 */

	void __iomem	*aud_crg_base;
	void __iomem	*wm5102_crg_base;

#if !defined(WITH_BINARY)

	aud_crg_base = ioremap(0x34670000, 0x100);
	if (!aud_crg_base) {
		printk("audio_crg ioremap failed\n");
		return -ENOMEM;
	}

	__raw_writel(0xffffff80, aud_crg_base + 0x100);

	odin_pd_on(PD_AUD, 0);

	__raw_writel(0xffffffff, aud_crg_base + 0x100);

	iounmap(aud_crg_base);
#endif

	aud_crg_base = ioremap(0x34670000, 0x100);
	if (!aud_crg_base) {
		printk("audio_crg ioremap failed\n");
		return -ENOMEM;
	}
	__raw_writel(0x1, aud_crg_base + 0x10);
	__raw_writel(0x1, aud_crg_base + 0x10);
	__raw_writel(0xF, aud_crg_base + 0x18);
	__raw_writel(0xF, aud_crg_base + 0x18);
	__raw_writel(0x1, aud_crg_base + 0x1C);
	__raw_writel(0x1, aud_crg_base + 0x1C);
	__raw_writel(0x1, aud_crg_base + 0x20);
	__raw_writel(0x1, aud_crg_base + 0x20);
	__raw_writel(0x1, aud_crg_base + 0x28);
	__raw_writel(0x1, aud_crg_base + 0x28);

	__raw_writel(0x1, aud_crg_base + 0x14);
	__raw_writel(0x1, aud_crg_base + 0x14);

	__raw_writel(0x1, aud_crg_base + 0x30);
	__raw_writel(0x1, aud_crg_base + 0x30);
	__raw_writel(0x1F, aud_crg_base + 0x38);
	__raw_writel(0x1F, aud_crg_base + 0x38);
	__raw_writel(0x1, aud_crg_base + 0x3C);
	__raw_writel(0x1, aud_crg_base + 0x3C);
	__raw_writel(0x1, aud_crg_base + 0x34);
	__raw_writel(0x1, aud_crg_base + 0x34);


	__raw_writel(0x30201, aud_crg_base + 0x0);
	__raw_writel(0x30201, aud_crg_base + 0x0);

	__raw_writel(0x3E80062, aud_crg_base + 0x4);

	__raw_writel(0x0, aud_crg_base + 0x50);
	__raw_writel(0x0, aud_crg_base + 0x58);
	__raw_writel(0x1, aud_crg_base + 0x5c);
	__raw_writel(0x1, aud_crg_base + 0x54);

	iounmap(aud_crg_base);

	wm5102_crg_base = ioremap(0x20009000, 0x40);
	if (!wm5102_crg_base) {
		printk("wm5102_crg_base ioremap failed\n");
		return -ENOMEM;
	}

	__raw_writel(0x01000031, wm5102_crg_base + 0x8);
	__raw_writel(0x01000031, wm5102_crg_base + 0x8);
	__raw_writel(0x00FF00FF, wm5102_crg_base + 0x34);
	__raw_writel(0x00FF00FF, wm5102_crg_base + 0x34);
	iounmap(wm5102_crg_base);

	wm5102_crg_base = ioremap(0x20008000, 0x40);
	if (!wm5102_crg_base) {
		printk("wm5102_crg_base ioremap failed\n");
		return -ENOMEM;
	}

	__raw_writel(0x31, wm5102_crg_base + 0x0);
	__raw_writel(0x00FF0000, wm5102_crg_base + 0x34);

	__raw_writel(0x01000031, wm5102_crg_base + 0x0);
	__raw_writel(0x00FF00FF, wm5102_crg_base + 0x34);
	iounmap(wm5102_crg_base);

#endif

	cdev = inode->i_cdev;
	dsp_dev = container_of(cdev, DSP_DEVICE_T, cdev);

	if (dsp_dev->dev_open_count == 0)
	{
		dsp_dev->is_initialized = 0;
	}

	
	event_alsa = kmalloc(sizeof(EVENT_ALSA),GFP_KERNEL);
	if (event_alsa == NULL)
	{
		printk("Can't allocate kernel memory\n");
		return RET_OUT_OF_MEMORY;
	}
	memset(event_alsa, 0, sizeof(EVENT_ALSA));
	init_waitqueue_head(&event_alsa->wait);


	alsa_workqueue = create_workqueue("alsa_work");
	if (alsa_workqueue == NULL)
		return -ENOMEM;

#if 1
	timer = kmalloc(sizeof(struct timer_list), GFP_KERNEL);
	memset(timer, 0, sizeof(struct timer_list));
	init_timer(timer);
	timer->expires = get_jiffies_64()+HZ;
	timer->data = (unsigned long)NULL;
	timer->function = dsp_kerneltimer;
	add_timer(timer);
#endif

	alsa_buffer_list = kmalloc(sizeof(ALSA_BUFFER_LIST), GFP_KERNEL);
	spin_lock_init(&alsa_buffer_list->lock);
	INIT_LIST_HEAD(&alsa_buffer_list->list);

	memset(&alsa_run_info, 0x0, sizeof(ALSA_RUN_INFO));

	mutex_init(&mutex);

#if	CHECK_DSP_DEBUG
	printk("call dsp_alsa open finish\n");
#endif
	dsp_mode->is_opened_alsa++;
	
	return RET_OK;
}

static int
dsp_alsa_close(struct inode *inode, struct file *filp)
{
	struct cdev *cdev;
	DSP_DEVICE_T *dsp_dev;
	unsigned int transaction_number;


#if	CHECK_DSP_DEBUG
	printk("call woden_dsp_pcm close start : %s\n", __FILE__);
#endif	
	cdev = inode->i_cdev;
	dsp_dev = container_of(cdev, DSP_DEVICE_T, cdev);


	switch (atomic_read(&alsa_status))
	{
		case ALSA_PLAY :
		case ALSA_CAPTURE:
			{
				dsp_alsa_pcm_stop();
				atomic_set(&alsa_status, ALSA_READY);
			}
			break;
		default:
			break;
	}

	dsp_power_off();
	atomic_set(&alsa_status, ALSA_OFF);

	if (dsp_dev->dev_open_count > 0)
	{
		--dsp_dev->dev_open_count;
	}

	dsp_mode->is_opened_alsa--;

#if 1
	kfree(alsa_buffer_list);
	kfree(event_alsa);
#endif

	del_timer(timer);
	kfree(timer);


#if	CHECK_DSP_DEBUG
	printk("call woden_dsp_pcm close finish : %s\n", __FILE__);
#endif	
	return RET_OK;
}

static long
dsp_alsa_ioctl(struct file *filp, unsigned int cmd, unsigned long arg )
{

	int ret;
	unsigned int transaction_number;

	switch (cmd)
	{
		case DSP_IO_POWER_ON :
			{

#if CHECK_DSP_DEBUG
				printk("DSP_IO_POWER_ON\n");
#endif
				if (atomic_read(&alsa_status) == ALSA_OFF)
				{
					ret = dsp_power_on();

					if (ret<0)
					{
						printk("dsp power can't open\n");
						return RET_ERROR;
					}
					atomic_set(&alsa_status, ALSA_ON);
				}
				else
					printk("dsp alread reset on\n");
			}
			break;
		case DSP_ALSA_IO_READY:
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

				dsp_clk = clk_get(NULL, "p_cclk_dsp");
				dsp_clk_rate = (unsigned int)clk_get_rate(dsp_clk);

#if CHECK_DSP_DEBUG
				printk("dsp_clk_rate = %d\n", dsp_clk_rate);
#endif
				dsp_clk_rate /= 1000000;
				dsp_clk = clk_get(NULL, "aclk_dsp");
				axi_ahb_clk = (unsigned int)clk_get_rate(dsp_clk);

#if CHECK_DSP_DEBUG
				printk("axi_ahb_clk = %d\n", axi_ahb_clk);
#endif
				axi_ahb_clk /= 1000000;
				dsp_clk = clk_get(NULL, "pclk_dsp");
				sync_apb_clk = (unsigned int)clk_get_rate(dsp_clk);

#if CHECK_DSP_DEBUG
				printk("sync_apb_clk = %d\n", sync_apb_clk);
#endif
				sync_apb_clk /= 1000000;
				dsp_clk = clk_get(NULL, "p_pclk_async");
				async_apb_clk = (unsigned int)clk_get_rate(dsp_clk);
#if CHECK_DSP_DEBUG
				printk("async_apb_clk = %d\n", async_apb_clk);
#endif
				async_apb_clk /= 1000000;

				transaction_number = hal_set_command(&send_data_register, READY, REQUEST);

				hal_set_parameter(8,
						(void *)&send_data_register,
						READY,
						0x1,
						0x1,
						0x1,
						0x1,
						pst_dsp_mem_cfg->sys_table_addr,
						pst_dsp_mem_cfg->sys_table_size);
#if 0
				hal_set_parameter(8,
						(void *)&send_data_register,
						READY,
						dsp_clk_rate,
						axi_ahb_clk,
						sync_apb_clk,
						async_apb_clk,
						pst_dsp_mem_cfg->sys_table_addr,
						pst_dsp_mem_cfg->sys_table_size);
#endif
				hal_send_mailbox(CH0_REQ_RES, &send_data_register);

#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if RECEIVE_RES_MAILBOX
#if CHECK_DSP_DEBUG
				printk("wait READY RES command\n");
#endif
				if (!wait_event_interruptible_timeout(event_alsa->wait,
							event_alsa->command == READY,HZ))
				{
					printk("Can't received READY in timeout\n");
					return RET_ERROR;
				}
#if CHECK_DSP_DEBUG
				printk("receive READY RES command\n");
#endif
#endif				
				atomic_set(&alsa_status, ALSA_READY);

				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number = hal_set_command(&send_data_register, CREATE, REQUEST);

				hal_set_parameter(4, &send_data_register, CREATE, PLAYBACK_CAPTURE, ALSA);

				hal_send_mailbox(CH0_REQ_RES, &send_data_register);

#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif

#if RECEIVE_RES_MAILBOX
#if CHECK_DSP_DEBUG
				printk("wait CREATE RES command\n");
#endif

				if (!wait_event_interruptible_timeout(event_alsa->wait,
							event_alsa->command == CREATE, HZ))
				{
					printk("Can't received CREATE in timeout\n");
					return RET_ERROR;
				}

#if CHECK_DSP_DEBUG
				printk("receive CREATE RES command\n");
#endif
#endif				
				atomic_set(&alsa_status, ALSA_CREATE);
			}
			break;
#if	!defined(WITH_BINARY)
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
		case DSP_ALSA_PCM_WRITE :
			{
				BUFFER_INFO buffer_info;
#if CHECK_DSP_DEBUG
				printk("DSP_ALSA_PCM_WRITE\n");
#endif

				if (copy_from_user(&buffer_info, (void __user *)arg, sizeof(BUFFER_INFO)))
				{
					printk("Can't recieve buffer info\n");
					return RET_ERROR;
				}
			}
	}

	return 0;
}

static int dsp_alsa_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;

	vma->vm_flags |= VM_IO;
	vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_page_prot =
		phys_mem_access_prot(filp, vma->vm_pgoff, size, vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start,
				vma->vm_pgoff, size, vma->vm_page_prot))
	{
		return -EAGAIN;
	}
	
	return RET_OK;
}

void dsp_alsa_pcm_write(unsigned int buffer,
		unsigned int size,
		unsigned int count,
		void (*callback_func)(unsigned long callback_param),
		unsigned long *run_info)
{
	MAILBOX_DATAREGISTER send_data_register;
	unsigned int transaction_number;

	void __iomem	*aud_crg_base;
#if 0
	printk("%s \n", __func__);
#endif
	
	if (atomic_read(&alsa_status) == ALSA_CAPTURE)
		dsp_alsa_pcm_stop();

#if CHECK_DSP_DEBUG
	printk("phys_addr = 0x%08X\n", buffer);
	printk("Totla size = 0x%08X\n", size);
	printk("NTF Size = 0x%08X\n", count);
#endif				

	alsa_run_info.buffer = buffer;
	alsa_run_info.size = size;
	alsa_run_info.count = count;
	alsa_run_info.callback_func = callback_func;
	alsa_run_info.callback_param = run_info;
	alsa_run_info.ops = PCM_PLAYBACK;


#if 1
	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number = hal_set_command(&send_data_register, PLAYBACK, REQUEST);
	hal_set_parameter(7,
			&send_data_register,
			PLAYBACK,
			buffer,
			size,
			count,
			START);
	hal_send_mailbox(CH0_REQ_RES, &send_data_register);

#if RECEIVE_SEND2
	
	wait_event_interruptible(dsp_wait->wait,
			received_transaction_number==transaction_number);
#endif /* #if RECEIVE_SEND2 */
#endif

	atomic_set(&alsa_status, ALSA_PLAY);

#if CHECK_DSP_DEBUG
	printk("Received PLAYBACK START RES\n");
#endif				
}

void dsp_alsa_pcm_read(unsigned int buffer,
		unsigned int size,
		unsigned int count,
		void (*callback_func)(unsigned long callback_param),
		unsigned long *run_info)
{

	MAILBOX_DATAREGISTER send_data_register;
	unsigned int transaction_number;

	if (atomic_read(&alsa_status) == ALSA_PLAY)
		dsp_alsa_pcm_stop();

#if CHECK_DSP_DEBUG
	printk("%s \n", __func__);
	
	printk("phys_addr = 0x%08X\n", buffer);
	printk("Totla size = 0x%08X\n", size);
	printk("NTF Size = 0x%08X\n", count);
#endif				

	alsa_run_info.buffer = buffer;
	alsa_run_info.size = size;
	alsa_run_info.count = count;
	alsa_run_info.callback_func = callback_func;
	alsa_run_info.callback_param = run_info;
	alsa_run_info.ops = PCM_CAPTURE;

#if 1
	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number = hal_set_command(&send_data_register, CAPTURE, REQUEST);
	hal_set_parameter(7,
			&send_data_register,
			CAPTURE,
			buffer,
			size,
			count,
			START);
	hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
	wait_event_interruptible(dsp_wait->wait,
			received_transaction_number==transaction_number);
#endif /* #if RECEIVE_SEND2 */
#endif

	atomic_set(&alsa_status, ALSA_CAPTURE);

#if CHECK_DSP_DEBUG
	printk("Received CAPTURE START REQ\n");
#endif
}

void dsp_alsa_pcm_stop(void)
{

	unsigned int transaction_number;
	MAILBOX_DATAREGISTER send_data_register;
	unsigned int operation = PLAYBACK;
	void __iomem	*aud_crg_base;

#if 0
	printk("%s \n", __func__);
#endif

#if 0
	aud_crg_base = ioremap(0x34670000, 0x100);

	__raw_writel(0x0, aud_crg_base + 0x40);	
	__raw_writel(0x0, aud_crg_base + 0x4C);	
	__raw_writel(0x0, aud_crg_base + 0x58);	
	__raw_writel(0x0, aud_crg_base + 0x64);	
	__raw_writel(0x0, aud_crg_base + 0x70);	
	__raw_writel(0x0, aud_crg_base + 0x7C);	

	iounmap(aud_crg_base);
#endif

	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));

	if (atomic_read(&alsa_status)==ALSA_PLAY)
	{
		operation = PLAYBACK;
	}
	else if (atomic_read(&alsa_status)==ALSA_CAPTURE)
	{
		operation = CAPTURE;
	}
	transaction_number = hal_set_command(&send_data_register, operation, REQUEST);
	hal_set_parameter(7,
			&send_data_register,
			operation,
			alsa_run_info.buffer,
			alsa_run_info.size,
			alsa_run_info.count,
			STOP);

	hal_send_mailbox(CH0_REQ_RES, &send_data_register);

#if RECEIVE_SEND2
	wait_event_interruptible(dsp_wait->wait,
			received_transaction_number==transaction_number);
#endif /* #if RECEIVE_SEND2 */

	atomic_set(&alsa_status, ALSA_READY);

#if CHECK_DSP_DEBUG
	printk("wait CAPTURE STOP REQ command\n");
#endif
}

static void workqueue_alsa(struct work_struct *unused)
{
	MAILBOX_DATAREGISTER received_data_register;

#if CHECK_DSP_DEBUG
	printk("Entered alsa workqueue\n");
	printk("alsa_workqueue_flag = %d\n", workqueue_flag.alsa_workqueue_flag);
#endif

	if (mutex_lock_interruptible(&mutex))
		return -ERESTARTSYS;

	memcpy(&received_data_register,
			pst_received_data_register,
			sizeof(MAILBOX_DATAREGISTER));

	if (workqueue_flag.alsa_workqueue_flag == -1)
	{
		spin_lock(&workqueue_flag.flag_lock);
		workqueue_flag.alsa_workqueue_flag = 0;
		spin_unlock(&workqueue_flag.flag_lock);
	}

	/* if REQ/RES mailbox(channel 0) */
	if (workqueue_flag.alsa_workqueue_flag == 1)
	{
		MAILBOX_DATA_REGISTER0_CHANNEL0 data_register0;

		data_register0 = (MAILBOX_DATA_REGISTER0_CHANNEL0)
			(received_data_register.data_register[0]);

		event_alsa->command = data_register0.field.command;
		event_alsa->type = data_register0.field.type;

		/* If RES Mailbox */
		if (event_alsa->type == RESPONSE)
		{
			if ((event_alsa->command == PLAYBACK)||
					(event_alsa->command == CAPTURE))
			{
#if CHECK_DSP_DEBUG
				printk("recieved  PLAYBACK/CAPTURE RES command\n");
#endif
				MAILBOX_DATA_REGISTER4_CHANNEL0 data_register4;
				data_register4 = (MAILBOX_DATA_REGISTER4_CHANNEL0)
					(received_data_register.data_register[4]);

				event_alsa->operation = data_register4.value;
			}

			wake_up_interruptible(&event_alsa->wait);

		}
		/* If REQ Mailbox */
		else if (event_alsa->type == REQUEST)
		{

		}

		spin_lock(&workqueue_flag.flag_lock);
		workqueue_flag.alsa_workqueue_flag = 0;
		spin_unlock(&workqueue_flag.flag_lock);
		mutex_unlock(&mutex);
		return;
	}
	/* if IND mailbox(channel 1) */
	else if (workqueue_flag.alsa_workqueue_flag == 2)
	{

	}
	/* if NTF mailbox(channel 2 or channel 7) */
	else if (workqueue_flag.alsa_workqueue_flag == 3)
	{
		MAILBOX_DATA_REGISTER0_CHANNEL2	data_register0;

		data_register0 = (MAILBOX_DATA_REGISTER0_CHANNEL2)
			(received_data_register.data_register[0]);

		event_alsa->command = data_register0.field.command;

		if ((atomic_read(&alsa_status) == ALSA_PLAY)||
				(atomic_read(&alsa_status)==ALSA_CAPTURE))
		{
			alsa_run_info.callback_func(alsa_run_info.callback_param);
		}

		spin_lock(&workqueue_flag.flag_lock);
		workqueue_flag.alsa_workqueue_flag = 0;
		spin_unlock(&workqueue_flag.flag_lock);
		mutex_unlock(&mutex);
		return;
	}

	spin_lock(&workqueue_flag.flag_lock);
	workqueue_flag.alsa_workqueue_flag = 0;
	spin_unlock(&workqueue_flag.flag_lock);
	mutex_unlock(&mutex);
	return;
}

void dsp_kerneltimer(unsigned long arg)
{
	mod_timer(timer, get_jiffies_64()+HZ);
}

struct file_operations gst_dsp_alsa_fops =
{
	.owner = THIS_MODULE,
	.open 	= dsp_alsa_open,
	.release= dsp_alsa_close,
	.unlocked_ioctl	= dsp_alsa_ioctl,
	.mmap = dsp_alsa_mmap,
};

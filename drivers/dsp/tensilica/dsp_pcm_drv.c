#include <linux/module.h>
#include <linux/cdev.h>
#include <asm/io.h>			/**< For ioremap_nocache */
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/sched.h>
#include "dsp_drv.h"
#include "dsp_pcm_drv.h"
#include "mailbox/mailbox_hal.h"
#include "dsp_ioctl_cmd.h"
	
#if defined(CONFIG_WODEN_DSP)
#include "woden/dsp_coredrv_woden.h"
#elif defined(CONFIG_ODIN_DSP)
#include "odin/dsp_coredrv_odin.h"
#endif

#define	CHECK_DSP_DEBUG	1
	
wait_queue_head_t pcm_poll_waitqueue;
EVENT_PCM *event_pcm;

static void workqueue_pcm(struct work_struct *unused);
struct workqueue_struct *pcm_workqueue;
DECLARE_WORK(pcm_work, workqueue_pcm);
struct work_struct pcm_work;

AUDIO_INFO pcm_info;

static int
dsp_pcm_open(struct inode *inode, struct file *filp)
{
	struct cdev *cdev;
	DSP_DEVICE_T *dsp_dev;
	
#if	CHECK_DSP_DEBUG
	printk("call woden_dsp_pcm open start\n");
#endif
	if (!dsp_mode->is_opened_lpa)
	{
		printk("lpa driver is not opened\n");

		return RET_ERROR;
	}

	cdev = inode->i_cdev;
	dsp_dev = container_of(cdev, DSP_DEVICE_T, cdev);

	if (dsp_dev->dev_open_count == 0)
	{
		dsp_dev->is_initialized = 0;
	}
	
	
	event_pcm = kmalloc(sizeof(EVENT_PCM),GFP_KERNEL);
	if (event_pcm == NULL)
	{
		printk("Can't allocate kernel memory\n");
		return RET_OUT_OF_MEMORY;
	}
	memset(event_pcm, 0, sizeof(EVENT_PCM));
	init_waitqueue_head(&pcm_poll_waitqueue);

	pcm_workqueue = create_workqueue("pcm_work");
	if (pcm_workqueue == NULL)
		return -ENOMEM;

#if	CHECK_DSP_DEBUG
	printk("call woden_dsp_pcm open finish\n");
#endif
	dsp_mode->is_opened_pcm++;
	
	return RET_OK;
}

static int
dsp_pcm_close(struct inode *inode, struct file *filp)
{
	struct cdev *cdev;
	DSP_DEVICE_T *dsp_dev;

#if	CHECK_DSP_DEBUG
	printk("call woden_dsp_pcm close start : %s\n", __FILE__);
#endif	
	cdev = inode->i_cdev;
	dsp_dev = container_of(cdev, DSP_DEVICE_T, cdev);

#if 0
	atomic_set(&workqueue_flag.pcm_workqueue_flag, -1);
#endif
	workqueue_flag.pcm_workqueue_flag= -1;

	wake_up(&pcm_poll_waitqueue);

	if (pcm_workqueue)
	{
		destroy_workqueue(pcm_workqueue);
		pcm_workqueue = NULL;
	}
	kfree(event_pcm);

	if (dsp_dev->dev_open_count > 0)
	{
		--dsp_dev->dev_open_count;
	}

	dsp_mode->is_opened_pcm--;


#if	CHECK_DSP_DEBUG
	printk("call woden_dsp_pcm close finish : %s\n", __FILE__);
#endif	
	return RET_OK;
}

static long
dsp_pcm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg )
{
	unsigned int transaction_number;

	switch (cmd)
	{
		case DSP_LPA_IOW_INIT:
		{
#if 0
			unsigned int stereo;
			unsigned int woofer;
#endif
#if CHECK_DSP_DEBUG
			printk("ioctl(DSP_LPA_IOW_INIT) called : %s\n", __FILE__);
#endif

			memset(&pcm_info, 0, sizeof(AUDIO_INFO));

			if (copy_from_user(&pcm_info, (void __user *)arg, sizeof(AUDIO_INFO)))
			{
				printk("Can't recieve audio_info\n");
				return RET_ERROR;
			}
		}
		break;
		case DSP_LPA_IOW_PLAY :
		{
			MAILBOX_DATAREGISTER send_data_register;
			BUFFER_INFO buffer_info;
			int alarm_volume;

#if CHECK_DSP_DEBUG
			printk("ioctl(DSP_LPA_IOW_PLAY) called : %s\n", __FILE__);
#endif

			if (copy_from_user(&buffer_info, (void __user *)arg, sizeof(BUFFER_INFO)))
			{
				printk("Can't recieve buffer_info\n");
				return RET_ERROR;
			}
#if CHECK_DSP_DEBUG
				printk("phys_addr = 0x%08X\n", buffer_info.buffer);
				printk("size = 0x%08X\n", buffer_info.size);
				printk("eos_flag = 0x%08X\n", buffer_info.eos_flag);
#endif				

				/* 알람 볼륨은 일단 fix */
				alarm_volume = 0x14;

				memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
				transaction_number =
					hal_set_command(&send_data_register,
							CONTROL_ALARM,
							REQUEST);

				hal_set_parameter(5,
						&send_data_register,
						CONTROL_ALARM,
						buffer_info.buffer,
						buffer_info.size,
						(unsigned int)alarm_volume);

				hal_send_mailbox(CH0_REQ_RES, &send_data_register);
#if RECEIVE_SEND2
				wait_event_interruptible(dsp_wait->wait,
						received_transaction_number==transaction_number);
#endif
		}
		break;
	}
	return 0;
}

static int
dsp_pcm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;

	vma->vm_flags |= VM_IO;
	vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_page_prot =
		phys_mem_access_prot(filp, vma->vm_pgoff,
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
dsp_pcm_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	
	poll_wait(filp, &pcm_poll_waitqueue, wait);

	if (event_pcm->pcm_readqueue_count)
	{	
#if CHECK_DSP_DEBUG
		printk("poll event wakeup : %s\n", __FUNCTION__);
#endif
		event_pcm->pcm_readqueue_count = 0;
		mask |= POLLIN | POLLRDNORM;
	}
	
	return mask;
}

static void
workqueue_pcm(struct work_struct *unused)
{
	MAILBOX_DATAREGISTER received_data_register;

	memcpy(&received_data_register,
			pst_received_data_register,
			sizeof(MAILBOX_DATAREGISTER));

#if CHECK_DSP_DEBUG
		printk("Entered pcm workqueue\n");
#endif

		if (workqueue_flag.pcm_workqueue_flag == -1)
		{
#if 0
			atomic_set(&workqueue_flag.pcm_workqueue_flag, 0);
#endif
			workqueue_flag.pcm_workqueue_flag = 0;

		}
		/* if REQ/RES mailbox(channel 0) */
		if (workqueue_flag.pcm_workqueue_flag == 1)
		{
			MAILBOX_DATA_REGISTER0_CHANNEL0 data_register0;

			data_register0 = (MAILBOX_DATA_REGISTER0_CHANNEL0)
				(received_data_register.data_register[0]);
			event_pcm->command = data_register0.field.command;
			event_pcm->type = data_register0.field.type;

			if (event_pcm->type == RESPONSE)
			{
				if (event_pcm->command == CONTROL_ALARM)
				{
#if CHECK_DSP_DEBUG
						printk("Received CONTROL_ALARM RES\n");
#endif
						event_pcm->pcm_readqueue_count = 1;
						wake_up(&pcm_poll_waitqueue);
				}
			}
		}
		workqueue_flag.pcm_workqueue_flag = 0;

#if CHECK_DSP_DEBUG
	printk("kthread_pcm closed\n");
#endif
}

ssize_t
dsp_pcm_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	unsigned long flags;

	local_save_flags(flags);
	local_irq_disable();

		
	put_user((char)event_pcm->command, (char *)buf);
#if 0
	if (event_lpa->lpa_readqueue_count)
	{	
		put_user((char)event_lpa->command, (char *)buf);

	}
#endif
	local_irq_restore(flags);

	return event_pcm->pcm_readqueue_count;
}

struct
file_operations gst_dsp_pcm_fops =
{
	.owner = THIS_MODULE,
	.open 	= dsp_pcm_open,
	.release= dsp_pcm_close,
	.unlocked_ioctl	= dsp_pcm_ioctl,
	.poll = dsp_pcm_poll,
	.mmap = dsp_pcm_mmap,
	.read = dsp_pcm_read,
};

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <asm/io.h>			/**< For ioremap_nocache */
#include <linux/poll.h>
#include "dsp_drv.h"
#include "dsp_omx_drv.h"
#include "mailbox/mailbox_hal.h"
#include "dsp_ioctl_cmd.h"

#if defined(CONFIG_WODEN_DSP)
#include "woden/dsp_coredrv_woden.h"
#elif defined(CONFIG_ODIN_DSP)
#include "odin/dsp_coredrv_odin.h"
#endif

#ifndef CONFIG_OF
#include <mach/irqs.h>
#endif

#define	CHECK_DSP_DEBUG	0

OMX_AUDIO_INFO omx_audio_info;

unsigned int old_cpb_status = CPB2_ACTIVE;
unsigned int new_dpb_status = DPB1_ACTIVE;

int dsp_omx_check_changed_info(void);

unsigned int *cpb1_vir_addr = NULL;
unsigned int *cpb2_vir_addr = NULL;
unsigned int *dpb1_vir_addr = NULL;
unsigned int *dpb2_vir_addr = NULL;

static void workqueue_omx(struct work_struct *unused);
struct workqueue_struct *omx_workqueue;
DECLARE_WORK(omx_work, workqueue_omx);
struct work_struct omx_work;

atomic_t received_mailbox_flag = ATOMIC_INIT(0);

static int
dsp_omx_open(struct inode *inode, struct file *filp)
{
	struct cdev *cdev;
	DSP_DEVICE_T *dsp_dev;

#if CHECK_DSP_DEBUG
	printk("call woden_omx_dsp open start\n");
#endif

	cdev = inode->i_cdev;
	dsp_dev = container_of(cdev, DSP_DEVICE_T, cdev);

	if (dsp_dev->dev_open_count == 0)
	{
		dsp_dev->is_initialized = 0;
	}

	omx_workqueue = create_workqueue("omx_work");
	if (omx_workqueue == NULL)
		return -ENOMEM;

#if 0
	/* if OMX is included in usebuffer, write this code.*/
	cpb1_vir_addr =
		ioremap_nocache(dsp_omx_mem_cfg.cpb1_address,
				dsp_omx_mem_cfg.cpb1_size);
	cpb2_vir_addr =
		ioremap_nocache(dsp_omx_mem_cfg.cpb2_address,
				dsp_omx_mem_cfg.cpb2_size);
	dpb1_vir_addr =
		ioremap_nocache(dsp_omx_mem_cfg.dpb1_address,
				dsp_omx_mem_cfg.dpb1_size);
	dpb2_vir_addr =
		ioremap_nocache(dsp_omx_mem_cfg.dpb2_address,
				dsp_omx_mem_cfg.dpb2_size);
#else
	cpb1_vir_addr =
		ioremap_nocache(pst_dsp_mem_cfg->cpb1_address,
				pst_dsp_mem_cfg->cpb1_size);
	cpb2_vir_addr =
		ioremap_nocache(pst_dsp_mem_cfg->cpb2_address,
				pst_dsp_mem_cfg->cpb2_size);
	dpb1_vir_addr =
		ioremap_nocache(pst_dsp_mem_cfg->dpb1_address,
				pst_dsp_mem_cfg->dpb1_size);
	dpb2_vir_addr =
		ioremap_nocache(pst_dsp_mem_cfg->dpb2_address,
				pst_dsp_mem_cfg->dpb2_size);

#endif

	dsp_mode->is_opened_omx++;

	return 0;
}

static int
dsp_omx_close(struct inode *inode, struct file *filp)
{
	struct cdev *cdev;
	DSP_DEVICE_T *dsp_dev;

#if 1
	iounmap(cpb1_vir_addr);
	iounmap(cpb2_vir_addr);
	iounmap(dpb1_vir_addr);
	iounmap(dpb2_vir_addr);
#endif

	if (omx_workqueue)
	{
		destroy_workqueue(omx_workqueue);
		omx_workqueue = NULL;
	}

	cdev = inode->i_cdev;
	dsp_dev = container_of(cdev, DSP_DEVICE_T, cdev);

	if (dsp_dev->dev_open_count > 0)
	{
		--dsp_dev->dev_open_count;
	}

	dsp_mode->is_opened_omx--;

	return 0;
}

static long
dsp_omx_ioctl(struct file *filp, unsigned int cmd, unsigned long arg )
{

	int ret;
	switch (cmd)
	{
		case DSP_IO_INIT_MODULE:
			{
#if CHECK_DSP_DEBUG
				printk("DSP_IO_POWER_ON\n");
#endif
				ret = dsp_power_on();
				if (ret<0)
				{
					printk("dsp power can't open\n");
					return RET_ERROR;
				}
			}
			break;

		case DSP_IOW_OPEN:
			{
				int send_mailbox_number;
				MAILBOX_DATA_REGISTER4_OMX data_register4;
#if 0
				unsigned int sampling_frequency;
				unsigned int bit_for_sample;
				unsigned int bit_rate;
				unsigned int codec_type;
#endif

#if CHECK_DSP_DEBUG
				printk("DSP_IOW_OPEN\n");
#endif
				printk("Audio OMX HW Start\n");

				if (copy_from_user(&omx_audio_info,
							(void __user *)arg,
							sizeof(AUDIO_INFO)))
				{
					printk("Can't recieve clip start info\n");
					return RET_ERROR;
				}

				memset(pst_mailbox_data_register,0,sizeof(MAILBOX_DATAREGISTER));

				hal_write_data_register_for_audio_manager(pst_mailbox_data_register,
						OMX_DECODE,
						OMX_OPEN,
						REQUEST);

#if CHECK_DSP_DEBUG
				printk("omx_audio_info.codec_type = 0x%08X\n", omx_audio_info.codec_type);
				printk("omx_audio_info.bit_rate = 0x%08X\n", omx_audio_info.bit_rate);
				printk("omx_audio_info.bit_per_sample = 0x%08X\n",
						omx_audio_info.bit_per_sample);
				printk("omx_audio_info.sampling_freq = 0x%08X\n",
						omx_audio_info.sampling_freq);
#endif

#if 1
				switch (omx_audio_info.codec_type)
				{
					case MP3 : omx_audio_info.codec_type = 0x1; break;
					case DTS : omx_audio_info.codec_type = 0x4; break;
				}
				switch (omx_audio_info.bit_rate)
				{
					case KBPS_48 : omx_audio_info.bit_rate = 0x1; break;
					case KBPS_64 : omx_audio_info.bit_rate = 0x2; break;
					case KBPS_96 : omx_audio_info.bit_rate = 0x3; break;
					case KBPS_128 : omx_audio_info.bit_rate = 0x4; break;
					case KBPS_196 : omx_audio_info.bit_rate = 0x5; break;
					case KBPS_320 : omx_audio_info.bit_rate = 0x6; break;
					case KBPS_384 : omx_audio_info.bit_rate = 0x7; break;
				}
				switch (omx_audio_info.bit_per_sample)
				{
					case BIT_8 : omx_audio_info.bit_per_sample = 0x1; break;
					case BIT_10 : omx_audio_info.bit_per_sample = 0x2; break;
					case BIT_12 : omx_audio_info.bit_per_sample = 0x3; break;
					case BIT_16 : omx_audio_info.bit_per_sample = 0x4; break;
					case BIT_24 : omx_audio_info.bit_per_sample = 0x5; break;
					case BIT_32 : omx_audio_info.bit_per_sample = 0x6; break;
					case BIT_64 : omx_audio_info.bit_per_sample = 0x7; break;
				}
				switch (omx_audio_info.sampling_freq)
				{
					case KHZ_4 : omx_audio_info.sampling_freq = 0x1;break;
					case KHZ_8 : omx_audio_info.sampling_freq = 0x2;break;
					case KHZ_11025 : omx_audio_info.sampling_freq = 0x3;break;
					case KHZ_12 : omx_audio_info.sampling_freq = 0x4;break;
					case KHZ_16 : omx_audio_info.sampling_freq = 0x5;break;
					case KHZ_2205 : omx_audio_info.sampling_freq = 0x6;break;
					case KHZ_24 : omx_audio_info.sampling_freq = 0x7;break;
					case KHZ_32 : omx_audio_info.sampling_freq = 0x8;break;
					case KHZ_441 : omx_audio_info.sampling_freq = 0x9;break;
					case KHZ_48 : omx_audio_info.sampling_freq = 0xa;break;
					case KHZ_64 : omx_audio_info.sampling_freq = 0xb;break;
					case KHZ_882 : omx_audio_info.sampling_freq = 0xc;break;
					case KHZ_96 : omx_audio_info.sampling_freq = 0xd;break;
					case KHZ_128 : omx_audio_info.sampling_freq = 0xe;break;
					case KHZ_176 : omx_audio_info.sampling_freq = 0xf;break;
					case KHZ_192 : omx_audio_info.sampling_freq = 0x10;break;
					case KHZ_768 : omx_audio_info.sampling_freq = 0x11;break;
				}
#endif

				memset(&data_register4, 0, sizeof(MAILBOX_DATA_REGISTER4_OMX));
				data_register4.field.codec_type = omx_audio_info.codec_type;
				data_register4.field.bit_rate = omx_audio_info.bit_rate;
				data_register4.field.channel_number = omx_audio_info.channel_number;
				data_register4.field.bit_per_sample = omx_audio_info.bit_per_sample;
				data_register4.field.sampling_frequency =
					omx_audio_info.sampling_freq;

				pst_mailbox_data_register->data_register[4]= hal_write_data_register4(
						data_register4.field.codec_type,
						data_register4.field.bit_rate,
						data_register4.field.channel_number,
						data_register4.field.bit_per_sample,
						data_register4.field.sampling_frequency);

#if 1
				send_mailbox_number = hal_find_send_mailbox_number();
#else
				send_mailbox_number = 0;
#endif

				hal_send_to_dsp(&(pst_audio_mailbox->mailbox[send_mailbox_number]),
						pst_mailbox_data_register);
#if CHECK_DSP_DEBUG
				printk("Sended OPEN Command\n");
#endif
				wait_event_interruptible(dsp_wait->wait,
						atomic_read(&received_mailbox_flag)==1);
				atomic_set(&received_mailbox_flag,0);

#if CHECK_DSP_DEBUG
				printk("Recieved OPEN Command!\n");
#endif

			}
			break;

		case DSP_IO_INIT:
			{
#if CHECK_DSP_DEBUG
				printk("DSP_IO_INIT\n");
#endif
				memset(pst_mailbox_data_register,0,sizeof(MAILBOX_DATAREGISTER));

				hal_write_data_register_for_audio_manager(pst_mailbox_data_register,
						OMX_DECODE,
						OMX_INIT,
						REQUEST);

				hal_send_to_dsp(&(pst_audio_mailbox->mailbox[0]),
						pst_mailbox_data_register);
#if CHECK_DSP_DEBUG
				printk("Sended INIT Command\n");
#endif
				wait_event_interruptible(dsp_wait->wait,
						atomic_read(&received_mailbox_flag)==1);
				atomic_set(&received_mailbox_flag,0);

#if CHECK_DSP_DEBUG
				printk("Recieved INIT Command!\n");
#endif

			}
			break;
		case DSP_IOW_DECODE_READY:
			{
				DSP_BUFFER input_buffer;

#if CHECK_DSP_DEBUG
				printk("Call DSP_Woden_DecodeReady\n");
#endif

				if (copy_from_user(&input_buffer, (void __user *)arg, sizeof(DSP_BUFFER)))
				{
					printk("Can't recieve clip Mem info\n");
					return -1;
				}
				
#if CHECK_DSP_DEBUG
				printk("input_buffer address = 0x%08X\n", input_buffer);
#endif
				memset(pst_mailbox_data_register,0,sizeof(MAILBOX_DATAREGISTER));

				if (old_cpb_status == CPB1_ACTIVE)
				{
					memcpy(cpb2_vir_addr,
							input_buffer.buffer_virtual_address,
							input_buffer.buffer_size);

					hal_write_data_register_for_cdaif(pst_mailbox_data_register,
							CPB2_ACTIVE,REQUEST,
							pst_dsp_mem_cfg->cpb2_address,
							input_buffer.buffer_size);

					old_cpb_status = CPB2_ACTIVE;
					new_dpb_status = DPB2_ACTIVE;
				}
				else
				{
					memcpy(cpb1_vir_addr,
							input_buffer.buffer_virtual_address,
							input_buffer.buffer_size);
					
					hal_write_data_register_for_cdaif(pst_mailbox_data_register,
							CPB1_ACTIVE,
							REQUEST,
							pst_dsp_mem_cfg->cpb1_address,input_buffer.buffer_size);

					old_cpb_status = CPB1_ACTIVE;
					new_dpb_status = DPB1_ACTIVE;
				}

#if CHECK_DSP_DEBUG
				printk("input_buffer virtual address = 0x%08X\n",
						input_buffer.buffer_virtual_address);
				printk("input_buffer size = 0x%08X\n", input_buffer.buffer_size);
#endif

				hal_send_to_dsp(&(pst_audio_mailbox->mailbox[0]),
						pst_mailbox_data_register);
#if CHECK_DSP_DEBUG
				printk("Sended CDAIF CPB_ACTIVE Command\n");
#endif
				wait_event_interruptible(dsp_wait->wait,
						atomic_read(&received_mailbox_flag)==1);
				atomic_set(&received_mailbox_flag,0);

#if CHECK_DSP_DEBUG
				printk("Recieved CDAIF CPB_ACTIVE Command!\n");
#endif
			}
			break;
		case DSP_IO_DECODE:
			{
				int ret = 0;
#if CHECK_DSP_DEBUG
				printk("Call DSP_Woden_Decode\n");
#endif
				memset(pst_mailbox_data_register,0,sizeof(MAILBOX_DATAREGISTER));
				hal_write_data_register_for_audio_manager(pst_mailbox_data_register,
						OMX_DECODE,
						OMX_EXECUTE,
						REQUEST);

#if CHECK_DSP_DEBUG
				printk("Sended EXECUTE Command\n");
#endif
				hal_send_to_dsp(&(pst_audio_mailbox->mailbox[0]),
						pst_mailbox_data_register);
				wait_event_interruptible(dsp_wait->wait,
						atomic_read(&received_mailbox_flag)==1);
				atomic_set(&received_mailbox_flag,0);

#if CHECK_DSP_DEBUG
				printk("Recieved EXCUTE Command!\n");
#endif
				ret = dsp_omx_check_changed_info();
				return ret;
			}
			break;

		case DSP_IOR_DECODING_INFO:
			{
				DSP_BUFFER pcm_buffer;

#if CHECK_DSP_DEBUG
				printk("call DSP_Woden_Getpcm_buffer\n");
#endif
				memset(pst_mailbox_data_register,0,sizeof(MAILBOX_DATAREGISTER));

				hal_write_data_register_for_cdaif(pst_mailbox_data_register,
						new_dpb_status,
						REQUEST,
						0,
						0);

				hal_send_to_dsp(&(pst_audio_mailbox->mailbox[0]),
						pst_mailbox_data_register);
#if CHECK_DSP_DEBUG
				printk("Sended CDAIF DPB_ACTIVE Command\n");
#endif
				wait_event_interruptible(dsp_wait->wait,
						atomic_read(&received_mailbox_flag)==1);
				atomic_set(&received_mailbox_flag,0);

#if CHECK_DSP_DEBUG
				printk("Recieved CDAIF DPB_ACTIVE Command!\n");
#endif

				if (new_dpb_status == DPB1_ACTIVE)
				{
					pcm_buffer.buffer_virtual_address = (unsigned char *)dpb1_vir_addr;
				}
				else
				{
					pcm_buffer.buffer_virtual_address = (unsigned char *)dpb2_vir_addr;
				}

				pcm_buffer.buffer_size = pst_received_data_register->data_register[6];

#if 1
				if (copy_to_user((void *)arg, (void *)&pcm_buffer, sizeof(DSP_BUFFER)))
				{
					return -EFAULT;
				}
#endif

				return 0;
			}
			break;
		case DSP_IOR_PCM_BUFFER:
			{
				if (new_dpb_status == DPB1_ACTIVE)
				{
					if (copy_to_user((void *)arg,
								(void *)dpb1_vir_addr,
								pst_received_data_register->data_register[6]))
					{
						return -EFAULT;
					}
				}
				else if (new_dpb_status == DPB2_ACTIVE)
				{
					if (copy_to_user((void *)arg,
								(void *)dpb2_vir_addr,
								pst_received_data_register->data_register[6]))
					{
						return -EFAULT;
					}
				}
				return 0;
			}
			break;
		case DSP_IOR_GET_INPUT_BUFFER1:
			{
				DSP_BUFFER input_buffer;
#if 0
				ret = DSP_DRV_GetInputBuffer1(&input_buffer);
#endif

				cpb1_vir_addr =
					ioremap_nocache(pst_dsp_mem_cfg->cpb1_address,
							pst_dsp_mem_cfg->cpb1_size);
				input_buffer.buffer_virtual_address = (unsigned char *)cpb1_vir_addr;

				input_buffer.buffer_size = pst_dsp_mem_cfg->cpb1_size;

				if (copy_to_user((void *)arg, (void *)&input_buffer, sizeof(DSP_BUFFER)))
					return -EFAULT;
			}
			break;
		case DSP_IOR_GET_INPUT_BUFFER2:
			{
				DSP_BUFFER input_buffer;
				
				input_buffer.buffer_virtual_address = (unsigned char *)cpb2_vir_addr;

				input_buffer.buffer_size = pst_dsp_mem_cfg->cpb2_size;

				if (copy_to_user((void *)arg, (void *)&input_buffer, sizeof(DSP_BUFFER)))
					return -EFAULT;
			}
			break;
		case DSP_IOR_GET_OUTPUT_BUFFER1:
			{
				DSP_BUFFER output_buffer;
				
				output_buffer.buffer_virtual_address = (unsigned char *)dpb1_vir_addr;
				
				output_buffer.buffer_size = pst_dsp_mem_cfg->dpb1_size;

				if (copy_to_user((void *)arg, (void *)&output_buffer, sizeof(DSP_BUFFER)))
					return -EFAULT;
			}
			break;
		case DSP_IOR_GET_OUTPUT_BUFFER2:
			{
				DSP_BUFFER output_buffer;
				
				dpb2_vir_addr =
					ioremap_nocache(pst_dsp_mem_cfg->dpb2_address,
							pst_dsp_mem_cfg->dpb2_size);

				output_buffer.buffer_virtual_address = (unsigned char *)dpb2_vir_addr;
				output_buffer.buffer_size = pst_dsp_mem_cfg->dpb2_size;

				if (copy_to_user((void *)arg, (void *)&output_buffer, sizeof(DSP_BUFFER)))
					return -EFAULT;
			}
			break;
#if !defined(WITH_BINARY)
		case DSP_IOW_FIRMWARE_DOWNLOAD_DATA:
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
		case DSP_IOW_FIRMWARE_DOWNLOAD_INST:
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
		case DSP_IOW_GET_AUDIO_INFOMATION:
			{
				AUDIO_INFO audio_info;

				memcpy(&audio_info, &omx_audio_info, sizeof(AUDIO_INFO));

				if (copy_to_user((void *)arg, &audio_info, sizeof(AUDIO_INFO)))
				{
					printk("Can't send Audio Information\n");
					return -EFAULT;
				}
			}
			break;
	}
	return 0;
}


int
dsp_omx_check_changed_info(void)
{
	unsigned int temp_data_register4, data_register4;
	
	temp_data_register4 = pst_mailbox_data_register->data_register[4];
	data_register4 = (pst_mailbox_data_register->data_register[4])>>20;

	if (data_register4 & 0x3)
	{
		omx_audio_info.sampling_freq= temp_data_register4 & 0x1f;
		temp_data_register4 = temp_data_register4 >> 9;
		omx_audio_info.channel_number = temp_data_register4 & 0xf;
		temp_data_register4 = temp_data_register4 >> 4;
		omx_audio_info.bit_rate = temp_data_register4 & 0x7;

#if CHECK_DSP_DEBUG
		printk("decoding information changed\n");
#endif
		return RET_CHANGED_INFORMATION;
	}
	else
	{
		return RET_NO_CHANGED_INFORMATION;
	}
	return RET_ERROR;
}

static void
workqueue_omx(struct work_struct *unused)
{
#if 0
	spinlock_t omx_thread_lock;
	int receivedMailboxNumber = 0;
#endif
	MAILBOX_DATAREGISTER received_data_register;
#if 0
	struct list_head *entry;
	MAILBOX_DATA_REGISTER_LIST *target;
#endif

	memcpy(&received_data_register,
			pst_received_data_register,
			sizeof(MAILBOX_DATAREGISTER));

#if CHECK_DSP_DEBUG
		printk("Entered omx workqueue\n");
#endif

#if 0
		for (j=0;j<NUMBER_OF_DATAREG;j++)
		{
			printk("DataRegister[%d]= 0x%08X\n",
					j, received_data_register.data_register.data_register[j]);
		}
#endif

		if (workqueue_flag.omx_workqueue_flag == -1)
			workqueue_flag.omx_workqueue_flag = 0;
		
		/* if REQ/RES mailbox(channel 0) */
		if (workqueue_flag.omx_workqueue_flag == 1)
		{
#if 0
			HalMailboxGetDataRegister(
					&pstReceivedMailbox->mailbox[receivedMailboxNumber],
					pst_mailbox_data_register);
#endif

#if 0
			list_for_each(entry, &pst_data_register_list->list) {
				target = list_entry(entry, MAILBOX_DATA_REGISTER_LIST, list);

				if (((received_data_register.data_register[1])>>16)==
						((target->data_register.data_register[1])>>16))
				{
					spin_lock(&pst_data_register_list->lock);
					list_del(entry);
					spin_unlock(&pst_data_register_list->lock);
					break;
				}
			}
#endif
			atomic_set(&received_mailbox_flag,1);
			wake_up_interruptible(&dsp_wait->wait);
		}
		workqueue_flag.omx_workqueue_flag = 0;
}

struct
file_operations gst_dsp_omx_fops =
{
	.owner = THIS_MODULE,
	.open 	= dsp_omx_open,
	.release= dsp_omx_close,
	.unlocked_ioctl	= dsp_omx_ioctl,
};

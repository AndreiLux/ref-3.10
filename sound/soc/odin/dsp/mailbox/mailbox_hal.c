#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <asm/types.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/sched.h>			/**< For isr */

#include "mailbox_hal.h"

AUD_MAILBOX *pst_audio_mailbox;
AUD_MAILBOX_INTERRUPT *pst_audio_mailbox_interrupt;

MAILBOX_DATAREGISTER *pst_mailbox_data_register;
MAILBOX_DATAREGISTER *pst_received_data_register;

MAILBOX_DATA_REGISTER_LIST *pst_data_register_list;

unsigned int transaction_number;

static DEFINE_MUTEX(mutex);

/* For LPA */
#if 1
unsigned int
hal_write_data_register0(unsigned int senderDeviceId,
		unsigned int receiverDeviceId,
		unsigned int senderObjectId,
		unsigned int receiverObjectId,
		unsigned int resourceId)
{
	unsigned int temp = 0;

	temp = (senderDeviceId<<28)|
		(receiverDeviceId<<24)|
		(senderObjectId<<16)|
		(receiverObjectId<<8)|
		(resourceId);

	return temp;
}

unsigned int
hal_write_data_register1_audio_manager(
		unsigned int transactionId,
		unsigned int taskGroup,
		unsigned int action,
		unsigned int type)
{
	unsigned int temp = 0;
	
	temp = (transactionId<<16)|
		(taskGroup << 9)|
		(action << 3)|
		(type);

	return temp;
}

unsigned int
hal_write_data_register1_cdaif(unsigned int transactionId,
		unsigned int status,
		unsigned int type)
{
	unsigned int temp = 0;
	
	temp = (transactionId<<16)|(status << 9)|(type);
	
	return temp;
}

unsigned int
hal_write_data_register4(unsigned int codec_type,
		unsigned int bit_rate,
		unsigned int channel_number,
		unsigned int bit_per_sample,
		unsigned int sampling_freq)
{
	unsigned int temp = 0;
	
	temp = (codec_type<<16)|
		(bit_rate<<13)|
		(channel_number<<9)|
		(bit_per_sample<<5)|
		(sampling_freq);
	
	return temp;

}
#endif


void
hal_write_data_register(AUD_MBOX_TypeDef *pMailbox,
		MAILBOX_DATAREGISTER *pst_mailbox_data_register)
{
	memcpy(&pMailbox->data_register,
			pst_mailbox_data_register,
			sizeof(MAILBOX_DATAREGISTER));
}

unsigned int
hal_get_transaction_number(void)
{
	return transaction_number;
}

void
hal_set_transaction_number(unsigned int number)
{
	transaction_number = number;
}

unsigned int
hal_set_command(MAILBOX_DATAREGISTER *data_register,
		unsigned int command,
		unsigned int type)
{
	unsigned int transaction_number;
	MAILBOX_DATA_REGISTER0_CHANNEL0 data_register0;
	
	transaction_number = hal_get_transaction_number();

	if (transaction_number != 0xffff)
	{
		transaction_number++;
	}
	else
	{
		transaction_number = 1;
	}

	hal_set_transaction_number(transaction_number);
	
	data_register0.value = 0;
	data_register0.field.command = command;
	data_register0.field.type = type;
	data_register0.field.transaction_id = transaction_number;

	data_register->data_register[0] = data_register0.value;

	return transaction_number;
}

MAILBOX_DATAREGISTER
hal_get_data_register(AUD_MBOX_TypeDef *pMailbox)
{
	MAILBOX_DATAREGISTER data_register;

	memcpy(&data_register,
			&pMailbox->data_register,
			sizeof(MAILBOX_DATAREGISTER));

	return data_register;
}

int
hal_send_mailbox(unsigned int channel_number,
		MAILBOX_DATAREGISTER *data_register)
{
	AUD_MBOX_TypeDef *p_mailbox;
	int i;

	if (mutex_lock_interruptible(&mutex))
		return -ERESTARTSYS;

	p_mailbox = &pst_audio_mailbox->mailbox[channel_number];

	for (i=0;i<10;i++)
	{
		if (!mailbox_get_source(p_mailbox))
			break;
		schedule_timeout_interruptible(5);
	}

	if (i==10)
	{
		pr_err("%s : AUD mailbox channel %d source register is not zero\n",
				__func__, channel_number);
	}

	mailbox_clear_source(p_mailbox);
	mailbox_clear_send(p_mailbox);
	mailbox_set_maskset(p_mailbox,0);

	mailbox_set_source(p_mailbox, (unsigned int)MAILBOX_ARM_ID_MASK);
	mailbox_set_maskset(p_mailbox,
			(unsigned int)(MAILBOX_ARM_ID_MASK|MAILBOX_DSP_ID_MASK));
	mailbox_set_destination(p_mailbox, (unsigned int)MAILBOX_DSP_ID_MASK);
	hal_write_data_register(p_mailbox, data_register);
	mailbox_start_send(p_mailbox);

	mutex_unlock(&mutex);

	return 0;
}


void
hal_set_parameter(int count, ...)
{	
	unsigned int command;	
	MAILBOX_DATAREGISTER *data_register;
	unsigned int param[10];
	va_list arglist;
	int i, cnt;

	va_start(arglist, count);
	data_register = (MAILBOX_DATAREGISTER *)va_arg(arglist, void*);

	command = va_arg(arglist, unsigned int);
	
	for (i=0;i<10;i++)
	{
		memset(param+i,0,sizeof(unsigned int));
	}

	for (cnt=2;cnt<count;cnt++)
	{
		param[cnt-2] = va_arg(arglist, unsigned int);	
	}
	
	va_end(arglist);
	
	switch (command)
	{
		case READY :
			{
				MAILBOX_DATA_REGISTER1_CHANNEL0	data_register1;
				MAILBOX_DATA_REGISTER2_CHANNEL0	data_register2;
				MAILBOX_DATA_REGISTER3_CHANNEL0	data_register3;
				MAILBOX_DATA_REGISTER4_CHANNEL0	data_register4;

				data_register1.value = 0;
				data_register2.value = 0;
				data_register3.value = 0;
				data_register4.value = 0;

				data_register1.field_ready.dsp_core_clock = param[0];
				data_register1.field_ready.axi_ahb_clock = param[1];
				data_register2.field_ready.sync_apb_clock = param[2];
				data_register2.field_ready.async_apb_clock = param[3];
				data_register3.field_ready.sys_table_addr = param[4];
				data_register4.field_ready.sys_table_length = param[5];

				
				data_register->data_register[1] = data_register1.value;
				data_register->data_register[2] = data_register2.value;
				data_register->data_register[3] = data_register3.value;
				data_register->data_register[4] = data_register4.value;
			}
			break;
		case CREATE :
			{
				MAILBOX_DATA_REGISTER1_CHANNEL0 data_register1;
				MAILBOX_DATA_REGISTER2_CHANNEL0 data_register2;
				
				data_register1.value = 0;
				data_register2.value = 0;

				data_register1.field_create.flow_type = param[0];
				data_register1.field_create.operation_mode = param[1];
				data_register2.field_create.i2s_sample_rate = param[2];

				data_register->data_register[1] = data_register1.value;
				data_register->data_register[2] = data_register2.value;
			}
			break;
		case OPEN :
			{
				MAILBOX_DATA_REGISTER1_CHANNEL0 data_register1;
				
				data_register1.value = 0;

				data_register1.field_open.algorithm_type = param[0];

				data_register->data_register[1] = data_register1.value;
			}
			break;
		case CONTROL_INIT :
			{
				MAILBOX_DATA_REGISTER1_CHANNEL0 data_register1;
				MAILBOX_DATA_REGISTER2_CHANNEL0 data_register2;
				MAILBOX_DATA_REGISTER3_CHANNEL0 data_register3;
				MAILBOX_DATA_REGISTER4_CHANNEL0 data_register4;
				MAILBOX_DATA_REGISTER5_CHANNEL0 data_register5;
	
				data_register1.value = 0;
				data_register2.value = 0;
				data_register3.value = 0;
				data_register4.value = 0;
				data_register5.value = 0;

				data_register1.field_control_init.stereo = param[0];
				data_register1.field_control_init.woofer = param[1];
				data_register1.field_control_init.sampling_frequency = param[2];
				data_register2.field_control_init.bit_per_sample = param[3];
				data_register2.field_control_init.bit_rate = param[4];
				data_register3.field_timer_wait.timer_wait_ddr_wakeup = param[5];
				data_register4.field_timer_wait.timer_wait_ddr_sleep = param[6];
				data_register5.field_timer_wait.timer_wait_arm_wakeup = param[7];


				data_register->data_register[1] = data_register1.value;
				data_register->data_register[2] = data_register2.value;
				data_register->data_register[3] = data_register3.value;
				data_register->data_register[4] = data_register4.value;
				data_register->data_register[5] = data_register5.value;
			}
			break;
		case CONTROL_PLAY_ENTIRE:
		case CONTROL_PLAY_PARTIAL:
		case CONTROL_PLAY_PARTIAL_END :
			{
				MAILBOX_DATA_REGISTER1_CHANNEL0 data_register1;
				MAILBOX_DATA_REGISTER2_CHANNEL0 data_register2;
				MAILBOX_DATA_REGISTER3_CHANNEL0 data_register3;

				data_register1.value = 0;
				data_register2.value = 0;
				data_register3.value = 0;

				data_register1.field_play.base_address = param[0];
				data_register2.field_play.length = param[1];
				if (command !=CONTROL_PLAY_PARTIAL_END)
				{
					data_register3.field_play.nearstop_threshold = param[2];
				}

				data_register->data_register[1] = data_register1.value;
				data_register->data_register[2] = data_register2.value;
				data_register->data_register[3] = data_register3.value;
			}
			break;
		case CONTROL_PAUSE:
			{
				MAILBOX_DATA_REGISTER1_CHANNEL0 data_register1;

				data_register1.value = 0;
				data_register1.value = param[0];
			}
			break;
		case CONTROL_VOLUME :
			{
				MAILBOX_DATA_REGISTER1_CHANNEL0 data_register1;

				data_register1.value = 0;

				data_register1.field_volume.vol_value = (int)param[0];

				data_register->data_register[1] = data_register1.value;
			}
			break;
		case CONTROL_GAIN :
			{
				MAILBOX_DATA_REGISTER1_CHANNEL0 data_register1;

				data_register1.value = 0;

				data_register1.field_gain.vol_value = (int)param[0];

				data_register->data_register[1] = data_register1.value;
			}
			break;
		case CONTROL_ALARM :
			{
				MAILBOX_DATA_REGISTER1_CHANNEL0 data_register1;
				MAILBOX_DATA_REGISTER2_CHANNEL0 data_register2;
				MAILBOX_DATA_REGISTER3_CHANNEL0 data_register3;

				data_register1.value = 0;
				data_register2.value = 0;
				data_register3.value = 0;

				data_register1.field_alarm.base_address = param[0];
				data_register2.field_alarm.length = param[1];
				data_register3.field_alarm.volume = (int)param[2];

				data_register->data_register[1] = data_register1.value;
				data_register->data_register[2] = data_register2.value;
				data_register->data_register[3] = data_register3.value;
			}
			break;
			/* ALSA */
		case PLAYBACK :
		case CAPTURE :
			{
				MAILBOX_DATA_REGISTER1_CHANNEL0 data_register1;
				MAILBOX_DATA_REGISTER2_CHANNEL0 data_register2;
				MAILBOX_DATA_REGISTER3_CHANNEL0 data_register3;
				MAILBOX_DATA_REGISTER4_CHANNEL0 data_register4;

				data_register1.value = 0;
				data_register2.value = 0;
				data_register3.value = 0;
				data_register4.value = 0;

				data_register1.field_playback.base_address = param[0];
				data_register2.field_playback.base_address = param[1];
				data_register3.field_playback.size = param[2];
				data_register4.field_playback.operation = param[3];

				data_register->data_register[1] = data_register1.value;
				data_register->data_register[2] = data_register2.value;
				data_register->data_register[3] = data_register3.value;
				data_register->data_register[4] = data_register4.value;
			}
			break;
		case NORMALIZER:
			{
				MAILBOX_DATA_REGISTER1_CHANNEL0 data_register1;
				data_register1.value = 0;
				data_register1.field_normalizer.is_normalizer_enable = param[0];
				data_register->data_register[1] = data_register1.value;
			}
			break;
		default:
			break;
	}
}

void
hal_send_to_dsp(AUD_MBOX_TypeDef *pMailbox,
		MAILBOX_DATAREGISTER *pstSendMailboxDataRegister)
{
	MAILBOX_DATA_REGISTER_LIST send_data_register_list;

	memcpy(&send_data_register_list.data_register,
			pstSendMailboxDataRegister,
			sizeof(MAILBOX_DATAREGISTER));

#if 0
	INIT_LIST_HEAD(&send_data_register_list.list);

	spin_lock(&pst_data_register_list->lock);
	list_add_tail(&send_data_register_list.list, &pst_data_register_list->list);
	spin_unlock(&pst_data_register_list->lock);
#endif


#if 0
	ViewList(&pst_data_register_list->list);
#endif

#if 0
	int j;
	for (j=0;j<NUMBER_OF_DATAREG;j++)
	{
		printk("data_register[%d]= 0x%08X\n",
				j, pstSendMailboxDataRegister->data_register[j]);
	}
#endif

	mailbox_set_source(pMailbox, (unsigned int)MAILBOX_ARM_ID_MASK);
	mailbox_set_maskset(pMailbox,
			(unsigned int)(MAILBOX_ARM_ID_MASK|MAILBOX_DSP_ID_MASK));
	mailbox_set_destination(pMailbox, (unsigned int)MAILBOX_DSP_ID_MASK);
	hal_write_data_register(pMailbox, pstSendMailboxDataRegister);
	mailbox_start_send(pMailbox);
}

void
hal_view_list(struct list_head *head)
{
	struct list_head *p;
	MAILBOX_DATA_REGISTER_LIST *temp_list;
	int i;

	list_for_each(p,head) {
		temp_list = list_entry(p,MAILBOX_DATA_REGISTER_LIST,list);
		for (i=0;i<NUMBER_OF_DATAREG;i++)
		{
			printk("send dataregister[%d] = 0x%08X\n",
					i,temp_list->data_register.data_register[i]);
		}
	}
}


void
hal_write_data_register_for_audio_manager(
		MAILBOX_DATAREGISTER *pst_mailbox_data_register,
		unsigned int taskGroup,
		unsigned int action,
		unsigned int type)
{
	int i;
	MAILBOX_DATA_REGISTER0_OMX data_register0;
	MAILBOX_DATA_REGISTER1_OMX data_register1;

	memset(&data_register0,0,sizeof(MAILBOX_DATA_REGISTER0_OMX));
	data_register0.field.sender_device_id = OMX_ARM;
	data_register0.field.receiver_device_id = OMX_DSP;
	data_register0.field.sender_object_id = DSP_DRIVER;
	data_register0.field.receiver_object_id = AUDIO_MANAGER;
	data_register0.field.resource_id = DEFAULT;

	pst_mailbox_data_register->data_register[0]= hal_write_data_register0(
			data_register0.field.sender_device_id,
			data_register0.field.receiver_device_id,
			data_register0.field.sender_object_id,
			data_register0.field.receiver_object_id,
			data_register0.field.resource_id);

	memset(&data_register1,0,sizeof(MAILBOX_DATA_REGISTER1_OMX));

	data_register1.field_audio_manager.transaction_id = transaction_number;
	data_register1.field_audio_manager.task_group = taskGroup;
	data_register1.field_audio_manager.action = action;
	data_register1.field_audio_manager.type = type;

	if (transaction_number == 0xFFFF)
	{
		transaction_number = 0;
	}
	else
	{
		transaction_number++;
	}


	pst_mailbox_data_register->data_register[1] =
		hal_write_data_register1_audio_manager(
			data_register1.field_audio_manager.transaction_id,
			data_register1.field_audio_manager.task_group,
			data_register1.field_audio_manager.action,
			data_register1.field_audio_manager.type);

}

void
hal_write_data_register_for_cdaif(
		MAILBOX_DATAREGISTER *pst_mailbox_data_register,
		unsigned int status,
		unsigned int type,
		unsigned int address,
		unsigned int size)
{
	int i;
	MAILBOX_DATA_REGISTER0_OMX data_register0;
	MAILBOX_DATA_REGISTER1_OMX data_register1;

	memset(&data_register0,0,sizeof(MAILBOX_DATA_REGISTER0_OMX));
	data_register0.field.sender_device_id = OMX_ARM;
	data_register0.field.receiver_device_id = OMX_DSP;
	data_register0.field.sender_object_id = DSP_DRIVER;
	data_register0.field.receiver_object_id = CDAIF;
	data_register0.field.resource_id = DEFAULT;

	pst_mailbox_data_register->data_register[0]= hal_write_data_register0(
			data_register0.field.sender_device_id,
			data_register0.field.receiver_device_id,
			data_register0.field.sender_object_id,
			data_register0.field.receiver_object_id,
			data_register0.field.resource_id);

	memset(&data_register1,0,sizeof(MAILBOX_DATA_REGISTER1_OMX));

	data_register1.field_cdaif.transaction_id = transaction_number;
	data_register1.field_cdaif.status = status;
	data_register1.field_cdaif.type = type;

	if (transaction_number == 0xFFFF)
	{
		transaction_number=0;
	}
	else
	{
		transaction_number++;
	}

	pst_mailbox_data_register->data_register[1]= hal_write_data_register1_cdaif(
			data_register1.field_cdaif.transaction_id,
			data_register1.field_cdaif.status,
			data_register1.field_cdaif.type);

	if (status == CPB1_ACTIVE || status == CPB2_ACTIVE)
	{
		pst_mailbox_data_register->data_register[4] = address;
	}
	else if (status == DPB1_ACTIVE || status == DPB2_ACTIVE)
	{
		pst_mailbox_data_register->data_register[5] = address;
	}
	pst_mailbox_data_register->data_register[6] = size;

}

int
hal_mailbox_init(void)
{
	int i;

	transaction_number = 0;

	for (i=0;i<NUMBER_OF_MAILBOX;i++)
	{
		mailbox_clear_source(&(pst_audio_mailbox->mailbox[i]));
	}

	pst_received_data_register =
		kmalloc(sizeof(MAILBOX_DATAREGISTER), GFP_KERNEL);
	pst_mailbox_data_register = kmalloc(sizeof(MAILBOX_DATAREGISTER),GFP_KERNEL);
	pst_data_register_list =
		kmalloc(sizeof(MAILBOX_DATA_REGISTER_LIST),GFP_KERNEL);
	spin_lock_init(&pst_data_register_list->lock);
	INIT_LIST_HEAD(&pst_data_register_list->list);
	return 1;
}

void
hal_mailbox_exit(void)
{
	kfree(pst_received_data_register);
	pst_received_data_register = NULL;
	kfree(pst_mailbox_data_register);
	pst_mailbox_data_register = NULL;
	kfree(pst_data_register_list);
	pst_data_register_list = NULL;
}

unsigned int
hal_find_send_mailbox_number(void)
{
	int i;

	for (i=0;i<NUMBER_OF_MAILBOX;i++)
	{
		if (pst_audio_mailbox->mailbox[i].send==0x0)
		{
			return i;
		}
	}
	return 0;
}


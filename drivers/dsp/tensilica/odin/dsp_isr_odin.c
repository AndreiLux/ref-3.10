#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/irq.h>			/**< For isr */
#include <linux/sched.h>			/**< For isr */

#include "dsp_coredrv_odin.h"
#include "../mailbox/mailbox_hal.h"
#include "../dsp_lpa_drv.h"
#include "../dsp_pcm_drv.h"
#include "../dsp_omx_drv.h"
#include "../dsp_alsa_drv.h"

#define SEND_REPLY			0

irqreturn_t
dsp_odin_interrupt(int irq, void *data)
{

	volatile int i,j;
	unsigned int num_of_received_mailbox;
	unsigned int received_mailbox_number[NUMBER_OF_MAILBOX] = {0,};
	unsigned int interrupt_status;
	unsigned long flags;

	AUD_MAILBOX *pst_received_mailbox;

#if 0
	printk("Entered Mailbox Inturrept!!\n");
#endif
	pst_received_mailbox = (AUD_MAILBOX *)data;

	num_of_received_mailbox = 0;

	/* Find received mailbox */
	interrupt_status = pst_audio_mailbox_interrupt->masked_interrupt_status;
	for (i=0;i<NUMBER_OF_MAILBOX;i++)
	{
		if (interrupt_status & 0x1)
		{
			received_mailbox_number[num_of_received_mailbox]=i;
			num_of_received_mailbox++;
		}
		interrupt_status = interrupt_status>>1;
	}

#if 0
		printk("num_of_received_mailbox = %d\n", num_of_received_mailbox);
		for (i=0;i<num_of_received_mailbox;i++)
		{
			for (j=0;j<NUMBER_OF_DATAREG;j++)
			{
			printk("DataRegister[%d]= 0x%08X\n", j,
					pst_received_mailbox->mailbox[received_mailbox_number[i]].
					data_register.data_register[j]);
			}
		}
#endif

	for (i=0;i<num_of_received_mailbox;i++)
	{
		/* send = 0x1 or 0x2 ? */
		if (mailbox_get_interrupt_send(&pst_received_mailbox->
					mailbox[received_mailbox_number[i]]) == 0x1)
		{
			*pst_received_data_register =
				hal_get_data_register(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]]);

			/* If REQ/RES Mailbox */
			if (received_mailbox_number[i] == CH0_REQ_RES)
			{
				kthread_mailbox_flag = 1;
			}
			/* If IND Mailbox */
			else if (received_mailbox_number[i] == CH1_IND)
			{
				kthread_mailbox_flag = 2;
			}
			/* If NTF Mailbox */
			else if ((received_mailbox_number[i] == CH2_NTF)||
					(received_mailbox_number[i] == CH7_NTF))
			{
				kthread_mailbox_flag = 3;
			}

			if (dsp_mode->is_opened_alsa)
			{
				MAILBOX_DATA_REGISTER0_CHANNEL0 data_register0;

				data_register0.value = pst_received_data_register->data_register[0];

				spin_lock_irqsave(&workqueue_flag.flag_lock, flags);
				workqueue_flag.alsa_workqueue_flag = kthread_mailbox_flag;
				spin_unlock_irqrestore(&workqueue_flag.flag_lock, flags);

				queue_work(alsa_workqueue, &alsa_work);

#if SEND_REPLY
				mailbox_clear_send(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]]);

				mailbox_start_reply(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]]);
#else
				mailbox_clear_source(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]]);
#endif
				if (i == num_of_received_mailbox - 1)
					return IRQ_HANDLED;

			}		
			else if (dsp_mode->is_opened_lpa)
			{
				MAILBOX_DATA_REGISTER0_CHANNEL0 data_register0;

				data_register0.value = pst_received_data_register->data_register[0];

				if (data_register0.field.command == CONTROL_ALARM)
				{
					spin_lock_irqsave(&workqueue_flag.flag_lock, flags);
					workqueue_flag.pcm_workqueue_flag = kthread_mailbox_flag;
					spin_unlock_irqrestore(&workqueue_flag.flag_lock, flags);
					
					queue_work(pcm_workqueue, &pcm_work);
				}
				else
				{	
					spin_lock_irqsave(&workqueue_flag.flag_lock, flags);
					workqueue_flag.lpa_workqueue_flag = kthread_mailbox_flag;
					spin_unlock_irqrestore(&workqueue_flag.flag_lock, flags);
					
					queue_work(lpa_workqueue, &lpa_work);
				}

#if SEND_REPLY
				mailbox_clear_send(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]]);
				
				mailbox_start_reply(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]]);
#else
				mailbox_clear_source(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]]);
#endif
				if (i == num_of_received_mailbox - 1)
					return IRQ_HANDLED;

			}
			else if (dsp_mode->is_opened_omx)
			{
				mailbox_clear_send(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]]);

				mailbox_set_maskclear(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]],0xf);

				mailbox_clear_destination(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]],0xf);

				mailbox_clear_source(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]]);

				mailbox_set_maskset(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]],0x5);
			
				atomic_set(&received_mailbox_flag,1);
				wake_up_interruptible(&dsp_wait->wait);

				if (i == num_of_received_mailbox - 1)
					return IRQ_HANDLED;
			}
		}
		else if (mailbox_get_interrupt_send(&pst_received_mailbox->
					mailbox[received_mailbox_number[i]]) == 0x2)
		{
			MAILBOX_DATA_REGISTER0_CHANNEL0 data_register0;

			data_register0 = (MAILBOX_DATA_REGISTER0_CHANNEL0)
				(pst_received_mailbox->mailbox[i].data_register.data_register[0]);
		
			received_transaction_number = data_register0.field.transaction_id;
			
			mailbox_clear_source(&pst_received_mailbox->
					mailbox[received_mailbox_number[i]]);

			wake_up_interruptible(&dsp_wait->wait);

			if (i == num_of_received_mailbox - 1)
				return IRQ_HANDLED;
		}
	}
	
	return IRQ_NONE;
}

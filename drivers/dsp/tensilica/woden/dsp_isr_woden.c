#include <linux/kernel.h>
#include <linux/irq.h>			/**< For isr */
#include <linux/sched.h>			/**< For isr */

#include "dsp_coredrv_woden.h"
#include "../mailbox/mailbox_hal.h"
#include "../dsp_lpa_drv.h"
#include "../dsp_pcm_drv.h"
#include "../dsp_omx_drv.h"
#include "../dsp_alsa_drv.h"

#define SEND_REPLY			0

irqreturn_t
dsp_woden_interrupt(int irq, void *data)
{
   
	//volatile int i,j;
	volatile int i;
	unsigned int numberOfreceivedMailbox;
	unsigned int receivedMailboxNumber[NUMBER_OF_MAILBOX] = {0,};
	unsigned int interruptStatus;
	unsigned long flags;

	AUD_MAILBOX *pstReceivedMailbox;

#if 0
	printk("Entered Mailbox Inturrept!!\n");
#endif
	pstReceivedMailbox = (AUD_MAILBOX *)data;

	numberOfreceivedMailbox = 0;

	//Find received mailbox
	interruptStatus = pst_audio_mailbox_interrupt->masked_interrupt_status;
	for(i=0;i<NUMBER_OF_MAILBOX;i++)
	{
		if(interruptStatus & 0x1)
		{
			receivedMailboxNumber[numberOfreceivedMailbox]=i;
			numberOfreceivedMailbox++;
		}
		interruptStatus = interruptStatus>>1;
	}

#if 0
		printk("numberOfreceivedMailbox = %d\n", numberOfreceivedMailbox);
		for(i=0;i<numberOfreceivedMailbox;i++)
		{
			for(j=0;j<NUMBER_OF_DATAREG;j++)
			{
			printk("DataRegister[%d]= 0x%08X\n", j, pstReceivedMailbox->mailbox[receivedMailboxNumber[i]].data_register.data_register[j]);
			}
		}
#endif

	for(i=0;i<numberOfreceivedMailbox;i++)
	{
		// send = 0x1 or 0x2 ?
		if(mailbox_get_interrupt_send(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]])== 0x1)
		{
			*pst_received_data_register = hal_get_data_register(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]]);
			if(receivedMailboxNumber[i] == CH0_REQ_RES)  //If REQ/RES Mailbox
			{
				kthread_mailbox_flag = 1;
			}
			else if(receivedMailboxNumber[i] == CH1_IND) //If IND Mailbox
			{
				kthread_mailbox_flag = 2;
			}
			else if((receivedMailboxNumber[i] == CH2_NTF)||(receivedMailboxNumber[i] == CH7_NTF)) //If NTF Mailbox
			{
				kthread_mailbox_flag = 3;
			}
 
			//LPA mode인지 OMX mode인지 등 어떤 operation mode인지 구분 필요 
			//지금은 operation mode가 메일박스 data_register[0]에 있지않아서
			//minor device가 open됐을 때 is_opened_lpa flag를 set시켜서 조건문 만듦
			if(dsp_mode->is_opened_alsa)
			{
				MAILBOX_DATA_REGISTER0_CHANNEL0 data_register0;

				data_register0.value = pst_received_data_register->data_register[0];

				spin_lock_irqsave(&workqueue_flag.flag_lock, flags);
				workqueue_flag.alsa_workqueue_flag = kthread_mailbox_flag;
				spin_unlock_irqrestore(&workqueue_flag.flag_lock, flags);

				queue_work(alsa_workqueue, &alsa_work);

#if SEND_REPLY
				mailbox_clear_send(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]]);
				mailbox_start_reply(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]]);
#else
				mailbox_clear_source(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]]);
#endif
				if(i == numberOfreceivedMailbox - 1)
					return IRQ_HANDLED;

			}		
			else if(dsp_mode->is_opened_lpa)
			{
				MAILBOX_DATA_REGISTER0_CHANNEL0 data_register0;

				data_register0.value = pst_received_data_register->data_register[0];

				if(data_register0.field.command == CONTROL_ALARM)
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
				mailbox_clear_send(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]]);
				mailbox_start_reply(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]]);
#else
				mailbox_clear_source(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]]);
#endif
				if(i == numberOfreceivedMailbox - 1)
					return IRQ_HANDLED;

			}
			else if(dsp_mode->is_opened_omx)
			{
				mailbox_clear_send(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]]);
				mailbox_set_maskclear(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]],0xf);
				mailbox_clear_destination(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]],0xf);
				mailbox_clear_source(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]]);
				mailbox_set_maskset(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]],0x5);
			
				atomic_set(&received_mailbox_flag,1);
				wake_up_interruptible(&dsp_wait->wait);


				if(i == numberOfreceivedMailbox - 1)
					return IRQ_HANDLED;
			}
		}
		else if(mailbox_get_interrupt_send(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]]) == 0x2)
		{
			MAILBOX_DATA_REGISTER0_CHANNEL0 data_register0;

			data_register0 = (MAILBOX_DATA_REGISTER0_CHANNEL0)(pstReceivedMailbox->mailbox[i].data_register.data_register[0]);
		
			received_transaction_number = data_register0.field.transaction_id;
			
			mailbox_clear_source(&pstReceivedMailbox->mailbox[receivedMailboxNumber[i]]);

			wake_up_interruptible(&dsp_wait->wait);


			if(i == numberOfreceivedMailbox - 1)
				return IRQ_HANDLED;
		}
	}
	
	return IRQ_NONE;
}

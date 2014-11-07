#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/irq.h>			/**< For isr */
#include <linux/sched.h>			/**< For isr */

#include "odin_dsp_driver.h"
#include "mailbox/mailbox_hal.h"

#define SEND_REPLY			0
				

unsigned int received_transaction_number;

irqreturn_t
odin_dsp_interrupt(int irq, void *data)
{
	int i;
	unsigned int num_of_received_mailbox;
	unsigned int received_mailbox_number[NUMBER_OF_MAILBOX] = {0,};
	unsigned int interrupt_status;
	AUD_MAILBOX *pst_received_mailbox;

	pr_debug("Entered Mailbox Inturrept!!\n");

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
			if (received_mailbox_number[i] == CH0_REQ_RES ||
					received_mailbox_number[i] == CH1_RES ||
					received_mailbox_number[i] == CH2_PROG)
			{
				queue_work(workqueue_odin_dsp, &odin_dsp_work);

				mailbox_clear_send(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]]);

				mailbox_start_reply(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]]);
			}
			else
			{
				mailbox_clear_source(&pst_received_mailbox->
						mailbox[received_mailbox_number[i]]);
			}
		}
		else if (mailbox_get_interrupt_send(&pst_received_mailbox->
					mailbox[received_mailbox_number[i]]) == 0x2)
		{
			mailbox_clear_source(&pst_received_mailbox->
					mailbox[received_mailbox_number[i]]);
		}

		if (i == num_of_received_mailbox - 1)
			return IRQ_HANDLED;

	}
	return IRQ_HANDLED;
}

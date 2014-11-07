#include "mailbox_drv.h"

/* About MailBox Source Register */
void
mailbox_set_source(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID)
{
	pMailbox->source = maskID;
}

unsigned int
mailbox_get_source(AUD_MBOX_TypeDef *pMailbox)
{
	return pMailbox->source;
}

void
mailbox_clear_source(AUD_MBOX_TypeDef *pMailbox)
{
	pMailbox->source = 0;
}

/* About MailBox Destination Register */
void
mailbox_set_destination(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID)
{
	pMailbox->destinationset = maskID;
}

unsigned int
mailbox_get_destinationset(AUD_MBOX_TypeDef *pMailbox)
{
	return pMailbox->destinationset;
}

unsigned int
mailbox_get_destinationclear(AUD_MBOX_TypeDef *pMailbox)
{
	return pMailbox->destinationclear;
}

void
mailbox_clear_destination(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID)
{
    pMailbox->destinationclear = maskID;
}

unsigned int
mailbox_get_destinationstatus(AUD_MBOX_TypeDef *pMailbox)
{
	return pMailbox->destinationstatus;
}

void
mailbox_set_destinationstatus(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID)
{
	pMailbox->destinationstatus = maskID;
}

/* About MailBox Mode Select Register */
unsigned int
mailbox_get_mode(AUD_MBOX_TypeDef *pMailbox)
{
	return pMailbox->mode;
}

void
mailbox_set_mode(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID)
{
	pMailbox->mode = maskID;
}

/* About MailBox Mask Register */
unsigned int
mailbox_get_maskset(AUD_MBOX_TypeDef *pMailbox)
{
	return pMailbox->maskset;
}

unsigned int
mailbox_get_maskclear(AUD_MBOX_TypeDef *pMailbox)
{
	return pMailbox->maskclear;
}

void
mailbox_set_maskset(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID)
{
		pMailbox->maskset = maskID;
}

void
mailbox_set_maskclear(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID)
{
		pMailbox->maskclear = maskID;
}

unsigned int
mailbox_get_maskstatus(AUD_MBOX_TypeDef *pMailbox)
{
	    return pMailbox->maskstatus;
}

void
mailbox_set_maskstatus(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID)
{
		pMailbox->maskstatus = maskID;
}

unsigned int
mailbox_get_interrupt_send(AUD_MBOX_TypeDef *pMailbox)
{
	    return pMailbox->send;
}

void
mailbox_set_interrupt_send(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID)
{
	pMailbox->send = maskID;
}

void
mailbox_clear_send(AUD_MBOX_TypeDef *pMailbox)
{
	pMailbox->send = 0x0;
}

void
mailbox_start_send(AUD_MBOX_TypeDef *pMailbox)
{
    pMailbox->send =  0x1;
}

void
mailbox_start_reply(AUD_MBOX_TypeDef *pMailbox)
{
    pMailbox->send =  0x2;
}

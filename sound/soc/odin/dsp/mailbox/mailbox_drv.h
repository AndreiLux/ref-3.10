#include "mailbox_reg.h"

extern void
mailbox_set_source(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID);

extern unsigned int
mailbox_get_source(AUD_MBOX_TypeDef *pMailbox);

extern void
mailbox_clear_source(AUD_MBOX_TypeDef *pMailbox);

extern void
mailbox_set_destination(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID);

extern unsigned int
mailbox_get_destinationset(AUD_MBOX_TypeDef *pMailbox);

extern unsigned int
mailbox_get_destinationclear(AUD_MBOX_TypeDef *pMailbox);

extern void
mailbox_clear_destination(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID);

extern unsigned int
mailbox_get_destinationstatus(AUD_MBOX_TypeDef *pMailbox);

extern void
mailbox_set_destinationstatus(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID);

extern unsigned int
mailbox_get_mode(AUD_MBOX_TypeDef *pMailbox);

extern void
mailbox_set_mode(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID);

extern unsigned int
mailbox_get_maskset(AUD_MBOX_TypeDef *pMailbox);

extern unsigned int
mailbox_get_maskclear(AUD_MBOX_TypeDef *pMailbox);

extern void
mailbox_set_maskset(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID);

extern void
mailbox_set_maskclear(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID);

extern unsigned int
mailbox_get_maskstatus(AUD_MBOX_TypeDef *pMailbox);

extern void
mailbox_set_maskstatus(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID);

extern unsigned int
mailbox_get_interrupt_send(AUD_MBOX_TypeDef *pMailbox);

extern void
mailbox_set_interrupt_send(AUD_MBOX_TypeDef *pMailbox, unsigned int maskID);
	
extern void
mailbox_start_send(AUD_MBOX_TypeDef *pMailbox);

extern void
mailbox_start_reply(AUD_MBOX_TypeDef *pMailbox);

extern void
mailbox_clear_send(AUD_MBOX_TypeDef *pMailbox);

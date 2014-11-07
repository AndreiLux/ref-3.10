#include <linux/list.h>

#include "mailbox_drv.h"
#include "mailbox_command.h"
#include "mailbox_data_reg.h"

typedef struct MAILBOX_DATA_REGISTER_LIST {
	struct list_head list;
	spinlock_t lock;
	MAILBOX_DATAREGISTER data_register;
} MAILBOX_DATA_REGISTER_LIST;

extern MAILBOX_DATA_REGISTER_LIST *pst_data_register_list;
extern AUD_MAILBOX *pst_audio_mailbox;
extern MAILBOX_DATAREGISTER *pst_mailbox_data_register;

extern MAILBOX_DATAREGISTER *pst_received_data_register;

extern AUD_MAILBOX_INTERRUPT *pst_audio_mailbox_interrupt;

extern unsigned int
hal_write_data_register0(unsigned int senderDeviceId,
		unsigned int receiverDeviceId,
		unsigned int senderObjectId,
		unsigned int receiverObjectId,
		unsigned int resourceId);

extern unsigned int
hal_write_data_register1_audio_manager(unsigned int transactionId,
		unsigned int taskGroup,
		unsigned int action,
		unsigned int type);

extern unsigned int
hal_write_data_register1_cdaif(unsigned int TransactionId,
		unsigned int status,
		unsigned int type);

extern unsigned int
hal_write_data_register4(unsigned int codec_type,
		unsigned int bit_rate,
		unsigned int channel_number,
		unsigned int bit_per_sample,
		unsigned int sampling_freq);

extern void
hal_write_data_register(AUD_MBOX_TypeDef *pMailbox,
		MAILBOX_DATAREGISTER *pstMailboxDataRegister);

extern void
hal_send_to_dsp(AUD_MBOX_TypeDef *pMailbox,
		MAILBOX_DATAREGISTER *pstSendMailboxDataRegister);

extern unsigned int
hal_get_transaction_number(void);

extern MAILBOX_DATAREGISTER
hal_get_data_register(AUD_MBOX_TypeDef *pMailbox);

extern unsigned int
hal_set_command(MAILBOX_DATAREGISTER *data_register,
		unsigned int command,
		unsigned int type);

extern void
hal_set_parameter(int count, ...);

int
hal_send_mailbox(unsigned int channel_number,
		MAILBOX_DATAREGISTER *data_register);

extern void
hal_write_data_register_for_audio_manager(
		MAILBOX_DATAREGISTER *pstMailboxDataRegister,
		unsigned int taskGroup,
		unsigned int action,
		unsigned int type);

extern void
hal_write_data_register_for_cdaif(
		MAILBOX_DATAREGISTER *pstMailboxDataRegister,
		unsigned int status,
		unsigned int type,
		unsigned int address,
		unsigned int size);

extern int
hal_mailbox_init(void);

extern void
hal_mailbox_exit(void);

unsigned int
hal_find_send_mailbox_number(void);

extern void
hal_view_list(struct list_head *head);

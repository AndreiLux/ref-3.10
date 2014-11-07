#define MAILBOX_ARM_ID_MASK 0x1
#define MAILBOX_DSP_ID_MASK 0x4

#define NUMBER_OF_MAILBOX	32
#define	NUMBER_OF_DATAREG	7

/***********************************************************************
* Mail Box  Type Definitions
************************************************************************/
typedef struct MAILBOX_DATAREGISTER {
	unsigned int data_register[NUMBER_OF_DATAREG];
} MAILBOX_DATAREGISTER;


typedef struct
{
	unsigned int source;					/* Offset  0x00 */
	unsigned int destinationset;			/* Offset  0x04 */
	unsigned int destinationclear;			/* Offset  0x08 */
	unsigned int destinationstatus;			/* Offset  0x0c */
	unsigned int mode;						/* Offset  0x10 */
	unsigned int maskset;					/* Offset  0x14 */
	unsigned int maskclear;					/* Offset  0x18 */
	unsigned int maskstatus;				/* Offset  0x1c */
	unsigned int send;						/* Offset  0x20 */
	MAILBOX_DATAREGISTER data_register;		/* Offset  0x24 */
} AUD_MBOX_TypeDef;

/* 0x34400000 - 0x0x344007FC */
typedef struct
{
	AUD_MBOX_TypeDef mailbox[NUMBER_OF_MAILBOX];
} AUD_MAILBOX;

typedef struct
{
	volatile unsigned int masked_interrupt_status;		/*0x34400800 */
	volatile unsigned int raw_interrupt_status;			/*0x34400804 */
} AUD_MAILBOX_INTERRUPT;


#ifndef __MT_BOARD_H__
#define __MT_BOARD_H__

#include <asm/mach/arch.h>

/* Board Type: 4 Hex digital numbers, it should be the same as the Product USB PID. */
#define BOARD_ID_STRINGRAY			0x0013
#define BOARD_ID_ASTON				0x0012
#define BOARD_ID_ARIEL				0x000F

/* Board Revision: two byte as a a group starting with character 00. */
/* Stingray */
#define BOARD_REV_STRINGRAY_1_0		0x0000
#define BOARD_REV_STRINGRAY_2_0		(BOARD_REV_STRINGRAY_1_0+1)
#define BOARD_REV_STRINGRAY_3_0		(BOARD_REV_STRINGRAY_2_0+1)

/* Ariel */
#define BOARD_REV_ARIEL_PROTO_2_0		0x0000
#define BOARD_REV_ARIEL_PROTO_2_1		(BOARD_REV_ARIEL_PROTO_2_0+1)
#define BOARD_REV_ARIEL_EVT_1_0		(BOARD_REV_ARIEL_PROTO_2_1+1)
#define BOARD_REV_ARIEL_EVT_1_1		(BOARD_REV_ARIEL_EVT_1_0+1)
#define BOARD_REV_ARIEL_EVT_2_0		(BOARD_REV_ARIEL_EVT_1_1+1)
#define BOARD_REV_ARIEL_DVT			(BOARD_REV_ARIEL_EVT_2_0+1)
#define BOARD_REV_ARIEL_PVT			(BOARD_REV_ARIEL_DVT+1)

/* Aston */
#define BOARD_REV_ASTON_PROTO			0x0000
#define BOARD_REV_ASTON_EVT_1_0		(BOARD_REV_ASTON_PROTO+1)
#define BOARD_REV_ASTON_DVT			(BOARD_REV_ASTON_EVT_1_0+1)
#define BOARD_REV_ASTON_PVT			(BOARD_REV_ASTON_DVT+1)

#define BOOTLOADER_LK_ADDRESS	0x81E00000
#define BOOTLOADER_LK_SIZE	0x00100000


#ifdef CONFIG_IDME
extern unsigned int idme_get_board_type(void);
extern unsigned int idme_get_board_rev(void);
#endif

extern struct sys_timer mt8135_timer;
extern struct smp_operations mt65xx_smp_ops;

void mt8135_timer_init(void);
void arm_machine_restart(char mode, const char *cmd);
void mt_fixup(struct tag *tags, char **cmdline, struct meminfo *mi);
void mt_power_off(void);
void mt_power_off_prepare(void);
void mt_init_early(void);
void mt_map_io(void);
void mt_dt_init_irq(void);
void mt_reserve(void);

#endif				/* __MT_BOARD_H__ */

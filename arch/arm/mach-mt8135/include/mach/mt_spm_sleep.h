#ifndef _MT_SPM_SLEEP_
#define _MT_SPM_SLEEP_

#include <linux/kernel.h>
#include <linux/device.h>
#include <mach/irqs.h>

typedef enum {
	WR_NONE = 0,
	WR_UART_BUSY = 1,
	WR_PCM_ASSERT = 2,
	WR_PCM_TIMER = 3,
	WR_WAKE_SRC = 4,
	WR_SW_ABORT = 5,
	WR_PCM_ABORT = 6
} wake_reason_t;

struct mt_wake_event {
	char *domain;
	struct mt_wake_event *parent;
	int code;
};

struct mt_wake_event_map {
	char *domain;
	int code;
	int irq;
	wakeup_event_t we;
};

void spm_report_wakeup_event(struct mt_wake_event *we, int code);
struct mt_wake_event *spm_get_wakeup_event(void);
void spm_set_wakeup_event_map(struct mt_wake_event_map *tbl);
wakeup_event_t spm_read_wakeup_event_and_irq(int *pirq);
void spm_clear_wakeup_event(void);

/*
 * for suspend
 */
extern int spm_set_sleep_wakesrc(u32 wakesrc, bool enable, bool replace);
extern wake_reason_t spm_go_to_sleep(bool cpu_pdn, bool infra_pdn);
extern wake_reason_t spm_go_to_sleep_dpidle(bool cpu_pdn, u8 pwrlevel);

/*
 * for deep idle
 */
extern bool spm_dpidle_can_enter(void);	/* can be redefined */
extern void spm_dpidle_before_wfi(void);	/* can be redefined */
extern void spm_dpidle_after_wfi(void);	/* can be redefined */
extern wake_reason_t spm_go_to_dpidle(bool cpu_pdn, u8 pwrlevel);

extern bool spm_is_md1_sleep(void);
extern bool spm_is_md2_sleep(void);

extern void spm_output_sleep_option(void);

extern void Golden_Setting_Compare_for_Suspend(void);

extern int get_dynamic_period(int first_use, int first_wakeup_time, int battery_capacity_level);

extern int mt_irq_mask_all(struct mtk_irq_mask *mask);
extern int mt_irq_mask_restore(struct mtk_irq_mask *mask);
extern void mt_irq_unmask_for_sleep(unsigned int irq);

extern void mtk_uart_restore(void);
extern void dump_uart_reg(void);

extern spinlock_t spm_lock;
extern int force_infra_pdn;
extern int slp_pcm_timer;

#endif

#if !defined(AEE_COMMON_H)
#define AEE_COMMON_H

#undef WDT_DEBUG_VERBOSE
/* #define WDT_DEBUG_VERBOSE */

int get_memory_size(void);

int in_fiq_handler(void);

int aee_dump_stack_top_binary(char *buf, int buf_len, unsigned long bottom, unsigned long top);

extern void ram_console_write(struct console *console, const char *s, unsigned int count);

#ifdef WDT_DEBUG_VERBOSE
extern int dump_localtimer_info(char *buffer, int size);
extern int dump_idle_info(char *buffer, int size);
#endif

#ifdef CONFIG_SCHED_DEBUG
extern int sysrq_sched_debug_show_at_KE(void);
extern int sysrq_sched_debug_show(void);
#endif

#ifdef CONFIG_SMP
extern void dump_log_idle(void);
extern void irq_raise_softirq(const struct cpumask *mask, unsigned int irq);
#endif

#ifdef CONFIG_MTK_AEE_IPANIC
extern void aee_dumpnative(void);
#endif
extern int debug_locks;

extern int g_is_panic;
#endif				/* AEE_COMMON_H */

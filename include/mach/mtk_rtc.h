#ifndef MTK_RTC_H
#define MTK_RTC_H

#ifdef CONFIG_RTC_DRV_MTK
#include <mach/rtc.h>
static inline bool crystal_exist_status(void) { return rtc_crystal_exist_status(); }
#else

#include <linux/ioctl.h>
#include <linux/rtc.h>
#include <mach/mt_typedefs.h>

typedef enum {
	RTC_GPIO_USER_WIFI = 8,
	RTC_GPIO_USER_GPS = 9,
	RTC_GPIO_USER_BT = 10,
	RTC_GPIO_USER_FM = 11,
	RTC_GPIO_USER_PMIC = 12,
} rtc_gpio_user_t;

enum rtc_reboot_reason {
	RTC_REBOOT_REASON_WARM,
	RTC_REBOOT_REASON_PANIC,
	RTC_REBOOT_REASON_SW_WDT,
	RTC_REBOOT_REASON_FROM_POC
};

#ifdef CONFIG_MTK_RTC

/*
 * NOTE:
 * 1. RTC_GPIO always exports 32K enabled by some user even if the phone is powered off
 */

extern unsigned long rtc_read_hw_time(void);
extern void rtc_gpio_enable_32k(rtc_gpio_user_t user);
extern void rtc_gpio_disable_32k(rtc_gpio_user_t user);
extern bool rtc_gpio_32k_status(void);
extern void hal_write_dcxo_c2(u16 val, bool flag);

/* for AUDIOPLL (deprecated) */
extern void rtc_enable_abb_32k(void);
extern void rtc_disable_abb_32k(void);

/* NOTE: used in Sleep driver to workaround Vrtc-Vore level shifter issue */
extern void rtc_enable_writeif(void);
extern void rtc_disable_writeif(void);

extern int  rtc_get_reboot_reason(void);
extern void rtc_mark_reboot_reason(int);

extern void rtc_mark_recovery(void);
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
extern void rtc_mark_kpoc(void);
extern void rtc_mark_enter_kpoc(void);

#endif
extern void rtc_mark_fast(void);

extern void rtc_mark_emergency(void);

extern void rtc_mark_clear_lprst(void);

extern void rtc_mark_enter_lprst(void);

extern void rtc_mark_enter_sw_lprst(void);

extern u16 rtc_rdwr_uart_bits(u16 *val);

extern void rtc_bbpu_power_down(void);

extern void rtc_read_pwron_alarm(struct rtc_wkalrm *alm);


extern int get_rtc_spare_fg_value(void);
extern int set_rtc_spare_fg_value(int val);

extern void rtc_irq_handler(void);

extern bool crystal_exist_status(void);

extern bool rtc_lprst_detected(void);
extern bool rtc_enter_kpo_detected(void);

#else
#define rtc_read_hw_time()              ({ 0; })
#define rtc_gpio_enable_32k(user)	do {} while (0)
#define rtc_gpio_disable_32k(user)	do {} while (0)
#define rtc_gpio_32k_status()		do {} while (0)
#define rtc_enable_abb_32k()		do {} while (0)
#define rtc_disable_abb_32k()		do {} while (0)
#define rtc_enable_writeif()		do {} while (0)
#define rtc_disable_writeif()		do {} while (0)
#define rtc_mark_recovery()             do {} while (0)
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
#define rtc_mark_kpoc()                 do {} while (0)
#define rtc_mark_enter_kpoc()           do {} while (0)
#endif
#define rtc_mark_fast()		        do {} while (0)
#define rtc_mark_emergency()		do {} while (0)
#define rtc_mark_clear_lprst()		do {} while (0)
#define rtc_mark_enter_lprst()		do {} while (0)
#define rtc_mark_enter_sw_lprst()	do {} while (0)
#define rtc_rdwr_uart_bits(val)		({ 0; })
#define rtc_bbpu_power_down()		do {} while (0)
#define rtc_read_pwron_alarm(alm)	do {} while (0)

#define get_rtc_spare_fg_value()	({ 0; })
#define set_rtc_spare_fg_value(val)	({ 0; })

#define rtc_get_reboot_reason()	(0)
#define rtc_mark_reboot_reason(arg)	do {} while (0)

#define rtc_irq_handler()			do {} while (0)

#define crystal_exist_status()		do {} while (0)
#define rtc_lprst_detected()		do {} while (0)
#define rtc_enter_kpoc_detected()		({ 0; })
#define hal_write_dcxo_c2(val, flag) do {} while (0)
#endif

#endif

#endif

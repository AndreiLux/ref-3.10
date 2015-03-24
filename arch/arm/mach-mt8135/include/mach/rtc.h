#ifndef __ARCH_ARM_MTK_RTC_H
#define __ARCH_ARM_MTK_RTC_H

#include <linux/ioctl.h>
#include <linux/rtc.h>
#include <mach/mt_typedefs.h>

enum rtc_gpio_user {
	RTC_GPIO_USER_WIFI = 8,
	RTC_GPIO_USER_GPS = 9,
	RTC_GPIO_USER_BT = 10,
	RTC_GPIO_USER_FM = 11,
	RTC_GPIO_USER_PMIC = 12,
};

#ifdef CONFIG_RTC_DRV_MTK

enum rtc_reboot_reason {
	RTC_REBOOT_REASON_WARM,
	RTC_REBOOT_REASON_PANIC,
	RTC_REBOOT_REASON_SW_WDT,
	RTC_REBOOT_REASON_FROM_POC
};

int  rtc_get_reboot_reason(void);
void rtc_mark_reboot_reason(int);

/*
 * NOTE:
 * 1. RTC_GPIO always exports 32K enabled by some user even if the phone is powered off
 */

unsigned long rtc_read_hw_time(void);
void rtc_gpio_enable_32k(enum rtc_gpio_user user);
void rtc_gpio_disable_32k(enum rtc_gpio_user user);
bool rtc_gpio_32k_status(void);
void rtc_enable_abb_32k(void);
void rtc_disable_abb_32k(void);
void rtc_enable_writeif(void);
void rtc_disable_writeif(void);

bool rtc_crystal_exist_status(void);
bool rtc_low_power_detected(void);
bool rtc_lprst_detected(void);
bool rtc_enter_kpoc_detected(void);

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
void rtc_mark_kpoc(void);
void rtc_mark_enter_kpoc(void);
#endif

void rtc_mark_recovery(void);
void rtc_mark_fast(void);
void rtc_mark_emergency(void);
void rtc_mark_clear_lprst(void);
void rtc_mark_enter_lprst(void);
void rtc_mark_enter_sw_lprst(void);
u16 rtc_rdwr_uart_bits(u16 *val);
void rtc_bbpu_power_down(void);
void rtc_read_pwron_alarm(struct rtc_wkalrm *alm);

int get_rtc_spare_fg_value(void);
int set_rtc_spare_fg_value(int val);

void rtc_write_dcxo_c2(u16 val, bool flag);

#else

#define rtc_read_hw_time()              ({ 0; })
#define rtc_gpio_enable_32k(user)	do {} while (0)
#define rtc_gpio_disable_32k(user)	do {} while (0)
#define rtc_gpio_32k_status()		do {} while (0)
#define rtc_enable_abb_32k()		do {} while (0)
#define rtc_disable_abb_32k()		do {} while (0)
#define rtc_enable_writeif()		do {} while (0)
#define rtc_disable_writeif()		do {} while (0)

#define rtc_crystal_exist_status()	({ 0; })
#define rtc_low_power_detected()	({ 0; })
#define rtc_lprst_detected()		({ 0; })
#define rtc_enter_kpoc_detected()		({ 0; })

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
#define rtc_mark_kpoc()                 do {} while (0)
#define rtc_mark_enter_kpoc()           do {} while (0)
#endif

#define rtc_mark_recovery()             do {} while (0)
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

#define rtc_write_dcxo_c2(val, flag) do {} while (0)

#endif

#endif /* __ARCH_ARM_MTK_RTC_H */

/*
 * rtc-pmurtc.h - Registers definition and platform data structure for the hi6421 pmu RTC.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Author: chenmudan <chenmudan@huawei.com>.
 */
#ifndef __LINUX_RTC_PMURTC_H
#define __LINUX_RTC_PMURTC_H

#define HI6421V300_RTCDR0               (0x12C) /* Data read register */
#define HI6421V300_RTCDR1               (0x12D) /* Data read register */
#define HI6421V300_RTCDR2               (0x12E) /* Data read register */
#define HI6421V300_RTCDR3               (0x12F) /* Data read register */
#define HI6421V300_RTCMR0               (0x130) /* Match register */
#define HI6421V300_RTCMR1               (0x131) /* Match register */
#define HI6421V300_RTCMR2               (0x132) /* Match register */
#define HI6421V300_RTCMR3               (0x133) /* Match register */
#define HI6421V300_RTCLR0               (0x134) /* Data load register */
#define HI6421V300_RTCLR1               (0x135) /* Data load register */
#define HI6421V300_RTCLR2               (0x136) /* Data load register */
#define HI6421V300_RTCLR3               (0x137) /* Data load register */
#define HI6421V300_RTCCTRL              (0x138) /* Control register */
#define HI6421V300_RTCIRQ0              (0x120) /* Alarm interrupt bit */
#define HI6421V300_RTCIRQM0             (0x102) /* Alarm interrupt mask and set register  1:enable,0:disable */
#define HI6421V300_ALARM_ON             (0x001) /* Alarm current status */
#define ALARM_ENABLE_MASK               0xFE

extern unsigned int get_pd_charge_flag(void);

#endif /*__LINUX_RTC_PMURTC_H*/

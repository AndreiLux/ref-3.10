#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/aee.h>
#include <linux/device.h>

#include <mach/irqs.h>
#include <mach/mt_boot.h>
#include <mach/mt_cirq.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_sleep.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_dcm.h>
#include <mach/mt_dormant.h>
#include <mach/mt_mci.h>
#include <mach/eint.h>
#include <mach/wd_api.h>
#include <mach/mt_gpio_def.h>
#include <mach/mt_gpt.h>

/**************************************
 * only for internal debug
 **************************************/
#ifdef CONFIG_MTK_LDVT
#define SPM_PWAKE_EN            0
#define SPM_BYPASS_SYSPWREQ     1
#else
#define SPM_PWAKE_EN            1
#define SPM_BYPASS_SYSPWREQ     0
#endif

#if defined(CONFIG_AMAZON_METRICS_LOG)
/* forced trigger system_resume:off_mode metrics log */
int force_gpt = 0;
module_param(force_gpt, int, 0644);
#endif

static struct mt_wake_event spm_wake_event = {
	.domain = "SPM",
};

static struct mt_wake_event *mt_wake_event;
static struct mt_wake_event_map *mt_wake_event_tbl;

/**********************************************************
 * PCM code for suspend (v5rc12 @ 2013-08-16)
 **********************************************************/
static const u32 spm_pcm_suspend[] = {
	0x19c0001f, 0x003c0bd7, 0x1800001f, 0xc7cffffd, 0x1b80001f, 0x20000000,
	0x1800001f, 0xc7cfff94, 0x19c0001f, 0x003c0bc7, 0x1b00001f, 0x3ffff7ff,
	0xf0000000, 0x17c07c1f, 0x1b00001f, 0x3fffefff, 0x19c0001f, 0x00382bd7,
	0x1800001f, 0xc7cffffd, 0x1800001f, 0xc7fffffd, 0x19c0001f, 0x001823d7,
	0xf0000000, 0x17c07c1f, 0xf0000000, 0x17c07c1f, 0xf0000000, 0x17c07c1f,
	0xe2e00e16, 0x1380201f, 0xe2e00e1e, 0x1380201f, 0xe2e00e0e, 0x1380201f,
	0xe2e00e0f, 0xe2e00e0d, 0xe2e00e0d, 0xe2e00c0d, 0xe2e0080d, 0xe2e0000d,
	0xf0000000, 0x17c07c1f, 0xd800060a, 0x17c07c1f, 0xe2e0006d, 0xe2e0002d,
	0xd82006aa, 0x17c07c1f, 0xe2e0002f, 0xe2e0003e, 0xe2e00032, 0xf0000000,
	0x17c07c1f, 0xe2e0000d, 0xe2e0000c, 0xe2e0001c, 0xe2e0001e, 0xe2e00016,
	0xe2e00012, 0xf0000000, 0x17c07c1f, 0xe2e00016, 0x1380201f, 0xe2e0001e,
	0x1380201f, 0xe2e0001c, 0x1380201f, 0xe2e0000c, 0xe2e0000d, 0xf0000000,
	0x17c07c1f, 0xa1d40407, 0x1391841f, 0xa1d90407, 0xf0000000, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001,
	0xa1d48407, 0x1990001f, 0x10006400, 0x18c0001f, 0x10200114, 0x1910001f,
	0x10200114, 0xa9000004, 0x00000010, 0xe0c00004, 0x1b00001f, 0x3fffe7ff,
	0x1b80001f, 0xd00f0000, 0x8880000c, 0x3effe7ff, 0xd8003fc2, 0x1140041f,
	0x809c040c, 0xd8002182, 0x8098040d, 0xe8208000, 0x10006354, 0xfffffffd,
	0xc0c03bc0, 0x81471801, 0xd8002885, 0x17c07c1f, 0x89c00007, 0xffffefff,
	0x18c0001f, 0x10006200, 0xc0c00580, 0x12807c1f, 0xe8208000, 0x1000625c,
	0x00000001, 0x1890001f, 0x1000625c, 0x81040801, 0xd82024e4, 0x17c07c1f,
	0xc0c00580, 0x1280041f, 0x18c0001f, 0x10006204, 0xc0c006e0, 0x1280041f,
	0x18c0001f, 0x10006208, 0xc0c00580, 0x12807c1f, 0xe8208000, 0x10006244,
	0x00000001, 0x1890001f, 0x10006244, 0x81040801, 0xd8202724, 0x17c07c1f,
	0xc0c00580, 0x1280041f, 0x18c0001f, 0x10006290, 0xc0c00580, 0x1280041f,
	0xc0c00920, 0x81879801, 0x1b00001f, 0xffffffff, 0x1b80001f, 0xd0010000,
	0x8880000c, 0x3effe7ff, 0xd80039a2, 0x17c07c1f, 0x8880000c, 0x40000800,
	0xd80028c2, 0x17c07c1f, 0x18d0001f, 0x10001050, 0x88c00003, 0xfffffeff,
	0x1900001f, 0x10001050, 0xe1000003, 0x18d0001f, 0x10001050, 0x19c0001f,
	0x00240b01, 0x1880001f, 0x10006320, 0xe8208000, 0x10006354, 0xffffffff,
	0xc0c03c60, 0xe080000f, 0xd80028c3, 0x17c07c1f, 0xe8208000, 0x10006310,
	0x0b1600f8, 0xe080001f, 0x19c0001f, 0x003c0bc7, 0x1b80001f, 0x20000030,
	0xe8208000, 0x1000627c, 0x00000005, 0xe8208000, 0x1000627c, 0x00000004,
	0xd8002f46, 0x17c07c1f, 0x18c0001f, 0x10006240, 0xc0c03ac0, 0x17c07c1f,
	0x1800001f, 0x00000094, 0x1800001f, 0x00003f94, 0x1800001f, 0x87c0bf94,
	0x1800001f, 0xc7cfff94, 0xd8003166, 0x17c07c1f, 0x18c0001f, 0x10006234,
	0xc0c03ac0, 0x17c07c1f, 0xe8208000, 0x10006234, 0x000f0e12, 0x1b00001f,
	0x3fffe7ff, 0x19c0001f, 0x003c2bd7, 0x1800001f, 0xc7fffffd, 0x19c0001f,
	0x001823d7, 0x1b80001f, 0x90100000, 0x19c0001f, 0x003c0bd7, 0x1800001f,
	0xc7cffffd, 0x1b80001f, 0x20000000, 0x1800001f, 0xc7cfff94, 0x19c0001f,
	0x003c0bc7, 0x18c0001f, 0x10006240, 0x1900001f, 0x10006234, 0xd8003566,
	0x17c07c1f, 0xc10003c0, 0x1211841f, 0xe0e00f16, 0xe0e00f1e, 0xe0e00f0e,
	0xe0e00f0f, 0xe8208000, 0x1000627c, 0x00000005, 0x1b80001f, 0x20000020,
	0xe8208000, 0x1000627c, 0x00000009, 0x19c0001f, 0x003c0b87, 0x1800001f,
	0x87c0bf94, 0x1b80001f, 0x20000424, 0x1800001f, 0x04003f94, 0x1b80001f,
	0x20000424, 0x1800001f, 0x00003f94, 0x1b80001f, 0x20000424, 0x1800001f,
	0x00000094, 0x10007c1f, 0x1b80001f, 0x2000000a, 0xe0e00f0d, 0x1b80001f,
	0x20000424, 0x19c0001f, 0x00240b07, 0xd82039a2, 0x17c07c1f, 0xe8208000,
	0x10006354, 0xfffffffd, 0x19c0001f, 0x00210b05, 0x19c0001f, 0x00210a05,
	0xd0003fc0, 0x17c07c1f, 0xe2e0000d, 0xe2e0020d, 0xe2e0060d, 0xe2e00e0f,
	0xe2e00e1e, 0xe2e00e12, 0xf0000000, 0x17c07c1f, 0xa1d10407, 0x1b80001f,
	0x20000014, 0xf0000000, 0x17c07c1f, 0xa1d08407, 0x1b80001f, 0x20000280,
	0x82eab401, 0x1a00001f, 0x10006814, 0xe200000b, 0xf0000000, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0xe8208000, 0x10006354, 0xfffffffd, 0x1890001f,
	0x10006600, 0x80800801, 0x18d0001f, 0x10001050, 0x8a000003, 0xfffffeff,
	0x1900001f, 0x10001050, 0xe1000008, 0x18d0001f, 0x10001050, 0xa0d40803,
	0xe1000003, 0xd80047a5, 0x17c07c1f, 0x18c0001f, 0x10006290, 0xc0c04860,
	0x1280041f, 0x18c0001f, 0x10006208, 0x1212841f, 0xc0c04860, 0x12807c1f,
	0xe8208000, 0x10006244, 0x00000000, 0x1890001f, 0x10006244, 0x81040801,
	0xd80043e4, 0x17c07c1f, 0x1b80001f, 0x20000080, 0xc0c04860, 0x1280041f,
	0x18c0001f, 0x10006204, 0xc0c007e0, 0x1280041f, 0x18c0001f, 0x10006200,
	0x1212841f, 0xc0c04860, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000000,
	0x1890001f, 0x1000625c, 0x81040801, 0xd8004684, 0x17c07c1f, 0x1b80001f,
	0x20000080, 0xc0c04860, 0x1280041f, 0x19c0001f, 0x00211800, 0x1ac0001f,
	0x55aa55aa, 0x10007c1f, 0xf0000000, 0xd800496a, 0x17c07c1f, 0xe2e00036,
	0x1380201f, 0xe2e0003e, 0x1380201f, 0xe2e0002e, 0x1380201f, 0xd8204a2a,
	0x17c07c1f, 0xe2e0006e, 0xe2e0004e, 0xe2e0004c, 0xe2e0004d, 0xf0000000,
	0x17c07c1f
};

#define PCM_SUSPEND_BASE        __pa(spm_pcm_suspend)
#define PCM_SUSPEND_LEN         (595 - 1)
#define PCM_SUSPEND_VEC0        EVENT_VEC(11, 1, 0, 0)	/* 26M-wake event */
#define PCM_SUSPEND_VEC1        EVENT_VEC(12, 1, 0, 14)	/* 26M-sleep event */
#define PCM_SUSPEND_VEC2        EVENT_VEC(30, 1, 0, 26)	/* APSRC-wake event */
#define PCM_SUSPEND_VEC3        EVENT_VEC(31, 1, 0, 28)	/* APSRC-sleep event */


/**********************************************************
 * E1 PCM code for deep idle (v3rc6 @ 2013-08-16)
 **********************************************************/
static const u32 spm_pcm_dpidle[] = {
	0x1800001f, 0xc7cfff94, 0x1b00001f, 0x3ffff7ff, 0xf0000000, 0x17c07c1f,
	0x1b00001f, 0x3fffefff, 0x1800001f, 0xc7cffff4, 0x80c01801, 0xd82001e3,
	0x17c07c1f, 0x1800001f, 0xc7cffffc, 0xf0000000, 0x17c07c1f, 0xf0000000,
	0x17c07c1f, 0xf0000000, 0x17c07c1f, 0xe2e00f16, 0x1380201f, 0xe2e00f1e,
	0x1380201f, 0xe2e00f0e, 0x1380201f, 0xe2e00f0c, 0xe2e00f0d, 0xe2e00e0d,
	0xe2e00c0d, 0xe2e0080d, 0xe2e0000d, 0xf0000000, 0x17c07c1f, 0xe2e0010d,
	0xe2e0030d, 0xe2e0070d, 0xe2e00f0d, 0xe2e00f1e, 0xe2e00f12, 0xf0000000,
	0x17c07c1f, 0xd800060a, 0x17c07c1f, 0xe2e00036, 0xe2e0003e, 0xe2e0002e,
	0xd82006ca, 0x17c07c1f, 0xe2e0006e, 0xe2e0004e, 0xe2e0004c, 0xe2e0004d,
	0xf0000000, 0x17c07c1f, 0xd800078a, 0x17c07c1f, 0xe2e0006d, 0xe2e0002d,
	0xd820082a, 0x17c07c1f, 0xe2e0002f, 0xe2e0003e, 0xe2e00032, 0xf0000000,
	0x17c07c1f, 0xa1d10407, 0x1b80001f, 0x20000014, 0x12c07c1f, 0xf0000000,
	0x17c07c1f, 0xa1d08407, 0x1b80001f, 0x20000280, 0x82eab401, 0x1a00001f,
	0x10006814, 0xe200000b, 0xf0000000, 0x17c07c1f, 0x12802c1f, 0x82ed3401,
	0xd8200b2a, 0x17c07c1f, 0x1a00001f, 0x10006814, 0xe200000b, 0xf0000000,
	0x17c07c1f, 0x1a00001f, 0x10006604, 0xd8000ceb, 0x17c07c1f, 0xe2200005,
	0x1b80001f, 0x20000020, 0xe2200007, 0x1b80001f, 0x20000020, 0xd8200dab,
	0x17c07c1f, 0xe2200004, 0x1b80001f, 0x20000020, 0xe2200006, 0x1b80001f,
	0x20000020, 0xf0000000, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001,
	0x18c0001f, 0x10200114, 0x1910001f, 0x10200114, 0xa9000004, 0x00000010,
	0xe0c00004, 0xa1d48407, 0x1b00001f, 0x3fffe7ff, 0x1b80001f, 0xd00f0000,
	0x8880000c, 0x3effe7ff, 0xd80038c2, 0x1140041f, 0x809c040c, 0xd8002142,
	0x8098040d, 0xe8208000, 0x10006354, 0xfffffffd, 0xc0c00860, 0x17c07c1f,
	0x1950001f, 0x10006400, 0x80d70405, 0xd8002763, 0x17c07c1f, 0x89c00007,
	0xffffefff, 0x18c0001f, 0x10006200, 0xc0c00700, 0x12807c1f, 0xe8208000,
	0x1000625c, 0x00000001, 0x1b80001f, 0x20000020, 0xc0c00700, 0x1280041f,
	0x18c0001f, 0x10006208, 0xc0c00700, 0x12807c1f, 0xe8208000, 0x10006248,
	0x00000000, 0x1b80001f, 0x20000020, 0xc0c00700, 0x1280041f, 0x18c0001f,
	0x10006290, 0xc0c00700, 0x1280041f, 0x1b00001f, 0xffffffff, 0x1b80001f,
	0xd0010000, 0x8880000c, 0x3fffe7ff, 0xd8003462, 0x17c07c1f, 0x8880000c,
	0x40000800, 0xd8002762, 0x17c07c1f, 0x1990001f, 0x10006600, 0x1880001f,
	0x10006320, 0x80c01801, 0x81009801, 0xa1d40407, 0x1b80001f, 0x20000014,
	0xa1d90407, 0xe8208000, 0x10006354, 0xfffffffd, 0xc0c00860, 0xe080000f,
	0xd8002763, 0x17c07c1f, 0xc0c00920, 0xe080000f, 0xd8002763, 0x17c07c1f,
	0xe080001f, 0xe8208000, 0x10006310, 0x0b160038, 0x19c0001f, 0x00240b27,
	0x1800001f, 0x00000094, 0x1800001f, 0x00013e94, 0x1800001f, 0x87c1be94,
	0x1800001f, 0xc7cfff94, 0xc0c00b60, 0x10c07c1f, 0x80c01801, 0xd8202e23,
	0x17c07c1f, 0x1800001f, 0xc7cf1f3c, 0x1b00001f, 0x3fffe7ff, 0x1b80001f,
	0x90100000, 0x80881c01, 0xd8003222, 0x17c07c1f, 0x1800001f, 0xc7cfff34,
	0x19c0001f, 0x00240b07, 0xc0c00b60, 0x10c0041f, 0x1800001f, 0x87c0bf14,
	0x1b80001f, 0x20000424, 0x1800001f, 0x04403e14, 0x1b80001f, 0x20000424,
	0x1800001f, 0x04003e14, 0x1b80001f, 0x20000424, 0x1800001f, 0x00000014,
	0x1b80001f, 0x20000424, 0x10007c1f, 0xd8203382, 0x17c07c1f, 0x1800001f,
	0x8380be10, 0x1b80001f, 0x20000424, 0x1800001f, 0x00003e10, 0x1b80001f,
	0x20000424, 0x1800001f, 0x00000010, 0x10007c1f, 0x19c0001f, 0x00250b04,
	0x1b80001f, 0x20000a50, 0xe8208000, 0x10006354, 0xffff00fd, 0x19c0001f,
	0x00210b04, 0x19c0001f, 0x00210a04, 0x1950001f, 0x10006400, 0x80d70405,
	0xd80038c3, 0x17c07c1f, 0x18c0001f, 0x10006290, 0xc0c00560, 0x1280041f,
	0x18c0001f, 0x10006208, 0xc0c00560, 0x12807c1f, 0xe8208000, 0x10006248,
	0x00000001, 0x1b80001f, 0x20000080, 0xc0c00560, 0x1280041f, 0x18c0001f,
	0x10006200, 0xc0c00560, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000000,
	0x1b80001f, 0x20000080, 0xc0c00560, 0x1280041f, 0x19c0001f, 0x00211800,
	0x1ac0001f, 0x55aa55aa, 0x10007c1f, 0xf0000000
};

#define PCM_DPIDLE_BASE         __pa(spm_pcm_dpidle)
#define PCM_DPIDLE_LEN          (460 - 1)
#define PCM_DPIDLE_VEC0         EVENT_VEC(11, 1, 0, 0)	/* 26M-wake event */
#define PCM_DPIDLE_VEC1         EVENT_VEC(12, 1, 0, 6)	/* 26M-sleep event */
#define PCM_DPIDLE_VEC2         EVENT_VEC(30, 1, 0, 17)	/* APSRC-wake event */
#define PCM_DPIDLE_VEC3         EVENT_VEC(31, 1, 0, 19)	/* APSRC-sleep event */

/**********************************************************
 * E2 PCM code for deep idle (v3rc8 @ 2013-11-07)
 **********************************************************/
static const u32 E2_spm_pcm_dpidle[] = {
	0x1800001f, 0xc7cfff94, 0x1b00001f, 0x3ffff7ff, 0xf0000000, 0x17c07c1f,
	0x1b00001f, 0x3fffefff, 0x1800001f, 0xc7cffff4, 0x80c01801, 0xd82001e3,
	0x17c07c1f, 0x1800001f, 0xc7cffffc, 0xf0000000, 0x17c07c1f, 0xf0000000,
	0x17c07c1f, 0xf0000000, 0x17c07c1f, 0xe2e00f16, 0x1380201f, 0xe2e00f1e,
	0x1380201f, 0xe2e00f0e, 0x1380201f, 0xe2e00f0c, 0xe2e00f0d, 0xe2e00e0d,
	0xe2e00c0d, 0xe2e0080d, 0xe2e0000d, 0xf0000000, 0x17c07c1f, 0xe2e0010d,
	0xe2e0030d, 0xe2e0070d, 0xe2e00f0d, 0xe2e00f1e, 0xe2e00f12, 0xf0000000,
	0x17c07c1f, 0xd800060a, 0x17c07c1f, 0xe2e00036, 0xe2e0003e, 0xe2e0002e,
	0xd82006ca, 0x17c07c1f, 0xe2e0006e, 0xe2e0004e, 0xe2e0004c, 0xe2e0004d,
	0xf0000000, 0x17c07c1f, 0xd800078a, 0x17c07c1f, 0xe2e0006d, 0xe2e0002d,
	0xd820082a, 0x17c07c1f, 0xe2e0002f, 0xe2e0003e, 0xe2e00032, 0xf0000000,
	0x17c07c1f, 0xa1d10407, 0x1b80001f, 0x20000014, 0x12c07c1f, 0xf0000000,
	0x17c07c1f, 0xa1d08407, 0x1b80001f, 0x20000280, 0x82eab401, 0x1a00001f,
	0x10006814, 0xe200000b, 0xf0000000, 0x17c07c1f, 0x12802c1f, 0x82ed3401,
	0xd8200b2a, 0x17c07c1f, 0x1a00001f, 0x10006814, 0xe200000b, 0xf0000000,
	0x17c07c1f, 0x1a00001f, 0x10006604, 0xd8000ceb, 0x17c07c1f, 0xe2200005,
	0x1b80001f, 0x2000009c, 0xe2200007, 0x1b80001f, 0x2000009c, 0xd8200dab,
	0x17c07c1f, 0xe2200004, 0x1b80001f, 0x2000009c, 0xe2200006, 0x1b80001f,
	0x2000009c, 0xf0000000, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001,
	0xa1d48407, 0x18c0001f, 0x10001014, 0x1910001f, 0x10001014, 0x81200404,
	0xe0c00004, 0x1910001f, 0x10001014, 0x18c0001f, 0x10006608, 0x1910001f,
	0x10006608, 0xa1128404, 0xa1130404, 0xe0c00004, 0x1910001f, 0x10006608,
	0x1b80001f, 0x500f0000, 0x18c0001f, 0x10200114, 0x1910001f, 0x10200114,
	0xa9000004, 0x00000010, 0xe0c00004, 0x1910001f, 0x10200114, 0x18c0001f,
	0x10006608, 0x1910001f, 0x10006608, 0x81328404, 0x81330404, 0xe0c00004,
	0x1910001f, 0x10006608, 0x1b00001f, 0x3fffe7ff, 0x1b80001f, 0x500f0000,
	0x809c040c, 0xd8002502, 0x8098040d, 0xe8208000, 0x10006354, 0xfffffffd,
	0xc0c00860, 0x17c07c1f, 0x18c0001f, 0x10001000, 0x1910001f, 0x10001000,
	0x89000004, 0xfffffff3, 0xe0c00004, 0x1910001f, 0x10001000, 0x1b80001f,
	0x20000100, 0x1950001f, 0x10006400, 0x80d70405, 0xd8002c03, 0x17c07c1f,
	0x89c00007, 0xffffefff, 0x18c0001f, 0x10006200, 0xc0c00700, 0x12807c1f,
	0xe8208000, 0x1000625c, 0x00000001, 0x1b80001f, 0x20000020, 0xc0c00700,
	0x1280041f, 0x18c0001f, 0x10006208, 0xc0c00700, 0x12807c1f, 0xe8208000,
	0x10006248, 0x00000000, 0x1b80001f, 0x20000020, 0xc0c00700, 0x1280041f,
	0x18c0001f, 0x10006290, 0xc0c00700, 0x1280041f, 0x1b00001f, 0xffffffff,
	0x1b80001f, 0xd0010000, 0x8880000c, 0x3fffe7ff, 0xd80038c2, 0x17c07c1f,
	0x8880000c, 0x40000800, 0xd8002c02, 0x17c07c1f, 0x1990001f, 0x10006600,
	0x1880001f, 0x10006320, 0x80c01801, 0x81009801, 0xa1d40407, 0x1b80001f,
	0x20000014, 0xa1d90407, 0xe8208000, 0x10006354, 0xfffffffd, 0xc0c00860,
	0xe080000f, 0xd8002c03, 0x17c07c1f, 0xc0c00920, 0xe080000f, 0xd8002c03,
	0x17c07c1f, 0xe080001f, 0xe8208000, 0x10006310, 0x0b160038, 0x19c0001f,
	0x00240b27, 0x1800001f, 0x00000094, 0x1800001f, 0x00003f94, 0x1800001f,
	0x87c0bf94, 0x1800001f, 0xc7cfff94, 0xc0c00b60, 0x10c07c1f, 0x80c01801,
	0xd82032c3, 0x17c07c1f, 0x1800001f, 0xc7cf1f3c, 0x1b00001f, 0x3fffe7ff,
	0x1b80001f, 0x90100000, 0x80881c01, 0xd80036c2, 0x17c07c1f, 0x1800001f,
	0xc7cfff34, 0x19c0001f, 0x00240b07, 0xc0c00b60, 0x10c0041f, 0x1800001f,
	0x87c0bf14, 0x1b80001f, 0x20000424, 0x1800001f, 0x04003f14, 0x1b80001f,
	0x20000424, 0x1800001f, 0x00003f14, 0x1b80001f, 0x20000424, 0x1800001f,
	0x00000014, 0x1b80001f, 0x20000424, 0x10007c1f, 0xd8203822, 0x17c07c1f,
	0x1800001f, 0x8380be10, 0x1b80001f, 0x20000424, 0x1800001f, 0x00003e10,
	0x1b80001f, 0x20000424, 0x1800001f, 0x00000010, 0x10007c1f, 0x19c0001f,
	0x00250b04, 0xe8208000, 0x10006354, 0xffff00fd, 0x19c0001f, 0x00210b04,
	0x19c0001f, 0x00210a04, 0x1950001f, 0x10006400, 0x80d70405, 0xd8003d23,
	0x17c07c1f, 0x18c0001f, 0x10006290, 0xc0c00560, 0x1280041f, 0x18c0001f,
	0x10006208, 0xc0c00560, 0x12807c1f, 0xe8208000, 0x10006248, 0x00000001,
	0x1b80001f, 0x20000080, 0xc0c00560, 0x1280041f, 0x18c0001f, 0x10006200,
	0xc0c00560, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000000, 0x1b80001f,
	0x20000080, 0xc0c00560, 0x1280041f, 0x19c0001f, 0x00211800, 0x1ac0001f,
	0x55aa55aa, 0x10007c1f, 0x18c0001f, 0x10001000, 0x1910001f, 0x10001000,
	0xa1110404, 0xe0c00004, 0x1910001f, 0x10001000, 0x1b80001f, 0x20000100,
	0x18c0001f, 0x10001014, 0x1910001f, 0x10001014, 0xa1000404, 0xe0c00004,
	0x1910001f, 0x10001014, 0xf0000000
};

#define E2_PCM_DPIDLE_BASE         __pa(E2_spm_pcm_dpidle)
#define E2_PCM_DPIDLE_LEN          (513 - 1)
#define E2_PCM_DPIDLE_VEC0         EVENT_VEC(11, 1, 0, 0)	/* 26M-wake event */
#define E2_PCM_DPIDLE_VEC1         EVENT_VEC(12, 1, 0, 6)	/* 26M-sleep event */
#define E2_PCM_DPIDLE_VEC2         EVENT_VEC(30, 1, 0, 17)	/* APSRC-wake event */
#define E2_PCM_DPIDLE_VEC3         EVENT_VEC(31, 1, 0, 19)	/* APSRC-sleep event */

/**************************************
 * SW code for suspend and deep idle
 **************************************/
#define SPM_SYSCLK_SETTLE       128	/* 3.9ms */

#define WAIT_UART_ACK_TIMES     80	/* 80 * 10us */

#define SPM_WAKE_PERIOD         600	/* sec */

#define WAKE_SRC_FOR_SUSPEND                                                \
	(WAKE_SRC_EINT |	                                        \
	WAKE_SRC_USB0_CD | WAKE_SRC_USB1_CD | WAKE_SRC_THERM |                 \
	WAKE_SRC_PWRAP | WAKE_SRC_THERM2 | WAKE_SRC_SYSPWREQ | WAKE_SRC_KP)

#define WAKE_SRC_FOR_DPIDLE                                                 \
	(WAKE_SRC_GPT | WAKE_SRC_EINT |                           \
	WAKE_SRC_USB0_CD | WAKE_SRC_USB1_CD |                                  \
	WAKE_SRC_USB1_PDN | WAKE_SRC_USB0_PDN | WAKE_SRC_AFE |                 \
	WAKE_SRC_THERM | WAKE_SRC_PWRAP | WAKE_SRC_THERM2 | WAKE_SRC_SYSPWREQ | WAKE_SRC_KP)

#define wfi_with_sync()                         \
do {                                            \
    isb();                                      \
    dsb();                                      \
    __asm__ __volatile__("wfi" : : : "memory"); \
} while (0)

#define spm_crit2(fmt, args...)     \
do {                                \
    aee_sram_printk(fmt, ##args);   \
    spm_crit(fmt, ##args);          \
} while (0)

#define spm_error2(fmt, args...)    \
do {                                \
    aee_sram_printk(fmt, ##args);   \
    spm_error(fmt, ##args);         \
} while (0)

typedef struct {
	u32 debug_reg;		/* PCM_REG_DATA_INI */
	u32 r12;		/* PCM_REG12_DATA */
	u32 raw_sta;		/* SLEEP_ISR_RAW_STA */
	u32 cpu_wake;		/* SLEEP_CPU_WAKEUP_EVENT */
	u32 timer_out;		/* PCM_TIMER_OUT */
	u32 event_reg;		/* PCM_EVENT_REG_STA */
	u32 isr;		/* SLEEP_ISR_STATUS */
	u32 r13;		/* PCM_REG13_DATA */
} wake_status_t;

/* extern void mtk_wdt_suspend(void); */
/* extern void mtk_wdt_resume(void); */

u32 spm_sleep_wakesrc = WAKE_SRC_FOR_SUSPEND;

static void spm_set_sysclk_settle(void)
{
	u32 settle;

	/* SYSCLK settle = VTCXO settle time */
	spm_write(SPM_CLK_SETTLE, SPM_SYSCLK_SETTLE);
	settle = spm_read(SPM_CLK_SETTLE);

	spm_crit2("settle = %u\n", settle);
}

static void spm_reset_and_init_pcm(void)
{
	/* reset PCM */
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY | CON0_PCM_SW_RESET);

	/* wait until normal SPM code finish reset */
	while ((spm_read(SPM_PCM_FSM_STA) & 0x0003FFF0) != 0x00008490)
		;
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY);

	/* init PCM control register (disable event vector and PCM timer) */
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY | CON0_IM_SLEEP_DVS);
	spm_write(SPM_PCM_CON1, CON1_CFG_KEY | CON1_IM_NONRP_EN | CON1_MIF_APBEN);
}

/*
 * code_base: SPM_PCM_XXX_BASE
 * code_len : SPM_PCM_XXX_LEN
 */
static void spm_kick_im_to_fetch(u32 code_base, u16 code_len)
{
	u32 con0;

	/* tell IM where is PCM code */
	BUG_ON(code_base & 0x00000003);	/* check 4-byte alignment */
	spm_write(SPM_PCM_IM_PTR, code_base);
	spm_write(SPM_PCM_IM_LEN, code_len);

	/* kick IM to fetch (only toggle IM_KICK) */
	con0 = spm_read(SPM_PCM_CON0) & ~(CON0_IM_KICK | CON0_PCM_KICK);
	spm_write(SPM_PCM_CON0, con0 | CON0_CFG_KEY | CON0_IM_KICK);
	spm_write(SPM_PCM_CON0, con0 | CON0_CFG_KEY);
}

static int spm_request_uart_to_sleep(void)
{
	u32 val1;
	int i = 0;

	/* request UART to sleep */
	val1 = spm_read(SPM_POWER_ON_VAL1);
	spm_write(SPM_POWER_ON_VAL1, val1 | R7_UART_CLK_OFF_REQ);

	/* wait for UART to ACK */
	while (!(spm_read(SPM_PCM_REG13_DATA) & R13_UART_CLK_OFF_ACK)) {
		if (i++ >= WAIT_UART_ACK_TIMES) {
			spm_write(SPM_POWER_ON_VAL1, val1);
			spm_error2("CANNOT GET UART SLEEP ACK (0x%x)\n", spm_read(PERI_PDN0_STA));
			dump_uart_reg();
			return -EBUSY;
		}
		udelay(10);
	}

	return 0;
}

static void spm_init_pcm_register(void)
{
	/* init r0 with POWER_ON_VAL0 */
	spm_write(SPM_PCM_REG_DATA_INI, spm_read(SPM_POWER_ON_VAL0));
	spm_write(SPM_PCM_PWR_IO_EN, PCM_RF_SYNC_R0);
	spm_write(SPM_PCM_PWR_IO_EN, 0);

	/* init r7 with POWER_ON_VAL1 */
	spm_write(SPM_PCM_REG_DATA_INI, spm_read(SPM_POWER_ON_VAL1));
	spm_write(SPM_PCM_PWR_IO_EN, PCM_RF_SYNC_R7);
	spm_write(SPM_PCM_PWR_IO_EN, 0);

	/* clear REG_DATA_INI for PCM after init rX */
	spm_write(SPM_PCM_REG_DATA_INI, 0);
}

/*
 * vec0_cfg: SPM_PCM_XXX_VEC0 or 0
 * vec1_cfg: SPM_PCM_XXX_VEC1 or 0
 * vec2_cfg: SPM_PCM_XXX_VEC2 or 0
 * vec3_cfg: SPM_PCM_XXX_VEC3 or 0
 * vec4_cfg: SPM_PCM_XXX_VEC4 or 0
 * vec5_cfg: SPM_PCM_XXX_VEC5 or 0
 * vec6_cfg: SPM_PCM_XXX_VEC6 or 0
 * vec7_cfg: SPM_PCM_XXX_VEC7 or 0
 */
static void spm_init_event_vector(u32 vec0_cfg, u32 vec1_cfg, u32 vec2_cfg, u32 vec3_cfg,
				  u32 vec4_cfg, u32 vec5_cfg, u32 vec6_cfg, u32 vec7_cfg)
{
	/* init event vector register */
	spm_write(SPM_PCM_EVENT_VECTOR0, vec0_cfg);
	spm_write(SPM_PCM_EVENT_VECTOR1, vec1_cfg);
	spm_write(SPM_PCM_EVENT_VECTOR2, vec2_cfg);
	spm_write(SPM_PCM_EVENT_VECTOR3, vec3_cfg);
	spm_write(SPM_PCM_EVENT_VECTOR4, vec4_cfg);
	spm_write(SPM_PCM_EVENT_VECTOR5, vec5_cfg);
	spm_write(SPM_PCM_EVENT_VECTOR6, vec6_cfg);
	spm_write(SPM_PCM_EVENT_VECTOR7, vec7_cfg);

	/* event vector will be enabled by PCM itself */
}

static void spm_set_pwrctl_for_sleep(void)
{
	spm_write(SPM_APMCU_PWRCTL, 0);

	spm_write(SPM_AP_STANBY_CON, (0x0 << 16) |	/* unmask DISP and MFG */
		  (0 << 6) |	/* check SCU idle */
		  (0 << 5) |	/* check L2C idle */
		  (1U << 4));	/* Reduce AND */
	spm_write(SPM_CORE0_WFI_SEL, 0x1);
	spm_write(SPM_CORE1_WFI_SEL, 0x1);
	spm_write(SPM_CORE2_WFI_SEL, 0x1);
	spm_write(SPM_CORE3_WFI_SEL, 0x1);
}

static void spm_set_pwrctl_for_dpidle(u8 pwrlevel)
{
	if (pwrlevel > 1)
		pwrlevel = 1;
	spm_write(SPM_APMCU_PWRCTL, 1U << pwrlevel);

	spm_write(SPM_AP_STANBY_CON, (0x0 << 16) |	/* unmask DISP and MFG */
		  (0 << 6) |	/* check SCU idle */
		  (0 << 5) |	/* check L2C idle */
		  (1U << 4));	/* Reduce AND */
	spm_write(SPM_CORE0_WFI_SEL, 0x1);
	spm_write(SPM_CORE1_WFI_SEL, 0x1);
	spm_write(SPM_CORE2_WFI_SEL, 0x1);
	spm_write(SPM_CORE3_WFI_SEL, 0x1);
}

/*
 * timer_val: PCM timer value (0 = disable)
 * wake_src : WAKE_SRC_XXX
 */
static void spm_set_wakeup_event(u32 timer_val, u32 wake_src)
{
	/* set PCM timer (set to max when disable) */
	spm_write(SPM_PCM_TIMER_VAL, timer_val ? : 0xffffffff);
	spm_write(SPM_PCM_CON1, spm_read(SPM_PCM_CON1) | CON1_CFG_KEY | CON1_PCM_TIMER_EN);

	/* unmask wakeup source */
#if SPM_BYPASS_SYSPWREQ
	wake_src &= ~WAKE_SRC_SYSPWREQ;	/* make 26M off when attach ICE */
#endif
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, ~wake_src);

	/* unmask SPM ISR */
	spm_write(SPM_SLEEP_ISR_MASK, 0x0E04);
}

static void spm_kick_pcm_to_run(bool cpu_pdn, bool infra_pdn)
{
	u32 clk, con0;

	/* keep CPU or INFRA/DDRPHY power if needed and lock INFRA DCM */
	clk = spm_read(SPM_CLK_CON) & ~(CC_DISABLE_DORM_PWR | CC_DISABLE_INFRA_PWR);
	if (!cpu_pdn)
		clk |= CC_DISABLE_DORM_PWR;
	if (!infra_pdn)
		clk |= CC_DISABLE_INFRA_PWR;
	spm_write(SPM_CLK_CON, clk | CC_LOCK_INFRA_DCM);

	/* init pause request mask for PCM */
	spm_write(SPM_PCM_MAS_PAUSE_MASK, 0xffff00fd /*0xffffffff */);

	/* enable r0 and r7 to control power */
	spm_write(SPM_PCM_PWR_IO_EN, PCM_PWRIO_EN_R0 | PCM_PWRIO_EN_R7 |
		  PCM_PWRIO_EN_R1 | PCM_PWRIO_EN_R2);

/* spm_write(SPM_PCM_SRC_REQ, SRC_PCM_F26M_REQ); //SRCLKVOLTEN won't toggle with SRCLKENAI */
	/* kick PCM to run (only toggle PCM_KICK) */
	con0 = spm_read(SPM_PCM_CON0) & ~(CON0_IM_KICK | CON0_PCM_KICK);
	spm_write(SPM_PCM_CON0, con0 | CON0_CFG_KEY | CON0_PCM_KICK);
	spm_write(SPM_PCM_CON0, con0 | CON0_CFG_KEY);
}

static void spm_trigger_wfi_for_sleep(bool cpu_pdn, bool infra_pdn)
{
	if (1) {
		if (!cpu_power_down(SHUTDOWN_MODE)) {
			switch_to_amp();
			wfi_with_sync();
		}
		switch_to_smp();
		cpu_check_dormant_abort();	/* TODO: check if dormant API changed */
	} else {
/* mci_snoop_sleep(); //FIXME : use CCI snoop */
		wfi_with_sync();
/* mci_snoop_restore(); //FIXME : use CCI snoop */
	}

	if (infra_pdn) {
		mtk_uart_restore();	/* TODO: check why dpidle case don't need to do mtk_uart_restore, it also request uart sleep */
	}
}

static void spm_trigger_wfi_for_dpidle(bool cpu_pdn)
{
	/* pmicspi_mempll2clksq();     *//* PCM will do this */

	if (cpu_pdn) {
/* if (!cpu_power_down(DORMANT_MODE)) { */
		if (!cpu_power_down(SHUTDOWN_MODE)) {
			switch_to_amp();
			wfi_with_sync();
		}
		switch_to_smp();
		cpu_check_dormant_abort();
	} else {
/* mci_snoop_sleep(); //FIXME : use CCI snoop */
		wfi_with_sync();
/* mci_snoop_restore(); //FIXME : use CCI snoop */
		spm_write(0xF0200114, spm_read(0xF0200114) & ~0x10);
	}

	/* pmicspi_clksq2mempll();     *//* PCM has done this */
}

static void spm_get_wakeup_status(wake_status_t *wakesta)
{
	/* get PC value if PCM assert (pause abort) */
	wakesta->debug_reg = spm_read(SPM_PCM_REG_DATA_INI);

	/* get wakeup event */
	wakesta->r12 = spm_read(SPM_PCM_REG12_DATA);
	wakesta->raw_sta = spm_read(SPM_SLEEP_ISR_RAW_STA);
	wakesta->cpu_wake = spm_read(SPM_SLEEP_CPU_WAKEUP_EVENT);

	/* get sleep time */
	wakesta->timer_out = spm_read(SPM_PCM_TIMER_OUT);

	/* get special pattern if sleep abort */
	wakesta->event_reg = spm_read(SPM_PCM_EVENT_REG_STA);

	/* get ISR status */
	wakesta->isr = spm_read(SPM_SLEEP_ISR_STATUS);

	/* get MD related status */
	wakesta->r13 = spm_read(SPM_PCM_REG13_DATA);
}

static void spm_clean_after_wakeup(void)
{
	/* PCM has cleared uart_clk_off_req and now clear it in POWER_ON_VAL1 */
	spm_write(SPM_POWER_ON_VAL1, spm_read(SPM_POWER_ON_VAL1) & ~R7_UART_CLK_OFF_REQ);

	/* re-enable POWER_ON_VAL0/1 to control power */
	spm_write(SPM_PCM_PWR_IO_EN, 0);

	/* unlock INFRA DCM */
	spm_write(SPM_CLK_CON, spm_read(SPM_CLK_CON) & ~CC_LOCK_INFRA_DCM);

	/* clean PCM timer event */
	spm_write(SPM_PCM_CON1, CON1_CFG_KEY | (spm_read(SPM_PCM_CON1) & ~CON1_PCM_TIMER_EN));

	/* clean CPU wakeup event (pause abort) */
	spm_write(SPM_SLEEP_CPU_WAKEUP_EVENT, 0);

	/* clean wakeup event raw status */
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, 0xffffffff);

	/* clean ISR status */
	spm_write(SPM_SLEEP_ISR_MASK, 0x0F0C);	/* PCM_ISR_ROOT_MASK, PCM_TWAM_ISR_MASK */
	spm_write(SPM_SLEEP_ISR_STATUS, 0x000C);	/* ISR_PCM, ISR_TWAM_IRQ */
	spm_write(SPM_PCM_SW_INT_CLEAR, 0x000F);
}

static wake_reason_t spm_output_wake_reason(wake_status_t *wakesta, bool dpidle)
{
	char *str = "<NONE>";
	wake_reason_t wr = WR_NONE;

	if (wakesta->debug_reg != 0) {
		spm_error2("PCM ASSERT AND PC = %u (0x%x)(0x%x)\n",
			   wakesta->debug_reg, wakesta->r13, wakesta->event_reg);
		return WR_PCM_ASSERT;
	}

	if (dpidle)		/* bypass wakeup event check */
		return WR_WAKE_SRC;

	if (wakesta->r12 & (1U << 0)) {
		if (!wakesta->cpu_wake) {
			str = "PCM_TIMER";
			if (slp_pcm_timer == 0)
				wr = WR_PCM_TIMER;
			else
				wr = WR_WAKE_SRC;
		} else {
			str = "CPU";
			wr = WR_WAKE_SRC;
		}
	}
	if (wakesta->r12 & WAKE_SRC_TS) {
		str = "TS";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_KP) {
		str = "KP";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_GPT) {
		str = "GPT";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_EINT) {
		str = "EINT";
		wr = WR_WAKE_SRC;
	}
#if 0
	if (wakesta->r12 & WAKE_SRC_CCIF_MD2) {
		str = "CCIF_MD2";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_CCIF_MD1) {
		str = "CCIF_MD1";
		wr = WR_WAKE_SRC;
	}
#endif
	if (wakesta->r12 & WAKE_SRC_LOW_BAT) {
		str = "LOW_BAT";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_USB0_CD) {
		str = "USB0_CD";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_USB1_CD) {
		str = "USB1_CD";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_USB1_PDN) {
		str = "USB1_PDN";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_USB0_PDN) {
		str = "USB0_PDN";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_DBGSYS) {
		str = "DBGSYS";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_AFE) {
		str = "AFE";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_THERM) {
		str = "THERM";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_CIRQ) {
		str = "CIRQ";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_PWRAP) {
		str = "PWRAP";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_SYSPWREQ) {
		str = "SYSPWREQ";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_THERM2) {
		str = "THERM2";
		wr = WR_WAKE_SRC;
	}
#if 0
	if (wakesta->r12 & WAKE_SRC_MD_WDT) {
		str = "MD_WDT";
		wr = WR_WAKE_SRC;
	}
#endif
	if (wakesta->r12 & WAKE_SRC_CPU0_IRQ) {
		str = "CPU0_IRQ";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_CPU1_IRQ) {
		str = "CPU1_IRQ";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_CPU2_IRQ) {
		str = "CPU2_IRQ";
		wr = WR_WAKE_SRC;
	}
	if (wakesta->r12 & WAKE_SRC_CPU3_IRQ) {
		str = "CPU3_IRQ";
		wr = WR_WAKE_SRC;
	}

	if ((wakesta->event_reg & 0x100000) == 0) {
		spm_crit2("Sleep Abort!\n");
		wr = WR_PCM_ABORT;
	}

	spm_crit2("wake up by %s(0x%x)(0x%x)(%u)(%d)\n",
		  str, wakesta->r12, wakesta->raw_sta, wakesta->timer_out,
		  (wakesta->timer_out / 32768));
	spm_crit2("event_reg = 0x%x, isr = 0x%x, r13 = 0x%x\n", wakesta->event_reg, wakesta->isr,
		  wakesta->r13);

	BUG_ON(wr == WR_NONE);

#if 0
	if (wakesta->r12 & WAKE_SRC_CCIF_MD2)
		exec_ccci_kern_func_by_md_id(1, ID_GET_MD_WAKEUP_SRC, NULL, 0);
	if (wakesta->r12 & WAKE_SRC_CCIF_MD1)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC, NULL, 0);
#endif
	return wr;
}

#if SPM_PWAKE_EN
static u32 spm_get_wake_period(wake_reason_t last_wr)
{
	int period = SPM_WAKE_PERIOD;

#if 1
	/* use FG to get the period of 1% battery decrease */
	period = get_dynamic_period(last_wr != WR_PCM_TIMER ? 1 : 0, SPM_WAKE_PERIOD, 1);
	if (period <= 0) {
		spm_warning("CANNOT GET PERIOD FROM FUEL GAUGE\n");
		period = SPM_WAKE_PERIOD;
	} else if (period > 36 * 3600) {	/* max period is 36.4 hours */
		period = 36 * 3600;
	}
#endif

	if (slp_pcm_timer != 0) {
		spm_crit2("PCM timer has been overwritten (%d)!\n", slp_pcm_timer);
		period = slp_pcm_timer;
	}

	return period;
}
#endif

/*
 * wakesrc: WAKE_SRC_XXX
 * enable : enable or disable @wakesrc
 * replace: if true, will replace the default setting
 */
int spm_set_sleep_wakesrc(u32 wakesrc, bool enable, bool replace)
{
	unsigned long flags;

	if (spm_is_wakesrc_invalid(wakesrc))
		return -EINVAL;

	spin_lock_irqsave(&spm_lock, flags);
	if (enable) {
		if (replace)
			spm_sleep_wakesrc = wakesrc;
		else
			spm_sleep_wakesrc |= wakesrc;
	} else {
		if (replace)
			spm_sleep_wakesrc = 0;
		else
			spm_sleep_wakesrc &= ~wakesrc;
	}
	spin_unlock_irqrestore(&spm_lock, flags);

	return 0;
}

void spm_set_wakeup_event_map(struct mt_wake_event_map *tbl)
{
	mt_wake_event_tbl = tbl;
}

static struct mt_wake_event_map *spm_map_wakeup_event(struct mt_wake_event *mt_we)
{
	/* map proprietary mt_wake_event to wake_source_t */
	struct mt_wake_event_map *tbl = mt_wake_event_tbl;
	if (!tbl || !mt_we)
		return NULL;
	for (; tbl->domain; tbl++) {
		if (!strcmp(tbl->domain, mt_we->domain) && tbl->code == mt_we->code)
			return tbl;
	}
	return NULL;
}

wakeup_event_t spm_read_wakeup_event_and_irq(int *pirq)
{
	struct mt_wake_event_map *tbl =
		spm_map_wakeup_event(spm_get_wakeup_event());

	if (!tbl)
		return WEV_NONE;

	if (pirq)
		*pirq = tbl->irq;
	return tbl->we;
}
EXPORT_SYMBOL(spm_read_wakeup_event_and_irq);

void spm_report_wakeup_event(struct mt_wake_event *we, int code)
{
	unsigned long flags;
	struct mt_wake_event *head;
	struct mt_wake_event_map *evt;

	static char *ev_desc[] = {
		"RTC", "WIFI", "WAN", "USB",
		"PWR", "HALL", "BT", "CHARGER",
	};

	spin_lock_irqsave(&spm_lock, flags);
	head = mt_wake_event;
	mt_wake_event = we;
	we->parent = head;
	we->code = code;
	mt_wake_event = we;
	spin_unlock_irqrestore(&spm_lock, flags);
	pr_err("%s: WAKE EVT: %s#%d (parent %s#%d)\n",
		__func__, we->domain, we->code,
		head ? head->domain : "NONE",
		head ? head->code : -1);
	evt = spm_map_wakeup_event(we);
	if (evt && evt->we != WEV_NONE) {
		char *name = (evt->we >= 0 && evt->we < ARRAY_SIZE(ev_desc)) ? ev_desc[evt->we] : "UNKNOWN";
		pm_report_resume_irq(evt->irq);
		pr_err("%s: WAKEUP from source %d [%s]\n",
			__func__, evt->we, name);
	}
}
EXPORT_SYMBOL(spm_report_wakeup_event);

void spm_clear_wakeup_event(void)
{
	mt_wake_event = NULL;
}
EXPORT_SYMBOL(spm_clear_wakeup_event);

wakeup_event_t irq_to_wakeup_ev(int irq)
{
	struct mt_wake_event_map *tbl = mt_wake_event_tbl;

	if (!tbl)
		return WEV_NONE;

	for (; tbl->domain; tbl++) {
		if (tbl->irq == irq)
			return tbl->we;
	}

	return WEV_MAX;
}
EXPORT_SYMBOL(irq_to_wakeup_ev);

struct mt_wake_event *spm_get_wakeup_event(void)
{
	return mt_wake_event;
}
EXPORT_SYMBOL(spm_get_wakeup_event);

static void spm_report_wake_source(u32 event_mask)
{
	int event = -1;
	u32 mask = event_mask;
	int event_count = 0;
	while (mask) {
		event = __ffs(mask);
		event_count++;
		mask &= ~(1 << event);
	}
	if (WARN_ON(event_count != 1))
		pr_err("%s: multiple wakeup events detected: %08X\n",
			__func__, event_mask);

	if (event >= 0)
		spm_report_wakeup_event(&spm_wake_event, event);
}

/*
 * cpu_pdn:
 *    true  = CPU shutdown
 *    false = CPU standby
 * infra_pdn:
 *    true  = INFRA/DDRPHY power down
 *    false = keep INFRA/DDRPHY power
 */
wake_reason_t spm_go_to_sleep(bool cpu_pdn, bool infra_pdn)
{
	u32 sec = 0;
	wake_status_t wakesta;
	unsigned long flags;
	struct mtk_irq_mask mask;
	static wake_reason_t last_wr = WR_NONE;
	int res = 0;
	struct wd_api *wd_api = NULL;
	res = get_wd_api(&wd_api);

#if SPM_PWAKE_EN
	sec = spm_get_wake_period(last_wr);
#else
	sec = slp_pcm_timer;
#endif

	/* mtk_wdt_suspend(); */
	if (res) {
		pr_info("spm_go_to_sleep, get wd api error\n");
	} else {
		wd_api->wd_suspend_notify();
	}

	if (force_infra_pdn == 0x0)
		infra_pdn = 0;
	else if (force_infra_pdn == 0x1)
		infra_pdn = 1;

	spin_lock_irqsave(&spm_lock, flags);
	mt_irq_mask_all(&mask);
	mt_irq_unmask_for_sleep(MT_SPM_IRQ0_ID);
	mt_cirq_clone_gic();
	mt_cirq_enable();

	/* make sure SPM APB register is accessible */
	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	spm_set_sysclk_settle();

#if defined(CONFIG_AMAZON_METRICS_LOG)
	/* forced trigger system_resume:off_mode metrics log */
	if (force_gpt == 1) {
		gpt_set_cmp(GPT4, 1);
		start_gpt(GPT4);
		/* wait HW GPT trigger */
		udelay(200);
		spm_sleep_wakesrc |= WAKE_SRC_GPT;
	}
#endif

	spm_crit2("sec = %u, wakesrc = 0x%x (%u)(%u)\n",
		  sec, spm_sleep_wakesrc, cpu_pdn, infra_pdn);

	spm_reset_and_init_pcm();

	spm_kick_im_to_fetch(PCM_SUSPEND_BASE, PCM_SUSPEND_LEN);

	if (spm_request_uart_to_sleep()) {
		last_wr = WR_UART_BUSY;
		goto RESTORE_IRQ;
	}

	mt_wake_event = NULL;

	spm_init_pcm_register();

	spm_init_event_vector(PCM_SUSPEND_VEC0, PCM_SUSPEND_VEC1, PCM_SUSPEND_VEC2,
			      PCM_SUSPEND_VEC3, 0, 0, 0, 0);

	spm_set_pwrctl_for_sleep();

	spm_set_wakeup_event(sec * 32768, spm_sleep_wakesrc);

	spm_kick_pcm_to_run(cpu_pdn, infra_pdn);

	spm_trigger_wfi_for_sleep(cpu_pdn, infra_pdn);

	spm_get_wakeup_status(&wakesta);

	spm_clean_after_wakeup();

	last_wr = spm_output_wake_reason(&wakesta, false);

#if defined(CONFIG_AMAZON_METRICS_LOG)
	/* forced trigger system_resume:off_mode metrics log */
	if (force_gpt == 1) {
		if (gpt_check_and_ack_irq(GPT4))
			spm_crit2("GPT4 triggered for off_mode metrics test\n");
		spm_sleep_wakesrc &= ~WAKE_SRC_GPT;
	}
#endif

 RESTORE_IRQ:
	mt_cirq_flush();
	mt_cirq_disable();
	mt_irq_mask_restore(&mask);
	spin_unlock_irqrestore(&spm_lock, flags);

	if (last_wr == WR_WAKE_SRC)
		spm_report_wake_source(wakesta.r12);

	spm_go_to_normal();	/* let PCM help to do thermal protection */

	/* mtk_wdt_resume(); */
	if (res) {
		pr_info("spm_go_to_sleep, get wd api error\n");
	} else {
		wd_api->wd_resume_notify();
	}

	return last_wr;
}

/*
 * cpu_pdn:
 *    true  = CPU dormant
 *    false = CPU standby
 * pwrlevel:
 *    0 = AXI is off
 *    1 = AXI is 26M
 */
wake_reason_t spm_go_to_sleep_dpidle(bool cpu_pdn, u8 pwrlevel)
{
	u32 sec = 0;
	wake_status_t wakesta;
	unsigned long flags;
	struct mtk_irq_mask mask;
	static wake_reason_t last_wr = WR_NONE;

	int res = 0;
	struct wd_api *wd_api = NULL;
	res = get_wd_api(&wd_api);

#if SPM_PWAKE_EN
	sec = spm_get_wake_period(last_wr);
#else
	sec = slp_pcm_timer;
#endif
	if (res) {
		pr_info("spm_go_to_sleep_dpidle, get wd api error\n");
	} else {
		wd_api->wd_suspend_notify();
	}
	/* mtk_wdt_suspend(); */

	spin_lock_irqsave(&spm_lock, flags);
	mt_irq_mask_all(&mask);
	mt_irq_unmask_for_sleep(MT_SPM_IRQ0_ID);
	mt_cirq_clone_gic();
	mt_cirq_enable();

	spm_crit2("sec = %u, wakesrc = 0x%x [%u][%u]\n", sec, spm_sleep_wakesrc, cpu_pdn, pwrlevel);

	/* make sure SPM APB register is accessible */
	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	spm_reset_and_init_pcm();

	if (mt_get_chip_sw_ver() == 0x0)	/* E1 */
		spm_kick_im_to_fetch(PCM_DPIDLE_BASE, PCM_DPIDLE_LEN);
	else			/* E2 */
		spm_kick_im_to_fetch(E2_PCM_DPIDLE_BASE, E2_PCM_DPIDLE_LEN);

	if (spm_request_uart_to_sleep()) {
		last_wr = WR_UART_BUSY;
		goto RESTORE_IRQ;
	}

	spm_init_pcm_register();

	if (mt_get_chip_sw_ver() == 0x0)	/* E1 */
		spm_init_event_vector(PCM_DPIDLE_VEC0, PCM_DPIDLE_VEC1, PCM_DPIDLE_VEC2,
				      PCM_DPIDLE_VEC3, 0, 0, 0, 0);
	else			/* E2 */
		spm_init_event_vector(E2_PCM_DPIDLE_VEC0, E2_PCM_DPIDLE_VEC1, E2_PCM_DPIDLE_VEC2,
				      E2_PCM_DPIDLE_VEC3, 0, 0, 0, 0);

	spm_set_pwrctl_for_dpidle(pwrlevel);

	spm_set_wakeup_event(sec * 32768, spm_sleep_wakesrc);

	spm_kick_pcm_to_run(cpu_pdn, false);	/* keep INFRA/DDRPHY power */

	spm_trigger_wfi_for_dpidle(cpu_pdn);

	spm_get_wakeup_status(&wakesta);

	spm_clean_after_wakeup();

	last_wr = spm_output_wake_reason(&wakesta, false);

 RESTORE_IRQ:
	mt_cirq_flush();
	mt_cirq_disable();
	mt_irq_mask_restore(&mask);
	spin_unlock_irqrestore(&spm_lock, flags);

	spm_go_to_normal();	/* let PCM help to do thermal protection */

	/* mtk_wdt_resume(); */
	if (res) {
		pr_info("spm_go_to_sleep, get wd api error\n");
	} else {
		wd_api->wd_resume_notify();
	}
	return last_wr;
}

bool __attribute__ ((weak)) spm_dpidle_can_enter(void)
{
	return true;
}

void __attribute__ ((weak)) spm_dpidle_before_wfi(void)
{
}

void __attribute__ ((weak)) spm_dpidle_after_wfi(void)
{
}

/*
 * cpu_pdn:
 *    true  = CPU dormant
 *    false = CPU standby
 * pwrlevel:
 *    0 = AXI is off
 *    1 = AXI is 26M
 */
wake_reason_t spm_go_to_dpidle(bool cpu_pdn, u8 pwrlevel)
{
	wake_status_t wakesta;
	unsigned long flags;
	struct mtk_irq_mask mask;
	wake_reason_t wr = WR_NONE;

	spin_lock_irqsave(&spm_lock, flags);
	mt_irq_mask_all(&mask);
	mt_irq_unmask_for_sleep(MT_SPM_IRQ0_ID);
/* mt_cirq_clone_gic(); */
/* mt_cirq_enable(); */

	/* make sure SPM APB register is accessible */
	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	spm_reset_and_init_pcm();

	if (mt_get_chip_sw_ver() == 0x0)	/* E1 */
		spm_kick_im_to_fetch(PCM_DPIDLE_BASE, PCM_DPIDLE_LEN);
	else			/* E2 */
		spm_kick_im_to_fetch(E2_PCM_DPIDLE_BASE, E2_PCM_DPIDLE_LEN);

	if (spm_request_uart_to_sleep()) {
		wr = WR_UART_BUSY;
		goto RESTORE_IRQ;
	}

	spm_init_pcm_register();

	if (mt_get_chip_sw_ver() == 0x0)	/* E1 */
		spm_init_event_vector(PCM_DPIDLE_VEC0, PCM_DPIDLE_VEC1, PCM_DPIDLE_VEC2,
				      PCM_DPIDLE_VEC3, 0, 0, 0, 0);
	else			/* E2 */
		spm_init_event_vector(E2_PCM_DPIDLE_VEC0, E2_PCM_DPIDLE_VEC1, E2_PCM_DPIDLE_VEC2,
				      E2_PCM_DPIDLE_VEC3, 0, 0, 0, 0);

	spm_set_pwrctl_for_dpidle(pwrlevel);

	spm_set_wakeup_event(6553, WAKE_SRC_FOR_DPIDLE);	/* 200ms */

	if (spm_dpidle_can_enter()) {
		spm_kick_pcm_to_run(cpu_pdn, false);	/* keep INFRA/DDRPHY power */

		spm_dpidle_before_wfi();

		spm_trigger_wfi_for_dpidle(cpu_pdn);

		spm_dpidle_after_wfi();

		spm_get_wakeup_status(&wakesta);

		spm_clean_after_wakeup();

		wr = spm_output_wake_reason(&wakesta, true);
	} else {
		spm_clean_after_wakeup();

		wr = WR_SW_ABORT;
	}

 RESTORE_IRQ:
/* mt_cirq_flush(); */
/* mt_cirq_disable(); */
	mt_irq_mask_restore(&mask);
	spin_unlock_irqrestore(&spm_lock, flags);

	spm_go_to_normal();	/* let PCM help to do thermal protection */

	return wr;
}

#if 0
bool spm_is_md1_sleep(void)
{
	return !(spm_read(SPM_PCM_REG13_DATA) & R13_MD1_SRCCLKENI);
}

bool spm_is_md2_sleep(void)
{
	return !(spm_read(SPM_PCM_REG13_DATA) & R13_MD2_SRCCLKENI);
}
#endif

void spm_output_sleep_option(void)
{
	spm_notice("PWAKE_EN:%d, BYPASS_SYSPWREQ:%d, SW_VER:%d\n",
		   SPM_PWAKE_EN, SPM_BYPASS_SYSPWREQ, mt_get_chip_sw_ver());
}

MODULE_DESCRIPTION("MT8135 SPM-Sleep Driver v0.9");

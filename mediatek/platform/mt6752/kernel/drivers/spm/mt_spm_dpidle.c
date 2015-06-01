#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/of_fdt.h>

#include <mach/irqs.h>
#include <mach/mt_cirq.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_cpuidle.h>
#include <mach/wd_api.h>
#include <mach/mt_gpt.h>
#include <mach/mt_cpufreq.h>
#include <mach/mt_ccci_common.h>

#include "mt_spm_internal.h"


/**************************************
 * only for internal debug
 **************************************/
//FIXME: for FPGA early porting
#define  CONFIG_MTK_LDVT

#ifdef CONFIG_MTK_LDVT
#define SPM_PWAKE_EN            0
#define SPM_BYPASS_SYSPWREQ     1
#else
#define SPM_PWAKE_EN            1
#define SPM_BYPASS_SYSPWREQ     0
#endif

#define WAKE_SRC_FOR_DPIDLE                                                                      \
    (WAKE_SRC_MD32_WDT| WAKE_SRC_KP | WAKE_SRC_GPT | WAKE_SRC_CONN2AP | WAKE_SRC_EINT | WAKE_SRC_CONN_WDT| \
     WAKE_SRC_CCIF0_MD | WAKE_SRC_CCIF1_MD |WAKE_SRC_MD32_SPM | WAKE_SRC_USB_CD| WAKE_SRC_USB_PDN| \
     WAKE_SRC_AFE | WAKE_SRC_MD2_WDT | WAKE_SRC_SYSPWREQ | WAKE_SRC_MD_WDT | WAKE_SRC_CLDMA_MD | WAKE_SRC_SEJ)

#define WAKE_SRC_FOR_MD32  0

#define I2C_CHANNEL 2

#define spm_is_wakesrc_invalid(wakesrc)     (!!((u32)(wakesrc) & 0xc0003803))

#define CA70_BUS_CONFIG          0xF020002C //(CA7MCUCFG_BASE + 0x1C) //0x1020011c
#define CA71_BUS_CONFIG          0xF020022C //(CA7MCUCFG_BASE + 0x1C) //0x1020011c

/**********************************************************
 * PCM code for deep idle
 **********************************************************/ 
static const u32 dpidle_binary[] = {
	0x81f48407, 0x80328400, 0x80318400, 0xe8208000, 0x10006354, 0xffff1fff,
	0xe8208000, 0x10001108, 0x00000000, 0x1b80001f, 0x20000034, 0xe8208000,
	0x10006b04, 0x00000000, 0xc28032c0, 0x1290041f, 0x1b00001f, 0x7ffcf7ff,
	0xf0000000, 0x17c07c1f, 0x1b00001f, 0x3ffce7ff, 0x1b80001f, 0x20000004,
	0xd820038c, 0x17c07c1f, 0xd0000620, 0x17c07c1f, 0xe8208000, 0x10001108,
	0x00000002, 0x1b80001f, 0x20000034, 0xe8208000, 0x10006354, 0xffffffff,
	0xe8208000, 0x10006b04, 0x00000001, 0xc28032c0, 0x1290841f, 0xa0118400,
	0xa0128400, 0xe8208000, 0x10006b04, 0x00000004, 0x1b00001f, 0x3ffcefff,
	0xa1d48407, 0xf0000000, 0x17c07c1f, 0x81489801, 0xd80007c5, 0x17c07c1f,
	0x81419801, 0xd80007c5, 0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200004,
	0xc0c033c0, 0x10807c1f, 0x81411801, 0xd80009e5, 0x17c07c1f, 0x18c0001f,
	0x1000f644, 0x1910001f, 0x1000f644, 0xa1140404, 0xe0c00004, 0x1b80001f,
	0x20000208, 0x18c0001f, 0x10006240, 0xe0e00016, 0xe0e0001e, 0xe0e0000e,
	0xe0e0000f, 0x81481801, 0xd8200d25, 0x17c07c1f, 0x18c0001f, 0x10004828,
	0x1910001f, 0x10004828, 0x89000004, 0x3fffffff, 0xe0c00004, 0x18c0001f,
	0x100041dc, 0x1910001f, 0x100041dc, 0x89000004, 0x3fffffff, 0xe0c00004,
	0x18c0001f, 0x1000f63c, 0x1910001f, 0x1000f63c, 0x89000004, 0xfffffff9,
	0xe0c00004, 0xc28032c0, 0x1294841f, 0x803e0400, 0x1b80001f, 0x20000050,
	0x803e8400, 0x803f0400, 0x803f8400, 0x1b80001f, 0x20000208, 0x803d0400,
	0x1b80001f, 0x20000034, 0x80380400, 0xa01d8400, 0x1b80001f, 0x20000034,
	0x803d8400, 0x803b0400, 0x1b80001f, 0x20000158, 0x81481801, 0xd8201005,
	0x17c07c1f, 0xa01d8400, 0x80340400, 0x81481801, 0xd8201145, 0x17c07c1f,
	0x18c0001f, 0x1000f698, 0x1910001f, 0x1000f698, 0xa1120404, 0xe0c00004,
	0x80310400, 0x81481801, 0xd8201265, 0x17c07c1f, 0xe8208000, 0x10000044,
	0x00000200, 0xd00012c0, 0x17c07c1f, 0xe8208000, 0x10000044, 0x00000100,
	0xe8208000, 0x10000004, 0x00000002, 0x1b80001f, 0x20000068, 0x1b80001f,
	0x2000000a, 0x81481801, 0xd82017a5, 0x17c07c1f, 0x18c0001f, 0x1000f640,
	0x1910001f, 0x1000f640, 0x81200404, 0xe0c00004, 0xa1000404, 0xe0c00004,
	0x18c0001f, 0x10004828, 0x1910001f, 0x10004828, 0xa9000004, 0xc0000000,
	0xe0c00004, 0x18c0001f, 0x100041dc, 0x1910001f, 0x100041dc, 0xa9000004,
	0xc0000000, 0xe0c00004, 0x18c0001f, 0x1000f63c, 0x1910001f, 0x1000f63c,
	0xa9000004, 0x00000006, 0xe0c00004, 0x18c0001f, 0x10006240, 0xe0e0000d,
	0x81fa0407, 0x18c0001f, 0x100040f4, 0x1910001f, 0x100040f4, 0xa11c8404,
	0xe0c00004, 0x813c8404, 0xe0c00004, 0x1b80001f, 0x20000100, 0x81f08407,
	0xe8208000, 0x10006354, 0xfff01b47, 0xa1d80407, 0xa1dc0407, 0xa1de8407,
	0xa1df0407, 0xc28032c0, 0x1291041f, 0x1b00001f, 0xbffce7ff, 0xf0000000,
	0x17c07c1f, 0x1b80001f, 0x20000fdf, 0x1a50001f, 0x10006608, 0x80c9a401,
	0x810aa401, 0x10918c1f, 0xa0939002, 0x80ca2401, 0x810ba401, 0xa09c0c02,
	0xa0979002, 0x8080080d, 0xd8201e42, 0x17c07c1f, 0x1b00001f, 0x3ffce7ff,
	0x1b80001f, 0x20000004, 0xd80027ec, 0x17c07c1f, 0x1b00001f, 0xbffce7ff,
	0xd00027e0, 0x17c07c1f, 0x81f80407, 0x81fc0407, 0x81fe8407, 0x81ff0407,
	0x1880001f, 0x10006320, 0xc0c02b60, 0xe080000f, 0xd8001d03, 0x17c07c1f,
	0xe080001f, 0xa1da0407, 0x81481801, 0xd82020c5, 0x17c07c1f, 0xe8208000,
	0x10000048, 0x00000300, 0xd0002120, 0x17c07c1f, 0xe8208000, 0x10000048,
	0x00000100, 0xe8208000, 0x10000004, 0x00000002, 0x1b80001f, 0x20000068,
	0xa0110400, 0x81481801, 0xd8202325, 0x17c07c1f, 0x18c0001f, 0x1000f698,
	0x1910001f, 0x1000f698, 0x81320404, 0xe0c00004, 0x803d8400, 0xa0140400,
	0xa01b0400, 0xa0180400, 0xa01d0400, 0xa01f8400, 0xa01f0400, 0xa01e8400,
	0xa01e0400, 0x1b80001f, 0x20000104, 0x81411801, 0xd8002605, 0x17c07c1f,
	0x18c0001f, 0x10006240, 0xc0c02aa0, 0x17c07c1f, 0x18c0001f, 0x1000f644,
	0x1910001f, 0x1000f644, 0x81340404, 0xe0c00004, 0x81489801, 0xd8002765,
	0x17c07c1f, 0x81419801, 0xd8002765, 0x17c07c1f, 0x1a00001f, 0x10006604,
	0xe2200005, 0xc0c033c0, 0x10807c1f, 0xc28032c0, 0x1291841f, 0x1b00001f,
	0x7ffcf7ff, 0xf0000000, 0x17c07c1f, 0x1900001f, 0x10006830, 0xe1000003,
	0xf0000000, 0x17c07c1f, 0xe0f07f16, 0x1380201f, 0xe0f07f1e, 0x1380201f,
	0xe0f07f0e, 0x1b80001f, 0x20000104, 0xe0f07f0c, 0xe0f07f0d, 0xe0f07e0d,
	0xe0f07c0d, 0xe0f0780d, 0xe0f0700d, 0xf0000000, 0x17c07c1f, 0xe0f07f0d,
	0xe0f07f0f, 0xe0f07f1e, 0xe0f07f12, 0xf0000000, 0x17c07c1f, 0xa1d08407,
	0x1b80001f, 0x20000080, 0x80eab401, 0x1a00001f, 0x10006814, 0xe2000003,
	0xf0000000, 0x17c07c1f, 0x81429801, 0xd8002e45, 0x17c07c1f, 0x18c0001f,
	0x65930005, 0x1900001f, 0x10006830, 0xe1000003, 0xe8208000, 0x10006834,
	0x00000000, 0xe8208000, 0x10006834, 0x00000001, 0xf0000000, 0x17c07c1f,
	0xa1d10407, 0x1b80001f, 0x20000020, 0xf0000000, 0x17c07c1f, 0xa1d00407,
	0x1b80001f, 0x20000100, 0x80ea3401, 0x1a00001f, 0x10006814, 0xe2000003,
	0xf0000000, 0x17c07c1f, 0xe0e0000f, 0xe0e0000e, 0xe0e0001e, 0xe0e00012,
	0xf0000000, 0x17c07c1f, 0xd800324a, 0x17c07c1f, 0xe0e00016, 0xe0e0001e,
	0x1380201f, 0xe0e0001f, 0xe0e0001d, 0xe0e0000d, 0xd0003280, 0x17c07c1f,
	0xe0e03301, 0xe0e03101, 0xf0000000, 0x17c07c1f, 0x18c0001f, 0x10006b6c,
	0x1910001f, 0x10006b6c, 0xa1002804, 0xe0c00004, 0xf0000000, 0x17c07c1f,
	0x18d0001f, 0x10006604, 0x10cf8c1f, 0xd82033c3, 0x17c07c1f, 0xf0000000,
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
	0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001, 0x1990001f, 0x10006b08,
	0xe8208000, 0x10006b6c, 0x00000000, 0x1b00001f, 0x2ffce7ff, 0x1b80001f,
	0x500f0000, 0xe8208000, 0x10006354, 0xfff01b47, 0xc0c02e80, 0x81401801,
	0xd8004765, 0x17c07c1f, 0x81f60407, 0x18c0001f, 0x10006200, 0xc0c063c0,
	0x12807c1f, 0xe8208000, 0x1000625c, 0x00000001, 0x1890001f, 0x1000625c,
	0x81040801, 0xd8204344, 0x17c07c1f, 0xc0c063c0, 0x1280041f, 0x18c0001f,
	0x10006208, 0xc0c063c0, 0x12807c1f, 0x1b80001f, 0x20000003, 0xe8208000,
	0x10006248, 0x00000000, 0x1890001f, 0x10006248, 0x81040801, 0xd8004544,
	0x17c07c1f, 0xc0c063c0, 0x1280041f, 0x18c0001f, 0x10006290, 0xe0e0004f,
	0xc0c063c0, 0x1280041f, 0xe8208000, 0x10006404, 0x00003101, 0xc28032c0,
	0x1292041f, 0x1b00001f, 0x2ffce7ff, 0x1b80001f, 0x30000004, 0x8880000c,
	0x2ffce7ff, 0xd8005dc2, 0x17c07c1f, 0xe8208000, 0x10006294, 0x0003ffff,
	0x18c0001f, 0x10006294, 0xe0e03fff, 0xe0e003ff, 0x81449801, 0xd8004c65,
	0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200006, 0xc0c033c0, 0x17c07c1f,
	0x1a00001f, 0x10006604, 0x81491801, 0xd8004c05, 0x17c07c1f, 0xc28032c0,
	0x1294041f, 0xe2200003, 0xc0c06820, 0x12807c1f, 0xc0c033c0, 0x17c07c1f,
	0xd0004c60, 0x17c07c1f, 0xe2200003, 0xc0c033c0, 0x17c07c1f, 0x81489801,
	0xd8204f45, 0x17c07c1f, 0x81419801, 0xd8004f45, 0x17c07c1f, 0x1a00001f,
	0x10006604, 0xe2200000, 0xc0c033c0, 0x17c07c1f, 0xc0c06c00, 0x17c07c1f,
	0xe2200001, 0xc0c033c0, 0x17c07c1f, 0xc0c06c00, 0x17c07c1f, 0xe2200005,
	0xc0c033c0, 0x17c07c1f, 0xc0c06c00, 0x17c07c1f, 0xc0c06760, 0x17c07c1f,
	0xa1d38407, 0xa1d98407, 0xa0108400, 0xa0120400, 0xa0148400, 0xa0150400,
	0xa0158400, 0xa01b8400, 0xa01c0400, 0xa01c8400, 0xa0188400, 0xa0190400,
	0xa0198400, 0xe8208000, 0x10006310, 0x0b1600f8, 0x1b00001f, 0xbffce7ff,
	0x1b80001f, 0x90100000, 0x1240301f, 0x80c28001, 0xc8c00003, 0x17c07c1f,
	0x80c10001, 0xc8c00663, 0x17c07c1f, 0x1b00001f, 0x2ffce7ff, 0x18c0001f,
	0x10006294, 0xe0e007fe, 0xe0e00ffc, 0xe0e01ff8, 0xe0e03ff0, 0xe0e03fe0,
	0xe0e03fc0, 0x1b80001f, 0x20000020, 0xe8208000, 0x10006294, 0x0003ffc0,
	0xe8208000, 0x10006294, 0x0003fc00, 0x80388400, 0x80390400, 0x80398400,
	0x1b80001f, 0x20000300, 0x803b8400, 0x803c0400, 0x803c8400, 0x1b80001f,
	0x20000300, 0x80348400, 0x80350400, 0x80358400, 0x1b80001f, 0x20000104,
	0x80308400, 0x80320400, 0x81f38407, 0x81f98407, 0x81f90407, 0x81f40407,
	0x81489801, 0xd8205aa5, 0x17c07c1f, 0x81419801, 0xd8005aa5, 0x17c07c1f,
	0x1a00001f, 0x10006604, 0xe2200001, 0xc0c033c0, 0x17c07c1f, 0xc0c06c00,
	0x17c07c1f, 0xe2200000, 0xc0c033c0, 0x17c07c1f, 0xc0c06c00, 0x17c07c1f,
	0xe2200004, 0xc0c033c0, 0x17c07c1f, 0xc0c06c00, 0x17c07c1f, 0x81449801,
	0xd8005dc5, 0x17c07c1f, 0x1a00001f, 0x10006604, 0x81491801, 0xd8005c85,
	0x17c07c1f, 0xe2200002, 0xc0c06820, 0x1280041f, 0xc0c033c0, 0x17c07c1f,
	0xd0005ce0, 0x17c07c1f, 0xe2200002, 0xc0c033c0, 0x17c07c1f, 0x1a00001f,
	0x10006604, 0xe2200007, 0xc0c033c0, 0x17c07c1f, 0x1b80001f, 0x200016a8,
	0x81401801, 0xd8006325, 0x17c07c1f, 0xe8208000, 0x10006404, 0x00000101,
	0x18c0001f, 0x10006290, 0x1212841f, 0xc0c06540, 0x12807c1f, 0xc0c06540,
	0x1280041f, 0x18c0001f, 0x10006208, 0x1212841f, 0xc0c06540, 0x12807c1f,
	0xe8208000, 0x10006248, 0x00000001, 0x1890001f, 0x10006248, 0x81040801,
	0xd8206064, 0x17c07c1f, 0xc0c06540, 0x1280041f, 0x18c0001f, 0x10006200,
	0x1212841f, 0xc0c06540, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000000,
	0x1890001f, 0x1000625c, 0x81040801, 0xd8006244, 0x17c07c1f, 0xc0c06540,
	0x1280041f, 0x19c0001f, 0x61415820, 0x1ac0001f, 0x55aa55aa, 0xf0000000,
	0xd800646a, 0x17c07c1f, 0xe2e0004f, 0xe2e0006f, 0xe2e0002f, 0xd820650a,
	0x17c07c1f, 0xe2e0002e, 0xe2e0003e, 0xe2e00032, 0xf0000000, 0x17c07c1f,
	0xd800660a, 0x17c07c1f, 0xe2e00036, 0xe2e0003e, 0x1380201f, 0xe2e0003c,
	0xd820672a, 0x17c07c1f, 0x1380201f, 0xe2e0007c, 0x1b80001f, 0x20000003,
	0xe2e0005c, 0xe2e0004c, 0xe2e0004d, 0xf0000000, 0x17c07c1f, 0xa1d40407,
	0x1391841f, 0xa1d90407, 0x1392841f, 0xf0000000, 0x17c07c1f, 0xe8208000,
	0x10059c14, 0x00000002, 0xe8208000, 0x10059c20, 0x00000001, 0xe8208000,
	0x10059c04, 0x000000d6, 0x1a00001f, 0x10059c00, 0xd8206aea, 0x17c07c1f,
	0xe2200088, 0xe2200002, 0xe8208000, 0x10059c24, 0x00000001, 0x1b80001f,
	0x20000158, 0xd0006bc0, 0x17c07c1f, 0xe2200088, 0xe2200000, 0xe8208000,
	0x10059c24, 0x00000001, 0x1b80001f, 0x20000158, 0xf0000000, 0x17c07c1f,
	0x1880001f, 0x0000001d, 0x814a1801, 0xd8006e65, 0x17c07c1f, 0x81499801,
	0xd8006f65, 0x17c07c1f, 0x814a9801, 0xd8007065, 0x17c07c1f, 0x18d0001f,
	0x40000000, 0x18d0001f, 0x40000000, 0xd8006d62, 0x00a00402, 0xd0007160,
	0x17c07c1f, 0x18d0001f, 0x40000000, 0x18d0001f, 0x80000000, 0xd8006e62,
	0x00a00402, 0xd0007160, 0x17c07c1f, 0x18d0001f, 0x40000000, 0x18d0001f,
	0x60000000, 0xd8006f62, 0x00a00402, 0xd0007160, 0x17c07c1f, 0x18d0001f,
	0x40000000, 0x18d0001f, 0xc0000000, 0xd8007062, 0x00a00402, 0xd0007160,
	0x17c07c1f, 0xf0000000, 0x17c07c1f
};
static struct pcm_desc dpidle_pcm = {
	.version	= "pcm_deepidle_v19.15_20140731-exp1-no_6311_low_power_mode",
	.base		= dpidle_binary,
	.size		= 909,
	.sess		= 2,
	.replace	= 0,
	.vec0		= EVENT_VEC(11, 1, 0, 0),	/* FUNC_26M_WAKEUP */
	.vec1		= EVENT_VEC(12, 1, 0, 20),	/* FUNC_26M_SLEEP */
	.vec2		= EVENT_VEC(30, 1, 0, 51),	/* FUNC_APSRC_WAKEUP */
	.vec3		= EVENT_VEC(31, 1, 0, 217),	/* FUNC_APSRC_SLEEP */
};
 

static struct pwr_ctrl dpidle_ctrl = {
	.wake_src		= WAKE_SRC_FOR_DPIDLE,
	.wake_src_md32		= WAKE_SRC_FOR_MD32,
	.r0_ctrl_en		= 1,
	.r7_ctrl_en		= 1,
	.infra_dcm_lock		= 1,
	.wfi_op			= WFI_OP_AND,
	.ca15_wfi0_en		= 1,
	.ca15_wfi1_en		= 1,
	.ca15_wfi2_en		= 1,
	.ca15_wfi3_en		= 1,
	.ca7_wfi0_en		= 1,
	.ca7_wfi1_en		= 1,
	.ca7_wfi2_en		= 1,
	.ca7_wfi3_en		= 1,
	.disp_req_mask		= 1,
	.mfg_req_mask		= 1,
	.lte_mask		= 1,
	.syspwreq_mask		= 1,
};

struct spm_lp_scen __spm_dpidle = {
	.pcmdesc	= &dpidle_pcm,
	.pwrctrl	= &dpidle_ctrl,
};

#ifndef CONFIG_ARM64
extern int mt_irq_mask_all(struct mtk_irq_mask *mask);
extern int mt_irq_mask_restore(struct mtk_irq_mask *mask);
extern void mt_irq_unmask_for_sleep(unsigned int irq);
#endif
extern int request_uart_to_sleep(void);
extern int request_uart_to_wakeup(void);
extern unsigned int mt_get_clk_mem_sel(void);
extern int is_ext_buck_exist(void);

//FIXME: K2 early porting
#if 1
void __attribute__((weak))
mt_cirq_clone_gic(void)
{
	return;//FIXME: K2 early porting
}
void __attribute__((weak))
mt_cirq_enable(void)
{
	return;//FIXME: K2 early porting
}
void __attribute__((weak))
mt_cirq_flush(void)
{
	return;//FIXME: K2 early porting
}

void __attribute__((weak))
mt_cirq_disable(void)
{
	return;//FIXME: K2 early porting
}
#endif

static void spm_trigger_wfi_for_dpidle(struct pwr_ctrl *pwrctrl)
{
    if (is_cpu_pdn(pwrctrl->pcm_flags)) {
        mt_cpu_dormant(CPU_DEEPIDLE_MODE);
    } else {
        spm_write(CA70_BUS_CONFIG, spm_read(CA70_BUS_CONFIG) | 0x10);
        spm_write(CA71_BUS_CONFIG, spm_read(CA71_BUS_CONFIG) | 0x10);    
        wfi_with_sync();
        spm_write(CA71_BUS_CONFIG, spm_read(CA71_BUS_CONFIG) & ~0x10);
        spm_write(CA70_BUS_CONFIG, spm_read(CA70_BUS_CONFIG) & ~0x10);		
    }
}

/*
 * wakesrc: WAKE_SRC_XXX
 * enable : enable or disable @wakesrc
 * replace: if true, will replace the default setting
 */
int spm_set_dpidle_wakesrc(u32 wakesrc, bool enable, bool replace)
{
    unsigned long flags;

    if (spm_is_wakesrc_invalid(wakesrc))
        return -EINVAL;

    spin_lock_irqsave(&__spm_lock, flags);
    if (enable) {
        if (replace)
            __spm_dpidle.pwrctrl->wake_src = wakesrc;
        else
            __spm_dpidle.pwrctrl->wake_src |= wakesrc;
    } else {
        if (replace)
            __spm_dpidle.pwrctrl->wake_src = 0;
        else
            __spm_dpidle.pwrctrl->wake_src &= ~wakesrc;
    }
    spin_unlock_irqrestore(&__spm_lock, flags);

    return 0;
}

extern void spm_i2c_control(u32 channel, bool onoff);
U32 g_bus_ctrl = 0;
U32 g_sys_ck_sel = 0;
static void spm_dpidle_pre_process(void)
{
    spm_i2c_control(I2C_CHANNEL, 1);
    g_bus_ctrl=spm_read(0xF0001070);
    spm_write(0xF0001070 , g_bus_ctrl | (1 << 21)); //bus dcm disable
    g_sys_ck_sel = spm_read(0xF0001108);
    //spm_write(0xF0001108 , g_sys_ck_sel &~ (1<<1) );
    spm_write(0xF0001108 , 0x0);
    spm_write(0xF0000204 , spm_read(0xF0000204) | (1 << 0));  // BUS 26MHz enable 
}

static void spm_dpidle_post_process(void)
{
    spm_i2c_control(I2C_CHANNEL, 0);
    spm_write(0xF0001070 , g_bus_ctrl); //26:26 enable 
}
enum mempll_type{
    MEMP26MHZ		= 0,
    MEMPLL3PLL		= 1,
    MEMPLL1PLL		= 2,
};
#ifdef CONFIG_OF
static int dt_scan_memory(unsigned long node, const char *uname, int depth, void *data)
{
	char *type = of_get_flat_dt_prop(node, "device_type", NULL);
	__be32 *reg, *endp;
	unsigned long l;

	/* We are scanning "memory" nodes only */
	if (type == NULL) {
		/*
		 * The longtrail doesn't have a device_type on the
		 * /memory node, so look for the node called /memory@0.
		 */
		if (depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0)
		return 0;

		reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;

	endp = reg + (l / sizeof(__be32));

	return node;
}
#endif

extern u32 slp_spm_deepidle_flags;
bool spm_set_dpidle_pcm_init_flag(void)
{
#ifdef CONFIG_OF
    int node, i;
    dram_info_t *dram_info;
    node = of_scan_flat_dt(dt_scan_memory, NULL);
	
    if (node) {
        /* orig_dram_info */
        dram_info = (dram_info_t *)of_get_flat_dt_prop(node,
            "orig_dram_info", NULL);
    }

    /*SPM dummy read rank selection*/		
    slp_spm_deepidle_flags &=~(SPM_DRAM_RANK1_ADDR_SEL0|SPM_DRAM_RANK1_ADDR_SEL1|SPM_DRAM_RANK1_ADDR_SEL2);

    if(dram_info->rank_info[1].start==0x60000000)
        slp_spm_deepidle_flags |= SPM_DRAM_RANK1_ADDR_SEL0;
    else if(dram_info->rank_info[1].start==0x80000000)
        slp_spm_deepidle_flags |= SPM_DRAM_RANK1_ADDR_SEL1;		
    else if(dram_info->rank_info[1].start==0xc0000000)
        slp_spm_deepidle_flags |= SPM_DRAM_RANK1_ADDR_SEL2;	
    else if(dram_info->rank_info[1].size!=0x0)
    {
        printk("dram rank1_info_error: %x\n",dram_info->rank_info[1].start);
        BUG_ON(1);
        //return false;
    }

    //pwrctrl->pcm_flags |= (1<<31);
#else
    printk("dram rank1_info_error: no rank info\n");
    BUG_ON(1);
    //return false;
#endif

    if(is_ext_buck_exist())
        slp_spm_deepidle_flags&=~SPM_BUCK_SEL;
    else
        slp_spm_deepidle_flags|=SPM_BUCK_SEL;

    return true;

}

static bool spm_set_dpidle_pcm_flag(struct pwr_ctrl *pwrctrl)
{	
    if (pwrctrl->pcm_flags_cust == 0)
    {
        if(mt_get_clk_mem_sel()==MEMPLL3PLL)
            pwrctrl->pcm_flags &=~ (SPM_MEMPLL_1PLL_3PLL_SEL|SPM_VCORE_DVS_POSITION);
        else if(mt_get_clk_mem_sel()==MEMPLL1PLL)
            pwrctrl->pcm_flags |= (SPM_MEMPLL_1PLL_3PLL_SEL|SPM_VCORE_DVS_POSITION);
        else
            return false;
        return true;
    }
    return true;
}

wake_reason_t spm_go_to_dpidle(u32 spm_flags, u32 spm_data)
{
    struct wake_status wakesta;
    unsigned long flags;
    struct mtk_irq_mask mask;
    wake_reason_t wr = WR_NONE;
    struct pcm_desc *pcmdesc = __spm_dpidle.pcmdesc;
    struct pwr_ctrl *pwrctrl = __spm_dpidle.pwrctrl;
    struct spm_lp_scen *lpscen;

    set_pwrctrl_pcm_flags(pwrctrl, spm_flags);
	if(spm_set_dpidle_pcm_flag(pwrctrl)==false)
		return WR_UNKNOWN;
#ifndef CONFIG_ARM64
    /* set PMIC WRAP table for deepidle power control */
    mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE);
#endif

#ifndef CONFIG_ARM64
	spm_dpidle_before_wfi();
#endif

    spin_lock_irqsave(&__spm_lock, flags);
	//FIXME: for K2 fpga early porting
#ifndef CONFIG_ARM64
    mt_irq_mask_all(&mask);
    mt_irq_unmask_for_sleep(195/*MT_SPM_IRQ_ID*/);
#endif	
    mt_cirq_clone_gic();
    mt_cirq_enable();

    if (request_uart_to_sleep()) {
        wr = WR_UART_BUSY;
        goto RESTORE_IRQ;
    }
	
    __spm_reset_and_init_pcm(pcmdesc);

    __spm_kick_im_to_fetch(pcmdesc);
	
    __spm_init_pcm_register();

    __spm_init_event_vector(pcmdesc);

    __spm_set_power_control(pwrctrl);

    __spm_set_wakeup_event(pwrctrl);

    __spm_kick_pcm_to_run(pwrctrl);

    spm_dpidle_pre_process();

    spm_trigger_wfi_for_dpidle(pwrctrl);

    spm_dpidle_post_process();

    __spm_get_wakeup_status(&wakesta);

    __spm_clean_after_wakeup();

    request_uart_to_wakeup();

    wr = __spm_output_wake_reason(&wakesta, pcmdesc, false);

RESTORE_IRQ:
    mt_cirq_flush();
    mt_cirq_disable();
//FIXME: for K2 fpga early porting
#ifndef CONFIG_ARM64	
    mt_irq_mask_restore(&mask);
#endif
    spin_unlock_irqrestore(&__spm_lock, flags);

#ifndef CONFIG_ARM64
    spm_dpidle_after_wfi();
#endif

#ifndef CONFIG_ARM64
    /* set PMIC WRAP table for normal power control */
    mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);
#endif
    return wr;
}

/*
 * cpu_pdn:
 *    true  = CPU dormant
 *    false = CPU standby
 * pwrlevel:
 *    0 = AXI is off
 *    1 = AXI is 26M
 * pwake_time:
 *    >= 0  = specific wakeup period
 */
wake_reason_t spm_go_to_sleep_dpidle(u32 spm_flags, u32 spm_data)
{
    u32 sec = 0;
    int wd_ret;
    struct wake_status wakesta;
    unsigned long flags;
    struct mtk_irq_mask mask;
    struct wd_api *wd_api;
    static wake_reason_t last_wr = WR_NONE;
    struct pcm_desc *pcmdesc = __spm_dpidle.pcmdesc;
    struct pwr_ctrl *pwrctrl = __spm_dpidle.pwrctrl;

    set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

#if SPM_PWAKE_EN
    sec = spm_get_wake_period(-1 /* FIXME */, last_wr);
#endif
    pwrctrl->timer_val = sec * 32768;

    pwrctrl->wake_src = spm_get_sleep_wakesrc();

    wd_ret = get_wd_api(&wd_api);
    if (!wd_ret)
        wd_api->wd_suspend_notify();

    spin_lock_irqsave(&__spm_lock, flags);
//FIXME: for K2 fpga early porting
#ifndef CONFIG_ARM64	
    mt_irq_mask_all(&mask);
    mt_irq_unmask_for_sleep(195/*MT_SPM_IRQ_ID*/);
#endif	
    mt_cirq_clone_gic();
    mt_cirq_enable();
#ifndef CONFIG_ARM64	
    /* set PMIC WRAP table for deepidle power control */
    mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE);
#endif
    spm_crit2("sleep_deepidle, sec = %u, wakesrc = 0x%x [%u]\n",
              sec, pwrctrl->wake_src, is_cpu_pdn(pwrctrl->pcm_flags));

    __spm_reset_and_init_pcm(pcmdesc);

    __spm_kick_im_to_fetch(pcmdesc);

    if (request_uart_to_sleep()) {
        last_wr = WR_UART_BUSY;
        goto RESTORE_IRQ;
    }

    __spm_init_pcm_register();

    __spm_init_event_vector(pcmdesc);

    __spm_set_power_control(pwrctrl);

    __spm_set_wakeup_event(pwrctrl);

    __spm_kick_pcm_to_run(pwrctrl);

    spm_trigger_wfi_for_dpidle(pwrctrl);

    __spm_get_wakeup_status(&wakesta);

    __spm_clean_after_wakeup();

    request_uart_to_wakeup();

    last_wr = __spm_output_wake_reason(&wakesta, pcmdesc, true);

RESTORE_IRQ:
#ifndef CONFIG_ARM64	
    /* set PMIC WRAP table for normal power control */
    mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);
#endif
    mt_cirq_flush();
    mt_cirq_disable();
//FIXME: for K2 fpga early porting
#ifndef CONFIG_ARM64	
    mt_irq_mask_restore(&mask);
#endif
    spin_unlock_irqrestore(&__spm_lock, flags);

    if (!wd_ret)
        wd_api->wd_resume_notify();

    return last_wr;  
}

MODULE_DESCRIPTION("SPM-DPIdle Driver v0.1");

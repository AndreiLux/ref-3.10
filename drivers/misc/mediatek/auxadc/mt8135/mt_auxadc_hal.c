/*****************************************************************************
 *
 * Filename:
 * ---------
 *   mt_auxadc_hal.c (for mt8135)
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of mt8135 AUXADC.
 *
 * Author:
 * -------
 *   Kui Wang
 *
 ****************************************************************************/

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <mach/mt_gpt.h>
#include <mach/mt_clkmgr.h>
#include <mach/sync_write.h>
#include "mt_auxadc_sw.h"
#include "mt_auxadc_hw.h"

#define DRV_ClearBits(addr, data)     {\
   kal_uint16 temp;\
   temp = DRV_Reg(addr);\
   temp &=  ~(data);\
   mt65xx_reg_sync_writew(temp, addr);\
}

#define DRV_SetBits(addr, data)     {\
   kal_uint16 temp;\
   temp = DRV_Reg(addr);\
   temp |= (data);\
   mt65xx_reg_sync_writew(temp, addr);\
}

#define DRV_SetData(addr, bitmask, value)     {\
   kal_uint16 temp;\
   temp = (~(bitmask)) & DRV_Reg(addr);\
   temp |= (value);\
   mt65xx_reg_sync_writew(temp, addr);\
}

#define AUXADC_DRV_ClearBits16(addr, data)           DRV_ClearBits(addr, data)
#define AUXADC_DRV_SetBits16(addr, data)             DRV_SetBits(addr, data)
#define AUXADC_DRV_WriteReg16(addr, data)            mt65xx_reg_sync_writew(data, addr)
#define AUXADC_DRV_ReadReg16(addr)                   DRV_Reg(addr)
#define AUXADC_DRV_SetData16(addr, bitmask, value)   DRV_SetData(addr, bitmask, value)

#define AUXADC_DVT_DELAYMACRO(u4Num)                                     \
{                                                                        \
    unsigned int u4Count = 0;                                           \
    for (u4Count = 0; u4Count < u4Num; u4Count++);                      \
}

#define AUXADC_CLR_BITS(BS, REG)     {\
   kal_uint32 temp;\
   temp = DRV_Reg32(REG);\
   temp &=  ~(BS);\
   mt65xx_reg_sync_writel(temp, REG);\
}

#define AUXADC_SET_BITS(BS, REG)     {\
   kal_uint32 temp;\
   temp = DRV_Reg32(REG);\
   temp |= (BS);\
   mt65xx_reg_sync_writel(temp, REG);\
}

#define AUXADC_Print(format, ...)    do { pr_info("[auxadc] " format, ##__VA_ARGS__); } while (0)

#define VOLTAGE_FULL_RANGE  1500	/* VA voltage */
#define AUXADC_PRECISE      4096	/* 12 bits */

/*****************************************************************************
 * Integrate with NVRAM
****************************************************************************/
static DEFINE_MUTEX(mutex_get_cali_value);
static int adc_auto_set;

/* step1 check con2 if auxadc is busy */
/* step2 clear bit */
/* step3 read channel and make sure the old ready bit be 0 */
/* step4 set bit to trigger sample */
/* step5 read channel until the ready bit be 1 */
/* step6 read data */
int IMM_auxadc_GetOneChannelValue(int dwChannel, int data[4], int *rawdata)
{
	unsigned int reg_val = 0;
	int timeout_counter = 0;

	mutex_lock(&mutex_get_cali_value);

#if 0
	if (enable_clock(MT_PDN_PERI_AUXADC, "AUXADC")) {
		AUXADC_Print("hwEnableClock AUXADC failed.");
	}
#endif

	/* step1 check con2 if auxadc is busy */
	while ((*(volatile u16 *)AUXADC_CON2) & 0x01) {
		AUXADC_Print("wait for module idle\n");
		msleep(100);

		if (++timeout_counter > 30) {
			/* wait for idle time out */
			AUXADC_Print("wait for aux/adc idle time out\n");
			mutex_unlock(&mutex_get_cali_value);
			return -1;
		}
	}

	timeout_counter = 0;

	/* step2 clear bit */
	if (0 == adc_auto_set) {
		/* clear bit */
		AUXADC_DRV_ClearBits16(AUXADC_CON1, (1 << dwChannel));
	}
	/* step3 read channel and make sure the old ready bit be 0 */
	while ((*(volatile u16 *)(AUXADC_DAT0 + dwChannel * 0x04)) & (1 << 12)) {
		AUXADC_Print("wait for channel[%d] ready bit clear\n", dwChannel);
		msleep(10);

		if (++timeout_counter > 30) {
			/* wait for idle time out */
			AUXADC_Print("wait for channel[%d] ready bit clear time out\n", dwChannel);
			mutex_unlock(&mutex_get_cali_value);
			return -2;
		}
	}

	timeout_counter = 0;

	/* step4 set bit to trigger sample */
	if (0 == adc_auto_set) {
		AUXADC_DRV_SetBits16(AUXADC_CON1, (1 << dwChannel));
	}
	/* step5 read channel until the ready bit be 1 */
	mdelay(1);		/* we must dealay here for hw sample cahnnel data */

	while (0 == ((*(volatile u16 *)(AUXADC_DAT0 + dwChannel * 0x04)) & (1 << 12))) {
		AUXADC_Print("wait for channel[%d] ready bit to be 1\n", dwChannel);
		msleep(10);

		if (++timeout_counter > 30) {
			/* wait for idle time out */
			AUXADC_Print("wait for channel[%d] data ready time out\n", dwChannel);
			mutex_unlock(&mutex_get_cali_value);
			return -3;
		}
	}

	/* step6 read data */
	reg_val = (*(volatile u16 *)(AUXADC_DAT0 + dwChannel * 0x04)) & 0x0FFF;

	if (NULL != rawdata) {
		*rawdata = reg_val;
	}
	/* AUXADC_Print("imm mode raw data => channel[%d] = %d\n",dwChannel, channel[dwChannel]); */
	/* AUXADC_Print("imm mode => channel[%d] = %d.%02d\n", dwChannel, (channel[dwChannel] * 150 / AUXADC_PRECISE / 100), ((channel[dwChannel] * 150 / AUXADC_PRECISE) % 100)); */
	data[0] = (reg_val * (VOLTAGE_FULL_RANGE / 10) / AUXADC_PRECISE) / 100;
	data[1] = (reg_val * (VOLTAGE_FULL_RANGE / 10) / AUXADC_PRECISE) % 100;

#if 0
	if (disable_clock(MT_PDN_PERI_AUXADC, "AUXADC")) {
		AUXADC_Print("hwEnableClock AUXADC failed.");
	}
#endif

	mutex_unlock(&mutex_get_cali_value);
	return 0;
}

/* 1v == 1000000 uv */
/* this function voltage Unit is uv */
int IMM_auxadc_GetOneChannelValue_Cali(int Channel, int *voltage)
{
	int ret, data[4], rawvalue;
	ret = IMM_auxadc_GetOneChannelValue(Channel, data, &rawvalue);

	if (ret) {
		AUXADC_Print("IMM_auxadc_GetOneChannelValue_Cali() get raw value error %d\n", ret);
		return -1;
	}

	*voltage = rawvalue * (VOLTAGE_FULL_RANGE * 1000) / AUXADC_PRECISE;
	/* AUXADC_Print("IMM_auxadc_GetOneChannelValue_Cali  voltage= %d uv\n",*voltage); */
	return 0;
}

#if 0
static int IMM_auxadc_get_evrage_data(int times, int Channel)
{
	int sum_volt = 0, data[4], i = times, volt = 0;

	while (i--) {
		if (IMM_auxadc_GetOneChannelValue(Channel, data, &volt) == 0) {
			sum_volt += volt;
			AUXADC_Print("channel[%d]=%d\n", Channel, volt);
		} else {
			--times;
			continue;
		}
	}

	return (times == 0) ? 0 : (sum_volt / times);
}

static void mt_auxadc_cal_prepare(void)
{
	/* mt8135 no voltage calibration */
}
#endif

void mt_auxadc_hal_init(void)
{

}

void mt_auxadc_hal_suspend(void)
{
	int timeout_counter = 0;

	while (clock_is_on(MT_CG_PERI_AUXADC_PDN) && (timeout_counter++ < 1000)) {
		AUXADC_Print("disable_clock()\n");
		if (disable_clock(MT_CG_PERI_AUXADC_PDN, "AUXADC") != 0) {
			AUXADC_Print("disable_clock() failed.\n");
		}
	}
}

void mt_auxadc_hal_resume(void)
{
	if (!clock_is_on(MT_CG_PERI_AUXADC_PDN)) {
		AUXADC_Print("enable_clock()\n");
		if (enable_clock(MT_CG_PERI_AUXADC_PDN, "AUXADC") != 0) {
			AUXADC_Print("enable_clock() failed.\n");
		}
	}
}

int mt_auxadc_dump_register(char *buf)
{
	AUXADC_Print("AUXADC_CON0=0x%04x\n", *(volatile u16 *)AUXADC_CON0);
	AUXADC_Print("AUXADC_CON1=0x%04x\n", *(volatile u16 *)AUXADC_CON1);
	AUXADC_Print("AUXADC_CON2=0x%04x\n", *(volatile u16 *)AUXADC_CON2);
	return sprintf(buf, "AUXADC_CON0:0x%04x\nAUXADC_CON1:0x%04x\nAUXADC_CON2:0x%04x\n",
		       *(volatile u16 *)AUXADC_CON0, *(volatile u16 *)AUXADC_CON1,
		       *(volatile u16 *)AUXADC_CON2);
}

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_dcm.h>
#include "adc.h"

#define ADC_DEBUG   1

#define ADC_NAME    "dvt-adc"

#if ADC_DEBUG
#define adc_print(fmt, arg...)  printk("[adc_udvt]: " fmt, ##arg)
#else
#define adc_print(fmt, arg...)  do {} while (0)
#endif

#define ReadREG(reg)         (*((volatile const u16 *)(reg)))
#define WriteREG(reg, val)   ((*((volatile u16 *)(reg))) = ((u16)(val)))
#define MskWriteREG(reg, val, msk)   WriteREG((reg), ((ReadREG((reg)) & (~((u16)(msk)))) | (((u16)(val)) & ((u16)(msk)))))

struct udvt_cmd {
	int cmd;
	int value;
};

static int adc_using_set_clr_mode = 1;
static int adc_auto_set;
static unsigned int adc_auto_set_need_trigger_1st_sample;
static int adc_run;
/* static int adc_mode = 0; */
static int adc_log_en;
#if 0
static int adc_3g_tx_en;
static int adc_2g_tx_en;
#endif
static int adc_bg_detect_en;
static int adc_wakeup_src_en;

struct timer_list adc_udvt_timer;
struct tasklet_struct adc_udvt_tasklet;

extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);


static void dump_register(void)
{
	/* /u8 data; */
	printk("[adc_udvt]: AUXADC_CON0=0x%x\n", *(volatile u16 *)AUXADC_CON0);
	printk("[adc_udvt]: AUXADC_CON1=0x%x\n", *(volatile u16 *)AUXADC_CON1);
	printk("[adc_udvt]: AUXADC_CON2=0x%x\n", *(volatile u16 *)AUXADC_CON2);
	/* printk("[adc_udvt]: AUXADC_CON3=%x\n",*(volatile u16 *)AUXADC_CON3); */

	printk("[adc_udvt]: clock_is_on()=%i\n", clock_is_on(MT_CG_PERI_AUXADC_PDN));
}

/* // Common API */

int IMM_GetOneChannelValue(int dwChannel, int data[4])
{
	if (adc_auto_set) {
		int idle_count = 0;
		int data_ready_count = 0;
		u32 reg_val = 0;

		/* Polling until bit STA = 0 */
		while (ReadREG(AUXADC_CON2) & 0x01) {
			printk("[adc_api]: wait for module idle\n");
			mdelay(100);

			if (++idle_count > 30) {
				/* wait for idle time out */
				printk("[adc_api]: wait for aux/adc idle time out\n");
				return -1;
			}
		}

		if (adc_auto_set_need_trigger_1st_sample & (1 << dwChannel)) {
			/* clear bit */
			MskWriteREG(AUXADC_CON1, 0 << dwChannel, 1 << dwChannel);

			/* read data to make sure old ready bit be 0 */
			reg_val = ReadREG(AUXADC_DAT0 + dwChannel * 0x04);
			mdelay(20);

			/* set bit */
			MskWriteREG(AUXADC_CON1, 1 << dwChannel, 1 << dwChannel);
			mdelay(20);

			adc_auto_set_need_trigger_1st_sample &= (~(1 << dwChannel));
		}
		/* read data */
		while (1) {
			reg_val = ReadREG(AUXADC_DAT0 + dwChannel * 0x04);

			if (reg_val & (1 << 12)) {
				break;
			} else {
				printk("[adc_api]: wait for channel[%d] data ready\n", dwChannel);
				mdelay(100);

				if (++data_ready_count > 30) {
					/* wait for idle time out */
					printk
					    ("[adc_api]: wait for channel[%d] data ready time out\n",
					     dwChannel);
					return -2;
				}
			}
		}

		data[0] = (reg_val & 0x0FFF) * 1500 / 4096;

		return 0;
	} else {
		int idle_count = 0;
		int data_ready_count = 0;
		u32 reg_val = 0;

		/* unsigned int i = 0, data = 0; */
		/* unsigned int poweron, poweroff; */

		/* Polling until bit STA = 0 */
		while (ReadREG(AUXADC_CON2) & 0x01) {
			printk("[adc_api]: wait for module idle\n");
			mdelay(100);

			if (++idle_count > 30) {
				/* wait for idle time out */
				printk("[adc_api]: wait for aux/adc idle time out\n");
				return -1;
			}
		}

		if (adc_using_set_clr_mode) {
			/* clear bit */
			WriteREG(AUXADC_CON1_CLR, 1 << dwChannel);
		} else {
			/* clear bit */
			MskWriteREG(AUXADC_CON1, 0 << dwChannel, 1 << dwChannel);
		}


		/* read data to make sure old ready bit be 0 */
		reg_val = ReadREG(AUXADC_DAT0 + dwChannel * 0x04);
		mdelay(20);

		/* read data */
		if (adc_using_set_clr_mode) {
			/* set bit */
			WriteREG(AUXADC_CON1_SET, 1 << dwChannel);
		} else {
			/* set bit */
			MskWriteREG(AUXADC_CON1, 1 << dwChannel, 1 << dwChannel);
		}

		mdelay(20);

		/* read data */
		while (1) {
			reg_val = ReadREG(AUXADC_DAT0 + dwChannel * 0x04);

			if (reg_val & (1 << 12)) {
				break;
			} else {
				printk("[adc_api]: wait for channel[%d] data ready\n", dwChannel);
				mdelay(100);

				if (++data_ready_count > 30) {
					/* wait for idle time out */
					printk
					    ("[adc_api]: wait for channel[%d] data ready time out\n",
					     dwChannel);
					return -2;
				}
			}
		}

		data[0] = (reg_val & 0x0FFF) * 1500 / 4096;
		mdelay(20);

		if (adc_using_set_clr_mode) {
			/* clear bit */
			WriteREG(AUXADC_CON1_CLR, 1 << dwChannel);
		} else {
			/* clear bit */
			MskWriteREG(AUXADC_CON1, 0 << dwChannel, 1 << dwChannel);
		}

		return 0;
	}
}

int IMM_GetAllChannelValue(int data[16])
{
	printk("[adc_api]: IMM_GetAllChannelValue()\n");
	if (adc_auto_set) {
		int i;
		int idle_count = 0;
		int data_ready_count = 0;
		u32 reg_val = 0;

		/* Polling until bit STA = 0 */
		while (ReadREG(AUXADC_CON2) & 0x01) {
			printk("[adc_api]: wait for module idle\n");
			mdelay(100);

			if (++idle_count > 30) {
				/* wait for idle time out */
				printk("[adc_api]: wait for aux/adc idle time out\n");
				return -1;
			}
		}

		if (adc_auto_set_need_trigger_1st_sample & 0xFFFF) {
			/* clear bit */
			MskWriteREG(AUXADC_CON1, 0, 0xFFFF);

			/* read data to make sure old ready bit be 0 */
			for (i = 0; i < 16; ++i) {
				reg_val = ReadREG(AUXADC_DAT0 + i * 0x04);
			}
			mdelay(20);

			/* set bit */
			MskWriteREG(AUXADC_CON1, 0xFFFF, 0xFFFF);
			mdelay(20);

			adc_auto_set_need_trigger_1st_sample = 0;
		}
		/* read data */
		for (i = 0; i < 16; ++i) {
			while (1) {
				reg_val = ReadREG(AUXADC_DAT0 + i * 0x04);

				if (reg_val & (1 << 12)) {
					break;
				} else {
					printk("[adc_api]: wait for channel[%d] data ready\n", i);
					mdelay(100);

					if (++data_ready_count > 30) {
						/* wait for idle time out */
						printk
						    ("[adc_api]: wait for channel[%d] data ready time out\n",
						     i);
						return -2;
					}
				}
			}

			data[i] = (reg_val & 0x0FFF) * 1500 / 4096;
		}

		return 0;
	} else {
		int i;
		int idle_count = 0;
		int data_ready_count = 0;
		u32 reg_val = 0;

		/* unsigned int i = 0, data = 0; */
		/* unsigned int poweron, poweroff; */

		/* Polling until bit STA = 0 */
		while (ReadREG(AUXADC_CON2) & 0x01) {
			printk("[adc_api]: wait for module idle\n");
			mdelay(100);

			if (++idle_count > 30) {
				/* wait for idle time out */
				printk("[adc_api]: wait for aux/adc idle time out\n");
				return -1;
			}
		}

		if (adc_using_set_clr_mode) {
			/* clear bit */
			WriteREG(AUXADC_CON1_CLR, 0xFFFF);
		} else {
			/* clear bit */
			MskWriteREG(AUXADC_CON1, 0, 0xFFFF);
		}


		/* read data to make sure old ready bit be 0 */
		for (i = 0; i < 16; ++i) {
			reg_val = ReadREG(AUXADC_DAT0 + i * 0x04);
		}
		mdelay(20);

		/* read data */
		if (adc_using_set_clr_mode) {
			/* set bit */
			WriteREG(AUXADC_CON1_SET, 0xFFFF);
		} else {
			/* set bit */
			MskWriteREG(AUXADC_CON1, 0xFFFF, 0xFFFF);
		}

		mdelay(20);

		/* read data */
		for (i = 0; i < 16; ++i) {
			while (1) {
				reg_val = ReadREG(AUXADC_DAT0 + i * 0x04);

				if (reg_val & (1 << 12)) {
					break;
				} else {
					printk("[adc_api]: wait for channel[%d] data ready\n", i);
					mdelay(100);

					if (++data_ready_count > 30) {
						/* wait for idle time out */
						printk
						    ("[adc_api]: wait for channel[%d] data ready time out\n",
						     i);
						return -2;
					}
				}
			}

			data[i] = (reg_val & 0x0FFF) * 1500 / 4096;
		}
		mdelay(20);

		if (adc_using_set_clr_mode) {
			/* clear bit */
			WriteREG(AUXADC_CON1_CLR, 0xFFFF);
		} else {
			/* clear bit */
			MskWriteREG(AUXADC_CON1, 0, 0xFFFF);
		}

		return 0;
	}
}


/* handle adc timer trigger */
#define AUXADC_GET_EACH_CHANNEL_ONE_BY_ONE  0
#define AUXADC_GET_EACH_CHANNEL_TOGETHER    1
#define AUXADC_GET_EACH_CHANNEL_METHOD      AUXADC_GET_EACH_CHANNEL_ONE_BY_ONE
void adc_udvt_tasklet_fn(unsigned long unused)
{
	unsigned int i = 0
#if (AUXADC_GET_EACH_CHANNEL_METHOD == AUXADC_GET_EACH_CHANNEL_ONE_BY_ONE)
	, data[4] = { 0 }
#elif (AUXADC_GET_EACH_CHANNEL_METHOD == AUXADC_GET_EACH_CHANNEL_TOGETHER)
	, data_all_channel[16] = { 0 }
#endif
	;
	/* unsigned int poweron, poweroff; */
	int res = 0;

	/* if (adc_mode) */
	/* read data */
	printk("[adc_api]: adc_using_set_clr_mode=%d\n", adc_using_set_clr_mode);
#if (AUXADC_GET_EACH_CHANNEL_METHOD == AUXADC_GET_EACH_CHANNEL_ONE_BY_ONE)
	for (i = 0; i < 16; i++) {
		res = IMM_GetOneChannelValue(i, data);

		if (res < 0) {
			printk("[adc_udvt]: get data error\n");
			break;
		} else {
			printk("[adc_udvt]: channel[%2u]=%4u mV\n", i, data[0]);
		}
	}
#elif (AUXADC_GET_EACH_CHANNEL_METHOD == AUXADC_GET_EACH_CHANNEL_TOGETHER)
	res = IMM_GetAllChannelValue(data_all_channel);
	if (res < 0) {
		printk("[adc_udvt]: get data error\n");
	} else {
		for (i = 0; i < 16; ++i) {
			printk("[adc_udvt]: channel[%2u]=%4u mV\n", i, data_all_channel[i]);
		}
	}
#endif
	dump_register();
	mod_timer(&adc_udvt_timer, jiffies + 50);
	return;
}

/* timer keep polling touch panel status */
void adc_udvt_timer_fn(unsigned long arg)
{
	if (!adc_run) {
		mod_timer(&adc_udvt_timer, jiffies + 500);
		dump_register();
		return;
	}

	if (adc_log_en) {
		printk("[adc_udvt]: timer trigger\n");
	}

	if (adc_udvt_tasklet.state != TASKLET_STATE_RUN) {
		tasklet_hi_schedule(&adc_udvt_tasklet);
	}
}

/* handle interrupt from adc */
irqreturn_t adc_udvt_handler(int irq, void *dev_id)
{
	if ((adc_log_en && adc_bg_detect_en) || adc_wakeup_src_en) {
		printk("[adc_udvt]: interrupt trigger\n");
	}

	return IRQ_HANDLED;
}

static long adc_udvt_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	/* void __user *uarg = (void __user *)arg; */
	struct udvt_cmd *pcmd = (struct udvt_cmd *)arg;

	/* printk("cmd:%d, value:0x%x\n", pcmd->cmd, pcmd->value); */

	switch (pcmd->cmd) {
	case ENABLE_ADC_CLOCK:
		if (enable_clock(MT_CG_PERI_AUXADC_PDN, "AUXADC") == 0) {
			printk("[adc_udvt]: ENABLE_ADC_CLOCK\n");
		} else {
			printk("[adc_udvt]: enable_clock failed.\n");
		}
		break;

	case DISABLE_ADC_CLOCK:
		if (disable_clock(MT_CG_PERI_AUXADC_PDN, "AUXADC") == 0) {
			printk("[adc_udvt]: DISABLE_ADC_CLOCK\n");
		} else {
			printk("[adc_udvt]: disable_clock failed.\n");
		}
		break;

	case ENABLE_ADC_DCM:
		printk("[adc_udvt]: dcm_enable(ALL_DCM)\n");
		dcm_enable(ALL_DCM);
		break;

	case ENABLE_ADC_AUTO_SET:
		adc_auto_set = 1;
		adc_auto_set_need_trigger_1st_sample = 0xffff;
		WriteREG(AUXADC_CON0, 0xffff);
		break;

	case DISABLE_ADC_AUTO_SET:
		adc_auto_set = 0;
		adc_auto_set_need_trigger_1st_sample = 0;
		WriteREG(AUXADC_CON0, 0x0000);
		break;

	case SET_AUXADC_CON0:
		WriteREG(AUXADC_CON0, pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_CON1:
		WriteREG(AUXADC_CON1, pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_CON1_SET:
		printk("[adc_udvt]: adc_using_set_clr_mode = 1\n");
		adc_using_set_clr_mode = 1;
		break;

	case SET_AUXADC_CON1_CLR:
		printk("[adc_udvt]: adc_using_set_clr_mode = 0\n");
		adc_using_set_clr_mode = 0;
		break;

	case SET_AUXADC_CON2:
		*(volatile u16 *)AUXADC_CON2 = (u16) (pcmd->value & 0xFFFF);
		break;
#if 0

	case SET_AUXADC_CON3:
		*(volatile u16 *)AUXADC_CON3 = (u16) (pcmd->value & 0xFFFF);
		break;
#endif

	case SET_AUXADC_DAT0:
		*(volatile u16 *)AUXADC_DAT0 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT1:
		*(volatile u16 *)AUXADC_DAT1 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT2:
		*(volatile u16 *)AUXADC_DAT2 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT3:
		*(volatile u16 *)AUXADC_DAT3 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT4:
		*(volatile u16 *)AUXADC_DAT4 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT5:
		*(volatile u16 *)AUXADC_DAT5 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT6:
		*(volatile u16 *)AUXADC_DAT6 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT7:
		*(volatile u16 *)AUXADC_DAT7 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT8:
		*(volatile u16 *)AUXADC_DAT8 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT9:
		*(volatile u16 *)AUXADC_DAT9 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT10:
		*(volatile u16 *)AUXADC_DAT10 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT11:
		*(volatile u16 *)AUXADC_DAT11 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT12:
		*(volatile u16 *)AUXADC_DAT12 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT13:
		*(volatile u16 *)AUXADC_DAT13 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT14:
		*(volatile u16 *)AUXADC_DAT13 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT15:
		*(volatile u16 *)AUXADC_DAT13 = (u16) (pcmd->value & 0xFFFF);
		break;

	case GET_AUXADC_CON0:
		pcmd->value = ((*(volatile u16 *)AUXADC_CON0) & 0xFFFF);
		break;

	case GET_AUXADC_CON1:
		pcmd->value = ((*(volatile u16 *)AUXADC_CON1) & 0xFFFF);
		break;

	case GET_AUXADC_CON2:
		pcmd->value = ((*(volatile u16 *)AUXADC_CON2) & 0xFFFF);
		break;
#if 0

	case GET_AUXADC_CON3:
		pcmd->value = ((*(volatile u16 *)AUXADC_CON3) & 0xFFFF);
		break;
#endif

	case GET_AUXADC_DAT0:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT0) & 0xFFFF);
		break;

	case GET_AUXADC_DAT1:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT1) & 0xFFFF);
		break;

	case GET_AUXADC_DAT2:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT2) & 0xFFFF);
		break;

	case GET_AUXADC_DAT3:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT3) & 0xFFFF);
		break;

	case GET_AUXADC_DAT4:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT4) & 0xFFFF);
		break;

	case GET_AUXADC_DAT5:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT5) & 0xFFFF);
		break;

	case GET_AUXADC_DAT6:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT6) & 0xFFFF);
		break;

	case GET_AUXADC_DAT7:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT7) & 0xFFFF);
		break;

	case GET_AUXADC_DAT8:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT8) & 0xFFFF);
		break;

	case GET_AUXADC_DAT9:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT9) & 0xFFFF);
		break;

	case GET_AUXADC_DAT10:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT10) & 0xFFFF);
		break;

	case GET_AUXADC_DAT11:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT11) & 0xFFFF);
		break;

	case GET_AUXADC_DAT12:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT12) & 0xFFFF);
		break;

	case GET_AUXADC_DAT13:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT13) & 0xFFFF);
		break;

	case GET_AUXADC_DAT14:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT13) & 0xFFFF);
		break;

	case GET_AUXADC_DAT15:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT13) & 0xFFFF);
		break;
#if 0

	case ENABLE_SYN_MODE:
		adc_mode = 1;
		break;

	case DISABLE_SYN_MODE:
		adc_mode = 0;
		break;
#endif

	case ENABLE_ADC_RUN:
		adc_run = 1;
#if 0
		printk("hwDisableClock AUXADC.");

		if (disable_clock(MT65XX_PDN_PERI_AUXADC, "AUXADC")) {
			printk("hwEnableClock AUXADC failed.");
		}
#endif
		break;

	case DISABLE_ADC_RUN:
		adc_run = 0;
		break;

	case ENABLE_ADC_LOG:
		adc_log_en = 1;
		break;

	case DISABLE_ADC_LOG:
		adc_log_en = 0;
		break;

	case ENABLE_BG_DETECT:
		adc_bg_detect_en = 1;
		break;

	case DISABLE_BG_DETECT:
		adc_bg_detect_en = 0;
		break;
#if 0

	case ENABLE_3G_TX:
		adc_3g_tx_en = 1;
		break;

	case DISABLE_3G_TX:
		adc_3g_tx_en = 0;
		break;

	case ENABLE_2G_TX:
		adc_2g_tx_en = 1;
		break;

	case DISABLE_2G_TX:
		adc_2g_tx_en = 0;
		break;
#endif

	case SET_ADC_WAKE_SRC:
		/* slp_set_wakesrc(WAKE_SRC_CFG_KEY|WAKE_SRC_LOW_BAT, TRUE, FALSE); */
		adc_wakeup_src_en = 1;
		break;

	case SET_DET_VOLT:
		(*(volatile u16 *)AUXADC_DET_VOLT) = pcmd->value;
		printk("AUXADC_DET_VOLT: 0x%x\n", (*(volatile u16 *)AUXADC_DET_VOLT));
		break;

	case SET_DET_PERIOD:
		(*(volatile u16 *)AUXADC_DET_PERIOD) = pcmd->value;
		printk("AUXADC_DET_PERIOD: 0x%x\n", (*(volatile u16 *)AUXADC_DET_PERIOD));
		break;

	case SET_DET_DEBT:
		(*(volatile u16 *)AUXADC_DET_DEBT) = pcmd->value;
		printk("AUXADC_DET_DEBT: 0x%x\n", (*(volatile u16 *)AUXADC_DET_DEBT));
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int adc_udvt_dev_open(struct inode *inode, struct file *file)
{
	/* if(hwEnableClock(MT65XX_PDN_PERI_TP,"Touch")==FALSE) */
	/* printk("hwEnableClock TP failed.\n"); */
	mod_timer(&adc_udvt_timer, jiffies + 500);
	return 0;
}

static struct file_operations adc_udvt_dev_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = adc_udvt_dev_ioctl,
	.open = adc_udvt_dev_open,
};

static struct miscdevice adc_udvt_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = ADC_NAME,
	.fops = &adc_udvt_dev_fops,
};

static int adc_default_register_value_check(void)
{
	int i;
	u16 reg_val;

	printk("[adc_udvt]: Enter adc_default_register_value_check() AUXADC_CON0=0x%08x\n",
	       AUXADC_CON0);

	if ((reg_val = ReadREG(AUXADC_CON0)) != 0x0000) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_CON0)=0x%04x is not the default value 0x0000\n",
		     reg_val);
	}

	if ((reg_val = ReadREG(AUXADC_CON1)) != 0x0000) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_CON1)=0x%04x is not the default value 0x0000\n",
		     reg_val);
	}

	if ((reg_val = ReadREG(AUXADC_CON1_SET)) != 0x0000) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_CON1_SET)=0x%04x is not the default value 0x0000\n",
		     reg_val);
	}

	if ((reg_val = ReadREG(AUXADC_CON1_CLR)) != 0x0000) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_CON1_CLR)=0x%04x is not the default value 0x0000\n",
		     reg_val);
	}

	if ((reg_val = ReadREG(AUXADC_CON2)) != 0x0000) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_CON2)=0x%04x is not the default value 0x0000\n",
		     reg_val);
	}

	for (i = 0; i < 16; ++i) {
		if ((reg_val = ReadREG(AUXADC_DAT0 + i * 4)) != 0x0000) {
			printk
			    ("[adc_udvt]: ERR: ReadREG(AUXADC_DAT%i)=0x%04x is not the default value 0x0000\n",
			     i, reg_val);
		}
	}

	if ((reg_val = ReadREG(AUXADC_DET_VOLT)) != 0x0000) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_DET_VOLT)=0x%04x is not the default value 0x0000\n",
		     reg_val);
	}

	if ((reg_val = ReadREG(AUXADC_DET_SEL)) != 0x0000) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_DET_SEL)=0x%04x is not the default value 0x0000\n",
		     reg_val);
	}

	if ((reg_val = ReadREG(AUXADC_DET_PERIOD)) != 0x0000) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_DET_PERIOD)=0x%04x is not the default value 0x0000\n",
		     reg_val);
	}

	if ((reg_val = ReadREG(AUXADC_DET_DEBT)) != 0x0001) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_DET_DEBT)=0x%04x is not the default value 0x0001\n",
		     reg_val);
	}

	if ((reg_val = ReadREG(AUXADC_MISC)) != 0x0008) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_MISC)=0x%04x is not the default value 0x0008\n",
		     reg_val);
	}

	if ((reg_val = ReadREG(AUXADC_ECC)) != 0x0081) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_ECC)=0x%04x is not the default value 0x0081\n",
		     reg_val);
	}

	if ((reg_val = ReadREG(AUXADC_SAMPLE_LIST)) != 0x0000) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_SAMPLE_LIST)=0x%04x is not the default value 0x0000\n",
		     reg_val);
	}

	if ((reg_val = ReadREG(AUXADC_ABIST_PERIOD)) != 0x0000) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_ABIST_PERIOD)=0x%04x is not the default value 0x0000\n",
		     reg_val);
	}

	if ((reg_val = ReadREG(AUXADC_TST)) != 0x0100) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_TST)=0x%04x is not the default value 0x0100\n",
		     reg_val);
	}

	if ((reg_val = ReadREG(AUXADC_SPL_EN)) != 0x0000) {
		printk
		    ("[adc_udvt]: ERR: ReadREG(AUXADC_SPL_EN)=0x%04x is not the default value 0x0000\n",
		     reg_val);
	}

	printk("[adc_udvt]: Return adc_default_register_value_check()\n");

	return 0;
}

int adc_udvt_local_init(void)
{
	init_timer(&adc_udvt_timer);
	adc_udvt_timer.function = adc_udvt_timer_fn;
	tasklet_init(&adc_udvt_tasklet, adc_udvt_tasklet_fn, 0);
	mt_irq_set_sens(MT65XX_IRQ_LOWBAT_LINE, MT_EDGE_SENSITIVE);
	mt_irq_set_polarity(MT65XX_IRQ_LOWBAT_LINE, MT_POLARITY_LOW);

	if (request_irq(MT65XX_IRQ_LOWBAT_LINE, adc_udvt_handler, 0, "_ADC_UDVT", NULL)) {
		printk("[adc_udvt]: request_irq failed.\n");
	}
	/* don't need to enable clock? */
	/* print all registers here!! */
	adc_default_register_value_check();

	WriteREG(AUXADC_CON0, 0x0000);
	WriteREG(AUXADC_CON1, 0x0000);
	WriteREG(AUXADC_CON2, 0x0000);

	/* Change the period to 0 to disable background detection before we change the configuration */
	WriteREG(AUXADC_DET_PERIOD, 0);
	/* Set the Debounce Time */
	WriteREG(AUXADC_DET_DEBT, 0x20);
	/* set the voltage for background detection(bit 15 set higher or lower) */
	/* *(volatile u16 *)AUXADC_DET_VOLT = 0x000F; */
	WriteREG(AUXADC_DET_VOLT, 0x83FF);
	/* Select the Channel */
	/* *(volatile u16 *)AUXADC_DET_SEL =0x6;// 0x8; */
	WriteREG(AUXADC_DET_SEL, 0x0);	/* 0x8; */
	/* Change the Period */
	WriteREG(AUXADC_DET_PERIOD, 0xA0);
	return 0;
}

/* static DEFINE_SPINLOCK(g_adc_lock); */
/* static unsigned long g_adc_flags; */

static int __init adc_udvt_mod_init(void)
{
	int ret;
	/* *(volatile u16 *)0 = 0xA0; */
/* spin_lock_irqsave(&g_adc_lock, g_adc_flags); */
	/* spin_lock_irqsave(&g_gpt_lock, g_gpt_flags); */
	ret = misc_register(&adc_udvt_dev);

	if (ret) {
		printk("[adc_udvt]: register driver failed (%d)\n", ret);
		return ret;
	}

	adc_udvt_local_init();
	printk("[adc_udvt]: adc_udvt_local_init initialization\n");
	return 0;
}

static void __exit adc_udvt_mod_exit(void)
{
	int ret;
	ret = misc_deregister(&adc_udvt_dev);

	if (ret) {
		printk("[ts_udvt]: unregister driver failed\n");
	}
}
module_init(adc_udvt_mod_init);
module_exit(adc_udvt_mod_exit);

MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("MT8135 AUXADC Driver for UDVT");
MODULE_LICENSE("GPL");

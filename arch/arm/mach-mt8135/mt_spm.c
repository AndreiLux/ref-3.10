#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/export.h>
#include <linux/kobject.h>
#include <linux/string.h>

#include <mach/irqs.h>
#include <mach/mt_spm.h>
#include <mach/wd_api.h>
#include <mach/mt_boot.h>

/**********************************************************
 * PCM code for normal (v4 @ 2013-07-31)
 **********************************************************/
static const u32 spm_normal[] = {
	0x1840001f, 0x00000001, 0x1a40001f, 0x00000000, 0x1a80001f, 0x00000000,
	0xe8208000, 0x10200138, 0x0000000e, 0xe8208000, 0x1020021c, 0x0000000e,
	0x1890001f, 0x10200264, 0x18c0001f, 0x10200264, 0x80b48402, 0xe0c00002,
	0x1a10001f, 0x1000660c, 0x80a8a001, 0x80e92001, 0x80800c02, 0xd8000562,
	0x17c07c1f, 0x1890001f, 0x1020013c, 0x1082081f, 0x88800002, 0x00000003,
	0x18d0001f, 0x1020025c, 0x10c10c1f, 0x88c00003, 0x00000003, 0x1280241f,
	0xa2510c02, 0x81402809, 0xd8200565, 0x17c07c1f, 0xe8208000, 0x100063e0,
	0x00000002, 0x1890001f, 0x10006824, 0x1380081f, 0x1b00001f, 0x02200000,
	0x1b80001f, 0x2000000f, 0xd820084c, 0x17c07c1f, 0x809c840d, 0x80d7840d,
	0xa0800c02, 0xd8200842, 0x17c07c1f, 0xa1d78407, 0x1890001f, 0x10006014,
	0x18c0001f, 0x10006014, 0xa0978402, 0xe0c00002, 0x1b80001f, 0x00001000,
	0xd0000240, 0x17c07c1f, 0xf0000000
};

#define PCM_NORMAL_BASE        __pa(spm_normal)
#define PCM_NORMAL_LEN         (69 - 1)

/**********************************************************
 * PCM code for normal (v2 @ 2013-07-16)
 **********************************************************/
static const u32 spm_normal_alt[] = {
	0x1840001f, 0x00000001, 0x1b00001f, 0x02200000, 0x1b80001f, 0x80001000,
	0x809c840d, 0x80d7840d, 0xa0800c02, 0xd8200042, 0x17c07c1f, 0xa1d78407,
	0x1890001f, 0x10006014, 0x18c0001f, 0x10006014, 0xa0978402, 0xe0c00002,
	0x1b80001f, 0x00001000, 0xf0000000
};

#define PCM_NORMAL_BASE_ALT        __pa(spm_normal_alt)
#define PCM_NORMAL_LEN_ALT         (21 - 1)

/**************************************
 * for General
 **************************************/
#define spm_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

DEFINE_SPINLOCK(spm_lock);



int pcm_select = 0;
int force_infra_pdn = 0xFF;

/**************************************
 * SW code for general and normal
 **************************************/
#define WAKE_SRC_FOR_NORMAL     (WAKE_SRC_THERM | WAKE_SRC_THERM2)

/**************************************
 * SW code for WFE enhance
 **************************************/
#define WFE_ENHANCE

void spm_go_to_normal(void)
{
	unsigned long flags;

	spin_lock_irqsave(&spm_lock, flags);

	/* make sure SPM APB register is accessible */
	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	/* reset PCM */
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY | CON0_PCM_SW_RESET);
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY);

	/* init PCM_CON1 (disable non-replace mode) (enable external path) */
	spm_write(SPM_PCM_CON1, CON1_CFG_KEY);


	/* tell IM where is PCM code */
	BUG_ON(PCM_NORMAL_BASE & 0x00000003);	/* check 4-byte alignment */
	switch (pcm_select) {
	case 0:
		spm_write(SPM_PCM_IM_PTR, PCM_NORMAL_BASE);
		spm_write(SPM_PCM_IM_LEN, PCM_NORMAL_LEN);
		break;
	case 1:
		spm_write(SPM_PCM_IM_PTR, PCM_NORMAL_BASE_ALT);
		spm_write(SPM_PCM_IM_LEN, PCM_NORMAL_LEN_ALT);
		break;
	default:
		spm_write(SPM_PCM_IM_PTR, PCM_NORMAL_BASE);
		spm_write(SPM_PCM_IM_LEN, PCM_NORMAL_LEN);
		break;
	}
	/* unmask wakeup source */
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, ~WAKE_SRC_FOR_NORMAL);

#if defined(CONFIG_FIQ) && defined(WFE_ENHANCE)
	if (mt_get_chip_sw_ver() <= CHIP_SW_VER_02) {
		spm_write(SPM_SLEEP_ISR_MASK, 0x0D04);	/* unmask SPM_IRQ1 */
		spm_write(SPM_PCM_WDT_TIMER_VAL, 0x200019C8);	/* 100us */
		/* spm_write(SPM_PCM_WDT_TIMER_VAL,  0x200101D0); // 1ms */
		/* spm_write(SPM_PCM_WDT_TIMER_VAL,  0x200A1220); // 10ms */
		/* spm_write(SPM_PCM_WDT_TIMER_VAL,  0x2064B540); // 100ms */
	}
#endif

	/* kick IM and PCM to run */
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY | CON0_IM_KICK | CON0_PCM_KICK);
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY);
	spin_unlock_irqrestore(&spm_lock, flags);
}

static irqreturn_t spm_irq0_handler(int irq, void *dev_id)
{
	spm_error("!!! SPM ISR 0 SHOULD NOT BE EXECUTED !!!\n");

	spin_lock(&spm_lock);
	/* clean ISR status */
	spm_write(SPM_SLEEP_ISR_MASK, 0x0F0C);	/* PCM_ISR_ROOT_MASK, PCM_TWAM_ISR_MASK */
	spm_write(SPM_SLEEP_ISR_STATUS, 0x000C);	/* ISR_PCM, ISR_TWAM_IRQ */
	spm_write(SPM_PCM_SW_INT_CLEAR, 0x000F);

	spin_unlock(&spm_lock);

	return IRQ_HANDLED;
}

#if defined(CONFIG_FIQ) && defined(WFE_ENHANCE)
static int wfe_monitor_times;
static void wfe_monitor_fiq(void *arg, void *regs, void *svc_sp)
{
	spm_write(SPM_SLEEP_ISR_STATUS, 0x000C);
	spm_write(SPM_PCM_SW_INT_CLEAR, 0x0002);

	wfe_monitor_times++;
	sev();
}

static int __init wfe_monitor_init(void)
{
	int ret = 0;

	if (mt_get_chip_sw_ver() <= CHIP_SW_VER_02) {
		ret = request_fiq(MT_SPM_IRQ1_ID, wfe_monitor_fiq, IRQF_TRIGGER_LOW, NULL);
		if (ret != 0) {
			pr_info("%s: failed to request irq (%d)\n", __func__, ret);
			WARN_ON(1);
			return ret;
		}
	}

	return ret;
}
#endif


void __init spm_module_init(void)
{
	int r;
	unsigned long flags;

	spin_lock_irqsave(&spm_lock, flags);
	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	/* init power control register (select PCM clock to qaxi and SRCLKENA_MD=1) */
	spm_write(SPM_POWER_ON_VAL0, 0);
	spm_write(SPM_POWER_ON_VAL1, 0x00011830);
	spm_write(SPM_PCM_PWR_IO_EN, 0);

	/* reset PCM */
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY | CON0_PCM_SW_RESET);
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY);

	/* init PCM control register (disable event vector and PCM timer) */
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY | CON0_IM_SLEEP_DVS);
	spm_write(SPM_PCM_CON1, CON1_CFG_KEY | CON1_IM_NONRP_EN /*| CON1_MIF_APBEN */);

	/* SRCVOLTEN: r7[11] | !PCM_IM_SLP */
	/* SRCLKENA: SRCLKENI_OR | r7[14] */
	/* CLKSQ0_OFF: r0[20] */
	spm_write(SPM_CLK_CON, CC_SYSCLK0_EN_0 | CC_SYSCLK0_EN_1 | CC_SRCVOLTEN_MASK);

	/* clean wakeup event raw status */
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, 0xffffffff);

	/* clean ISR status */
	spm_write(SPM_SLEEP_ISR_MASK, 0x0F0C);	/* PCM_ISR_ROOT_MASK, PCM_TWAM_ISR_MASK */
	spm_write(SPM_SLEEP_ISR_STATUS, 0x000C);	/* ISR_PCM, ISR_TWAM_IRQ */
	spm_write(SPM_PCM_SW_INT_CLEAR, 0x000F);
	spin_unlock_irqrestore(&spm_lock, flags);

	r = request_irq(MT_SPM_IRQ0_ID, spm_irq0_handler, IRQF_TRIGGER_LOW, "mt-spm-0", NULL);
	if (r) {
		spm_error("SPM IRQ 0 register failed (%d)\n", r);
		WARN_ON(1);
	}

	if (mt_get_chip_sw_ver() > CHIP_SW_VER_02) {
		pcm_select = 1;
	}

	spm_go_to_normal();	/* let PCM help to do thermal protection */

}


/**************************************
 * for CPU DVFS
 **************************************/
#define MAX_RETRY_COUNT (100)

int spm_dvfs_ctrl_volt(u32 value)
{
	u32 ap_dvfs_con;
	int retry = 0;

	ap_dvfs_con = spm_read(SPM_AP_DVFS_CON_SET);
	spm_write(SPM_AP_DVFS_CON_SET, (ap_dvfs_con & ~(0x7)) | value);
	udelay(5);

	while ((spm_read(SPM_AP_DVFS_CON_SET) & (0x1 << 31)) == 0) {
		if (retry >= MAX_RETRY_COUNT) {
			pr_info("FAIL: no response from PMIC wrapper\n");
			return -1;
		}

		retry++;
		pr_info("wait for ACK signal from PMIC wrapper, retry = %d\n", retry);

		udelay(5);
	}

	return 0;
}


/* Sysfs Implementation  */
#if defined(CONFIG_FIQ) && defined(WFE_ENHANCE)
static int spm_fiq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	char *p = buf;

	p += sprintf(p, "FIQ:%d\n", wfe_monitor_times);

	len = p - buf;

	return len;
}

static ssize_t spm_fiq_store(struct kobject *kobj,
			     struct kobj_attribute *attr, const char *buf, size_t n)
{
	return 0;
}

spm_attr(spm_fiq);

static ssize_t spm_latency_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	int latency;
	char *p = buf;

	latency = (spm_read(SPM_PCM_WDT_TIMER_VAL) & ~0xF0000000) / 66;	/* unit:us */

	p += sprintf(p, "%dus\n", latency);

	len = p - buf;

	return len;
}

static ssize_t spm_latency_store(struct kobject *kobj,
				 struct kobj_attribute *attr, const char *buf, size_t n)
{
	int latency, reg_value;

	if (buf[0] == '0' || buf[0] == 'x') {
		if (sscanf(buf, "%x", &latency) != 1)
			latency = 100;
	} else {
		if (sscanf(buf, "%d", &latency) != 1)
			latency = 100;
  }

	reg_value = latency * 66;

	spm_write(SPM_PCM_WDT_TIMER_VAL, 0x20000000 | reg_value);

	return n;
}

spm_attr(spm_latency);
#endif

static ssize_t spm_suspend_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	char *p = buf;

	p += sprintf(p, "slp_cpu_pdn(%d), slp_infra_pdn(%d), slp_pcm_timer(%d),\n", slp_cpu_pdn,
		     slp_infra_pdn, slp_pcm_timer);
	p += sprintf(p,
		     "slp_ck26m_on(%d), slp_chk_golden(%d), slp_dump_gpio(%d), console_suspend_enabled(%d)\n",
		     slp_ck26m_on, slp_chk_golden, slp_dump_gpio, console_suspend_enabled);
	p += sprintf(p,
		     "slp_dump_regs(%d), spm_sleep_wakesrc(0x%X), pcm_select(%d), force_infra_pdn(%d)\n",
		     slp_dump_regs, spm_sleep_wakesrc, pcm_select, force_infra_pdn);

	len = p - buf;
	return len;
}

static ssize_t spm_suspend_store(struct kobject *kobj,
				 struct kobj_attribute *attr, const char *buf, size_t n)
{
	char field[30];
	int value;

	if (buf[0] == '0' || buf[0] == 'x') {
		if (sscanf(buf, "%x%29s", &value, field) != 2)
			return n;
	}	else {
		if (sscanf(buf, "%d%29s", &value, field) != 2)
			return n;
	}

	if (strcmp(field, "cpu_pdn") == 0)
		slp_cpu_pdn = value;
	else if (strcmp(field, "infra_pdn") == 0)
		force_infra_pdn = value;
	else if (strcmp(field, "timer_val") == 0)
		slp_pcm_timer = value;
	else if (strcmp(field, "ck26m_on") == 0)
		slp_ck26m_on = value;
	else if (strcmp(field, "chk_golden") == 0)
		slp_chk_golden = value;
	else if (strcmp(field, "dump_gpio") == 0)
		slp_dump_gpio = value;
	else if (strcmp(field, "dump_reg") == 0)
		slp_dump_regs = value;
	else if (strcmp(field, "wake_src") == 0)
		spm_sleep_wakesrc = value;
	else if (strcmp(field, "sel") == 0)
		pcm_select = value;
	else if (strcmp(field, "console") == 0)
		console_suspend_enabled = value;

	return n;
}

spm_attr(spm_suspend);

struct kobject *spm_kobj;
EXPORT_SYMBOL_GPL(spm_kobj);

static struct attribute *g[] = {
	&spm_suspend_attr.attr,
#if defined(CONFIG_FIQ) && defined(WFE_ENHANCE)
	&spm_latency_attr.attr,
	&spm_fiq_attr.attr,
#endif
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

void __init spm_fs_init(void)
{
	int error = 0;

	spm_kobj = kobject_create_and_add("spm", NULL);
	if (!spm_kobj) {
		spm_notice("[%s]: spm_kobj create failed\n", __func__);
		return;
	}

	error = sysfs_create_group(spm_kobj, &attr_group);
	if (error) {
		spm_notice("[%s]: spm_kobj->attr_group create failed\n", __func__);
		return;
	}
}

#if defined(CONFIG_FIQ) && defined(WFE_ENHANCE)
module_init(wfe_monitor_init);
#endif
MODULE_DESCRIPTION("MT8135 SPM Driver v0.9");

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/platform_device.h>
#include <linux/export.h>
#include <linux/kobject.h>
#include <linux/string.h>

#include "mach/mt_typedefs.h"
#include "mach/irqs.h"
#include "mach/mt_ptp.h"
#include <mach/mt_spm_idle.h>
#include "mach/mt_reg_base.h"
#include "mach/mt_cpufreq.h"
#include "mach/mt_thermal.h"

int ptp_log = 0;

#define ptp_emerg(fmt, args...)\
do { if (ptp_log)\
	pr_emerg("[PTP] " fmt, ##args);\
} while (0)
#define ptp_alert(fmt, args...)\
do { if (ptp_log)\
	pr_alert("[PTP] " fmt, ##args);\
} while (0)
#define ptp_crit(fmt, args...)\
do { if (ptp_log)\
	pr_crit("[PTP] " fmt, ##args);\
} while (0)
#define ptp_error(fmt, args...)	\
do { if (ptp_log)\
	pr_err("[PTP] " fmt, ##args);\
} while (0)
#define ptp_warning(fmt, args...)\
do { if (ptp_log)\
	pr_warn("[PTP] " fmt, ##args);\
} while (0)
#define ptp_notice(fmt, args...)\
do { if (ptp_log)\
	pr_notice("[PTP] " fmt, ##args);\
} while (0)
#define ptp_info(fmt, args...)\
do { if (ptp_log)\
	pr_info("[PTP] " fmt, ##args);\
} while (0)
#define ptp_debug(fmt, args...)\
do { if (ptp_log)\
	pr_debug("[PTP] " fmt, ##args);\
} while (0)

#if En_ISR_log
#define ptp_isr_info(fmt, args...)     ptp_notice(fmt, ##args)
#else
#define ptp_isr_info(fmt, args...)     ptp_debug(fmt, ##args)
#endif

#define ptp_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

u32 PTP_Enable = 1;
#if En_PTP_OD
volatile u32 ptp_data[6] = { 0xffffffff, 0, 0, 0xffffffff, 0, 0 };
#else
volatile u32 ptp_data[6] = { 0, 0, 0, 0, 0, 0 };
#endif
volatile u32 ptp_returndata[1] = { 0 };

u32 PTP_output_voltage_failed = 0;

u32 val_0 = 0x0F185707;
u32 val_1 = 0xEB555555;
u32 val_2 = 0x00000000;
u32 val_3 = 0x40360400;
u32 val_4 = 0xE10F1857;
u32 val_5 = 0x00555555;
u32 val_6 = 0x00000000;

unsigned int ptpod_ca7_pmic_volt[8] = { 0x51, 0x47, 0x37, 0x27, 0x17, 0, 0, 0 };
unsigned int ptpod_ca15_pmic_volt[8] = { 0x51, 0x47, 0x37, 0x27, 0x17, 0, 0, 0 };

/* CA7 PTP */
u8 ca7_freq_0, ca7_freq_1, ca7_freq_2, ca7_freq_3;
u8 ca7_freq_4, ca7_freq_5, ca7_freq_6, ca7_freq_7;

/* CA15 PTP */
u8 ca15_freq_0, ca15_freq_1, ca15_freq_2, ca15_freq_3;
u8 ca15_freq_4, ca15_freq_5, ca15_freq_6, ca15_freq_7;

u32 ptp_ca7_level = 0, ptp_ca15_level = 0;

u32 ptp_dcm_cfg;

static void PTP_set_ptp_ca7_volt(void)
{
#if Set_PMIC_Volt
	u32 array_size;

	array_size = 0;

	/* set PTP_CA7_VO_0 ~ PTP_CA7_VO_7 to PMIC */
	if (ca7_freq_0 != 0) {
		/* Voltage = 0.7V + VPROC_VOSEL_ON * 0.00625V */
		/* 1.5G : 1_05v, 1.4G : 1_05v, 1.3G : 1_000v, 1.2G : 1_000v */
		if (((ptp_ca7_level == 3) && (PTP_CA7_VO_0 < 56))
		    || ((ptp_ca7_level == 2) && (PTP_CA7_VO_0 < 56)) || ((PTP_CA7_VO_0 < 48))) {
			/* restore default DVFS table (PMIC) */
			ptp_crit("PTP Error : ptp_ca7_level = 0x%x, PTP_CA7_VO_0 = 0x%x\n",
				 ptp_ca7_level, PTP_CA7_VO_0);
			mt_cpufreq_return_default_DVS_by_ptpod(0);
			PTP_output_voltage_failed = 1;

			return;
		}
		ptpod_ca7_pmic_volt[0] = PTP_CA7_VO_0;
		array_size++;
	}

	if (ca7_freq_1 != 0) {
		/* 1_000v */
		if ((PTP_CA7_VO_1 < 48)) {
			/* restore default DVFS table (PMIC) */
			ptp_crit("PTP Error : ptp_ca7_level = 0x%x, PTP_CA7_VO_1 = 0x%x\n",
				 ptp_ca7_level, PTP_CA7_VO_1);
			mt_cpufreq_return_default_DVS_by_ptpod(0);
			PTP_output_voltage_failed = 1;

			return;
		}
		ptpod_ca7_pmic_volt[1] = PTP_CA7_VO_1;
		array_size++;
	}

	if (ca7_freq_2 != 0) {
		ptpod_ca7_pmic_volt[2] = PTP_CA7_VO_2;
		array_size++;
	}

	if (ca7_freq_3 != 0) {
		ptpod_ca7_pmic_volt[3] = PTP_CA7_VO_3;
		array_size++;
	}

	if (ca7_freq_4 != 0) {
		ptpod_ca7_pmic_volt[4] = PTP_CA7_VO_4;
		array_size++;
	}

	if (ca7_freq_5 != 0) {
		ptpod_ca7_pmic_volt[5] = PTP_CA7_VO_5;
		array_size++;
	}

	if (ca7_freq_6 != 0) {
		ptpod_ca7_pmic_volt[6] = PTP_CA7_VO_6;
		array_size++;
	}

	if (ca7_freq_7 != 0) {
		ptpod_ca7_pmic_volt[7] = PTP_CA7_VO_7;
		array_size++;
	}

	mt_cpufreq_voltage_set_by_ptpod(ptpod_ca7_pmic_volt, array_size, 0);

	ptp_crit("CA7(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n", ptpod_ca7_pmic_volt[0],
		 ptpod_ca7_pmic_volt[1], ptpod_ca7_pmic_volt[2], ptpod_ca7_pmic_volt[3],
		 ptpod_ca7_pmic_volt[4], ptpod_ca7_pmic_volt[5], ptpod_ca7_pmic_volt[6],
		 ptpod_ca7_pmic_volt[7]);

#endif
}

static void PTP_set_ptp_ca15_volt(void)
{
#if Set_PMIC_Volt
	u32 array_size;

	array_size = 0;

	/* set PTP_CA15_VO_0 ~ PTP_CA15_VO_7 to PMIC */
	if (ca15_freq_0 != 0) {
		/* 1.8G : 1_025v, 1.7G : 1_025v, 1.6G : 1_000v, 1.5G : 1_000v , 1.4G : 1_000v */
		if (((ptp_ca15_level == 3) && (PTP_CA15_VO_0 < 52))
		    || ((ptp_ca15_level == 2) && (PTP_CA15_VO_0 < 52)) || ((PTP_CA15_VO_0 < 48))) {
			/* restore default DVFS table (PMIC) */
			ptp_crit("PTP Error : ptp_ca15_level = 0x%x, PTP_CA15_VO_0 = 0x%x\n",
				 ptp_ca15_level, PTP_CA15_VO_0);
			mt_cpufreq_return_default_DVS_by_ptpod(1);
			PTP_output_voltage_failed = 1;

			return;
		}
		ptpod_ca15_pmic_volt[0] = PTP_CA15_VO_0;
		array_size++;
	}

	if (ca15_freq_1 != 0) {
		/* 1_000v */
		if ((PTP_CA15_VO_1 < 48)) {
			/* restore default DVFS table (PMIC) */
			ptp_crit("PTP Error : ptp_ca15_level = 0x%x, PTP_CA15_VO_1 = 0x%x\n",
				 ptp_ca15_level, PTP_CA15_VO_1);
			mt_cpufreq_return_default_DVS_by_ptpod(1);
			PTP_output_voltage_failed = 1;

			return;
		}
		ptpod_ca15_pmic_volt[1] = PTP_CA15_VO_1;
		array_size++;
	}

	if (ca15_freq_2 != 0) {
		ptpod_ca15_pmic_volt[2] = PTP_CA15_VO_2;
		array_size++;
	}

	if (ca15_freq_3 != 0) {
		ptpod_ca15_pmic_volt[3] = PTP_CA15_VO_3;
		array_size++;
	}

	if (ca15_freq_4 != 0) {
		ptpod_ca15_pmic_volt[4] = PTP_CA15_VO_4;
		array_size++;
	}

	if (ca15_freq_5 != 0) {
		ptpod_ca15_pmic_volt[5] = PTP_CA15_VO_5;
		array_size++;
	}

	if (ca15_freq_6 != 0) {
		ptpod_ca15_pmic_volt[6] = PTP_CA15_VO_6;
		array_size++;
	}

	if (ca15_freq_7 != 0) {
		ptpod_ca15_pmic_volt[7] = PTP_CA15_VO_7;
		array_size++;
	}

	mt_cpufreq_voltage_set_by_ptpod(ptpod_ca15_pmic_volt, array_size, 1);

	ptp_crit("CA15(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n", ptpod_ca15_pmic_volt[0],
		 ptpod_ca15_pmic_volt[1], ptpod_ca15_pmic_volt[2], ptpod_ca15_pmic_volt[3],
		 ptpod_ca15_pmic_volt[4], ptpod_ca15_pmic_volt[5], ptpod_ca15_pmic_volt[6],
		 ptpod_ca15_pmic_volt[7]);

#endif
}

irqreturn_t MT8135_PTP_CA7_ISR(int irq, void *dev_id)
{
	u32 PTPINTSTS, temp, temp_0, temp_ptpen;

	PTPINTSTS = ptp_read(PTP_CA7_PTPINTSTS);
	temp_ptpen = ptp_read(PTP_CA7_PTPEN);
	ptp_isr_info("===============ISR ISR ISR ISR ISR=============\n");
	ptp_isr_info("CA7_PTPINTSTS = 0x%x\n", PTPINTSTS);
	ptp_isr_info("CA7_PTP_PTPEN = 0x%x\n", temp_ptpen);
	ptp_data[1] = ptp_read(0xf100c240);
	ptp_data[2] = ptp_read(0xf100c27c);
	ptp_isr_info("*(0x1100c240) = 0x%x\n", ptp_data[1]);
	ptp_isr_info("*(0x1100c27c) = 0x%x\n", ptp_data[2]);
	ptp_data[0] = 0;

#if 0
	for (temp = 0xf100c200; temp <= 0xf100c25c; temp += 4) {
		ptp_isr_info("*(0x%x) = 0x%x\n", temp, ptp_read(temp));
	}
#endif

	if (PTPINTSTS == 0x1)	{/* PTP init1 or init2 */
		if ((temp_ptpen & 0x7) == 0x1) {/* init 1 ============================= */
#if 0
			/*
			   // read PTP_CA7_VO_0 ~ PTP_CA7_VO_3
			   temp = ptp_read( PTP_CA7_VOP30 );
			   PTP_CA7_VO_0 = temp & 0xff;
			   PTP_CA7_VO_1 = (temp>>8) & 0xff;
			   PTP_CA7_VO_2 = (temp>>16) & 0xff;
			   PTP_CA7_VO_3 = (temp>>24) & 0xff;

			   // read PTP_CA7_VO_3 ~ PTP_CA7_VO_7
			   temp = ptp_read( PTP_CA7_VOP74 );
			   PTP_CA7_VO_4 = temp & 0xff;
			   PTP_CA7_VO_5 = (temp>>8) & 0xff;
			   PTP_CA7_VO_6 = (temp>>16) & 0xff;
			   PTP_CA7_VO_7 = (temp>>24) & 0xff;

			   // show PTP_CA7_VO_0 ~ PTP_CA7_VO_7 to PMIC
			   ptp_isr_info("PTP_CA7_VO_0 = 0x%x\n", PTP_CA7_VO_0);
			   ptp_isr_info("PTP_CA7_VO_1 = 0x%x\n", PTP_CA7_VO_1);
			   ptp_isr_info("PTP_CA7_VO_2 = 0x%x\n", PTP_CA7_VO_2);
			   ptp_isr_info("PTP_CA7_VO_3 = 0x%x\n", PTP_CA7_VO_3);
			   ptp_isr_info("PTP_CA7_VO_4 = 0x%x\n", PTP_CA7_VO_4);
			   ptp_isr_info("PTP_CA7_VO_5 = 0x%x\n", PTP_CA7_VO_5);
			   ptp_isr_info("PTP_CA7_VO_6 = 0x%x\n", PTP_CA7_VO_6);
			   ptp_isr_info("PTP_CA7_VO_7 = 0x%x\n", PTP_CA7_VO_7);
			   ptp_isr_info("ptp_ca7_level = 0x%x\n", ptp_ca7_level);
			   ptp_isr_info("================================================\n");

			   PTP_set_ptp_ca7_volt();
			 */
#endif

			/* Read & store 16 bit values DCVALUES.DCVOFFSET and AGEVALUES.AGEVOFFSET for later use in INIT2 procedure */
			PTP_CA7_DCVOFFSET = ~(ptp_read(PTP_CA7_DCVALUES) & 0xffff) + 1;	/* hw bug, workaround */
			PTP_CA7_AGEVOFFSET = ptp_read(PTP_CA7_AGEVALUES) & 0xffff;

			PTP_CA7_INIT_FLAG = 1;

			/* Set PTPEN.PTPINITEN/PTPEN.PTPINIT2EN = 0x0 & Clear PTP INIT interrupt PTPINTSTS = 0x00000001 */
			ptp_write(PTP_CA7_PTPEN, 0x0);
			ptp_write(PTP_CA7_PTPINTSTS, 0x1);
			/* tasklet_schedule(&ptp_do_init2_tasklet); */
			if ((ptp_data[0] == 0) && (ptp_data[3] == 0))	{ /* check both CA7/CA15 PTP INIT I finished */
				mt_cpufreq_enable_by_ptpod();	/* enable DVFS */
				mt_fh_popod_restore();	/* enable frequency hopping (main PLL) */
			}
			PTP_CA7_INIT_02();
		} else if ((temp_ptpen & 0x7) == 0x5)	{ /* init 2 ============================= */
			/* read PTP_CA7_VO_0 ~ PTP_CA7_VO_3 */
			temp = ptp_read(PTP_CA7_VOP30);
			PTP_CA7_VO_0 = temp & 0xff;
			PTP_CA7_VO_1 = (temp >> 8) & 0xff;
			PTP_CA7_VO_2 = (temp >> 16) & 0xff;
			PTP_CA7_VO_3 = (temp >> 24) & 0xff;

			/* read PTP_CA7_VO_3 ~ PTP_CA7_VO_7 */
			temp = ptp_read(PTP_CA7_VOP74);
			PTP_CA7_VO_4 = temp & 0xff;
			PTP_CA7_VO_5 = (temp >> 8) & 0xff;
			PTP_CA7_VO_6 = (temp >> 16) & 0xff;
			PTP_CA7_VO_7 = (temp >> 24) & 0xff;

			/* save init2 PTP_init2_VO_0 ~ PTP_VO_7 */
			PTP_CA7_init2_VO_0 = PTP_CA7_VO_0;
			PTP_CA7_init2_VO_1 = PTP_CA7_VO_1;
			PTP_CA7_init2_VO_2 = PTP_CA7_VO_2;
			PTP_CA7_init2_VO_3 = PTP_CA7_VO_3;
			PTP_CA7_init2_VO_4 = PTP_CA7_VO_4;
			PTP_CA7_init2_VO_5 = PTP_CA7_VO_5;
			PTP_CA7_init2_VO_6 = PTP_CA7_VO_6;
			PTP_CA7_init2_VO_7 = PTP_CA7_VO_7;

			/* show PTP_VO_0 ~ PTP_VO_7 to PMIC */
			ptp_isr_info("PTP_CA7_VO_0 = 0x%x\n", PTP_CA7_VO_0);
			ptp_isr_info("PTP_CA7_VO_1 = 0x%x\n", PTP_CA7_VO_1);
			ptp_isr_info("PTP_CA7_VO_2 = 0x%x\n", PTP_CA7_VO_2);
			ptp_isr_info("PTP_CA7_VO_3 = 0x%x\n", PTP_CA7_VO_3);
			ptp_isr_info("PTP_CA7_VO_4 = 0x%x\n", PTP_CA7_VO_4);
			ptp_isr_info("PTP_CA7_VO_5 = 0x%x\n", PTP_CA7_VO_5);
			ptp_isr_info("PTP_CA7_VO_6 = 0x%x\n", PTP_CA7_VO_6);
			ptp_isr_info("PTP_CA7_VO_7 = 0x%x\n", PTP_CA7_VO_7);
			ptp_isr_info("ptp_ca7_level = 0x%x\n", ptp_ca7_level);
			ptp_isr_info("================================================\n");

			PTP_set_ptp_ca7_volt();

			if (PTP_output_voltage_failed == 1) {
				ptp_write(PTP_CA7_PTPEN, 0x0);	/* disable PTP */
				ptp_write(PTP_CA7_PTPINTSTS, 0x00ffffff);	/* Clear PTP interrupt PTPINTSTS */
				PTP_CA7_INIT_FLAG = 0;
				ptp_crit("disable CA7 PTP : PTP_output_voltage_failed(init_2).\n");
				return IRQ_HANDLED;
			}

			PTP_CA7_INIT_FLAG = 1;

			/* Set PTPEN.PTPINITEN/PTPEN.PTPINIT2EN = 0x0 & Clear PTP INIT interrupt PTPINTSTS = 0x00000001 */
			ptp_write(PTP_CA7_PTPEN, 0x0);
			ptp_write(PTP_CA7_PTPINTSTS, 0x1);
			/* tasklet_schedule(&ptp_do_mon_tasklet); */
			PTP_CA7_MON_MODE();
		} else {	/* error : init1 or init2 , but enable setting is wrong. */
			ptp_crit("====================================================\n");
			ptp_crit("PTP CA7 error_0 (0x%x) : CA7_PTPINTSTS = 0x%x\n", temp_ptpen,
				 PTPINTSTS);
			ptp_crit("PTP_CA7_SMSTATE0 (0x%x) = 0x%x\n", PTP_CA7_SMSTATE0,
				 ptp_read(PTP_CA7_SMSTATE0));
			ptp_crit("PTP_CA7_SMSTATE1 (0x%x) = 0x%x\n", PTP_CA7_SMSTATE1,
				 ptp_read(PTP_CA7_SMSTATE1));
			ptp_crit("====================================================\n");

			/* disable PTP */
			ptp_write(PTP_CA7_PTPEN, 0x0);

			/* Clear PTP interrupt PTPINTSTS */
			ptp_write(PTP_CA7_PTPINTSTS, 0x00ffffff);

			/* restore default DVFS table (PMIC) */
			mt_cpufreq_return_default_DVS_by_ptpod(0);

			PTP_CA7_INIT_FLAG = 0;
		}
	} else if ((PTPINTSTS & 0x00ff0000) != 0x0)	{ /* PTP Monitor mode */
		/* check if thermal sensor init completed? */
		temp_0 = (ptp_read(PTP_CA7_TEMP) & 0xff);	/* temp_0 */

		if ((temp_0 > 0x4b) && (temp_0 < 0xd3)) {
			ptp_crit
			    ("CA7 thermal sensor init has not been completed. (temp_0 = 0x%x)\n",
			     temp_0);
		} else {
			/* read PTP_CA7_VO_0 ~ PTP_CA7_VO_3 */
			temp = ptp_read(PTP_CA7_VOP30);
			PTP_CA7_VO_0 = temp & 0xff;
			PTP_CA7_VO_1 = (temp >> 8) & 0xff;
			PTP_CA7_VO_2 = (temp >> 16) & 0xff;
			PTP_CA7_VO_3 = (temp >> 24) & 0xff;

			/* read PTP_CA7_VO_3 ~ PTP_CA7_VO_7 */
			temp = ptp_read(PTP_CA7_VOP74);
			PTP_CA7_VO_4 = temp & 0xff;
			PTP_CA7_VO_5 = (temp >> 8) & 0xff;
			PTP_CA7_VO_6 = (temp >> 16) & 0xff;
			PTP_CA7_VO_7 = (temp >> 24) & 0xff;

			/* show PTP_CA7_VO_0 ~ PTP_CA7_VO_7 to PMIC */
			ptp_isr_info("===============ISR ISR ISR ISR Monitor mode=============\n");
			ptp_isr_info("PTP_CA7_VO_0 = 0x%x\n", PTP_CA7_VO_0);
			ptp_isr_info("PTP_CA7_VO_1 = 0x%x\n", PTP_CA7_VO_1);
			ptp_isr_info("PTP_CA7_VO_2 = 0x%x\n", PTP_CA7_VO_2);
			ptp_isr_info("PTP_CA7_VO_3 = 0x%x\n", PTP_CA7_VO_3);
			ptp_isr_info("PTP_CA7_VO_4 = 0x%x\n", PTP_CA7_VO_4);
			ptp_isr_info("PTP_CA7_VO_5 = 0x%x\n", PTP_CA7_VO_5);
			ptp_isr_info("PTP_CA7_VO_6 = 0x%x\n", PTP_CA7_VO_6);
			ptp_isr_info("PTP_CA7_VO_7 = 0x%x\n", PTP_CA7_VO_7);
			ptp_isr_info("ptp_ca7_level = 0x%x\n", ptp_ca7_level);
			ptp_isr_info("PTP_CA7_TEMP (0x%x) = 0x%x\n", PTP_CA7_TEMP,
				     ptp_read(PTP_CA7_TEMP));
			ptp_isr_info("ISR : CA7_TEMPSPARE1 = 0x%x\n", ptp_read(PTP_CA7_TEMPSPARE1));
			ptp_isr_info("=============================================\n");

			PTP_set_ptp_ca7_volt();

			if (PTP_output_voltage_failed == 1) {
				ptp_write(PTP_CA7_PTPEN, 0x0);	/* disable PTP */
				ptp_write(PTP_CA7_PTPINTSTS, 0x00ffffff);	/* Clear PTP interrupt PTPINTSTS */
				PTP_CA7_INIT_FLAG = 0;
				ptp_crit("disable CA7 PTP : PTP_output_voltage_failed(Mon).\n");
				return IRQ_HANDLED;
			}
		}

		/* Clear PTP INIT interrupt PTPINTSTS = 0x00ff0000 */
		ptp_write(PTP_CA7_PTPINTSTS, 0x00ff0000);
	} else			/* PTP error */
	{
		if (((temp_ptpen & 0x7) == 0x1) || ((temp_ptpen & 0x7) == 0x5))	{
/* init 1  || init 2 error handler ====================== */
			ptp_crit("====================================================\n");
			ptp_crit("PTP CA7 error_1 error_2 (0x%x) : CA7_PTPINTSTS = 0x%x\n",
				 temp_ptpen, PTPINTSTS);
			ptp_crit("PTP_CA7_SMSTATE0 (0x%x) = 0x%x\n", PTP_CA7_SMSTATE0,
				 ptp_read(PTP_CA7_SMSTATE0));
			ptp_crit("PTP_CA7_SMSTATE1 (0x%x) = 0x%x\n", PTP_CA7_SMSTATE1,
				 ptp_read(PTP_CA7_SMSTATE1));
			ptp_crit("====================================================\n");

			/* disable PTP */
			ptp_write(PTP_CA7_PTPEN, 0x0);

			/* Clear PTP interrupt PTPINTSTS */
			ptp_write(PTP_CA7_PTPINTSTS, 0x00ffffff);

			/* restore default DVFS table (PMIC) */
			mt_cpufreq_return_default_DVS_by_ptpod(0);

			PTP_CA7_INIT_FLAG = 0;
		} else { /* PTP Monitor mode error handler ====================== */
			ptp_crit("====================================================\n");
			ptp_crit("PTP CA7 error_m (0x%x) : CA7_PTPINTSTS = 0x%x\n", temp_ptpen,
				 PTPINTSTS);
			ptp_crit("PTP_CA7_SMSTATE0 (0x%x) = 0x%x\n", PTP_CA7_SMSTATE0,
				 ptp_read(PTP_CA7_SMSTATE0));
			ptp_crit("PTP_CA7_SMSTATE1 (0x%x) = 0x%x\n", PTP_CA7_SMSTATE1,
				 ptp_read(PTP_CA7_SMSTATE1));
			ptp_crit("PTP_CA7_TEMP (0x%x) = 0x%x\n", PTP_CA7_TEMP,
				 ptp_read(PTP_CA7_TEMP));

			ptp_crit("PTP_CA7_TEMPMSR0 (0x%x) = 0x%x\n", PTP_CA7_TEMPMSR0,
				 ptp_read(PTP_CA7_TEMPMSR0));
			ptp_crit("PTP_CA7_TEMPMSR1 (0x%x) = 0x%x\n", PTP_CA7_TEMPMSR1,
				 ptp_read(PTP_CA7_TEMPMSR1));
			ptp_crit("PTP_CA7_TEMPMSR2 (0x%x) = 0x%x\n", PTP_CA7_TEMPMSR2,
				 ptp_read(PTP_CA7_TEMPMSR2));
			ptp_crit("PTP_CA7_TEMPMONCTL0 (0x%x) = 0x%x\n", PTP_CA7_TEMPMONCTL0,
				 ptp_read(PTP_CA7_TEMPMONCTL0));
			ptp_crit("PTP_CA7_TEMPMSRCTL1 (0x%x) = 0x%x\n", PTP_CA7_TEMPMSRCTL1,
				 ptp_read(PTP_CA7_TEMPMSRCTL1));
			ptp_crit("====================================================\n");

			/* disable PTP */
			ptp_write(PTP_CA7_PTPEN, 0x0);

			/* Clear PTP interrupt PTPINTSTS */
			ptp_write(PTP_CA7_PTPINTSTS, 0x00ffffff);

			/* set init2 value to DVFS table (PMIC) */
			PTP_CA7_VO_0 = PTP_CA7_init2_VO_0;
			PTP_CA7_VO_1 = PTP_CA7_init2_VO_1;
			PTP_CA7_VO_2 = PTP_CA7_init2_VO_2;
			PTP_CA7_VO_3 = PTP_CA7_init2_VO_3;
			PTP_CA7_VO_4 = PTP_CA7_init2_VO_4;
			PTP_CA7_VO_5 = PTP_CA7_init2_VO_5;
			PTP_CA7_VO_6 = PTP_CA7_init2_VO_6;
			PTP_CA7_VO_7 = PTP_CA7_init2_VO_7;
			PTP_set_ptp_ca7_volt();
		}

	}

	return IRQ_HANDLED;
}

irqreturn_t MT8135_PTP_CA15_ISR(int irq, void *dev_id)
{
	u32 PTPINTSTS, temp, temp_0, temp_ptpen;

	PTPINTSTS = ptp_read(PTP_CA15_PTPINTSTS);
	temp_ptpen = ptp_read(PTP_CA15_PTPEN);
	ptp_isr_info("===============ISR ISR ISR ISR ISR=============\n");
	ptp_isr_info("CA15_PTPINTSTS = 0x%x\n", PTPINTSTS);
	ptp_isr_info("CA15_PTP_PTPEN = 0x%x\n", temp_ptpen);
	ptp_data[4] = ptp_read(0xf101a240);
	ptp_data[5] = ptp_read(0xf101a27c);
	ptp_isr_info("*(0x1101a240) = 0x%x\n", ptp_data[1]);
	ptp_isr_info("*(0x1101a27c) = 0x%x\n", ptp_data[2]);
	ptp_data[3] = 0;

#if 0
	for (temp = 0xf101a200; temp <= 0xf101a25c; temp += 4) {
		ptp_isr_info("*(0x%x) = 0x%x\n", temp, ptp_read(temp));
	}
#endif

	if (PTPINTSTS == 0x1)	{ /* PTP init1 or init2 */
		if ((temp_ptpen & 0x7) == 0x1) { /* init 1 ============================= */
#if 0
			/*
			   // read PTP_CA15_VO_0 ~ PTP_CA15_VO_3
			   temp = ptp_read( PTP_CA15_VOP30 );
			   PTP_CA15_VO_0 = temp & 0xff;
			   PTP_CA15_VO_1 = (temp>>8) & 0xff;
			   PTP_CA15_VO_2 = (temp>>16) & 0xff;
			   PTP_CA15_VO_3 = (temp>>24) & 0xff;

			   // read PTP_CA15_VO_3 ~ PTP_CA15_VO_7
			   temp = ptp_read( PTP_CA15_VOP74 );
			   PTP_CA15_VO_4 = temp & 0xff;
			   PTP_CA15_VO_5 = (temp>>8) & 0xff;
			   PTP_CA15_VO_6 = (temp>>16) & 0xff;
			   PTP_CA15_VO_7 = (temp>>24) & 0xff;

			   // show PTP_CA15_VO_0 ~ PTP_CA15_VO_7 to PMIC
			   ptp_isr_info("PTP_CA15_VO_0 = 0x%x\n", PTP_CA15_VO_0);
			   ptp_isr_info("PTP_CA15_VO_1 = 0x%x\n", PTP_CA15_VO_1);
			   ptp_isr_info("PTP_CA15_VO_2 = 0x%x\n", PTP_CA15_VO_2);
			   ptp_isr_info("PTP_CA15_VO_3 = 0x%x\n", PTP_CA15_VO_3);
			   ptp_isr_info("PTP_CA15_VO_4 = 0x%x\n", PTP_CA15_VO_4);
			   ptp_isr_info("PTP_CA15_VO_5 = 0x%x\n", PTP_CA15_VO_5);
			   ptp_isr_info("PTP_CA15_VO_6 = 0x%x\n", PTP_CA15_VO_6);
			   ptp_isr_info("PTP_CA15_VO_7 = 0x%x\n", PTP_CA15_VO_7);
			   ptp_isr_info("ptp_ca15_level = 0x%x\n", ptp_ca15_level);
			   ptp_isr_info("================================================\n");

			   PTP_set_ptp_ca15_volt();
			 */
#endif

			/* Read & store 16 bit values DCVALUES.DCVOFFSET and AGEVALUES.AGEVOFFSET for later use in INIT2 procedure */
			PTP_CA15_DCVOFFSET = ~(ptp_read(PTP_CA15_DCVALUES) & 0xffff) + 1;	/* hw bug, workaround */
			PTP_CA15_AGEVOFFSET = ptp_read(PTP_CA15_AGEVALUES) & 0xffff;

			PTP_CA15_INIT_FLAG = 1;

			/* Set PTPEN.PTPINITEN/PTPEN.PTPINIT2EN = 0x0 & Clear PTP INIT interrupt PTPINTSTS = 0x00000001 */
			ptp_write(PTP_CA15_PTPEN, 0x0);
			ptp_write(PTP_CA15_PTPINTSTS, 0x1);
			/* tasklet_schedule(&ptp_do_init2_tasklet); */
			if ((ptp_data[0] == 0) && (ptp_data[3] == 0))	{ /* check both CA15/CA15 PTP INIT I finished */
				mt_cpufreq_enable_by_ptpod();	/* enable DVFS */
				mt_fh_popod_restore();	/* enable frequency hopping (main PLL) */
			}
			PTP_CA15_INIT_02();
		} else if ((temp_ptpen & 0x7) == 0x5)	{ /* init 2 ============================= */
			/* read PTP_CA15_VO_0 ~ PTP_CA15_VO_3 */
			temp = ptp_read(PTP_CA15_VOP30);
			PTP_CA15_VO_0 = temp & 0xff;
			PTP_CA15_VO_1 = (temp >> 8) & 0xff;
			PTP_CA15_VO_2 = (temp >> 16) & 0xff;
			PTP_CA15_VO_3 = (temp >> 24) & 0xff;

			/* read PTP_CA15_VO_3 ~ PTP_CA15_VO_7 */
			temp = ptp_read(PTP_CA15_VOP74);
			PTP_CA15_VO_4 = temp & 0xff;
			PTP_CA15_VO_5 = (temp >> 8) & 0xff;
			PTP_CA15_VO_6 = (temp >> 16) & 0xff;
			PTP_CA15_VO_7 = (temp >> 24) & 0xff;

			/* save init2 PTP_init2_VO_0 ~ PTP_VO_7 */
			PTP_CA15_init2_VO_0 = PTP_CA15_VO_0;
			PTP_CA15_init2_VO_1 = PTP_CA15_VO_1;
			PTP_CA15_init2_VO_2 = PTP_CA15_VO_2;
			PTP_CA15_init2_VO_3 = PTP_CA15_VO_3;
			PTP_CA15_init2_VO_4 = PTP_CA15_VO_4;
			PTP_CA15_init2_VO_5 = PTP_CA15_VO_5;
			PTP_CA15_init2_VO_6 = PTP_CA15_VO_6;
			PTP_CA15_init2_VO_7 = PTP_CA15_VO_7;

			/* show PTP_VO_0 ~ PTP_VO_7 to PMIC */
			ptp_isr_info("PTP_CA15_VO_0 = 0x%x\n", PTP_CA15_VO_0);
			ptp_isr_info("PTP_CA15_VO_1 = 0x%x\n", PTP_CA15_VO_1);
			ptp_isr_info("PTP_CA15_VO_2 = 0x%x\n", PTP_CA15_VO_2);
			ptp_isr_info("PTP_CA15_VO_3 = 0x%x\n", PTP_CA15_VO_3);
			ptp_isr_info("PTP_CA15_VO_4 = 0x%x\n", PTP_CA15_VO_4);
			ptp_isr_info("PTP_CA15_VO_5 = 0x%x\n", PTP_CA15_VO_5);
			ptp_isr_info("PTP_CA15_VO_6 = 0x%x\n", PTP_CA15_VO_6);
			ptp_isr_info("PTP_CA15_VO_7 = 0x%x\n", PTP_CA15_VO_7);
			ptp_isr_info("ptp_ca15_level = 0x%x\n", ptp_ca15_level);
			ptp_isr_info("================================================\n");

			PTP_set_ptp_ca15_volt();

			if (PTP_output_voltage_failed == 1) {
				ptp_write(PTP_CA15_PTPEN, 0x0);	/* disable PTP */
				ptp_write(PTP_CA15_PTPINTSTS, 0x00ffffff);	/* Clear PTP interrupt PTPINTSTS */
				PTP_CA15_INIT_FLAG = 0;
				ptp_crit("disable CA15 PTP : PTP_output_voltage_failed(init_2).\n");
				return IRQ_HANDLED;
			}

			PTP_CA15_INIT_FLAG = 1;

			/* Set PTPEN.PTPINITEN/PTPEN.PTPINIT2EN = 0x0 & Clear PTP INIT interrupt PTPINTSTS = 0x00000001 */
			ptp_write(PTP_CA15_PTPEN, 0x0);
			ptp_write(PTP_CA15_PTPINTSTS, 0x1);
			/* tasklet_schedule(&ptp_do_mon_tasklet); */
			PTP_CA15_MON_MODE();
		} else { /* error : init1 or init2 , but enable setting is wrong. */
			ptp_crit("====================================================\n");
			ptp_crit("PTP CA15 error_0 (0x%x) : CA15_PTPINTSTS = 0x%x\n", temp_ptpen,
				 PTPINTSTS);
			ptp_crit("PTP_CA15_SMSTATE0 (0x%x) = 0x%x\n", PTP_CA15_SMSTATE0,
				 ptp_read(PTP_CA15_SMSTATE0));
			ptp_crit("PTP_CA15_SMSTATE1 (0x%x) = 0x%x\n", PTP_CA15_SMSTATE1,
				 ptp_read(PTP_CA15_SMSTATE1));
			ptp_crit("====================================================\n");

			/* disable PTP */
			ptp_write(PTP_CA15_PTPEN, 0x0);

			/* Clear PTP interrupt PTPINTSTS */
			ptp_write(PTP_CA15_PTPINTSTS, 0x00ffffff);

			/* restore default DVFS table (PMIC) */
			mt_cpufreq_return_default_DVS_by_ptpod(1);

			PTP_CA15_INIT_FLAG = 0;
		}
	} else if ((PTPINTSTS & 0x00ff0000) != 0x0) { /* PTP Monitor mode */
		/* check if thermal sensor init completed? */
		temp_0 = (ptp_read(PTP_CA15_TEMP) & 0xff);	/* temp_0 */

		if ((temp_0 > 0x4b) && (temp_0 < 0xd3)) {
			ptp_crit
			    ("CA15 thermal sensor init has not been completed. (temp_0 = 0x%x)\n",
			     temp_0);
		} else {
			/* read PTP_CA15_VO_0 ~ PTP_CA15_VO_3 */
			temp = ptp_read(PTP_CA15_VOP30);
			PTP_CA15_VO_0 = temp & 0xff;
			PTP_CA15_VO_1 = (temp >> 8) & 0xff;
			PTP_CA15_VO_2 = (temp >> 16) & 0xff;
			PTP_CA15_VO_3 = (temp >> 24) & 0xff;

			/* read PTP_CA15_VO_3 ~ PTP_CA15_VO_7 */
			temp = ptp_read(PTP_CA15_VOP74);
			PTP_CA15_VO_4 = temp & 0xff;
			PTP_CA15_VO_5 = (temp >> 8) & 0xff;
			PTP_CA15_VO_6 = (temp >> 16) & 0xff;
			PTP_CA15_VO_7 = (temp >> 24) & 0xff;

			/* show PTP_CA15_VO_0 ~ PTP_CA15_VO_7 to PMIC */
			ptp_isr_info("===============ISR ISR ISR ISR Monitor mode=============\n");
			ptp_isr_info("PTP_CA15_VO_0 = 0x%x\n", PTP_CA15_VO_0);
			ptp_isr_info("PTP_CA15_VO_1 = 0x%x\n", PTP_CA15_VO_1);
			ptp_isr_info("PTP_CA15_VO_2 = 0x%x\n", PTP_CA15_VO_2);
			ptp_isr_info("PTP_CA15_VO_3 = 0x%x\n", PTP_CA15_VO_3);
			ptp_isr_info("PTP_CA15_VO_4 = 0x%x\n", PTP_CA15_VO_4);
			ptp_isr_info("PTP_CA15_VO_5 = 0x%x\n", PTP_CA15_VO_5);
			ptp_isr_info("PTP_CA15_VO_6 = 0x%x\n", PTP_CA15_VO_6);
			ptp_isr_info("PTP_CA15_VO_7 = 0x%x\n", PTP_CA15_VO_7);
			ptp_isr_info("ptp_ca15_level = 0x%x\n", ptp_ca15_level);
			ptp_isr_info("PTP_CA15_TEMP (0x%x) = 0x%x\n", PTP_CA15_TEMP,
				     ptp_read(PTP_CA15_TEMP));
			ptp_isr_info("ISR : CA15_TEMPSPARE1 = 0x%x\n",
				     ptp_read(PTP_CA15_TEMPSPARE1));
			ptp_isr_info("=============================================\n");

			PTP_set_ptp_ca15_volt();

			if (PTP_output_voltage_failed == 1) {
				ptp_write(PTP_CA15_PTPEN, 0x0);	/* disable PTP */
				ptp_write(PTP_CA15_PTPINTSTS, 0x00ffffff);	/* Clear PTP interrupt PTPINTSTS */
				PTP_CA15_INIT_FLAG = 0;
				ptp_crit("disable CA15 PTP : PTP_output_voltage_failed(Mon).\n");
				return IRQ_HANDLED;
			}
		}

		/* Clear PTP INIT interrupt PTPINTSTS = 0x00ff0000 */
		ptp_write(PTP_CA15_PTPINTSTS, 0x00ff0000);
	} else { /* PTP error */
		if (((temp_ptpen & 0x7) == 0x1) || ((temp_ptpen & 0x7) == 0x5)) { /* init 1  || init 2 error handler ====================== */
			ptp_crit("====================================================\n");
			ptp_crit("PTP CA15 error_1 error_2 (0x%x) : CA15_PTPINTSTS = 0x%x\n",
				 temp_ptpen, PTPINTSTS);
			ptp_crit("PTP_CA15_SMSTATE0 (0x%x) = 0x%x\n", PTP_CA15_SMSTATE0,
				 ptp_read(PTP_CA15_SMSTATE0));
			ptp_crit("PTP_CA15_SMSTATE1 (0x%x) = 0x%x\n", PTP_CA15_SMSTATE1,
				 ptp_read(PTP_CA15_SMSTATE1));
			ptp_crit("====================================================\n");

			/* disable PTP */
			ptp_write(PTP_CA15_PTPEN, 0x0);

			/* Clear PTP interrupt PTPINTSTS */
			ptp_write(PTP_CA15_PTPINTSTS, 0x00ffffff);

			/* restore default DVFS table (PMIC) */
			mt_cpufreq_return_default_DVS_by_ptpod(1);

			PTP_CA15_INIT_FLAG = 0;
		} else { /* PTP Monitor mode error handler ====================== */
			ptp_crit("====================================================\n");
			ptp_crit("PTP CA15 error_m (0x%x) : CA15_PTPINTSTS = 0x%x\n", temp_ptpen,
				 PTPINTSTS);
			ptp_crit("PTP_CA15_SMSTATE0 (0x%x) = 0x%x\n", PTP_CA15_SMSTATE0,
				 ptp_read(PTP_CA15_SMSTATE0));
			ptp_crit("PTP_CA15_SMSTATE1 (0x%x) = 0x%x\n", PTP_CA15_SMSTATE1,
				 ptp_read(PTP_CA15_SMSTATE1));
			ptp_crit("PTP_CA15_TEMP (0x%x) = 0x%x\n", PTP_CA15_TEMP,
				 ptp_read(PTP_CA15_TEMP));

			ptp_crit("PTP_CA15_TEMPMSR0 (0x%x) = 0x%x\n", PTP_CA15_TEMPMSR0,
				 ptp_read(PTP_CA15_TEMPMSR0));
			ptp_crit("PTP_CA15_TEMPMSR1 (0x%x) = 0x%x\n", PTP_CA15_TEMPMSR1,
				 ptp_read(PTP_CA15_TEMPMSR1));
			ptp_crit("PTP_CA15_TEMPMSR2 (0x%x) = 0x%x\n", PTP_CA15_TEMPMSR2,
				 ptp_read(PTP_CA15_TEMPMSR2));
			ptp_crit("PTP_CA15_TEMPMONCTL0 (0x%x) = 0x%x\n", PTP_CA15_TEMPMONCTL0,
				 ptp_read(PTP_CA15_TEMPMONCTL0));
			ptp_crit("PTP_CA15_TEMPMSRCTL1 (0x%x) = 0x%x\n", PTP_CA15_TEMPMSRCTL1,
				 ptp_read(PTP_CA15_TEMPMSRCTL1));
			ptp_crit("====================================================\n");

			/* disable PTP */
			ptp_write(PTP_CA15_PTPEN, 0x0);

			/* Clear PTP interrupt PTPINTSTS */
			ptp_write(PTP_CA15_PTPINTSTS, 0x00ffffff);

			/* set init2 value to DVFS table (PMIC) */
			PTP_CA15_VO_0 = PTP_CA15_init2_VO_0;
			PTP_CA15_VO_1 = PTP_CA15_init2_VO_1;
			PTP_CA15_VO_2 = PTP_CA15_init2_VO_2;
			PTP_CA15_VO_3 = PTP_CA15_init2_VO_3;
			PTP_CA15_VO_4 = PTP_CA15_init2_VO_4;
			PTP_CA15_VO_5 = PTP_CA15_init2_VO_5;
			PTP_CA15_VO_6 = PTP_CA15_init2_VO_6;
			PTP_CA15_VO_7 = PTP_CA15_init2_VO_7;
			PTP_set_ptp_ca15_volt();
		}

	}

	return IRQ_HANDLED;
}


static void PTP_CA7_Initialization_01(PTP_Init_T *PTP_Init_val)
{

	/* config PTP register ================================================================= */
	ptp_write(PTP_CA7_DESCHAR,
		  ((((PTP_Init_val->BDES) << 8) & 0xff00) | ((PTP_Init_val->MDES) & 0xff)));
	ptp_write(PTP_CA7_TEMPCHAR,
		  ((((PTP_Init_val->VCO) << 16) & 0xff0000) | (((PTP_Init_val->
								 MTDES) << 8) & 0xff00) |
		   ((PTP_Init_val->DVTFIXED) & 0xff)));
	ptp_write(PTP_CA7_DETCHAR,
		  ((((PTP_Init_val->DCBDET) << 8) & 0xff00) | ((PTP_Init_val->DCMDET) & 0xff)));
	ptp_write(PTP_CA7_AGECHAR,
		  ((((PTP_Init_val->AGEDELTA) << 8) & 0xff00) | ((PTP_Init_val->AGEM) & 0xff)));
	ptp_write(PTP_CA7_DCCONFIG, ((PTP_Init_val->DCCONFIG)));
	ptp_write(PTP_CA7_AGECONFIG, ((PTP_Init_val->AGECONFIG)));

	if (PTP_Init_val->AGEM == 0x0) {
		ptp_write(PTP_CA7_RUNCONFIG, 0x80000000);
	} else {
		u32 temp_i, temp_filter, temp_value;

		temp_value = 0x0;

		for (temp_i = 0; temp_i < 24; temp_i += 2) {
			temp_filter = 0x3 << temp_i;

			if (((PTP_Init_val->AGECONFIG) & temp_filter) == 0x0) {
				temp_value |= (0x1 << temp_i);
			} else {
				temp_value |= ((PTP_Init_val->AGECONFIG) & temp_filter);
			}
		}
		ptp_write(PTP_CA7_RUNCONFIG, temp_value);
	}

	ptp_write(PTP_CA7_FREQPCT30,
		  ((((PTP_Init_val->FREQPCT3) << 24) & 0xff000000) | (((PTP_Init_val->
									FREQPCT2) << 16) & 0xff0000)
		   | (((PTP_Init_val->FREQPCT1) << 8) & 0xff00) | ((PTP_Init_val->FREQPCT0) &
								   0xff)));
	ptp_write(PTP_CA7_FREQPCT74,
		  ((((PTP_Init_val->FREQPCT7) << 24) & 0xff000000) | (((PTP_Init_val->
									FREQPCT6) << 16) & 0xff0000)
		   | (((PTP_Init_val->FREQPCT5) << 8) & 0xff00) | ((PTP_Init_val->FREQPCT4) &
								   0xff)));

	ptp_write(PTP_CA7_LIMITVALS,
		  ((((PTP_Init_val->VMAX) << 24) & 0xff000000) | (((PTP_Init_val->
								    VMIN) << 16) & 0xff0000) |
		   (((PTP_Init_val->DTHI) << 8) & 0xff00) | ((PTP_Init_val->DTLO) & 0xff)));
	ptp_write(PTP_CA7_VBOOT, (((PTP_Init_val->VBOOT) & 0xff)));
	ptp_write(PTP_CA7_DETWINDOW, (((PTP_Init_val->DETWINDOW) & 0xffff)));
	ptp_write(PTP_CA7_PTPCONFIG, (((PTP_Init_val->DETMAX) & 0xffff)));

	/* clear all pending PTP interrupt & config PTPINTEN ================================================================= */
	ptp_write(PTP_CA7_PTPINTSTS, 0xffffffff);
	ptp_write(PTP_CA7_PTPINTEN, 0x00005f01);

	/* enable PTP INIT measurement ================================================================= */
	ptp_write(PTP_CA7_PTPEN, 0x00000001);

}


static void PTP_CA15_Initialization_01(PTP_Init_T *PTP_Init_val)
{

	/* config PTP register ================================================================= */
	ptp_write(PTP_CA15_DESCHAR,
		  ((((PTP_Init_val->BDES) << 8) & 0xff00) | ((PTP_Init_val->MDES) & 0xff)));
	ptp_write(PTP_CA15_TEMPCHAR,
		  ((((PTP_Init_val->VCO) << 16) & 0xff0000) | (((PTP_Init_val->
								 MTDES) << 8) & 0xff00) |
		   ((PTP_Init_val->DVTFIXED) & 0xff)));
	ptp_write(PTP_CA15_DETCHAR,
		  ((((PTP_Init_val->DCBDET) << 8) & 0xff00) | ((PTP_Init_val->DCMDET) & 0xff)));
	ptp_write(PTP_CA15_AGECHAR,
		  ((((PTP_Init_val->AGEDELTA) << 8) & 0xff00) | ((PTP_Init_val->AGEM) & 0xff)));
	ptp_write(PTP_CA15_DCCONFIG, ((PTP_Init_val->DCCONFIG)));
	ptp_write(PTP_CA15_AGECONFIG, ((PTP_Init_val->AGECONFIG)));

	if (PTP_Init_val->AGEM == 0x0) {
		ptp_write(PTP_CA15_RUNCONFIG, 0x80000000);
	} else {
		u32 temp_i, temp_filter, temp_value;

		temp_value = 0x0;

		for (temp_i = 0; temp_i < 24; temp_i += 2) {
			temp_filter = 0x3 << temp_i;

			if (((PTP_Init_val->AGECONFIG) & temp_filter) == 0x0) {
				temp_value |= (0x1 << temp_i);
			} else {
				temp_value |= ((PTP_Init_val->AGECONFIG) & temp_filter);
			}
		}
		ptp_write(PTP_CA15_RUNCONFIG, temp_value);
	}

	ptp_write(PTP_CA15_FREQPCT30,
		  ((((PTP_Init_val->FREQPCT3) << 24) & 0xff000000) | (((PTP_Init_val->
									FREQPCT2) << 16) & 0xff0000)
		   | (((PTP_Init_val->FREQPCT1) << 8) & 0xff00) | ((PTP_Init_val->FREQPCT0) &
								   0xff)));
	ptp_write(PTP_CA15_FREQPCT74,
		  ((((PTP_Init_val->FREQPCT7) << 24) & 0xff000000) | (((PTP_Init_val->
									FREQPCT6) << 16) & 0xff0000)
		   | (((PTP_Init_val->FREQPCT5) << 8) & 0xff00) | ((PTP_Init_val->FREQPCT4) &
								   0xff)));

	ptp_write(PTP_CA15_LIMITVALS,
		  ((((PTP_Init_val->VMAX) << 24) & 0xff000000) | (((PTP_Init_val->
								    VMIN) << 16) & 0xff0000) |
		   (((PTP_Init_val->DTHI) << 8) & 0xff00) | ((PTP_Init_val->DTLO) & 0xff)));
	ptp_write(PTP_CA15_VBOOT, (((PTP_Init_val->VBOOT) & 0xff)));
	ptp_write(PTP_CA15_DETWINDOW, (((PTP_Init_val->DETWINDOW) & 0xffff)));
	ptp_write(PTP_CA15_PTPCONFIG, (((PTP_Init_val->DETMAX) & 0xffff)));

	/* clear all pending PTP interrupt & config PTPINTEN ================================================================= */
	ptp_write(PTP_CA15_PTPINTSTS, 0xffffffff);
	ptp_write(PTP_CA15_PTPINTEN, 0x00005f01);

	/* enable PTP INIT measurement ================================================================= */
	ptp_write(PTP_CA15_PTPEN, 0x00000001);

}


static void PTP_CA7_Initialization_02(PTP_Init_T *PTP_Init_val)
{

	/* config PTP register ================================================================= */
	ptp_write(PTP_CA7_DESCHAR,
		  ((((PTP_Init_val->BDES) << 8) & 0xff00) | ((PTP_Init_val->MDES) & 0xff)));
	ptp_write(PTP_CA7_TEMPCHAR,
		  ((((PTP_Init_val->VCO) << 16) & 0xff0000) | (((PTP_Init_val->
								 MTDES) << 8) & 0xff00) |
		   ((PTP_Init_val->DVTFIXED) & 0xff)));
	ptp_write(PTP_CA7_DETCHAR,
		  ((((PTP_Init_val->DCBDET) << 8) & 0xff00) | ((PTP_Init_val->DCMDET) & 0xff)));
	ptp_write(PTP_CA7_AGECHAR,
		  ((((PTP_Init_val->AGEDELTA) << 8) & 0xff00) | ((PTP_Init_val->AGEM) & 0xff)));
	ptp_write(PTP_CA7_DCCONFIG, ((PTP_Init_val->DCCONFIG)));
	ptp_write(PTP_CA7_AGECONFIG, ((PTP_Init_val->AGECONFIG)));

	if (PTP_Init_val->AGEM == 0x0) {
		ptp_write(PTP_CA7_RUNCONFIG, 0x80000000);
	} else {
		u32 temp_i, temp_filter, temp_value;

		temp_value = 0x0;

		for (temp_i = 0; temp_i < 24; temp_i += 2) {
			temp_filter = 0x3 << temp_i;

			if (((PTP_Init_val->AGECONFIG) & temp_filter) == 0x0) {
				temp_value |= (0x1 << temp_i);
			} else {
				temp_value |= ((PTP_Init_val->AGECONFIG) & temp_filter);
			}
		}

		ptp_write(PTP_CA7_RUNCONFIG, temp_value);
	}

	ptp_write(PTP_CA7_FREQPCT30,
		  ((((PTP_Init_val->FREQPCT3) << 24) & 0xff000000) | (((PTP_Init_val->
									FREQPCT2) << 16) & 0xff0000)
		   | (((PTP_Init_val->FREQPCT1) << 8) & 0xff00) | ((PTP_Init_val->FREQPCT0) &
								   0xff)));
	ptp_write(PTP_CA7_FREQPCT74,
		  ((((PTP_Init_val->FREQPCT7) << 24) & 0xff000000) | (((PTP_Init_val->
									FREQPCT6) << 16) & 0xff0000)
		   | (((PTP_Init_val->FREQPCT5) << 8) & 0xff00) | ((PTP_Init_val->FREQPCT4) &
								   0xff)));

	ptp_write(PTP_CA7_LIMITVALS,
		  ((((PTP_Init_val->VMAX) << 24) & 0xff000000) | (((PTP_Init_val->
								    VMIN) << 16) & 0xff0000) |
		   (((PTP_Init_val->DTHI) << 8) & 0xff00) | ((PTP_Init_val->DTLO) & 0xff)));
	ptp_write(PTP_CA7_VBOOT, (((PTP_Init_val->VBOOT) & 0xff)));
	ptp_write(PTP_CA7_DETWINDOW, (((PTP_Init_val->DETWINDOW) & 0xffff)));
	ptp_write(PTP_CA7_PTPCONFIG, (((PTP_Init_val->DETMAX) & 0xffff)));

	/* clear all pending PTP interrupt & config PTPINTEN ================================================================= */
	ptp_write(PTP_CA7_PTPINTSTS, 0xffffffff);
	ptp_write(PTP_CA7_PTPINTEN, 0x00005f01);
	ptp_write(PTP_CA7_INIT2VALS,
		  ((((PTP_Init_val->AGEVOFFSETIN) << 16) & 0xffff0000) | ((PTP_Init_val->
									   DCVOFFSETIN) & 0xffff)));

	/* enable PTP INIT measurement ================================================================= */
	ptp_write(PTP_CA7_PTPEN, 0x00000005);

}


static void PTP_CA15_Initialization_02(PTP_Init_T *PTP_Init_val)
{

	/* config PTP register ================================================================= */
	ptp_write(PTP_CA15_DESCHAR,
		  ((((PTP_Init_val->BDES) << 8) & 0xff00) | ((PTP_Init_val->MDES) & 0xff)));
	ptp_write(PTP_CA15_TEMPCHAR,
		  ((((PTP_Init_val->VCO) << 16) & 0xff0000) | (((PTP_Init_val->
								 MTDES) << 8) & 0xff00) |
		   ((PTP_Init_val->DVTFIXED) & 0xff)));
	ptp_write(PTP_CA15_DETCHAR,
		  ((((PTP_Init_val->DCBDET) << 8) & 0xff00) | ((PTP_Init_val->DCMDET) & 0xff)));
	ptp_write(PTP_CA15_AGECHAR,
		  ((((PTP_Init_val->AGEDELTA) << 8) & 0xff00) | ((PTP_Init_val->AGEM) & 0xff)));
	ptp_write(PTP_CA15_DCCONFIG, ((PTP_Init_val->DCCONFIG)));
	ptp_write(PTP_CA15_AGECONFIG, ((PTP_Init_val->AGECONFIG)));

	if (PTP_Init_val->AGEM == 0x0) {
		ptp_write(PTP_CA15_RUNCONFIG, 0x80000000);
	} else {
		u32 temp_i, temp_filter, temp_value;

		temp_value = 0x0;

		for (temp_i = 0; temp_i < 24; temp_i += 2) {
			temp_filter = 0x3 << temp_i;

			if (((PTP_Init_val->AGECONFIG) & temp_filter) == 0x0) {
				temp_value |= (0x1 << temp_i);
			} else {
				temp_value |= ((PTP_Init_val->AGECONFIG) & temp_filter);
			}
		}

		ptp_write(PTP_CA15_RUNCONFIG, temp_value);
	}

	ptp_write(PTP_CA15_FREQPCT30,
		  ((((PTP_Init_val->FREQPCT3) << 24) & 0xff000000) | (((PTP_Init_val->
									FREQPCT2) << 16) & 0xff0000)
		   | (((PTP_Init_val->FREQPCT1) << 8) & 0xff00) | ((PTP_Init_val->FREQPCT0) &
								   0xff)));
	ptp_write(PTP_CA15_FREQPCT74,
		  ((((PTP_Init_val->FREQPCT7) << 24) & 0xff000000) | (((PTP_Init_val->
									FREQPCT6) << 16) & 0xff0000)
		   | (((PTP_Init_val->FREQPCT5) << 8) & 0xff00) | ((PTP_Init_val->FREQPCT4) &
								   0xff)));

	ptp_write(PTP_CA15_LIMITVALS,
		  ((((PTP_Init_val->VMAX) << 24) & 0xff000000) | (((PTP_Init_val->
								    VMIN) << 16) & 0xff0000) |
		   (((PTP_Init_val->DTHI) << 8) & 0xff00) | ((PTP_Init_val->DTLO) & 0xff)));
	ptp_write(PTP_CA15_VBOOT, (((PTP_Init_val->VBOOT) & 0xff)));
	ptp_write(PTP_CA15_DETWINDOW, (((PTP_Init_val->DETWINDOW) & 0xffff)));
	ptp_write(PTP_CA15_PTPCONFIG, (((PTP_Init_val->DETMAX) & 0xffff)));

	/* clear all pending PTP interrupt & config PTPINTEN ================================================================= */
	ptp_write(PTP_CA15_PTPINTSTS, 0xffffffff);
	ptp_write(PTP_CA15_PTPINTEN, 0x00005f01);
	ptp_write(PTP_CA15_INIT2VALS,
		  ((((PTP_Init_val->AGEVOFFSETIN) << 16) & 0xffff0000) | ((PTP_Init_val->
									   DCVOFFSETIN) & 0xffff)));

	/* enable PTP INIT measurement ================================================================= */
	ptp_write(PTP_CA15_PTPEN, 0x00000005);

}

#if 0
static void PTP_CTP_Config_Temp_Reg(void)
{
	/* configure temperature measurement polling times */
	ptp_write(PTP_TEMPMONCTL1, 0x0);	/* set the PERIOD_UNIT in number of cycles of hclk_ck cycles */
	ptp_write(PTP_TEMPMONCTL2, 0x0);	/* set the set the interval in number of PERIOD_UNIT between each sample of a sensing point */
	ptp_write(PTP_TEMPAHBPOLL, 0x0);	/* set the number of PERIOD_UNIT between ADC polling on the AHB bus */
	ptp_write(PTP_TEMPAHBTO, 0x1998);	/* set the number of PERIOD_UNIT for AHB polling the temperature voltage from AUXADC without getting any response */

	ptp_write(PTP_TEMPMONIDET0, 0x2aa);
	ptp_write(PTP_TEMPMONIDET1, 0x2aa);
	ptp_write(PTP_TEMPMONIDET2, 0x2aa);
	ptp_write(PTP_TEMPMONIDET3, 0x2aa);

	ptp_write(PTP_TEMPH2NTHRE, 0xff);
	ptp_write(PTP_TEMPHTHRE, 0xaa);
	ptp_write(PTP_TEMPCTHRE, 0x133);
	ptp_write(PTP_TEMPOFFSETH, 0xbb);
	ptp_write(PTP_TEMPOFFSETL, 0x111);

	/* configure temperature measurement filter setting */
	ptp_write(PTP_TEMPMSRCTL0, 0x6db);

	/* configure temperature measurement addresses */
	ptp_write(PTP_TEMPADCMUXADDR, PTP_TEMPSPARE1);	/* Addr for AUXADC mux select */
	ptp_write(PTP_TEMPADCENADDR, PTP_TEMPSPARE1);	/* Addr for AUXADC enable */
	ptp_write(PTP_TEMPPNPMUXADDR, PTP_TEMPSPARE1);	/* Addr for PNP sensor mux select */
	ptp_write(PTP_TEMPADCVOLTADDR, PTP_TEMPSPARE1);	/* Addr for AUXADC voltage output */

	ptp_write(PTP_TEMPADCPNP0, 0x0);	/* MUX select value for PNP0 */
	ptp_write(PTP_TEMPADCPNP1, 0x1);	/* MUX select value for PNP1 */
	ptp_write(PTP_TEMPADCPNP2, 0x2);	/* MUX select value for PNP2 */
	ptp_write(PTP_TEMPADCPNP3, 0x3);	/* MUX select value for PNP3 */

	ptp_write(PTP_TEMPADCMUX, 0x800);	/* AHB value for AUXADC mux select */
	ptp_write(PTP_TEMPADCEXT, 0x22);
	ptp_write(PTP_TEMPADCEXT1, 0x33);
	ptp_write(PTP_TEMPADCEN, 0x800);

	ptp_write(PTP_TEMPPNPMUXADDR, PTP_TEMPSPARE0);
	ptp_write(PTP_TEMPADCMUXADDR, PTP_TEMPSPARE0);
	ptp_write(PTP_TEMPADCEXTADDR, PTP_TEMPSPARE0);
	ptp_write(PTP_TEMPADCEXT1ADDR, PTP_TEMPSPARE0);
	ptp_write(PTP_TEMPADCENADDR, PTP_TEMPSPARE0);
	ptp_write(PTP_TEMPADCVALIDADDR, PTP_TEMPSPARE0);
	ptp_write(PTP_TEMPADCVOLTADDR, PTP_TEMPSPARE0);

	ptp_write(PTP_TEMPRDCTRL, 0x0);
	ptp_write(PTP_TEMPADCVALIDMASK, 0x2c);
	ptp_write(PTP_TEMPADCVOLTAGESHIFT, 0x0);
	ptp_write(PTP_TEMPADCWRITECTRL, 0x2);

	ptp_write(PTP_TEMPMONINT, 0x1fffffff);
	ptp_write(PTP_TEMPMONCTL0, 0xf);
}
#endif

static void PTP_CA7_Monitor_Mode(PTP_Init_T *PTP_Init_val)
{

	/* config PTP register ================================================================= */
	ptp_write(PTP_CA7_DESCHAR,
		  ((((PTP_Init_val->BDES) << 8) & 0xff00) | ((PTP_Init_val->MDES) & 0xff)));
	ptp_write(PTP_CA7_TEMPCHAR,
		  ((((PTP_Init_val->VCO) << 16) & 0xff0000) | (((PTP_Init_val->
								 MTDES) << 8) & 0xff00) |
		   ((PTP_Init_val->DVTFIXED) & 0xff)));
	ptp_write(PTP_CA7_DETCHAR,
		  ((((PTP_Init_val->DCBDET) << 8) & 0xff00) | ((PTP_Init_val->DCMDET) & 0xff)));
	ptp_write(PTP_CA7_AGECHAR,
		  ((((PTP_Init_val->AGEDELTA) << 8) & 0xff00) | ((PTP_Init_val->AGEM) & 0xff)));
	ptp_write(PTP_CA7_DCCONFIG, ((PTP_Init_val->DCCONFIG)));
	ptp_write(PTP_CA7_AGECONFIG, ((PTP_Init_val->AGECONFIG)));
	ptp_write(PTP_CA7_TSCALCS,
		  ((((PTP_Init_val->BTS) << 12) & 0xfff000) | ((PTP_Init_val->MTS) & 0xfff)));

	if (PTP_Init_val->AGEM == 0x0) {
		ptp_write(PTP_CA7_RUNCONFIG, 0x80000000);
	} else {
		u32 temp_i, temp_filter, temp_value;

		temp_value = 0x0;

		for (temp_i = 0; temp_i < 24; temp_i += 2) {
			temp_filter = 0x3 << temp_i;

			if (((PTP_Init_val->AGECONFIG) & temp_filter) == 0x0) {
				temp_value |= (0x1 << temp_i);
			} else {
				temp_value |= ((PTP_Init_val->AGECONFIG) & temp_filter);
			}
		}

		ptp_write(PTP_CA7_RUNCONFIG, temp_value);
	}

	ptp_write(PTP_CA7_FREQPCT30,
		  ((((PTP_Init_val->FREQPCT3) << 24) & 0xff000000) | (((PTP_Init_val->
									FREQPCT2) << 16) & 0xff0000)
		   | (((PTP_Init_val->FREQPCT1) << 8) & 0xff00) | ((PTP_Init_val->FREQPCT0) &
								   0xff)));
	ptp_write(PTP_CA7_FREQPCT74,
		  ((((PTP_Init_val->FREQPCT7) << 24) & 0xff000000) | (((PTP_Init_val->
									FREQPCT6) << 16) & 0xff0000)
		   | (((PTP_Init_val->FREQPCT5) << 8) & 0xff00) | ((PTP_Init_val->FREQPCT4) &
								   0xff)));

	ptp_write(PTP_CA7_LIMITVALS,
		  ((((PTP_Init_val->VMAX) << 24) & 0xff000000) | (((PTP_Init_val->
								    VMIN) << 16) & 0xff0000) |
		   (((PTP_Init_val->DTHI) << 8) & 0xff00) | ((PTP_Init_val->DTLO) & 0xff)));
	ptp_write(PTP_CA7_VBOOT, (((PTP_Init_val->VBOOT) & 0xff)));
	ptp_write(PTP_CA7_DETWINDOW, (((PTP_Init_val->DETWINDOW) & 0xffff)));
	ptp_write(PTP_CA7_PTPCONFIG, (((PTP_Init_val->DETMAX) & 0xffff)));

	/* clear all pending PTP interrupt & config PTPINTEN ================================================================= */
	ptp_write(PTP_CA7_PTPINTSTS, 0xffffffff);
/* ptp_write(PTP_CA7_PTPINTEN, 0x00FF0002); */
	ptp_write(PTP_CA7_PTPINTEN, 0x00FF0000);	/* monitor mode interrupt only when state change */


	/* enable PTP monitor mode ================================================================= */
	ptp_write(PTP_CA7_PTPEN, 0x00000002);

}


static void PTP_CA15_Monitor_Mode(PTP_Init_T *PTP_Init_val)
{

	/* config PTP register ================================================================= */
	ptp_write(PTP_CA15_DESCHAR,
		  ((((PTP_Init_val->BDES) << 8) & 0xff00) | ((PTP_Init_val->MDES) & 0xff)));
	ptp_write(PTP_CA15_TEMPCHAR,
		  ((((PTP_Init_val->VCO) << 16) & 0xff0000) | (((PTP_Init_val->
								 MTDES) << 8) & 0xff00) |
		   ((PTP_Init_val->DVTFIXED) & 0xff)));
	ptp_write(PTP_CA15_DETCHAR,
		  ((((PTP_Init_val->DCBDET) << 8) & 0xff00) | ((PTP_Init_val->DCMDET) & 0xff)));
	ptp_write(PTP_CA15_AGECHAR,
		  ((((PTP_Init_val->AGEDELTA) << 8) & 0xff00) | ((PTP_Init_val->AGEM) & 0xff)));
	ptp_write(PTP_CA15_DCCONFIG, ((PTP_Init_val->DCCONFIG)));
	ptp_write(PTP_CA15_AGECONFIG, ((PTP_Init_val->AGECONFIG)));
	ptp_write(PTP_CA15_TSCALCS,
		  ((((PTP_Init_val->BTS) << 12) & 0xfff000) | ((PTP_Init_val->MTS) & 0xfff)));

	if (PTP_Init_val->AGEM == 0x0) {
		ptp_write(PTP_CA15_RUNCONFIG, 0x80000000);
	} else {
		u32 temp_i, temp_filter, temp_value;

		temp_value = 0x0;

		for (temp_i = 0; temp_i < 24; temp_i += 2) {
			temp_filter = 0x3 << temp_i;

			if (((PTP_Init_val->AGECONFIG) & temp_filter) == 0x0) {
				temp_value |= (0x1 << temp_i);
			} else {
				temp_value |= ((PTP_Init_val->AGECONFIG) & temp_filter);
			}
		}

		ptp_write(PTP_CA15_RUNCONFIG, temp_value);
	}

	ptp_write(PTP_CA15_FREQPCT30,
		  ((((PTP_Init_val->FREQPCT3) << 24) & 0xff000000) | (((PTP_Init_val->
									FREQPCT2) << 16) & 0xff0000)
		   | (((PTP_Init_val->FREQPCT1) << 8) & 0xff00) | ((PTP_Init_val->FREQPCT0) &
								   0xff)));
	ptp_write(PTP_CA15_FREQPCT74,
		  ((((PTP_Init_val->FREQPCT7) << 24) & 0xff000000) | (((PTP_Init_val->
									FREQPCT6) << 16) & 0xff0000)
		   | (((PTP_Init_val->FREQPCT5) << 8) & 0xff00) | ((PTP_Init_val->FREQPCT4) &
								   0xff)));

	ptp_write(PTP_CA15_LIMITVALS,
		  ((((PTP_Init_val->VMAX) << 24) & 0xff000000) | (((PTP_Init_val->
								    VMIN) << 16) & 0xff0000) |
		   (((PTP_Init_val->DTHI) << 8) & 0xff00) | ((PTP_Init_val->DTLO) & 0xff)));
	ptp_write(PTP_CA15_VBOOT, (((PTP_Init_val->VBOOT) & 0xff)));
	ptp_write(PTP_CA15_DETWINDOW, (((PTP_Init_val->DETWINDOW) & 0xffff)));
	ptp_write(PTP_CA15_PTPCONFIG, (((PTP_Init_val->DETMAX) & 0xffff)));

	/* clear all pending PTP interrupt & config PTPINTEN ================================================================= */
	ptp_write(PTP_CA15_PTPINTSTS, 0xffffffff);
/* ptp_write(PTP_CA15_PTPINTEN, 0x00FF0002); */
	ptp_write(PTP_CA15_PTPINTEN, 0x00FF0000);	/* monitor mode interrupt only when state change */


	/* enable PTP monitor mode ================================================================= */
	ptp_write(PTP_CA15_PTPEN, 0x00000002);

}


static void init_PTP_interrupt(void)
{
	int r;

	/* Set PTP IRQ ========================================= */
	r = request_irq(MT_PTP_FSM_IRQ_ID, MT8135_PTP_CA7_ISR, IRQF_TRIGGER_LOW, "PTP_CA7", NULL);
	if (r) {
		ptp_notice("PTP CA7 IRQ register failed (%d)\n", r);
		WARN_ON(1);
	}

	r = request_irq(MT_CA15_PTP_FSM_IRQ_ID, MT8135_PTP_CA15_ISR, IRQF_TRIGGER_LOW, "PTP_CA15",
			NULL);
	if (r) {
		ptp_notice("PTP CA15 IRQ register failed (%d)\n", r);
		WARN_ON(1);
	}

	ptp_notice("Set PTP IRQ OK.\n");
}

/*
 * PTP_CA7_INIT_01: PTP_CA7_INIT_01.
 * @psInput:
 * @psOutput:
 * Return 0/1.
 */
u32 PTP_CA7_INIT_01(void)
{
	PTP_Init_T PTP_Init_value;

	ptp_data[0] = 0xffffffff;

	ptp_notice("PTP_CA7_INIT_01() start (ptp_ca7_level = 0x%x).\n", ptp_ca7_level);

#if PTP_Get_Real_Val
	val_0 = get_devinfo_with_index(16);
	val_1 = get_devinfo_with_index(17);
	val_2 = get_devinfo_with_index(18);
	val_3 = get_devinfo_with_index(19);
#endif

	/* PTP test code ================================ */
	PTP_Init_value.PTPINITEN = (val_0) & 0x1;
	PTP_Init_value.PTPMONEN = (val_0 >> 1) & 0x1;
	PTP_Init_value.MDES = (val_0 >> 8) & 0xff;
	PTP_Init_value.BDES = (val_0 >> 16) & 0xff;
	PTP_Init_value.DCMDET = (val_0 >> 24) & 0xff;

	PTP_Init_value.DCCONFIG = (val_1) & 0xffffff;
	PTP_Init_value.DCBDET = (val_1 >> 24) & 0xff;

	PTP_Init_value.AGECONFIG = (val_2) & 0xffffff;
	PTP_Init_value.AGEM = (val_2 >> 24) & 0xff;

/* PTP_Init_value.AGEDELTA = (val_3) & 0xff; */
	PTP_Init_value.AGEDELTA = 0x88;
	PTP_Init_value.DVTFIXED = (val_3 >> 8) & 0xff;
	PTP_Init_value.MTDES = (val_3 >> 16) & 0xff;
	PTP_Init_value.VCO = (val_3 >> 24) & 0xff;

	PTP_Init_value.FREQPCT0 = ca7_freq_0;
	PTP_Init_value.FREQPCT1 = ca7_freq_1;
	PTP_Init_value.FREQPCT2 = ca7_freq_2;
	PTP_Init_value.FREQPCT3 = ca7_freq_3;
	PTP_Init_value.FREQPCT4 = ca7_freq_4;
	PTP_Init_value.FREQPCT5 = ca7_freq_5;
	PTP_Init_value.FREQPCT6 = ca7_freq_6;
	PTP_Init_value.FREQPCT7 = ca7_freq_7;

	PTP_Init_value.DETWINDOW = 0xa28;	/* 100 us, This is the PTP Detector sampling time as represented in cycles of bclk_ck during INIT. 52 MHz */
	PTP_Init_value.VMAX = 0x5d;	/* 1.28125v (700mv + n * 6.25mv) */
	PTP_Init_value.VMIN = 0x28;	/* 0.95v (700mv + n * 6.25mv) */
	PTP_Init_value.DTHI = 0x01;	/* positive */
	PTP_Init_value.DTLO = 0xfe;	/* negative (2・s compliment) */
	PTP_Init_value.VBOOT = 0x48;	/* 115v  (700mv + n * 6.25mv) */
	PTP_Init_value.DETMAX = 0xffff;	/* This timeout value is in cycles of bclk_ck. */

	if (PTP_Init_value.PTPINITEN == 0x0) {
		ptp_notice("PTPINITEN = 0x%x\n", PTP_Init_value.PTPINITEN);
		return 1;
	}
	/* start PTP_CA7_INIT_1 ========================================= */
	ptp_notice("===================11111111111111===============\n");
	ptp_notice("PTPINITEN = 0x%x\n", PTP_Init_value.PTPINITEN);
	ptp_notice("PTPMONEN = 0x%x\n", PTP_Init_value.PTPMONEN);
	ptp_notice("CA7_MDES = 0x%x\n", PTP_Init_value.MDES);
	ptp_notice("CA7_BDES = 0x%x\n", PTP_Init_value.BDES);
	ptp_notice("CA7_DCMDET = 0x%x\n", PTP_Init_value.DCMDET);

	ptp_notice("CA7_DCCONFIG = 0x%x\n", PTP_Init_value.DCCONFIG);
	ptp_notice("CA7_DCBDET = 0x%x\n", PTP_Init_value.DCBDET);

	ptp_notice("CA7_AGECONFIG = 0x%x\n", PTP_Init_value.AGECONFIG);
	ptp_notice("CA7_AGEM = 0x%x\n", PTP_Init_value.AGEM);

	ptp_notice("CA7_AGEDELTA = 0x%x\n", PTP_Init_value.AGEDELTA);
	ptp_notice("CA7_DVTFIXED = 0x%x\n", PTP_Init_value.DVTFIXED);
	ptp_notice("CA7_MTDES = 0x%x\n", PTP_Init_value.MTDES);
	ptp_notice("CA7_VCO = 0x%x\n", PTP_Init_value.VCO);

	ptp_notice("CA7_FREQPCT0 = %d\n", PTP_Init_value.FREQPCT0);
	ptp_notice("CA7_FREQPCT1 = %d\n", PTP_Init_value.FREQPCT1);
	ptp_notice("CA7_FREQPCT2 = %d\n", PTP_Init_value.FREQPCT2);
	ptp_notice("CA7_FREQPCT3 = %d\n", PTP_Init_value.FREQPCT3);
	ptp_notice("CA7_FREQPCT4 = %d\n", PTP_Init_value.FREQPCT4);
	ptp_notice("CA7_FREQPCT5 = %d\n", PTP_Init_value.FREQPCT5);
	ptp_notice("CA7_FREQPCT6 = %d\n", PTP_Init_value.FREQPCT6);
	ptp_notice("CA7_FREQPCT7 = %d\n", PTP_Init_value.FREQPCT7);

	mt_fh_popod_save();	/* disable frequency hopping (main PLL) */
	mt_cpufreq_disable_by_ptpod();	/* disable DVFS and set vproc = 1.15v (1 GHz) */

	ptp_write(0xF0000104, 0);	/* disable bus dcm */
	PTP_CA7_Initialization_01(&PTP_Init_value);

	return 0;
}


/*
 * PTP_CA15_INIT_01: PTP_CA15_INIT_01.
 * @psInput:
 * @psOutput:
 * Return 0/1.
 */
u32 PTP_CA15_INIT_01(void)
{
	PTP_Init_T PTP_Init_value;

	ptp_data[3] = 0xffffffff;

	ptp_notice("PTP_CA15_INIT_01() start (ptp_ca15_level = 0x%x).\n", ptp_ca15_level);

#if PTP_Get_Real_Val
	val_0 = get_devinfo_with_index(16);
	val_2 = get_devinfo_with_index(18);
	val_3 = get_devinfo_with_index(19);
	val_4 = get_devinfo_with_index(21);
	val_5 = get_devinfo_with_index(22);
	val_6 = get_devinfo_with_index(23);
#endif

	/* PTP test code ================================ */
	PTP_Init_value.PTPINITEN = (val_0) & 0x1;
	PTP_Init_value.PTPMONEN = (val_0 >> 1) & 0x1;
	PTP_Init_value.MDES = (val_4) & 0xff;
	PTP_Init_value.BDES = (val_4 >> 8) & 0xff;
	PTP_Init_value.DCMDET = (val_4 >> 16) & 0xff;

	PTP_Init_value.DCCONFIG = (val_5) & 0xffffff;
	PTP_Init_value.DCBDET = (val_4 >> 24) & 0xff;

	PTP_Init_value.AGECONFIG = (val_6) & 0xffffff;
	PTP_Init_value.AGEM = (val_2 >> 24) & 0xff;

/* PTP_Init_value.AGEDELTA = (val_3) & 0xff; */
	PTP_Init_value.AGEDELTA = 0x88;
	PTP_Init_value.DVTFIXED = (val_3 >> 8) & 0xff;
	PTP_Init_value.MTDES = (val_3 >> 16) & 0xff;
	PTP_Init_value.VCO = (val_3 >> 24) & 0xff;

	PTP_Init_value.FREQPCT0 = ca15_freq_0;
	PTP_Init_value.FREQPCT1 = ca15_freq_1;
	PTP_Init_value.FREQPCT2 = ca15_freq_2;
	PTP_Init_value.FREQPCT3 = ca15_freq_3;
	PTP_Init_value.FREQPCT4 = ca15_freq_4;
	PTP_Init_value.FREQPCT5 = ca15_freq_5;
	PTP_Init_value.FREQPCT6 = ca15_freq_6;
	PTP_Init_value.FREQPCT7 = ca15_freq_7;

	PTP_Init_value.DETWINDOW = 0xa28;	/* 100 us, This is the PTP Detector sampling time as represented in cycles of bclk_ck during INIT. 52 MHz */
	PTP_Init_value.VMAX = 0x5d;	/* 1.28125v (700mv + n * 6.25mv) */
	PTP_Init_value.VMIN = 0x28;	/* 0.95v (700mv + n * 6.25mv) */
	PTP_Init_value.DTHI = 0x01;	/* positive */
	PTP_Init_value.DTLO = 0xfe;	/* negative (2・s compliment) */
	PTP_Init_value.VBOOT = 0x48;	/* 115v  (700mv + n * 6.25mv) */
	PTP_Init_value.DETMAX = 0xffff;	/* This timeout value is in cycles of bclk_ck. */

	if (PTP_Init_value.PTPINITEN == 0x0) {
		ptp_notice("PTPINITEN = 0x%x\n", PTP_Init_value.PTPINITEN);
		return 1;
	}
	/* start PTP_CA15_INIT_1 ========================================= */
	ptp_notice("===================11111111111111===============\n");
	ptp_notice("PTPINITEN = 0x%x\n", PTP_Init_value.PTPINITEN);
	ptp_notice("PTPMONEN = 0x%x\n", PTP_Init_value.PTPMONEN);
	ptp_notice("CA15_MDES = 0x%x\n", PTP_Init_value.MDES);
	ptp_notice("CA15_BDES = 0x%x\n", PTP_Init_value.BDES);
	ptp_notice("CA15_DCMDET = 0x%x\n", PTP_Init_value.DCMDET);

	ptp_notice("CA15_DCCONFIG = 0x%x\n", PTP_Init_value.DCCONFIG);
	ptp_notice("CA15_DCBDET = 0x%x\n", PTP_Init_value.DCBDET);

	ptp_notice("CA15_AGECONFIG = 0x%x\n", PTP_Init_value.AGECONFIG);
	ptp_notice("CA15_AGEM = 0x%x\n", PTP_Init_value.AGEM);

	ptp_notice("CA15_AGEDELTA = 0x%x\n", PTP_Init_value.AGEDELTA);
	ptp_notice("CA15_DVTFIXED = 0x%x\n", PTP_Init_value.DVTFIXED);
	ptp_notice("CA15_MTDES = 0x%x\n", PTP_Init_value.MTDES);
	ptp_notice("CA15_VCO = 0x%x\n", PTP_Init_value.VCO);

	ptp_notice("CA15_FREQPCT0 = %d\n", PTP_Init_value.FREQPCT0);
	ptp_notice("CA15_FREQPCT1 = %d\n", PTP_Init_value.FREQPCT1);
	ptp_notice("CA15_FREQPCT2 = %d\n", PTP_Init_value.FREQPCT2);
	ptp_notice("CA15_FREQPCT3 = %d\n", PTP_Init_value.FREQPCT3);
	ptp_notice("CA15_FREQPCT4 = %d\n", PTP_Init_value.FREQPCT4);
	ptp_notice("CA15_FREQPCT5 = %d\n", PTP_Init_value.FREQPCT5);
	ptp_notice("CA15_FREQPCT6 = %d\n", PTP_Init_value.FREQPCT6);
	ptp_notice("CA15_FREQPCT7 = %d\n", PTP_Init_value.FREQPCT7);

	mt_fh_popod_save();	/* disable frequency hopping (main PLL) */
	mt_cpufreq_disable_by_ptpod();	/* disable DVFS and set vproc = 1.15v (1 GHz) */

	ptp_write(0xF0000104, 0);	/* disable bus dcm */
	PTP_CA15_Initialization_01(&PTP_Init_value);

	return 0;
}



/*
 * PTP_CA7_INIT_02: PTP_CA7_INIT_02.
 * @psInput:
 * @psOutput:
 * Return 0/1.
 */
u32 PTP_CA7_INIT_02(void)
{
	PTP_Init_T PTP_Init_value;

	ptp_data[0] = 0xffffffff;

	ptp_notice("PTP_CA7_INIT_02() start (ptp_ca7_level = 0x%x).\n", ptp_ca7_level);

#if PTP_Get_Real_Val
	val_0 = get_devinfo_with_index(16);
	val_1 = get_devinfo_with_index(17);
	val_2 = get_devinfo_with_index(18);
	val_3 = get_devinfo_with_index(19);
#endif

	/* PTP test code ================================ */
	PTP_Init_value.PTPINITEN = (val_0) & 0x1;
	PTP_Init_value.PTPMONEN = (val_0 >> 1) & 0x1;
	PTP_Init_value.MDES = (val_0 >> 8) & 0xff;
	PTP_Init_value.BDES = (val_0 >> 16) & 0xff;
	PTP_Init_value.DCMDET = (val_0 >> 24) & 0xff;

	PTP_Init_value.DCCONFIG = (val_1) & 0xffffff;
	PTP_Init_value.DCBDET = (val_1 >> 24) & 0xff;

	PTP_Init_value.AGECONFIG = (val_2) & 0xffffff;
	PTP_Init_value.AGEM = (val_2 >> 24) & 0xff;

/* PTP_Init_value.AGEDELTA = (val_3) & 0xff; */
	PTP_Init_value.AGEDELTA = 0x88;
	PTP_Init_value.DVTFIXED = (val_3 >> 8) & 0xff;
	PTP_Init_value.MTDES = (val_3 >> 16) & 0xff;
	PTP_Init_value.VCO = (val_3 >> 24) & 0xff;

	PTP_Init_value.FREQPCT0 = ca7_freq_0;
	PTP_Init_value.FREQPCT1 = ca7_freq_1;
	PTP_Init_value.FREQPCT2 = ca7_freq_2;
	PTP_Init_value.FREQPCT3 = ca7_freq_3;
	PTP_Init_value.FREQPCT4 = ca7_freq_4;
	PTP_Init_value.FREQPCT5 = ca7_freq_5;
	PTP_Init_value.FREQPCT6 = ca7_freq_6;
	PTP_Init_value.FREQPCT7 = ca7_freq_7;

	PTP_Init_value.DETWINDOW = 0xa28;	/* 100 us, This is the PTP Detector sampling time as represented in cycles of bclk_ck during INIT. 52 MHz */
	PTP_Init_value.VMAX = 0x5d;	/* 1.28125v (700mv + n * 6.25mv) */
	PTP_Init_value.VMIN = 0x28;	/* 0.95v (700mv + n * 6.25mv) */
	PTP_Init_value.DTHI = 0x01;	/* positive */
	PTP_Init_value.DTLO = 0xfe;	/* negative (2・s compliment) */
	PTP_Init_value.VBOOT = 0x48;	/* 115v  (700mv + n * 6.25mv) */
	PTP_Init_value.DETMAX = 0xffff;	/* This timeout value is in cycles of bclk_ck. */

	PTP_Init_value.DCVOFFSETIN = PTP_CA7_DCVOFFSET;
	PTP_Init_value.AGEVOFFSETIN = PTP_CA7_AGEVOFFSET;

	ptp_notice("CA7_DCVOFFSETIN = 0x%x\n", PTP_Init_value.DCVOFFSETIN);
	ptp_notice("CA7_AGEVOFFSETIN = 0x%x\n", PTP_Init_value.AGEVOFFSETIN);

	if (PTP_Init_value.PTPINITEN == 0x0) {
		ptp_notice("CA7_PTPINITEN = 0x%x\n", PTP_Init_value.PTPINITEN);
		return 1;
	}
	/* start PTP_INIT_2 ========================================= */
	ptp_isr_info("===================22222222222222===============\n");
	ptp_isr_info("CA7_PTPINITEN = 0x%x\n", PTP_Init_value.PTPINITEN);
	ptp_isr_info("CA7_PTPMONEN = 0x%x\n", PTP_Init_value.PTPMONEN);
	ptp_isr_info("CA7_MDES = 0x%x\n", PTP_Init_value.MDES);
	ptp_isr_info("CA7_BDES = 0x%x\n", PTP_Init_value.BDES);
	ptp_isr_info("CA7_DCMDET = 0x%x\n", PTP_Init_value.DCMDET);

	ptp_isr_info("CA7_DCCONFIG = 0x%x\n", PTP_Init_value.DCCONFIG);
	ptp_isr_info("CA7_DCBDET = 0x%x\n", PTP_Init_value.DCBDET);

	ptp_isr_info("CA7_AGECONFIG = 0x%x\n", PTP_Init_value.AGECONFIG);
	ptp_isr_info("CA7_AGEM = 0x%x\n", PTP_Init_value.AGEM);

	ptp_isr_info("CA7_AGEDELTA = 0x%x\n", PTP_Init_value.AGEDELTA);
	ptp_isr_info("CA7_DVTFIXED = 0x%x\n", PTP_Init_value.DVTFIXED);
	ptp_isr_info("CA7_MTDES = 0x%x\n", PTP_Init_value.MTDES);
	ptp_isr_info("CA7_VCO = 0x%x\n", PTP_Init_value.VCO);

	ptp_isr_info("CA7_DCVOFFSETIN = 0x%x\n", PTP_Init_value.DCVOFFSETIN);
	ptp_isr_info("CA7_AGEVOFFSETIN = 0x%x\n", PTP_Init_value.AGEVOFFSETIN);

	ptp_isr_info("CA7_FREQPCT0 = %d\n", PTP_Init_value.FREQPCT0);
	ptp_isr_info("CA7_FREQPCT1 = %d\n", PTP_Init_value.FREQPCT1);
	ptp_isr_info("CA7_FREQPCT2 = %d\n", PTP_Init_value.FREQPCT2);
	ptp_isr_info("CA7_FREQPCT3 = %d\n", PTP_Init_value.FREQPCT3);
	ptp_isr_info("CA7_FREQPCT4 = %d\n", PTP_Init_value.FREQPCT4);
	ptp_isr_info("CA7_FREQPCT5 = %d\n", PTP_Init_value.FREQPCT5);
	ptp_isr_info("CA7_FREQPCT6 = %d\n", PTP_Init_value.FREQPCT6);
	ptp_isr_info("CA7_FREQPCT7 = %d\n", PTP_Init_value.FREQPCT7);

	PTP_CA7_Initialization_02(&PTP_Init_value);

	return 0;
}


/*
 * PTP_CA15_INIT_02: PTP_CA15_INIT_02.
 * @psInput:
 * @psOutput:
 * Return 0/1.
 */
u32 PTP_CA15_INIT_02(void)
{
	PTP_Init_T PTP_Init_value;

	ptp_data[3] = 0xffffffff;

	ptp_notice("PTP_CA15_INIT_02() start (ptp_ca15_level = 0x%x).\n", ptp_ca15_level);

#if PTP_Get_Real_Val
	val_0 = get_devinfo_with_index(16);
	val_2 = get_devinfo_with_index(18);
	val_3 = get_devinfo_with_index(19);
	val_4 = get_devinfo_with_index(21);
	val_5 = get_devinfo_with_index(22);
	val_6 = get_devinfo_with_index(23);
#endif

	/* PTP test code ================================ */
	PTP_Init_value.PTPINITEN = (val_0) & 0x1;
	PTP_Init_value.PTPMONEN = (val_0 >> 1) & 0x1;
	PTP_Init_value.MDES = (val_4) & 0xff;
	PTP_Init_value.BDES = (val_4 >> 8) & 0xff;
	PTP_Init_value.DCMDET = (val_4 >> 16) & 0xff;

	PTP_Init_value.DCCONFIG = (val_5) & 0xffffff;
	PTP_Init_value.DCBDET = (val_4 >> 24) & 0xff;

	PTP_Init_value.AGECONFIG = (val_6) & 0xffffff;
	PTP_Init_value.AGEM = (val_2 >> 24) & 0xff;

/* PTP_Init_value.AGEDELTA = (val_3) & 0xff; */
	PTP_Init_value.AGEDELTA = 0x88;
	PTP_Init_value.DVTFIXED = (val_3 >> 8) & 0xff;
	PTP_Init_value.MTDES = (val_3 >> 16) & 0xff;
	PTP_Init_value.VCO = (val_3 >> 24) & 0xff;

	PTP_Init_value.FREQPCT0 = ca15_freq_0;
	PTP_Init_value.FREQPCT1 = ca15_freq_1;
	PTP_Init_value.FREQPCT2 = ca15_freq_2;
	PTP_Init_value.FREQPCT3 = ca15_freq_3;
	PTP_Init_value.FREQPCT4 = ca15_freq_4;
	PTP_Init_value.FREQPCT5 = ca15_freq_5;
	PTP_Init_value.FREQPCT6 = ca15_freq_6;
	PTP_Init_value.FREQPCT7 = ca15_freq_7;

	PTP_Init_value.DETWINDOW = 0xa28;	/* 100 us, This is the PTP Detector sampling time as represented in cycles of bclk_ck during INIT. 52 MHz */
	PTP_Init_value.VMAX = 0x5d;	/* 1.28125v (700mv + n * 6.25mv) */
	PTP_Init_value.VMIN = 0x28;	/* 0.95v (700mv + n * 6.25mv) */
	PTP_Init_value.DTHI = 0x01;	/* positive */
	PTP_Init_value.DTLO = 0xfe;	/* negative (2・s compliment) */
	PTP_Init_value.VBOOT = 0x48;	/* 115v  (700mv + n * 6.25mv) */
	PTP_Init_value.DETMAX = 0xffff;	/* This timeout value is in cycles of bclk_ck. */

	PTP_Init_value.DCVOFFSETIN = PTP_CA15_DCVOFFSET;
	PTP_Init_value.AGEVOFFSETIN = PTP_CA15_AGEVOFFSET;

	ptp_notice("CA15_DCVOFFSETIN = 0x%x\n", PTP_Init_value.DCVOFFSETIN);
	ptp_notice("CA15_AGEVOFFSETIN = 0x%x\n", PTP_Init_value.AGEVOFFSETIN);

	if (PTP_Init_value.PTPINITEN == 0x0) {
		ptp_notice("CA15_PTPINITEN = 0x%x\n", PTP_Init_value.PTPINITEN);
		return 1;
	}
	/* start PTP_INIT_2 ========================================= */
	ptp_isr_info("===================22222222222222===============\n");
	ptp_isr_info("CA15_PTPINITEN = 0x%x\n", PTP_Init_value.PTPINITEN);
	ptp_isr_info("CA15_PTPMONEN = 0x%x\n", PTP_Init_value.PTPMONEN);
	ptp_isr_info("CA15_MDES = 0x%x\n", PTP_Init_value.MDES);
	ptp_isr_info("CA15_BDES = 0x%x\n", PTP_Init_value.BDES);
	ptp_isr_info("CA15_DCMDET = 0x%x\n", PTP_Init_value.DCMDET);

	ptp_isr_info("CA15_DCCONFIG = 0x%x\n", PTP_Init_value.DCCONFIG);
	ptp_isr_info("CA15_DCBDET = 0x%x\n", PTP_Init_value.DCBDET);

	ptp_isr_info("CA15_AGECONFIG = 0x%x\n", PTP_Init_value.AGECONFIG);
	ptp_isr_info("CA15_AGEM = 0x%x\n", PTP_Init_value.AGEM);

	ptp_isr_info("CA15_AGEDELTA = 0x%x\n", PTP_Init_value.AGEDELTA);
	ptp_isr_info("CA15_DVTFIXED = 0x%x\n", PTP_Init_value.DVTFIXED);
	ptp_isr_info("CA15_MTDES = 0x%x\n", PTP_Init_value.MTDES);
	ptp_isr_info("CA15_VCO = 0x%x\n", PTP_Init_value.VCO);

	ptp_isr_info("CA15_DCVOFFSETIN = 0x%x\n", PTP_Init_value.DCVOFFSETIN);
	ptp_isr_info("CA15_AGEVOFFSETIN = 0x%x\n", PTP_Init_value.AGEVOFFSETIN);

	ptp_isr_info("CA15_FREQPCT0 = %d\n", PTP_Init_value.FREQPCT0);
	ptp_isr_info("CA15_FREQPCT1 = %d\n", PTP_Init_value.FREQPCT1);
	ptp_isr_info("CA15_FREQPCT2 = %d\n", PTP_Init_value.FREQPCT2);
	ptp_isr_info("CA15_FREQPCT3 = %d\n", PTP_Init_value.FREQPCT3);
	ptp_isr_info("CA15_FREQPCT4 = %d\n", PTP_Init_value.FREQPCT4);
	ptp_isr_info("CA15_FREQPCT5 = %d\n", PTP_Init_value.FREQPCT5);
	ptp_isr_info("CA15_FREQPCT6 = %d\n", PTP_Init_value.FREQPCT6);
	ptp_isr_info("CA15_FREQPCT7 = %d\n", PTP_Init_value.FREQPCT7);

	PTP_CA15_Initialization_02(&PTP_Init_value);

	return 0;
}


/*
 * PTP_CA7_MON_MODE: PTP_CA7_MON_MODE.
 * @psInput:
 * @psOutput:
 * Return 0/1.
 */
u32 PTP_CA7_MON_MODE(void)
{
	PTP_Init_T PTP_Init_value;
	struct TS_PTPOD ts_info;

	ptp_notice("PTP_CA7_MON_MODE() start (ptp_ca7_level = 0x%x).\n", ptp_ca7_level);

#if PTP_Get_Real_Val
	val_0 = get_devinfo_with_index(16);
	val_1 = get_devinfo_with_index(17);
	val_2 = get_devinfo_with_index(18);
	val_3 = get_devinfo_with_index(19);
#endif

	/* PTP test code ================================ */
	PTP_Init_value.PTPINITEN = (val_0) & 0x1;
	PTP_Init_value.PTPMONEN = (val_0 >> 1) & 0x1;
	PTP_Init_value.ADC_CALI_EN = (val_0 >> 2) & 0x1;
	PTP_Init_value.MDES = (val_0 >> 8) & 0xff;
	PTP_Init_value.BDES = (val_0 >> 16) & 0xff;
	PTP_Init_value.DCMDET = (val_0 >> 24) & 0xff;

	PTP_Init_value.DCCONFIG = (val_1) & 0xffffff;
	PTP_Init_value.DCBDET = (val_1 >> 24) & 0xff;

	PTP_Init_value.AGECONFIG = (val_2) & 0xffffff;
	PTP_Init_value.AGEM = (val_2 >> 24) & 0xff;

/* PTP_Init_value.AGEDELTA = (val_3) & 0xff; */
	PTP_Init_value.AGEDELTA = 0x88;
	PTP_Init_value.DVTFIXED = (val_3 >> 8) & 0xff;
	PTP_Init_value.MTDES = (val_3 >> 16) & 0xff;
	PTP_Init_value.VCO = (val_3 >> 24) & 0xff;

#if 1				/* FIXME : enable after thermal API ready */
	get_thermal_ca7_slope_intercept(&ts_info);
	PTP_Init_value.MTS = ts_info.ts_MTS;
	PTP_Init_value.BTS = ts_info.ts_BTS;
#else
	PTP_Init_value.MTS = 505;
	PTP_Init_value.BTS = 1760;
#endif
	PTP_Init_value.FREQPCT0 = ca7_freq_0;
	PTP_Init_value.FREQPCT1 = ca7_freq_1;
	PTP_Init_value.FREQPCT2 = ca7_freq_2;
	PTP_Init_value.FREQPCT3 = ca7_freq_3;
	PTP_Init_value.FREQPCT4 = ca7_freq_4;
	PTP_Init_value.FREQPCT5 = ca7_freq_5;
	PTP_Init_value.FREQPCT6 = ca7_freq_6;
	PTP_Init_value.FREQPCT7 = ca7_freq_7;

	PTP_Init_value.DETWINDOW = 0xa28;	/* 100 us, This is the PTP Detector sampling time as represented in cycles of bclk_ck during INIT. 52 MHz */
	PTP_Init_value.VMAX = 0x5d;	/* 1.28125v (700mv + n * 6.25mv) */
	PTP_Init_value.VMIN = 0x28;	/* 0.95v (700mv + n * 6.25mv) */
	PTP_Init_value.DTHI = 0x01;	/* positive */
	PTP_Init_value.DTLO = 0xfe;	/* negative (2・s compliment) */
	PTP_Init_value.VBOOT = 0x48;	/* 115v  (700mv + n * 6.25mv) */
	PTP_Init_value.DETMAX = 0xffff;	/* This timeout value is in cycles of bclk_ck. */

	if ((PTP_Init_value.PTPINITEN == 0x0)
	    || (PTP_Init_value.PTPMONEN == 0x0) /* || (PTP_Init_value.ADC_CALI_EN == 0x0) */) {
		ptp_notice("CA7_PTPINITEN = 0x%x, CA7_PTPMONEN = 0x%x\n", PTP_Init_value.PTPINITEN,
			   PTP_Init_value.PTPMONEN);
		return 1;
	}
	/* start PTP_Monitor Mode ================================== */
	ptp_isr_info("===================MMMMMMMMMMMM===============\n");
	ptp_isr_info("CA7_PTPINITEN = 0x%x\n", PTP_Init_value.PTPINITEN);
	ptp_isr_info("CA7_PTPMONEN = 0x%x\n", PTP_Init_value.PTPMONEN);
	ptp_isr_info("CA7_MDES = 0x%x\n", PTP_Init_value.MDES);
	ptp_isr_info("CA7_BDES = 0x%x\n", PTP_Init_value.BDES);
	ptp_isr_info("CA7_DCMDET = 0x%x\n", PTP_Init_value.DCMDET);

	ptp_isr_info("CA7_DCCONFIG = 0x%x\n", PTP_Init_value.DCCONFIG);
	ptp_isr_info("CA7_DCBDET = 0x%x\n", PTP_Init_value.DCBDET);

	ptp_isr_info("CA7_AGECONFIG = 0x%x\n", PTP_Init_value.AGECONFIG);
	ptp_isr_info("CA7_AGEM = 0x%x\n", PTP_Init_value.AGEM);

	ptp_isr_info("CA7_AGEDELTA = 0x%x\n", PTP_Init_value.AGEDELTA);
	ptp_isr_info("CA7_DVTFIXED = 0x%x\n", PTP_Init_value.DVTFIXED);
	ptp_isr_info("CA7_MTDES = 0x%x\n", PTP_Init_value.MTDES);
	ptp_isr_info("CA7_VCO = 0x%x\n", PTP_Init_value.VCO);
	ptp_isr_info("CA7_MTS = 0x%x\n", PTP_Init_value.MTS);
	ptp_isr_info("CA7_BTS = 0x%x\n", PTP_Init_value.BTS);

	ptp_isr_info("CA7_FREQPCT0 = %d\n", PTP_Init_value.FREQPCT0);
	ptp_isr_info("CA7_FREQPCT1 = %d\n", PTP_Init_value.FREQPCT1);
	ptp_isr_info("CA7_FREQPCT2 = %d\n", PTP_Init_value.FREQPCT2);
	ptp_isr_info("CA7_FREQPCT3 = %d\n", PTP_Init_value.FREQPCT3);
	ptp_isr_info("CA7_FREQPCT4 = %d\n", PTP_Init_value.FREQPCT4);
	ptp_isr_info("CA7_FREQPCT5 = %d\n", PTP_Init_value.FREQPCT5);
	ptp_isr_info("CA7_FREQPCT6 = %d\n", PTP_Init_value.FREQPCT6);
	ptp_isr_info("CA7_FREQPCT7 = %d\n", PTP_Init_value.FREQPCT7);

/* PTP_CTP_Config_Temp_Reg(); */
	PTP_CA7_Monitor_Mode(&PTP_Init_value);

	return 0;
}


/*
 * PTP_CA15_MON_MODE: PTP_CA15_MON_MODE.
 * @psInput:
 * @psOutput:
 * Return 0/1.
 */
u32 PTP_CA15_MON_MODE(void)
{
	PTP_Init_T PTP_Init_value;
	struct TS_PTPOD ts_info;

	ptp_notice("PTP_CA15_MON_MODE() start (ptp_ca15_level = 0x%x).\n", ptp_ca15_level);

#if PTP_Get_Real_Val
	val_0 = get_devinfo_with_index(16);
	val_2 = get_devinfo_with_index(18);
	val_3 = get_devinfo_with_index(19);
	val_4 = get_devinfo_with_index(21);
	val_5 = get_devinfo_with_index(22);
	val_6 = get_devinfo_with_index(23);
#endif

	/* PTP test code ================================ */
	PTP_Init_value.PTPINITEN = (val_0) & 0x1;
	PTP_Init_value.PTPMONEN = (val_0 >> 1) & 0x1;
	PTP_Init_value.ADC_CALI_EN = (val_0 >> 2) & 0x1;
	PTP_Init_value.MDES = (val_4) & 0xff;
	PTP_Init_value.BDES = (val_4 >> 8) & 0xff;
	PTP_Init_value.DCMDET = (val_4 >> 16) & 0xff;

	PTP_Init_value.DCCONFIG = (val_5) & 0xffffff;
	PTP_Init_value.DCBDET = (val_4 >> 24) & 0xff;

	PTP_Init_value.AGECONFIG = (val_6) & 0xffffff;
	PTP_Init_value.AGEM = (val_2 >> 24) & 0xff;

/* PTP_Init_value.AGEDELTA = (val_3) & 0xff; */
	PTP_Init_value.AGEDELTA = 0x88;
	PTP_Init_value.DVTFIXED = (val_3 >> 8) & 0xff;
	PTP_Init_value.MTDES = (val_3 >> 16) & 0xff;
	PTP_Init_value.VCO = (val_3 >> 24) & 0xff;

#if 1				/* FIXME : enable after thermal API ready */
	get_thermal_ca15_slope_intercept(&ts_info);
	PTP_Init_value.MTS = ts_info.ts_MTS;
	PTP_Init_value.BTS = ts_info.ts_BTS;
#else
	PTP_Init_value.MTS = 505;
	PTP_Init_value.BTS = 1760;
#endif

	PTP_Init_value.FREQPCT0 = ca15_freq_0;
	PTP_Init_value.FREQPCT1 = ca15_freq_1;
	PTP_Init_value.FREQPCT2 = ca15_freq_2;
	PTP_Init_value.FREQPCT3 = ca15_freq_3;
	PTP_Init_value.FREQPCT4 = ca15_freq_4;
	PTP_Init_value.FREQPCT5 = ca15_freq_5;
	PTP_Init_value.FREQPCT6 = ca15_freq_6;
	PTP_Init_value.FREQPCT7 = ca15_freq_7;

	PTP_Init_value.DETWINDOW = 0xa28;	/* 100 us, This is the PTP Detector sampling time as represented in cycles of bclk_ck during INIT. 52 MHz */
	PTP_Init_value.VMAX = 0x5d;	/* 1.28125v (700mv + n * 6.25mv) */
	PTP_Init_value.VMIN = 0x28;	/* 0.95v (700mv + n * 6.25mv) */
	PTP_Init_value.DTHI = 0x01;	/* positive */
	PTP_Init_value.DTLO = 0xfe;	/* negative (2・s compliment) */
	PTP_Init_value.VBOOT = 0x48;	/* 115v  (700mv + n * 6.25mv) */
	PTP_Init_value.DETMAX = 0xffff;	/* This timeout value is in cycles of bclk_ck. */

	if ((PTP_Init_value.PTPINITEN == 0x0)
	    || (PTP_Init_value.PTPMONEN == 0x0) /* || (PTP_Init_value.ADC_CALI_EN == 0x0) */) {
		ptp_notice("CA15_PTPINITEN = 0x%x, CA15_PTPMONEN = 0x%x\n",
			   PTP_Init_value.PTPINITEN, PTP_Init_value.PTPMONEN);
		return 1;
	}
	/* start PTP_Monitor Mode ================================== */
	ptp_isr_info("===================MMMMMMMMMMMM===============\n");
	ptp_isr_info("CA15_PTPINITEN = 0x%x\n", PTP_Init_value.PTPINITEN);
	ptp_isr_info("CA15_PTPMONEN = 0x%x\n", PTP_Init_value.PTPMONEN);
	ptp_isr_info("CA15_MDES = 0x%x\n", PTP_Init_value.MDES);
	ptp_isr_info("CA15_BDES = 0x%x\n", PTP_Init_value.BDES);
	ptp_isr_info("CA15_DCMDET = 0x%x\n", PTP_Init_value.DCMDET);

	ptp_isr_info("CA15_DCCONFIG = 0x%x\n", PTP_Init_value.DCCONFIG);
	ptp_isr_info("CA15_DCBDET = 0x%x\n", PTP_Init_value.DCBDET);

	ptp_isr_info("CA15_AGECONFIG = 0x%x\n", PTP_Init_value.AGECONFIG);
	ptp_isr_info("CA15_AGEM = 0x%x\n", PTP_Init_value.AGEM);

	ptp_isr_info("CA15_AGEDELTA = 0x%x\n", PTP_Init_value.AGEDELTA);
	ptp_isr_info("CA15_DVTFIXED = 0x%x\n", PTP_Init_value.DVTFIXED);
	ptp_isr_info("CA15_MTDES = 0x%x\n", PTP_Init_value.MTDES);
	ptp_isr_info("CA15_VCO = 0x%x\n", PTP_Init_value.VCO);
	ptp_isr_info("CA15_MTS = 0x%x\n", PTP_Init_value.MTS);
	ptp_isr_info("CA15_BTS = 0x%x\n", PTP_Init_value.BTS);

	ptp_isr_info("CA15_FREQPCT0 = %d\n", PTP_Init_value.FREQPCT0);
	ptp_isr_info("CA15_FREQPCT1 = %d\n", PTP_Init_value.FREQPCT1);
	ptp_isr_info("CA15_FREQPCT2 = %d\n", PTP_Init_value.FREQPCT2);
	ptp_isr_info("CA15_FREQPCT3 = %d\n", PTP_Init_value.FREQPCT3);
	ptp_isr_info("CA15_FREQPCT4 = %d\n", PTP_Init_value.FREQPCT4);
	ptp_isr_info("CA15_FREQPCT5 = %d\n", PTP_Init_value.FREQPCT5);
	ptp_isr_info("CA15_FREQPCT6 = %d\n", PTP_Init_value.FREQPCT6);
	ptp_isr_info("CA15_FREQPCT7 = %d\n", PTP_Init_value.FREQPCT7);

/* PTP_CTP_Config_Temp_Reg(); */
	PTP_CA15_Monitor_Mode(&PTP_Init_value);

	return 0;
}

unsigned int PTP_get_ptp_ca7_level(void)
{
	unsigned int ptp_level_temp, m_hw_res4;

	m_hw_res4 = get_devinfo_with_index(15) & 0xFFFF;	/* M_HW_RES4 */

	ptp_level_temp = get_devinfo_with_index(5) & 0x7;	/* SPARE2[2:0] */
	ptp_notice("M_HW_RES4(0x%x), SPARE2(0x%x), ptp_ca7_level_temp(%d)\n", m_hw_res4,
		   get_devinfo_with_index(5), ptp_level_temp);

	if (m_hw_res4 == 0x1000)
		return 0;	/* 1.2G */
	else if (m_hw_res4 == 0x1001)
		return 0;	/* 1.2G */
	else if (m_hw_res4 == 0x1003)
		return 9;	/* 1.0G */
	else
		return 0;	/* 1.2G */

	if (ptp_level_temp == 0) {/* free mode */
		ptp_level_temp = (get_devinfo_with_index(25) >> 4) & 0x7;	/* M_HW2_RES4[6:4] CPU2_SPD_BOND */
		if (ptp_level_temp == 0)	/* 1.2GHz */
			return 0;
		else if (ptp_level_temp == 1)	/* 1.5GHz */
			return 3;
		else if (ptp_level_temp == 2)	/* 1.4GHz */
			return 2;
		else if (ptp_level_temp == 1)	/* 1.3GHz */
			return 1;
		else if (ptp_level_temp == 4)	/* 1.2GHz */
			return 0;
		else if (ptp_level_temp == 5)	/* 1.1GHz */
			return 8;
		else if (ptp_level_temp == 6)	/* 1.0GHz */
			return 9;
		else		/* 1.0GHz */
			return 10;
	} else if (ptp_level_temp == 1)	{ /* 1.5GHz */
		return 3;
	} else if (ptp_level_temp == 2)	{ /* 1.4GHz */
		return 2;
	} else if (ptp_level_temp == 3)	{ /* 1.3GHz */
		return 1;
	} else if (ptp_level_temp == 4)	{ /* 1.2GHz */
		return 0;
	} else if (ptp_level_temp == 5)	{ /* 1.1GHz */
		return 8;
	} else if (ptp_level_temp == 6)	{ /* 1.0GHz */
		return 9;
	} else { /* 1.0GHz */
		return 10;
	}
}

unsigned int PTP_get_ptp_ca15_level(void)
{
	unsigned int ptp_level_temp, m_hw_res4;

	m_hw_res4 = get_devinfo_with_index(15) & 0xFFFF;	/* M_HW_RES4 */

	ptp_level_temp = get_devinfo_with_index(3) & 0x7;	/* SPARE0[2:0] */
	ptp_notice("SPARE0(0x%x), ptp_ca15_level_temp(%d)\n", get_devinfo_with_index(3),
		   ptp_level_temp);

	if (m_hw_res4 == 0x1000)
		return 3;	/* 1.8G */
	else if (m_hw_res4 == 0x1001)
		return 0;	/* 1.5G */
	else if (m_hw_res4 == 0x1003)
		return 9;	/* 1.35G */
	else
		return 0;	/* 1.5G */

	if (ptp_level_temp == 0) {/* free mode */
		ptp_level_temp = get_devinfo_with_index(25) & 0x7;	/* M_HW2_RES4[2:0] CPU_SPD_BOND */
		if (ptp_level_temp == 0)	/* no binning efuse value */
			return 11;
		else if (ptp_level_temp == 1)	/* 2.0GHz (still set 1.8GHz) */
			return 3;
		else if (ptp_level_temp == 2)	/* 1.8GHz */
			return 3;
		else if (ptp_level_temp == 3)	/* 1.7GHz */
			return 2;
		else if (ptp_level_temp == 4)	/* 1.6GHz */
			return 1;
		else if (ptp_level_temp == 5)	/* 1.5GHz */
			return 0;
		else if (ptp_level_temp == 6)	/* 1.3GHz */
			return 9;
		else if (ptp_level_temp == 7)	/* 1.0GHz */
			return 10;
		else		/* 1.0GHz */
			return 10;
	} else if (ptp_level_temp == 1)	{ /* 2.0GHz */
/* return 4; */
		return 3;	/* max 1.8 */
	} else if (ptp_level_temp == 2)	{ /* 1.8GHz */
		return 3;
	} else if (ptp_level_temp == 3)	{ /* 1.7GHz */
		return 2;
	} else if (ptp_level_temp == 4)	{ /* 1.6GHz */
		return 1;
	} else if (ptp_level_temp == 5)	{ /* 1.5GHz */
		return 0;
	} else if (ptp_level_temp == 6)	{/* 1.3GHz */
		return 9;
	} else { /* 1.0GHz */
		return 10;
	}
}

#if En_PTP_OD

static int ptp_probe(struct platform_device *pdev)
{
	ptp_crit("[ptp_probe]!\n");

#if PTP_Get_Real_Val
	val_0 = get_devinfo_with_index(16);
	val_1 = get_devinfo_with_index(17);
	val_2 = get_devinfo_with_index(18);
	val_3 = get_devinfo_with_index(19);
	val_4 = get_devinfo_with_index(21);
	val_5 = get_devinfo_with_index(22);
	val_6 = get_devinfo_with_index(23);
#endif

	if ((val_0 & 0x1) == 0x0) {
		ptp_notice("PTPINITEN = 0x%x\n", (val_0 & 0x1));
		ptp_data[0] = 0;
		ptp_data[3] = 0;
		return 0;
	}
	/* Set PTP IRQ ========================================= */
	init_PTP_interrupt();

	/* Get DVFS frequency table ================================ */
	ca7_freq_0 = (u8) (mt_cpufreq_max_frequency_by_DVS(0, 0) / 11830);	/* max freq 1183 x 100% */
	ca7_freq_1 = (u8) (mt_cpufreq_max_frequency_by_DVS(1, 0) / 11830);
	ca7_freq_2 = (u8) (mt_cpufreq_max_frequency_by_DVS(2, 0) / 11830);
	ca7_freq_3 = (u8) (mt_cpufreq_max_frequency_by_DVS(3, 0) / 11830);
	ca7_freq_4 = (u8) (mt_cpufreq_max_frequency_by_DVS(4, 0) / 11830);
	ca7_freq_5 = (u8) (mt_cpufreq_max_frequency_by_DVS(5, 0) / 11830);
	ca7_freq_6 = (u8) (mt_cpufreq_max_frequency_by_DVS(6, 0) / 11830);
	ca7_freq_7 = (u8) (mt_cpufreq_max_frequency_by_DVS(7, 0) / 11830);

	ca15_freq_0 = (u8) (mt_cpufreq_max_frequency_by_DVS(0, 1) / 13910);	/* max freq 1391 x 100% */
	ca15_freq_1 = (u8) (mt_cpufreq_max_frequency_by_DVS(1, 1) / 13910);
	ca15_freq_2 = (u8) (mt_cpufreq_max_frequency_by_DVS(2, 1) / 13910);
	ca15_freq_3 = (u8) (mt_cpufreq_max_frequency_by_DVS(3, 1) / 13910);
	ca15_freq_4 = (u8) (mt_cpufreq_max_frequency_by_DVS(4, 1) / 13910);
	ca15_freq_5 = (u8) (mt_cpufreq_max_frequency_by_DVS(5, 1) / 13910);
	ca15_freq_6 = (u8) (mt_cpufreq_max_frequency_by_DVS(6, 1) / 13910);
	ca15_freq_7 = (u8) (mt_cpufreq_max_frequency_by_DVS(7, 1) / 13910);

	ptp_ca7_level = PTP_get_ptp_ca7_level();
	ptp_ca15_level = PTP_get_ptp_ca15_level();

	PTP_CA7_INIT_01();
	PTP_CA15_INIT_01();

	return 0;
}


static int ptp_resume(struct platform_device *pdev)
{
	if (PTP_Enable) {
		if ((val_0 & 0x1) == 0x0) {
			ptp_notice("PTPINITEN = 0x%x\n", (val_0 & 0x1));
			ptp_data[0] = 0;
			ptp_data[3] = 0;
			return 0;
		}
		PTP_CA7_INIT_02();
		PTP_CA15_INIT_02();
	}
	return 0;
}

static struct platform_driver mtk_ptp_driver = {
	.remove = NULL,
	.shutdown = NULL,
	.probe = ptp_probe,
	.suspend = NULL,
	.resume = ptp_resume,
	.driver = {
		   .name = "mtk-ptp",
		   },
};

void PTP_disable_ptp(void)
{
	unsigned long flags;

	/* Mask ARM i bit */
	local_irq_save(flags);

	/* disable PTP */
	ptp_write(PTP_CA7_PTPEN, 0x0);
	ptp_write(PTP_CA15_PTPEN, 0x0);

	/* Clear PTP interrupt PTPINTSTS */
	ptp_write(PTP_CA7_PTPINTSTS, 0x00ffffff);
	ptp_write(PTP_CA15_PTPINTSTS, 0x00ffffff);

	/* restore default DVFS table (PMIC) */
	mt_cpufreq_return_default_DVS_by_ptpod(0);
	mt_cpufreq_return_default_DVS_by_ptpod(1);

	PTP_Enable = 0;
	PTP_CA7_INIT_FLAG = 0;
	PTP_CA15_INIT_FLAG = 0;
	ptp_notice("Disable PTP-OD done.\n");

	/* Un-Mask ARM i bit */
	local_irq_restore(flags);
}

/***************************
* show current PTP status
****************************/
static ssize_t ptp_debug_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	char *p = buf;

	if (PTP_Enable) {
		p += sprintf(p, "PTP enabled (0x%x, 0x%x, 0x%x)\n", val_0, ptp_ca7_level,
			     ptp_ca15_level);
		p += sprintf(p, "PTP CA7(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n",
			     PTP_CA7_VO_0, PTP_CA7_VO_1, PTP_CA7_VO_2, PTP_CA7_VO_3, PTP_CA7_VO_4,
			     PTP_CA7_VO_5, PTP_CA7_VO_6, PTP_CA7_VO_7);
		p += sprintf(p, "PTP CA15(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n",
			     PTP_CA15_VO_0, PTP_CA15_VO_1, PTP_CA15_VO_2, PTP_CA15_VO_3,
			     PTP_CA15_VO_4, PTP_CA15_VO_5, PTP_CA15_VO_6, PTP_CA15_VO_7);
	} else
		p += sprintf(p, "PTP disabled (0x%x, 0x%x, 0x%x)\n", val_0, ptp_ca7_level,
			     ptp_ca15_level);

	p += sprintf(p, "efuse(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n", val_0, val_1, val_2,
		     val_3, val_4, val_5, val_6);

	len = p - buf;
	return len;
}

/************************************
* set PTP stauts by sysfs interface
*************************************/
static ssize_t ptp_debug_store(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf, size_t n)
{
	int enabled = 0;

	if (sscanf(buf, "%d", &enabled) == 1) {
		if (enabled == 0) {
			/* Disable PTP and Restore default DVFS table (PMIC) */
			PTP_disable_ptp();
		} else {
			ptp_notice("bad argument_0!! argument should be \"0\"\n");
		}
	} else {
		ptp_notice("bad argument_1!! argument should be \"0\"\n");
	}

	return n;
}

ptp_attr(ptp_debug);

struct kobject *ptp_kobj;
EXPORT_SYMBOL_GPL(ptp_kobj);

static struct attribute *g[] = {
	&ptp_debug_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int __init ptp_init(void)
{
	int ptp_err = 0;

	ptp_data[0] = 0xffffffff;
	ptp_data[3] = 0xffffffff;

	ptp_crit("[ptp_init]!\n");

	ptp_kobj = kobject_create_and_add("ptp", NULL);
	if (!ptp_kobj) {
		ptp_notice("[%s]: ptp_kobj create failed\n", __func__);
	}

	ptp_err = sysfs_create_group(ptp_kobj, &attr_group);
	if (ptp_err) {
		ptp_notice("[%s]: ptp_kobj->attr_group create failed\n", __func__);
	}

	ptp_err = platform_driver_register(&mtk_ptp_driver);

	if (ptp_err) {
		ptp_notice("PTP driver callback register failed..\n");
		return ptp_err;
	}

	return 0;
}

static void __exit ptp_exit(void)
{
	ptp_notice("Exit PTP\n");
}

#endif

u32 PTP_INIT_01_API(void)
{
	/* only for CPU stress */

	PTP_Init_T PTP_Init_value;
	u32 ptp_counter = 0;

	ptp_notice("PTP_INIT_01_API() start.\n");
	ptp_data[0] = 0xffffffff;
	ptp_data[1] = 0xffffffff;
	ptp_data[2] = 0xffffffff;
	ptp_data[3] = 0xffffffff;
	ptp_data[4] = 0xffffffff;
	ptp_data[5] = 0xffffffff;

#if PTP_Get_Real_Val
	val_0 = get_devinfo_with_index(16);
	val_1 = get_devinfo_with_index(17);
	val_2 = get_devinfo_with_index(18);
	val_3 = get_devinfo_with_index(19);
	val_4 = get_devinfo_with_index(21);
	val_5 = get_devinfo_with_index(22);
	val_6 = get_devinfo_with_index(23);
#endif

	/* disable PTP */
	ptp_write(PTP_CA7_PTPEN, 0x0);
	ptp_write(PTP_CA15_PTPEN, 0x0);

	/* Set PTP IRQ ========================================= */
	init_PTP_interrupt();

	/* CA7 PTP INIT 1 */
	PTP_Init_value.PTPINITEN = (val_0) & 0x1;
	PTP_Init_value.PTPMONEN = (val_0 >> 1) & 0x1;
	PTP_Init_value.MDES = (val_0 >> 8) & 0xff;
	PTP_Init_value.BDES = (val_0 >> 16) & 0xff;
	PTP_Init_value.DCMDET = (val_0 >> 24) & 0xff;

	PTP_Init_value.DCCONFIG = (val_1) & 0xffffff;
	PTP_Init_value.DCBDET = (val_1 >> 24) & 0xff;

	PTP_Init_value.AGECONFIG = (val_2) & 0xffffff;
	PTP_Init_value.AGEM = (val_2 >> 24) & 0xff;

/* PTP_Init_value.AGEDELTA = (val_3) & 0xff; */
	PTP_Init_value.AGEDELTA = 0x88;
	PTP_Init_value.DVTFIXED = (val_3 >> 8) & 0xff;
	PTP_Init_value.MTDES = (val_3 >> 16) & 0xff;
	PTP_Init_value.VCO = (val_3 >> 24) & 0xff;

	/* Get DVFS frequency table ================================ */
	ca7_freq_0 = (u8) (mt_cpufreq_max_frequency_by_DVS(0, 0) / 11830);	/* max freq 1183 x 100% */
	ca7_freq_1 = (u8) (mt_cpufreq_max_frequency_by_DVS(1, 0) / 11830);
	ca7_freq_2 = (u8) (mt_cpufreq_max_frequency_by_DVS(2, 0) / 11830);
	ca7_freq_3 = (u8) (mt_cpufreq_max_frequency_by_DVS(3, 0) / 11830);
	ca7_freq_4 = (u8) (mt_cpufreq_max_frequency_by_DVS(4, 0) / 11830);
	ca7_freq_5 = (u8) (mt_cpufreq_max_frequency_by_DVS(5, 0) / 11830);
	ca7_freq_6 = (u8) (mt_cpufreq_max_frequency_by_DVS(6, 0) / 11830);
	ca7_freq_7 = (u8) (mt_cpufreq_max_frequency_by_DVS(7, 0) / 11830);

	PTP_Init_value.FREQPCT0 = ca7_freq_0;
	PTP_Init_value.FREQPCT1 = ca7_freq_1;
	PTP_Init_value.FREQPCT2 = ca7_freq_2;
	PTP_Init_value.FREQPCT3 = ca7_freq_3;
	PTP_Init_value.FREQPCT4 = ca7_freq_4;
	PTP_Init_value.FREQPCT5 = ca7_freq_5;
	PTP_Init_value.FREQPCT6 = ca7_freq_6;
	PTP_Init_value.FREQPCT7 = ca7_freq_7;

	PTP_Init_value.DETWINDOW = 0xa28;	/* 100 us, This is the PTP Detector sampling time as represented in cycles of bclk_ck during INIT. 52 MHz */
	PTP_Init_value.VMAX = 0x5d;	/* 1.28125v (700mv + n * 6.25mv) */
	PTP_Init_value.VMIN = 0x28;	/* 0.95v (700mv + n * 6.25mv) */
	PTP_Init_value.DTHI = 0x01;	/* positive */
	PTP_Init_value.DTLO = 0xfe;	/* negative (2・s compliment) */
	PTP_Init_value.VBOOT = 0x48;	/* 115v  (700mv + n * 6.25mv) */
	PTP_Init_value.DETMAX = 0xffff;	/* This timeout value is in cycles of bclk_ck. */

	if (PTP_Init_value.PTPINITEN == 0x0) {
		ptp_notice("PTPINITEN = 0x%x\n", PTP_Init_value.PTPINITEN);
		return 0;
	}
	/* start PTP init_1 ============================= */
	PTP_CA7_Initialization_01(&PTP_Init_value);

	/* ========================================= */

	while (1) {
		ptp_counter++;

		if (ptp_counter >= 0xffffff) {
			ptp_notice("ptp_counter = 0x%x\n", ptp_counter);
			return 0;
		}

		if (ptp_data[0] == 0) {
			break;
		}
	}

	/* CA15 PTP INIT 1 */
	PTP_Init_value.PTPINITEN = (val_0) & 0x1;
	PTP_Init_value.PTPMONEN = (val_0 >> 1) & 0x1;
	PTP_Init_value.MDES = (val_4) & 0xff;
	PTP_Init_value.BDES = (val_4 >> 8) & 0xff;
	PTP_Init_value.DCMDET = (val_4 >> 16) & 0xff;

	PTP_Init_value.DCCONFIG = (val_5) & 0xffffff;
	PTP_Init_value.DCBDET = (val_4 >> 24) & 0xff;

	PTP_Init_value.AGECONFIG = (val_6) & 0xffffff;
	PTP_Init_value.AGEM = (val_2 >> 24) & 0xff;

/* PTP_Init_value.AGEDELTA = (val_3) & 0xff; */
	PTP_Init_value.AGEDELTA = 0x88;
	PTP_Init_value.DVTFIXED = (val_3 >> 8) & 0xff;
	PTP_Init_value.MTDES = (val_3 >> 16) & 0xff;
	PTP_Init_value.VCO = (val_3 >> 24) & 0xff;

	/* Get DVFS frequency table ================================ */
	ca15_freq_0 = (u8) (mt_cpufreq_max_frequency_by_DVS(0, 1) / 13910);	/* max freq 1391 x 100% */
	ca15_freq_1 = (u8) (mt_cpufreq_max_frequency_by_DVS(1, 1) / 13910);
	ca15_freq_2 = (u8) (mt_cpufreq_max_frequency_by_DVS(2, 1) / 13910);
	ca15_freq_3 = (u8) (mt_cpufreq_max_frequency_by_DVS(3, 1) / 13910);
	ca15_freq_4 = (u8) (mt_cpufreq_max_frequency_by_DVS(4, 1) / 13910);
	ca15_freq_5 = (u8) (mt_cpufreq_max_frequency_by_DVS(5, 1) / 13910);
	ca15_freq_6 = (u8) (mt_cpufreq_max_frequency_by_DVS(6, 1) / 13910);
	ca15_freq_7 = (u8) (mt_cpufreq_max_frequency_by_DVS(7, 1) / 13910);

	PTP_Init_value.FREQPCT0 = ca15_freq_0;
	PTP_Init_value.FREQPCT1 = ca15_freq_1;
	PTP_Init_value.FREQPCT2 = ca15_freq_2;
	PTP_Init_value.FREQPCT3 = ca15_freq_3;
	PTP_Init_value.FREQPCT4 = ca15_freq_4;
	PTP_Init_value.FREQPCT5 = ca15_freq_5;
	PTP_Init_value.FREQPCT6 = ca15_freq_6;
	PTP_Init_value.FREQPCT7 = ca15_freq_7;

	PTP_Init_value.DETWINDOW = 0xa28;	/* 100 us, This is the PTP Detector sampling time as represented in cycles of bclk_ck during INIT. 52 MHz */
	PTP_Init_value.VMAX = 0x5d;	/* 1.28125v (700mv + n * 6.25mv) */
	PTP_Init_value.VMIN = 0x28;	/* 0.95v (700mv + n * 6.25mv) */
	PTP_Init_value.DTHI = 0x01;	/* positive */
	PTP_Init_value.DTLO = 0xfe;	/* negative (2・s compliment) */
	PTP_Init_value.VBOOT = 0x48;	/* 115v  (700mv + n * 6.25mv) */
	PTP_Init_value.DETMAX = 0xffff;	/* This timeout value is in cycles of bclk_ck. */

	if (PTP_Init_value.PTPINITEN == 0x0) {
		ptp_notice("PTPINITEN = 0x%x\n", PTP_Init_value.PTPINITEN);
		return 0;
	}
	/* start PTP init_1 ============================= */
	PTP_CA15_Initialization_01(&PTP_Init_value);

	/* ========================================= */

	while (1) {
		ptp_counter++;

		if (ptp_counter >= 0xffffff) {
			ptp_notice("ptp_counter = 0x%x\n", ptp_counter);
			return 0;
		}

		if (ptp_data[3] == 0) {
			break;
		}
	}

	/* ptp_returndata : bit[31:16] CA15 ptp-od data, bit[15:0] CA7 ptp-od data */
	ptp_returndata[0] = ((ptp_data[1] >> 16) & 0xffff) | (((ptp_data[4] >> 16) & 0xffff) << 16);

	return (u32) (&ptp_returndata[0]);
}

module_param(ptp_log, int, 0644);

MODULE_DESCRIPTION("MT8135 PTP Driver v0.1");
#if En_PTP_OD
late_initcall(ptp_init);
#endif

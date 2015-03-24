#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/reboot.h>

#include <mach/mt_irq.h>
#include "mach/mtk_thermal_monitor.h"
#include <mach/system.h>
/* #include "mach/mtk_cpu_management.h" */

#include "mach/mt_typedefs.h"
#include "mach/mt_thermal.h"
#include "mach/mt_cpufreq.h"
#include "mach/mt_gpufreq.h"

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#define METRICS_STR_LEN 128
static char *mPrefix = "cpu_thermal_zone:def:monitor=1";
static char mBuf[METRICS_STR_LEN];
static ktime_t last_time;
#endif

static int g_curr_temp;
static int g_prev_temp;

static unsigned int interval = 100;	/* mili-seconds, 0 : no auto polling */
static unsigned int trip_temp[10] = { 120000, 95000, 65700, 90000, 80000, 70000, 65000, 60000, 55000, 50000 };

static unsigned int *cl_dev_state;
static unsigned int cl_dev_sysrst_state;
static struct thermal_zone_device *thz_dev;

static struct thermal_cooling_device **cl_dev;
static struct thermal_cooling_device *cl_dev_sysrst;

static int mtktscpu_debug_log;
static int mtktscpu_base_temp;
static int kernelmode;
static struct work_struct twork;
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip = 3;
int MA_len = 1;
int MA_len_temp = 0;
static int proc_write_flag;
static char *cooler_name;
static const char adaptive_cooler_name[] = "cpu_adaptive_cooler";

static char g_bind0[20] = "mtktscpu-sysrst";
static char g_bind1[20] = "mtk-cl-shutdown00";
static char g_bind2[20] = "cpu_adaptive_cooler";
static char g_bind3[20] = { 0 };
static char g_bind4[20] = { 0 };
static char g_bind5[20] = { 0 };
static char g_bind6[20] = { 0 };
static char g_bind7[20] = { 0 };
static char g_bind8[20] = { 0 };
static char g_bind9[20] = { 0 };


#define MTKTSCPU_TEMP_CRIT 120000	/* 120.000 degree Celsius */

#define mtktscpu_dprintk(fmt, args...)   \
do {                                    \
	if (mtktscpu_debug_log) {                \
		xlog_printk(ANDROID_LOG_INFO, "Power/CPU_Thermal", fmt, ##args); \
	}                                   \
} while (0)

/* extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata); */
/* extern int IMM_IsAdcInitReady(void); */

static kal_int32 temperature_to_raw_roomt(kal_uint32 ret);
/* static int last_cpu_t=0; */
int last_abb_t = 0;
/* int   last_pa_t=0; */

static kal_int32 g_adc_ge;
static kal_int32 g_adc_oe;
static kal_int32 g_o_vtsmcu1;
static kal_int32 g_o_vtsmcu2;
static kal_int32 g_o_vtsmcu3;
static kal_int32 g_o_vtsabb;
static kal_int32 g_o_vtsmcu4;
static kal_int32 g_degc_cali;
static kal_int32 g_adc_cali_en;
static kal_int32 g_o_slope;
static kal_int32 g_o_slope_sign;
static kal_int32 g_id;

static kal_int32 g_ge;
static kal_int32 g_oe;
static kal_int32 g_gain;

static kal_int32 g_x_roomt[4] = { 0, 0, 0, 0 };

static int Num_of_OPP;
static int Num_of_GPU_OPP;
/* static int curr_high=0, curr_low=0; */

#define y_curr_repeat_times 1
#define THERMAL_NAME    "mtk-thermal"
#define TC2_THERMAL_NAME    "mtk-thermal2"

static struct mt_cpu_power_info *mtk_cpu_power;
static struct mt_gpufreq_power_info *mtk_gpu_power;

#define CONFIG_SUPPORT_MET_MTKTSCPU 1
#if CONFIG_SUPPORT_MET_MTKTSCPU
/* MET */
#include <linux/export.h>
#include <linux/met_drv.h>

static char header[] =
    "met-info [000] 0.0: ms_ud_sys_header: CPU_Temp,"
    "TS1_temp,TS2_temp,TS3_temp,TS4_temp,d,d,d,d\n"
    "met-info [000] 0.0: ms_ud_sys_header: P_adaptive," "CPU_power,GPU_power,d,d\n";
static const char help[] = "  --mtktscpu                              monitor mtktscpu\n";
static int sample_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help);
}

static int sample_print_header(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, header);
}

unsigned int met_mtktscpu_dbg = 0;
static void sample_start(void)
{
	met_mtktscpu_dbg = 1;
	return;
}

static void sample_stop(void)
{
	met_mtktscpu_dbg = 0;
	return;
}

struct metdevice met_mtktscpu = {
	.name = "mtktscpu",
	.owner = THIS_MODULE,
	.type = MET_TYPE_BUS,
	.start = sample_start,
	.stop = sample_stop,
	.print_help = sample_print_help,
	.print_header = sample_print_header,
};
EXPORT_SYMBOL(met_mtktscpu);
#endif

void get_thermal_ca7_slope_intercept(struct TS_PTPOD *ts_info)
{
	unsigned int temp0, temp1, temp2;
	struct TS_PTPOD ts_ptpod;

	/* temp0 = (10000*100000/4096/g_gain)*15/18; */
	temp0 = (10000 * 100000 / g_gain) * 15 / 18;
/* pr_info("temp0=%d\n", temp0); */
	if (g_o_slope_sign == 0)
		temp1 = temp0 / (165 + g_o_slope);
	else
		temp1 = temp0 / (165 - g_o_slope);

/* pr_info("temp1=%d\n", temp1); */
	/* ts_ptpod.ts_MTS = temp1 - (2*temp1) + 2048; */
	ts_ptpod.ts_MTS = temp1;

	temp0 = (g_degc_cali * 10 / 2);
	temp1 = ((10000 * 100000 / 4096 / g_gain) * g_oe + g_x_roomt[1] * 10) * 15 / 18;
/* pr_info("temp1=%d\n", temp1); */
	if (g_o_slope_sign == 0)
		temp2 = temp1 * 10 / (165 + g_o_slope);
	else
		temp2 = temp1 * 10 / (165 - g_o_slope);

/* pr_info("temp2=%d\n", temp2); */
	ts_ptpod.ts_BTS = (temp0 + temp2 - 250) * 4 / 10;

	/* ts_info = &ts_ptpod; */
	ts_info->ts_MTS = ts_ptpod.ts_MTS;
	ts_info->ts_BTS = ts_ptpod.ts_BTS;
	mtktscpu_dprintk("ts_MTS=%d, ts_BTS=%d\n", ts_ptpod.ts_MTS, ts_ptpod.ts_BTS);

	return;
}
EXPORT_SYMBOL(get_thermal_ca7_slope_intercept);

void get_thermal_ca15_slope_intercept(struct TS_PTPOD *ts_info)
{
	unsigned int temp0, temp1, temp2;
	struct TS_PTPOD ts_ptpod;

	/* temp0 = (10000*100000/4096/g_gain)*15/18; */
	temp0 = (10000 * 100000 / g_gain) * 15 / 18;
/* pr_info("temp0=%d\n", temp0); */
	if (g_o_slope_sign == 0)
		temp1 = temp0 / (165 + g_o_slope);
	else
		temp1 = temp0 / (165 - g_o_slope);

/* pr_info("temp1=%d\n", temp1); */
	/* ts_ptpod.ts_MTS = temp1 - (2*temp1) + 2048; */
	ts_ptpod.ts_MTS = temp1;

	temp0 = (g_degc_cali * 10 / 2);
	temp1 = ((10000 * 100000 / 4096 / g_gain) * g_oe + g_x_roomt[3] * 10) * 15 / 18;
/* pr_info("temp1=%d\n", temp1); */
	if (g_o_slope_sign == 0)
		temp2 = temp1 * 10 / (165 + g_o_slope);
	else
		temp2 = temp1 * 10 / (165 - g_o_slope);

/* pr_info("temp2=%d\n", temp2); */
	ts_ptpod.ts_BTS = (temp0 + temp2 - 250) * 4 / 10;

	/* ts_info = &ts_ptpod; */
	ts_info->ts_MTS = ts_ptpod.ts_MTS;
	ts_info->ts_BTS = ts_ptpod.ts_BTS;
	mtktscpu_dprintk("ts_MTS=%d, ts_BTS=%d\n", ts_ptpod.ts_MTS, ts_ptpod.ts_BTS);

	return;
}
EXPORT_SYMBOL(get_thermal_ca15_slope_intercept);

static irqreturn_t thermal_interrupt_handler(int irq, void *dev_id)
{
	kal_uint32 ret = 0;
/* int i=0; */
	ret = DRV_Reg32(TEMPMONINTSTS);

	xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]",
		    "thermal_isr: [Interrupt trigger]: status = 0x%x\n", ret);
	if (ret & THERMAL_MON_CINTSTS0) {
		xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]",
			    "thermal_isr: thermal sensor point 0 - cold interrupt trigger\n");
		/* call thermal monitor interrupt, mode=0 */
/*		mtk_thermal_zone_bind_trigger_trip(thz_dev,  curr_low, 0);
		for(i=0; i<num_trip; i++)
		{
			if(curr_low == trip_temp[i])
				break;
		}
		if(i==0)
		{
			xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]", "thermal_isr: cold interrupt error\n");
		}
		else if(i==num_trip)
			set_high_low_threshold(trip_temp[i-1], 10000);
		else
			set_high_low_threshold(trip_temp[i], trip_temp[i+1]);
*/
	}
	if (ret & THERMAL_MON_HINTSTS0) {
		xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]",
			    "thermal_isr: thermal sensor point 0 - hot interrupt trigger\n");
		/* call thermal monitor interrupt, mode=1 */
/*		mtk_thermal_zone_bind_trigger_trip(thz_dev, curr_high, 1);
		for(i=0; i<num_trip; i++)
		{
			if(curr_high == trip_temp[i])
				break;
		}
		if(i==0)
		{
			//do nothing
		}
		else if(i==num_trip)
			xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]", "thermal_isr: hot interrupt error\n");
		else
			set_high_low_threshold(trip_temp[i-1], trip_temp[i]);
*/
	}

	if (ret & THERMAL_tri_SPM_State0)
		xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]",
			    "thermal_isr: Thermal state0 to trigger SPM state0\n");
	if (ret & THERMAL_tri_SPM_State1)
		xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]",
			    "thermal_isr: Thermal state1 to trigger SPM state1\n");
	if (ret & THERMAL_tri_SPM_State2)
		xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]",
			    "thermal_isr: Thermal state2 to trigger SPM state2\n");

	return IRQ_HANDLED;
}

static irqreturn_t TC2_thermal_interrupt_handler(int irq, void *dev_id)
{
	kal_uint32 ret = 0;
/* int i=0; */
	ret = DRV_Reg32(TC2_TEMPMONINTSTS);

	xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]",
		    "thermal_isr2: [Interrupt trigger]: status = 0x%x\n", ret);
	if (ret & THERMAL_MON_CINTSTS0) {
		xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]",
			    "thermal_isr2: thermal sensor point 0 - cold interrupt trigger\n");
		/* call thermal monitor interrupt, mode=0 */
/*		mtk_thermal_zone_bind_trigger_trip(thz_dev,  curr_low, 0);
		for(i=0; i<num_trip; i++)
		{
			if(curr_low == trip_temp[i])
				break;
		}
		if(i==0)
		{
			xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]", "thermal_isr: cold interrupt error\n");
		}
		else if(i==num_trip)
			set_high_low_threshold(trip_temp[i-1], 10000);
		else
			set_high_low_threshold(trip_temp[i], trip_temp[i+1]);
*/
	}
	if (ret & THERMAL_MON_HINTSTS0) {
		xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]",
			    "thermal_isr2: thermal sensor point 0 - hot interrupt trigger\n");
		/* call thermal monitor interrupt, mode=1 */
/*		mtk_thermal_zone_bind_trigger_trip(thz_dev, curr_high, 1);
		for(i=0; i<num_trip; i++)
		{
			if(curr_high == trip_temp[i])
				break;
		}
		if(i==0)
		{
			//do nothing
		}
		else if(i==num_trip)
			xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]", "thermal_isr: hot interrupt error\n");
		else
			set_high_low_threshold(trip_temp[i-1], trip_temp[i]);
*/
	}

	if (ret & THERMAL_tri_SPM_State0)
		xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]",
			    "thermal_isr2: Thermal state0 to trigger SPM state0\n");
	if (ret & THERMAL_tri_SPM_State1)
		xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]",
			    "thermal_isr2: Thermal state1 to trigger SPM state1\n");
	if (ret & THERMAL_tri_SPM_State2)
		xlog_printk(ANDROID_LOG_DEBUG, "[Power/CPU_Thermal]",
			    "thermal_isr2: Thermal state2 to trigger SPM state2\n");

	return IRQ_HANDLED;
}

static void thermal_reset_and_initial(void)
{
	UINT32 temp = 0;

	mtktscpu_dprintk("[Reset and init thermal controller]\n");

	/* reset thremal ctrl */
	temp = DRV_Reg32(PERI_GLOBALCON_RST0);
	temp |= 0x00010000;
	DRV_WriteReg32(PERI_GLOBALCON_RST0, temp);

	temp = DRV_Reg32(PERI_GLOBALCON_RST0);
	temp &= 0xFFFEFFFF;
	DRV_WriteReg32(PERI_GLOBALCON_RST0, temp);

	DRV_WriteReg32(TS_CON0_V, DRV_Reg32(TS_CON0_V) & 0xFFFFFF3F);   /* read tsmcu need */
	udelay(200);

	/* enable VBE_SEL */
	DRV_WriteReg32(TS_CON0_V, DRV_Reg32(TS_CON0_V) | 0x00000020);

	/* AuxADC Initialization */
	temp = DRV_Reg32(AUXADC_CON0_V);
	temp &= 0xFFFFF3FF;
	DRV_WriteReg32(AUXADC_CON0_V, temp);	/* disable auxadc channel 10,11 synchronous mode */

	DRV_WriteReg32(AUXADC_CON1_CLR_V, 0xC00);	/* disable auxadc channel 10,11 immediate mode */
	temp = DRV_Reg32(AUXADC_CON1_V);
	temp &= 0xFFFFF3FF;
	DRV_WriteReg32(AUXADC_CON1_V, temp);	/* disable auxadc channel 10, 11 immediate mode */


	DRV_WriteReg32(TEMPMONCTL1, 0x000003FF);	/* counting unit is 1024 / 66M = 15.5us */


	/* DRV_WriteReg32(TEMPMONCTL2, 0x02190219);      // sensing interval is 537 * 15.5us = 8.3235ms */
	DRV_WriteReg32(TEMPMONCTL2, 0x010C010C);	/* sensing interval is 268 * 15.5us = 4.154ms */

	/* DRV_WriteReg32(TEMPAHBPOLL, 0x00086470);      //total update time = 8.3235ms+ 8.3333ms = 16.6568ms */
	DRV_WriteReg32(TEMPAHBPOLL, 0x00043238);	/* total update time = 4.154ms+ 4.167ms = 8.321ms */
	DRV_WriteReg32(TEMPAHBTO, 0xFFFFFFFF);	/* exceed this polling time, IRQ would be inserted */

	DRV_WriteReg32(TEMPMONIDET0, 0x00000000);	/* times for interrupt occurrance */
	DRV_WriteReg32(TEMPMONIDET1, 0x00000000);	/* times for interrupt occurrance */
	DRV_WriteReg32(TEMPMONIDET2, 0x00000000);	/* times for interrupt occurrance */


/* DRV_WriteReg32(TEMPHTHRE, 0x0000008FC);     // set hot threshold */
/* DRV_WriteReg32(TEMPOFFSETH, 0x00000960);    // set high offset threshold */
/* DRV_WriteReg32(TEMPH2NTHRE, 0x00000A8C);    // set hot to normal threshold */
/* DRV_WriteReg32(TEMPOFFSETL, 0x00000C80);    // set low offset threshold */
/* DRV_WriteReg32(TEMPCTHRE, 0x00000CE4);      // set cold threshold */

	/* DRV_WriteReg32(TEMPMSRCTL0, 0x00000124);     // temperature sampling control, 10 sample */
	DRV_WriteReg32(TEMPMSRCTL0, 0x00000092);	/* temperature sampling control, 4 sample */

	/* DRV_WriteReg32(AUXADC_CON1_SET_V, 0x800);    // enable auxadc channel 11 immediate mode */

	DRV_WriteReg32(TEMPADCPNP0, 0x0);
	DRV_WriteReg32(TEMPADCPNP0, 0x1);
	DRV_WriteReg32(TEMPADCPNP0, 0x2);
	DRV_WriteReg32(TEMPPNPMUXADDR, TS_CON1_P);	/* AHB address for pnp sensor mux selection */

	DRV_WriteReg32(TEMPADCMUX, 0x800);
	DRV_WriteReg32(TEMPADCMUXADDR, AUXADC_CON1_SET_P);	/* AHB address for auxadc mux selection */

	DRV_WriteReg32(TEMPADCEN, 0x800);	/* AHB value for auxadc enable */
	DRV_WriteReg32(TEMPADCENADDR, AUXADC_CON1_CLR_P);

	DRV_WriteReg32(TEMPADCVALIDADDR, AUXADC_DAT11_P);	/* AHB address for auxadc valid bit */
	DRV_WriteReg32(TEMPADCVOLTADDR, AUXADC_DAT11_P);	/* AHB address for auxadc voltage output */
	DRV_WriteReg32(TEMPRDCTRL, 0x0);	/* read valid & voltage are at the same register */
	DRV_WriteReg32(TEMPADCVALIDMASK, 0x0000002C);	/* valid bit is (the 12th bit is valid bit and 1 is valid) */
	DRV_WriteReg32(TEMPADCVOLTAGESHIFT, 0x0);	/* do not need to shift */
	DRV_WriteReg32(TEMPADCWRITECTRL, 0x3);	/* enable auxadc mux & pnp mux write transaction */

	DRV_WriteReg32(TEMPMONCTL0, 0x00000007);	/* enable periodoc temperature sensing point 0,1,2 */

/* Thermal Controller 2 */
	DRV_WriteReg32(TC2_TEMPMONCTL1, 0x000003FF);	/* counting unit is 1024 / 66M = 15.5us */
	/* DRV_WriteReg32(TC2_TEMPMONCTL2, 0x02190219);            // sensing interval is 537 * 15.5us = 8.3235ms */
	/* total update time = 8.3235ms+ 8.3333ms = 16.6568ms */
	/* DRV_WriteReg32(TC2_TEMPAHBPOLL, 0x00086470);  */
	DRV_WriteReg32(TC2_TEMPMONCTL2, 0x010C010C);	/* sensing interval is 268 * 15.5us = 4.154ms */
	DRV_WriteReg32(TC2_TEMPAHBPOLL, 0x00043238);	/* total update time = 4.154ms+ 4.167ms = 8.321ms */
	DRV_WriteReg32(TC2_TEMPAHBTO, 0xFFFFFFFF);	/* exceed this polling time, IRQ would be inserted */
	DRV_WriteReg32(TC2_TEMPMONIDET0, 0x00000000);	/* times=1 for interrupt occurrance */
	DRV_WriteReg32(TC2_TEMPMONIDET1, 0x00000000);	/* times=1 for interrupt occurrance */
	DRV_WriteReg32(TC2_TEMPMONIDET2, 0x00000000);	/* times=1 for interrupt occurrance */
	/* DRV_WriteReg32(TC2_TEMPMSRCTL0, 0x00000124);            // (10 sampling) */
	DRV_WriteReg32(TC2_TEMPMSRCTL0, 0x00000092);	/* temperature measurement sampling control(4 sampling) */

	DRV_WriteReg32(TC2_TEMPADCPNP0, 0x3);	/* this value will be stored to TEMPPNPMUXADDR automatically by hw */
	DRV_WriteReg32(TC2_TEMPADCPNP1, 0x2);	/* this value will be stored to TEMPPNPMUXADDR automatically by hw */
	DRV_WriteReg32(TC2_TEMPADCPNP2, 0x1);	/* this value will be stored to TEMPPNPMUXADDR automatically by hw */
	DRV_WriteReg32(TC2_TEMPPNPMUXADDR, TS_CON2_P);	/* AHB address for pnp sensor mux selection */

	/* DRV_WriteReg32(AUXADC_CON1_SET, 0x400); // enable auxadc channel 10 immediate mode */

	DRV_WriteReg32(TC2_TEMPADCMUX, 0x400);	/* channel 10 */
	DRV_WriteReg32(TC2_TEMPADCMUXADDR, AUXADC_CON1_SET_P);	/* AHB address for auxadc mux selection */

	DRV_WriteReg32(TC2_TEMPADCEN, 0x400);	/* channel 10, AHB value for auxadc enable */
	DRV_WriteReg32(TC2_TEMPADCENADDR, AUXADC_CON1_CLR_P);	/* (channel 10 immediate mode selected) */

	DRV_WriteReg32(TC2_TEMPADCVALIDADDR, AUXADC_DAT10_P);	/* AHB address for auxadc valid bit */
	DRV_WriteReg32(TC2_TEMPADCVOLTADDR, AUXADC_DAT10_P);	/* AHB address for auxadc voltage output */
	DRV_WriteReg32(TC2_TEMPRDCTRL, 0x0);	/* read valid & voltage are at the same register */
	DRV_WriteReg32(TC2_TEMPADCVALIDMASK, 0x0000002C); /* the 12th bit is valid bit and 1 is valid */
	DRV_WriteReg32(TC2_TEMPADCVOLTAGESHIFT, 0x0);	/* do not need to shift */
	DRV_WriteReg32(TC2_TEMPADCWRITECTRL, 0x3);	/* enable auxadc mux & pnp write transaction */

	/* enable all interrupt, will enable in set_thermal_ctrl_trigger_SPM */
	/* DRV_WriteReg32(TC2_TEMPMONINT, 0x0007FFFF); */
	DRV_WriteReg32(TC2_TEMPMONCTL0, 0x00000007);	/* enable all sensing point (sensing point 0,1,2) */
}


static void set_thermal_ctrl_trigger_SPM(int temperature)
{
	int temp = 0;
	int raw_high, raw_middle, raw_low;

	mtktscpu_dprintk("[Set_thermal_ctrl_trigger_SPM]: temperature=%d\n", temperature);

	/* temperature to trigger SPM state2 */
	raw_high = temperature_to_raw_roomt(temperature);
	raw_middle = temperature_to_raw_roomt(40000);
	raw_low = temperature_to_raw_roomt(10000);

	temp = DRV_Reg32(TEMPMONINT);
	DRV_WriteReg32(TEMPMONINT, temp & 0x1FFFFFFF);	/* disable trigger SPM interrupt */

	DRV_WriteReg32(TEMPPROTCTL, 0x20000);
	DRV_WriteReg32(TEMPPROTTA, raw_low);
	DRV_WriteReg32(TEMPPROTTB, raw_middle);
	DRV_WriteReg32(TEMPPROTTC, raw_high);

	DRV_WriteReg32(TEMPMONINT, temp | 0x80000000);	/* only enable trigger SPM high temp interrupt */

/* Thermal Controller 2 */
	temp = DRV_Reg32(TC2_TEMPMONINT);
	DRV_WriteReg32(TC2_TEMPMONINT, temp & 0x1FFFFFFF);	/* disable trigger SPM interrupt */

	DRV_WriteReg32(TC2_TEMPPROTCTL, 0x20000);
	DRV_WriteReg32(TC2_TEMPPROTTA, raw_low);
	DRV_WriteReg32(TC2_TEMPPROTTB, raw_middle);
	DRV_WriteReg32(TC2_TEMPPROTTC, raw_high);

	DRV_WriteReg32(TC2_TEMPMONINT, temp | 0x80000000);	/* only enable trigger SPM high temp interrupt */
}


static int init_cooler(void)
{
	int i;
	int num = 20;		/* 700~2500, adaptive_cooler */

	cl_dev_state = kzalloc((num) * sizeof(unsigned int), GFP_KERNEL);
	if (cl_dev_state == NULL)
		return -ENOMEM;

	cl_dev =
	    (struct thermal_cooling_device **)kzalloc((num) *
						      sizeof(struct thermal_cooling_device *),
						      GFP_KERNEL);
	if (cl_dev == NULL)
		return -ENOMEM;

	cooler_name = kzalloc((num) * sizeof(char) * 20, GFP_KERNEL);
	if (cooler_name == NULL)
		return -ENOMEM;

	/* adaptive_cooler */
	sprintf(cooler_name, "%s", adaptive_cooler_name);

	/* 700~2500 */
	for (i = 1; i < num; i++)
		sprintf(cooler_name + (i * 20), "%d", (i * 100 + 700));

	Num_of_OPP = num;
	return 0;
}


static void thermal_cal_prepare(void)
{
	kal_uint32 temp0, temp1, temp2 = 0;

	temp0 = DRV_Reg32(0xF0009100);
	temp1 = DRV_Reg32(0xF0009104);
	temp2 = DRV_Reg32(0xF0009108);
	pr_info
	    ("[Power/CPU_Thermal] [Thermal calibration] ");
	pr_info
	    ("Reg(0xF0009100)=0x%x, Reg(0xF0009104)=0x%x, Reg(0xF0009108)=0x%x\n",
	     temp0, temp1, temp2);

	g_adc_ge = (temp1 & 0xFFC00000) >> 22;
	g_adc_oe = (temp1 & 0x003FF000) >> 12;

	g_o_vtsmcu1 = (temp0 & 0x03FE0000) >> 17;
	g_o_vtsmcu2 = (temp0 & 0x0001FF00) >> 8;
	g_o_vtsabb = (temp1 & 0x000001FF);
	g_o_vtsmcu3 = (temp2 & 0x03FE0000) >> 17;
	g_o_vtsmcu4 = (temp2 & 0x0001FF00) >> 8;

	g_degc_cali = (temp0 & 0x0000007E) >> 1;
	g_adc_cali_en = (temp0 & 0x00000001);
	g_o_slope_sign = (temp0 & 0x00000080) >> 7;
	g_o_slope = (temp0 & 0xFC000000) >> 26;

	g_id = (temp1 & 0x00000200) >> 9;
	if (g_id == 0)
		g_o_slope = 0;

	if (g_adc_cali_en == 1) {
		/* thermal_enable = true; */
	} else {
		g_adc_ge = 512;
		g_adc_oe = 512;
		g_degc_cali = 40;
		g_o_slope = 0;
		g_o_slope_sign = 0;
		g_o_vtsmcu1 = 260;
		g_o_vtsmcu2 = 260;
		g_o_vtsmcu3 = 260;
		g_o_vtsmcu4 = 260;
	}
	pr_info
	    ("[Power/CPU_Thermal] [Thermal calibration] g_adc_ge = 0x%x, g_adc_oe = 0x%x, g_degc_cali = 0x%x, ",
	     g_adc_ge, g_adc_oe, g_degc_cali);
	pr_info
	    ("g_adc_cali_en = 0x%x, g_o_slope = 0x%x, g_o_slope_sign = 0x%x, g_id = 0x%x\n",
	     g_adc_cali_en, g_o_slope, g_o_slope_sign, g_id);
	pr_info
	    ("[Power/CPU_Thermal] [Thermal calibration] g_o_vtsmcu1 = 0x%x, g_o_vtsmcu2 = 0x%x, ",
	     g_o_vtsmcu1, g_o_vtsmcu2);
	pr_info
	    ("g_o_vtsmcu3 = 0x%x, g_o_vtsabb = 0x%x, g_o_vtsmcu4 = 0x%x\n",
	     g_o_vtsmcu3, g_o_vtsabb, g_o_vtsmcu4);
}

static void thermal_cal_prepare_2(kal_uint32 ret)
{
	kal_int32 format_1, format_2, format_3, format_4 = 0;

	g_ge = ((g_adc_ge - 512) * 10000) / 4096;	/* ge * 10000 */
	g_oe = (g_adc_oe - 512);
	g_gain = (10000 + g_ge);	/* gain * 10000 */

	format_1 = (g_o_vtsmcu1 + 3350 - g_oe);
	format_2 = (g_o_vtsmcu2 + 3350 - g_oe);
	format_3 = (g_o_vtsmcu3 + 3350 - g_oe);
	format_4 = (g_o_vtsmcu4 + 3350 - g_oe);

	g_x_roomt[0] = (((format_1 * 10000) / 4096) * 10000) / g_gain;	/* x_roomt * 10000 */
	g_x_roomt[1] = (((format_2 * 10000) / 4096) * 10000) / g_gain;	/* x_roomt * 10000 */
	g_x_roomt[2] = (((format_3 * 10000) / 4096) * 10000) / g_gain;	/* x_roomt * 10000 */
	g_x_roomt[3] = (((format_4 * 10000) / 4096) * 10000) / g_gain;	/* x_roomt * 10000 */

	pr_info
	    ("[Power/CPU_Thermal] [Thermal calibration] g_ge = %d, g_oe = %d, g_gain = %d, ",
	     g_ge, g_oe, g_gain);
	pr_info
	    ("g_x_roomt1 = %d, g_x_roomt2 = %d, g_x_roomt3 = %d, g_x_roomt4 = %d\n",
	     g_x_roomt[0], g_x_roomt[1], g_x_roomt[2], g_x_roomt[3]);
}

/*
static kal_int32 thermal_cal_exec(kal_uint32 ret)
{
	kal_int32 t_current = 0;
	kal_int32 y_curr = ret;
	kal_int32 format_1 = 0;
	kal_int32 format_2 = 0;
	kal_int32 format_3 = 0;
	kal_int32 format_4 = 0;

	if(ret==0)
	{
		return 0;
	}

	format_1 = (g_degc_cali / 2);
	format_2 = (y_curr - g_oe);
	format_3 = (((((format_2) * 10000) / 4096) * 10000) / g_gain) - g_x_roomt1;
	format_3 = format_3 * 15/18;

	//format_4 = ((format_3 * 100) / 139); // uint = 0.1 deg
	if(g_o_slope_sign==0)
	{
		//format_4 = ((format_3 * 100) / (139+g_o_slope)); // uint = 0.1 deg
		format_4 = ((format_3 * 100) / (165+g_o_slope)); // uint = 0.1 deg
	}
	else
	{
		//format_4 = ((format_3 * 100) / (139-g_o_slope)); // uint = 0.1 deg
		format_4 = ((format_3 * 100) / (165-g_o_slope)); // uint = 0.1 deg
	}
	format_4 = format_4 - (2 * format_4);

	t_current = (format_1 * 10) + format_4; // uint = 0.1 deg

	return t_current;
}
*/

static void raw_to_temperature_roomt(UINT32 raw[], UINT32 temp[])
{
	int i = 0;
	int y_curr[4];
	int format_1 = 0;
	int format_2[4];
	int format_3[4];
	int format_4[4];

	for (i = 0; i < 4; i++) {
		y_curr[i] = raw[i];
		format_2[i] = 0;
		format_3[i] = 0;
		format_4[i] = 0;
	}

	mtktscpu_dprintk
	    ("[Power/CPU_Thermal]: g_ge = %d, g_oe = %d, g_gain = %d, ",
	     g_ge, g_oe, g_gain);
	mtktscpu_dprintk
	    ("g_x_roomt1 = %d, g_x_roomt2 = %d, g_x_roomt3 = %d, g_x_roomt4 = %d\n",
	     g_x_roomt[0], g_x_roomt[1], g_x_roomt[2], g_x_roomt[3]);

	format_1 = (g_degc_cali / 2);
	mtktscpu_dprintk("[Power/CPU_Thermal]: format_1=%d\n", format_1);

	for (i = 0; i < 4; i++) {
		format_2[i] = (y_curr[i] - g_oe);
		format_3[i] = (((((format_2[i]) * 10000) / 4096) * 10000) / g_gain)
			- g_x_roomt[i];	/* 10000 * format3 */
		format_3[i] = (format_3[i] * 15) / 18;
		if (g_o_slope_sign == 0)
			format_4[i] = ((format_3[i] * 100) / (165 + g_o_slope));	/* uint = 0.1 deg */
		else
			format_4[i] = ((format_3[i] * 100) / (165 - g_o_slope));	/* uint = 0.1 deg */

		format_4[i] = format_4[i] - (2 * format_4[i]);

		if (y_curr[i] == 0)
			temp[i] = 0;
		else
			temp[i] = (format_1 * 10) + format_4[i];	/* uint = 0.1 deg */

		mtktscpu_dprintk("[Power/CPU_Thermal]: format_2[%d]=%d, format_3[%d]=%d\n", i,
				 format_2[i], i, format_3[i]);
	}

	return;
}

static kal_int32 temperature_to_raw_roomt(kal_uint32 ret)
{
	/* Ycurr = [(Tcurr - DEGC_cali/2)*(-1)*(165+O_slope)*(18/15)*(1/100000)+X_roomtx]*Gain*4096 + OE */
	/* Tcurr unit is 1000 degC */

	kal_int32 t_curr = ret;
	kal_int32 format_1 = 0;
	kal_int32 format_2 = 0;
	kal_int32 format_3[4] = { 0 };
	kal_int32 format_4[4] = { 0 };
	kal_int32 i, index = 0, temp = 0;

	if (g_o_slope_sign == 0) {
		format_1 = t_curr - (g_degc_cali * 1000 / 2);
		format_2 = format_1 * (165 + g_o_slope) * 18 / 15;
		format_2 = format_2 - 2 * format_2;
		for (i = 0; i < 4; i++) {
			format_3[i] = format_2 / 1000 + g_x_roomt[i] * 10;
			format_4[i] = (format_3[i] * 4096 / 10000 * g_gain) / 100000 + g_oe;
			mtktscpu_dprintk
			    ("[Temperature_to_raw_roomt][roomt%d] format_1=%d, format_2=%d, format_3=%d, format_4=%d\n",
			     i, format_1, format_2, format_3[i], format_4[i]);
		}
	} else {
		format_1 = t_curr - (g_degc_cali * 1000 / 2);
		format_2 = format_1 * (165 - g_o_slope) * 18 / 15;
		format_2 = format_2 - 2 * format_2;
		for (i = 0; i < 4; i++) {
			format_3[i] = format_2 / 1000 + g_x_roomt[i] * 10;
			format_4[i] = (format_3[i] * 4096 / 10000 * g_gain) / 100000 + g_oe;
			mtktscpu_dprintk
			    ("[Temperature_to_raw_roomt][roomt%d] format_1=%d, format_2=%d, format_3=%d, format_4=%d\n",
			     i, format_1, format_2, format_3[i], format_4[i]);
		}
	}

	temp = 0;
	for (i = 0; i < 4; i++) {
		if (temp < format_4[i]) {
			temp = format_4[i];
			index = i;
		}
	}

	mtktscpu_dprintk("[Temperature_to_raw_roomt] temperature=%d, raw[%d]=%d", ret, index,
			 format_4[index]);
	return format_4[index];
}

/*
static int thermal_auxadc_get_data(int times, int Channel)
{
	int ret = 0, data[4], i, ret_value = 0, ret_temp = 0;

	if( IMM_IsAdcInitReady() == 0 )
	{
	mtktscpu_dprintk("[thermal_auxadc_get_data]: AUXADC is not ready\n");
		return 0;
	}

	i = times;
	while (i--)
	{
		ret_value = IMM_GetOneChannelValue(Channel, data, &ret_temp);
		ret += ret_temp;
		mtktscpu_dprintk("[thermal_auxadc_get_data(ADCIN5)]: ret_temp=%d\n",ret_temp);
	}

	ret = ret / times;
	return ret;
}
*/
static void thermal_calibration(void)
{
	if (g_adc_cali_en == 0)
		pr_info("Not Calibration\n");
	thermal_cal_prepare_2(0);
}

static DEFINE_MUTEX(TS_lock);
static int MA_counter = 0, MA_first_time;
/*
static int mtktscpu_get_hw_temp(void)
{
	int ret=0, len=0;
	int t_ret1=0;
	static int abb[60]={0};
	int	i=0;

	mutex_lock(&TS_lock);

	if(proc_write_flag==1)
	{
		MA_counter=0, MA_first_time=0;
		MA_len=MA_len_temp;
		proc_write_flag=0;
		mtktscpu_dprintk("[mtktscpu_get_hw_temp]:MA_len=%d",MA_len);
	}

	//get HW Abb temp(TS_abb)
	DRV_WriteReg32(TS_CON0, 0x40);	//abb
	msleep(1);
	ret = thermal_auxadc_get_data(y_curr_repeat_times, 11);
	mtktscpu_dprintk("[mtktsabb_get_hw_temp]: TSABB average %d times channel 11 (0x%x,0x%x) = %d\n",
					y_curr_repeat_times, DRV_Reg32(TS_CON0), DRV_Reg32(TS_CON1), ret);

	t_ret1 = raw_to_temperature_abb(ret);
	t_ret1 = t_ret1 * 100;
	abb[MA_counter] = t_ret1;
	mtktscpu_dprintk("[mtktscpu_get_hw_temp] T_ABB, %d\n", t_ret1);


	if(MA_counter==0 && MA_first_time==0 && MA_len!=1)
	{
		MA_counter++;

		//get HW Abb temp(TS_abb)
		DRV_WriteReg32(TS_CON0, 0x40);	//abb
		msleep(1);
		ret = thermal_auxadc_get_data(y_curr_repeat_times, 11);
		mtktscpu_dprintk("[mtktsabb_get_hw_temp]: TSABB average %d times channel 11 (0x%x,0x%x) = %d\n",
					y_curr_repeat_times, DRV_Reg32(TS_CON0), DRV_Reg32(TS_CON1), ret);

		t_ret1 = raw_to_temperature_abb(ret);
		t_ret1 = t_ret1 * 100;
		abb[MA_counter] = t_ret1;
		mtktscpu_dprintk("[mtktscpu_get_hw_temp] T_ABB, %d\n", t_ret1);

	}
	DRV_WriteReg32(TS_CON0, DRV_Reg32(TS_CON0) | 0x000000C0); // turn off the sensor buffer to save power


	MA_counter++;
	if(MA_first_time==0)
		len = MA_counter;
	else
		len = MA_len;

	t_ret1 = 0;
	for(i=0; i<len; i++)
	{
		t_ret1 += abb[i];
	}
	last_abb_t = t_ret1 / len;

	mtktscpu_dprintk("[mtktscpu_get_hw_temp] MA_ABB=%d,\n", last_abb_t);
	mtktscpu_dprintk("[mtktscpu_get_hw_temp] MA_counter=%d, MA_first_time=%d, MA_len=%d\n",
					MA_counter, MA_first_time, MA_len);

	if(MA_counter==MA_len )
	{
		MA_counter=0;
		MA_first_time=1;
	}

	mutex_unlock(&TS_lock);

	return last_abb_t;
}
*/

static int CPU_Temp(void)
{
	int curr_raw[4], curr_temp[4];
	int con, index = 0, max_temp;

	curr_raw[0] = DRV_Reg32(TEMPMSR0);
	curr_raw[1] = DRV_Reg32(TEMPMSR1);
	curr_raw[2] = DRV_Reg32(TEMPMSR2);
	curr_raw[3] = DRV_Reg32(TC2_TEMPMSR0);
	curr_raw[0] = curr_raw[0] & 0x0fff;
	curr_raw[1] = curr_raw[1] & 0x0fff;
	curr_raw[2] = curr_raw[2] & 0x0fff;
	curr_raw[3] = curr_raw[3] & 0x0fff;
	raw_to_temperature_roomt(curr_raw, curr_temp);
	curr_temp[0] = curr_temp[0] * 100;
	curr_temp[1] = curr_temp[1] * 100;
	curr_temp[2] = curr_temp[2] * 100;
	curr_temp[3] = curr_temp[3] * 100;

	max_temp = 0;
	for (con = 0; con < 4; con++) {
		if (max_temp < curr_temp[con]) {
			max_temp = curr_temp[con];
			index = con;
		}
	}

#if CONFIG_SUPPORT_MET_MTKTSCPU
	if (met_mtktscpu_dbg) {
		trace_printk("%d,%d,%d,%d\n", curr_temp[0], curr_temp[1], curr_temp[2],
			     curr_temp[3]);
	}
#endif

	mtktscpu_dprintk
	    ("[CPU_Temp] temp[0]=%d, raw[0]=%d, temp[1]=%d, raw[1]=%d, temp[2]=%d, raw[2]=%d, temp[3]=%d, raw[3]=%d\n",
	     curr_temp[0], curr_raw[0], curr_temp[1], curr_raw[1], curr_temp[2], curr_raw[2],
	     curr_temp[3], curr_raw[3]);
	mtktscpu_dprintk("[CPU_Temp] max_temp=%d, index=%d\n", max_temp, index);

	return max_temp;
}

static int mtktscpu_get_TC_temp(void)
{
/* int curr_raw, curr_temp; */
	int /*ret=0, */ len = 0, i = 0;
	int t_ret1 = 0;
	static int abb[60] = { 0 };

	mutex_lock(&TS_lock);

	if (proc_write_flag == 1) {
		MA_counter = 0, MA_first_time = 0;
		MA_len = MA_len_temp;
		proc_write_flag = 0;
		mtktscpu_dprintk("[mtktscpu_get_hw_temp]:MA_len=%d", MA_len);
	}

	t_ret1 = CPU_Temp();
	abb[MA_counter] = t_ret1;
/* mtktscpu_dprintk("[mtktscpu_get_temp] temp=%d, raw=%d\n", t_ret1, curr_raw); */

	if (MA_counter == 0 && MA_first_time == 0 && MA_len != 1) {
		MA_counter++;

		t_ret1 = CPU_Temp();
		abb[MA_counter] = t_ret1;
/* mtktscpu_dprintk("[mtktscpu_get_temp] temp=%d, raw=%d\n", t_ret1, curr_raw); */
	}
	MA_counter++;
	if (MA_first_time == 0)
		len = MA_counter;
	else
		len = MA_len;

	t_ret1 = 0;
	for (i = 0; i < len; i++)
		t_ret1 += abb[i];

	last_abb_t = t_ret1 / len;

	mtktscpu_dprintk("[mtktscpu_get_hw_temp] MA_ABB=%d,\n", last_abb_t);
	mtktscpu_dprintk("[mtktscpu_get_hw_temp] MA_counter=%d, MA_first_time=%d, MA_len=%d\n",
			 MA_counter, MA_first_time, MA_len);

	if (MA_counter == MA_len) {
		MA_counter = 0;
		MA_first_time = 1;
	}

	mutex_unlock(&TS_lock);
	return last_abb_t;
}

static int mtktscpu_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int curr_temp;

	curr_temp = mtktscpu_get_TC_temp();
	curr_temp += mtktscpu_base_temp;

	if ((curr_temp > 120000) | (curr_temp < -30000))
		pr_notice("[Power/CPU_Thermal] CPU T=%d, base temp=%d\n",
			curr_temp, mtktscpu_base_temp);

	/* curr_temp = mtktscpu_get_hw_temp(); */
	*t = (unsigned long)curr_temp;

	g_prev_temp = g_curr_temp;
	g_curr_temp = curr_temp;

	return 0;
}

static int mtktscpu_bind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		set_thermal_ctrl_trigger_SPM(trip_temp[0]);
		mtktscpu_dprintk("[mtktscpu_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktscpu_dprintk("[mtktscpu_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktscpu_dprintk("[mtktscpu_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktscpu_dprintk("[mtktscpu_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktscpu_dprintk("[mtktscpu_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktscpu_dprintk("[mtktscpu_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktscpu_dprintk("[mtktscpu_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktscpu_dprintk("[mtktscpu_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktscpu_dprintk("[mtktscpu_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktscpu_dprintk("[mtktscpu_bind] %s\n", cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtktscpu_dprintk("[mtktscpu_bind] error binding cooling dev\n");
		return -EINVAL;
	} else {
		mtktscpu_dprintk("[mtktscpu_bind] binding OK, %d\n", table_val);
	}

	return 0;
}

static int mtktscpu_unbind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktscpu_dprintk("[mtktscpu_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktscpu_dprintk("[mtktscpu_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktscpu_dprintk("[mtktscpu_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktscpu_dprintk("[mtktscpu_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktscpu_dprintk("[mtktscpu_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktscpu_dprintk("[mtktscpu_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktscpu_dprintk("[mtktscpu_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktscpu_dprintk("[mtktscpu_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktscpu_dprintk("[mtktscpu_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktscpu_dprintk("[mtktscpu_unbind] %s\n", cdev->type);
	} else
		return 0;


	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtktscpu_dprintk("[mtktscpu_unbind] error unbinding cooling dev\n");
		return -EINVAL;
	} else {
		mtktscpu_dprintk("[mtktscpu_unbind] unbinding OK\n");
	}

	return 0;
}

static int mtktscpu_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtktscpu_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktscpu_get_trip_type(struct thermal_zone_device *thermal, int trip,
				  enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktscpu_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				  unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktscpu_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *temperature)
{
	*temperature = MTKTSCPU_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktscpu_dev_ops = {
	.bind = mtktscpu_bind,
	.unbind = mtktscpu_unbind,
	.get_temp = mtktscpu_get_temp,
	.get_mode = mtktscpu_get_mode,
	.set_mode = mtktscpu_set_mode,
	.get_trip_type = mtktscpu_get_trip_type,
	.get_trip_temp = mtktscpu_get_trip_temp,
	.get_crit_temp = mtktscpu_get_crit_temp,
};

/* #define TARGET_TJ (65700) */
#define TARGET_HIGH_RANGE (2000)
#define TARGET_LOW_RANGE (1000)
static int TARGET_TJ = 72400;
static int TARGET_TJ_HIGH = 73400;
static int TARGET_TJ_LOW = 71400;
/* #define PACKAGE_THETA_JA (10) */
static int PACKAGE_THETA_JA = 10;
/* DVFS-4 */
/* #define MINIMUM_CPU_POWER (582) */
/* #define MAXIMUM_CPU_POWER (4124) */
/* DVFS-7 */
#define MINIMUM_CPU_POWER (594)
static int MAXIMUM_CPU_POWER = 4051;
static int MINIMUM_GPU_POWER = 500;
static int MAXIMUM_GPU_POWER = 1449;
static int MINIMUM_TOTAL_POWER = 1094;
static int MAXIMUM_TOTAL_POWER = 5500;
/* DVFS-4 */
/* #define FIRST_STEP_TOTAL_POWER_BUDGET (1550) */
/* DVFS-7 */
/* #define FIRST_STEP_TOTAL_POWER_BUDGET (1450) */
/* #define FIRST_STEP_TOTAL_POWER_BUDGET (1763) */
static int FIRST_STEP_TOTAL_POWER_BUDGET = 4600;

/* 1. MINIMUM_BUDGET_CHANGE = 0 ==> thermal equilibrium maybe at higher than TARGET_TJ_HIGH */
/* 2. Set MINIMUM_BUDGET_CHANGE > 0 if to keep Tj at TARGET_TJ */
#define MINIMUM_BUDGET_CHANGE (50)	/* mW */

static int NOTIFY_HOTPLUG;

static int P_adaptive(int total_power, unsigned int gpu_loading)
{
	static int cpu_power = 0, gpu_power;
	static int last_cpu_power = 0, last_gpu_power;

	if (total_power == 0) {
		if (!NOTIFY_HOTPLUG)
			mt_cpufreq_thermal_protect(MAXIMUM_CPU_POWER);
		else
			thermal_budget_notify(MAXIMUM_CPU_POWER, "cpu_adaptive_cooler");

		mt_gpufreq_thermal_protect(MAXIMUM_GPU_POWER);
#if CONFIG_SUPPORT_MET_MTKTSCPU
		if (met_mtktscpu_dbg)
			trace_printk("%d,%d\n", 5000, 5000);
#endif
		return 0;
	}

	last_cpu_power = cpu_power;
	last_gpu_power = gpu_power;

	if (Num_of_GPU_OPP >= 2) {
		if (gpu_loading < 50)
			gpu_power =
			    (((gpu_loading * gpu_loading) + (500 * gpu_loading) +
			      2000) * 105) / 5000;
		else
			gpu_power =
			    (((gpu_loading * gpu_loading) + (500 * gpu_loading) +
			      3000) * 115) / 5000;
	} else {
		gpu_power = MINIMUM_GPU_POWER;
	}

	cpu_power = total_power - gpu_power;
	if (cpu_power < MINIMUM_CPU_POWER) {
		cpu_power = MINIMUM_CPU_POWER;
		gpu_power = total_power - MINIMUM_CPU_POWER;

		if (gpu_power != last_gpu_power) {
			mt_gpufreq_thermal_protect(gpu_power);
			mtktscpu_dprintk("mt_gpufreq_thermal_protect, gpu_power=%d\n", gpu_power);
		}
	} else if (cpu_power > MAXIMUM_CPU_POWER) {
		cpu_power = MAXIMUM_CPU_POWER;
	}

	if (cpu_power != last_cpu_power) {
		if (!NOTIFY_HOTPLUG) {
			mt_cpufreq_thermal_protect(cpu_power);
			mtktscpu_dprintk("mt_cpufreq_thermal_protect, cpu_power=%d\n", cpu_power);
		} else {
			thermal_budget_notify(cpu_power, "cpu_adaptive_cooler");
			mtktscpu_dprintk("thermal_budget_notify, cpu_power=%d\n", cpu_power);
		}
	}

	mtktscpu_dprintk("P_adaptive: cpu_power=%d, gpu_power=%d\n", cpu_power, gpu_power);
#if CONFIG_SUPPORT_MET_MTKTSCPU
	if (met_mtktscpu_dbg)
		trace_printk("%d,%d\n", cpu_power, gpu_power);
#endif

	return 0;
}

static int _adaptive_power(long prev_temp, long curr_temp, unsigned int gpu_loading)
{
	static int triggered = 0, total_power;
	int delta_power = 0;

	if (cl_dev_state[0] == 1) {
		/* Check if it is triggered */
		if (!triggered) {
			if (curr_temp < TARGET_TJ)
				return 0;
			else {
#ifdef CONFIG_AMAZON_METRICS_LOG
				last_time = ktime_get();
				snprintf(mBuf, METRICS_STR_LEN,
					 "%s;throttling_start=1:CT;1;NR",
					 mPrefix);
				log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", mBuf);
#endif
				triggered = 1;
				total_power = FIRST_STEP_TOTAL_POWER_BUDGET;
				return P_adaptive(total_power, gpu_loading);
			}
		}

		/* Adjust total power budget if necessary */
		if ((curr_temp > TARGET_TJ_HIGH) && (curr_temp >= prev_temp)) {
			delta_power = (curr_temp - prev_temp) / PACKAGE_THETA_JA;
			if (prev_temp > TARGET_TJ_HIGH) {
				delta_power =
				    (delta_power >
				     MINIMUM_BUDGET_CHANGE) ? delta_power : MINIMUM_BUDGET_CHANGE;
			}
			total_power -= delta_power;
			total_power =
			    (total_power > MINIMUM_TOTAL_POWER) ? total_power : MINIMUM_TOTAL_POWER;
		}

		if ((curr_temp < TARGET_TJ_LOW) && (curr_temp <= prev_temp)) {
			delta_power = (prev_temp - curr_temp) / PACKAGE_THETA_JA;
			if (prev_temp < TARGET_TJ_LOW) {
				delta_power =
				    (delta_power >
				     MINIMUM_BUDGET_CHANGE) ? delta_power : MINIMUM_BUDGET_CHANGE;
			}
			total_power += delta_power;
			total_power =
			    (total_power < MAXIMUM_TOTAL_POWER) ? total_power : MAXIMUM_TOTAL_POWER;
		}

		return P_adaptive(total_power, gpu_loading);
	} else {
		if (triggered) {
#ifdef CONFIG_AMAZON_METRICS_LOG
			ktime_t delta = ktime_sub(ktime_get(), last_time);
			snprintf(mBuf, METRICS_STR_LEN,
				 "%s;throttling_stop=1;for_ms=%lld:CT;1;NR",
				 mPrefix,
				 ktime_to_ms(delta));
			log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", mBuf);
#endif

			triggered = 0;
			return P_adaptive(0, 0);
		}
	}

	return 0;
}

static int previous_step = -1;
/*
static int step0_mask[11] = {1,1,1,1,1,1,1,1,1,1,1};
static int step1_mask[11] = {0,1,1,1,1,1,1,1,1,1,1};
static int step2_mask[11] = {0,0,1,1,1,1,1,1,1,1,1};
static int step3_mask[11] = {0,0,0,1,1,1,1,1,1,1,1};
static int step4_mask[11] = {0,0,0,0,1,1,1,1,1,1,1};
static int step5_mask[11] = {0,0,0,0,0,1,1,1,1,1,1};
static int step6_mask[11] = {0,0,0,0,0,0,1,1,1,1,1};
static int step7_mask[11] = {0,0,0,0,0,0,0,1,1,1,1};
static int step8_mask[11] = {0,0,0,0,0,0,0,0,1,1,1};
static int step9_mask[11] = {0,0,0,0,0,0,0,0,0,1,1};
static int step10_mask[11]= {0,0,0,0,0,0,0,0,0,0,1};
*/
static int P_static(void)
{
	int i = 0;
	int power = 0;
	for (i = 0; i < Num_of_OPP; i++) {
		if (1 == cl_dev_state[i]) {
			if (i != previous_step) {
				xlog_printk(ANDROID_LOG_INFO, "Power/CPU_Thermal",
					    "previous_opp=%d, now_opp=%d\n", previous_step, i);
				previous_step = i;
				if (Num_of_GPU_OPP > 2) {
					power =
					    (i * 100 + 700) - mtk_gpu_power[Num_of_GPU_OPP -
									    2].gpufreq_power;
					mt_cpufreq_thermal_protect(power);
					mt_gpufreq_thermal_protect(mtk_gpu_power
								   [Num_of_GPU_OPP -
								    2].gpufreq_power);
				} else {
					mt_cpufreq_thermal_protect(i * 100 + 700);
				}
			}
			break;
		}
	}

	if (i == Num_of_OPP) {
		if (previous_step != -1) {
			previous_step = -1;
			xlog_printk(ANDROID_LOG_INFO, "Power/CPU_Thermal",
				    "Free all thermal limit, previous_opp=%d\n", previous_step);
			mt_cpufreq_thermal_protect(MAXIMUM_CPU_POWER);
			mt_gpufreq_thermal_protect(MAXIMUM_GPU_POWER);
		}
	}
	return 0;
}


int mtk_cpufreq_register(struct mt_cpu_power_info *freqs, int num)
{
	int i = 0;
	mtktscpu_dprintk("[Power/CPU_Thermal] setup cpu power table\n");

	mtk_cpu_power = kzalloc((num) * sizeof(struct mt_cpu_power_info), GFP_KERNEL);
	if (mtk_cpu_power == NULL)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		mtk_cpu_power[i].cpufreq_khz = freqs[i].cpufreq_khz;
		mtk_cpu_power[i].cpufreq_khz_ca7 = freqs[i].cpufreq_khz_ca7;
		mtk_cpu_power[i].cpufreq_khz_ca15 = freqs[i].cpufreq_khz_ca15;
		mtk_cpu_power[i].cpufreq_ncpu = freqs[i].cpufreq_ncpu;
		mtk_cpu_power[i].cpufreq_ncpu_ca7 = freqs[i].cpufreq_ncpu_ca7;
		mtk_cpu_power[i].cpufreq_ncpu_ca15 = freqs[i].cpufreq_ncpu_ca15;
		mtk_cpu_power[i].cpufreq_power = freqs[i].cpufreq_power;
		mtk_cpu_power[i].cpufreq_power_ca7 = freqs[i].cpufreq_power_ca7;
		mtk_cpu_power[i].cpufreq_power_ca15 = freqs[i].cpufreq_power_ca15;

		mtktscpu_dprintk
		    ("[Power/CPU_Thermal] freqs[%d].cpufreq_khz=%u, .cpufreq_khz_ca7=%u, .cpufreq_khz_ca15=%u\n",
		     i, freqs[i].cpufreq_khz, freqs[i].cpufreq_khz_ca7, freqs[i].cpufreq_khz_ca15);
		mtktscpu_dprintk
		    ("[Power/CPU_Thermal] freqs[%d].cpufreq_ncpu=%u, .cpufreq_ncpu_ca7=%u, .cpufreq_ncpu_ca15=%u\n",
		     i, freqs[i].cpufreq_ncpu, freqs[i].cpufreq_ncpu_ca7,
		     freqs[i].cpufreq_ncpu_ca15);
		mtktscpu_dprintk
		    ("[Power/CPU_Thermal] freqs[%d].cpufreq_power=%u, .cpufreq_power_ca7=%u, .cpufreq_power_ca15=%u\n",
		     i, freqs[i].cpufreq_power, freqs[i].cpufreq_power_ca7,
		     freqs[i].cpufreq_power_ca15);
	}

	MAXIMUM_CPU_POWER = mtk_cpu_power[0].cpufreq_power;
	MAXIMUM_TOTAL_POWER = MAXIMUM_CPU_POWER + MAXIMUM_GPU_POWER;
	mtktscpu_dprintk("[Power/CPU_Thermal] MAXIMUM_CPU_POWER=%d, MAXIMUM_TOTAL_POWER=%d\n",
			 MAXIMUM_CPU_POWER, MAXIMUM_TOTAL_POWER);

	return 0;
}
EXPORT_SYMBOL(mtk_cpufreq_register);


int mtk_gpufreq_register(struct mt_gpufreq_power_info *freqs, int num)
{
	int i = 0;
	mtktscpu_dprintk("[Power/CPU_Thermal] setup gpu power table\n");

	mtk_gpu_power = kzalloc((num) * sizeof(struct mt_gpufreq_power_info), GFP_KERNEL);
	if (mtk_gpu_power == NULL)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		mtk_gpu_power[i].gpufreq_khz = freqs[i].gpufreq_khz;
		mtk_gpu_power[i].gpufreq_power = freqs[i].gpufreq_power;

		mtktscpu_dprintk("[Power/CPU_Thermal] freqs[%d].gpufreq_khz=%u, .gpufreq_power=%u\n",
			i, freqs[i].gpufreq_khz, freqs[i].gpufreq_power);
	}

	Num_of_GPU_OPP = num;

	MAXIMUM_GPU_POWER = mtk_gpu_power[0].gpufreq_power;
	MINIMUM_GPU_POWER = mtk_gpu_power[Num_of_GPU_OPP - 1].gpufreq_power;
	MAXIMUM_TOTAL_POWER = MAXIMUM_CPU_POWER + MAXIMUM_GPU_POWER;
	mtktscpu_dprintk("[Power/CPU_Thermal] MAXIMUM_GPU_POWER=%d, MAXIMUM_TOTAL_POWER=%d\n",
			 MAXIMUM_GPU_POWER, MAXIMUM_TOTAL_POWER);

	return 0;
}
EXPORT_SYMBOL(mtk_gpufreq_register);


static int cpufreq_F0x2_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	mtktscpu_dprintk("cpufreq_F0x2_get_max_state\n");
	*state = 1;
	return 0;
}

static int cpufreq_F0x2_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	int i = 0;
	mtktscpu_dprintk("get_cur_state, %s\n", cdev->type);

	for (i = 0; i < Num_of_OPP; i++) {
		if (!strcmp(cdev->type, &cooler_name[i * 20])) {
			*state = cl_dev_state[i];
			mtktscpu_dprintk("get_cur_state: cl_dev_state[%d]=%d\n", i,
					 cl_dev_state[i]);
			break;
		}
	}
	return 0;
}

static int cpufreq_F0x2_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	int i = 0;
	mtktscpu_dprintk("set_cur_state, %s\n", cdev->type);

	for (i = 0; i < Num_of_OPP; i++) {
		if (!strcmp(cdev->type, &cooler_name[i * 20])) {
			cl_dev_state[i] = state;
			mtktscpu_dprintk("set_cur_state: cl_dev_state[%d]=%d\n", i,
					 cl_dev_state[i]);
			if (!strcmp(cdev->type, adaptive_cooler_name)) {
				_adaptive_power(g_prev_temp, g_curr_temp,
						mt_gpufreq_get_cur_load());
			} else {
				P_static();
			}
			break;
		}
	}
	return 0;
}



/*
 * cooling device callback functions (mtktscpu_cooling_sysrst_ops)
 * 1 : ON and 0 : OFF
 */
static int sysrst_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	mtktscpu_dprintk("sysrst_get_max_state\n");
	*state = 1;
	return 0;
}

static int sysrst_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	mtktscpu_dprintk("sysrst_get_cur_state\n");
	*state = cl_dev_sysrst_state;
	return 0;
}

static int sysrst_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	mtktscpu_dprintk("sysrst_set_cur_state\n");
	cl_dev_sysrst_state = state;
	if (cl_dev_sysrst_state == 1) {
		pr_emerg("Power/CPU_Thermal: reset, reset, reset!!!");
		pr_emerg("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		pr_emerg("*****************************************");
		pr_emerg("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
#ifdef CONFIG_AMAZON_METRICS_LOG
		snprintf(mBuf, METRICS_STR_LEN,
			 "%s;thermal_caught_shutdown=1;CT;1:NR",
			 mPrefix);
		log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", mBuf);
#endif
		orderly_poweroff(true);
	}
	return 0;
}

/* bind fan callbacks to fan device */

static struct thermal_cooling_device_ops mtktscpu_cooling_F0x2_ops = {
	.get_max_state = cpufreq_F0x2_get_max_state,
	.get_cur_state = cpufreq_F0x2_get_cur_state,
	.set_cur_state = cpufreq_F0x2_set_cur_state,
};

static struct thermal_cooling_device_ops mtktscpu_cooling_sysrst_ops = {
	.get_max_state = sysrst_get_max_state,
	.get_cur_state = sysrst_get_cur_state,
	.set_cur_state = sysrst_set_cur_state,
};

static int mtktscpu_show_opp(struct seq_file *s, void *v)
{
	int i = 0;

	for (i = 0; i < Num_of_OPP; i++) {
		if (1 == cl_dev_state[i]) {
			seq_printf(s, "%s\n", &cooler_name[i * 20]);
			break;
		}
	}

	/* CTFang 2012/10/26: if no limit, still return a string to indicate there is no limit. */
	if (i == Num_of_OPP)
		seq_puts(s, "no_limit\n");

	return 0;
}

static int mtktscpu_open_opp(struct inode *inode, struct file *file)
{
	return single_open(file, mtktscpu_show_opp, NULL);
}

static int mtktscpu_show_log(struct seq_file *s, void *v)
{
	seq_printf(s, "[mtktscpu_show_log] log = %d\n", mtktscpu_debug_log);
	return 0;
}

static int mtktscpu_open_log(struct inode *inode, struct file *file)
{
	return single_open(file, mtktscpu_show_log, NULL);
}

static int mtktscpu_show(struct seq_file *s, void *v)
{
	int i;
	seq_puts(s, "[mtktscpu_show]\n");

	for (i = 0; i < 10; i++)
		seq_printf(s, "trip_%d_temp=%d,", i, trip_temp[i]);
	seq_puts(s, "\n");

	for (i = 0; i < 10; i++)
		seq_printf(s, "g_THERMAL_TRIP_%d=%d,", i, g_THERMAL_TRIP[i]);
	seq_puts(s, "\n");

	seq_printf(s, "cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
				  g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
	seq_printf(s, "cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
				  g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

	seq_printf(s, "time_ms=%d\n", interval);

	return 0;
}

static int mtktscpu_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktscpu_show, NULL);
}

static ssize_t mtktscpu_write_log(struct file *file, const char *buffer, size_t count,
				  loff_t *data)
{
	char desc[32];
	int log_switch, base_temp;
	int len = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &log_switch, &base_temp) == 2) {
		mtktscpu_debug_log = log_switch;
		mtktscpu_dprintk("[mtktscpu_write_log] mtktscpu_debug_log=%d\n",
				 mtktscpu_debug_log);
		mtktscpu_base_temp = base_temp;
		mtktscpu_dprintk("[mtktscpu_write_log] mtktscpu_base_temp=%d\n",
				 mtktscpu_base_temp);
		return count;
	} else if (sscanf(desc, "%d", &log_switch) == 1) {
		mtktscpu_debug_log = log_switch;
		mtktscpu_dprintk("[mtktscpu_write_log] mtktscpu_debug_log=%d\n",
				 mtktscpu_debug_log);
		return count;
	} else {
		mtktscpu_dprintk("[mtktscpu_write_log] bad argument\n");
	}
	return -EINVAL;
}

static int mtktscpu_show_dtm_setting(struct seq_file *s, void *v)
{
	seq_printf(s, "first_step = %d\n", FIRST_STEP_TOTAL_POWER_BUDGET);
	seq_printf(s, "theta = %d\n", PACKAGE_THETA_JA);
	return 0;
}

static int mtktscpu_open_dtm_setting(struct inode *inode, struct file *file)
{
	return single_open(file, mtktscpu_show_dtm_setting, NULL);
}

static ssize_t mtktscpu_write_dtm_setting(struct file *file, const char *buffer, size_t count,
					  loff_t *data)
{
	char desc[32];
	char arg_name[32] = { 0 };
	int arg_val = 0;
	int len = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (2 == sscanf(desc, "%s %d", arg_name, &arg_val)) {
		if (0 == strncmp(arg_name, "first_step", 10)) {
			FIRST_STEP_TOTAL_POWER_BUDGET = arg_val;
			mtktscpu_dprintk("[mtktscpu_write_dtm_setting] first_step=%d\n",
					 FIRST_STEP_TOTAL_POWER_BUDGET);
		} else if (0 == strncmp(arg_name, "theta", 5)) {
			PACKAGE_THETA_JA = arg_val;
			mtktscpu_dprintk("[mtktscpu_write_dtm_setting] theta=%d\n",
					 PACKAGE_THETA_JA);
		} else if (0 == strncmp(arg_name, "hotplug", 7)) {
			NOTIFY_HOTPLUG = arg_val;
			mtktscpu_dprintk("[mtktscpu_write_dtm_setting] notify_hotplug=%d\n",
					 NOTIFY_HOTPLUG);
		}
		return count;
	} else {
		mtktscpu_dprintk("[mtktscpu_write_dtm_setting] bad argument\n");
	}
	return -EINVAL;
}

static int mtktscpu_register_thermal(void);
static void mtktscpu_unregister_thermal(void);

static ssize_t mtktscpu_write(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	int len = 0, time_msec = 0;
	int trip[10] = { 0 };
	int t_type[10] = { 0 };
	int i;
	char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
	char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
	char desc[512];
	int index;
/* int curr_temp; */

/* return 0; //test */

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf
	    (desc,
	     "%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d",
	     &num_trip, &trip[0], &t_type[0], bind0, &trip[1], &t_type[1], bind1, &trip[2],
	     &t_type[2], bind2, &trip[3], &t_type[3], bind3, &trip[4], &t_type[4], bind4, &trip[5],
	     &t_type[5], bind5, &trip[6], &t_type[6], bind6, &trip[7], &t_type[7], bind7, &trip[8],
	     &t_type[8], bind8, &trip[9], &t_type[9], bind9, &time_msec, &MA_len_temp) == 33) {

		if (num_trip < 1 || num_trip > 10) {
			mtktscpu_dprintk("[mtktscpu_write] bad argument: num_trip=%d\n", num_trip);
			return -EINVAL;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0] = g_bind4[0] = g_bind5[0] =
		    g_bind6[0] = g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = bind0[i];
			g_bind1[i] = bind1[i];
			g_bind2[i] = bind2[i];
			g_bind3[i] = bind3[i];
			g_bind4[i] = bind4[i];
			g_bind5[i] = bind5[i];
			g_bind6[i] = bind6[i];
			g_bind7[i] = bind7[i];
			g_bind8[i] = bind8[i];
			g_bind9[i] = bind9[i];
		}

#if 1				/* for cpu adative cooler setting */
		index = -1;

		if (!strcmp(bind0, adaptive_cooler_name))
			index = 0;
		else if (!strcmp(bind1, adaptive_cooler_name))
			index = 1;
		else if (!strcmp(bind2, adaptive_cooler_name))
			index = 2;
		else if (!strcmp(bind3, adaptive_cooler_name))
			index = 3;
		else if (!strcmp(bind4, adaptive_cooler_name))
			index = 4;
		else if (!strcmp(bind5, adaptive_cooler_name))
			index = 5;
		else if (!strcmp(bind6, adaptive_cooler_name))
			index = 6;
		else if (!strcmp(bind7, adaptive_cooler_name))
			index = 7;
		else if (!strcmp(bind8, adaptive_cooler_name))
			index = 8;
		else if (!strcmp(bind9, adaptive_cooler_name))
			index = 9;

		if (index != -1) {
			TARGET_TJ = trip[index];
			TARGET_TJ_HIGH = TARGET_TJ + TARGET_HIGH_RANGE;
			TARGET_TJ_LOW = TARGET_TJ - TARGET_LOW_RANGE;
		}
#endif

		mtktscpu_dprintk("[mtktscou_write] ");
		for (i = 0; i < 10; i++)
			mtktscpu_dprintk("g_THERMAL_TRIP_%d=%d,", i, g_THERMAL_TRIP[i]);
		mtktscpu_dprintk("\n");

		mtktscpu_dprintk("[mtktscpu_write] ");
		mtktscpu_dprintk("cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
				  g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
		mtktscpu_dprintk("cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
				  g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = trip[i];

		interval = time_msec;

		mtktscpu_dprintk("[mtktsbattery_write] ");
		for (i = 0; i < 10; i++)
			mtktscpu_dprintk("trip_%d_temp=%d,", i, trip_temp[i]);
		mtktscpu_dprintk("time_ms=%d, num_trip=%d\n", interval, num_trip);


		/* get temp, set high low threshold */
/*		curr_temp = CPU_Temp();
		for(i=0; i<num_trip; i++)
		{
			if(curr_temp>trip_temp[i])
				break;
		}
		if(i==0)
		{
			pr_info("Power/CPU_Thermal: [mtktscpu_write] setting error");
		}
		else if(i==num_trip)
			set_high_low_threshold(trip_temp[i-1], 10000);
		else
			set_high_low_threshold(trip_temp[i-1], trip_temp[i]);
*/
		mtktscpu_dprintk("[mtktscpu_write] mtktscpu_register_thermal\n");

		if (thz_dev) {
			thz_dev->polling_delay = time_msec;
			schedule_work(&twork);
		}
		proc_write_flag = 1;

		return count;
	} else {
		mtktscpu_dprintk("[mtktscpu_write] bad argument\n");
	}

	return -EINVAL;
}

int mtktscpu_register_DVFS_hotplug_cooler(void)
{
	int i;

	mtktscpu_dprintk("[mtktscpu_register_DVFS_hotplug_cooler]\n");
	for (i = 0; i < Num_of_OPP; i++) {
		cl_dev[i] = mtk_thermal_cooling_device_register(&cooler_name[i * 20], NULL,
								&mtktscpu_cooling_F0x2_ops);
	}
	cl_dev_sysrst = mtk_thermal_cooling_device_register("mtktscpu-sysrst", NULL,
							    &mtktscpu_cooling_sysrst_ops);

	return 0;
}

static int mtktscpu_register_thermal(void)
{
	mtktscpu_dprintk("[mtktscpu_register_thermal]\n");

	/* trips : trip 0~3 */
	thz_dev = mtk_thermal_zone_device_register("mtktscpu", num_trip, NULL,
						   &mtktscpu_dev_ops, 0, 0, 0, interval);
	return 0;
}

void mtktscpu_unregister_DVFS_hotplug_cooler(void)
{
	int i;
	for (i = 0; i < Num_of_OPP; i++) {
		if (cl_dev[i]) {
			mtk_thermal_cooling_device_unregister(cl_dev[i]);
			cl_dev[i] = NULL;
		}
	}
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

static void mtktscpu_unregister_thermal(void)
{
	mtktscpu_dprintk("[mtktscpu_unregister_thermal]\n");
	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtk_thermal_suspend(struct platform_device *dev, pm_message_t state)
{
	int temp = 0;
	int cnt = 0;

	mtktscpu_dprintk("[mtk_thermal_suspend]\n");
#if 1				/* CJ's suggestion, from Jerry.Wu */
	cnt = 0;
	while (cnt < 10) {	/* if bit7 and bit0=0 */
		temp = DRV_Reg32(TEMPMSRCTL1);
		mtktscpu_dprintk("TEMPMSRCTL1 = 0x%x\n", temp);
		/*
		   TEMPMSRCTL1[7]:Temperature measurement bus status[1]
		   TEMPMSRCTL1[0]:Temperature measurement bus status[0]

		   00: IDLE                           <=can pause    ,TEMPMSRCTL1[7][0]=0x00
		   01: Write transaction                                 <=can not pause,TEMPMSRCTL1[7][0]=0x01
		   10: Read transaction                          <=can not pause,TEMPMSRCTL1[7][0]=0x10
		   11: Waiting for read after Write   <=can pause    ,TEMPMSRCTL1[7][0]=0x11
		 */
		if (((temp & 0x81) == 0x00) || ((temp & 0x81) == 0x81)) {
			/*
			   Pause periodic temperature measurement for sensing point 0,sensing point 1,sensing point 2
			 */
			/* set bit1=bit2=bit3=1 to pause sensing point 0,1,2 */
			DRV_WriteReg32(TEMPMSRCTL1, (temp | 0x0E));

			break;
		}
		mtktscpu_dprintk("temp=0x%x, cnt=%d\n", temp, cnt);
		udelay(10);
		cnt++;
	}

	cnt = 0;
	while (cnt < 10) {	/* if bit7 and bit0=0 */
		temp = DRV_Reg32(TC2_TEMPMSRCTL1);
		mtktscpu_dprintk("TC2_TEMPMSRCTL1 = 0x%x\n", temp);
		/*
		   TEMPMSRCTL1[7]:Temperature measurement bus status[1]
		   TEMPMSRCTL1[0]:Temperature measurement bus status[0]

		   00: IDLE                           <=can pause    ,TEMPMSRCTL1[7][0]=0x00
		   01: Write transaction                                 <=can not pause,TEMPMSRCTL1[7][0]=0x01
		   10: Read transaction                          <=can not pause,TEMPMSRCTL1[7][0]=0x10
		   11: Waiting for read after Write   <=can pause    ,TEMPMSRCTL1[7][0]=0x11
		 */
		if (((temp & 0x81) == 0x00) || ((temp & 0x81) == 0x81)) {
			/*
			   Pause periodic temperature measurement for sensing point 0,sensing point 1,sensing point 2
			 */
			/* set bit1=bit2=bit3=1 to pause sensing point 0,1,2 */
			DRV_WriteReg32(TC2_TEMPMSRCTL1, (temp | 0x0E));

			break;
		}
		mtktscpu_dprintk("temp=0x%x, cnt=%d\n", temp, cnt);
		udelay(10);
		cnt++;
	}
#endif

	/* disable ALL periodoc temperature sensing point */
	DRV_WriteReg32(TEMPMONCTL0, 0x00000000);
	DRV_WriteReg32(TC2_TEMPMONCTL0, 0x00000000);

	DRV_WriteReg32(TS_CON0_V, DRV_Reg32(TS_CON0_V) | 0x000000C0);	/* turn off the sensor buffer to save power */

	return 0;
}

static int mtk_thermal_resume(struct platform_device *dev)
{
	mtktscpu_dprintk("[mtk_thermal_resume]\n");
	thermal_reset_and_initial();
	set_thermal_ctrl_trigger_SPM(trip_temp[0]);

	return 0;
}

static const struct file_operations mtktscpu_fops = {
	.owner = THIS_MODULE,
	.write = mtktscpu_write,
	.open = mtktscpu_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations mtktscpu_log_fops = {
	.owner = THIS_MODULE,
	.write = mtktscpu_write_log,
	.open = mtktscpu_open_log,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations mtktscpu_opp_fops = {
	.owner = THIS_MODULE,
	.open = mtktscpu_open_opp,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations mtktscpu_dtm_setting_fops = {
	.owner = THIS_MODULE,
	.write = mtktscpu_write_dtm_setting,
	.open = mtktscpu_open_dtm_setting,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void mtktscpu_work(struct work_struct *work)
{
	if (thz_dev)
		thermal_zone_device_update(thz_dev);
}

static int mtk_thermal_probe(struct platform_device *pdev)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktscpu_dir = NULL;

	mtktscpu_dprintk("[mtktscpu_init]\n");

	thermal_cal_prepare();
	thermal_calibration();

	DRV_WriteReg32(TS_CON0_V, DRV_Reg32(TS_CON0_V) | 0x000000C0);	/* turn off the sensor buffer to save power */
	thermal_reset_and_initial();
	/* set_high_low_threshold(20000, 10000);//test */

	err = init_cooler();
	if (err)
		return err;

	err = mtktscpu_register_DVFS_hotplug_cooler();
	if (err)
		return err;

	err = mtktscpu_register_thermal();
	if (err)
		goto err_unreg;

	err =
	    request_irq(MT_PTP_THERM_IRQ_ID, thermal_interrupt_handler, IRQF_TRIGGER_LOW,
			THERMAL_NAME, NULL);
	if (err)
		mtktscpu_dprintk("[mtktscpu_init] IRQ register fail\n");

	err =
	    request_irq(MT_CA15_PTP_THERM_IRQ_ID, TC2_thermal_interrupt_handler, IRQF_TRIGGER_LOW,
			TC2_THERMAL_NAME, NULL);
	if (err)
		mtktscpu_dprintk("[mtktscpu_init] IRQ TC2 register fail\n");

	mtktscpu_dir = proc_mkdir("mtktscpu", NULL);
	if (!mtktscpu_dir) {
		mtktscpu_dprintk("[mtktscpu_init]: mkdir /proc/mtktscpu failed\n");
	} else {
		entry = proc_create("mtktscpu", S_IRUGO | S_IWUSR, mtktscpu_dir, &mtktscpu_fops);
		if (!entry) {
			mtktscpu_dprintk
			    ("[mtktscpu_init]: create /proc/mtktscpu/mtktscpu failed\n");
		}

		entry =
		    proc_create("mtktscpu_log", S_IRUGO | S_IWUSR, mtktscpu_dir,
				&mtktscpu_log_fops);
		if (!entry) {
			mtktscpu_dprintk
			    ("[mtktscpu_init]: create /proc/mtktscpu/mtktscpu_log failed\n");
		}

		entry = proc_create("mtktscpu_opp", S_IRUGO, mtktscpu_dir, &mtktscpu_opp_fops);
		if (!entry) {
			mtktscpu_dprintk
			    ("[mtktscpu_init]: create /proc/mtktscpu/mtktscpu_opp failed\n");
		}

		entry =
		    proc_create("mtktscpu_dtm_setting", S_IRUGO | S_IWUSR, mtktscpu_dir,
				&mtktscpu_dtm_setting_fops);
		if (!entry) {
			mtktscpu_dprintk
			    ("[mtktscpu_init]: create /proc/mtktscpu/mtktscpu_dtm_setting failed\n");
		}
	}

	INIT_WORK(&twork, mtktscpu_work);
	return 0;

 err_unreg:
	mtktscpu_unregister_DVFS_hotplug_cooler();
	return err;
}

static struct platform_driver mtk_thermal_driver = {
	.remove = NULL,
	.shutdown = NULL,
	.probe = mtk_thermal_probe,
	.suspend = mtk_thermal_suspend,
	.resume = mtk_thermal_resume,
	.driver = {
		   .name = THERMAL_NAME,
		   },
};

static int __init mtktscpu_init(void)
{
	return platform_driver_register(&mtk_thermal_driver);
}

static void __exit mtktscpu_exit(void)
{
	mtktscpu_dprintk("[mtktscpu_exit]\n");
	cancel_work_sync(&twork);
	mtktscpu_unregister_thermal();
	mtktscpu_unregister_DVFS_hotplug_cooler();
	return platform_driver_unregister(&mtk_thermal_driver);
}
module_init(mtktscpu_init);
module_exit(mtktscpu_exit);

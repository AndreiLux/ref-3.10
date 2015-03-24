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
#include <linux/reboot.h>

#include <mach/system.h>
#include "mach/mtk_thermal_monitor.h"
#include "mach/mt_typedefs.h"
#include "mach/mt_thermal.h"

/* #include <mach/pmic_mt6329_hw_bank1.h> */
/* #include <mach/pmic_mt6329_sw_bank1.h> */
/* #include <mach/pmic_mt6329_hw.h> */
/* #include <mach/pmic_mt6329_sw.h> */
/* #include <mach/upmu_common_sw.h> */
#include <mach/upmu_hw.h>
#include <mach/mt_pmic_wrap.h>
#include "mach/pmic_sw.h"

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#ifndef THERMO_METRICS_STR_LEN
#define THERMO_METRICS_STR_LEN 128
#endif
static char *mPrefix = "pmic_thermal_zone:def:monitor=1";
static char mBuf[THERMO_METRICS_STR_LEN];
#endif

static unsigned int interval = 1000;	/* mseconds, 0 : no auto polling */
static unsigned int trip_temp[10] = { 150000, 110000, 100000, 90000, 80000, 70000, 65000, 60000, 55000, 50000 };

static unsigned int cl_dev_sysrst_state;
static struct thermal_zone_device *thz_dev;

static struct thermal_cooling_device *cl_dev_sysrst;
static int mtktspmic_debug_log;
static int kernelmode;
static struct work_struct twork;

static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip = 1;
static char g_bind0[20] = "mtktspmic-sysrst";
static char g_bind1[20] = { 0 };
static char g_bind2[20] = { 0 };
static char g_bind3[20] = { 0 };
static char g_bind4[20] = { 0 };
static char g_bind5[20] = { 0 };
static char g_bind6[20] = { 0 };
static char g_bind7[20] = { 0 };
static char g_bind8[20] = { 0 };
static char g_bind9[20] = { 0 };

#define mtktspmic_TEMP_CRIT 150000	/* 150.000 degree Celsius */

#define mtktspmic_dprintk(fmt, args...)   \
do {									\
	if (mtktspmic_debug_log) {				\
		xlog_printk(ANDROID_LOG_INFO, "Power/PMIC_Thermal", fmt, ##args); \
	}								   \
} while (0)


static kal_int32 g_adc_ge;
static kal_int32 g_adc_oe;
static kal_int32 g_o_vts;
static kal_int32 g_degc_cali;
static kal_int32 g_adc_cali_en;
static kal_int32 g_o_slope;
static kal_int32 g_o_slope_sign;
static kal_int32 g_id;

#define y_pmic_repeat_times	1

static u16 pmic_read(u16 addr)
{
	u32 rdata = 0;
	pwrap_read((u32) addr, &rdata);
	return (u16) rdata;
}

/*
static void pmic_write(u16 addr, u16 data)
{
	pwrap_write((u32)addr, (u32)data);
}
*/
static void pmic_cali_prepare(void)
{
	kal_uint32 temp0, temp1, temp2, sign;

	temp0 = pmic_read(0x1E2);
	temp1 = pmic_read(0x1EA);
	temp2 = pmic_read(0x1EC);
	pr_info("Power/PMIC_Thermal: Reg(0x1E2)=0x%x, Reg(0x1EA)=0x%x, Reg(0x1EC)=0x%x\n", temp0,
		temp1, temp2);

	g_adc_ge = (temp0 >> 1) & 0x007f;
	g_adc_oe = (temp0 >> 8) & 0x003f;
	g_o_vts = ((temp2 & 0x0001) << 8) + ((temp1 >> 8) & 0x00ff);
	g_degc_cali = (temp1 >> 2) & 0x003f;
	g_adc_cali_en = (temp1 >> 1) & 0x0001;
	g_o_slope_sign = (temp2 >> 1) & 0x0001;
	g_o_slope = (temp2 >> 2) & 0x003f;
	g_id = (temp2 >> 8) & 0x0001;

	sign = (temp0 >> 7) & 0x0001;
	if (sign == 1)
		g_adc_ge = g_adc_ge - 0x80;

	sign = (temp0 >> 13) & 0x0001;
	if (sign == 1)
		g_adc_oe = g_adc_oe - 0x40;

	if (g_id == 0)
		g_o_slope = 0;

	if (g_adc_cali_en != 1) {
		/* no cali, use default vaule */
		g_adc_ge = 0;
		g_adc_oe = 0;
		/* g_o_vts = 608; */
		g_o_vts = 352;
		g_degc_cali = 50;
		g_o_slope = 0;
		g_o_slope_sign = 0;
	}
	pr_info
	    ("Power/PMIC_Thermal: ");
	pr_info
	    ("g_adc_ge = 0x%x, g_adc_oe = 0x%x, g_o_vts = 0x%x, g_degc_cali = 0x%x,",
	     g_adc_ge, g_adc_oe, g_o_vts, g_degc_cali);
	pr_info
	    ("g_adc_cali_en = 0x%x, g_o_slope = 0x%x, g_o_slope_sign = 0x%x, g_id = 0x%x\n",
	     g_adc_cali_en, g_o_slope, g_o_slope_sign, g_id);

}

static kal_int32 thermal_cal_exec(kal_uint32 ret)
{
	kal_int32 t_current = 0;
	kal_int32 y_curr = ret;
	kal_int32 format_1 = 0;
	kal_int32 format_2 = 0;
	kal_int32 format_3 = 0;
	kal_int32 format_4 = 0;

	if (ret == 0)
		return 0;

	format_1 = (g_degc_cali * 1000 / 2);
	format_2 = (g_adc_ge + 1024) * (g_o_vts + 256) + g_adc_oe * 1024;
	format_3 = (format_2 * 1200) / 1024 * 100 / 1024;
	mtktspmic_dprintk("format1=%d, format2=%d, format3=%d\n", format_1, format_2, format_3);

	if (g_o_slope_sign == 0) {
		/* format_4 = ((format_3 * 1000) / (164+g_o_slope));//unit = 0.001 degress */
		/* format_4 = (y_curr*1000 - format_3)*100 / (164+g_o_slope); */
		format_4 = (y_curr * 100 - format_3) * 1000 / (171 + g_o_slope);
	} else {
		/* format_4 = ((format_3 * 1000) / (164-g_o_slope)); */
		/* format_4 = (y_curr*1000 - format_3)*100 / (164-g_o_slope); */
		format_4 = (y_curr * 100 - format_3) * 1000 / (171 - g_o_slope);
	}
	format_4 = format_4 - (2 * format_4);
	t_current = (format_1) + format_4;	/* unit = 0.001 degress */
/* mtktspmic_dprintk("[mtktspmic_get_hw_temp] T_PMIC=%d\n",t_current); */
	return t_current;
}

/* extern void pmic_thermal_dump_reg(void); */

/* int ts_pmic_at_boot_time=0; */
static DEFINE_MUTEX(TSPMIC_lock);
static int pre_temp1 = 0, PMIC_counter;
static int mtktspmic_get_hw_temp(void)
{
	int temp = 0, temp1 = 0;

	mutex_lock(&TSPMIC_lock);

	temp = PMIC_IMM_GetOneChannelValue(4, y_pmic_repeat_times, 2);
	temp1 = thermal_cal_exec(temp);

	mtktspmic_dprintk("[mtktspmic_get_hw_temp] PMIC_IMM_GetOneChannel 4=%d, T=%d\n", temp,
			  temp1);

/* pmic_thermal_dump_reg(); // test */

	if ((temp1 > 100000) || (temp1 < -30000)) {
		pr_info("[Power/PMIC_Thermal] raw=%d, PMIC T=%d", temp, temp1);
/* pmic_thermal_dump_reg(); */
	}

	if ((temp1 > 150000) || (temp1 < -50000)) {
		pr_info("[Power/PMIC_Thermal] drop this data\n");
		temp1 = pre_temp1;
	} else if ((PMIC_counter != 0)
		   && (((pre_temp1 - temp1) > 30000) || ((temp1 - pre_temp1) > 30000))) {
		pr_info("[Power/PMIC_Thermal] drop this data 2\n");
		temp1 = pre_temp1;
	} else {
		/* update previous temp */
		pre_temp1 = temp1;
		mtktspmic_dprintk("[Power/PMIC_Thermal] pre_temp1=%d\n", pre_temp1);

		if (PMIC_counter == 0)
			PMIC_counter++;
	}

	mutex_unlock(&TSPMIC_lock);
	return temp1;
}

static int mtktspmic_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	*t = mtktspmic_get_hw_temp();
	return 0;
}

static int mtktspmic_bind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtktspmic_dprintk("[mtktspmic_bind] error binding cooling dev\n");
		return -EINVAL;
	} else {
		mtktspmic_dprintk("[mtktspmic_bind] binding OK, %d\n", table_val);
	}

	return 0;
}

static int mtktspmic_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtktspmic_dprintk("[mtktspmic_unbind] error unbinding cooling dev\n");
		return -EINVAL;
	} else {
		mtktspmic_dprintk("[mtktspmic_unbind] unbinding OK\n");
	}

	return 0;
}

static int mtktspmic_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtktspmic_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktspmic_get_trip_type(struct thermal_zone_device *thermal, int trip,
				   enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktspmic_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				   unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktspmic_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *temperature)
{
	*temperature = mtktspmic_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktspmic_dev_ops = {
	.bind = mtktspmic_bind,
	.unbind = mtktspmic_unbind,
	.get_temp = mtktspmic_get_temp,
	.get_mode = mtktspmic_get_mode,
	.set_mode = mtktspmic_set_mode,
	.get_trip_type = mtktspmic_get_trip_type,
	.get_trip_temp = mtktspmic_get_trip_temp,
	.get_crit_temp = mtktspmic_get_crit_temp,
};

static int sysrst_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int sysrst_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_sysrst_state;
	return 0;
}

static int sysrst_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_sysrst_state = state;
	if (cl_dev_sysrst_state == 1) {
		pr_emerg("Power/PMIC_Thermal: reset, reset, reset!!!");
		pr_emerg("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		pr_emerg("*****************************************");
		pr_emerg("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
#ifdef CONFIG_AMAZON_METRICS_LOG
		snprintf(mBuf, THERMO_METRICS_STR_LEN,
			 "%s;thermal_caught_shutdown=1;CT;1:NR",
			 mPrefix);
		log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", mBuf);
#endif
		orderly_poweroff(true);
	}
	return 0;
}

static struct thermal_cooling_device_ops mtktspmic_cooling_sysrst_ops = {
	.get_max_state = sysrst_get_max_state,
	.get_cur_state = sysrst_get_cur_state,
	.set_cur_state = sysrst_set_cur_state,
};

static int mtktspmic_show(struct seq_file *s, void *v)
{
	int i;
	seq_puts(s, "[mtktspmic_show]\n");

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

static int mtktspmic_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktspmic_show, NULL);
}

static int mtktspmic_register_thermal(void);
static void mtktspmic_unregister_thermal(void);

static ssize_t mtktspmic_write(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	int len = 0, time_msec = 0;
	int trip[10] = { 0 };
	int t_type[10] = { 0 };
	int i;
	char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
	char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
	char desc[512];


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf
	    (desc,
	     "%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
	     &num_trip, &trip[0], &t_type[0], bind0, &trip[1], &t_type[1], bind1, &trip[2],
	     &t_type[2], bind2, &trip[3], &t_type[3], bind3, &trip[4], &t_type[4], bind4, &trip[5],
	     &t_type[5], bind5, &trip[6], &t_type[6], bind6, &trip[7], &t_type[7], bind7, &trip[8],
	     &t_type[8], bind8, &trip[9], &t_type[9], bind9, &time_msec) == 32) {

		if (num_trip < 1 || num_trip > 10) {
			mtktspmic_dprintk("[mtktspmic_write] bad argument: num_trip=%d\n", num_trip);
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

		mtktspmic_dprintk("[mtktspmic_write] ");
		for (i = 0; i < 10; i++)
			mtktspmic_dprintk("g_THERMAL_TRIP_%d=%d,", i, g_THERMAL_TRIP[i]);
		mtktspmic_dprintk("\n");

		mtktspmic_dprintk("[mtktspmic_write] ");
		mtktspmic_dprintk("cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
				  g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
		mtktspmic_dprintk("cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
				  g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = trip[i];

		interval = time_msec;

		mtktspmic_dprintk("[mtktspmic_write] ");
		for (i = 0; i < 10; i++)
			mtktspmic_dprintk("trip_%d_temp=%d,", i, trip_temp[i]);
		mtktspmic_dprintk("time_ms=%d\n", interval);

		mtktspmic_dprintk("[mtktspmic_write] mtktspmic_register_thermal\n");
		if (thz_dev) {
			thz_dev->polling_delay = time_msec;
			schedule_work(&twork);
		}

		return count;
	} else {
		mtktspmic_dprintk("[mtktspmic_write] bad argument\n");
	}

	return -EINVAL;
}

int mtktspmic_register_cooler(void)
{
	cl_dev_sysrst = mtk_thermal_cooling_device_register("mtktspmic-sysrst", NULL,
							    &mtktspmic_cooling_sysrst_ops);
	return 0;
}

static int mtktspmic_register_thermal(void)
{
	mtktspmic_dprintk("[mtktspmic_register_thermal]\n");

	/* trips : trip 0~2 */
	thz_dev = mtk_thermal_zone_device_register("mtktspmic", num_trip, NULL,
						   &mtktspmic_dev_ops, 0, 0, 0, interval);

	return 0;
}

void mtktspmic_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

static void mtktspmic_unregister_thermal(void)
{
	mtktspmic_dprintk("[mtktspmic_unregister_thermal]\n");

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static const struct file_operations mtktspmic_fops = {
	.owner = THIS_MODULE,
	.write = mtktspmic_write,
	.open = mtktspmic_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void mtktspmic_work(struct work_struct *work)
{
	if (thz_dev)
		thermal_zone_device_update(thz_dev);
}

static int __init mtktspmic_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktspmic_dir = NULL;

	mtktspmic_dprintk("[mtktspmic_init]\n");
	pmic_cali_prepare();


	err = mtktspmic_register_cooler();
	if (err)
		return err;
	err = mtktspmic_register_thermal();
	if (err)
		goto err_unreg;

	mtktspmic_dir = proc_mkdir("mtktspmic", NULL);
	if (!mtktspmic_dir) {
		mtktspmic_dprintk("[mtktspmic_init]: mkdir /proc/mtktspmic failed\n");
	} else {
		entry = proc_create("mtktspmic", S_IRUGO | S_IWUSR, mtktspmic_dir, &mtktspmic_fops);
		if (!entry) {
			mtktspmic_dprintk
			    ("[mtktspmic_init]: create /proc/mtktspmic/mtktspmic failed\n");
		}
	}

	INIT_WORK(&twork, mtktspmic_work);
	return 0;

 err_unreg:
	mtktspmic_unregister_cooler();
	return err;
}

static void __exit mtktspmic_exit(void)
{
	mtktspmic_dprintk("[mtktspmic_exit]\n");
	cancel_work_sync(&twork);
	mtktspmic_unregister_thermal();
	mtktspmic_unregister_cooler();
}
module_init(mtktspmic_init);
module_exit(mtktspmic_exit);

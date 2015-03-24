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
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <asm/uaccess.h>
#include <asm/string.h>
#include <linux/spinlock.h>
#include <linux/reboot.h>

#include <mach/system.h>

#include "mach/mtk_thermal_monitor.h"
#include "mach/mt_typedefs.h"
#include "mach/mt_thermal.h"
#include "mach/mtk_mdm_monitor.h"

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#ifndef THERMO_METRICS_STR_LEN
#define THERMO_METRICS_STR_LEN 128
#endif
static char *mPrefix = "pa_thermal_zone:def:monitor=1";
static char mBuf[THERMO_METRICS_STR_LEN];
#endif

static unsigned int interval = 10000;	/* mseconds, 0 : no auto polling */
static unsigned int trip_temp[10] = { 120000, 80000, 70000, 60000, 50000, 40000, 30000, 20000, 10000, 5000 };
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static unsigned int cl_dev_sysrst_state;
static struct thermal_zone_device *thz_dev;
static struct thermal_cooling_device *cl_dev_sysrst;
static int mtktspa_debug_log;
static int kernelmode;
static struct work_struct twork;

static int num_trip = 1;
static char g_bind0[20] = "mtktspa-sysrst";
static char g_bind1[20] = { 0 };
static char g_bind2[20] = { 0 };
static char g_bind3[20] = { 0 };
static char g_bind4[20] = { 0 };
static char g_bind5[20] = { 0 };
static char g_bind6[20] = { 0 };
static char g_bind7[20] = { 0 };
static char g_bind8[20] = { 0 };
static char g_bind9[20] = { 0 };

#define mtktspa_TEMP_CRIT 85000	/* 85.000 degree Celsius */

#define mtktspa_dprintk(fmt, args...)   \
do {                                    \
	if (mtktspa_debug_log) {                \
		xlog_printk(ANDROID_LOG_INFO, "Power/PA_Thermal", fmt, ##args); \
	}                                   \
} while (0)


/*
struct md_info{
		char *attribute;
		int value;
		char *unit;
		int invalid_value;
		int index;
};
struct md_info g_pinfo_list[] =
{{"TXPWR_MD1", -127, "db", -127, 0},
 {"TXPWR_MD2", -127, "db", -127, 1},
 {"RFTEMP_2G_MD1", -32767, "¢XC", -32767, 2},
 {"RFTEMP_2G_MD2", -32767, "¢XC", -32767, 3},
 {"RFTEMP_3G_MD1", -32767, "¢XC", -32767, 4},
 {"RFTEMP_3G_MD2", -32767, "¢XC", -32767, 5}};
*/
static DEFINE_MUTEX(TSPA_lock);
static int mtktspa_get_hw_temp(void)
{
	struct md_info *p_info;
	int size, i;

	mutex_lock(&TSPA_lock);
	mtk_mdm_get_md_info(&p_info, &size);
	for (i = 0; i < size; i++) {
		mtktspa_dprintk("PA temperature: name:%s, vaule:%d, invalid_value=%d\n",
				p_info[i].attribute, p_info[i].value, p_info[i].invalid_value);
		if (!strcmp(p_info[i].attribute, "RFTEMP_2G_MD1")) {
			mtktspa_dprintk("PA temperature: RFTEMP_2G_MD1\n");
			if (p_info[i].value != p_info[i].invalid_value)
				break;
		} else if (!strcmp(p_info[i].attribute, "RFTEMP_3G_MD1")) {
			mtktspa_dprintk("PA temperature: RFTEMP_3G_MD1\n");
			if (p_info[i].value != p_info[i].invalid_value)
				break;
		}
	}

	if (i == size) {
		mtktspa_dprintk("PA temperature: not ready\n");
		mutex_unlock(&TSPA_lock);
		return -127000;
	} else {
		mtktspa_dprintk("PA temperature: %d\n", p_info[i].value);

		if ((p_info[i].value > 100) | (p_info[i].value < -30))
			pr_info("[Power/PA_Thermal] PA T=%d\n", p_info[i].value);
		mutex_unlock(&TSPA_lock);
		return p_info[i].value;
	}

}

static int mtktspa_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	*t = mtktspa_get_hw_temp();
	return 0;
}

static int mtktspa_bind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else
		return 0;


	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtktspa_dprintk("[mtktspa_bind] error binding cooling dev\n");
		return -EINVAL;
	} else {
		mtktspa_dprintk("[mtktspa_bind] binding OK\n");
	}

	return 0;
}

static int mtktspa_unbind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtktspa_dprintk("[mtktspa_unbind] error unbinding cooling dev\n");
		return -EINVAL;
	} else {
		mtktspa_dprintk("[mtktspa_unbind] unbinding OK\n");
	}

	return 0;
}

static int mtktspa_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;

	return 0;
}

static int mtktspa_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktspa_get_trip_type(struct thermal_zone_device *thermal, int trip,
				 enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktspa_get_trip_temp(struct thermal_zone_device *thermal, int trip, unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktspa_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *temperature)
{
	*temperature = mtktspa_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktspa_dev_ops = {
	.bind = mtktspa_bind,
	.unbind = mtktspa_unbind,
	.get_temp = mtktspa_get_temp,
	.get_mode = mtktspa_get_mode,
	.set_mode = mtktspa_set_mode,
	.get_trip_type = mtktspa_get_trip_type,
	.get_trip_temp = mtktspa_get_trip_temp,
	.get_crit_temp = mtktspa_get_crit_temp,
};

/*
 * cooling device callback functions (mtktspa_cooling_sysrst_ops)
 * 1 : ON and 0 : OFF
 */
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
		pr_emerg("Power/PA_Thermal: reset, reset, reset!!!");
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

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtktspa_cooling_sysrst_ops = {
	.get_max_state = sysrst_get_max_state,
	.get_cur_state = sysrst_get_cur_state,
	.set_cur_state = sysrst_set_cur_state,
};

static int mtktspa_register_thermal(void);
static void mtktspa_unregister_thermal(void);

static int mtktspa_show(struct seq_file *s, void *v)
{
	int i;
	seq_puts(s, "[mtktspa_show]\n");

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

static int mtktspa_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktspa_show, NULL);
}


static ssize_t mtktspa_write(struct file *file, const char *buffer, size_t count, loff_t *data)
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
			mtktspa_dprintk("[mtktspa_write] bad argument: num_trip=%d\n", num_trip);
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

		mtktspa_dprintk("[mtktsbattery_write] ");
		for (i = 0; i < 10; i++)
			mtktspa_dprintk("g_THERMAL_TRIP_%d=%d,", i, g_THERMAL_TRIP[i]);
		mtktspa_dprintk("\n");

		mtktspa_dprintk("[mtktsbattery_write] ");
		mtktspa_dprintk("cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
				  g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
		mtktspa_dprintk("cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
				  g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = trip[i];

		interval = time_msec;

		mtktspa_dprintk("[mtktspa_write] ");
		for (i = 0; i < 10; i++)
			mtktspa_dprintk("trip_%d_temp=%d,", i, trip_temp[i]);
		mtktspa_dprintk("time_ms=%d\n", interval);

		mtktspa_dprintk("[mtktspa_write] mtktspa_register_thermal\n");
		if (thz_dev) {
			thz_dev->polling_delay = time_msec;
			schedule_work(&twork);
		}
		return count;
	} else {
		mtktspa_dprintk("[mtktspa_write] bad argument\n");
	}

	return -EINVAL;

}

int mtktspa_register_cooler(void)
{
	/* cooling devices */
	cl_dev_sysrst = mtk_thermal_cooling_device_register("mtktspa-sysrst", NULL,
							    &mtktspa_cooling_sysrst_ops);
	return 0;
}

static int mtktspa_register_thermal(void)
{
	mtktspa_dprintk("[mtktspa_register_thermal]\n");

	/* trips */
	thz_dev = mtk_thermal_zone_device_register("mtktspa", num_trip, NULL,
						   &mtktspa_dev_ops, 0, 0, 0, interval);

	return 0;
}

void mtktspa_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

static void mtktspa_unregister_thermal(void)
{
	mtktspa_dprintk("[mtktspa_unregister_thermal]\n");

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static const struct file_operations mtktspa_fops = {
	.owner = THIS_MODULE,
	.write = mtktspa_write,
	.open = mtktspa_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void mtktspa_work(struct work_struct *work)
{
	if (thz_dev)
		thermal_zone_device_update(thz_dev);
}

static int __init mtktspa_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktspa_dir = NULL;

	mtktspa_dprintk("[mtktspa_init]\n");

	err = mtktspa_register_cooler();
	if (err)
		return err;

	err = mtktspa_register_thermal();
	if (err)
		goto err_unreg;

	mtktspa_dir = proc_mkdir("mtktspa", NULL);
	if (!mtktspa_dir) {
		mtktspa_dprintk("[mtktspa_init]: mkdir /proc/mtktspa failed\n");
	} else {
		entry = proc_create("mtktspa", S_IRUGO | S_IWUSR, mtktspa_dir, &mtktspa_fops);
		if (!entry)
			mtktspa_dprintk("[mtktspa_init]: create /proc/mtktspa/mtktspa failed\n");
	}

	INIT_WORK(&twork, mtktspa_work);
	return 0;

 err_unreg:
	mtktspa_unregister_cooler();
	return err;
}

static void __exit mtktspa_exit(void)
{
	mtktspa_dprintk("[mtktspa_exit]\n");
	cancel_work_sync(&twork);
	mtktspa_unregister_thermal();
	mtktspa_unregister_cooler();
}
module_init(mtktspa_init);
module_exit(mtktspa_exit);

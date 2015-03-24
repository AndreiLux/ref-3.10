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
#include <linux/reboot.h>
#include <asm/uaccess.h>

#include <mach/system.h>
#include "mach/mtk_thermal_monitor.h"
#include "mach/mt_typedefs.h"
#include "mach/mt_thermal.h"
#include "mach/battery_custom_data.h"
#include "mach/battery_common.h"
#include "linux/thermal_framework.h"

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#ifndef THERMO_METRICS_STR_LEN
#define THERMO_METRICS_STR_LEN 128
#endif
static char *mPrefix = "battery_thermal_zone:def:monitor=1";
static char mBuf[THERMO_METRICS_STR_LEN];
#endif

static unsigned int interval = 1000;	/* mseconds, 0 : no auto polling */
static unsigned int trip_temp[10] = { 62000, 110000, 100000, 90000, 80000, 70000, 65000, 60000, 55000, 50000 };
/* static unsigned int cl_dev_dis_charge_state = 0; */
static unsigned int cl_dev_sysrst_state;
static struct thermal_zone_device *thz_dev;
/* static struct thermal_cooling_device *cl_dev_dis_charge; */
static struct thermal_cooling_device *cl_dev_sysrst;
static int mtktsbattery_debug_log;
static int kernelmode;
static struct work_struct twork;
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip = 1;
static char g_bind0[20] = "mtktsbattery-sysrst";
static char g_bind1[20] = { 0 };
static char g_bind2[20] = { 0 };
static char g_bind3[20] = { 0 };
static char g_bind4[20] = { 0 };
static char g_bind5[20] = { 0 };
static char g_bind6[20] = { 0 };
static char g_bind7[20] = { 0 };
static char g_bind8[20] = { 0 };
static char g_bind9[20] = { 0 };

/* static int battery_write_flag=0; */

#define mtktsbattery_TEMP_CRIT 60000	/* 60.000 degree Celsius */

#define mtktsbattery_dprintk(fmt, args...)   \
do {                                    \
	if (mtktsbattery_debug_log) {                \
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery_Thermal", fmt, ##args); \
	}                                   \
} while (0)

/*
 * kernel fopen/fclose
 */
/*
static mm_segment_t oldfs;

static void my_close(int fd)
{
	set_fs(oldfs);
	sys_close(fd);
}

static int my_open(char *fname, int flag)
{
	oldfs = get_fs();
    set_fs(KERNEL_DS);
    return sys_open(fname, flag, 0);
}
*/
static int get_hw_battery_temp(void)
{
/*
	int fd;
    char buf[64];
    char *pmtdbufp = NULL;
    ssize_t pmtdsize;

    char *pvalue = NULL;
    int got_value=0;

    //open file and read current value
    fd = my_open("/sys/class/power_supply/battery/batt_temp", O_RDONLY);
    if (fd < 0)
    {
	mtktsbattery_dprintk("[get_hw_battery_temp]: open file fail");
	return 0;
    }
    mtktsbattery_dprintk("[get_hw_battery_temp]: open file ok");
    buf[sizeof(buf) - 1] = '\0';
    pmtdsize = sys_read(fd, buf, sizeof(buf) - 1);
    pmtdbufp = buf;
    got_value = simple_strtol(pmtdbufp,&pvalue,10);

    // close file
    my_close(fd);

    // debug
    mtktsbattery_dprintk("[get_hw_battery_temp]: got_value=%d\n", got_value);

    return got_value;
*/
	int ret = 0;
#if defined(CONFIG_POWER_EXT)
	/* EVB */
	ret = -1270;
#else
	/* Phone */
	ret = read_tbat_value();
	ret = ret * 10;
#endif

	return ret;
}

static DEFINE_MUTEX(Battery_lock);
int ts_battery_at_boot_time = 0;
static int mtktsbattery_get_hw_temp(void)
{
	int t_ret = 0;
	static int battery[60] = { 0 };
	static int counter = 0, first_time;
	int i = 0;

	if (ts_battery_at_boot_time == 0) {
		ts_battery_at_boot_time = 1;
		mtktsbattery_dprintk
		    ("[mtktsbattery_get_hw_temp] at boot time, return 25000 as default\n");
		battery[counter] = 25000;
		counter++;
		return 25000;
	}

	mutex_lock(&Battery_lock);

	/* get HW battery temp (TSBATTERY) */
	/* cat /sys/class/power_supply/battery/batt_temp */
	t_ret = get_hw_battery_temp();
	t_ret = t_ret * 100;
	battery[counter] = t_ret;

	if (first_time != 0) {
		t_ret = 0;
		for (i = 0; i < MA_len; i++)
			t_ret += battery[i];
		t_ret = t_ret / MA_len;
	}

	counter++;
	if (counter == MA_len) {
		counter = 0;
		first_time = 1;
	}
	if (counter > MA_len) {
		counter = 0;
		first_time = 0;
	}
	mutex_unlock(&Battery_lock);

	if (t_ret)

		mtktsbattery_dprintk
		    ("[mtktsbattery_get_hw_temp] counter=%d, first_time =%d, MA_len=%d\n", counter,
		     first_time, MA_len);
	mtktsbattery_dprintk("[mtktsbattery_get_hw_temp] T_Battery, %d\n", t_ret);
	return t_ret;
}

static int mtktsbattery_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	*t = mtktsbattery_get_hw_temp();
	return 0;
}

static int mtktsbattery_bind(struct thermal_zone_device *thermal,
			     struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktsbattery_dprintk("[mtktsbattery_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktsbattery_dprintk("[mtktsbattery_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktsbattery_dprintk("[mtktsbattery_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktsbattery_dprintk("[mtktsbattery_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktsbattery_dprintk("[mtktsbattery_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktsbattery_dprintk("[mtktsbattery_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktsbattery_dprintk("[mtktsbattery_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktsbattery_dprintk("[mtktsbattery_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktsbattery_dprintk("[mtktsbattery_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktsbattery_dprintk("[mtktsbattery_bind] %s\n", cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtktsbattery_dprintk("[mtktsbattery_bind] error binding cooling dev\n");
		return -EINVAL;
	} else {
		mtktsbattery_dprintk("[mtktsbattery_bind] binding OK, %d\n", table_val);
	}

	return 0;
}

static int mtktsbattery_unbind(struct thermal_zone_device *thermal,
			       struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktsbattery_dprintk("[mtktsbattery_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktsbattery_dprintk("[mtktsbattery_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktsbattery_dprintk("[mtktsbattery_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktsbattery_dprintk("[mtktsbattery_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktsbattery_dprintk("[mtktsbattery_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktsbattery_dprintk("[mtktsbattery_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktsbattery_dprintk("[mtktsbattery_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktsbattery_dprintk("[mtktsbattery_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktsbattery_dprintk("[mtktsbattery_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktsbattery_dprintk("[mtktsbattery_unbind] %s\n", cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtktsbattery_dprintk("[mtktsbattery_unbind] error unbinding cooling dev\n");
		return -EINVAL;
	} else {
		mtktsbattery_dprintk("[mtktsbattery_unbind] unbinding OK\n");
	}

	return 0;
}

static int mtktsbattery_get_mode(struct thermal_zone_device *thermal,
				 enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtktsbattery_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktsbattery_get_trip_type(struct thermal_zone_device *thermal, int trip,
				      enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktsbattery_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				      unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktsbattery_get_crit_temp(struct thermal_zone_device *thermal,
				      unsigned long *temperature)
{
	*temperature = mtktsbattery_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktsbattery_dev_ops = {
	.bind = mtktsbattery_bind,
	.unbind = mtktsbattery_unbind,
	.get_temp = mtktsbattery_get_temp,
	.get_mode = mtktsbattery_get_mode,
	.set_mode = mtktsbattery_set_mode,
	.get_trip_type = mtktsbattery_get_trip_type,
	.get_trip_temp = mtktsbattery_get_trip_temp,
	.get_crit_temp = mtktsbattery_get_crit_temp,
};

/*
static int dis_charge_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
		*state = 1;
		return 0;
}
static int dis_charge_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
		*state = cl_dev_dis_charge_state;
		return 0;
}
static int dis_charge_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
    cl_dev_dis_charge_state = state;
    if(cl_dev_dis_charge_state == 1) {
	mtktsbattery_dprintk("[dis_charge_set_cur_state] disable charging\n");
    }
    return 0;
}
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
		pr_emerg("Power/battery_Thermal: reset, reset, reset!!!");
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

/*
static struct thermal_cooling_device_ops mtktsbattery_cooling_dis_charge_ops = {
	.get_max_state = dis_charge_get_max_state,
	.get_cur_state = dis_charge_get_cur_state,
	.set_cur_state = dis_charge_set_cur_state,
};*/
static struct thermal_cooling_device_ops mtktsbattery_cooling_sysrst_ops = {
	.get_max_state = sysrst_get_max_state,
	.get_cur_state = sysrst_get_cur_state,
	.set_cur_state = sysrst_set_cur_state,
};

static int mtktsbattery_show(struct seq_file *s, void *v)
{
	int i;
	seq_puts(s, "[mtktsbattery_show]\n");

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

static int mtktsbattery_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktsbattery_show, NULL);
}

static int mtktsbattery_register_thermal(void);
static void mtktsbattery_unregister_thermal(void);

static ssize_t mtktsbattery_write(struct file *file, const char *buffer, size_t count,
				  loff_t *data)
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
			mtktsbattery_dprintk("[mtktsbattery_write] bad argument: num_trip=%d\n", num_trip);
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

		mtktsbattery_dprintk("[mtktsbattery_write] ");
		for (i = 0; i < 10; i++)
			mtktsbattery_dprintk("g_THERMAL_TRIP_%d=%d,", i, g_THERMAL_TRIP[i]);
		mtktsbattery_dprintk("\n");

		mtktsbattery_dprintk("[mtktsbattery_write] ");
		mtktsbattery_dprintk("cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
				  g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
		mtktsbattery_dprintk("cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
				  g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = trip[i];

		interval = time_msec;

		mtktsbattery_dprintk("[mtktsbattery_write] ");
		for (i = 0; i < 10; i++)
			mtktsbattery_dprintk("trip_%d_temp=%d,", i, trip_temp[i]);
		mtktsbattery_dprintk("time_ms=%d\n", interval);

		mtktsbattery_dprintk("[mtktsbattery_write] mtktsbattery_register_thermal\n");
		if (thz_dev) {
			thz_dev->polling_delay = time_msec;
			schedule_work(&twork);
		}
		/* battery_write_flag=1; */
		return count;
	} else {
		mtktsbattery_dprintk("[mtktsbattery_write] bad argument\n");
	}

	return -EINVAL;
}


int mtktsbattery_register_cooler(void)
{
	/* cooling devices */
	cl_dev_sysrst = mtk_thermal_cooling_device_register("mtktsbattery-sysrst", NULL,
							    &mtktsbattery_cooling_sysrst_ops);
	return 0;
}

static int mtktsbattery_register_thermal(void)
{
	mtktsbattery_dprintk("[mtktsbattery_register_thermal]\n");

	/* trips : trip 0~1 */
	thz_dev = mtk_thermal_zone_device_register("mtktsbattery", num_trip, NULL,
						   &mtktsbattery_dev_ops, 0, 0, 0, interval);

	return 0;
}

void mtktsbattery_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

static void mtktsbattery_unregister_thermal(void)
{
	mtktsbattery_dprintk("[mtktsbattery_unregister_thermal]\n");

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static const struct file_operations mtktsbattery_fops = {
	.owner = THIS_MODULE,
	.write = mtktsbattery_write,
	.open = mtktsbattery_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void mtktsbattery_work(struct work_struct *work)
{
	if (thz_dev)
		thermal_zone_device_update(thz_dev);
}

static int __init mtktsbattery_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktsbattery_dir = NULL;

	mtktsbattery_dprintk("[mtktsbattery_init]\n");

	err = mtktsbattery_register_cooler();
	if (err)
		return err;

	err = mtktsbattery_register_thermal();
	if (err)
		goto err_unreg;

	mtktsbattery_dir = proc_mkdir("mtktsbattery", NULL);
	if (!mtktsbattery_dir) {
		mtktsbattery_dprintk("[mtktsbattery_init]: mkdir /proc/mtktsbattery failed\n");
	} else {
		entry =
		    proc_create("mtktsbattery", S_IRUGO | S_IWUSR, mtktsbattery_dir,
				&mtktsbattery_fops);
		if (!entry) {
			mtktsbattery_dprintk
			    ("[mtktsbattery_init]: create /proc/mtktsbattery/mtktsbattery failed\n");
		}
	}

	INIT_WORK(&twork, mtktsbattery_work);
	return 0;

 err_unreg:
	mtktsbattery_unregister_cooler();
	return err;
}

static void __exit mtktsbattery_exit(void)
{
	mtktsbattery_dprintk("[mtktsbattery_exit]\n");
	cancel_work_sync(&twork);
	mtktsbattery_unregister_thermal();
	mtktsbattery_unregister_cooler();
}
module_init(mtktsbattery_init);
module_exit(mtktsbattery_exit);

/*****************************************************************************
 *
 * Filename:
 * ---------
 *    battery_common.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of mt6323 Battery charging algorithm
 *   and the Anroid Battery service for updating the battery status
 *
 * Author:
 * -------
 * Oscar Liu
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
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/xlog.h>
#include <linux/seq_file.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <mach/hardware.h>
#include <mach/system.h>
#include <mach/mt_sleep.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpt.h>
#include <mach/mt_boot.h>
#include <mach/pmic_sw.h>

/* #include <mach/upmu_common.h> */
#include <mach/upmu_hw.h>
#include <mach/charging.h>
#include <mach/battery_custom_data.h>
#include "battery_charging_custom_wrap.h"
#include <mach/battery_common.h>
#include <mach/battery_meter.h>
#include <mach/mt_boot.h>
#include <misc/charger_notifier.h>

struct battery_common_data g_bat;

#if defined(CONFIG_AMAZON_METRICS_LOG)

#if defined(CONFIG_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#include <linux/metricslog.h>

#include <linux/irq.h>
#include <mach/mt_pmic_irq.h>
#include <linux/reboot.h>

extern kal_int32 g_fg_dbg_bat_qmax;

enum BQ_FLAGS {
	BQ_STATUS_RESUMING = 0x2,
};

struct battery_info {
	struct mutex lock;

	int flags;

	/* Time when system enters full suspend */
	struct timespec suspend_time;
	/* Time when system enters early suspend */
	struct timespec early_suspend_time;
	/* Battery capacity when system enters full suspend */
	int suspend_capacity;
	/* Battery capacity, relative and high-precision, when system enters full suspend */
	int suspend_bat_car;
	/* Battery capacity when system enters early suspend */
	int early_suspend_capacity;
#if defined(CONFIG_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
};

struct battery_info BQ_info;
#endif

/* Battery Notify */
#define BATTERY_NOTIFY_CASE_0001_VCHARGER
#define BATTERY_NOTIFY_CASE_0002_VBATTEMP
/* #define BATTERY_NOTIFY_CASE_0003_ICHARGING */
#define BATTERY_NOTIFY_CASE_0004_VBAT
#define BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME

/* Precise Tunning */
#define BATTERY_AVERAGE_DATA_NUMBER	3
#define BATTERY_AVERAGE_SIZE	30

#define CUST_CAPACITY_OCV2CV_TRANSFORM

/* ////////////////////////////////////////////////////////////////////////////// */
/* Battery Logging Entry */
/* ////////////////////////////////////////////////////////////////////////////// */
int Enable_BATDRV_LOG = BAT_LOG_ERROR;
/* static struct proc_dir_entry *proc_entry; */
char proc_bat_data[32];

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Smart Battery Structure */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
PMU_ChargerStruct BMT_status;


/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Thermal related flags */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* 0:nothing, 1:enable batTT&chrTimer, 2:disable batTT&chrTimer, 3:enable batTT, disable chrTimer */
int g_battery_thermal_throttling_flag = 1;
int battery_cmd_thermal_test_mode = 0;
int battery_cmd_thermal_test_mode_value = 0;
int g_battery_tt_check_flag = 0;	/* 0:default enable check batteryTT, 1:default disable check batteryTT */


/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Global Variable */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
struct wake_lock battery_suspend_lock;
unsigned int g_BatteryNotifyCode = 0x0000;
unsigned int g_BN_TestMode = 0x0000;
kal_bool g_bat_init_flag = 0;
kal_bool g_call_state = CALL_IDLE;
kal_bool g_charging_full_reset_bat_meter = KAL_FALSE;
int g_platform_boot_mode = 0;
static kal_bool battery_meter_initilized = KAL_FALSE;
kal_bool g_cmd_hold_charging = KAL_FALSE;
kal_int32 g_custom_charging_current = -1;
kal_bool battery_suspended = KAL_FALSE;
kal_bool g_refresh_ui_soc = KAL_FALSE;
static bool fg_battery_shutdown;
struct mt_battery_charging_custom_data *p_bat_charging_data;

/* ////////////////////////////////////////////////////////////////////////////// */
/* Integrate with NVRAM */
/* ////////////////////////////////////////////////////////////////////////////// */
#define ADC_CALI_DEVNAME "MT_pmic_adc_cali"
#define TEST_ADC_CALI_PRINT _IO('k', 0)
#define SET_ADC_CALI_Slop _IOW('k', 1, int)
#define SET_ADC_CALI_Offset _IOW('k', 2, int)
#define SET_ADC_CALI_Cal _IOW('k', 3, int)
#define ADC_CHANNEL_READ _IOW('k', 4, int)
#define BAT_STATUS_READ _IOW('k', 5, int)
#define Set_Charger_Current _IOW('k', 6, int)
/* add for meta tool----------------------------------------- */
#define Get_META_BAT_VOL _IOW('k', 10, int)
#define Get_META_BAT_SOC _IOW('k', 11, int)
/* add for meta tool----------------------------------------- */

static struct class *adc_cali_class;
static int adc_cali_major;
static dev_t adc_cali_devno;
static struct cdev *adc_cali_cdev;

int adc_cali_slop[14] = {
	1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000
	};
int adc_cali_offset[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int adc_cali_cal[1] = { 0 };
int battery_in_data[1] = { 0 };
int battery_out_data[1] = { 0 };
int charging_level_data[1] = { 0 };

kal_bool g_ADC_Cali = KAL_FALSE;
kal_bool g_ftm_battery_flag = KAL_FALSE;
static kal_bool need_clear_current_window = KAL_FALSE;

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Thread related */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
#define BAT_MS_TO_NS(x) (x * 1000 * 1000)
static atomic_t bat_thread_wakeup;
static atomic_t charger_hv_detect;
static kal_bool chr_wake_up_bat = KAL_FALSE;	/* charger in/out to wake up battery thread */
static kal_bool bat_meter_timeout = KAL_FALSE;
static DEFINE_MUTEX(bat_mutex);
static DEFINE_MUTEX(charger_type_mutex);
static DECLARE_WAIT_QUEUE_HEAD(bat_thread_wq);
static struct task_struct *charger_hv_detect_thread;
static DECLARE_WAIT_QUEUE_HEAD(charger_hv_detect_waiter);

#if defined(CONFIG_AMAZON_METRICS_LOG)
void metrics_charger(bool connect)
{
	int cap = BMT_status.UI_SOC;
	char buf[128];
	if (connect == true) {
		snprintf(buf, sizeof(buf),
			"bq24297:def:POWER_STATUS_CHARGING=1;CT;1,"
			"cap=%u;CT;1,mv=%d;CT;1,current_avg=%d;CT;1:NR",
			cap, BMT_status.bat_vol, BMT_status.ICharging);

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
	} else {
		snprintf(buf, sizeof(buf),
			"bq24297:def:POWER_STATUS_DISCHARGING=1;CT;1,"
			"cap=%u;CT;1,mv=%d;CT;1,current_avg=%d;CT;1:NR",
			cap, BMT_status.bat_vol, BMT_status.ICharging);

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
	}
}
void metrics_charger_update(int ac, int usb)
{
	static bool ischargeronline;
	int onceonline = 0;

	if (ac == 1)
		onceonline = 1;
	else if (usb == 1)
		onceonline = 1;
	if (ischargeronline != onceonline) {
		ischargeronline = onceonline;
		metrics_charger(ischargeronline);
	}
}
#endif


#ifdef CONFIG_AMAZON_METRICS_LOG
static void
battery_critical_voltage_check(void)
{
	if (BMT_status.charger_exist)
		return;
	if (BMT_status.bat_exist != KAL_TRUE)
		return;
	if (BMT_status.UI_SOC != 0)
		return;
	if (BMT_status.bat_vol <= SYSTEM_OFF_VOLTAGE) {
		char buf[128];
		snprintf(buf, sizeof(buf),
			"bq24297:def:critical_shutdown=1;CT;1:HI");

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
	}
}

static void
battery_metrics_locked(struct battery_info *info);

static void metrics_handle(void)
{
	struct battery_info *info = &BQ_info;

	mutex_lock(&info->lock);

	/* Check for critical battery voltage */
	battery_critical_voltage_check();


	if ((info->flags & BQ_STATUS_RESUMING)) {
		info->flags &= ~BQ_STATUS_RESUMING;
		battery_metrics_locked(info);
	}

	mutex_unlock(&info->lock);

	return;
}


#if defined(CONFIG_EARLYSUSPEND)
static void bq_log_metrics(struct battery_info *info, char *msg,
	char *metricsmsg)
{
	int value = BMT_status.UI_SOC;
	struct timespec curr = current_kernel_time();
	/* Compute elapsed time and determine screen off or on drainage */
	struct timespec diff = timespec_sub(curr,
			info->early_suspend_time);

	if (info->early_suspend_capacity != -1) {
		char buf[512];
		snprintf(buf, sizeof(buf),
			"%s:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
			metricsmsg,
			info->early_suspend_capacity - value,
			diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC);
		log_to_metrics(ANDROID_LOG_INFO, "drain_metrics", buf);
	}
	/* Cache the current capacity */
	info->early_suspend_capacity = BMT_status.UI_SOC;
	/* Mark the suspend or resume time */
	info->early_suspend_time = curr;
}

static void battery_early_suspend(struct early_suspend *handler)
{
	struct battery_info *info = &BQ_info;

	bq_log_metrics(info, "Screen on drainage", "screen_on_drain");

}

static void battery_late_resume(struct early_suspend *handler)
{
	struct battery_info *info = &BQ_info;
	bq_log_metrics(info, "Screen off drainage", "screen_off_drain");
}
#endif

static int metrics_init(void)
{
	struct battery_info *info = &BQ_info;

	mutex_init(&info->lock);

	info->suspend_capacity = -1;
	info->early_suspend_capacity = -1;

#if defined(CONFIG_EARLYSUSPEND)
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	info->early_suspend.suspend = battery_early_suspend;
	info->early_suspend.resume = battery_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

	mutex_lock(&info->lock);
	info->flags = 0;
	mutex_unlock(&info->lock);

	return 0;
}
static void metrics_uninit(void)
{
	struct battery_info *info = &BQ_info;
	unregister_early_suspend(&info->early_suspend);
}
static void metrics_suspend(void)
{
	struct battery_info *info = &BQ_info;

	/* Cache the current capacity */
	info->suspend_capacity = BMT_status.UI_SOC;
	info->suspend_bat_car = battery_meter_get_car();

	battery_xlog_printk(BAT_LOG_FULL,
		"setting suspend_bat_car to %d\n", info->suspend_bat_car);

	battery_critical_voltage_check();

	/* Mark the suspend time */
	info->suspend_time = current_kernel_time();
}

#endif
/* ////////////////////////////////////////////////////////////////////////////// */
/* FOR ANDROID BATTERY SERVICE */
/* ////////////////////////////////////////////////////////////////////////////// */
struct ac_data {
	struct power_supply psy;
	int AC_ONLINE;
};

struct usb_data {
	struct power_supply psy;
	int USB_ONLINE;
};

struct battery_data {
	struct power_supply psy;
	int BAT_STATUS;
	int BAT_HEALTH;
	int BAT_PRESENT;
	int BAT_TECHNOLOGY;
	int BAT_CAPACITY;
	/* Add for Battery Service */
	int BAT_VOLTAGE_NOW;
	int BAT_VOLTAGE_AVG;
	int BAT_TEMP;
	/* Add for EM */
	int BAT_TemperatureR;
	int BAT_TempBattVoltage;
	int BAT_InstatVolt;
	int BAT_BatteryAverageCurrent;
	int BAT_BatterySenseVoltage;
	int BAT_ISenseVoltage;
	int BAT_ChargerVoltage;
	/* ACOS_MOD_BEGIN {metrics_log} */
	int old_CAR;    /* as read from hardware */
	int BAT_ChargeCounter;   /* monotonically declining */
	int BAT_ChargeFull;
	int BAT_SuspendDrain;
	int BAT_SuspendDrainHigh;
	int BAT_SuspendRealtime;
	/* ACOS_MOD_END {metrics_log} */
};

static enum power_supply_property ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	/* Add for Battery Service */
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_TEMP,
	/* Add for EM */
	POWER_SUPPLY_PROP_TemperatureR,
	POWER_SUPPLY_PROP_TempBattVoltage,
	POWER_SUPPLY_PROP_InstatVolt,
	POWER_SUPPLY_PROP_BatteryAverageCurrent,
	POWER_SUPPLY_PROP_BatterySenseVoltage,
	POWER_SUPPLY_PROP_ISenseVoltage,
	POWER_SUPPLY_PROP_ChargerVoltage,
	/* ACOS_MOD_BEGIN {metrics_log} */
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_SUSPEND_DRAIN,
	POWER_SUPPLY_PROP_SUSPEND_DRAIN_HIGH,
	POWER_SUPPLY_PROP_SUSPEND_REALTIME,
	/* ACOS_MOD_END {metrics_log} */
};

int read_tbat_value(void)
{
	return BMT_status.temperature;
}

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // PMIC PCHR Related APIs */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
kal_bool upmu_is_chr_det(void)
{
#if defined(CONFIG_POWER_EXT)
	/* return KAL_TRUE; */
	return bat_charger_get_detect_status();
#else
	kal_uint32 tmp32;
	tmp32 = bat_charger_get_detect_status();
	if (tmp32 == 0) {
		return KAL_FALSE;
	} else {
		if (mt_usb_is_device()) {
			battery_xlog_printk(BAT_LOG_FULL,
					    "[upmu_is_chr_det] Charger exist and USB is not host\n");

			return KAL_TRUE;
		} else {
			battery_xlog_printk(BAT_LOG_FULL,
					    "[upmu_is_chr_det] Charger exist but USB is host\n");

			return KAL_FALSE;
		}
	}
#endif
}
EXPORT_SYMBOL(upmu_is_chr_det);

static inline void _do_wake_up_bat_thread(void)
{
	atomic_inc(&bat_thread_wakeup);
	wake_up(&bat_thread_wq);
}

/* for charger plug-in/out */
void wake_up_bat(void)
{
	pr_debug("%s:\n", __func__);
	chr_wake_up_bat = KAL_TRUE;
	_do_wake_up_bat_thread();
}
EXPORT_SYMBOL(wake_up_bat);

/* for meter update */
static void wake_up_bat_update_meter(void)
{
	pr_debug("%s:\n", __func__);
	bat_meter_timeout = KAL_TRUE;
	_do_wake_up_bat_thread();
}

static ssize_t bat_log_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
	if (copy_from_user(&proc_bat_data, buff, len)) {
		battery_xlog_printk(BAT_LOG_FULL, "bat_log_write error.\n");
		return -EFAULT;
	}

	if (proc_bat_data[0] == '1') {
		battery_xlog_printk(BAT_LOG_CRTI, "enable battery driver log system\n");
		Enable_BATDRV_LOG = 1;
	} else if (proc_bat_data[0] == '2') {
		battery_xlog_printk(BAT_LOG_CRTI, "enable battery driver log system:2\n");
		Enable_BATDRV_LOG = 2;
	} else {
		battery_xlog_printk(BAT_LOG_CRTI, "Disable battery driver log system\n");
		Enable_BATDRV_LOG = 0;
	}

	return len;
}

static const struct file_operations bat_proc_fops = {
	.write = bat_log_write,
};

int init_proc_log(void)
{
	int ret = 0;

#if 1
	proc_create("batdrv_log", 0644, NULL, &bat_proc_fops);
	battery_xlog_printk(BAT_LOG_CRTI, "proc_create bat_proc_fops\n");
#else
	proc_entry = create_proc_entry("batdrv_log", 0644, NULL);

	if (proc_entry == NULL) {
		ret = -ENOMEM;
		battery_xlog_printk(BAT_LOG_FULL, "init_proc_log: Couldn't create proc entry\n");
	} else {
		proc_entry->write_proc = bat_log_write;
		battery_xlog_printk(BAT_LOG_CRTI, "init_proc_log loaded.\n");
	}
#endif

	return ret;
}



static int ac_get_property(struct power_supply *psy,
			   enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct ac_data *data = container_of(psy, struct ac_data, psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->AC_ONLINE;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int usb_get_property(struct power_supply *psy,
			    enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct usb_data *data = container_of(psy, struct usb_data, psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
#if defined(CONFIG_POWER_EXT)
		/* #if 0 */
		data->USB_ONLINE = 1;
		val->intval = data->USB_ONLINE;
#else
		val->intval = data->USB_ONLINE;
#endif
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int battery_get_property(struct power_supply *psy,
				enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct battery_data *data = container_of(psy, struct battery_data, psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = data->BAT_STATUS;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = data->BAT_HEALTH;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = data->BAT_PRESENT;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = data->BAT_TECHNOLOGY;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = data->BAT_CAPACITY;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = data->BAT_VOLTAGE_NOW;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = data->BAT_TEMP;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = data->BAT_VOLTAGE_AVG;
		break;
	case POWER_SUPPLY_PROP_TemperatureR:
		val->intval = data->BAT_TemperatureR;
		break;
	case POWER_SUPPLY_PROP_TempBattVoltage:
		val->intval = data->BAT_TempBattVoltage;
		break;
	case POWER_SUPPLY_PROP_InstatVolt:
		val->intval = data->BAT_InstatVolt;
		break;
	case POWER_SUPPLY_PROP_BatteryAverageCurrent:
		val->intval = data->BAT_BatteryAverageCurrent;
		break;
	case POWER_SUPPLY_PROP_BatterySenseVoltage:
		val->intval = data->BAT_BatterySenseVoltage;
		break;
	case POWER_SUPPLY_PROP_ISenseVoltage:
		val->intval = data->BAT_ISenseVoltage;
		break;
	case POWER_SUPPLY_PROP_ChargerVoltage:
		val->intval = data->BAT_ChargerVoltage;
		break;
	/* ACOS_MOD_BEGIN {metrics_log} */
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = data->BAT_ChargeCounter;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = data->BAT_ChargeFull;
		break;
	case POWER_SUPPLY_PROP_SUSPEND_DRAIN:
		val->intval = data->BAT_SuspendDrain;
		break;
	case POWER_SUPPLY_PROP_SUSPEND_DRAIN_HIGH:
		val->intval = data->BAT_SuspendDrainHigh;
		break;
	case POWER_SUPPLY_PROP_SUSPEND_REALTIME:
		val->intval = data->BAT_SuspendRealtime;
		break;
	/* ACOS_MOD_END {metrics_log} */

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}


/* ac_data initialization */
static struct ac_data ac_main = {
	.psy = {
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.properties = ac_props,
		.num_properties = ARRAY_SIZE(ac_props),
		.get_property = ac_get_property,
		},
	.AC_ONLINE = 0,
};

/* usb_data initialization */
static struct usb_data usb_main = {
	.psy = {
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
		.properties = usb_props,
		.num_properties = ARRAY_SIZE(usb_props),
		.get_property = usb_get_property,
		},
	.USB_ONLINE = 0,
};

/* battery_data initialization */
static struct battery_data battery_main = {
	.psy = {
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = battery_props,
		.num_properties = ARRAY_SIZE(battery_props),
		.get_property = battery_get_property,
		},
/* CC: modify to have a full power supply status */
#if defined(CONFIG_POWER_EXT)
	.BAT_STATUS = POWER_SUPPLY_STATUS_FULL,
	.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD,
	.BAT_PRESENT = 1,
	.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION,
	.BAT_CAPACITY = 100,
	.BAT_VOLTAGE_NOW = 4200000,
	.BAT_VOLTAGE_AVG = 4200000,
	.BAT_TEMP = 22,
#else
	.BAT_STATUS = POWER_SUPPLY_STATUS_NOT_CHARGING,
	.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD,
	.BAT_PRESENT = 1,
	.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION,
	.BAT_CAPACITY = 50,
	.BAT_VOLTAGE_NOW = 0,
	.BAT_VOLTAGE_AVG = 0,
	.BAT_TEMP = 0,
#endif
};

#ifdef CONFIG_AMAZON_METRICS_LOG
extern kal_int32 gFG_aging_factor;
extern kal_int32 gFG_battery_cycle;
extern kal_int32 gFG_columb_sum;
/* must be called with info->lock held */
static void
battery_metrics_locked(struct battery_info *info)
{
	struct timespec diff;

	diff = timespec_sub(current_kernel_time(), info->suspend_time);

	if (info->suspend_capacity != -1) {
		char buf[256];
		int drain_diff;
		int drain_diff_high;

		snprintf(buf, sizeof(buf),
			"suspend_drain:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
			info->suspend_capacity - BMT_status.UI_SOC,
			diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC);
		log_to_metrics(ANDROID_LOG_INFO, "drain_metrics", buf);


		snprintf(buf, sizeof(buf),
			"batt:def:cap=%d;CT;1,mv=%d;CT;1,current_avg=%d;CT;1,"
			"temp_g=%d;CT;1,charge=%d;CT;1,charge_design=%d;CT;1,aging_factor=%d;"
			"CT;1,battery_cycle=%d;CT;1,columb_sum=%d;CT;1:NR",
			BMT_status.UI_SOC, BMT_status.bat_vol,
			BMT_status.ICharging, BMT_status.temperature,
			gFG_BATT_CAPACITY_aging, /*battery_remaining_charge,?*/
			gFG_BATT_CAPACITY, /*battery_remaining_charge_design*/
			gFG_aging_factor, /* aging factor */
			gFG_battery_cycle,
			gFG_columb_sum
			);
		log_to_metrics(ANDROID_LOG_INFO, "bq24297", buf);

		/* These deltas may not always be positive.
		 * BMT_status.UI_SOC may be stale by as much as 10 seconds.
		 */
		drain_diff = info->suspend_capacity - BMT_status.UI_SOC;
		drain_diff_high = (info->suspend_bat_car - battery_meter_get_car())
				 * 10000 / g_fg_dbg_bat_qmax;
		if (battery_main.BAT_STATUS == POWER_SUPPLY_STATUS_DISCHARGING) {
			battery_main.BAT_SuspendDrain += drain_diff;
			battery_main.BAT_SuspendDrainHigh += drain_diff_high;
			battery_main.BAT_SuspendRealtime += diff.tv_sec;
			battery_xlog_printk(BAT_LOG_FULL,
				"Added drain_diff_high(%d) to BAT_SuspendDrainHigh(%d)\n",
				drain_diff_high, battery_main.BAT_SuspendDrainHigh);
		}
	}
}
#endif

#if !defined(CONFIG_POWER_EXT)
/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Charger_Voltage */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Charger_Voltage(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] show_ADC_Charger_Voltage : %d\n",
			    BMT_status.charger_vol);
	return sprintf(buf, "%d\n", BMT_status.charger_vol);
}

static ssize_t store_ADC_Charger_Voltage(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Charger_Voltage, 0664, show_ADC_Charger_Voltage, store_ADC_Charger_Voltage);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_0_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_0_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_slop + 0));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_0_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_0_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_0_Slope, 0664, show_ADC_Channel_0_Slope, store_ADC_Channel_0_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_1_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_1_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_slop + 1));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_1_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_1_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_1_Slope, 0664, show_ADC_Channel_1_Slope, store_ADC_Channel_1_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_2_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_2_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_slop + 2));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_2_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_2_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_2_Slope, 0664, show_ADC_Channel_2_Slope, store_ADC_Channel_2_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_3_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_3_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_slop + 3));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_3_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_3_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_3_Slope, 0664, show_ADC_Channel_3_Slope, store_ADC_Channel_3_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_4_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_4_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_slop + 4));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_4_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_4_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_4_Slope, 0664, show_ADC_Channel_4_Slope, store_ADC_Channel_4_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_5_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_5_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_slop + 5));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_5_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_5_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_5_Slope, 0664, show_ADC_Channel_5_Slope, store_ADC_Channel_5_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_6_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_6_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_slop + 6));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_6_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_6_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_6_Slope, 0664, show_ADC_Channel_6_Slope, store_ADC_Channel_6_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_7_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_7_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_slop + 7));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_7_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_7_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_7_Slope, 0664, show_ADC_Channel_7_Slope, store_ADC_Channel_7_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_8_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_8_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_slop + 8));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_8_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_8_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_8_Slope, 0664, show_ADC_Channel_8_Slope, store_ADC_Channel_8_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_9_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_9_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_slop + 9));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_9_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_9_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_9_Slope, 0664, show_ADC_Channel_9_Slope, store_ADC_Channel_9_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_10_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_10_Slope(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_slop + 10));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_10_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_10_Slope(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_10_Slope, 0664, show_ADC_Channel_10_Slope,
		   store_ADC_Channel_10_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_11_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_11_Slope(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_slop + 11));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_11_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_11_Slope(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_11_Slope, 0664, show_ADC_Channel_11_Slope,
		   store_ADC_Channel_11_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_12_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_12_Slope(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_slop + 12));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_12_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_12_Slope(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_12_Slope, 0664, show_ADC_Channel_12_Slope,
		   store_ADC_Channel_12_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_13_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_13_Slope(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_slop + 13));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_13_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_13_Slope(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_13_Slope, 0664, show_ADC_Channel_13_Slope,
		   store_ADC_Channel_13_Slope);


/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_0_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_0_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_offset + 0));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_0_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_0_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_0_Offset, 0664, show_ADC_Channel_0_Offset,
		   store_ADC_Channel_0_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_1_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_1_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_offset + 1));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_1_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_1_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_1_Offset, 0664, show_ADC_Channel_1_Offset,
		   store_ADC_Channel_1_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_2_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_2_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_offset + 2));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_2_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_2_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_2_Offset, 0664, show_ADC_Channel_2_Offset,
		   store_ADC_Channel_2_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_3_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_3_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_offset + 3));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_3_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_3_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_3_Offset, 0664, show_ADC_Channel_3_Offset,
		   store_ADC_Channel_3_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_4_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_4_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_offset + 4));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_4_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_4_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_4_Offset, 0664, show_ADC_Channel_4_Offset,
		   store_ADC_Channel_4_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_5_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_5_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_offset + 5));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_5_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_5_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_5_Offset, 0664, show_ADC_Channel_5_Offset,
		   store_ADC_Channel_5_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_6_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_6_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_offset + 6));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_6_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_6_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_6_Offset, 0664, show_ADC_Channel_6_Offset,
		   store_ADC_Channel_6_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_7_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_7_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_offset + 7));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_7_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_7_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_7_Offset, 0664, show_ADC_Channel_7_Offset,
		   store_ADC_Channel_7_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_8_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_8_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_offset + 8));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_8_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_8_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_8_Offset, 0664, show_ADC_Channel_8_Offset,
		   store_ADC_Channel_8_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_9_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_9_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_offset + 9));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_9_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_9_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_9_Offset, 0664, show_ADC_Channel_9_Offset,
		   store_ADC_Channel_9_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_10_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_10_Offset(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_offset + 10));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_10_Offset : %d\n", ret_value);
	return sprintf(buf, "%d\n", ret_value);
}

static ssize_t store_ADC_Channel_10_Offset(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_10_Offset, 0664, show_ADC_Channel_10_Offset,
		   store_ADC_Channel_10_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_11_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_11_Offset(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_offset + 11));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_11_Offset : %d\n", ret_value);
	return sprintf(buf, "%d\n", ret_value);
}

static ssize_t store_ADC_Channel_11_Offset(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_11_Offset, 0664, show_ADC_Channel_11_Offset,
		   store_ADC_Channel_11_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_12_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_12_Offset(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_offset + 12));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_12_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_12_Offset(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_12_Offset, 0664, show_ADC_Channel_12_Offset,
		   store_ADC_Channel_12_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_13_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_13_Offset(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;
	ret_value = (*(adc_cali_offset + 13));
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_13_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_13_Offset(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_13_Offset, 0664, show_ADC_Channel_13_Offset,
		   store_ADC_Channel_13_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_Is_Calibration */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_Is_Calibration(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	int ret_value = 2;
	ret_value = g_ADC_Cali;
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] ADC_Channel_Is_Calibration : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_Is_Calibration(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_Is_Calibration, 0664, show_ADC_Channel_Is_Calibration,
		   store_ADC_Channel_Is_Calibration);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : Power_On_Voltage */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_Power_On_Voltage(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret_value = 1;
	ret_value = 3400;
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Power_On_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_On_Voltage(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Power_On_Voltage, 0664, show_Power_On_Voltage, store_Power_On_Voltage);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : Power_Off_Voltage */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_Power_Off_Voltage(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret_value = 1;
	ret_value = 3400;
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Power_Off_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_Off_Voltage(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Power_Off_Voltage, 0664, show_Power_Off_Voltage, store_Power_Off_Voltage);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : Charger_TopOff_Value */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_Charger_TopOff_Value(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;
	ret_value = 4110;
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Charger_TopOff_Value : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Charger_TopOff_Value(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Charger_TopOff_Value, 0664, show_Charger_TopOff_Value,
		   store_Charger_TopOff_Value);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : FG_Battery_CurrentConsumption */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_FG_Battery_CurrentConsumption(struct device *dev, struct device_attribute *attr,
						  char *buf)
{
	int ret_value = 8888;
	ret_value = battery_meter_get_battery_current();
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] FG_Battery_CurrentConsumption : %d/10 mA\n",
			    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_FG_Battery_CurrentConsumption(struct device *dev,
						   struct device_attribute *attr, const char *buf,
						   size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(FG_Battery_CurrentConsumption, 0664, show_FG_Battery_CurrentConsumption,
		   store_FG_Battery_CurrentConsumption);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : FG_SW_CoulombCounter */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_FG_SW_CoulombCounter(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	kal_int32 ret_value = 7777;
	ret_value = battery_meter_get_car();
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] FG_SW_CoulombCounter : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_FG_SW_CoulombCounter(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(FG_SW_CoulombCounter, 0664, show_FG_SW_CoulombCounter,
		   store_FG_SW_CoulombCounter);


static ssize_t show_Charging_CallState(struct device *dev, struct device_attribute *attr, char *buf)
{
	battery_xlog_printk(BAT_LOG_CRTI, "call state = %d\n", g_call_state);
	return sprintf(buf, "%u\n", g_call_state);
}

static ssize_t store_Charging_CallState(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	int ret, call_state;
	ret = sscanf(buf, "%u", &call_state);
	g_call_state = (call_state ? KAL_TRUE : KAL_FALSE);
	battery_xlog_printk(BAT_LOG_CRTI, "call state = %d\n", g_call_state);
	return size;
}

static DEVICE_ATTR(Charging_CallState, 0664, show_Charging_CallState, store_Charging_CallState);

static ssize_t show_Charging_Enable(struct device *dev, struct device_attribute *attr, char *buf)
{
	battery_xlog_printk(BAT_LOG_CRTI, "hold charging = %d\n", g_cmd_hold_charging);
	return sprintf(buf, "%u\n", !g_cmd_hold_charging);
}

static ssize_t store_Charging_Enable(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	int charging_enable = 1;
	int ret;
	ret = sscanf(buf, "%u", &charging_enable);

	if (charging_enable == 1)
		g_cmd_hold_charging = KAL_FALSE;
	else if (charging_enable == 0)
		g_cmd_hold_charging = KAL_TRUE;
	wake_up_bat_update_meter();
	battery_xlog_printk(BAT_LOG_CRTI, "hold charging = %d\n", g_cmd_hold_charging);
	return size;
}
static DEVICE_ATTR(Charging_Enable, 0664, show_Charging_Enable, store_Charging_Enable);

static ssize_t show_Custom_Charging_Current(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	battery_xlog_printk(BAT_LOG_CRTI, "custom charging current = %d\n",
			    g_custom_charging_current);
	return sprintf(buf, "%d\n", g_custom_charging_current);
}

static ssize_t store_Custom_Charging_Current(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	int ret, cur;
	ret = sscanf(buf, "%d", &cur);
	g_custom_charging_current = cur;
	battery_xlog_printk(BAT_LOG_CRTI, "custom charging current = %d\n",
			    g_custom_charging_current);
	wake_up_bat_update_meter();
	return size;
}

static DEVICE_ATTR(Custom_Charging_Current, 0664, show_Custom_Charging_Current,
		   store_Custom_Charging_Current);

static void mt_battery_update_EM(struct battery_data *bat_data)
{
	bat_data->BAT_CAPACITY = BMT_status.UI_SOC;
	bat_data->BAT_TemperatureR = BMT_status.temperatureR;	/* API */
	bat_data->BAT_TempBattVoltage = BMT_status.temperatureV;	/* API */
	bat_data->BAT_InstatVolt = BMT_status.bat_vol;	/* VBAT */
	bat_data->BAT_BatteryAverageCurrent = BMT_status.ICharging;
	bat_data->BAT_BatterySenseVoltage = BMT_status.bat_vol;
	bat_data->BAT_ISenseVoltage = BMT_status.Vsense;	/* API */
	bat_data->BAT_ChargerVoltage = BMT_status.charger_vol;

	if ((BMT_status.UI_SOC == 100) && BMT_status.charger_exist)
		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_FULL;

#ifdef CONFIG_MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION
	if (bat_data->BAT_CAPACITY <= 0)
		bat_data->BAT_CAPACITY = 1;

	battery_xlog_printk(BAT_LOG_CRTI,
			    "BAT_CAPACITY=1, due to define MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION\r\n");
#endif
}


static kal_bool mt_battery_100Percent_tracking_check(void)
{
	kal_bool resetBatteryMeter = KAL_FALSE;

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	kal_uint32 cust_sync_time = CUST_SOC_JEITA_SYNC_TIME;
	static kal_uint32 timer_counter = (CUST_SOC_JEITA_SYNC_TIME / BAT_TASK_PERIOD);
#else
	kal_uint32 cust_sync_time = ONEHUNDRED_PERCENT_TRACKING_TIME;
	static kal_uint32 timer_counter = (ONEHUNDRED_PERCENT_TRACKING_TIME / BAT_TASK_PERIOD);
#endif

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	if (g_temp_status != TEMP_POS_10_TO_POS_45 && g_temp_status != TEMP_POS_0_TO_POS_10) {
		battery_xlog_printk(BAT_LOG_FULL, "Skip 100percent tracking due to not 4.2V full-charging.\n");
		return KAL_FALSE;
	}
#endif

	if (BMT_status.bat_full == KAL_TRUE) {	/* charging full first, UI tracking to 100% */

		if (BMT_status.bat_in_recharging_state == KAL_TRUE) {
			if (BMT_status.UI_SOC >= 100)
				BMT_status.UI_SOC = 100;

			resetBatteryMeter = KAL_FALSE;
		} else if (BMT_status.UI_SOC >= 100) {
			BMT_status.UI_SOC = 100;

			if ((g_charging_full_reset_bat_meter == KAL_TRUE)
			    && (BMT_status.bat_charging_state == CHR_BATFULL)) {
				resetBatteryMeter = KAL_TRUE;
				g_charging_full_reset_bat_meter = KAL_FALSE;
			} else {
				resetBatteryMeter = KAL_FALSE;
			}
		} else {
			/* increase UI percentage every xxs */
			if (timer_counter >= (cust_sync_time / BAT_TASK_PERIOD)) {
				timer_counter = 1;
				BMT_status.UI_SOC++;
			} else {
				timer_counter++;

				return resetBatteryMeter;
			}

			resetBatteryMeter = KAL_TRUE;
		}

		battery_xlog_printk(BAT_LOG_FULL,
				    "[Battery] mt_battery_100percent_tracking(), Charging full first UI(%d), reset(%d) \r\n",
				    BMT_status.UI_SOC, resetBatteryMeter);
	} else {
		/* charging is not full,  UI keep 99% if reaching 100%, */

		if (BMT_status.UI_SOC >= 99 && battery_meter_get_battery_current_sign()) {
			BMT_status.UI_SOC = 99;
			resetBatteryMeter = KAL_TRUE;

			battery_xlog_printk(BAT_LOG_FULL,
					    "[Battery] mt_battery_100percent_tracking(), UI full first, keep (%d) \r\n",
					    BMT_status.UI_SOC);
		}

		timer_counter = (cust_sync_time / BAT_TASK_PERIOD);

	}

	return resetBatteryMeter;
}


static kal_bool mt_battery_nPercent_tracking_check(void)
{
	kal_bool resetBatteryMeter = KAL_FALSE;
#if defined(CONFIG_SOC_BY_HW_FG)
	static kal_uint32 timer_counter = (NPERCENT_TRACKING_TIME / BAT_TASK_PERIOD);

	if (BMT_status.nPrecent_UI_SOC_check_point == 0)
		return KAL_FALSE;

	/* fuel gauge ZCV < 15%, but UI > 15%,  15% can be customized */
	if ((BMT_status.ZCV <= BMT_status.nPercent_ZCV)
	    && (BMT_status.UI_SOC > BMT_status.nPrecent_UI_SOC_check_point)) {
		if (timer_counter == (NPERCENT_TRACKING_TIME / BAT_TASK_PERIOD)) {
			BMT_status.UI_SOC--; /* every x sec decrease UI percentage */
			timer_counter = 1;
		} else {
			timer_counter++;
			return resetBatteryMeter;
		}

		resetBatteryMeter = KAL_TRUE;

		battery_xlog_printk(BAT_LOG_CRTI,
				    "[Battery]mt_battery_nPercent_tracking_check(), ZCV(%d) <= BMT_status.nPercent_ZCV(%d), UI_SOC=%d., tracking UI_SOC=%d \r\n",
				    BMT_status.ZCV, BMT_status.nPercent_ZCV, BMT_status.UI_SOC,
				    BMT_status.nPrecent_UI_SOC_check_point);
	} else if ((BMT_status.ZCV > BMT_status.nPercent_ZCV)
		   && (BMT_status.UI_SOC == BMT_status.nPrecent_UI_SOC_check_point)) {
		/* UI less than 15 , but fuel gague is more than 15, hold UI 15% */
		timer_counter = (NPERCENT_TRACKING_TIME / BAT_TASK_PERIOD);
		resetBatteryMeter = KAL_TRUE;

		battery_xlog_printk(BAT_LOG_CRTI,
				    "[Battery]mt_battery_nPercent_tracking_check() ZCV(%d) > BMT_status.nPercent_ZCV(%d) and UI SOC (%d), then keep %d. \r\n",
				    BMT_status.ZCV, BMT_status.nPercent_ZCV, BMT_status.UI_SOC,
				    BMT_status.nPrecent_UI_SOC_check_point);
	} else {
		timer_counter = (NPERCENT_TRACKING_TIME / BAT_TASK_PERIOD);
	}
#endif
	return resetBatteryMeter;

}

static kal_bool mt_battery_0Percent_tracking_check(void)
{
	kal_bool resetBatteryMeter = KAL_TRUE;

	if (BMT_status.UI_SOC <= 0) {
		BMT_status.UI_SOC = 0;
	} else {
		BMT_status.UI_SOC--;
	}

	battery_xlog_printk(BAT_LOG_CRTI,
			    "[Battery] mt_battery_0Percent_tracking_check(), VBAT < %d UI_SOC = (%d)\r\n",
			    SYSTEM_OFF_VOLTAGE, BMT_status.UI_SOC);

	return resetBatteryMeter;
}


static void mt_battery_Sync_UI_Percentage_to_Real(void)
{
	static kal_uint32 timer_counter;

	if ((BMT_status.UI_SOC > BMT_status.SOC) && ((BMT_status.UI_SOC != 1))) {
		/* reduce after xxs */
		if (g_refresh_ui_soc || timer_counter == (SYNC_TO_REAL_TRACKING_TIME / BAT_TASK_PERIOD)) {
			BMT_status.UI_SOC--;
			timer_counter = 0;
			g_refresh_ui_soc = KAL_FALSE;
		} else {
			timer_counter++;
		}

		battery_xlog_printk(BAT_LOG_CRTI,
				    "Sync UI percentage to Real one, BMT_status.UI_SOC=%d, BMT_status.SOC=%d, counter = %d\r\n",
				    BMT_status.UI_SOC, BMT_status.SOC, timer_counter);
	} else {
		timer_counter = 0;

		if (BMT_status.UI_SOC == -1)
			BMT_status.UI_SOC = BMT_status.SOC;
		else if (BMT_status.charger_exist && BMT_status.bat_charging_state != CHR_ERROR) {
			if (BMT_status.UI_SOC < BMT_status.SOC && (BMT_status.SOC - BMT_status.UI_SOC > 1))
				BMT_status.UI_SOC++;
			else
				BMT_status.UI_SOC = BMT_status.SOC;
		}
	}

	if (BMT_status.UI_SOC <= 0) {
		BMT_status.UI_SOC = 1;
		battery_xlog_printk(BAT_LOG_CRTI, "[Battery]UI_SOC get 0 first (%d)\r\n",
				    BMT_status.UI_SOC);
	}
}

static void battery_update(struct battery_data *bat_data)
{
	struct power_supply *bat_psy = &bat_data->psy;
	kal_bool resetBatteryMeter = KAL_FALSE;

	bat_data->BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
	bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
	bat_data->BAT_VOLTAGE_AVG = BMT_status.bat_vol * 1000;
	bat_data->BAT_VOLTAGE_NOW = BMT_status.bat_vol * 1000;	/* voltage_now unit is microvolt */
	bat_data->BAT_TEMP = BMT_status.temperature * 10;
	bat_data->BAT_PRESENT = BMT_status.bat_exist;

	if (BMT_status.charger_exist && (BMT_status.bat_charging_state != CHR_ERROR) && !g_cmd_hold_charging) {
		if (BMT_status.bat_exist) {	/* charging */
			if (BMT_status.bat_vol <= SYSTEM_OFF_VOLTAGE)
				resetBatteryMeter = mt_battery_0Percent_tracking_check();
			else
				resetBatteryMeter = mt_battery_100Percent_tracking_check();

			bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_CHARGING;
		} else {	/* No Battery, Only Charger */

			bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_UNKNOWN;
			BMT_status.UI_SOC = 0;
		}

	} else {		/* Only Battery */

		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_DISCHARGING;
		if (BMT_status.bat_vol <= SYSTEM_OFF_VOLTAGE)
			resetBatteryMeter = mt_battery_0Percent_tracking_check();
		else
			resetBatteryMeter = mt_battery_nPercent_tracking_check();
	}

	if (resetBatteryMeter == KAL_TRUE) {
		battery_meter_reset(KAL_TRUE);
	} else {
			mt_battery_Sync_UI_Percentage_to_Real();
		}

	battery_xlog_printk(BAT_LOG_FULL, "UI_SOC=(%d), resetBatteryMeter=(%d)\n",
			    BMT_status.UI_SOC, resetBatteryMeter);

#ifdef CUST_CAPACITY_OCV2CV_TRANSFORM
	/* We store capacity before loading compenstation in RTC */
	if (battery_meter_get_battery_soc() <= 1)
		set_rtc_spare_fg_value(1);
	else
		set_rtc_spare_fg_value(battery_meter_get_battery_soc());  /*use battery_soc*/
#else

	/* set RTC SOC to 1 to avoid SOC jump in charger boot. */
	if (BMT_status.UI_SOC <= 1)
		set_rtc_spare_fg_value(1);
	else
		set_rtc_spare_fg_value(BMT_status.UI_SOC);
#endif
	battery_xlog_printk(BAT_LOG_FULL, "RTC_SOC=(%d)\n", get_rtc_spare_fg_value());

	mt_battery_update_EM(bat_data);
#if defined(CONFIG_AMAZON_METRICS_LOG)
	bat_data->BAT_ChargeFull = g_fg_dbg_bat_qmax;
	/*
	 * Correctness of the BatteryStats High-Precision drain metrics
	 * depend on the BatteryStatsExtension plug event handler
	 * reading a value from /sys/.../BAT_ChargeCounter that:
	 *   1. Differs from previous value read only by reflecting
	 *      battery discharge over the interim, not (a) any charging
	 *      or (b) counter resets.
	 *   2. Is reasonably up-to-date relative to the hardware.
	 * Correctness depends on the above for lack of a way to synchronize
	 * the BatteryStats reads with this driver's resets of the hardware
	 * Charge Counter (CAR).
	 *
	 * Satisfy (1a) by ignoring any non-negative new CAR value.
	 * Satisfy (1b) by noting that CAR resets will always be to zero, and
	 * ignoring any non-negative new CAR value.
	 * Satisfy (2) by reading directly from the CAR hardware -- battery_meter_get_car();.
	 */
	{
		int new_CAR = battery_meter_get_car();

		battery_xlog_printk(BAT_LOG_FULL,
			"reading CAR: new_CAR %d, bat_data->old_CAR %d\n",
			new_CAR, bat_data->old_CAR);

		if (new_CAR < 0) {
			bat_data->BAT_ChargeCounter += (new_CAR - bat_data->old_CAR);
			battery_xlog_printk(BAT_LOG_FULL,
				"setting BAT_ChargeCounter to %d\n",
				bat_data->BAT_ChargeCounter);
		}
		bat_data->old_CAR = new_CAR;
	}
	metrics_handle();
#endif
	power_supply_changed(bat_psy);
}

static void ac_update(struct ac_data *ac_data)
{
	static int ac_status = -1;
	struct power_supply *ac_psy = &ac_data->psy;

	if (BMT_status.charger_exist) {
		if ((BMT_status.charger_type == NONSTANDARD_CHARGER) ||
		    (BMT_status.charger_type == STANDARD_CHARGER) ||
		    (BMT_status.charger_type == APPLE_1_0A_CHARGER) ||
		    (BMT_status.charger_type == APPLE_2_1A_CHARGER)) {
			ac_data->AC_ONLINE = 1;
			ac_psy->type = POWER_SUPPLY_TYPE_MAINS;
		} else
			ac_data->AC_ONLINE = 0;

	} else {
		ac_data->AC_ONLINE = 0;
	}

	if (ac_status != ac_data->AC_ONLINE) {
		ac_status = ac_data->AC_ONLINE;
		power_supply_changed(ac_psy);
	}
}

static void usb_update(struct usb_data *usb_data)
{
	static int usb_status = -1;
	struct power_supply *usb_psy = &usb_data->psy;

	if (BMT_status.charger_exist) {
		if ((BMT_status.charger_type == STANDARD_HOST) ||
		    (BMT_status.charger_type == CHARGING_HOST)) {
			usb_data->USB_ONLINE = 1;
			usb_psy->type = POWER_SUPPLY_TYPE_USB;
		} else
			usb_data->USB_ONLINE = 0;
	} else {
		usb_data->USB_ONLINE = 0;
	}

	if (usb_status != usb_data->USB_ONLINE) {
		usb_status = usb_data->USB_ONLINE;
		power_supply_changed(usb_psy);
	}
}

#endif

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Battery Temprature Parameters and functions */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
kal_bool pmic_chrdet_status(void)
{
	if (upmu_is_chr_det() == KAL_TRUE) {
		return KAL_TRUE;
	} else {
		battery_xlog_printk(BAT_LOG_CRTI, "[pmic_chrdet_status] No charger\r\n");
		return KAL_FALSE;
	}
}

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Pulse Charging Algorithm */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
kal_bool bat_is_charger_exist(void)
{
	return bat_charger_get_detect_status();
}


kal_bool bat_is_charging_full(void)
{
	if ((BMT_status.bat_full == KAL_TRUE) && (BMT_status.bat_in_recharging_state == KAL_FALSE))
		return KAL_TRUE;
	else
		return KAL_FALSE;
}


kal_uint32 bat_get_ui_percentage(void)
{
	/* for plugging out charger in recharge phase, using SOC as UI_SOC */
	if (chr_wake_up_bat == KAL_TRUE)
		return BMT_status.SOC;
	else
		return BMT_status.UI_SOC;
}

/* Full state --> recharge voltage --> full state */
kal_uint32 bat_is_recharging_phase(void)
{
	if (BMT_status.bat_in_recharging_state || BMT_status.bat_full == KAL_TRUE)
		return KAL_TRUE;
	else
		return KAL_FALSE;
}

unsigned long BAT_Get_Battery_Voltage(int polling_mode)
{
	unsigned long ret_val = 0;

#if defined(CONFIG_POWER_EXT)
	ret_val = 4000;
#else
	ret_val = battery_meter_get_battery_voltage();
#endif

	return ret_val;
}


static void mt_battery_average_method_init(kal_uint32 *bufferdata, kal_uint32 data,
					   kal_int32 *sum)
{
	kal_uint32 i;
	static kal_bool batteryBufferFirst = KAL_TRUE;
	static kal_bool previous_charger_exist = KAL_FALSE;
	static kal_bool previous_in_recharge_state = KAL_FALSE;
	static kal_uint8 index;

	/* reset charging current window while plug in/out */
	if (need_clear_current_window) {
		if (BMT_status.charger_exist) {
			if (previous_charger_exist == KAL_FALSE) {
				batteryBufferFirst = KAL_TRUE;
				previous_charger_exist = KAL_TRUE;
				if ((BMT_status.charger_type == STANDARD_CHARGER)
				    || (BMT_status.charger_type == APPLE_1_0A_CHARGER)
				    || (BMT_status.charger_type == APPLE_2_1A_CHARGER)
				    || (BMT_status.charger_type == CHARGING_HOST))
					data = 650;	/* mA */
				else	/* USB or non-stadanrd charger */
					data = 450;	/* mA */
			} else if ((previous_in_recharge_state == KAL_FALSE)
				   && (BMT_status.bat_in_recharging_state == KAL_TRUE)) {
				batteryBufferFirst = KAL_TRUE;
				if ((BMT_status.charger_type == STANDARD_CHARGER)
				    || (BMT_status.charger_type == APPLE_1_0A_CHARGER)
				    || (BMT_status.charger_type == APPLE_2_1A_CHARGER)
				    || (BMT_status.charger_type == CHARGING_HOST))
					data = 650;	/* mA */
				else	/* USB or non-stadanrd charger */
					data = 450;	/* mA */
			}

			previous_in_recharge_state = BMT_status.bat_in_recharging_state;
		} else {
			if (previous_charger_exist == KAL_TRUE) {
				batteryBufferFirst = KAL_TRUE;
				previous_charger_exist = KAL_FALSE;
				data = 0;
			}
		}
	}

	battery_xlog_printk(BAT_LOG_FULL, "batteryBufferFirst =%d, data= (%d)\n",
			    batteryBufferFirst, data);

	if (batteryBufferFirst == KAL_TRUE) {
		for (i = 0; i < BATTERY_AVERAGE_SIZE; i++)
			bufferdata[i] = data;

		*sum = data * BATTERY_AVERAGE_SIZE;
	}

	index++;
	if (index >= BATTERY_AVERAGE_DATA_NUMBER) {
		index = BATTERY_AVERAGE_DATA_NUMBER;
		batteryBufferFirst = KAL_FALSE;
	}
}


static kal_uint32 mt_battery_average_method(kal_uint32 *bufferdata, kal_uint32 data,
					    kal_int32 *sum, kal_uint8 batteryIndex)
{
	kal_uint32 avgdata;

	mt_battery_average_method_init(bufferdata, data, sum);

	*sum -= bufferdata[batteryIndex];
	*sum += data;
	bufferdata[batteryIndex] = data;
	avgdata = (*sum) / BATTERY_AVERAGE_SIZE;

	battery_xlog_printk(BAT_LOG_FULL, "bufferdata[%d]= (%d)\n", batteryIndex,
			    bufferdata[batteryIndex]);
	return avgdata;
}

void mt_battery_GetBatteryData(void)
{
	kal_uint32 bat_vol, charger_vol, Vsense, ZCV;
	kal_int32 ICharging, temperature, temperatureR, temperatureV, SOC;
	kal_int32 avg_temperature;
	static kal_int32 bat_sum, icharging_sum, temperature_sum;
	static kal_int32 batteryVoltageBuffer[BATTERY_AVERAGE_SIZE];
	static kal_int32 batteryCurrentBuffer[BATTERY_AVERAGE_SIZE];
	static kal_int32 batteryTempBuffer[BATTERY_AVERAGE_SIZE];
	static kal_uint8 batteryIndex;
	static kal_int32 previous_SOC = -1;

	bat_vol = battery_meter_get_battery_voltage();
	Vsense = battery_meter_get_VSense();
	ICharging = battery_meter_get_charging_current();
	charger_vol = battery_meter_get_charger_voltage();
	temperature = battery_meter_get_battery_temperature();
	temperatureV = battery_meter_get_tempV();
	temperatureR = battery_meter_get_tempR(temperatureV);

	if (bat_meter_timeout == KAL_TRUE) {
		SOC = battery_meter_get_battery_percentage();
		bat_meter_timeout = KAL_FALSE;
	} else {
		if (previous_SOC == -1)
			SOC = battery_meter_get_battery_percentage();
		else
			SOC = previous_SOC;
	}

	ZCV = battery_meter_get_battery_zcv();

	need_clear_current_window = KAL_TRUE;
	BMT_status.ICharging =
	    mt_battery_average_method(&batteryCurrentBuffer[0], ICharging, &icharging_sum,
				      batteryIndex);
	need_clear_current_window = KAL_FALSE;
#if 1
	if (previous_SOC == -1 && bat_vol <= SYSTEM_OFF_VOLTAGE) {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "battery voltage too low, use ZCV to init average data.\n");
		BMT_status.bat_vol =
		    mt_battery_average_method(&batteryVoltageBuffer[0], ZCV, &bat_sum,
					      batteryIndex);
	} else {
		BMT_status.bat_vol =
		    mt_battery_average_method(&batteryVoltageBuffer[0], bat_vol, &bat_sum,
					      batteryIndex);
	}
#else
	BMT_status.bat_vol =
	    mt_battery_average_method(&batteryVoltageBuffer[0], bat_vol, &bat_sum, batteryIndex);
#endif
	avg_temperature =
	    mt_battery_average_method(&batteryTempBuffer[0], temperature, &temperature_sum,
				      batteryIndex);

	if (USE_AVG_TEMPERATURE)
		BMT_status.temperature = avg_temperature;
	else
		BMT_status.temperature = temperature;

	if ((g_battery_thermal_throttling_flag == 1) || (g_battery_thermal_throttling_flag == 3)) {
		if (battery_cmd_thermal_test_mode == 1) {
			BMT_status.temperature = battery_cmd_thermal_test_mode_value;
			battery_xlog_printk(BAT_LOG_FULL,
				"[Battery] In thermal_test_mode , Tbat=%d\n", BMT_status.temperature);
		}

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		if (temperature > 60 || temperature < -10) {
			struct battery_data *bat_data = &battery_main;
			struct power_supply *bat_psy = &bat_data->psy;

			pr_warn("[Battery] instant Tbat(%d) out of range, power down device.\n", temperature);

			bat_data->BAT_CAPACITY = 0;
			power_supply_changed(bat_psy);

			/* can not power down due to charger exist, so need reset system */
			if (BMT_status.charger_exist)
				bat_charger_set_platform_reset();

			/* avoid SW no feedback */
			mt_power_off();
		}
#endif
	}

	BMT_status.Vsense = Vsense;
	BMT_status.charger_vol = charger_vol;
	BMT_status.temperatureV = temperatureV;
	BMT_status.temperatureR = temperatureR;
	BMT_status.SOC = SOC;
	BMT_status.ZCV = ZCV;

#ifndef CUST_CAPACITY_OCV2CV_TRANSFORM
	if (BMT_status.charger_exist == KAL_FALSE) {
		if (BMT_status.SOC > previous_SOC && previous_SOC >= 0)
			BMT_status.SOC = previous_SOC;
	}
#endif

	previous_SOC = BMT_status.SOC;

	batteryIndex++;
	if (batteryIndex >= BATTERY_AVERAGE_SIZE)
		batteryIndex = 0;



	battery_xlog_printk(BAT_LOG_CRTI,
			    "AvgVbat=(%d),bat_vol=(%d),AvgI=(%d),I=(%d),VChr=(%d),AvgT=(%d),T=(%d),pre_SOC=(%d),SOC=(%d),ZCV=(%d)\n",
			    BMT_status.bat_vol, bat_vol, BMT_status.ICharging, ICharging,
			    BMT_status.charger_vol, BMT_status.temperature, temperature,
			    previous_SOC, BMT_status.SOC, BMT_status.ZCV);


}


static PMU_STATUS mt_battery_CheckBatteryTemp(void)
{
	PMU_STATUS status = PMU_STATUS_OK;

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)

	battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] support JEITA, temperature=%d\n",
			    BMT_status.temperature);

	if (do_jeita_state_machine() == PMU_STATUS_FAIL) {
		battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] JEITA : fail\n");
		status = PMU_STATUS_FAIL;
	}
#else

#ifdef CONFIG_BAT_LOW_TEMP_PROTECT_ENABLE
	if ((BMT_status.temperature < MIN_CHARGE_TEMPERATURE)
	    || (BMT_status.temperature == ERR_CHARGE_TEMPERATURE)) {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[BATTERY] Battery Under Temperature or NTC fail !!\n\r");
		status = PMU_STATUS_FAIL;
	}
#endif
	if (BMT_status.temperature >= MAX_CHARGE_TEMPERATURE) {
		battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Battery Over Temperature !!\n\r");
		status = PMU_STATUS_FAIL;
	}
#endif

	return status;
}


static PMU_STATUS mt_battery_CheckChargerVoltage(void)
{
	PMU_STATUS status = PMU_STATUS_OK;

	if (BMT_status.charger_exist) {
		if (V_CHARGER_ENABLE == 1) {
			if (BMT_status.charger_vol <= V_CHARGER_MIN) {
				battery_xlog_printk(BAT_LOG_CRTI,
						    "[BATTERY]Charger under voltage!!\r\n");
				BMT_status.bat_charging_state = CHR_ERROR;
				status = PMU_STATUS_FAIL;
			}
		}
		if (BMT_status.charger_vol >= V_CHARGER_MAX) {
			battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY]Charger over voltage !!\r\n");
			BMT_status.charger_protect_status = charger_OVER_VOL;
			BMT_status.bat_charging_state = CHR_ERROR;
			status = PMU_STATUS_FAIL;
		}
	}

	return status;
}


static PMU_STATUS mt_battery_CheckChargingTime(void)
{
	PMU_STATUS status = PMU_STATUS_OK;

	if ((g_battery_thermal_throttling_flag == 2) || (g_battery_thermal_throttling_flag == 3)) {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[TestMode] Disable Safty Timer. bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
				    g_battery_thermal_throttling_flag,
				    battery_cmd_thermal_test_mode,
				    battery_cmd_thermal_test_mode_value);

	} else {
		/* Charging OT */
		if (BMT_status.total_charging_time >= MAX_CHARGING_TIME) {
			battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Charging Over Time.\n");

			status = PMU_STATUS_FAIL;
		}
	}

	return status;

}

#if defined(CONFIG_STOP_CHARGING_IN_TAKLING)
static PMU_STATUS mt_battery_CheckCallState(void)
{
	PMU_STATUS status = PMU_STATUS_OK;

	if ((g_call_state == CALL_ACTIVE) && (BMT_status.bat_vol > V_CC2TOPOFF_THRES))
		status = PMU_STATUS_FAIL;

	return status;
}
#endif

static void mt_battery_CheckBatteryStatus(void)
{
	if (mt_battery_CheckBatteryTemp() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}

	if (mt_battery_CheckChargerVoltage() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}
#if defined(CONFIG_STOP_CHARGING_IN_TAKLING)
	if (mt_battery_CheckCallState() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_HOLD;
		return;
	}
#endif

	if (mt_battery_CheckChargingTime() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}

	if (g_cmd_hold_charging == KAL_TRUE) {
		BMT_status.bat_charging_state = CHR_CMD_HOLD;
		return;
	} else if (BMT_status.bat_charging_state == CHR_CMD_HOLD) {
		BMT_status.bat_charging_state = CHR_PRE;
		return;
	}

}


static void mt_battery_notify_TatalChargingTime_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME)
	if ((g_battery_thermal_throttling_flag == 2) || (g_battery_thermal_throttling_flag == 3)) {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[TestMode] Disable Safty Timer : no UI display\n");
	} else {
		if (BMT_status.total_charging_time >= MAX_CHARGING_TIME)
			/* if(BMT_status.total_charging_time >= 60) //test */
		{
			g_BatteryNotifyCode |= 0x0010;
			battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Charging Over Time\n");
		} else {
			g_BatteryNotifyCode &= ~(0x0010);
		}
	}

	battery_xlog_printk(BAT_LOG_FULL,
			    "[BATTERY] BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME (%x)\n",
			    g_BatteryNotifyCode);
#endif
}


static void mt_battery_notify_VBat_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0004_VBAT)
	if (BMT_status.bat_vol > 4350)
		/* if(BMT_status.bat_vol > 3800) //test */
	{
		g_BatteryNotifyCode |= 0x0008;
		battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] bat_vlot(%ld) > 4350mV\n",
				    BMT_status.bat_vol);
	} else {
		g_BatteryNotifyCode &= ~(0x0008);
	}

	battery_xlog_printk(BAT_LOG_FULL, "[BATTERY] BATTERY_NOTIFY_CASE_0004_VBAT (%x)\n",
			    g_BatteryNotifyCode);

#endif
}


static void mt_battery_notify_ICharging_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0003_ICHARGING)
	if ((BMT_status.ICharging > 1000) && (BMT_status.total_charging_time > 300)) {
		g_BatteryNotifyCode |= 0x0004;
		battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] I_charging(%ld) > 1000mA\n",
				    BMT_status.ICharging);
	} else {
		g_BatteryNotifyCode &= ~(0x0004);
	}

	battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] BATTERY_NOTIFY_CASE_0003_ICHARGING (%x)\n",
			    g_BatteryNotifyCode);

#endif
}


static void mt_battery_notify_VBatTemp_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0002_VBATTEMP)

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	if ((BMT_status.temperature >= MAX_CHARGE_TEMPERATURE)
		|| (BMT_status.temperature < TEMP_NEG_10_THRESHOLD)) {
#else
#ifdef CONFIG_BAT_LOW_TEMP_PROTECT_ENABLE
	if ((BMT_status.temperature >= MAX_CHARGE_TEMPERATURE)
		|| (BMT_status.temperature < MIN_CHARGE_TEMPERATURE)) {
#else
	if (BMT_status.temperature >= MAX_CHARGE_TEMPERATURE) {
#endif
#endif				/* #if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT) */
		g_BatteryNotifyCode |= 0x0002;
		battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] bat_temp(%d) out of range\n",
				    BMT_status.temperature);
	} else {
		g_BatteryNotifyCode &= ~(0x0002);
	}

	battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] BATTERY_NOTIFY_CASE_0002_VBATTEMP (%x)\n",
			    g_BatteryNotifyCode);

#endif
}


static void mt_battery_notify_VCharger_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0001_VCHARGER)
	if (BMT_status.charger_vol > V_CHARGER_MAX) {
		g_BatteryNotifyCode |= 0x0001;
		battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] BMT_status.charger_vol(%ld) > %d mV\n",
				    BMT_status.charger_vol, V_CHARGER_MAX);
	} else {
		g_BatteryNotifyCode &= ~(0x0001);
	}

	battery_xlog_printk(BAT_LOG_FULL, "[BATTERY] BATTERY_NOTIFY_CASE_0001_VCHARGER (%x)\n",
			    g_BatteryNotifyCode);
#endif
}


static void mt_battery_notify_UI_test(void)
{
	if (g_BN_TestMode == 0x0001) {
		g_BatteryNotifyCode = 0x0001;
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0001_VCHARGER\n");
	} else if (g_BN_TestMode == 0x0002) {
		g_BatteryNotifyCode = 0x0002;
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0002_VBATTEMP\n");
	} else if (g_BN_TestMode == 0x0003) {
		g_BatteryNotifyCode = 0x0004;
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0003_ICHARGING\n");
	} else if (g_BN_TestMode == 0x0004) {
		g_BatteryNotifyCode = 0x0008;
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0004_VBAT\n");
	} else if (g_BN_TestMode == 0x0005) {
		g_BatteryNotifyCode = 0x0010;
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME\n");
	} else {
		battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Unknown BN_TestMode Code : %x\n",
				    g_BN_TestMode);
	}
}


void mt_battery_notify_check(void)
{
	g_BatteryNotifyCode = 0x0000;

	if (g_BN_TestMode == 0x0000) {	/* for normal case */
		battery_xlog_printk(BAT_LOG_FULL, "[BATTERY] mt_battery_notify_check\n");

		mt_battery_notify_VCharger_check();

		mt_battery_notify_VBatTemp_check();

		mt_battery_notify_ICharging_check();

		mt_battery_notify_VBat_check();

		mt_battery_notify_TatalChargingTime_check();
	} else {		/* for UI test */

		mt_battery_notify_UI_test();
	}
}

static void mt_battery_thermal_check(void)
{
	if ((g_battery_thermal_throttling_flag == 1) || (g_battery_thermal_throttling_flag == 3)) {
		if (battery_cmd_thermal_test_mode == 1) {
			BMT_status.temperature = battery_cmd_thermal_test_mode_value;
			battery_xlog_printk(BAT_LOG_FULL,
					    "[Battery] In thermal_test_mode , Tbat=%d\n",
					    BMT_status.temperature);
		}
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		if (BMT_status.temperature > 60 || BMT_status.temperature < -10) {
			struct battery_data *bat_data = &battery_main;
			struct power_supply *bat_psy = &bat_data->psy;

			pr_warn("[Battery] Tbat(%d)out of range, power down device.\n", BMT_status.temperature);

			bat_data->BAT_CAPACITY = 0;
			power_supply_changed(bat_psy);

			/* can not power down due to charger exist, so need reset system */
			if (BMT_status.charger_exist)
				bat_charger_set_platform_reset();

			/* avoid SW no feedback */
			mt_power_off();
		}
#else
		if (BMT_status.temperature >= 60) {
#if defined(CONFIG_POWER_EXT)
			battery_xlog_printk(BAT_LOG_CRTI,
					    "[BATTERY] CONFIG_POWER_EXT, no update battery update power down.\n");
#else
			{
				if ((g_platform_boot_mode == META_BOOT)
				    || (g_platform_boot_mode == ADVMETA_BOOT)
				    || (g_platform_boot_mode == ATE_FACTORY_BOOT)) {
					battery_xlog_printk(BAT_LOG_FULL,
							    "[BATTERY] boot mode = %d, bypass temperature check\n",
							    g_platform_boot_mode);
				} else {
					struct battery_data *bat_data = &battery_main;
					struct power_supply *bat_psy = &bat_data->psy;

					battery_xlog_printk(BAT_LOG_ERROR,
							    "[Battery] Tbat(%d)>=60, system need power down.\n",
							    BMT_status.temperature);

					bat_data->BAT_CAPACITY = 0;

					power_supply_changed(bat_psy);

					if (BMT_status.charger_exist) {
						/* can not power down due to charger exist, so need reset system */
						bat_charger_set_platform_reset();
					}
					/* avoid SW no feedback */
					mt_power_off();
				}
			}
#endif
		}
#endif

	}

}

CHARGER_TYPE bat_charger_type_detection(void)
{
	mutex_lock(&charger_type_mutex);
	if (BMT_status.charger_type == CHARGER_UNKNOWN)
		BMT_status.charger_type = bat_charger_get_charger_type();
	mutex_unlock(&charger_type_mutex);

	return BMT_status.charger_type;
}


static void mt_battery_charger_detect_check(void)
{
	static kal_bool fg_first_detect = KAL_FALSE;

	if (upmu_is_chr_det() == KAL_TRUE) {

		if (!BMT_status.charger_exist)
			wake_lock(&battery_suspend_lock);

		BMT_status.charger_exist = KAL_TRUE;

		/* re-detect once after 10s if it is non-standard type */
		if (BMT_status.charger_type == NONSTANDARD_CHARGER && fg_first_detect) {
			mutex_lock(&charger_type_mutex);
			BMT_status.charger_type = bat_charger_get_charger_type();
			mutex_unlock(&charger_type_mutex);
			fg_first_detect = KAL_FALSE;
			if (BMT_status.charger_type != NONSTANDARD_CHARGER)
				pr_warn("Update charger type to %d!\n", BMT_status.charger_type);
		}

		if (BMT_status.charger_type == CHARGER_UNKNOWN) {
			bat_charger_type_detection();
			if ((BMT_status.charger_type == STANDARD_HOST)
			    || (BMT_status.charger_type == CHARGING_HOST)) {
				mt_usb_connect();
			}
			if (BMT_status.charger_type != CHARGER_UNKNOWN)
				fg_first_detect = KAL_TRUE;
		}

		battery_xlog_printk(BAT_LOG_FULL, "[BAT_thread]Cable in, CHR_Type_num=%d\r\n",
				    BMT_status.charger_type);

	} else {
		if (BMT_status.charger_exist)
			wake_lock_timeout(&battery_suspend_lock, HZ/2);

		fg_first_detect = KAL_FALSE;

		BMT_status.charger_exist = KAL_FALSE;
		BMT_status.charger_type = CHARGER_UNKNOWN;
		BMT_status.bat_full = KAL_FALSE;
		BMT_status.bat_in_recharging_state = KAL_FALSE;
		BMT_status.bat_charging_state = CHR_PRE;
		BMT_status.total_charging_time = 0;
		BMT_status.PRE_charging_time = 0;
		BMT_status.CC_charging_time = 0;
		BMT_status.TOPOFF_charging_time = 0;
		BMT_status.POSTFULL_charging_time = 0;

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
		if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
			|| g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_warn("[pmic_thread_kthread] Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
			orderly_poweroff(true);
		}
#endif

		if (g_cmd_hold_charging) {
			g_cmd_hold_charging = KAL_FALSE;
			bat_charger_enable_power_path(true);
		}

		battery_xlog_printk(BAT_LOG_FULL, "[BAT_thread]Cable out \r\n");
		mt_usb_disconnect();
	}
}


static void mt_battery_update_status(void)
{
#if defined(CONFIG_POWER_EXT)
	battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] CONFIG_POWER_EXT, no update Android.\n");
#else
	{
		if (battery_meter_initilized == KAL_TRUE)
			battery_update(&battery_main);

		ac_update(&ac_main);
		usb_update(&usb_main);
#if defined(CONFIG_AMAZON_METRICS_LOG)
		metrics_charger_update(ac_main.AC_ONLINE, usb_main.USB_ONLINE);
#endif
	}
#endif
}

static void do_chrdet_int_task(void)
{
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[128] = {0};
#endif
	if (g_bat_init_flag == KAL_TRUE) {
		if (upmu_is_chr_det() == KAL_TRUE) {
			pr_info("[do_chrdet_int_task] charger exist!\n");

			if (!BMT_status.charger_exist)
				wake_lock(&battery_suspend_lock);
			BMT_status.charger_exist = KAL_TRUE;

#ifdef CONFIG_AMAZON_METRICS_LOG
			snprintf(buf, sizeof(buf),
				"%s:bq24297:vbus_on=1;CT;1:NR",
				__func__);
			log_to_metrics(ANDROID_LOG_INFO, "USBCableEvent", buf);
#endif

#if defined(CONFIG_POWER_EXT)
			mt_usb_connect();
			battery_xlog_printk(BAT_LOG_CRTI,
					    "[do_chrdet_int_task] call mt_usb_connect() in EVB\n");
#endif
		} else {
			pr_info("[do_chrdet_int_task] charger NOT exist!\n");
			if (BMT_status.charger_exist)
				wake_lock_timeout(&battery_suspend_lock, HZ/2);
			BMT_status.charger_exist = KAL_FALSE;
#ifdef CONFIG_AMAZON_METRICS_LOG
			memset(buf, '\0', sizeof(buf));
			snprintf(buf, sizeof(buf),
				"%s:bq24297:vbus_off=1;CT;1;DV;1:NR",
				__func__);
			log_to_metrics(ANDROID_LOG_INFO, "USBCableEvent", buf);
#endif

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
			if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
			    || g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
				battery_xlog_printk(BAT_LOG_CRTI,
						    "[pmic_thread_kthread] Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
				orderly_poweroff(true);
			}
#endif

#if defined(CONFIG_POWER_EXT)
			mt_usb_disconnect();
			battery_xlog_printk(BAT_LOG_CRTI,
					    "[do_chrdet_int_task] call mt_usb_disconnect() in EVB\n");
#endif
		}

#ifdef CONFIG_AMAZON_METRICS_LOG
			memset(buf, '\0', sizeof(buf));
			if (BMT_status.charger_type > CHARGER_UNKNOWN) {
				snprintf(buf, sizeof(buf),
					"%s:bq24297:detect=1;CT;1;type=%d;DV;1:NR",
					__func__, BMT_status.charger_type);
				log_to_metrics(ANDROID_LOG_INFO, "USBCableEvent", buf);
			}
#endif
		if (BMT_status.UI_SOC == 100 && BMT_status.charger_exist) {
			BMT_status.bat_charging_state = CHR_BATFULL;
			BMT_status.bat_full = KAL_TRUE;
			g_charging_full_reset_bat_meter = KAL_TRUE;
		}
		mt_battery_update_status();
		wake_up_bat();
	} else {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[do_chrdet_int_task] battery thread not ready, will do after bettery init.\n");
	}

}

irqreturn_t ops_chrdet_int_handler(int irq, void *dev_id)
{
	pr_info("[Power/Battery][chrdet_bat_int_handler]....\n");

	do_chrdet_int_task();

	return IRQ_HANDLED;
}

void BAT_thread(void)
{
	if (battery_meter_initilized == KAL_FALSE) {
		battery_meter_initial();	/* move from battery_probe() to decrease booting time */
		BMT_status.nPercent_ZCV = battery_meter_get_battery_nPercent_zcv();
		battery_meter_initilized = KAL_TRUE;
	}

	mt_battery_charger_detect_check();

	if (fg_battery_shutdown)
		return;

	mt_battery_GetBatteryData();

	if (fg_battery_shutdown)
		return;

	mt_battery_thermal_check();
	mt_battery_notify_check();

	if (BMT_status.charger_exist && !fg_battery_shutdown) {
		mt_battery_CheckBatteryStatus();
		mt_battery_charging_algorithm();
	}

	mt_battery_update_status();
}

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Internal API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
int bat_thread_kthread(void *x)
{
	ktime_t ktime = ktime_set(3, 0);	/* 10s, 10* 1000 ms */

	/* Run on a process content */
	while (!fg_battery_shutdown) {
		int ret;

		mutex_lock(&bat_mutex);

		if (g_bat.init_done && !battery_suspended)
			BAT_thread();

		mutex_unlock(&bat_mutex);

		ret = wait_event_hrtimeout(bat_thread_wq, atomic_read(&bat_thread_wakeup), ktime);
		if (ret == -ETIME)
			bat_meter_timeout = true;
		else
			atomic_dec(&bat_thread_wakeup);

		pr_debug("%s: waking up: on %s; wake_flag=%d\n",
			__func__, ret == -ETIME ? "timer" : "event",
			atomic_read(&bat_thread_wakeup));

		if (!fg_battery_shutdown)
			ktime = ktime_set(BAT_TASK_PERIOD, 0);	/* 10s, 10* 1000 ms */

		if (chr_wake_up_bat == KAL_TRUE) {	/* for charger plug in/ out */

			if (g_bat.init_done)
				battery_meter_reset(KAL_FALSE);
			chr_wake_up_bat = KAL_FALSE;

			battery_xlog_printk(BAT_LOG_CRTI,
					    "[BATTERY] Charger plug in/out, Call battery_meter_reset. (%d)\n",
					    BMT_status.UI_SOC);
		}

	}
	mutex_lock(&bat_mutex);
	g_bat.down = true;
	mutex_unlock(&bat_mutex);

	return 0;
}
/*
 * This is charger interface to USB OTG code.
 * If OTG is host, charger functionality, and charger interrupt
 * must be disabled
 * */
void bat_detect_set_usb_host_mode(bool usb_host_mode)
{
	mutex_lock(&bat_mutex);
	/* Don't change the charger event state
	 * if charger logic is not running */
	if (g_bat.init_done) {
		if (usb_host_mode && !g_bat.usb_host_mode)
			disable_irq(g_bat.irq);
		if (!usb_host_mode && g_bat.usb_host_mode)
			enable_irq(g_bat.irq);
		g_bat.usb_host_mode = usb_host_mode;
	}
	mutex_unlock(&bat_mutex);
}
EXPORT_SYMBOL(bat_detect_set_usb_host_mode);

static int bat_setup_charger_locked(void)
{
	int ret = -EAGAIN;

	if (g_bat.common_init_done && g_bat.charger && !g_bat.init_done) {

		/* AP:
		 * Both common_battery and charger code are ready to go.
		 * Finalize init of common_battery.
		 */
		g_platform_boot_mode = bat_charger_get_platform_boot_mode();
		battery_xlog_printk(BAT_LOG_CRTI, "[BAT_probe] g_platform_boot_mode = %d\n ",
			g_platform_boot_mode);

		/* AP:
		 * MTK implementation requires that BAT_thread() be called at least once
		 * before battery event is enabled.
		 * Although this should not be necessary, we maintain compatibility
		 * until rework is complete.
		 */

		BAT_thread();
		g_bat.init_done = true;
		enable_irq(g_bat.irq);
		pr_notice("%s: charger setup done\n", __func__);
	}

	return ret;
}

int bat_charger_register(CHARGING_CONTROL ctrl)
{
	int ret;

	mutex_lock(&bat_mutex);
	g_bat.charger = ctrl;
	ret = bat_setup_charger_locked();
	mutex_unlock(&bat_mutex);

	return ret;
}
EXPORT_SYMBOL(bat_charger_register);

#if defined(CONFIG_AMAZON_METRICS_LOG)
static void metrics_resume(void)
{
	struct battery_info *info = &BQ_info;
	/* invalidate all the measurements */
	mutex_lock(&info->lock);
	info->flags |= BQ_STATUS_RESUMING;
	mutex_unlock(&info->lock);
}
#endif
/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // fop API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static long adc_cali_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int *user_data_addr;
	int *naram_data_addr;
	int i = 0;
	int ret = 0;
	int adc_in_data[2] = { 1, 1 };
	int adc_out_data[2] = { 1, 1 };

	mutex_lock(&bat_mutex);

	switch (cmd) {
	case TEST_ADC_CALI_PRINT:
		g_ADC_Cali = KAL_FALSE;
		break;

	case SET_ADC_CALI_Slop:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_slop, naram_data_addr, 36);
		g_ADC_Cali = KAL_FALSE;	/* enable calibration after setting ADC_CALI_Cal */
		/* Protection */
		for (i = 0; i < 14; i++) {
			if ((*(adc_cali_slop + i) == 0) || (*(adc_cali_slop + i) == 1))
				*(adc_cali_slop + i) = 1000;
		}
		/*
		for (i = 0; i < 14; i++)
			battery_xlog_printk(BAT_LOG_CRTI, "adc_cali_slop[%d] = %d\n", i,
					    *(adc_cali_slop + i));
		battery_xlog_printk(BAT_LOG_FULL,
				    "**** unlocked_ioctl : SET_ADC_CALI_Slop Done!\n");
		*/
		break;

	case SET_ADC_CALI_Offset:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_offset, naram_data_addr, 36);
		g_ADC_Cali = KAL_FALSE;	/* enable calibration after setting ADC_CALI_Cal */
		/*
		for (i = 0; i < 14; i++)
			battery_xlog_printk(BAT_LOG_CRTI, "adc_cali_offset[%d] = %d\n", i,
					    *(adc_cali_offset + i));
		battery_xlog_printk(BAT_LOG_FULL,
				    "**** unlocked_ioctl : SET_ADC_CALI_Offset Done!\n");
		*/
		break;

	case SET_ADC_CALI_Cal:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_cal, naram_data_addr, 4);
		g_ADC_Cali = KAL_TRUE;
		if (adc_cali_cal[0] == 1)
			g_ADC_Cali = KAL_TRUE;
		else
			g_ADC_Cali = KAL_FALSE;

		for (i = 0; i < 1; i++)
			battery_xlog_printk(BAT_LOG_CRTI, "adc_cali_cal[%d] = %d\n", i,
					    *(adc_cali_cal + i));
		battery_xlog_printk(BAT_LOG_FULL, "**** unlocked_ioctl : SET_ADC_CALI_Cal Done!\n");
		break;

	case ADC_CHANNEL_READ:
		/* g_ADC_Cali = KAL_FALSE; // 20100508 Infinity */
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);	/* 2*int = 2*4 */

		if (adc_in_data[0] == 0)	/* I_SENSE */
			adc_out_data[0] = battery_meter_get_VSense() * adc_in_data[1];
		else if (adc_in_data[0] == 1)	/* BAT_SENSE */
			adc_out_data[0] = battery_meter_get_battery_voltage() * adc_in_data[1];
		else if (adc_in_data[0] == 3)	/* V_Charger */
			adc_out_data[0] = battery_meter_get_charger_voltage() * adc_in_data[1];
		else if (adc_in_data[0] == 30)	/* V_Bat_temp magic number */
			adc_out_data[0] = battery_meter_get_battery_temperature() * adc_in_data[1];
		else if (adc_in_data[0] == 66) {
			adc_out_data[0] = (battery_meter_get_battery_current()) / 10;

			if (battery_meter_get_battery_current_sign() == KAL_TRUE)
				adc_out_data[0] = 0 - adc_out_data[0];	/* charging */

		} else {
			battery_xlog_printk(BAT_LOG_FULL, "unknown channel(%d,%d)%d\n",
					    adc_in_data[0], adc_in_data[1]);
		}

		if (adc_out_data[0] < 0)
			adc_out_data[1] = 1;	/* failed */
		else
			adc_out_data[1] = 0;	/* success */

		if (adc_in_data[0] == 30)
			adc_out_data[1] = 0;	/* success */

		if (adc_in_data[0] == 66)
			adc_out_data[1] = 0;	/* success */

		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		battery_xlog_printk(BAT_LOG_CRTI,
				    "**** unlocked_ioctl : Channel %d * %d times = %d\n",
				    adc_in_data[0], adc_in_data[1], adc_out_data[0]);
		break;

	case BAT_STATUS_READ:
		user_data_addr = (int *)arg;
		ret = copy_from_user(battery_in_data, user_data_addr, 4);
		/* [0] is_CAL */
		if (g_ADC_Cali)
			battery_out_data[0] = 1;
		else
			battery_out_data[0] = 0;

		ret = copy_to_user(user_data_addr, battery_out_data, 4);
		battery_xlog_printk(BAT_LOG_CRTI, "**** unlocked_ioctl : CAL:%d\n",
				    battery_out_data[0]);
		break;

	case Set_Charger_Current:	/* For Factory Mode */
		user_data_addr = (int *)arg;
		ret = copy_from_user(charging_level_data, user_data_addr, 4);
		g_ftm_battery_flag = KAL_TRUE;
		if (charging_level_data[0] == 0)
			charging_level_data[0] = CHARGE_CURRENT_70_00_MA;
		else if (charging_level_data[0] == 1)
			charging_level_data[0] = CHARGE_CURRENT_200_00_MA;
		else if (charging_level_data[0] == 2)
			charging_level_data[0] = CHARGE_CURRENT_400_00_MA;
		else if (charging_level_data[0] == 3)
			charging_level_data[0] = CHARGE_CURRENT_450_00_MA;
		else if (charging_level_data[0] == 4)
			charging_level_data[0] = CHARGE_CURRENT_550_00_MA;
		else if (charging_level_data[0] == 5)
			charging_level_data[0] = CHARGE_CURRENT_650_00_MA;
		else if (charging_level_data[0] == 6)
			charging_level_data[0] = CHARGE_CURRENT_700_00_MA;
		else if (charging_level_data[0] == 7)
			charging_level_data[0] = CHARGE_CURRENT_800_00_MA;
		else if (charging_level_data[0] == 8)
			charging_level_data[0] = CHARGE_CURRENT_900_00_MA;
		else if (charging_level_data[0] == 9)
			charging_level_data[0] = CHARGE_CURRENT_1000_00_MA;
		else if (charging_level_data[0] == 10)
			charging_level_data[0] = CHARGE_CURRENT_1100_00_MA;
		else if (charging_level_data[0] == 11)
			charging_level_data[0] = CHARGE_CURRENT_1200_00_MA;
		else if (charging_level_data[0] == 12)
			charging_level_data[0] = CHARGE_CURRENT_1300_00_MA;
		else if (charging_level_data[0] == 13)
			charging_level_data[0] = CHARGE_CURRENT_1400_00_MA;
		else if (charging_level_data[0] == 14)
			charging_level_data[0] = CHARGE_CURRENT_1500_00_MA;
		else if (charging_level_data[0] == 15)
			charging_level_data[0] = CHARGE_CURRENT_1600_00_MA;
		else
			charging_level_data[0] = CHARGE_CURRENT_450_00_MA;

		wake_up_bat();
		battery_xlog_printk(BAT_LOG_CRTI, "**** unlocked_ioctl : set_Charger_Current:%d\n",
				    charging_level_data[0]);
		break;
		/* add for meta tool------------------------------- */
	case Get_META_BAT_VOL:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = BMT_status.bat_vol;
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		break;
	case Get_META_BAT_SOC:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = BMT_status.UI_SOC;
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		break;
		/* add bing meta tool------------------------------- */

	default:
		g_ADC_Cali = KAL_FALSE;
		break;
	}

	mutex_unlock(&bat_mutex);

	return 0;
}

static int adc_cali_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int adc_cali_release(struct inode *inode, struct file *file)
{
	return 0;
}


static const struct file_operations adc_cali_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = adc_cali_ioctl,
	.open = adc_cali_open,
	.release = adc_cali_release,
};


void check_battery_exist(void)
{
#if defined(CONFIG_CONFIG_DIS_CHECK_BATTERY)
	battery_xlog_printk(BAT_LOG_FULL, "[BATTERY] Disable check battery exist.\n");
#else
	kal_uint32 baton_count = 0;
	kal_uint32 i;

	for (i = 0; i < 3; i++)
		baton_count += bat_charger_get_battery_status();

	if (baton_count >= 3) {
		if ((g_platform_boot_mode == META_BOOT) || (g_platform_boot_mode == ADVMETA_BOOT)
		    || (g_platform_boot_mode == ATE_FACTORY_BOOT)) {
			battery_xlog_printk(BAT_LOG_FULL,
					    "[BATTERY] boot mode = %d, bypass battery check\n",
					    g_platform_boot_mode);
		} else {
			battery_xlog_printk(BAT_LOG_FULL,
					    "[BATTERY] Battery is not exist, power off FAN5405 and system (%d)\n",
					    baton_count);

			bat_charger_enable(false);
			bat_charger_set_platform_reset();
		}
	}
#endif
}

int charger_hv_detect_sw_thread_handler(void *unused)
{
	int ret;
	ktime_t ktime;

	do {
		ktime = ktime_set(0, BAT_MS_TO_NS(2000));

		bat_charger_set_hv_threshold(BATTERY_VOLT_07_000000_V);

		ret = wait_event_interruptible_hrtimeout(charger_hv_detect_waiter,
					 atomic_read(&charger_hv_detect), ktime);
		if (ret != -ETIME)
			atomic_dec(&charger_hv_detect);

		pr_debug("%s: %s\n", __func__, ret == -ETIME ? "timeout" : "event");

		if (upmu_is_chr_det())
			check_battery_exist();

		if (bat_charger_get_hv_status()) {
			battery_xlog_printk(BAT_LOG_CRTI,
					    "[charger_hv_detect_sw_thread_handler] charger hv\n");

			bat_charger_enable(false);
		} else {
			battery_xlog_printk(BAT_LOG_FULL,
					    "[charger_hv_detect_sw_thread_handler] upmu_chr_get_vcdt_hv_det() != 1\n");
		}

		if (!fg_battery_shutdown)
			bat_charger_reset_watchdog_timer();

	} while (!kthread_should_stop());

	return 0;
}

static void wake_up_hv_sw_thread(void)
{
	atomic_inc(&charger_hv_detect);
	wake_up_interruptible(&charger_hv_detect_waiter);
}

void charger_hv_detect_sw_workaround_init(void)
{
	charger_hv_detect_thread =
	    kthread_run(charger_hv_detect_sw_thread_handler, 0,
			"mtk charger_hv_detect_sw_workaround");
	if (IS_ERR(charger_hv_detect_thread)) {
		battery_xlog_printk(BAT_LOG_FULL,
				    "[%s]: failed to create charger_hv_detect_sw_workaround thread\n",
				    __func__);
	}

	battery_xlog_printk(BAT_LOG_CRTI, "charger_hv_detect_sw_workaround_init : done\n");
}

// redo charger detection
static int slimport_charging_notifier_callback(struct notifier_block *nb, unsigned long val, void *data)
{
	mutex_lock(&charger_type_mutex);
	BMT_status.charger_type = bat_charger_get_charger_type();
	mutex_unlock(&charger_type_mutex);
	return 0;
}

static struct notifier_block charger_notifier_block = {
	.notifier_call = slimport_charging_notifier_callback,
};

static int battery_probe(struct platform_device *pdev)
{
	struct class_device *class_dev = NULL;
	struct device *dev = &pdev->dev;
	int ret = 0;

	pr_info("******** battery driver probe!! ********\n");

	/* AP:
	 * Use PMIC events as interrupts through kernel IRQ API.
	 */
	atomic_set(&bat_thread_wakeup, 0);
	atomic_set(&charger_hv_detect, 0);
	g_bat.irq = PMIC_IRQ(RG_INT_STATUS_CHRDET);
	p_bat_charging_data =
		(struct mt_battery_charging_custom_data *) dev_get_platdata(dev);
	if (!p_bat_charging_data) {
		pr_err("%s: platform data is missing\n", __func__);
		return -EINVAL;
	}

	/*as per MTK, we can't enable interrupt, before we call BAT_thread()
	 * for the first time. */
	irq_set_status_flags(g_bat.irq, IRQ_NOAUTOEN);
	ret = request_threaded_irq(g_bat.irq, NULL,
		ops_chrdet_int_handler,
		IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "ops_mt6397_chrdet", pdev);
	if (ret) {
		pr_err("%s: request_threaded_irq err = %d\n", __func__, ret);
		return ret;
	}
	ret = irq_set_irq_wake(g_bat.irq, true);
	if (ret) {
		pr_err("%s: irq_set_irq_wake err = %d\n", __func__, ret);
		free_irq(g_bat.irq, pdev);
		return ret;
	}
	/* AP:
	 * Now, proceed with unmodified MTK code, which does not return errors,
	 * when it fails.
	 */

	/* Integrate with NVRAM */
	ret = alloc_chrdev_region(&adc_cali_devno, 0, 1, ADC_CALI_DEVNAME);
	if (ret)
		battery_xlog_printk(BAT_LOG_CRTI, "Error: Can't Get Major number for adc_cali\n");
	adc_cali_cdev = cdev_alloc();
	adc_cali_cdev->owner = THIS_MODULE;
	adc_cali_cdev->ops = &adc_cali_fops;
	ret = cdev_add(adc_cali_cdev, adc_cali_devno, 1);
	if (ret)
		battery_xlog_printk(BAT_LOG_CRTI, "adc_cali Error: cdev_add\n");
	adc_cali_major = MAJOR(adc_cali_devno);
	adc_cali_class = class_create(THIS_MODULE, ADC_CALI_DEVNAME);
	class_dev = (struct class_device *)device_create(adc_cali_class,
							 NULL,
							 adc_cali_devno, NULL, ADC_CALI_DEVNAME);
	battery_xlog_printk(BAT_LOG_CRTI, "[BAT_probe] adc_cali prepare : done !!\n ");

	wake_lock_init(&battery_suspend_lock, WAKE_LOCK_SUSPEND, "battery suspend wakelock");

	/* Integrate with Android Battery Service */
	ret = power_supply_register(dev, &ac_main.psy);
	if (ret) {
		battery_xlog_printk(BAT_LOG_CRTI, "[BAT_probe] power_supply_register AC Fail !!\n");
		return ret;
	}
	battery_xlog_printk(BAT_LOG_CRTI, "[BAT_probe] power_supply_register AC Success !!\n");

	ret = power_supply_register(dev, &usb_main.psy);
	if (ret) {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[BAT_probe] power_supply_register USB Fail !!\n");
		return ret;
	}
	battery_xlog_printk(BAT_LOG_CRTI, "[BAT_probe] power_supply_register USB Success !!\n");

	ret = power_supply_register(dev, &battery_main.psy);
	if (ret) {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[BAT_probe] power_supply_register Battery Fail !!\n");
		return ret;
	}
	battery_xlog_printk(BAT_LOG_CRTI, "[BAT_probe] power_supply_register Battery Success !!\n");

	ret = charger_notifier_register(&charger_notifier_block);
	if (ret) {
		printk(KERN_ERR "registering charger notifier block failed!\n");
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[BAT_probe] registering charger notifier block failed !!\n");
		return ret;
	}
#if !defined(CONFIG_POWER_EXT)
	/* For EM */
	{
		int ret_device_file = 0;

		ret_device_file = device_create_file(dev, &dev_attr_ADC_Charger_Voltage);

		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_0_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_1_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_2_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_3_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_4_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_5_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_6_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_7_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_8_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_9_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_10_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_11_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_12_Slope);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_13_Slope);

		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_0_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_1_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_2_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_3_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_4_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_5_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_6_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_7_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_8_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_9_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_10_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_11_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_12_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_13_Offset);
		ret_device_file = device_create_file(dev, &dev_attr_ADC_Channel_Is_Calibration);

		ret_device_file = device_create_file(dev, &dev_attr_Power_On_Voltage);
		ret_device_file = device_create_file(dev, &dev_attr_Power_Off_Voltage);
		ret_device_file = device_create_file(dev, &dev_attr_Charger_TopOff_Value);
		ret_device_file = device_create_file(dev, &dev_attr_FG_Battery_CurrentConsumption);
		ret_device_file = device_create_file(dev, &dev_attr_FG_SW_CoulombCounter);
		ret_device_file = device_create_file(dev, &dev_attr_Charging_CallState);
		ret_device_file = device_create_file(dev, &dev_attr_Charging_Enable);
		ret_device_file = device_create_file(dev, &dev_attr_Custom_Charging_Current);
	}

	/* battery_meter_initial();      //move to mt_battery_GetBatteryData() to decrease booting time */

	/* Initialization BMT Struct */
	BMT_status.bat_exist = KAL_TRUE;	/* phone must have battery */
	BMT_status.charger_exist = KAL_FALSE;	/* for default, no charger */
	BMT_status.bat_vol = 0;
	BMT_status.ICharging = 0;
	BMT_status.temperature = 0;
	BMT_status.charger_vol = 0;
	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;
	BMT_status.SOC = 0;
	BMT_status.UI_SOC = -1;

	BMT_status.bat_charging_state = CHR_PRE;
	BMT_status.bat_in_recharging_state = KAL_FALSE;
	BMT_status.bat_full = KAL_FALSE;
	BMT_status.nPercent_ZCV = 0;
	BMT_status.nPrecent_UI_SOC_check_point = battery_meter_get_battery_nPercent_UI_SOC();

	kthread_run(bat_thread_kthread, NULL, "bat_thread_kthread");
	battery_xlog_printk(BAT_LOG_CRTI, "[battery_probe] bat_thread_kthread Done\n");

	charger_hv_detect_sw_workaround_init();

	/*LOG System Set */
	init_proc_log();

#endif
	g_bat_init_flag = KAL_TRUE;

#if defined(CONFIG_AMAZON_METRICS_LOG)
	metrics_init();
#endif

	mutex_lock(&bat_mutex);
	g_bat.common_init_done = true;
	bat_setup_charger_locked();
	mutex_unlock(&bat_mutex);

	return 0;
}


static int battery_remove(struct platform_device *dev)
{
#if defined(CONFIG_AMAZON_METRICS_LOG)
	metrics_uninit();
#endif
	charger_notifier_unregister(&charger_notifier_block);

	battery_xlog_printk(BAT_LOG_CRTI, "******** battery driver remove!! ********\n");

	return 0;
}

static void battery_shutdown(struct platform_device *pdev)
{
	int count = 0;
	pr_info("******** battery driver shutdown!! ********\n");

	disable_irq(g_bat.irq);

	mutex_lock(&bat_mutex);
	fg_battery_shutdown = true;
	wake_up_bat_update_meter();

	/* turn down workaround thread */
	if (charger_hv_detect_thread) {
		kthread_stop(charger_hv_detect_thread);
		wake_up_hv_sw_thread();
	}

	while (!g_bat.down && count < 5) {
		mutex_unlock(&bat_mutex);
		msleep(20);
		mutex_lock(&bat_mutex);
	}

	if (!g_bat.down)
		pr_err("%s: failed to terminate battery thread\n", __func__);

	/* turn down interrupt thread and wakeup ability */
	irq_set_irq_wake(g_bat.irq, false);
	free_irq(g_bat.irq, pdev);

	mutex_unlock(&bat_mutex);
}

static int battery_suspend(struct platform_device *dev, pm_message_t state)
{
	disable_irq(g_bat.irq);

	mutex_lock(&bat_mutex);
	battery_suspended = KAL_TRUE;
	mutex_unlock(&bat_mutex);

#if defined(CONFIG_AMAZON_METRICS_LOG)
	metrics_suspend();
#endif
	return 0;
}

static int battery_resume(struct platform_device *dev)
{
	battery_suspended = KAL_FALSE;
	g_refresh_ui_soc = KAL_TRUE;
	if (bat_charger_is_pcm_timer_trigger())
		wake_up_bat_update_meter();
#if defined(CONFIG_AMAZON_METRICS_LOG)
	metrics_resume();
#endif

	enable_irq(g_bat.irq);
	return 0;
}


/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Battery Notify API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_BatteryNotify(struct device *dev, struct device_attribute *attr, char *buf)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[Battery] show_BatteryNotify : %x\n",
			    g_BatteryNotifyCode);

	return sprintf(buf, "%u\n", g_BatteryNotifyCode);
}

static ssize_t store_BatteryNotify(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t size)
{
	char *pvalue = NULL;
	unsigned int reg_BatteryNotifyCode = 0;
	battery_xlog_printk(BAT_LOG_CRTI, "[Battery] store_BatteryNotify\n");
	if (buf != NULL && size != 0) {
		battery_xlog_printk(BAT_LOG_CRTI, "[Battery] buf is %s and size is %d\n", buf,
				    size);
		reg_BatteryNotifyCode = simple_strtoul(buf, &pvalue, 16);
		g_BatteryNotifyCode = reg_BatteryNotifyCode;
		battery_xlog_printk(BAT_LOG_CRTI, "[Battery] store code : %x\n",
				    g_BatteryNotifyCode);
	}
	return size;
}

static DEVICE_ATTR(BatteryNotify, 0664, show_BatteryNotify, store_BatteryNotify);

static ssize_t show_BN_TestMode(struct device *dev, struct device_attribute *attr, char *buf)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[Battery] show_BN_TestMode : %x\n", g_BN_TestMode);
	return sprintf(buf, "%u\n", g_BN_TestMode);
}

static ssize_t store_BN_TestMode(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	char *pvalue = NULL;
	unsigned int reg_BN_TestMode = 0;
	battery_xlog_printk(BAT_LOG_CRTI, "[Battery] store_BN_TestMode\n");
	if (buf != NULL && size != 0) {
		battery_xlog_printk(BAT_LOG_CRTI, "[Battery] buf is %s and size is %d\n", buf,
				    size);
		reg_BN_TestMode = simple_strtoul(buf, &pvalue, 16);
		g_BN_TestMode = reg_BN_TestMode;
		battery_xlog_printk(BAT_LOG_CRTI, "[Battery] store g_BN_TestMode : %x\n",
				    g_BN_TestMode);
	}
	return size;
}

static DEVICE_ATTR(BN_TestMode, 0664, show_BN_TestMode, store_BN_TestMode);


/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // platform_driver API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
#if 0
static int battery_cmd_read(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	char *p = buf;

	p += sprintf(p,
		     "g_battery_thermal_throttling_flag=%d,\nbattery_cmd_thermal_test_mode=%d,\nbattery_cmd_thermal_test_mode_value=%d\n",
		     g_battery_thermal_throttling_flag, battery_cmd_thermal_test_mode,
		     battery_cmd_thermal_test_mode_value);

	*start = buf + off;

	len = p - buf;
	if (len > off)
		len -= off;
	else
		len = 0;

	return len < count ? len : count;
}
#endif

static ssize_t battery_cmd_write(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	int len = 0, bat_tt_enable = 0, bat_thr_test_mode = 0, bat_thr_test_value = 0;
	char desc[32];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d %d", &bat_tt_enable, &bat_thr_test_mode, &bat_thr_test_value) == 3) {
		g_battery_thermal_throttling_flag = bat_tt_enable;
		battery_cmd_thermal_test_mode = bat_thr_test_mode;
		battery_cmd_thermal_test_mode_value = bat_thr_test_value;

		battery_xlog_printk(BAT_LOG_CRTI,
				    "bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
				    g_battery_thermal_throttling_flag,
				    battery_cmd_thermal_test_mode,
				    battery_cmd_thermal_test_mode_value);

		return count;
	} else {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "  bad argument, echo [bat_tt_enable] [bat_thr_test_mode] [bat_thr_test_value] > battery_cmd\n");
	}

	return -EINVAL;
}

static int proc_utilization_show(struct seq_file *m, void *v)
{
	seq_printf(m,
		   "=> g_battery_thermal_throttling_flag=%d,\nbattery_cmd_thermal_test_mode=%d,\nbattery_cmd_thermal_test_mode_value=%d\n",
		   g_battery_thermal_throttling_flag, battery_cmd_thermal_test_mode,
		   battery_cmd_thermal_test_mode_value);
	return 0;
}

static int proc_utilization_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_utilization_show, NULL);
}

static const struct file_operations battery_cmd_proc_fops = {
	.open = proc_utilization_open,
	.read = seq_read,
	.write = battery_cmd_write,
};

static int mt_batteryNotify_probe(struct platform_device *pdev)
{
#if defined(CONFIG_POWER_EXT)
#else
	struct device *dev = &pdev->dev;
	int ret_device_file = 0;
	/* struct proc_dir_entry *entry = NULL; */
	struct proc_dir_entry *battery_dir = NULL;

	battery_xlog_printk(BAT_LOG_CRTI, "******** mt_batteryNotify_probe!! ********\n");


	ret_device_file = device_create_file(dev, &dev_attr_BatteryNotify);
	ret_device_file = device_create_file(dev, &dev_attr_BN_TestMode);

	battery_dir = proc_mkdir("mtk_battery_cmd", NULL);
	if (!battery_dir) {
		pr_err("[%s]: mkdir /proc/mtk_battery_cmd failed\n", __func__);
	} else {
#if 1
		proc_create("battery_cmd", S_IRUGO | S_IWUSR, battery_dir, &battery_cmd_proc_fops);
		battery_xlog_printk(BAT_LOG_CRTI, "proc_create battery_cmd_proc_fops\n");
#else
		entry = create_proc_entry("battery_cmd", S_IRUGO | S_IWUSR, battery_dir);
		if (entry) {
			entry->read_proc = battery_cmd_read;
			entry->write_proc = battery_cmd_write;
		}
#endif
	}

	battery_xlog_printk(BAT_LOG_CRTI, "******** mtk_battery_cmd!! ********\n");
#endif
	return 0;

}

#if 0				/* move to board-common-battery.c */
struct platform_device battery_device = {
	.name = "battery",
	.id = -1,
};
#endif

static struct platform_driver battery_driver = {
	.probe = battery_probe,
	.remove = battery_remove,
	.shutdown = battery_shutdown,
	/* #ifdef CONFIG_PM */
	.suspend = battery_suspend,
	.resume = battery_resume,
	/* #endif */
	.driver = {
		   .name = "battery",
		   },
};

struct platform_device MT_batteryNotify_device = {
	.name = "mt-battery",
	.id = -1,
};

static struct platform_driver mt_batteryNotify_driver = {
	.probe = mt_batteryNotify_probe,
	.driver = {
		   .name = "mt-battery",
		   },
};

static int __init battery_init(void)
{
	int ret;

#if 0				/* move to board-common-battery.c */
	ret = platform_device_register(&battery_device);
	if (ret) {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "****[battery_driver] Unable to device register(%d)\n", ret);
		return ret;
	}
#endif

	ret = platform_driver_register(&battery_driver);
	if (ret) {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "****[battery_driver] Unable to register driver (%d)\n", ret);
		return ret;
	}
	/* battery notofy UI */
	ret = platform_device_register(&MT_batteryNotify_device);
	if (ret) {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "****[mt_batteryNotify] Unable to device register(%d)\n", ret);
		return ret;
	}
	ret = platform_driver_register(&mt_batteryNotify_driver);
	if (ret) {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "****[mt_batteryNotify] Unable to register driver (%d)\n", ret);
		return ret;
	}

	battery_xlog_printk(BAT_LOG_CRTI, "****[battery_driver] Initialization : DONE !!\n");
	return 0;
}

static void __exit battery_exit(void)
{
}
module_init(battery_init);
module_exit(battery_exit);

MODULE_AUTHOR("Oscar Liu");
MODULE_DESCRIPTION("Battery Device Driver");
MODULE_LICENSE("GPL");

#include <linux/kernel.h>
#include <linux/string.h>

#include <soc/qcom/lge/board_lge.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/platform_device.h>

#ifdef CONFIG_LGE_PM_USB_ID
#include <linux/err.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/power_supply.h>
#endif

#ifdef CONFIG_USB_G_LGE_ANDROID
#include <linux/platform_data/lge_android_usb.h>
#endif

#ifdef CONFIG_LGE_EARJACK_DEBUGGER
#include <soc/qcom/lge/board_lge.h>
#endif

#ifdef CONFIG_LGE_LCD_TUNING
#include "../drivers/video/msm/mdss/mdss_dsi.h"
int tun_lcd[128];

int lcd_set_values(int *tun_lcd_t)
{
	memset(tun_lcd,0,128*sizeof(int));
	memcpy(tun_lcd,tun_lcd_t,128*sizeof(int));
	printk("lcd_set_values ::: tun_lcd[0]=[%x], tun_lcd[1]=[%x], tun_lcd[2]=[%x] ......\n"
		,tun_lcd[0],tun_lcd[1],tun_lcd[2]);
	return 0;
}
static int lcd_get_values(int *tun_lcd_t)
{
	memset(tun_lcd_t,0,128*sizeof(int));
	memcpy(tun_lcd_t,tun_lcd,128*sizeof(int));
	printk("lcd_get_values\n");
	return 0;
}

static struct lcd_platform_data lcd_pdata ={
	.set_values = lcd_set_values,
	.get_values = lcd_get_values,
};
static struct platform_device lcd_ctrl_device = {
	.name = "lcd_ctrl",
	.dev = {
	.platform_data = &lcd_pdata,
	}
};

static int __init lge_add_lcd_ctrl_devices(void)
{
	return platform_device_register(&lcd_ctrl_device);
}
arch_initcall(lge_add_lcd_ctrl_devices);
#endif

#ifdef CONFIG_LGE_PM_USB_ID
struct chg_cable_info_table {
	int threshhold;
	enum acc_cable_type type;
	unsigned ta_ma;
	unsigned usb_ma;
};

#define ADC_NO_INIT_CABLE   0
#define C_NO_INIT_TA_MA     0
#define C_NO_INIT_USB_MA    0
#define ADC_CABLE_NONE      1900000
#define C_NONE_TA_MA        700
#define C_NONE_USB_MA       500

#define MAX_CABLE_NUM		15
static bool cable_type_defined;
static struct chg_cable_info_table lge_acc_cable_type_data[MAX_CABLE_NUM];
#endif

#ifdef CONFIG_LGE_KSWITCH
static int s_kswitch_flag;
#endif

#if defined(CONFIG_LGE_MIPI_JDI_INCELL_QHD_CMD_PANEL)
static char dsv_vendor[3];
int jdi_cut;
#endif
static enum hw_rev_type lge_bd_rev = HW_REV_MAX;
static int lge_fake_battery;

/* CAUTION: These strings are come from LK. */
char *rev_str[] = {"rev_0", "rev_a", "rev_b", "rev_c", "rev_d",
	"rev_e", "rev_f", "rev_g", "rev_10", "rev_11", "rev_12", "reserved"};
extern unsigned int system_rev;

static int __init board_revno_setup(char *rev_info)
{
	int i;

	for (i = 0; i < HW_REV_MAX; i++) {
		if (!strncmp(rev_info, rev_str[i], 6)) {
			lge_bd_rev = i;
			system_rev = lge_bd_rev;
			break;
		}
	}

	pr_info("BOARD : LGE %s\n", rev_str[lge_bd_rev]);
	return 1;
}
__setup("lge.rev=", board_revno_setup);

enum hw_rev_type lge_get_board_revno(void)
{
	return lge_bd_rev;
}

static int __init fake_battery_setup(char *fake_battery_info)
{
	if (!strncmp(fake_battery_info, "enable", 6))
		lge_fake_battery = 1;
	return 1;
}
__setup("fakebattery=", fake_battery_setup);

int lge_get_fake_battery(void)
{
	return lge_fake_battery;
}

/* get boot mode information from cmdline.
 * If any boot mode is not specified,
 * boot mode is normal type.
 */
static enum lge_boot_mode_type lge_boot_mode = LGE_BOOT_MODE_NORMAL;
int __init lge_boot_mode_init(char *s)
{
	if (!strcmp(s, "charger"))
		lge_boot_mode = LGE_BOOT_MODE_CHARGER;
	else if (!strcmp(s, "chargerlogo"))
		lge_boot_mode = LGE_BOOT_MODE_CHARGERLOGO;
	else if (!strcmp(s, "qem_56k"))
		lge_boot_mode = LGE_BOOT_MODE_QEM_56K;
	else if (!strcmp(s, "qem_130k"))
		lge_boot_mode = LGE_BOOT_MODE_QEM_130K;
	else if (!strcmp(s, "qem_910k"))
		lge_boot_mode = LGE_BOOT_MODE_QEM_910K;
	else if (!strcmp(s, "pif_56k"))
		lge_boot_mode = LGE_BOOT_MODE_PIF_56K;
	else if (!strcmp(s, "pif_130k"))
		lge_boot_mode = LGE_BOOT_MODE_PIF_130K;
	else if (!strcmp(s, "pif_910k"))
		lge_boot_mode = LGE_BOOT_MODE_PIF_910K;
	/*                            */
	else if (!strcmp(s, "miniOS"))
		lge_boot_mode = LGE_BOOT_MODE_MINIOS;
	pr_info("ANDROID BOOT MODE : %d %s\n", lge_boot_mode, s);
	/*                            */

	return 1;
}
__setup("androidboot.mode=", lge_boot_mode_init);

enum lge_boot_mode_type lge_get_boot_mode(void)
{
	return lge_boot_mode;
}

int lge_get_factory_boot(void)
{
	int res;

	/*   if boot mode is factory,
	 *   cable must be factory cable.
	 */
	switch (lge_boot_mode) {
		case LGE_BOOT_MODE_QEM_56K:
		case LGE_BOOT_MODE_QEM_130K:
		case LGE_BOOT_MODE_QEM_910K:
		case LGE_BOOT_MODE_PIF_56K:
		case LGE_BOOT_MODE_PIF_130K:
		case LGE_BOOT_MODE_PIF_910K:
		case LGE_BOOT_MODE_MINIOS:
			res = 1;
			break;
		default:
			res = 0;
			break;
	}
	return res;
}

int lge_get_minios(void)
{
	int res = 0;
	switch (lge_boot_mode) {
	case LGE_BOOT_MODE_QEM_56K:
	case LGE_BOOT_MODE_QEM_130K:
	case LGE_BOOT_MODE_MINIOS:
		res = 1;
		break;
	default:
		res = 0;
		break;
	}
	return res;
}

#ifdef CONFIG_LGE_PM_USB_ID
#define ADC_MINIOS_910K_MARGIN    140000
void get_cable_data_from_dt(void *of_node)
{
	int i;
	u32 cable_value[3];
	struct device_node *node_temp = (struct device_node *)of_node;
	const char *propname[MAX_CABLE_NUM] = {
		"lge,no-init-cable",
		"lge,cable-mhl-1k",
		"lge,cable-u-28p7k",
		"lge,cable-28p7k",
		"lge,cable-56k",
		"lge,cable-100k",
		"lge,cable-130k",
		"lge,cable-180k",
		"lge,cable-200k",
		"lge,cable-220k",
		"lge,cable-270k",
		"lge,cable-330k",
		"lge,cable-620k",
		"lge,cable-910k",
		"lge,cable-none"
	};
	if (cable_type_defined) {
		pr_info("Cable type is already defined\n");
		return;
	}


	for (i = 0; i < MAX_CABLE_NUM; i++) {
		of_property_read_u32_array(node_temp, propname[i],
				cable_value, 3);
		lge_acc_cable_type_data[i].threshhold =
			(!strcmp(propname[i], "lge,cable-620k")) ?
			lge_get_minios() ? (cable_value[0]-ADC_MINIOS_910K_MARGIN) : cable_value[0]
			: cable_value[0];
		lge_acc_cable_type_data[i].type = i;
		lge_acc_cable_type_data[i].ta_ma = cable_value[1];
		lge_acc_cable_type_data[i].usb_ma = cable_value[2];
	}
	cable_type_defined = 1;
}

int lge_pm_get_cable_info(struct qpnp_vadc_chip *vadc,
		struct chg_cable_info *cable_info)
{
	char *type_str[] = {
		"NOT INIT", "MHL 1K", "U_28P7K", "28P7K", "56K",
		"100K", "130K", "180K", "200K", "220K",
		"270K", "330K", "620K", "910K", "OPEN"
	};

	struct qpnp_vadc_result result;
	struct chg_cable_info *info = cable_info;
	struct chg_cable_info_table *table;
	int table_size = ARRAY_SIZE(lge_acc_cable_type_data);
	int acc_read_value = 0;
	int i, rc;
	int count = 1;

	if (!info) {
		pr_err("%s : invalid info parameters\n", __func__);
		return -EINVAL;
	}

	if (!vadc) {
		pr_err("%s : invalid vadc parameters\n", __func__);
		return -EINVAL;
	}

	if (!cable_type_defined) {
		pr_err("%s : cable type is not defined yet.\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		rc = qpnp_vadc_read(vadc, LR_MUX10_USB_ID_LV, &result);

		if (rc < 0) {
			if (rc == -ETIMEDOUT) {
				/* reason: adc read timeout,
				 * assume it is open cable
				 */
				info->cable_type = CABLE_NONE;
				info->ta_ma = C_NONE_TA_MA;
				info->usb_ma = C_NONE_USB_MA;
			}
			pr_err("%s : adc read error - %d\n", __func__, rc);
			return rc;
		}

		acc_read_value = (int)result.physical;
		pr_info("%s : adc_read-%d\n", __func__, (int)result.physical);
		/* mdelay(10); */
	}

	info->cable_type = NO_INIT_CABLE;
	info->ta_ma = C_NO_INIT_TA_MA;
	info->usb_ma = C_NO_INIT_USB_MA;

	/* assume: adc value must be existed in ascending order */
	for (i = 0; i < table_size; i++) {
		table = &lge_acc_cable_type_data[i];

		if (acc_read_value <= table->threshhold) {
			info->cable_type = table->type;
			info->ta_ma = table->ta_ma;
			info->usb_ma = table->usb_ma;
			break;
		}
	}

	pr_err("\n\n[PM]Cable detected: %d(%s)(%d, %d)\n\n",
			acc_read_value, type_str[info->cable_type],
			info->ta_ma, info->usb_ma);

	return 0;
}

/* Belows are for using in interrupt context */
static struct chg_cable_info lge_cable_info;

enum acc_cable_type lge_pm_get_cable_type(void)
{
	return lge_cable_info.cable_type;
}

unsigned lge_pm_get_ta_current(void)
{
	return lge_cable_info.ta_ma;
}

unsigned lge_pm_get_usb_current(void)
{
	return lge_cable_info.usb_ma;
}

/* This must be invoked in process context */
void lge_pm_read_cable_info(struct qpnp_vadc_chip *vadc)
{
	lge_cable_info.cable_type = NO_INIT_CABLE;
	lge_cable_info.ta_ma = C_NO_INIT_TA_MA;
	lge_cable_info.usb_ma = C_NO_INIT_USB_MA;

	lge_pm_get_cable_info(vadc, &lge_cable_info);
}
#endif

#ifdef CONFIG_LGE_EARJACK_DEBUGGER
/* s_uart_console_status bits format
 * ------higher than bit4 are not used
 * bit5...: not used
 * ------bit4 indicates whenter uart console was ready(probed)
 * bit4: [UART_CONSOLE_READY]
 * ------current uart console status -----------------
 * bit3: [UART_CONSOLE_ENABLED]
 * ------configuration bit field -----------------
 * bit2: [UART_CONSOLE_ENABLE_ON_DEFAULT]
 * bit1; [UART_CONSOLE_ENABLE_ON_EARJACK_DEBUGGER]
 * bit0: [UART_CONSOLE_ENABLE_ON_EARJACK]
 */
static unsigned int s_uart_console_status = 0;	/* disabling uart console */

unsigned int lge_uart_console_get_config(void)
{
	return (s_uart_console_status & UART_CONSOLE_MASK_CONFIG);
}

void lge_uart_console_set_config(unsigned int config)
{
	config &= UART_CONSOLE_MASK_CONFIG;
	s_uart_console_status |= config;
}

unsigned int lge_uart_console_get_enabled(void)
{
	return s_uart_console_status & UART_CONSOLE_MASK_ENABLED;
}

void lge_uart_console_set_enabled(int enabled)
{
#ifdef CONFIG_LGE_KSWITCH
	if ((s_kswitch_flag & LGE_KSWITCH_DISABLE_UART))
		enabled = 0;
#endif

	s_uart_console_status &= ~UART_CONSOLE_MASK_ENABLED;
	/* for caller conding convenience, regard no-zero as enabled also */
	s_uart_console_status |= (enabled ? UART_CONSOLE_ENABLED : 0);
}

unsigned int lge_uart_console_get_ready(void)
{
	return s_uart_console_status & UART_CONSOLE_MASK_READY;
}

void lge_uart_console_set_ready(unsigned int ready)
{
	s_uart_console_status &= ~UART_CONSOLE_MASK_READY;
	/* for caller side coding convenience, regard no-zero as ready also */
	s_uart_console_status |= (ready ? UART_CONSOLE_READY : 0);
}

static int __init lge_uart_mode(char *uart_mode)
{
	pr_info("[UART CONSOLE][%s] begin with bootcmd=%s\n", __func__, uart_mode);

	if (!strncmp("enable", uart_mode, 6)) {
		/* NOTE : if you want to disable uart console also when earjack is inserted,
		 * remove UART_CONSOLE_ENABLE_ON_EARJACK flag.
		 */
		lge_uart_console_set_config(UART_CONSOLE_ENABLE_ON_EARJACK_DEBUGGER | UART_CONSOLE_ENABLE_ON_DEFAULT | UART_CONSOLE_ENABLE_ON_EARJACK);
	} else if (!strncmp("detected", uart_mode, 8)) {
		lge_uart_console_set_config(UART_CONSOLE_ENABLE_ON_EARJACK_DEBUGGER | UART_CONSOLE_ENABLE_ON_DEFAULT);
		lge_uart_console_set_ready(UART_CONSOLE_READY);
	} else {
		/* Default: disable uart log if earjack debugger feature is enabled.
		 * On user version, remove comment if you want to enable uart log whenever earjack debugger is inserted.
		 * Enabling uart log enabling by earjack debugger It is usefull during developing stage.
		 * Becase uart log should be disabled on mass product version, it is disabled by default.
		 */
		/* lge_uart_console_set_config(UART_CONSOLE_ENABLE_ON_EARJACK_DEBUGGER); */
	}

	lge_uart_console_set_enabled(lge_uart_console_should_enable_on_default() ? UART_CONSOLE_ENABLED : 0);

	pr_info (KERN_INFO "[UART CONSOLE] status=0x%x, enable_on=%s|%s|%s, enabled=%s, ready=%s\n"
						, s_uart_console_status
						, (lge_uart_console_should_enable_on_default()) ? "default" : ""
						, (lge_uart_console_should_enable_on_earjack()) ? "earjack" : ""
						, (lge_uart_console_should_enable_on_earjack_debugger()) ? "earjack_debugger" : ""
						, lge_uart_console_get_enabled() ? "enabled" : "disabled"
						, lge_uart_console_get_ready() ? "ready" : "not-ready");

	return 1;
}

__setup("lge.uart=", lge_uart_mode);
#endif /*                             */

#if defined(CONFIG_LGE_MIPI_JDI_INCELL_QHD_CMD_PANEL)
static int __init display_dsv_setup(char *dsv_cmd)
{
	sscanf(dsv_cmd, "%s", dsv_vendor);
	pr_info("dsv vendor id is %s\n", dsv_vendor);

	return 1;
}
__setup("lge.dsv_id=", display_dsv_setup);

char* lge_get_dsv_vendor(void)
{
     return dsv_vendor;
}

static int __init display_jdi_setup(char *jdi_cmd)
{
	sscanf(jdi_cmd, "%d", &jdi_cut);
	pr_info("jdi panel cut is %d\n", jdi_cut);

	return 1;
}
__setup("lge.jdi_cut=", display_jdi_setup);

int lge_get_jdi_cut(void)
{
	return jdi_cut;
}

#endif
/*
   for download complete using LAF image
   return value : 1 --> right after laf complete & reset
 */

int android_dlcomplete = 0;

int __init lge_android_dlcomplete(char *s)
{
	if(strncmp(s,"1",1) == 0)
		android_dlcomplete = 1;
	else
		android_dlcomplete = 0;
	printk("androidboot.dlcomplete = %d\n", android_dlcomplete);

	return 1;
}
__setup("androidboot.dlcomplete=", lge_android_dlcomplete);

int lge_get_android_dlcomplete(void)
{
	return android_dlcomplete;
}

#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
struct pseudo_batt_info_type pseudo_batt_info = {
	.mode = 0,
};

int safety_timer = 1;

void pseudo_batt_set(struct pseudo_batt_info_type *info)
{
	struct power_supply *batt_psy;
	union power_supply_propval ret = {0,};

	batt_psy = power_supply_get_by_name("battery");

	if (!batt_psy) {
		pr_err("called before init\n");
		return;
	}

	pseudo_batt_info.mode = info->mode;
	pseudo_batt_info.id = info->id;
	pseudo_batt_info.therm = info->therm;
	pseudo_batt_info.temp = info->temp;
	pseudo_batt_info.volt = info->volt;
	pseudo_batt_info.capacity = info->capacity;
	pseudo_batt_info.charging = info->charging;

	pr_err("pseudo batt set success\n");
	ret.intval = !pseudo_batt_info.mode;
	batt_psy->set_property(batt_psy, POWER_SUPPLY_PROP_SAFTETY_CHARGER_TIMER, &ret);
	power_supply_changed(batt_psy);
}
#endif

static enum lge_laf_mode_type lge_laf_mode = LGE_LAF_MODE_NORMAL;

int __init lge_laf_mode_init(char *s)
{
	if (strcmp(s, ""))
		lge_laf_mode = LGE_LAF_MODE_LAF;

	return 1;
}
__setup("androidboot.laf=", lge_laf_mode_init);

enum lge_laf_mode_type lge_get_laf_mode(void)
{
	return lge_laf_mode;
}

#ifdef CONFIG_USB_G_LGE_ANDROID
int get_factory_cable(void)
{
	int res = 0;

	/* if boot mode is factory, cable must be factory cable. */
	switch (lge_boot_mode) {
	case LGE_BOOT_MODE_QEM_56K:
	case LGE_BOOT_MODE_PIF_56K:
		res = LGEUSB_FACTORY_56K;
		break;

	case LGE_BOOT_MODE_QEM_130K:
	case LGE_BOOT_MODE_PIF_130K:
		res = LGEUSB_FACTORY_130K;
		break;

	case LGE_BOOT_MODE_QEM_910K:
	case LGE_BOOT_MODE_PIF_910K:
		res = LGEUSB_FACTORY_910K;
		break;

	default:
		res = 0;
		break;
	}

	return res;
}

struct lge_android_usb_platform_data lge_android_usb_pdata = {
	.vendor_id = 0x1004,
	.factory_pid = 0x6000,
	.iSerialNumber = 0,
	.product_name = "LGE Android Phone",
	.manufacturer_name = "LG Electronics Inc.",
	.factory_composition = "acm,diag",
	.get_factory_cable = get_factory_cable,
};

static struct platform_device lge_android_usb_device = {
	.name = "lge_android_usb",
	.id = -1,
	.dev = {
		.platform_data = &lge_android_usb_pdata,
	},
};

static int __init lge_android_usb_devices_init(void)
{
	return platform_device_register(&lge_android_usb_device);
}
arch_initcall(lge_android_usb_devices_init);
#endif

#ifdef CONFIG_LGE_KSWITCH
static
int atoi(const char* str)
{
	int val = 0;

	if (str == NULL)
	{
		printk(KERN_CRIT "[KSwitch] kill switch flag string pointer is NULL\n");
		return -1;
	}

	for (;;str++)
	{
		switch (*str)
		{
			case '0' ... '9':
				val = 10*val + (*str - '0');
				break;
			default:
				return val;
		}
	}
}

static
int __init kswitch_setup(char* value)
{
	s_kswitch_flag = atoi(value);

	if (s_kswitch_flag < 0)
	{
		printk(KERN_CRIT "[KSwitch] malformed kswitch flag value is used to setup: 0x%x\n", s_kswitch_flag);
		s_kswitch_flag = 0;
	}

	return 1;
}
__setup("kswitch=", kswitch_setup);

int  lge_get_kswitch_status()
{
	return s_kswitch_flag;
}
#endif	/*                    */

#ifdef CONFIG_LGE_DIAG_USB_ACCESS_LOCK
static struct platform_device lg_diag_cmd_device = {
	.name = "lg_diag_cmd",
	.id = -1,
	.dev    = {
		.platform_data = 0, /* &lg_diag_cmd_pdata */
	},
};

static int __init lge_diag_devices_init(void)
{
	return platform_device_register(&lg_diag_cmd_device);
}
arch_initcall(lge_diag_devices_init);
#endif

#ifdef CONFIG_LGE_QFPROM_INTERFACE
static struct platform_device qfprom_device = {
	.name = "lge-qfprom",
	.id = -1,
};

static int __init lge_add_qfprom_devices(void)
{
	return platform_device_register(&qfprom_device);
}

arch_initcall(lge_add_qfprom_devices);
#endif

static int lge_boot_reason = -1; /* undefined for error checking */
static int __init lge_check_bootreason(char *reason)
{
	int ret = 0;

	/* handle corner case of kstrtoint */
	if (!strcmp(reason, "0xffffffff")) {
		lge_boot_reason = 0xffffffff;
		return 1;
	}

	ret = kstrtoint(reason, 16, &lge_boot_reason);
	if (!ret)
		printk(KERN_INFO "LGE REBOOT REASON: %x\n", lge_boot_reason);
	else
		printk(KERN_INFO "LGE REBOOT REASON: Couldn't get bootreason - %d\n",
				ret);

	return 1;
}
__setup("lge.bootreason=", lge_check_bootreason);

int lge_get_bootreason(void)
{
	return lge_boot_reason;
}

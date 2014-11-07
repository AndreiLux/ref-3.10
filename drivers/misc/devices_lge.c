#include <linux/kernel.h>
#include <linux/string.h>


#include <asm/setup.h>
#include <asm/system_info.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/vmalloc.h>
#include <linux/memblock.h>
#include <linux/board_lge.h>

/* for board revision */
static hw_rev_type lge_bd_rev = HW_REV_C; //HW_REV_B;

/* for sidd value hidden menu */
char sidd_kerenl_string[9];

/* CAUTION: These strings are come from LK. */
static char *rev_str[] = {"rev_a", "rev_b", "rev_c", "rev_d",
	"rev_e", "rev_f", "rev_g", "rev_h", "rev_i", "rev_j",
	"rev_10", "rev_11", "rev_12", "revserved"};

#if defined(CONFIG_LGE_KSWITCH)
static int kswitch_status;
#endif

static int __init board_revno_setup(char *rev_info)
{
	int i;

	for (i = 0; i < HW_REV_MAX; i++) {
		if (!strncmp(rev_info, rev_str[i], 6)) {
			lge_bd_rev = (hw_rev_type) i;
			/* it is defined externally in <asm/system_info.h> */
			//system_rev = lge_bd_rev;
			break;
		}
	}

	//                                                           
	return 1;
}
__setup("lge.rev=", board_revno_setup);

hw_rev_type lge_get_board_revno(void)
{
    return lge_bd_rev;
}
EXPORT_SYMBOL(lge_get_board_revno);

/* get boot mode information from cmdline.
 * If any boot mode is not specified,
 * boot mode is normal type.
 */
static lge_boot_mode_type lge_boot_mode = LGE_BOOT_MODE_NORMAL;
int __init lge_boot_mode_init(char *s)
{
	if (!strcmp(s, "charger"))
		lge_boot_mode = LGE_BOOT_MODE_CHARGER;
	else if (!strcmp(s, "chargerlogo"))
		lge_boot_mode = LGE_BOOT_MODE_CHARGERLOGO;
	else if (!strcmp(s, "qem_130k"))
		lge_boot_mode = LGE_BOOT_MODE_FACTORY;
	else if (!strcmp(s, "qem_56k"))
		lge_boot_mode = LGE_BOOT_MODE_FACTORY2;
	else if (!strcmp(s, "qem_910k"))
		lge_boot_mode = LGE_BOOT_MODE_FACTORY3;
	else if (!strcmp(s, "pif_130k"))
		lge_boot_mode = LGE_BOOT_MODE_PIFBOOT;
	else if (!strcmp(s, "pif_56k"))
		lge_boot_mode = LGE_BOOT_MODE_PIFBOOT2;
	else if (!strcmp(s, "pif_910k"))
		lge_boot_mode = LGE_BOOT_MODE_PIFBOOT3;

	//printk("ANDROID BOOT MODE : %d %s\n", lge_boot_mode, s);
	/*                            */

	return 1;
}
__setup("androidboot.mode=", lge_boot_mode_init);

lge_boot_mode_type lge_get_boot_mode(void)
{
	return lge_boot_mode;
}
EXPORT_SYMBOL(lge_get_boot_mode);

static lge_boot_reason_type lge_boot_reason = LGE_BOOT_HW_BOOT;
int __init lge_boot_reason_init(char *s)
{
	if (!strcmp(s, "hw_boot"))
		lge_boot_reason = LGE_BOOT_HW_BOOT;
	else if (!strcmp(s, "hidden_reset"))
		lge_boot_reason = LGE_BOOT_HIDDEN_RESET ;
	else if (!strcmp(s, "sw_reset"))
		lge_boot_reason = LGE_BOOT_SW_RESET;
	else if (!strcmp(s, "hw_reset"))
		lge_boot_reason = LGE_BOOT_HW_RESET;
	else if (!strcmp(s, "smpl_boot"))
		lge_boot_reason =LGE_BOOT_SMPL_BOOT ;
	else if (!strcmp(s, "key_boot"))
		lge_boot_reason =LGE_BOOT_KEY_BOOT ;
	else if (!strcmp(s, "cable_boot"))
		lge_boot_reason = LGE_BOOT_CABLE_BOOT;

	//printk("ANDROID BOOT REASON MODE : %d %s\n",lge_boot_reason , s);
	/*                            */

	return 1;
}
__setup("lge.r=", lge_boot_reason_init);

lge_boot_reason_type lge_get_boot_reason(void)
{
	return lge_boot_reason;
}
EXPORT_SYMBOL(lge_get_boot_reason);







bool lge_get_factory_boot(void)
{
	bool res;

	/*   if boot mode is factory,
	 *   cable must be factory cable.
	 */
	switch (lge_boot_mode) {
		case LGE_BOOT_MODE_FACTORY:
		case LGE_BOOT_MODE_FACTORY2:
		case LGE_BOOT_MODE_FACTORY3:
		case LGE_BOOT_MODE_PIFBOOT:
		case LGE_BOOT_MODE_PIFBOOT2:
		case LGE_BOOT_MODE_PIFBOOT3:
			res = true;
			break;
		default:
			res = false;
			break;
	}

	return res;
}
EXPORT_SYMBOL(lge_get_factory_boot);

bool lge_get_factory_56k_130k_boot(void)
{
	bool res;

	/*   if boot mode is factory,
	 *   cable must be factory cable.
	 */
	switch (lge_boot_mode) {
		case LGE_BOOT_MODE_FACTORY:
		case LGE_BOOT_MODE_FACTORY2:
		case LGE_BOOT_MODE_PIFBOOT:
		case LGE_BOOT_MODE_PIFBOOT2:
			res = true;
			break;
		default:
			res = false;
			break;
	}

	return res;
}
EXPORT_SYMBOL(lge_get_factory_56k_130k_boot);

#define MAX_CABLE_NUM	4

struct usb_cable_adc_type {
    int  adc_min;
    int  adc_max;
    unsigned int cable_type;
} ;

static struct usb_cable_adc_type usb_cable_type_data[MAX_CABLE_NUM] = {
    /*			adc min,		adc min,		type   */
    /* _56K  */  	{120,		150,	LT_CABLE_56K},
    /* _130K */  	{250,		330,	LT_CABLE_130K},
    /* _910K */  	{990,		1200,   LT_CABLE_910K},
    /* _OPEN */ 		{1600,		1800,   USB_CABLE_400MA}
};

extern int d2260_adc_get_usb_id(void);

unsigned int lge_pm_get_cable_info(void)
{
	char *type_str[MAX_CABLE_NUM] = {"56K", "130K", "910K", "OPEN"};

	struct usb_cable_adc_type *table;
	int adc_value = 0;
	int i, count = 1;
	unsigned int c_type = NO_INIT_CABLE;

	for (i = 0; i < count; i++) {
		adc_value = d2260_adc_get_usb_id();
		if (adc_value < 0) {
			c_type = NO_INIT_CABLE;
			pr_err("lge_pm_get_cable_info: adc read "
					"error - %d\n", adc_value);
			return c_type;
		}
	}

	/* assume: adc value must be existed in ascending order */
	for (i = 0; i < MAX_CABLE_NUM; i++) {
		table = &usb_cable_type_data[i];

		if (adc_value <= table->adc_min &&
				adc_value <= table->adc_max) {
			c_type = table->cable_type;
			break;
		}
	}

	if(i == MAX_CABLE_NUM) {
		c_type = ABNORMAL_USB_CABLE_400MA;
	}

	pr_info("[lge_pm_get_cable_info]Cable: %d\n", adc_value);

	return c_type;
}

EXPORT_SYMBOL(lge_pm_get_cable_info);

static unsigned int lge_batt_id = BATT_UNKNOWN;
int __init lge_batt_id_init(char *s)
{
	if (!strcmp(s, "unknown"))
		lge_batt_id = BATT_UNKNOWN;
	else if (!strcmp(s, "ds2704_n"))
		lge_batt_id = BATT_DS2704_N;
	else if (!strcmp(s, "ds2704_l"))
		lge_batt_id = BATT_DS2704_L;
	else if (!strcmp(s, "ds2704_c"))
		lge_batt_id = BATT_DS2704_C;
	else if (!strcmp(s, "isl6296_n"))
		lge_batt_id = BATT_ISL6296_N;
	else if (!strcmp(s, "isl6296_l"))
		lge_batt_id = BATT_ISL6296_L;
	else if (!strcmp(s, "isl6296_c"))
		lge_batt_id = BATT_ISL6296_C;
	else if (!strcmp(s, "no_bat"))
		lge_batt_id = BATT_NOT_PRESENT;
	else
		lge_batt_id = 999;
	return 1;
}
__setup("lge.batid=", lge_batt_id_init);

unsigned int lge_get_batt_id(void)
{
	return lge_batt_id;
}
EXPORT_SYMBOL(lge_get_batt_id);

static unsigned int lge_fbm_status = 0;
int __init lge_fbm_sts_init(char *s)
{
	if (!strcmp(s, "0"))
		lge_fbm_status = 0;
	else if (!strcmp(s, "1"))
		lge_fbm_status = 1;
	else
		lge_fbm_status = 999;
	return 1;
}
__setup("lge.fbm=", lge_fbm_sts_init);

unsigned int lge_get_fake_battery_mode(void)
{
	return lge_fbm_status;
}
EXPORT_SYMBOL(lge_get_fake_battery_mode);

/* SIDD Start */
int __init lge_sidd_id_init(char *s)
{
	int cnt = 0;

	for(cnt=0; cnt<8; cnt++){
		sidd_kerenl_string[cnt] = s[cnt];
	}

	sidd_kerenl_string[8] = '\0';
	pr_info("kernel_devices_lge sidd init: %s\n", sidd_kerenl_string);
	return 1;
}
__setup("lge.sidd=", lge_sidd_id_init);

char *lge_get_sidd(void)
{
	return sidd_kerenl_string;
}
EXPORT_SYMBOL(lge_get_sidd);
/* SIDD END */

static char *lk_boot_time;

static int lge_boot_time_setup(char *time)
{

	lk_boot_time = time;
	//printk(KERN_INFO "LK Boot Time : %s ms \n", lk_boot_time);
	return 1;

}
__setup("lge.lk_boot_time=", lge_boot_time_setup);


char* lge_get_boot_time(void)
{
        return lk_boot_time;
}
EXPORT_SYMBOL(lge_get_boot_time);

unsigned long odin_ca15_maxfreq = 1500000;
static int __init lge_ca15_maxfreq_setup(char *str)
{
	odin_ca15_maxfreq = simple_strtoul(str,NULL,0);
	return 1;
}

__setup("lge.ca15_maxfreq=", lge_ca15_maxfreq_setup);


unsigned long lge_get_ca15_maxfreq(void)
{
        return odin_ca15_maxfreq;
}
EXPORT_SYMBOL(lge_get_ca15_maxfreq);

/*
	for download complete using LAF image
	return value : 1 --> right after laf complete & reset
*/

int android_dlcomplete=0;

int __init lge_android_dlcomplete(char *s)
{
	if (strncmp(s, "1", 1) == 0)   /* if same string */
		android_dlcomplete = 1;
	else	/* not same string */
		android_dlcomplete = 0;
	//printk("androidboot.dlcomplete = %d\n", android_dlcomplete);

	return 1;
}

__setup("androidboot.dlcomplete=", lge_android_dlcomplete);

int lge_get_android_dlcomplete(void)
{
	pr_info("android_dlcomplete: %d\n", android_dlcomplete);
	return android_dlcomplete;
}

static enum lge_laf_mode_type lge_laf_mode = LGE_LAF_MODE_NORMAL;

int __init lge_laf_mode_init(char *s)
{
	pr_info("lge_laf_mode_init() input: %s\n", s);
	if (strcmp(s, "")) /*s is not empty:*/
		lge_laf_mode = LGE_LAF_MODE_LAF;

	return 1;
}

__setup("androidboot.laf=", lge_laf_mode_init);

enum lge_laf_mode_type lge_get_laf_mode(void)
{
	pr_info("lge_laf_mode: %d\n", (int)lge_laf_mode);
	return lge_laf_mode;
}

static lge_lcd_connect_check lge_lcd_connection = LGE_LCD_CONNECT ;
static int __init lcd_connect_check(char *lcd_connect_info)
{
	if (!strcmp(lcd_connect_info, "connect"))
	lge_lcd_connection = LGE_LCD_CONNECT;
	else if (!strcmp(lcd_connect_info, "disconnect"))
	lge_lcd_connection = LGE_LCD_DISCONNECT;

	return 1;
}
__setup("lge.lcd.connect=", lcd_connect_check);

unsigned int lge_get_lcd_connect(void)
{
    return lge_lcd_connection;
}
EXPORT_SYMBOL(lge_get_lcd_connect);

#if defined(CONFIG_LGE_KSWITCH)
static int atoi(const char *name)
{
	int val = 0;

	for(;; name++) {
		switch(*name) {
			case '0' ... '9' :
				val = 10*val+(*name-'0');
				break;
			default:
				return val;
		}
	}
}

static int __init kswitch_setup(char *s)
{
	kswitch_status = atoi(s);

	if (kswitch_status < 0)
		kswitch_status = 0;

	return 1;
}

__setup("kswitch=", kswitch_setup);

int lge_get_kswitch_status(void)
{
	return kswitch_status;
}
#endif

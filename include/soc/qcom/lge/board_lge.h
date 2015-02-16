#ifndef __ASM_ARCH_MSM_BOARD_LGE_H
#define __ASM_ARCH_MSM_BOARD_LGE_H

enum hw_rev_type {
	HW_REV_0 = 0,
	HW_REV_A,
	HW_REV_B,
	HW_REV_C,
	HW_REV_D,
	HW_REV_E,
	HW_REV_F,
	HW_REV_G,
	HW_REV_1_0,
	HW_REV_1_1,
       HW_REV_1_2,
	HW_REV_MAX
};

extern char *rev_str[];

enum hw_rev_type lge_get_board_revno(void);
int lge_get_fake_battery(void);

#ifdef CONFIG_LGE_PM_USB_ID
enum acc_cable_type {
	NO_INIT_CABLE = 0,
	CABLE_MHL_1K,
	CABLE_U_28P7K,
	CABLE_28P7K,
	CABLE_56K,
	CABLE_100K,
	CABLE_130K,
	CABLE_180K,
	CABLE_200K,
	CABLE_220K,
	CABLE_270K,
	CABLE_330K,
	CABLE_620K,
	CABLE_910K,
	CABLE_NONE
};

struct chg_cable_info {
	enum acc_cable_type cable_type;
	unsigned ta_ma;
	unsigned usb_ma;
};

void get_cable_data_from_dt(void *of_node);

struct qpnp_vadc_chip;
enum lge_boot_mode_type lge_get_boot_mode(void);
int lge_pm_get_cable_info(struct qpnp_vadc_chip *, struct chg_cable_info *);
void lge_pm_read_cable_info(struct qpnp_vadc_chip *);
enum acc_cable_type lge_pm_get_cable_type(void);
unsigned lge_pm_get_ta_current(void);
unsigned lge_pm_get_usb_current(void);
#endif
#ifdef CONFIG_LGE_LCD_TUNING
struct lcd_platform_data {
int (*set_values) (int *tun_lcd_t);
int (*get_values) (int *tun_lcd_t);
};

void __init lge_add_lcd_misc_devices(void);
#endif

#if defined(CONFIG_PRE_SELF_DIAGNOSIS)
int lge_pre_self_diagnosis(char *drv_bus_code, int func_code, char *dev_code, char *drv_code, int errno);
#endif

#if defined(CONFIG_LGE_MIPI_JDI_INCELL_QHD_CMD_PANEL)
char* lge_get_dsv_vendor(void);
int lge_get_jdi_cut(void);
#endif

#if defined(CONFIG_PRE_SELF_DIAGNOSIS)
struct pre_selfd_platform_data {
	int (*set_values) (int r, int g, int b);
	int (*get_values) (int *r, int *g, int *b);
};
#endif

#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
struct pseudo_batt_info_type {
	int mode;
	int id;
	int therm;
	int temp;
	int volt;
	int capacity;
	int charging;
};

struct pseudo_batt_info_type;
void pseudo_batt_set(struct pseudo_batt_info_type *);
#endif

enum lge_laf_mode_type {
	LGE_LAF_MODE_NORMAL = 0,
	LGE_LAF_MODE_LAF,
};

enum lge_laf_mode_type lge_get_laf_mode(void);

enum lge_boot_mode_type {
	LGE_BOOT_MODE_NORMAL = 0,
	LGE_BOOT_MODE_CHARGER,
	LGE_BOOT_MODE_CHARGERLOGO,
	LGE_BOOT_MODE_QEM_56K,
	LGE_BOOT_MODE_QEM_130K,
	LGE_BOOT_MODE_QEM_910K,
	LGE_BOOT_MODE_PIF_56K,
	LGE_BOOT_MODE_PIF_130K,
	LGE_BOOT_MODE_PIF_910K,
	LGE_BOOT_MODE_MINIOS    /*                          */
};

enum lge_boot_mode_type lge_get_boot_mode(void);
int lge_get_android_dlcomplete(void);
int lge_get_factory_boot(void);
int get_lge_frst_status(void);

#ifdef CONFIG_LGE_EARJACK_DEBUGGER
/* config */
# define UART_CONSOLE_ENABLE_ON_EARJACK						BIT(0)
# define UART_CONSOLE_ENABLE_ON_EARJACK_DEBUGGER	BIT(1)
# define UART_CONSOLE_ENABLE_ON_DEFAULT						BIT(2)
/* current status
 * ENABLED | DISABLED : logical enable/disable
 * READY : It means whether device is ready or not.
 *         So even if in ENABLED state, console output will not be emitted on NOT-ready state.
 */
# define UART_CONSOLE_ENABLED											BIT(3)
# define UART_CONSOLE_DISABLED										!(BIT(3))
# define UART_CONSOLE_READY												BIT(4)
/* filter */
# define UART_CONSOLE_MASK_ENABLE_ON					(BIT(0) | BIT(1) | BIT(2))
# define UART_CONSOLE_MASK_CONFIG							UART_CONSOLE_MASK_ENABLE_ON
# define UART_CONSOLE_MASK_ENABLED						BIT(3)
# define UART_CONSOLE_MASK_READY							BIT(4)

/* util macro */
# define lge_uart_console_should_enable_on_earjack()					(unsigned int)(lge_uart_console_get_config() & UART_CONSOLE_ENABLE_ON_EARJACK)
# define lge_uart_console_should_enable_on_earjack_debugger()	(unsigned int)(lge_uart_console_get_config() & UART_CONSOLE_ENABLE_ON_EARJACK_DEBUGGER)
# define lge_uart_console_should_enable_on_default()					(unsigned int)(lge_uart_console_get_config() & UART_CONSOLE_ENABLE_ON_DEFAULT)
# define lge_uart_console_on_earjack_in()							do{msm_serial_set_uart_console(lge_uart_console_should_enable_on_earjack());}while(0)
# define lge_uart_console_on_earjack_out()						do{msm_serial_set_uart_console(lge_uart_console_should_enable_on_default());}while(0)
# define lge_uart_console_on_earjack_debugger_in()		do{msm_serial_set_uart_console(lge_uart_console_should_enable_on_earjack_debugger());}while(0)
# define lge_uart_console_on_earjack_debugger_out()		do{msm_serial_set_uart_console(lge_uart_console_should_enable_on_default());}while(0)

/* config =  UART_CONSOLE_ENABLE_ON_XXX [| UART_CONSOLE_ENABLE_ON_XXX]* */
extern unsigned int lge_uart_console_get_config(void);
extern void lge_uart_console_set_config(unsigned int config);

/* logical uart console status modifier
 * used as a flag to tell "I want to enable/disable uart console"
 * @RETURN or @PARAM::enabled
 * UART_CONSOLE_ENABLED  (non-zero): enabled
 * !UART_CONSOLE_ENABLED (zero): disabled
 */
extern unsigned int lge_uart_console_get_enabled(void);
extern void lge_uart_console_set_enabled(int enabled);
/* internal uart console device status tracker
 *
 * @RETURN or @PARAM::ready
 * UART_CONSOLE_READY (non-zero): device is ready
 * !UART_CONSOLE_READY (zero): device is not ready
 */
extern unsigned int lge_uart_console_get_ready(void);
extern void lge_uart_console_set_ready(unsigned int ready);

/* real device enabler (or disabler)
 * control uart console device to enable/disable
 * NOTE @PARAM::enable should be selected by uart console enable/disable policy
 * which can be known by lge_uart_console_should_enable_on_xxx.
 * @PARAM::enable
 * zero : disabled
 * non-zero : enable
 */
extern int msm_serial_set_uart_console(int enable);
#endif

#ifdef CONFIG_LGE_KSWITCH
# define LGE_KSWITCH_DISABLE_UART	BIT(3)
extern int lge_get_kswitch_status(void);
#endif

extern int lge_get_bootreason(void);

#endif

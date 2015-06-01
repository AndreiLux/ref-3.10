#ifndef __MAX77819_COMMON_H__
#define __MAX77819_COMMON_H__

#ifdef MTK_MAX77819_SUPPORT

/*************************************************************************************
MAX77819 Support Backlight , Vibrator, Flashlight , Charger Functions,
You can set Feature switch here. If support, please define the Micro definition, otherwise, please mark it.
**************************************************************************************/
#define MAX77819_BACKLIGHT
//#define MAX77819_VIBRATOR
#define MAX77819_FLASHLIGHT
#define MAX77819_CHARGER

#ifdef MAX77819_CHARGER
extern int max77819_charger_set_current(void *data);
extern int max77819_charger_get_current(void *data);
extern int max77819_charger_set_input_current(void *data);
extern int max77819_charger_enable(void *data);
extern int max77819_charger_set_cv_voltage(void *data);
extern int max77819_charger_get_charging_status(void *data);
extern int max77819_charger_hw_init(void *data);
extern void max77819_charger_dump_register(void); 
#endif

//Backlight
#ifdef MAX77819_BACKLIGHT
extern int max77819_set_backlight_cntl_mode(unsigned char mode);
extern int max77819_set_backlight_level(int level);
extern int max77819_set_backlight_enable(unsigned char on);
#endif

//Vibrator
#ifdef MAX77819_VIBRATOR
extern int max77819_vib_set(bool on);
#endif

//Flashlight
#ifdef MAX77819_FLASHLIGHT
extern void max77819_flash_deinit(void);
extern void max77819_flash_disable(void);
extern void max77819_flash_enable(void);
extern void max77819_flash_init(void);
extern void max77819_flash_preOn(void);
extern void max77819_flash_set_duty(unsigned char duty);
#endif


#endif

#endif
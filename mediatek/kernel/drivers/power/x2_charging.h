/************************************************
         Lenovo-sw yexh1 add for Lenovo charging spec
         
*************************************************/
#ifndef  LENOVO_CHARGING_H
#define  LENOVO_CHARGING_H

#include <linux/init.h>        /* For init/exit macros */
#include <linux/module.h>      /* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/xlog.h>
#include <linux/kernel.h>
#include <mach/battery_common.h>
#include <mach/charging.h>
#include "cust_charging.h"
#include <mach/battery_meter.h>
#include <mach/mt_sleep.h>

#define LENOVO_BAT_LOG_CRTI 1
#define LENOVO_BAT_LOG_FULL 2
#define LENOVO_BAT_LOG_LEVEL   LENOVO_BAT_LOG_CRTI
#define lenovo_bat_printk(num, fmt, args...) \
  do { \
    if (LENOVO_BAT_LOG_LEVEL >= (int)num) { \
      xlog_printk(ANDROID_LOG_INFO, "Lenovo/Battery", fmt, ##args); \
    } \
  } while(0)

typedef enum {
    CHARGING_STATE_UNKNOWN = 0,
    CHARGING_STATE_CHGR_IN,    		
    CHARGING_STATE_PROTECT,      //no charging    
    CHARGING_STATE_CHARGING,
    CHARGING_STATE_LIMIT_CHARGING,  //0.3C
    CHARGING_STATE_CHGR_OUT,   
    CHARGING_STATE_ERROR,   
} LENOVO_CHARGING_STATE;

typedef enum{  
    LENOVO_TEMP_BELOW_0,
    LENOVO_TEMP_POS_0_TO_POS_10,
    LENOVO_TEMP_POS_10_TO_POS_45,
    LENOVO_TEMP_POS_45_TO_POS_50,
    LENOVO_TEMP_ABOVE_POS_50
}LENOVO_TEMP_STATE;


    
#define LENOVO_TEMP_POS_50_THRESHOLD  50
#define LENOVO_TEMP_POS_50_DOWN 48  

#define LENOVO_TEMP_POS_45_THRESHOLD  45
#define LENOVO_TEMP_POS_45_DOWN 43  
#define LENOVO_TEMP_POS_45_UP 46

#define LENOVO_TEMP_POS_10_THRESHOLD  10
#define LENOVO_TEMP_POS_10_DOWN 8  
#define LENOVO_TEMP_POS_10_UP 11

#define LENOVO_TEMP_POS_0_THRESHOLD  0
#define LENOVO_TEMP_POS_0_UP 2  

#define LENOVO_TEMP_NEG_10_THRESHOLD  -10
#define LENOVO_TEMP_NEG_20_THRESHOLD  -20

#define LENOVO_CHARGING_THREAD_PERIOD   10  //10s
#define LENOVO_TEMP_NEG_10_COUNT   (600/LENOVO_CHARGING_THREAD_PERIOD)   //10mins
#define LENOVO_TEMP_NEG_0_COUNT    (300/LENOVO_CHARGING_THREAD_PERIOD)   //5mins
#define LENOVO_TEMP_POS_50_COUNT    (300/LENOVO_CHARGING_THREAD_PERIOD)   //5mins
#define LENOVO_TEMP_ERROR_REPORT_COUNT    (20/LENOVO_CHARGING_THREAD_PERIOD)   //20s
#define LENOVO_TEMP_ERROR_RESUME_COUNT    (20/LENOVO_CHARGING_THREAD_PERIOD)   //20s
#define LENOVO_TEMP_CC_DEBOUNCE_COUNT    (20/LENOVO_CHARGING_THREAD_PERIOD)   //20s
#define LENOVO_TEMP_LIMIT_C_DEBOUNCE_COUNT    (20/LENOVO_CHARGING_THREAD_PERIOD)   //20s

#define BATTERY_TEMP_HIGH_STOP_CHARGING   0x8000
#define BATTERY_TEMP_LOW_STOP_CHARGING   0x4000
#define BATTERY_TEMP_LOW_SLOW_CHARGING   0x2000
#define BATTERY_TEMP_HIGH_SLOW_CHARGING  0x1000
#define BATTERY_TEMP_NORMAL_MASK               0x0FFF

#define BAT_TASK_CHARGING_PERIOD                     3  //1s

extern kal_bool lenovo_battery_is_battery_exist(kal_int64 R, kal_int64 Rdown);
extern void lenovo_get_charging_curret(void *data);
extern int lenovo_battery_create_sys_file(struct device *dev);
extern kal_bool  lenovo_battery_charging_thread(unsigned int *notify);
extern void lenovo_battery_set_Qmax_cali_status(int status);
#endif 

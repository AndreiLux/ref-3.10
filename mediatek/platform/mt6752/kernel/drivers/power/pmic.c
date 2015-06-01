/*****************************************************************************
 *
 * Filename:
 * ---------
 *    pmic.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines PMIC functions
 *
 * Author:
 * -------
 * James Lo
 *
 ****************************************************************************/
#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/earlysuspend.h>
#include <linux/seq_file.h>

#include <asm/uaccess.h>

#include <mach/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mach/mt_pm_ldo.h>
#include <mach/eint.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_gpio.h>
#include <mach/mtk_rtc.h>
#include <mach/mt_spm_mtcmos.h>

#include <mach/battery_common.h>
#include <linux/time.h>

#include "pmic_dvt.h"

#if defined (MTK_KERNEL_POWER_OFF_CHARGING)
#include <mach/mt_boot.h>
#include <mach/system.h>
#include "mach/mt_gpt.h"
#endif

#include <mach/mt6311.h>
#include <cust_pmic.h>
#include <cust_eint.h>
#include <cust_battery_meter.h>
    
extern int Enable_BATDRV_LOG;

//==============================================================================
// Global variable
//==============================================================================
#ifdef CUST_EINT_MT_PMIC_MT6325_NUM
unsigned int g_eint_pmit_mt6325_num = CUST_EINT_MT_PMIC_MT6325_NUM;
#else
unsigned int g_eint_pmit_mt6325_num = 24;
#endif

#ifdef CUST_EINT_MT_PMIC_DEBOUNCE_CN
unsigned int g_cust_eint_mt_pmic_debounce_cn = CUST_EINT_MT_PMIC_DEBOUNCE_CN;
#else
unsigned int g_cust_eint_mt_pmic_debounce_cn = 1;      
#endif 

#ifdef CUST_EINT_MT_PMIC_TYPE
unsigned int g_cust_eint_mt_pmic_type = CUST_EINT_MT_PMIC_TYPE;
#else
unsigned int g_cust_eint_mt_pmic_type = 4;
#endif

#ifdef CUST_EINT_MT_PMIC_DEBOUNCE_EN
unsigned int g_cust_eint_mt_pmic_debounce_en = CUST_EINT_MT_PMIC_DEBOUNCE_EN;
#else
unsigned int g_cust_eint_mt_pmic_debounce_en = 1;
#endif

//==============================================================================
// PMIC related define
//==============================================================================
static DEFINE_MUTEX(pmic_lock_mutex);
#define PMIC_EINT_SERVICE

//==============================================================================
// Extern
//==============================================================================
extern int bat_thread_kthread(void *x);
extern void charger_hv_detect_sw_workaround_init(void);
extern void pmu_drv_tool_customization_init(void);
extern void pmic_auxadc_init(void);
extern int PMIC_IMM_GetOneChannelValue(upmu_adc_chl_list_enum dwChannel, int deCount, int trimd);

#if defined (MTK_KERNEL_POWER_OFF_CHARGING)
extern void mt_power_off(void);
static kal_bool long_pwrkey_press = false;
static unsigned long timer_pre = 0; 
static unsigned long timer_pos = 0; 
#define LONG_PWRKEY_PRESS_TIME         500*1000000    //500ms
#endif

#if defined (MTK_VOW_SUPPORT) && defined (MTK_VOW_TEST)
#include <sound/mt_soc_audio.h>
#define VOW_ENABLE 1
#else
#define VOW_ENABLE 0
#endif


//==============================================================================
// PMIC lock/unlock APIs
//==============================================================================
void pmic_lock(void)
{
    mutex_lock(&pmic_lock_mutex);
}

void pmic_unlock(void)
{
    mutex_unlock(&pmic_lock_mutex);
}

kal_uint32 upmu_get_reg_value(kal_uint32 reg)
{
    U32 ret=0;
    U32 reg_val=0;
    
    ret=pmic_read_interface(reg, &reg_val, 0xFFFF, 0x0);
    
    return reg_val;
}
EXPORT_SYMBOL(upmu_get_reg_value);

void upmu_set_reg_value(kal_uint32 reg, kal_uint32 reg_val)
{
    U32 ret=0;
    
    ret=pmic_config_interface(reg, reg_val, 0xFFFF, 0x0);    
}

unsigned int get_pmic_mt6325_cid(void)
{
    return mt6325_upmu_get_swcid();
}

U32 get_mt6325_pmic_chip_version (void)
{
    return mt6325_upmu_get_swcid();
}

//==============================================================================
// buck current
//==============================================================================
int pmic_get_buck_current(int avg_times, int chip_type)
{
#if 0
    // no function
#else
    return 0;
#endif    
}
EXPORT_SYMBOL(pmic_get_buck_current);

static ssize_t show_MT6325_BUCK_CURRENT_METER(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=0;

    //ret_value = pmic_get_buck_current(10, MT6325_CHIP);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] MT6325 BUCK_CURRENT_METER : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_MT6325_BUCK_CURRENT_METER(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(MT6325_BUCK_CURRENT_METER, 0664, show_MT6325_BUCK_CURRENT_METER, store_MT6325_BUCK_CURRENT_METER);

//==============================================================================
// upmu_interrupt_chrdet_int_en
//==============================================================================
void upmu_interrupt_chrdet_int_en(kal_uint32 val)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[upmu_interrupt_chrdet_int_en] val=%d.\r\n", val);

    mt6325_upmu_set_rg_int_en_chrdet(val);
}
EXPORT_SYMBOL(upmu_interrupt_chrdet_int_en);

//==============================================================================
// PMIC charger detection
//==============================================================================
kal_uint32 upmu_get_rgs_chrdet(void)
{
    kal_uint32 val=0;
    
    val = mt6325_upmu_get_rgs_chrdet();
    
    battery_xlog_printk(BAT_LOG_CRTI,"[upmu_get_rgs_chrdet] CHRDET status = %d\n", val);

    return val;
}

//==============================================================================
// Low battery call back function
//==============================================================================
#define LBCB_NUM 16

#ifndef DISABLE_LOW_BATTERY_PROTECT
#define LOW_BATTERY_PROTECT
#endif

// ex. 3400/7200*4096=0x78E
#define BAT_HV_THD   (POWER_INT0_VOLT*4096/7200) //ex: 3400mV
#define BAT_LV_1_THD (POWER_INT1_VOLT*4096/7200) //ex: 3250mV
#define BAT_LV_2_THD (POWER_INT2_VOLT*4096/7200) //ex: 3100mV

int g_low_battery_level=0;
int g_low_battery_stop=0;

struct low_battery_callback_table
{
    void *lbcb;
};

struct low_battery_callback_table lbcb_tb[] ={
    {NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},
    {NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL}    
};

void (*low_battery_callback)(LOW_BATTERY_LEVEL);

void register_low_battery_notify( void (*low_battery_callback)(LOW_BATTERY_LEVEL), LOW_BATTERY_PRIO prio_val )
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[register_low_battery_notify] start\n");

    lbcb_tb[prio_val].lbcb = low_battery_callback;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[register_low_battery_notify] prio_val=%d\n",prio_val);
}

void exec_low_battery_callback(LOW_BATTERY_LEVEL low_battery_level) //0:no limit
{
    int i=0;

    if(g_low_battery_stop==1)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[exec_low_battery_callback] g_low_battery_stop=%d\n", g_low_battery_stop);
    }
    else
    {
        for(i=0 ; i<LBCB_NUM ; i++) 
        {
            if(lbcb_tb[i].lbcb != NULL)
            {
                low_battery_callback = lbcb_tb[i].lbcb;
                low_battery_callback(low_battery_level);
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[exec_low_battery_callback] prio_val=%d,low_battery=%d\n",i,low_battery_level);
            }        
        }
    }
}

void lbat_min_en_setting(int en_val)
{
    mt6325_upmu_set_rg_lbat_en_min(en_val);  //TBD
    mt6325_upmu_set_rg_lbat_irq_en_min(en_val);
    mt6325_upmu_set_rg_int_en_bat_l(en_val);

}

void lbat_max_en_setting(int en_val)
{
    mt6325_upmu_set_rg_lbat_en_max(en_val);
    mt6325_upmu_set_rg_lbat_irq_en_max(en_val);
    mt6325_upmu_set_rg_int_en_bat_h(en_val);
}

void low_battery_protect_init(void)
{
    //default setting
    mt6325_upmu_set_rg_lbat_debt_min(0);
    mt6325_upmu_set_rg_lbat_debt_max(0);
    mt6325_upmu_set_rg_lbat_det_prd_15_0(1);
    mt6325_upmu_set_rg_lbat_det_prd_19_16(0);

    mt6325_upmu_set_rg_lbat_volt_max(BAT_HV_THD);
    mt6325_upmu_set_rg_lbat_volt_min(BAT_LV_1_THD);


    lbat_min_en_setting(1);
    lbat_max_en_setting(0);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
            MT6325_AUXADC_CON5, upmu_get_reg_value(MT6325_AUXADC_CON5),
            MT6325_AUXADC_CON6, upmu_get_reg_value(MT6325_AUXADC_CON6),
            MT6325_INT_CON0, upmu_get_reg_value(MT6325_INT_CON0)
            );

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[low_battery_protect_init] %d mV, %d mV, %d mV\n",
        POWER_INT0_VOLT, POWER_INT1_VOLT, POWER_INT2_VOLT);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[low_battery_protect_init] Done\n");

}

//==============================================================================
// Battery OC call back function
//==============================================================================
#define OCCB_NUM 16

#ifndef DISABLE_BATTERY_OC_PROTECT
#define BATTERY_OC_PROTECT
#endif

// ex. Ireg = 65535 - (I * 950000uA / 2 / 158.122 / CAR_TUNE_VALUE * 100)
// (950000/2/158.122)*100~=300400

#define BAT_OC_H_THD   65535-((300400*POWER_BAT_OC_CURRENT_H/1000)/CAR_TUNE_VALUE)       //ex: 4670mA
#define BAT_OC_L_THD   65535-((300400*POWER_BAT_OC_CURRENT_L/1000)/CAR_TUNE_VALUE)       //ex: 5500mA

#define BAT_OC_H_THD_RE   65535-((300400*POWER_BAT_OC_CURRENT_H_RE/1000)/CAR_TUNE_VALUE) //ex: 3400mA
#define BAT_OC_L_THD_RE   65535-((300400*POWER_BAT_OC_CURRENT_L_RE/1000)/CAR_TUNE_VALUE) //ex: 4000mA

int g_battery_oc_level=0;
int g_battery_oc_stop=0;

struct battery_oc_callback_table
{
    void *occb;
};

struct battery_oc_callback_table occb_tb[] ={
    {NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},
    {NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL}    
};

void (*battery_oc_callback)(BATTERY_OC_LEVEL);

void register_battery_oc_notify( void (*battery_oc_callback)(BATTERY_OC_LEVEL), BATTERY_OC_PRIO prio_val )
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[register_battery_oc_notify] start\n");

    occb_tb[prio_val].occb = battery_oc_callback;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[register_battery_oc_notify] prio_val=%d\n",prio_val);
}

void exec_battery_oc_callback(BATTERY_OC_LEVEL battery_oc_level) //0:no limit
{
    int i=0;

    if(g_battery_oc_stop==1)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[exec_battery_oc_callback] g_battery_oc_stop=%d\n", g_battery_oc_stop);
    }
    else
    {
        for(i=0 ; i<OCCB_NUM ; i++) 
        {
            if(occb_tb[i].occb != NULL)
            {
                battery_oc_callback = occb_tb[i].occb;
                battery_oc_callback(battery_oc_level);
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[exec_battery_oc_callback] prio_val=%d,battery_oc_level=%d\n",i,battery_oc_level);
            }        
        }
    }
}

void bat_oc_h_en_setting(int en_val)
{
   mt6325_upmu_set_rg_int_en_fg_cur_h(en_val);   
}

void bat_oc_l_en_setting(int en_val)
{
   mt6325_upmu_set_rg_int_en_fg_cur_l(en_val);
}

void battery_oc_protect_init(void)
{
    mt6325_upmu_set_fg_cur_hth(BAT_OC_H_THD);
    mt6325_upmu_set_fg_cur_lth(BAT_OC_L_THD);

    bat_oc_h_en_setting(0);
    bat_oc_l_en_setting(1);

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
        MT6325_FGADC_CON23, upmu_get_reg_value(MT6325_FGADC_CON23),
        MT6325_FGADC_CON24, upmu_get_reg_value(MT6325_FGADC_CON24),
        MT6325_INT_CON2, upmu_get_reg_value(MT6325_INT_CON2)
        );

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[battery_oc_protect_init] %d mA, %d mA\n",
        POWER_BAT_OC_CURRENT_H, POWER_BAT_OC_CURRENT_L);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[battery_oc_protect_init] Done\n");  
}

void battery_oc_protect_reinit(void)
{
#ifdef BATTERY_OC_PROTECT
    mt6325_upmu_set_fg_cur_hth(BAT_OC_H_THD_RE);
    mt6325_upmu_set_fg_cur_lth(BAT_OC_L_THD_RE);

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
        MT6325_FGADC_CON23, upmu_get_reg_value(MT6325_FGADC_CON23),
        MT6325_FGADC_CON24, upmu_get_reg_value(MT6325_FGADC_CON24),
        MT6325_INT_CON2, upmu_get_reg_value(MT6325_INT_CON2)
        );

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[battery_oc_protect_reinit] %d mA, %d mA\n",
        POWER_BAT_OC_CURRENT_H_RE, POWER_BAT_OC_CURRENT_L_RE);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[battery_oc_protect_reinit] Done\n");  
#else
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[battery_oc_protect_reinit] no define BATTERY_OC_PROTECT\n");
#endif
}

//==============================================================================
// 15% notify service
//==============================================================================
static struct hrtimer bat_percent_notify_timer;
static struct task_struct *bat_percent_notify_thread = NULL;
static kal_bool bat_percent_notify_flag = KAL_FALSE;
static DECLARE_WAIT_QUEUE_HEAD(bat_percent_notify_waiter);
struct wake_lock bat_percent_notify_lock;
static DEFINE_MUTEX(bat_percent_notify_mutex);

extern kal_uint32 bat_get_ui_percentage(void);

#define BPCB_NUM 16

#ifndef DISABLE_BATTERY_PERCENT_PROTECT
#define BATTERY_PERCENT_PROTECT
#endif

int g_battery_percent_level=0;
int g_battery_percent_stop=0;

#define BAT_PERCENT_LINIT 15

struct battery_percent_callback_table
{
    void *bpcb;
};

struct battery_percent_callback_table bpcb_tb[] ={
    {NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},
    {NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL},{NULL}    
};

void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL);

void register_battery_percent_notify( void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL), BATTERY_PERCENT_PRIO prio_val )
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[register_battery_percent_notify] start\n");

    bpcb_tb[prio_val].bpcb = battery_percent_callback;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[register_battery_percent_notify] prio_val=%d\n",prio_val);

    if( (g_battery_percent_stop==0) && (g_battery_percent_level==1) )
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[register_battery_percent_notify] level l happen\n");
        battery_percent_callback(BATTERY_PERCENT_LEVEL_1);        
    }
}

void exec_battery_percent_callback(BATTERY_PERCENT_LEVEL battery_percent_level) //0:no limit
{
    int i=0;

    if(g_battery_percent_stop==1)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[exec_battery_percent_callback] g_battery_percent_stop=%d\n", g_battery_percent_stop);
    }
    else
    {
        for(i=0 ; i<BPCB_NUM ; i++) 
        {
            if(bpcb_tb[i].bpcb != NULL)
            {
                battery_percent_callback = bpcb_tb[i].bpcb;
                battery_percent_callback(battery_percent_level);
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[exec_battery_percent_callback] prio_val=%d,battery_percent_level=%d\n",i,battery_percent_level);
            }        
        }
    }
}

int bat_percent_notify_handler(void *unused)
{
    ktime_t ktime;
    int bat_per_val=0;

    do
    {
        ktime = ktime_set(10, 0);

        wait_event_interruptible(bat_percent_notify_waiter, (bat_percent_notify_flag == KAL_TRUE));

        wake_lock(&bat_percent_notify_lock);
        mutex_lock(&bat_percent_notify_mutex);

        bat_per_val=bat_get_ui_percentage();
        
        if( (upmu_get_rgs_chrdet()==0) && (g_battery_percent_level==0) && (bat_per_val<=BAT_PERCENT_LINIT) )
        {
            g_battery_percent_level=1;
            exec_battery_percent_callback(BATTERY_PERCENT_LEVEL_1);
        }
        else if( (g_battery_percent_level==1) && (bat_per_val>BAT_PERCENT_LINIT) )
        {
            g_battery_percent_level=0;
            exec_battery_percent_callback(BATTERY_PERCENT_LEVEL_0);
        }
        else
        {
        }
        bat_percent_notify_flag = KAL_FALSE;
        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "bat_per_level=%d,bat_per_val=%d\n",g_battery_percent_level,bat_per_val);

        mutex_unlock(&bat_percent_notify_mutex);
        wake_unlock(&bat_percent_notify_lock);
       
        hrtimer_start(&bat_percent_notify_timer, ktime, HRTIMER_MODE_REL);    
        
    } while (!kthread_should_stop());
    
    return 0;
}

enum hrtimer_restart bat_percent_notify_task(struct hrtimer *timer)
{
    bat_percent_notify_flag = KAL_TRUE; 
    wake_up_interruptible(&bat_percent_notify_waiter);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "bat_percent_notify_task is called\n");
    
    return HRTIMER_NORESTART;
}

void bat_percent_notify_init(void)
{
    ktime_t ktime;

    ktime = ktime_set(20, 0);
    hrtimer_init(&bat_percent_notify_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    bat_percent_notify_timer.function = bat_percent_notify_task;    
    hrtimer_start(&bat_percent_notify_timer, ktime, HRTIMER_MODE_REL);

    bat_percent_notify_thread = kthread_run(bat_percent_notify_handler, 0, "bat_percent_notify_thread");
    if (IS_ERR(bat_percent_notify_thread))
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Failed to create bat_percent_notify_thread\n");
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Create bat_percent_notify_thread : done\n");
    }
}


//==============================================================================
// PMIC Interrupt service
//==============================================================================
static DEFINE_MUTEX(pmic_mutex_mt6325);
static struct task_struct *pmic_6325_thread_handle = NULL;
struct wake_lock pmicThread_lock_mt6325;

void wake_up_pmic_mt6325(void)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[wake_up_pmic_mt6325]\r\n");
    wake_up_process(pmic_6325_thread_handle);
    wake_lock(&pmicThread_lock_mt6325);    
}
EXPORT_SYMBOL(wake_up_pmic_mt6325);

#ifdef PMIC_EINT_SERVICE
void cust_pmic_interrupt_en_setting_mt6325(void)
{
    //CON0
    mt6325_upmu_set_rg_int_en_pwrkey(1);
    mt6325_upmu_set_rg_int_en_homekey(1);
    mt6325_upmu_set_rg_int_en_pwrkey_r(0);
    mt6325_upmu_set_rg_int_en_homekey_r(0);
    mt6325_upmu_set_rg_int_en_thr_h(0);
    mt6325_upmu_set_rg_int_en_thr_l(0);

    #ifdef LOW_BATTERY_PROTECT
    // move to lbat_xxx_en_setting
    #else
    mt6325_upmu_set_rg_int_en_bat_h(0);
    mt6325_upmu_set_rg_int_en_bat_l(0);
    #endif
    
    mt6325_upmu_set_rg_int_en_bif(0);
    mt6325_upmu_set_rg_int_en_rtc(1);
    mt6325_upmu_set_rg_int_en_audio(0);
    mt6325_upmu_set_rg_int_en_vow(VOW_ENABLE);
    //mt6325_upmu_set_rg_int_en_accdet(0);
    //mt6325_upmu_set_rg_int_en_accdet_eint(0);
    //mt6325_upmu_set_rg_int_en_accdet_negv(0);
    mt6325_upmu_set_rg_int_en_ni_lbat_int(0);

    //CON1
    mt6325_upmu_set_rg_int_en_vdvfs11_oc(0);
    mt6325_upmu_set_rg_int_en_vdvfs12_oc(0);
    mt6325_upmu_set_rg_int_en_vrf18_0_oc(0);
    mt6325_upmu_set_rg_int_en_vdram_oc(0);
    mt6325_upmu_set_rg_int_en_vgpu_oc(0);
    mt6325_upmu_set_rg_int_en_vcore1_oc(0);
    mt6325_upmu_set_rg_int_en_vcore2_oc(0);
    mt6325_upmu_set_rg_int_en_vio18_oc(0);
    mt6325_upmu_set_rg_int_en_vpa_oc(0);
    mt6325_upmu_set_rg_int_en_ldo_oc(0);
    mt6325_upmu_set_rg_int_en_bat2_h(0);
    mt6325_upmu_set_rg_int_en_bat2_l(0);
    mt6325_upmu_set_rg_int_en_vismps0_h(0);
    mt6325_upmu_set_rg_int_en_vismps0_l(0);
    mt6325_upmu_set_rg_int_en_auxadc_imp(0);

    //CON2
    mt6325_upmu_set_rg_int_en_ov(0);
    mt6325_upmu_set_rg_int_en_bvalid_det(0);
    mt6325_upmu_set_rg_int_en_vbaton_undet(0);
    mt6325_upmu_set_rg_int_en_watchdog(0);
    mt6325_upmu_set_rg_int_en_pchr_cm_vdec(0);
    mt6325_upmu_set_rg_int_en_chrdet(1);
    mt6325_upmu_set_rg_int_en_pchr_cm_vinc(0);
    mt6325_upmu_set_rg_int_en_fg_bat_h(0);
    mt6325_upmu_set_rg_int_en_fg_bat_l(0);

    #ifdef BATTERY_OC_PROTECT
    // move to bat_oc_x_en_setting
    #else
    mt6325_upmu_set_rg_int_en_fg_cur_h(0);
    mt6325_upmu_set_rg_int_en_fg_cur_l(0);
    #endif
    
    mt6325_upmu_set_rg_int_en_fg_zcv(0);
    mt6325_upmu_set_rg_int_en_spkl_d(0);
    mt6325_upmu_set_rg_int_en_spkl_ab(0);
}


#if 0 //defined(CONFIG_MTK_FPGA)
void kpd_pwrkey_pmic_handler(unsigned long pressed)
{
    printk("no kpd_pwrkey_pmic_handler at FPGA\n");
}
#else
extern void kpd_pwrkey_pmic_handler(unsigned long pressed);
#endif

void pwrkey_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pwrkey_int_handler]....\n");
    
    if(mt6325_upmu_get_pwrkey_deb()==1)                
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pwrkey_int_handler] Release pwrkey\n");
        
        #if defined (MTK_KERNEL_POWER_OFF_CHARGING)
        if(g_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT && timer_pre != 0)
        {
                timer_pos = sched_clock();
                if(timer_pos - timer_pre >= LONG_PWRKEY_PRESS_TIME)
                {
                    long_pwrkey_press = true;
                }
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_thread_kthread] timer_pos = %ld, timer_pre = %ld, timer_pos-timer_pre = %ld, long_pwrkey_press = %d\r\n",timer_pos, timer_pre, timer_pos-timer_pre, long_pwrkey_press);
                if(long_pwrkey_press)   //500ms
                {
                    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_thread_kthread] Power Key Pressed during kernel power off charging, reboot OS\r\n");
                    arch_reset(0, NULL);
                }
        }        
        #endif
        
        kpd_pwrkey_pmic_handler(0x0);
        //mt6325_upmu_set_rg_pwrkey_int_sel(0);
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pwrkey_int_handler] Press pwrkey\n");
        
        #if defined (MTK_KERNEL_POWER_OFF_CHARGING)
        if(g_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT)
        {
            timer_pre = sched_clock();
        }
        #endif
        kpd_pwrkey_pmic_handler(0x1);
        //mt6325_upmu_set_rg_pwrkey_int_sel(1);
    }
    
    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,0);    
}

#if 0 //defined(CONFIG_MTK_FPGA)
void kpd_pmic_rstkey_handler(unsigned long pressed)
{
    printk("no kpd_pmic_rstkey_handler at FPGA\n");
}
#else
extern void kpd_pmic_rstkey_handler(unsigned long pressed);
#endif

void homekey_int_handler(void)
{
    kal_uint32 ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[homekey_int_handler]....\n");
    
    if(mt6325_upmu_get_homekey_deb()==1)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[homekey_int_handler] Release homekey\n");
        kpd_pmic_rstkey_handler(0x0);
        //mt6325_upmu_set_rg_homekey_int_sel(0);
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[homekey_int_handler] Press homekey\n");
        kpd_pmic_rstkey_handler(0x1);
        //mt6325_upmu_set_rg_homekey_int_sel(1);
    }
    
    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,1);    
}

void pwrkey_r_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pwrkey_r_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,2);
}

void homekey_r_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[homekey_r_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,3);
}

void thr_h_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[thr_h_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,4);
}

void thr_l_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[thr_l_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,5);
}

void bat_h_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[bat_h_int_handler]....\n");
    
    //sub-task
#ifdef LOW_BATTERY_PROTECT
    g_low_battery_level=0;
    exec_low_battery_callback(LOW_BATTERY_LEVEL_0);

    #if 0
    lbat_max_en_setting(0);
    mdelay(1);
    lbat_min_en_setting(1);
    #else
    
    mt6325_upmu_set_rg_lbat_volt_min(BAT_LV_1_THD);
    
    lbat_min_en_setting(0);
    lbat_max_en_setting(0);
    mdelay(1);
    lbat_min_en_setting(1);   
    #endif
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
            MT6325_AUXADC_CON5, upmu_get_reg_value(MT6325_AUXADC_CON5),
            MT6325_AUXADC_CON6, upmu_get_reg_value(MT6325_AUXADC_CON6),
            MT6325_INT_CON0, upmu_get_reg_value(MT6325_INT_CON0)
            );
#endif 

    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,6);
}

void bat_l_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[bat_l_int_handler]....\n");
    
    //sub-task
#ifdef LOW_BATTERY_PROTECT
    g_low_battery_level++;
    if(g_low_battery_level > 2)
       g_low_battery_level = 2; 

    if(g_low_battery_level==1)      
        exec_low_battery_callback(LOW_BATTERY_LEVEL_1);
    else if(g_low_battery_level==2) 
        exec_low_battery_callback(LOW_BATTERY_LEVEL_2);
    else                            
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[bat_l_int_handler]err,g_low_battery_level=%d\n", g_low_battery_level);

    #if 0
    lbat_min_en_setting(0);
    mdelay(1);
    lbat_max_en_setting(1);
    #else
    
    mt6325_upmu_set_rg_lbat_volt_min(BAT_LV_2_THD);
        
    lbat_min_en_setting(0);
    lbat_max_en_setting(0);
    mdelay(1);
    if(g_low_battery_level<2)
    {
        lbat_min_en_setting(1);
    }
    lbat_max_en_setting(1);
    #endif

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
            MT6325_AUXADC_CON5, upmu_get_reg_value(MT6325_AUXADC_CON5),
            MT6325_AUXADC_CON6, upmu_get_reg_value(MT6325_AUXADC_CON6),
            MT6325_INT_CON0, upmu_get_reg_value(MT6325_INT_CON0)
            );
#endif 

    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,7);
}

#ifdef MTK_PMIC_DVT_SUPPORT
extern void tc_bif_1008_step_1(void);//DVT
#endif

void bif_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[bif_int_handler]....\n");

    #ifdef MTK_PMIC_DVT_SUPPORT
    tc_bif_1008_step_1();//DVT
    #endif
    
    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,8);
}

void rtc_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[rtc_int_handler]....\n");
    //sub-task
    rtc_irq_handler();
    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,9);
}

void audio_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[audio_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,10);
}

void vow_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vow_int_handler]....\n");
    
    #if defined (MTK_VOW_SUPPORT) && defined (MTK_VOW_TEST)
    printk("vow_int_handler, vow_irq_handler\n");
    vow_irq_handler();
    #endif
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,11);
}


#if defined(CONFIG_MTK_ACCDET)
extern int accdet_irq_handler(void);
#endif

void accdet_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[accdet_int_handler]....\n");
	
	#if defined(CONFIG_MTK_ACCDET)
	ret = accdet_irq_handler();
	#endif
	if(0 == ret){
		xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[accdet_int_handler] don't finished\n");
	}

    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,12);
}

void accdet_eint_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[accdet_eint_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,13);
}

void accdet_negv_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[accdet_negv_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,14);
}

void ni_lbat_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[ni_lbat_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS0,0x1,0x1,15);
}

void vdvfs11_oc_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vdvfs11_oc_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,0);
}

void vdvfs12_oc_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vdvfs12_oc_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,1);
}

void vrf18_0_oc_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vrf18_0_oc_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,2);
}

void vdram_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vdram_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,3);
}

void vgpu_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vgpu_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,4);
}

void vcore1_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vcore1_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,5);
}

void vcore2_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vcore2_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,6);
}

void vio18_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vio18_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,7);
}

void vpa_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vpa_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,8);
}

void ldo_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[ldo_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,9);
}

void bat2_h_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[bat2_h_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,10);
}

void bat2_l_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[bat2_l_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,11);
}

void vismps0_h_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vismps0_h_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,12);
}

void vismps0_l_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vismps0_l_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,13);
}

void auxadc_imp_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[auxadc_imp_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS1,0x1,0x1,14);
}

void ov_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[ov_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS2,0x1,0x1,0);
}

void bvalid_det_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[bvalid_det_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS2,0x1,0x1,1);
}

void vbaton_undet_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[vbaton_undet_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS2,0x1,0x1,2);
}

void watchdog_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[watchdog_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS2,0x1,0x1,3);
}

void pchr_cm_vdec_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pchr_cm_vdec_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS2,0x1,0x1,4);
}

void chrdet_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[chrdet_int_handler]....\n");

    #ifdef MTK_KERNEL_POWER_OFF_CHARGING
    #ifndef MTK_DUAL_INPUT_CHARGER_SUPPORT
    if (!upmu_get_rgs_chrdet())
    {
        int boot_mode = 0;
        boot_mode = get_boot_mode();
        
        if(boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT || boot_mode == LOW_POWER_OFF_CHARGING_BOOT)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[chrdet_int_handler] Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
            mt_power_off();
        }
    }
    #endif
    #else
    upmu_get_rgs_chrdet();
    #endif

    mt6325_upmu_set_rg_usbdl_rst(1);
    //xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Before charging task, mt6325_upmu_set_rg_usbdl_rst(1)\n");

    do_chrdet_int_task();
    
    ret=pmic_config_interface(MT6325_INT_STATUS2,0x1,0x1,5);
}

void pchr_cm_vinc_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pchr_cm_vinc_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS2,0x1,0x1,6);
}

void fg_bat_h_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[fg_bat_h_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS2,0x1,0x1,7);
}

void fg_bat_l_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[fg_bat_l_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS2,0x1,0x1,8);
}

void fg_cur_h_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[fg_cur_h_int_handler]....\n");
    
    //sub-task
#ifdef BATTERY_OC_PROTECT
    g_battery_oc_level=0;
    exec_battery_oc_callback(BATTERY_OC_LEVEL_0);
    bat_oc_h_en_setting(0);
    bat_oc_l_en_setting(0);
    mdelay(1);
    bat_oc_l_en_setting(1);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
        MT6325_FGADC_CON23, upmu_get_reg_value(MT6325_FGADC_CON23),
        MT6325_FGADC_CON24, upmu_get_reg_value(MT6325_FGADC_CON24),
        MT6325_INT_CON2, upmu_get_reg_value(MT6325_INT_CON2)
        );
#endif 

    ret=pmic_config_interface(MT6325_INT_STATUS2,0x1,0x1,9);
}

void fg_cur_l_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[fg_cur_l_int_handler]....\n");
    
    //sub-task
#ifdef BATTERY_OC_PROTECT
    g_battery_oc_level=1;
    exec_battery_oc_callback(BATTERY_OC_LEVEL_1);       
    bat_oc_h_en_setting(0);
    bat_oc_l_en_setting(0);
    mdelay(1);
    bat_oc_h_en_setting(1);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
        MT6325_FGADC_CON23, upmu_get_reg_value(MT6325_FGADC_CON23),
        MT6325_FGADC_CON24, upmu_get_reg_value(MT6325_FGADC_CON24),
        MT6325_INT_CON2, upmu_get_reg_value(MT6325_INT_CON2)
        );
#endif
    
    ret=pmic_config_interface(MT6325_INT_STATUS2,0x1,0x1,10);
}

void fg_zcv_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[fg_zcv_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS2,0x1,0x1,11);
}

void spkl_d_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[spkl_d_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS2,0x1,0x1,12);
}

void spkl_ab_int_handler(void)
{
    kal_uint32 ret=0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[spkl_ab_int_handler]....\n");
    //sub-task
    ret=pmic_config_interface(MT6325_INT_STATUS2,0x1,0x1,13);
}


static void mt6325_int_handler(void)
{
    kal_uint32 ret=0;
    kal_uint32 mt6325_int_status_val_0=0;
    kal_uint32 mt6325_int_status_val_1=0;
    kal_uint32 mt6325_int_status_val_2=0;

    //--------------------------------------------------------------------------------
    ret=pmic_read_interface(MT6325_INT_STATUS0,(&mt6325_int_status_val_0),0xFFFF,0x0);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC_INT] mt6325_int_status_val_0=0x%x\n", mt6325_int_status_val_0);

    if( (((mt6325_int_status_val_0)&(0x0001))>>0)  == 1 )  { pwrkey_int_handler(); }
    if( (((mt6325_int_status_val_0)&(0x0002))>>1)  == 1 )  { homekey_int_handler(); }         
    if( (((mt6325_int_status_val_0)&(0x0004))>>2)  == 1 )  { pwrkey_r_int_handler(); }
    if( (((mt6325_int_status_val_0)&(0x0008))>>3)  == 1 )  { homekey_r_int_handler(); }
    if( (((mt6325_int_status_val_0)&(0x0010))>>4)  == 1 )  { thr_h_int_handler(); }
    if( (((mt6325_int_status_val_0)&(0x0020))>>5)  == 1 )  { thr_l_int_handler(); }
    if( (((mt6325_int_status_val_0)&(0x0040))>>6)  == 1 )  { bat_h_int_handler(); }
    if( (((mt6325_int_status_val_0)&(0x0080))>>7)  == 1 )  { bat_l_int_handler(); }
    if( (((mt6325_int_status_val_0)&(0x0100))>>8)  == 1 )  { bif_int_handler(); }
    if( (((mt6325_int_status_val_0)&(0x0200))>>9)  == 1 )  { rtc_int_handler(); }
    if( (((mt6325_int_status_val_0)&(0x0400))>>10) == 1 )  { audio_int_handler(); }
    if( (((mt6325_int_status_val_0)&(0x0800))>>11) == 1 )  { vow_int_handler(); }
    if( (((mt6325_int_status_val_0)&(0x1000))>>12) == 1 )  { accdet_int_handler(); }
    if( (((mt6325_int_status_val_0)&(0x2000))>>13) == 1 )  { accdet_eint_int_handler(); }
    if( (((mt6325_int_status_val_0)&(0x4000))>>14) == 1 )  { accdet_negv_int_handler(); }
    if( (((mt6325_int_status_val_0)&(0x8000))>>15) == 1 )  { ni_lbat_int_handler(); }
                 
    //--------------------------------------------------------------------------------
    ret=pmic_read_interface(MT6325_INT_STATUS1,(&mt6325_int_status_val_1),0xFFFF,0x0);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC_INT] mt6325_int_status_val_1=0x%x\n", mt6325_int_status_val_1);

    if( (((mt6325_int_status_val_1)&(0x0001))>>0)  == 1 )  { vdvfs11_oc_int_handler(); }
    if( (((mt6325_int_status_val_1)&(0x0002))>>1)  == 1 )  { vdvfs12_oc_int_handler(); }
    if( (((mt6325_int_status_val_1)&(0x0004))>>2)  == 1 )  { vrf18_0_oc_int_handler(); }
    if( (((mt6325_int_status_val_1)&(0x0008))>>3)  == 1 )  { vdram_int_handler(); }
    if( (((mt6325_int_status_val_1)&(0x0010))>>4)  == 1 )  { vgpu_int_handler(); }
    if( (((mt6325_int_status_val_1)&(0x0020))>>5)  == 1 )  { vcore1_int_handler(); }
    if( (((mt6325_int_status_val_1)&(0x0040))>>6)  == 1 )  { vcore2_int_handler(); }
    if( (((mt6325_int_status_val_1)&(0x0080))>>7)  == 1 )  { vio18_int_handler(); }
    if( (((mt6325_int_status_val_1)&(0x0100))>>8)  == 1 )  { vpa_int_handler(); }
    if( (((mt6325_int_status_val_1)&(0x0200))>>9)  == 1 )  { ldo_int_handler(); }
    if( (((mt6325_int_status_val_1)&(0x0400))>>10) == 1 )  { bat2_h_int_handler(); }
    if( (((mt6325_int_status_val_1)&(0x0800))>>11) == 1 )  { bat2_l_int_handler(); }
    if( (((mt6325_int_status_val_1)&(0x1000))>>12) == 1 )  { vismps0_h_int_handler(); }
    if( (((mt6325_int_status_val_1)&(0x2000))>>13) == 1 )  { vismps0_l_int_handler(); }
    if( (((mt6325_int_status_val_1)&(0x4000))>>14) == 1 )  { auxadc_imp_int_handler(); }

    //--------------------------------------------------------------------------------
    ret=pmic_read_interface(MT6325_INT_STATUS2,(&mt6325_int_status_val_2),0xFFFF,0x0);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[PMIC_INT] mt6325_int_status_val_2=0x%x\n", mt6325_int_status_val_2);

    if( (((mt6325_int_status_val_2)&(0x0001))>>0)  == 1 )  { ov_int_handler(); }
    if( (((mt6325_int_status_val_2)&(0x0002))>>1)  == 1 )  { bvalid_det_int_handler(); }
    if( (((mt6325_int_status_val_2)&(0x0004))>>2)  == 1 )  { vbaton_undet_int_handler(); }
    if( (((mt6325_int_status_val_2)&(0x0008))>>3)  == 1 )  { watchdog_int_handler(); }
    if( (((mt6325_int_status_val_2)&(0x0010))>>4)  == 1 )  { pchr_cm_vdec_int_handler(); }
    if( (((mt6325_int_status_val_2)&(0x0020))>>5)  == 1 )  { chrdet_int_handler(); }
    if( (((mt6325_int_status_val_2)&(0x0040))>>6)  == 1 )  { pchr_cm_vinc_int_handler(); }
    if( (((mt6325_int_status_val_2)&(0x0080))>>7)  == 1 )  { fg_bat_h_int_handler(); }
    if( (((mt6325_int_status_val_2)&(0x0100))>>8)  == 1 )  { fg_bat_l_int_handler(); }
    if( (((mt6325_int_status_val_2)&(0x0200))>>9)  == 1 )  { fg_cur_h_int_handler(); }
    if( (((mt6325_int_status_val_2)&(0x0400))>>10) == 1 )  { fg_cur_l_int_handler(); }
    if( (((mt6325_int_status_val_2)&(0x0800))>>11) == 1 )  { fg_zcv_int_handler(); }
    if( (((mt6325_int_status_val_2)&(0x1000))>>12) == 1 )  { spkl_d_int_handler(); }
    if( (((mt6325_int_status_val_2)&(0x2000))>>13) == 1 )  { spkl_ab_int_handler(); }
}


static int pmic_thread_kthread_mt6325(void *x)
{
#if 1
    kal_uint32 ret=0;
    kal_uint32 mt6325_int_status_val_0=0;
    kal_uint32 mt6325_int_status_val_1=0;
    kal_uint32 mt6325_int_status_val_2=0;
    U32 pwrap_eint_status=0;
    struct sched_param param = { .sched_priority = 98 };

    sched_setscheduler(current, SCHED_FIFO, &param);
    set_current_state(TASK_INTERRUPTIBLE);

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_INT] enter\n");

    /* Run on a process content */
    while (1) {
        mutex_lock(&pmic_mutex_mt6325);

        pwrap_eint_status = pmic_wrap_eint_status();    
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_INT] pwrap_eint_status=0x%x\n", pwrap_eint_status);

        mt6325_int_handler();
        
        pmic_wrap_eint_clr(0x0);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_INT] pmic_wrap_eint_clr(0x0);\n");

        //mdelay(1);
        //mt_eint_unmask(g_eint_pmit_mt6325_num);

        cust_pmic_interrupt_en_setting_mt6325();

        ret=pmic_read_interface(MT6325_INT_STATUS0,(&mt6325_int_status_val_0),0xFFFF,0x0);
        ret=pmic_read_interface(MT6325_INT_STATUS1,(&mt6325_int_status_val_1),0xFFFF,0x0);
        ret=pmic_read_interface(MT6325_INT_STATUS2,(&mt6325_int_status_val_2),0xFFFF,0x0);

        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_INT] after ,mt6325_int_status_val_0=0x%x\n", mt6325_int_status_val_0);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_INT] after ,mt6325_int_status_val_1=0x%x\n", mt6325_int_status_val_1);   
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_INT] after ,mt6325_int_status_val_2=0x%x\n", mt6325_int_status_val_2);   

        mdelay(1);
        
        mutex_unlock(&pmic_mutex_mt6325);
        wake_unlock(&pmicThread_lock_mt6325);

        set_current_state(TASK_INTERRUPTIBLE);        
        mt_eint_unmask(g_eint_pmit_mt6325_num);
        schedule();
    }
#endif

    return 0;
}

void mt_pmic_eint_irq_mt6325(void)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt_pmic_eint_irq_mt6325] receive interrupt\n");
    wake_up_pmic_mt6325();
    return ;
}

void PMIC_EINT_SETTING(void)
{
    //ON/OFF interrupt
    cust_pmic_interrupt_en_setting_mt6325();
        
    mt_eint_set_hw_debounce(g_eint_pmit_mt6325_num, g_cust_eint_mt_pmic_debounce_cn);
        
    mt_eint_registration(g_eint_pmit_mt6325_num,g_cust_eint_mt_pmic_type,mt_pmic_eint_irq_mt6325,0);
        
    mt_eint_unmask(g_eint_pmit_mt6325_num);    
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CUST_EINT] CUST_EINT_MT_PMIC_MT6325_NUM=%d\n", g_eint_pmit_mt6325_num);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_CN=%d\n", g_cust_eint_mt_pmic_debounce_cn);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CUST_EINT] CUST_EINT_PMIC_TYPE=%d\n", g_cust_eint_mt_pmic_type);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_EN=%d\n", g_cust_eint_mt_pmic_debounce_en);

    //for all interrupt events, turn on interrupt module clock
    mt6325_upmu_set_rg_intrp_ck_pdn(0);     
}
#endif // PMIC_EINT_SERVICE


//==============================================================================
// PMIC read/write APIs
//==============================================================================
#if 0 //defined(CONFIG_MTK_FPGA)
    // no CONFIG_PMIC_HW_ACCESS_EN
#else
    #define CONFIG_PMIC_HW_ACCESS_EN
#endif

static DEFINE_MUTEX(pmic_access_mutex);

U32 pmic_read_interface (U32 RegNum, U32 *val, U32 MASK, U32 SHIFT)
{
    U32 return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
    U32 pmic_reg = 0;
    U32 rdata;

    mutex_lock(&pmic_access_mutex);

    //mt_read_byte(RegNum, &pmic_reg);
    return_value= pwrap_wacs2(0, (RegNum), 0, &rdata);
    pmic_reg=rdata;
    if(return_value!=0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_read_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
        mutex_unlock(&pmic_access_mutex);
        return return_value;
    }
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_read_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg);

    pmic_reg &= (MASK << SHIFT);
    *val = (pmic_reg >> SHIFT);
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_read_interface] val=0x%x\n", *val);

    mutex_unlock(&pmic_access_mutex);
#else
    //xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_read_interface] Can not access HW PMIC\n");
#endif

    return return_value;
}

U32 pmic_config_interface (U32 RegNum, U32 val, U32 MASK, U32 SHIFT)
{
    U32 return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
    U32 pmic_reg = 0;
    U32 rdata;

    mutex_lock(&pmic_access_mutex);

    //1. mt_read_byte(RegNum, &pmic_reg);
    return_value= pwrap_wacs2(0, (RegNum), 0, &rdata);
    pmic_reg=rdata;
    if(return_value!=0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
        mutex_unlock(&pmic_access_mutex);
        return return_value;
    }
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_config_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg);

    pmic_reg &= ~(MASK << SHIFT);
    pmic_reg |= (val << SHIFT);

    //2. mt_write_byte(RegNum, pmic_reg);
    return_value= pwrap_wacs2(1, (RegNum), pmic_reg, &rdata);
    if(return_value!=0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
        mutex_unlock(&pmic_access_mutex);
        return return_value;
    }
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_config_interface] write Reg[%x]=0x%x\n", RegNum, pmic_reg);

    #if 0
    //3. Double Check
    //mt_read_byte(RegNum, &pmic_reg);
    return_value= pwrap_wacs2(0, (RegNum), 0, &rdata);
    pmic_reg=rdata;
    if(return_value!=0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_config_interface] Reg[%x]= pmic_wrap write data fail\n", RegNum);
        mutex_unlock(&pmic_access_mutex);
        return return_value;
    }
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_config_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg);
    #endif

    mutex_unlock(&pmic_access_mutex);
#else
    //xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_config_interface] Can not access HW PMIC\n");
#endif

    return return_value;
}

//==============================================================================
// PMIC read/write APIs : nolock
//==============================================================================
U32 pmic_read_interface_nolock (U32 RegNum, U32 *val, U32 MASK, U32 SHIFT)
{
    U32 return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
    U32 pmic_reg = 0;
    U32 rdata;

    //mt_read_byte(RegNum, &pmic_reg);
    return_value= pwrap_wacs2(0, (RegNum), 0, &rdata);
    pmic_reg=rdata;
    if(return_value!=0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_read_interface_nolock] Reg[%x]= pmic_wrap read data fail\n", RegNum);
        return return_value;
    }
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_read_interface_nolock] Reg[%x]=0x%x\n", RegNum, pmic_reg);

    pmic_reg &= (MASK << SHIFT);
    *val = (pmic_reg >> SHIFT);
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_read_interface_nolock] val=0x%x\n", *val);
#else
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_read_interface_nolock] Can not access HW PMIC\n");
#endif

    return return_value;
}

U32 pmic_config_interface_nolock (U32 RegNum, U32 val, U32 MASK, U32 SHIFT)
{
    U32 return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
    U32 pmic_reg = 0;
    U32 rdata;

    //1. mt_read_byte(RegNum, &pmic_reg);
    return_value= pwrap_wacs2(0, (RegNum), 0, &rdata);
    pmic_reg=rdata;
    if(return_value!=0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_config_interface_nolock] Reg[%x]= pmic_wrap read data fail\n", RegNum);
        return return_value;
    }
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_config_interface_nolock] Reg[%x]=0x%x\n", RegNum, pmic_reg);

    pmic_reg &= ~(MASK << SHIFT);
    pmic_reg |= (val << SHIFT);

    //2. mt_write_byte(RegNum, pmic_reg);
    return_value= pwrap_wacs2(1, (RegNum), pmic_reg, &rdata);
    if(return_value!=0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_config_interface_nolock] Reg[%x]= pmic_wrap read data fail\n", RegNum);
        return return_value;
    }
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_config_interface_nolock] write Reg[%x]=0x%x\n", RegNum, pmic_reg);
#else
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_config_interface_nolock] Can not access HW PMIC\n");
#endif

    return return_value;
}


//==============================================================================
// mt-pmic dev_attr APIs
//==============================================================================
U32 g_reg_value=0;
static ssize_t show_pmic_access(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_pmic_access] 0x%x\n", g_reg_value);
    return sprintf(buf, "%u\n", g_reg_value);
}
static ssize_t store_pmic_access(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    int ret=0;
    char *pvalue = NULL;
    U32 reg_value = 0;
    U32 reg_address = 0;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_pmic_access] \n");
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_pmic_access] buf is %s and size is %d \n",buf,size);
        reg_address = simple_strtoul(buf,&pvalue,16);

        if(size > 5)
        {
            reg_value = simple_strtoul((pvalue+1),NULL,16);
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_pmic_access] write PMU reg 0x%x with value 0x%x !\n",reg_address,reg_value);
            ret=pmic_config_interface(reg_address, reg_value, 0xFFFF, 0x0);
        }
        else
        {
            ret=pmic_read_interface(reg_address, &g_reg_value, 0xFFFF, 0x0);
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_pmic_access] read PMU reg 0x%x with value 0x%x !\n",reg_address,g_reg_value);
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_pmic_access] Please use \"cat pmic_access\" to get value\r\n");
        }
    }
    return size;
}
static DEVICE_ATTR(pmic_access, 0664, show_pmic_access, store_pmic_access); //664

//==============================================================================
// EM : enable status
//==============================================================================
#if 1

static ssize_t show_BUCK_VDVFS11_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x0001))>>0);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS11_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS11_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS11_STATUS, 0664, show_BUCK_VDVFS11_STATUS, store_BUCK_VDVFS11_STATUS);

static ssize_t show_BUCK_VDVFS12_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x0002))>>1);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS12_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS12_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS12_STATUS, 0664, show_BUCK_VDVFS12_STATUS, store_BUCK_VDVFS12_STATUS);

static ssize_t show_BUCK_11_VDVFS11_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret=0;
    kal_uint32 ret_value=0;
    kal_uint32 reg_address=0x808A;
		
	ret = pmic_read_interface(reg_address, &ret_value, 0x1, 4);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_11_VDVFS11_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_11_VDVFS11_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_11_VDVFS11_STATUS, 0664, show_BUCK_11_VDVFS11_STATUS, store_BUCK_11_VDVFS11_STATUS);

static ssize_t show_BUCK_11_VDVFS12_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret=0;
    kal_uint32 ret_value=0;
    kal_uint32 reg_address=0x8098;
		
	ret = pmic_read_interface(reg_address, &ret_value, 0x1, 4);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_11_VDVFS12_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_11_VDVFS12_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_11_VDVFS12_STATUS, 0664, show_BUCK_11_VDVFS12_STATUS, store_BUCK_11_VDVFS12_STATUS);

static ssize_t show_LDO_VBIF28_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_LDO_CON4);
    ret_value = (((val)&(0x8000))>>15);	
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VBIF28_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VBIF28_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VBIF28_STATUS, 0664, show_LDO_VBIF28_STATUS, store_LDO_VBIF28_STATUS);

static ssize_t show_BUCK_VDRAM_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x0004))>>2);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDRAM_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDRAM_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDRAM_STATUS, 0664, show_BUCK_VDRAM_STATUS, store_BUCK_VDRAM_STATUS);

static ssize_t show_BUCK_VRF18_0_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x0008))>>3);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VRF18_0_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VRF18_0_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VRF18_0_STATUS, 0664, show_BUCK_VRF18_0_STATUS, store_BUCK_VRF18_0_STATUS);

static ssize_t show_BUCK_VGPU_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x0010))>>4);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VGPU_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VGPU_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VGPU_STATUS, 0664, show_BUCK_VGPU_STATUS, store_BUCK_VGPU_STATUS);

static ssize_t show_BUCK_VCORE1_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x0020))>>5);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VCORE1_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VCORE1_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VCORE1_STATUS, 0664, show_BUCK_VCORE1_STATUS, store_BUCK_VCORE1_STATUS);

static ssize_t show_BUCK_VCORE2_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x0040))>>6);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VCORE2_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VCORE2_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VCORE2_STATUS, 0664, show_BUCK_VCORE2_STATUS, store_BUCK_VCORE2_STATUS);

static ssize_t show_BUCK_VIO18_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x0080))>>7);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VIO18_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VIO18_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VIO18_STATUS, 0664, show_BUCK_VIO18_STATUS, store_BUCK_VIO18_STATUS);

static ssize_t show_BUCK_VPA_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x0100))>>8);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VPA_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VPA_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VPA_STATUS, 0664, show_BUCK_VPA_STATUS, store_BUCK_VPA_STATUS);

static ssize_t show_LDO_VRTC_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x0200))>>9);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VRTC_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VRTC_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VRTC_STATUS, 0664, show_LDO_VRTC_STATUS, store_LDO_VRTC_STATUS);

static ssize_t show_LDO_VTCXO0_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x0400))>>10);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VTCXO0_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VTCXO0_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VTCXO0_STATUS, 0664, show_LDO_VTCXO0_STATUS, store_LDO_VTCXO0_STATUS);

static ssize_t show_LDO_VTCXO1_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x0800))>>11);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VTCXO1_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VTCXO1_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VTCXO1_STATUS, 0664, show_LDO_VTCXO1_STATUS, store_LDO_VTCXO1_STATUS);

static ssize_t show_LDO_VAUD28_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x1000))>>12);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VAUD28_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VAUD28_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VAUD28_STATUS, 0664, show_LDO_VAUD28_STATUS, store_LDO_VAUD28_STATUS);

static ssize_t show_LDO_VAUXA28_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x2000))>>13);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VAUXA28_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VAUXA28_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VAUXA28_STATUS, 0664, show_LDO_VAUXA28_STATUS, store_LDO_VAUXA28_STATUS);

static ssize_t show_LDO_VCAMA_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x4000))>>14);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAMA_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAMA_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAMA_STATUS, 0664, show_LDO_VCAMA_STATUS, store_LDO_VCAMA_STATUS);

static ssize_t show_LDO_VIO28_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x8000))>>15);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VIO28_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VIO28_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VIO28_STATUS, 0664, show_LDO_VIO28_STATUS, store_LDO_VIO28_STATUS);

static ssize_t show_LDO_VCAM_AF_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS1);
    ret_value = (((val)&(0x0001))>>0);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAM_AF_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAM_AF_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAM_AF_STATUS, 0664, show_LDO_VCAM_AF_STATUS, store_LDO_VCAM_AF_STATUS);

static ssize_t show_LDO_VMC_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS1);
    ret_value = (((val)&(0x0002))>>1);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VMC_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VMC_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VMC_STATUS, 0664, show_LDO_VMC_STATUS, store_LDO_VMC_STATUS);

static ssize_t show_LDO_VMCH_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS1);
    ret_value = (((val)&(0x0004))>>2);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VMCH_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VMCH_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VMCH_STATUS, 0664, show_LDO_VMCH_STATUS, store_LDO_VMCH_STATUS);

static ssize_t show_LDO_VEMC33_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS1);
    ret_value = (((val)&(0x0008))>>3);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VEMC33_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VEMC33_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VEMC33_STATUS, 0664, show_LDO_VEMC33_STATUS, store_LDO_VEMC33_STATUS);

static ssize_t show_LDO_VGP1_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS1);
    ret_value = (((val)&(0x0010))>>4);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VGP1_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VGP1_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VGP1_STATUS, 0664, show_LDO_VGP1_STATUS, store_LDO_VGP1_STATUS);

static ssize_t show_LDO_VEFUSE_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS1);
    ret_value = (((val)&(0x0020))>>5);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VEFUSE_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VEFUSE_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VEFUSE_STATUS, 0664, show_LDO_VEFUSE_STATUS, store_LDO_VEFUSE_STATUS);

static ssize_t show_LDO_VSIM1_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS1);
    ret_value = (((val)&(0x0040))>>6);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VSIM1_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VSIM1_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VSIM1_STATUS, 0664, show_LDO_VSIM1_STATUS, store_LDO_VSIM1_STATUS);

static ssize_t show_LDO_VSIM2_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS1);
    ret_value = (((val)&(0x0080))>>7);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VSIM2_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VSIM2_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VSIM2_STATUS, 0664, show_LDO_VSIM2_STATUS, store_LDO_VSIM2_STATUS);


static ssize_t show_LDO_VCN28_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS1);
    ret_value = (((val)&(0x0100))>>8);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCN28_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCN28_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCN28_STATUS, 0664, show_LDO_VCN28_STATUS, store_LDO_VCN28_STATUS);

static ssize_t show_LDO_VMIPI_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS1);
    ret_value = (((val)&(0x0200))>>9);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VMIPI_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VMIPI_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VMIPI_STATUS, 0664, show_LDO_VMIPI_STATUS, store_LDO_VMIPI_STATUS);

static ssize_t show_LDO_VCAMD_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS1);
    ret_value = (((val)&(0x0800))>>11);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAMD_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAMD_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAMD_STATUS, 0664, show_LDO_VCAMD_STATUS, store_LDO_VCAMD_STATUS);

static ssize_t show_LDO_VUSB33_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS0);
    ret_value = (((val)&(0x1000))>>12);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VUSB33_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VUSB33_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VUSB33_STATUS, 0664, show_LDO_VUSB33_STATUS, store_LDO_VUSB33_STATUS);

static ssize_t show_LDO_VCAM_IO_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS1);
    ret_value = (((val)&(0x2000))>>13);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAM_IO_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAM_IO_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAM_IO_STATUS, 0664, show_LDO_VCAM_IO_STATUS, store_LDO_VCAM_IO_STATUS);

static ssize_t show_LDO_VSRAM_DVFS1_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS1);
    ret_value = (((val)&(0x4000))>>14);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VSRAM_DVFS1_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VSRAM_DVFS1_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VSRAM_DVFS1_STATUS, 0664, show_LDO_VSRAM_DVFS1_STATUS, store_LDO_VSRAM_DVFS1_STATUS);

static ssize_t show_LDO_VGP2_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS1);
    ret_value = (((val)&(0x8000))>>15);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VGP2_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VGP2_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VGP2_STATUS, 0664, show_LDO_VGP2_STATUS, store_LDO_VGP2_STATUS);

static ssize_t show_LDO_VGP3_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS2);
    ret_value = (((val)&(0x0001))>>0);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VGP3_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VGP3_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VGP3_STATUS, 0664, show_LDO_VGP3_STATUS, store_LDO_VGP3_STATUS);

static ssize_t show_LDO_VBIASN_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS2);
    ret_value = (((val)&(0x0002))>>1);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VBIASN_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VBIASN_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VBIASN_STATUS, 0664, show_LDO_VBIASN_STATUS, store_LDO_VBIASN_STATUS);

static ssize_t show_LDO_VCN33_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS2);
    ret_value = (((val)&(0x0004))>>2);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCN33_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCN33_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCN33_STATUS, 0664, show_LDO_VCN33_STATUS, store_LDO_VCN33_STATUS);

static ssize_t show_LDO_VCN18_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS2);
    ret_value = (((val)&(0x0008))>>3);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCN33_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCN18_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCN18_STATUS, 0664, show_LDO_VCN18_STATUS, store_LDO_VCN18_STATUS);

static ssize_t show_LDO_VRF18_1_STATUS(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 val=0;
    kal_uint32 ret_value=0;
        
    val = upmu_get_reg_value(MT6325_EN_STATUS2);
    ret_value = (((val)&(0x0010))>>4);
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VRF18_1_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VRF18_1_STATUS(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VRF18_1_STATUS, 0664, show_LDO_VRF18_1_STATUS, store_LDO_VRF18_1_STATUS);

//==============================================================================
// EM : vosel status
//==============================================================================

kal_uint32 get_volt_val_hw_buck_ip_v1(kal_uint32 val)
{
    kal_uint32 volt_val=0;        
    volt_val = (val*6250)+600000; 
	//volt_val = volt_val / 1000;
    return volt_val;   //unit:uv
}

kal_uint32 get_volt_val_hw_buck_ip_v2(kal_uint32 val)
{
    kal_uint32 volt_val=0;        
    volt_val = (val*6250)+1400000;
   // volt_val = volt_val / 1000;
    return volt_val;   //unit:uv
}

kal_uint32 get_volt_val_hw_buck_ip_v3(kal_uint32 val)
{
    kal_uint32 volt_val=0;        
    volt_val = (((val*93750)+10500000)+9)/10;
	//volt_val = volt_val / 1000;   
    return volt_val;   //unit:uv
}

kal_uint32 get_volt_val_hw_buck_ip_v4(kal_uint32 val)
{
    kal_uint32 volt_val=0;        
    volt_val = (val*50000)+500000;
   // volt_val = volt_val / 1000;  
    return volt_val;   //unit:uv
}

kal_uint32 get_volt_val_hw_buck_ip_v5(kal_uint32 val)
{
    kal_uint32 volt_val=0;        
    volt_val = (val*6250)+600000;
    volt_val = volt_val*2;
	//volt_val = volt_val / 1000;
    return volt_val;  //unit:uv
}
static ssize_t show_BUCK_VDVFS11_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 reg=0;
    kal_uint32 ret_value=0;
        
	reg = mt6325_upmu_get_ni_vdvfs11_vosel();
	ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS11_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS11_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS11_VOLTAGE, 0664, show_BUCK_VDVFS11_VOLTAGE, store_BUCK_VDVFS11_VOLTAGE);

static ssize_t show_BUCK_VDVFS12_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 reg=0;
    kal_uint32 ret_value=0;
        
	reg = mt6325_upmu_get_ni_vdvfs12_vosel();
	ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS11_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS12_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS12_VOLTAGE, 0664, show_BUCK_VDVFS12_VOLTAGE, store_BUCK_VDVFS12_VOLTAGE);

static ssize_t show_BUCK_VIO18_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6325_upmu_get_ni_vio18_vosel();
    ret_value = get_volt_val_hw_buck_ip_v2(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VIO18_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VIO18_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VIO18_VOLTAGE, 0664, show_BUCK_VIO18_VOLTAGE, store_BUCK_VIO18_VOLTAGE);

static ssize_t show_BUCK_VPA_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6325_upmu_get_ni_vpa_vosel();
    ret_value = get_volt_val_hw_buck_ip_v4(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VPA_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VPA_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VPA_VOLTAGE, 0664, show_BUCK_VPA_VOLTAGE, store_BUCK_VPA_VOLTAGE);

static ssize_t show_BUCK_VCORE1_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6325_upmu_get_ni_vcore1_vosel();
    ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VCORE1_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VCORE1_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VCORE1_VOLTAGE, 0664, show_BUCK_VCORE1_VOLTAGE, store_BUCK_VCORE1_VOLTAGE);

static ssize_t show_BUCK_VCORE2_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6325_upmu_get_ni_vcore2_vosel();
    ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VCORE2_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VCORE2_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VCORE2_VOLTAGE, 0664, show_BUCK_VCORE2_VOLTAGE, store_BUCK_VCORE2_VOLTAGE);

static ssize_t show_BUCK_VDRAM_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6325_upmu_get_ni_vdram_vosel();
    ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDRAM_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDRAM_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDRAM_VOLTAGE, 0664, show_BUCK_VDRAM_VOLTAGE, store_BUCK_VDRAM_VOLTAGE);

static ssize_t show_BUCK_VGPU_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6325_upmu_get_ni_vgpu_vosel();
    ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VGPU_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VGPU_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VGPU_VOLTAGE, 0664, show_BUCK_VGPU_VOLTAGE, store_BUCK_VGPU_VOLTAGE);

static ssize_t show_BUCK_VRF18_0_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6325_upmu_get_ni_vrf18_0_vosel();
    ret_value = get_volt_val_hw_buck_ip_v5(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VRF18_0_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VRF18_0_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VRF18_0_VOLTAGE, 0664, show_BUCK_VRF18_0_VOLTAGE, store_BUCK_VRF18_0_VOLTAGE);

static ssize_t show_BUCK_11_VDVFS11_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg_val=0;
    
    reg_val = mt6311_get_ni_vdvfs11_vosel();

	ret_value = get_volt_val_hw_buck_ip_v1(reg_val);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_11_VDVFS11_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_11_VDVFS11_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_11_VDVFS11_VOLTAGE, 0664, show_BUCK_11_VDVFS11_VOLTAGE, store_BUCK_11_VDVFS11_VOLTAGE);

static ssize_t show_BUCK_11_VDVFS12_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg_val=0;
    
	reg_val = mt6311_get_ni_vdvfs12_vosel();

	ret_value = get_volt_val_hw_buck_ip_v1(reg_val);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_11_VDVFS11_STATUS : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_11_VDVFS12_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_11_VDVFS12_VOLTAGE, 0664, show_BUCK_11_VDVFS12_VOLTAGE, store_BUCK_11_VDVFS12_VOLTAGE);

static ssize_t show_LDO_VTCXO0_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=2800;    
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VTCXO0_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VTCXO0_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VTCXO0_VOLTAGE, 0664, show_LDO_VTCXO0_VOLTAGE, store_LDO_VTCXO0_VOLTAGE);

static ssize_t show_LDO_VTCXO1_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=2800;    
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VTCXO1_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VTCXO1_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VTCXO1_VOLTAGE, 0664, show_LDO_VTCXO1_VOLTAGE, store_LDO_VTCXO1_VOLTAGE);

static ssize_t show_LDO_VAUD28_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=2800;    
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VAUD28_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VAUD28_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VAUD28_VOLTAGE, 0664, show_LDO_VAUD28_VOLTAGE, store_LDO_VAUD28_VOLTAGE);

static ssize_t show_LDO_VAUXA28_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=2800;    
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VAUXA28_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VAUXA28_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VAUXA28_VOLTAGE, 0664, show_LDO_VAUXA28_VOLTAGE, store_LDO_VAUXA28_VOLTAGE);

static ssize_t show_LDO_VBIF28_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=2800;    
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VBIF28_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VBIF28_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VBIF28_VOLTAGE, 0664, show_LDO_VBIF28_VOLTAGE, store_LDO_VBIF28_VOLTAGE);

static ssize_t show_LDO_VCN28_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=2800;    
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCN28_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCN28_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCN28_VOLTAGE, 0664, show_LDO_VCN28_VOLTAGE, store_LDO_VCN28_VOLTAGE);

static ssize_t show_LDO_VUSB33_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=3300;    
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VUSB33_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VUSB33_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VUSB33_VOLTAGE, 0664, show_LDO_VUSB33_VOLTAGE, store_LDO_VUSB33_VOLTAGE);

static ssize_t show_LDO_VIO28_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=2800;    
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VIO28_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VIO28_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VIO28_VOLTAGE, 0664, show_LDO_VIO28_VOLTAGE, store_LDO_VIO28_VOLTAGE);

static ssize_t show_LDO_VRTC_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=2800;    
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VRTC_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VRTC_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VRTC_VOLTAGE, 0664, show_LDO_VRTC_VOLTAGE, store_LDO_VRTC_VOLTAGE);

static ssize_t show_LDO_VCAMA_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA42;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x3, 1);
    if(reg_val == 0)         ret_value = 1500;
    else if(reg_val == 1)    ret_value = 1800;   
    else if(reg_val == 2)    ret_value = 2500;
    else if(reg_val == 3)    ret_value = 2800;
    else                     ret_value = 0;  //default 2.8v          

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAMA_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAMA_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAMA_VOLTAGE, 0664, show_LDO_VCAMA_VOLTAGE, store_LDO_VCAMA_VOLTAGE);

static ssize_t show_LDO_VCN33_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA44;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x3, 1);
    if(reg_val == 0)         ret_value = 3300;
    else if(reg_val == 1)    ret_value = 3400;   
    else if(reg_val == 2)    ret_value = 3500;
    else if(reg_val == 3)    ret_value = 3600;
    else                     ret_value = 0;  //default 3.3v          

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCN33_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCN33_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCN33_VOLTAGE, 0664, show_LDO_VCN33_VOLTAGE, store_LDO_VCN33_VOLTAGE);

static ssize_t show_LDO_VRF18_1_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA46;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x3, 9);
    if(reg_val == 0)         ret_value = 1500;
    else if(reg_val == 1)    ret_value = 1800;   
    else if(reg_val == 2)    ret_value = 2500;
    else if(reg_val == 3)    ret_value = 2800;
    else                     ret_value = 0;  //default 1.8v          

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VRF18_1_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VRF18_1_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VRF18_1_VOLTAGE, 0664, show_LDO_VRF18_1_VOLTAGE, store_LDO_VRF18_1_VOLTAGE);

static ssize_t show_LDO_VMCH_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA48;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x1, 9);
    if(reg_val == 0)         ret_value = 3000;
    else if(reg_val == 1)    ret_value = 3300;   
    else                     ret_value = 0;  //default 3.3v          

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VMCH_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VMCH_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VMCH_VOLTAGE, 0664, show_LDO_VMCH_VOLTAGE, store_LDO_VMCH_VOLTAGE);

static ssize_t show_LDO_VMC_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA48;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x1, 1);
    if(reg_val == 0)         ret_value = 1800;
    else if(reg_val == 1)    ret_value = 3300;   
    else                     ret_value = 0;  //default 3.3v          

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VMC_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VMC_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VMC_VOLTAGE, 0664, show_LDO_VMC_VOLTAGE, store_LDO_VMC_VOLTAGE);

static ssize_t show_LDO_VEMC33_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA4A;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x1, 9);
    if(reg_val == 0)         ret_value = 3000;
    else if(reg_val == 1)    ret_value = 3300;   
    else                     ret_value = 0;  //default 3.3v          

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VEMC33_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VEMC33_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VEMC33_VOLTAGE, 0664, show_LDO_VEMC33_VOLTAGE, store_LDO_VEMC33_VOLTAGE);

static ssize_t show_LDO_VCAM_AF_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA4C;//which register
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 1);//which bit
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else if(reg_val == 4)    ret_value = 2500;
    else if(reg_val == 5)    ret_value = 2800;
    else if(reg_val == 6)    ret_value = 3000;
    else if(reg_val == 7)    ret_value = 3300;
    else                     ret_value = 0;  //default 2.8v           

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAM_AF_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAM_AF_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAM_AF_VOLTAGE, 0664, show_LDO_VCAM_AF_VOLTAGE, store_LDO_VCAM_AF_VOLTAGE);

static ssize_t show_LDO_VGP1_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA4E;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 9);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else if(reg_val == 4)    ret_value = 2500;
    else if(reg_val == 5)    ret_value = 2800;
    else if(reg_val == 6)    ret_value = 3000;
    else if(reg_val == 7)    ret_value = 3300;
    else                     ret_value = 0;      //default   2.8v   

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VGP1_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VGP1_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VGP1_VOLTAGE, 0664, show_LDO_VGP1_VOLTAGE, store_LDO_VGP1_VOLTAGE);

static ssize_t show_LDO_VEFUSE_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA52;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 1);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1600;
    else if(reg_val == 2)    ret_value = 1700;
    else if(reg_val == 3)    ret_value = 1800;
    else if(reg_val == 4)    ret_value = 1900;
    else if(reg_val == 5)    ret_value = 2000;
    else if(reg_val == 6)    ret_value = 2100;
    else if(reg_val == 7)    ret_value = 2200;
    else                     ret_value = 0;      //default   1.8v   

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VEFUSE_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VEFUSE_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VEFUSE_VOLTAGE, 0664, show_LDO_VEFUSE_VOLTAGE, store_LDO_VEFUSE_VOLTAGE);

static ssize_t show_LDO_VSIM1_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA50;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 9);
    if(reg_val == 1)         ret_value = 1650;
    else if(reg_val == 2)    ret_value = 1800;
    else if(reg_val == 3)    ret_value = 1850;
    else if(reg_val == 5)    ret_value = 2750;
    else if(reg_val == 6)    ret_value = 3000;
    else if(reg_val == 7)    ret_value = 3100;
    else                     ret_value = 0;  //non 0 & 4 gear, default 1.8v      

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VSIM1_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VSIM1_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VSIM1_VOLTAGE, 0664, show_LDO_VSIM1_VOLTAGE, store_LDO_VSIM1_VOLTAGE);

static ssize_t show_LDO_VSIM2_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA50;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 1);
    if(reg_val == 1)         ret_value = 1650;
    else if(reg_val == 2)    ret_value = 1800;
    else if(reg_val == 3)    ret_value = 1850;
    else if(reg_val == 5)    ret_value = 2750;
    else if(reg_val == 6)    ret_value = 3000;
    else if(reg_val == 7)    ret_value = 3100;
    else                     ret_value = 0;  //non 0 & 4 gear ,default 1.8v       

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VSIM2_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VSIM2_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VSIM2_VOLTAGE, 0664, show_LDO_VSIM2_VOLTAGE, store_LDO_VSIM2_VOLTAGE);

static ssize_t show_LDO_VMIPI_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA52;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x3, 9);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else                     ret_value = 0;    //default 1.8v        

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VMIPI_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VMIPI_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VMIPI_VOLTAGE, 0664, show_LDO_VMIPI_VOLTAGE, store_LDO_VMIPI_VOLTAGE);

static ssize_t show_LDO_VCN18_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA54;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 9);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else if(reg_val == 4)    ret_value = 2500;
    else if(reg_val == 5)    ret_value = 2800;
    else if(reg_val == 6)    ret_value = 3000;
    else if(reg_val == 7)    ret_value = 3300;
    else                     ret_value = 0;      //default   1.8v       

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCN18_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCN18_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCN18_VOLTAGE, 0664, show_LDO_VCN18_VOLTAGE, store_LDO_VCN18_VOLTAGE);

static ssize_t show_LDO_VGP2_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA4E;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x3, 1);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else                     ret_value = 0;      //default   1.2v       

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VGP2_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VGP2_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VGP2_VOLTAGE, 0664, show_LDO_VGP2_VOLTAGE, store_LDO_VGP2_VOLTAGE);

static ssize_t show_LDO_VCAMD_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA56;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 1);
    if(reg_val == 0)         ret_value = 900;
    else if(reg_val == 1)    ret_value = 1000;
    else if(reg_val == 2)    ret_value = 1100;
    else if(reg_val == 3)    ret_value = 1220;
    else if(reg_val == 4)    ret_value = 1300;
    else if(reg_val == 5)    ret_value = 1500;
    else if(reg_val == 6)    ret_value = 1500;
    else if(reg_val == 7)    ret_value = 1500;
    else                     ret_value = 0;   //default 1.3v         

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAMD_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAMD_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAMD_VOLTAGE, 0664, show_LDO_VCAMD_VOLTAGE, store_LDO_VCAMD_VOLTAGE);

static ssize_t show_LDO_VCAM_IO_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA56;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 9);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else if(reg_val == 4)    ret_value = 2500;
    else if(reg_val == 5)    ret_value = 2800;
    else if(reg_val == 6)    ret_value = 3000;
    else if(reg_val == 7)    ret_value = 3300;
    else                     ret_value = 0;   //default 1.8v         

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VCAM_IO_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VCAM_IO_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VCAM_IO_VOLTAGE, 0664, show_LDO_VCAM_IO_VOLTAGE, store_LDO_VCAM_IO_VOLTAGE);

static ssize_t show_LDO_VSRAM_DVFS1_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA58;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7F, 9);
    ret_value = 600000+(6250*reg_val);       

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VSRAM_DVFS1_VOLTAGE (uV) : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VSRAM_DVFS1_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VSRAM_DVFS1_VOLTAGE, 0664, show_LDO_VSRAM_DVFS1_VOLTAGE, store_LDO_VSRAM_DVFS1_VOLTAGE);

static ssize_t show_LDO_VGP3_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA5A;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x7, 9);
    if(reg_val == 0)         ret_value = 1200;
    else if(reg_val == 1)    ret_value = 1300;
    else if(reg_val == 2)    ret_value = 1500;
    else if(reg_val == 3)    ret_value = 1800;
    else if(reg_val == 4)    ret_value = 2500;
    else if(reg_val == 5)    ret_value = 2800;
    else if(reg_val == 6)    ret_value = 3000;
    else if(reg_val == 7)    ret_value = 3300;
    else                     ret_value = 0;  //default 1.5v          

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VGP3_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VGP3_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VGP3_VOLTAGE, 0664, show_LDO_VGP3_VOLTAGE, store_LDO_VGP3_VOLTAGE);

static ssize_t show_LDO_VBIASN_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 ret=0;
    kal_uint32 reg_address=0xA5C;
    kal_uint32 reg_val=0;
    
    ret = pmic_read_interface(reg_address, &reg_val, 0x1F, 11);
    
    if(reg_val > 0)
        ret_value = 200+(20*(reg_val-1));
    else
        ret_value = 0;

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] LDO_VBIASN_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_LDO_VBIASN_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(LDO_VBIASN_VOLTAGE, 0664, show_LDO_VBIASN_VOLTAGE, store_LDO_VBIASN_VOLTAGE);

#endif
//TBD
#if 0
kal_uint32 get_volt_val_hw_buck_ip_v1(kal_uint32 val)
{
    kal_uint32 volt_val=0;        
    volt_val = (val*6250)+700000; 
    return volt_val;
}

//voltage
static ssize_t show_BUCK_VDVFS11_VOLTAGE(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_uint32 ret_value=0;
    kal_uint32 reg=0;

    reg = mt6325_upmu_get_ni_vdvfs11_vosel();
    ret_value = get_volt_val_hw_buck_ip_v1(reg);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] BUCK_VDVFS11_VOLTAGE : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_BUCK_VDVFS11_VOLTAGE(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(BUCK_VDVFS11_VOLTAGE, 0664, show_BUCK_VDVFS11_VOLTAGE, store_BUCK_VDVFS11_VOLTAGE);
#endif

//==============================================================================
// LDO EN APIs
//==============================================================================
#if 1
void dct_pmic_VTCXO0_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VTCXO0_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vtcxo0_en(1);
    } else {
        mt6325_upmu_set_rg_vtcxo0_en(0);
    }
}

void dct_pmic_VTCXO1_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VTCXO1_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vtcxo1_en(1);
    } else {
        mt6325_upmu_set_rg_vtcxo1_en(0);
    }
}

void dct_pmic_VAUD28_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VAUD28_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vaud28_en(1);
    } else {
        mt6325_upmu_set_rg_vaud28_en(0);
    }
}

void dct_pmic_VAUXA28_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VAUXA28_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vauxa28_en(1);
    } else {
        mt6325_upmu_set_rg_vauxa28_en(0);
    }
}

void dct_pmic_VBIF28_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VBIF28_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vbif28_en(1);
    } else {
        mt6325_upmu_set_rg_vbif28_en(0);
    }
}

void dct_pmic_VCAMA_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VCAMA_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vcama_en(1);
    } else {
        mt6325_upmu_set_rg_vcama_en(0);
    }
}

void dct_pmic_VCN28_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VCN28_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vcn28_en(1);
    } else {
        mt6325_upmu_set_rg_vcn28_en(0);
    }
}

void dct_pmic_VCN33_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VCN33_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vcn33_en(1);
    } else {
        mt6325_upmu_set_rg_vcn33_en(0);
    }
}

void dct_pmic_VRF18_1_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VRF18_1_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vrf18_1_en(1);
    } else {
        mt6325_upmu_set_rg_vrf18_1_en(0);
    }
}

void dct_pmic_VUSB33_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VUSB33_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vusb33_en(1);
    } else {
        mt6325_upmu_set_rg_vusb33_en(0);
    }
}

void dct_pmic_VMCH_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VMCH_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vmch_en(1);
    } else {
        mt6325_upmu_set_rg_vmch_en(0);
    }
}

void dct_pmic_VMC_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VMC_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vmc_en(1);
    } else {
        mt6325_upmu_set_rg_vmc_en(0);
    }
}

void dct_pmic_VEMC33_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VEMC33_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vemc33_en(1);
    } else {
        mt6325_upmu_set_rg_vemc33_en(0);
    }
}

void dct_pmic_VIO28_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VIO28_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vio28_en(1);
    } else {
        mt6325_upmu_set_rg_vio28_en(0);
    }
}

void dct_pmic_VCAM_AF_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VCAM_AF_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vcam_af_en(1);
    } else {
        mt6325_upmu_set_rg_vcam_af_en(0);
    }
}

void dct_pmic_VGP1_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VGP1_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vgp1_en(1);
    } else {
        mt6325_upmu_set_rg_vgp1_en(0);
    }
}

void dct_pmic_VEFUSE_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VEFUSE_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vefuse_en(1);
    } else {
        mt6325_upmu_set_rg_vefuse_en(0);
    }
}

void dct_pmic_VSIM1_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VSIM1_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vsim1_en(1);
    } else {
        mt6325_upmu_set_rg_vsim1_en(0);
    }
}

void dct_pmic_VSIM2_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VSIM2_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vsim2_en(1);
    } else {
        mt6325_upmu_set_rg_vsim2_en(0);
    }
}

void dct_pmic_VMIPI_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VMIPI_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vmipi_en(1);
    } else {
        mt6325_upmu_set_rg_vmipi_en(0);
    }
}

void dct_pmic_VCN18_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VCN18_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vcn18_en(1);
    } else {
        mt6325_upmu_set_rg_vcn18_en(0);
    }
}

void dct_pmic_VGP2_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VGP2_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vgp2_en(1);
    } else {
        mt6325_upmu_set_rg_vgp2_en(0);
    }
}

void dct_pmic_VCAMD_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VCAMD_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vcamd_en(1);
    } else {
        mt6325_upmu_set_rg_vcamd_en(0);
    }
}

void dct_pmic_VCAM_IO_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VCAM_IO_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vcam_io_en(1);
    } else {
        mt6325_upmu_set_rg_vcam_io_en(0);
    }
}

void dct_pmic_VSRAM_DVFS1_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VSRAM_DVFS1_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vsram_dvfs1_en(1);
    } else {
        mt6325_upmu_set_rg_vsram_dvfs1_en(0);
    }
}

void dct_pmic_VGP3_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VGP3_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vgp3_en(1);
    } else {
        mt6325_upmu_set_rg_vgp3_en(0);
    }
}

void dct_pmic_VBIASN_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VBIASN_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vbiasn_en(1);
    } else {
        mt6325_upmu_set_rg_vbiasn_en(0);
    }
}

void dct_pmic_VRTC_enable(kal_bool dctEnable)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VRTC_enable] %d\n", dctEnable);
    
    if(dctEnable == KAL_TRUE) {
        mt6325_upmu_set_rg_vrtc_en(1);
    } else {
        mt6325_upmu_set_rg_vrtc_en(0);
    }
}
#endif

//==============================================================================
// LDO VOSEL APIs
//==============================================================================
#if 1
void dct_pmic_VTCXO0_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VTCXO0_sel] no HW support\n");    
}

void dct_pmic_VTCXO1_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VTCXO1_sel] no HW support\n");    
}

void dct_pmic_VAUD28_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VAUD28_sel] no HW support\n");    
}

void dct_pmic_VAUXA28_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VAUXA28_sel] no HW support\n");    
}

void dct_pmic_VBIF28_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VBIF28_sel] no HW support\n");    
}

void dct_pmic_VCAMA_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VCAMA_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vcama_vosel(3);}
    else if(volt == 1500)    {mt6325_upmu_set_rg_vcama_vosel(0);}
    else if(volt == 1800)    {mt6325_upmu_set_rg_vcama_vosel(1);}
    else if(volt == 2500)    {mt6325_upmu_set_rg_vcama_vosel(2);}
    else if(volt == 2800)    {mt6325_upmu_set_rg_vcama_vosel(3);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VCN28_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VCN28_sel] no HW support\n");    
}

void dct_pmic_VCN33_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VCN33_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vcn33_vosel(0);}
    else if(volt == 3300)    {mt6325_upmu_set_rg_vcn33_vosel(0);}
    else if(volt == 3400)    {mt6325_upmu_set_rg_vcn33_vosel(1);}
    else if(volt == 3500)    {mt6325_upmu_set_rg_vcn33_vosel(2);}
    else if(volt == 3600)    {mt6325_upmu_set_rg_vcn33_vosel(3);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VRF18_1_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VRF18_1_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vrf18_1_vosel(1);}
    else if(volt == 1500)    {mt6325_upmu_set_rg_vrf18_1_vosel(0);}
    else if(volt == 1800)    {mt6325_upmu_set_rg_vrf18_1_vosel(1);}
    else if(volt == 2500)    {mt6325_upmu_set_rg_vrf18_1_vosel(2);}
    else if(volt == 2800)    {mt6325_upmu_set_rg_vrf18_1_vosel(3);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VUSB33_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VUSB33_sel] no HW support\n");    
}

void dct_pmic_VMCH_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VMCH_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vmch_vosel(1);}
    else if(volt == 3000)    {mt6325_upmu_set_rg_vmch_vosel(0);}
    else if(volt == 3300)    {mt6325_upmu_set_rg_vmch_vosel(1);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VMC_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VMC_sel] value=%d \n", volt);

#if 1
    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vmc_vosel(1); pmic_config_interface(0xA64,0x0,0x1,15);}
    else if(volt == 1500)    {mt6325_upmu_set_rg_vmc_vosel(0); pmic_config_interface(0xA64,0x0,0x1,15);}
    else if(volt == 1800)    {mt6325_upmu_set_rg_vmc_vosel(0); pmic_config_interface(0xA64,0x1,0x1,15);}
    else if(volt == 3000)    {mt6325_upmu_set_rg_vmc_vosel(1); pmic_config_interface(0xA64,0x0,0x1,15);}
    else if(volt == 3300)    {mt6325_upmu_set_rg_vmc_vosel(1); pmic_config_interface(0xA64,0x1,0x1,15);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
#else
    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vmc_vosel(1); }
    else if(volt == 1500)    {mt6325_upmu_set_rg_vmc_vosel(0); }
    else if(volt == 1800)    {mt6325_upmu_set_rg_vmc_vosel(0); }
    else if(volt == 3000)    {mt6325_upmu_set_rg_vmc_vosel(1); }
    else if(volt == 3300)    {mt6325_upmu_set_rg_vmc_vosel(1); }
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
#endif    

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[dct_pmic_VMC_sel] [0x%x]=0x%x,[0x%x]=0x%x.\n", 
        0xA48, upmu_get_reg_value(0xA48),
        0xA64, upmu_get_reg_value(0xA64)
        );
}

void dct_pmic_VEMC33_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VEMC33_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vemc_3v3_vosel(1);}
    else if(volt == 3000)    {mt6325_upmu_set_rg_vemc_3v3_vosel(0);}
    else if(volt == 3300)    {mt6325_upmu_set_rg_vemc_3v3_vosel(1);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VIO28_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VIO28_sel] no HW support\n");    
}

void dct_pmic_VCAM_AF_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VCAM_AF_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vcamaf_vosel(5);}
    else if(volt == 1200)    {mt6325_upmu_set_rg_vcamaf_vosel(0);}
    else if(volt == 1300)    {mt6325_upmu_set_rg_vcamaf_vosel(1);}
    else if(volt == 1500)    {mt6325_upmu_set_rg_vcamaf_vosel(2);}
    else if(volt == 1800)    {mt6325_upmu_set_rg_vcamaf_vosel(3);}
    else if(volt == 2500)    {mt6325_upmu_set_rg_vcamaf_vosel(4);}
    else if(volt == 2800)    {mt6325_upmu_set_rg_vcamaf_vosel(5);}
    else if(volt == 3000)    {mt6325_upmu_set_rg_vcamaf_vosel(6);}
    else if(volt == 3300)    {mt6325_upmu_set_rg_vcamaf_vosel(7);}    
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VGP1_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VGP1_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vgp1_vosel(5);}
    else if(volt == 1200)    {mt6325_upmu_set_rg_vgp1_vosel(0);}
    else if(volt == 1300)    {mt6325_upmu_set_rg_vgp1_vosel(1);}
    else if(volt == 1500)    {mt6325_upmu_set_rg_vgp1_vosel(2);}
    else if(volt == 1800)    {mt6325_upmu_set_rg_vgp1_vosel(3);}
    else if(volt == 2500)    {mt6325_upmu_set_rg_vgp1_vosel(4);}
    else if(volt == 2800)    {mt6325_upmu_set_rg_vgp1_vosel(5);}
    else if(volt == 3000)    {mt6325_upmu_set_rg_vgp1_vosel(6);}
    else if(volt == 3300)    {mt6325_upmu_set_rg_vgp1_vosel(7);}    
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VEFUSE_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VEFUSE_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vefuse_vosel(3);}
    else if(volt == 1200)    {mt6325_upmu_set_rg_vefuse_vosel(0);}
    else if(volt == 1600)    {mt6325_upmu_set_rg_vefuse_vosel(1);}
    else if(volt == 1700)    {mt6325_upmu_set_rg_vefuse_vosel(2);}
    else if(volt == 1800)    {mt6325_upmu_set_rg_vefuse_vosel(3);}
    else if(volt == 1900)    {mt6325_upmu_set_rg_vefuse_vosel(4);}
    else if(volt == 2000)    {mt6325_upmu_set_rg_vefuse_vosel(5);}
    else if(volt == 2100)    {mt6325_upmu_set_rg_vefuse_vosel(6);}
    else if(volt == 2200)    {mt6325_upmu_set_rg_vefuse_vosel(7);}    
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VSIM1_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VSIM1_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vsim1_vosel(2);}
    //else if(volt == 00)    {mt6325_upmu_set_rg_vsim1_vosel(0);}
    else if(volt == 1650)    {mt6325_upmu_set_rg_vsim1_vosel(1);}
    else if(volt == 1800)    {mt6325_upmu_set_rg_vsim1_vosel(2);}
    else if(volt == 1850)    {mt6325_upmu_set_rg_vsim1_vosel(3);}
    //else if(volt == 00)    {mt6325_upmu_set_rg_vsim1_vosel(4);}
    else if(volt == 2750)    {mt6325_upmu_set_rg_vsim1_vosel(5);}
    else if(volt == 3000)    {mt6325_upmu_set_rg_vsim1_vosel(6);}
    else if(volt == 3100)    {mt6325_upmu_set_rg_vsim1_vosel(7);}    
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VSIM2_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VSIM2_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vsim2_vosel(2);}
    //else if(volt == 00)    {mt6325_upmu_set_rg_vsim2_vosel(0);}
    else if(volt == 1650)    {mt6325_upmu_set_rg_vsim2_vosel(1);}
    else if(volt == 1800)    {mt6325_upmu_set_rg_vsim2_vosel(2);}
    else if(volt == 1850)    {mt6325_upmu_set_rg_vsim2_vosel(3);}
    //else if(volt == 00)    {mt6325_upmu_set_rg_vsim2_vosel(4);}
    else if(volt == 2750)    {mt6325_upmu_set_rg_vsim2_vosel(5);}
    else if(volt == 3000)    {mt6325_upmu_set_rg_vsim2_vosel(6);}
    else if(volt == 3100)    {mt6325_upmu_set_rg_vsim2_vosel(7);}    
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VMIPI_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VMIPI_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vmipi_vosel(3);}
    else if(volt == 1200)    {mt6325_upmu_set_rg_vmipi_vosel(0);}
    else if(volt == 1300)    {mt6325_upmu_set_rg_vmipi_vosel(1);}
    else if(volt == 1500)    {mt6325_upmu_set_rg_vmipi_vosel(2);}
    else if(volt == 1800)    {mt6325_upmu_set_rg_vmipi_vosel(3);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VCN18_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VCN18_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vcn18_vosel(3);}
    else if(volt == 1200)    {mt6325_upmu_set_rg_vcn18_vosel(0);}
    else if(volt == 1300)    {mt6325_upmu_set_rg_vcn18_vosel(1);}
    else if(volt == 1500)    {mt6325_upmu_set_rg_vcn18_vosel(2);}
    else if(volt == 1800)    {mt6325_upmu_set_rg_vcn18_vosel(3);}
    else if(volt == 2500)    {mt6325_upmu_set_rg_vcn18_vosel(4);}
    else if(volt == 2800)    {mt6325_upmu_set_rg_vcn18_vosel(5);}
    else if(volt == 3000)    {mt6325_upmu_set_rg_vcn18_vosel(6);}
    else if(volt == 3300)    {mt6325_upmu_set_rg_vcn18_vosel(7);}    
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VGP2_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VGP2_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vgp2_vosel(0);}
    else if(volt == 1200)    {mt6325_upmu_set_rg_vgp2_vosel(0);}
    else if(volt == 1300)    {mt6325_upmu_set_rg_vgp2_vosel(1);}
    else if(volt == 1500)    {mt6325_upmu_set_rg_vgp2_vosel(2);}
    else if(volt == 1800)    {mt6325_upmu_set_rg_vgp2_vosel(3);}
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VCAMD_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VCAMD_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vcamd_vosel(4);}
    else if(volt ==  900)    {mt6325_upmu_set_rg_vcamd_vosel(0);}
    else if(volt == 1000)    {mt6325_upmu_set_rg_vcamd_vosel(1);}
    else if(volt == 1100)    {mt6325_upmu_set_rg_vcamd_vosel(2);}
    else if(volt == 1220)    {mt6325_upmu_set_rg_vcamd_vosel(3);}
    else if(volt == 1300)    {mt6325_upmu_set_rg_vcamd_vosel(4);}
    else if(volt == 1500)    {mt6325_upmu_set_rg_vcamd_vosel(5);}
    else if(volt == 1500)    {mt6325_upmu_set_rg_vcamd_vosel(6);}
    else if(volt == 1500)    {mt6325_upmu_set_rg_vcamd_vosel(7);}    
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VCAM_IO_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VCAM_IO_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vcamio_vosel(3);}
    else if(volt == 1200)    {mt6325_upmu_set_rg_vcamio_vosel(0);}
    else if(volt == 1300)    {mt6325_upmu_set_rg_vcamio_vosel(1);}
    else if(volt == 1500)    {mt6325_upmu_set_rg_vcamio_vosel(2);}
    else if(volt == 1800)    {mt6325_upmu_set_rg_vcamio_vosel(3);}
    else if(volt == 2500)    {mt6325_upmu_set_rg_vcamio_vosel(4);}
    else if(volt == 2800)    {mt6325_upmu_set_rg_vcamio_vosel(5);}
    else if(volt == 3000)    {mt6325_upmu_set_rg_vcamio_vosel(6);}
    else if(volt == 3300)    {mt6325_upmu_set_rg_vcamio_vosel(7);}    
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VSRAM_DVFS1_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VSRAM_DVFS1_sel] not support\n");
}

void dct_pmic_VGP3_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VGP3_sel] value=%d \n", volt);

    if(volt == VOL_DEFAULT)  {mt6325_upmu_set_rg_vgp3_vosel(2);}
    else if(volt == 1200)    {mt6325_upmu_set_rg_vgp3_vosel(0);}
    else if(volt == 1300)    {mt6325_upmu_set_rg_vgp3_vosel(1);}
    else if(volt == 1500)    {mt6325_upmu_set_rg_vgp3_vosel(2);}
    else if(volt == 1800)    {mt6325_upmu_set_rg_vgp3_vosel(3);}
    else if(volt == 2500)    {mt6325_upmu_set_rg_vgp3_vosel(4);}
    else if(volt == 2800)    {mt6325_upmu_set_rg_vgp3_vosel(5);}
    else if(volt == 3000)    {mt6325_upmu_set_rg_vgp3_vosel(6);}
    else if(volt == 3300)    {mt6325_upmu_set_rg_vgp3_vosel(7);}    
    else{
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
}

void dct_pmic_VBIASN_sel(kal_uint32 volt)
{
    int val=0;
        
    if(volt==0)
    {
        mt6325_upmu_set_rg_vbiasn_vosel(0);
    }
    else if(volt > 800)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Error Setting %d. DO nothing.\r\n", volt);
    }
    else
    {
        val = ( (volt-200) / 20 ) + 1;
        mt6325_upmu_set_rg_vbiasn_vosel(val);
    }

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VBIASN_sel] value=%d, val=%d \n", volt, val);
}

void dct_pmic_VRTC_sel(kal_uint32 volt)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[dct_pmic_VRTC_sel] no HW support\n");    
}
#endif

//==============================================================================
// LDO EN & SEL common API
//==============================================================================
void pmic_ldo_enable(MT_POWER powerId, kal_bool powerEnable)
{
    //Need integrate with DCT : using DCT APIs

#if 1
    if(     powerId == MT6325_POWER_LDO_VTCXO0      )  { dct_pmic_VTCXO0_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VTCXO1      )  { dct_pmic_VTCXO1_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VAUD28      )  { dct_pmic_VAUD28_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VAUXA28     )  { dct_pmic_VAUXA28_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VBIF28      )  { dct_pmic_VBIF28_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VCAMA       )  { dct_pmic_VCAMA_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VCN28       )  { dct_pmic_VCN28_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VCN33       )  { dct_pmic_VCN33_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VRF18_1     )  { dct_pmic_VRF18_1_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VUSB33      )  { dct_pmic_VUSB33_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VMCH        )  { dct_pmic_VMCH_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VMC         )  { dct_pmic_VMC_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VEMC33      )  { dct_pmic_VEMC33_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VIO28       )  { dct_pmic_VIO28_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VCAM_AF     )  { dct_pmic_VCAM_AF_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VGP1        )  { dct_pmic_VGP1_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VEFUSE      )  { dct_pmic_VEFUSE_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VSIM1       )  { dct_pmic_VSIM1_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VSIM2       )  { dct_pmic_VSIM2_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VMIPI       )  { dct_pmic_VMIPI_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VCN18       )  { dct_pmic_VCN18_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VGP2        )  { dct_pmic_VGP2_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VCAMD       )  { dct_pmic_VCAMD_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VCAM_IO     )  { dct_pmic_VCAM_IO_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VSRAM_DVFS1 )  { dct_pmic_VSRAM_DVFS1_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VGP3        )  { dct_pmic_VGP3_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VBIASN      )  { dct_pmic_VBIASN_enable(powerEnable); }
    else if(powerId == MT6325_POWER_LDO_VRTC        )  { dct_pmic_VRTC_enable(powerEnable); }
           
    else
    {
        xlog_printk(ANDROID_LOG_WARN, "Power/PMIC", "[pmic_ldo_enable] UnKnown powerId (%d)\n", powerId);
    }

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_ldo_enable] Receive powerId %d, action is %d\n", powerId, powerEnable);
#endif    
}

void pmic_ldo_vol_sel(MT_POWER powerId, MT_POWER_VOLTAGE powerVolt)
{
    //Need integrate with DCT : using DCT APIs

#if 1
    if(     powerId == MT6325_POWER_LDO_VTCXO0      )  { dct_pmic_VTCXO0_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VTCXO1      )  { dct_pmic_VTCXO1_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VAUD28      )  { dct_pmic_VAUD28_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VAUXA28     )  { dct_pmic_VAUXA28_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VBIF28      )  { dct_pmic_VBIF28_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VCAMA       )  { dct_pmic_VCAMA_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VCN28       )  { dct_pmic_VCN28_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VCN33       )  { dct_pmic_VCN33_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VRF18_1     )  { dct_pmic_VRF18_1_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VUSB33      )  { dct_pmic_VUSB33_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VMCH        )  { dct_pmic_VMCH_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VMC         )  { dct_pmic_VMC_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VEMC33      )  { dct_pmic_VEMC33_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VIO28       )  { dct_pmic_VIO28_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VCAM_AF     )  { dct_pmic_VCAM_AF_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VGP1        )  { dct_pmic_VGP1_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VEFUSE      )  { dct_pmic_VEFUSE_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VSIM1       )  { dct_pmic_VSIM1_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VSIM2       )  { dct_pmic_VSIM2_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VMIPI       )  { dct_pmic_VMIPI_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VCN18       )  { dct_pmic_VCN18_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VGP2        )  { dct_pmic_VGP2_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VCAMD       )  { dct_pmic_VCAMD_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VCAM_IO     )  { dct_pmic_VCAM_IO_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VSRAM_DVFS1 )  { dct_pmic_VSRAM_DVFS1_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VGP3        )  { dct_pmic_VGP3_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VBIASN      )  { dct_pmic_VBIASN_sel(powerVolt); }
    else if(powerId == MT6325_POWER_LDO_VRTC        )  { dct_pmic_VRTC_sel(powerVolt); }

    else
    {
        xlog_printk(ANDROID_LOG_WARN, "Power/PMIC", "[pmic_ldo_ldo_vol_sel] UnKnown powerId (%d)\n", powerId);
    }

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[pmic_ldo_vol_sel] Receive powerId %d, action is %d\n", powerId, powerVolt);    
#endif    
}

//==============================================================================
// PMIC device driver
//==============================================================================
void ldo_service_test(void)
{
}

//==============================================================================
// Dump all LDO status 
//==============================================================================
void dump_ldo_status_read_debug(void)
{
#if 1
    kal_uint32 val_0=0, val_1=0, val_2=0;

    //MT6325
    val_0 = upmu_get_reg_value(MT6325_EN_STATUS0);
    val_1 = upmu_get_reg_value(MT6325_EN_STATUS1);
    val_2 = upmu_get_reg_value(MT6325_EN_STATUS2);
    
    printk("********** BUCK/LDO status dump [1:ON,0:OFF]**********\n");

    printk("VDVFS11     =%d, ",  (((val_0)&(0x0001))>>0) ); 
    printk("VDVFS12     =%d, ",  (((val_0)&(0x0002))>>1) );
    printk("VDRAM       =%d, ",  (((val_0)&(0x0004))>>2) ); 
    printk("VRF18_0     =%d\n",  (((val_0)&(0x0008))>>3) );
    
    printk("VGPU        =%d, ",  (((val_0)&(0x0010))>>4) );    
    printk("VCORE1      =%d, ",  (((val_0)&(0x0020))>>5) ); 
    printk("VCORE2      =%d, ",  (((val_0)&(0x0040))>>6) );         
    printk("VIO18       =%d\n",  (((val_0)&(0x0080))>>7) );        
    
    printk("VPA         =%d, ",  (((val_0)&(0x0100))>>8) );     
    printk("VRTC        =%d, ",  (((val_0)&(0x0200))>>9) ); 
    printk("VTCXO0      =%d, ",  (((val_0)&(0x0400))>>10) );         
    printk("VTCXO1      =%d\n",  (((val_0)&(0x0800))>>11) );    
    
    printk("VAUD28      =%d, ",  (((val_0)&(0x1000))>>12) );    
    printk("VAUXA28     =%d, ",  (((val_0)&(0x2000))>>13) );
    printk("VCAMA       =%d, ",  (((val_0)&(0x4000))>>14) );         
    printk("VIO28       =%d\n",  (((val_0)&(0x8000))>>15) ); 

    printk("VCAM_AF     =%d, ",  (((val_1)&(0x0001))>>0) ); 
    printk("VMC         =%d, ",  (((val_1)&(0x0002))>>1) );
    printk("VMCH        =%d, ",  (((val_1)&(0x0004))>>2) ); 
    printk("VEMC33      =%d\n",  (((val_1)&(0x0008))>>3) );
    
    printk("VGP1        =%d, ",  (((val_1)&(0x0010))>>4) );    
    printk("VEFUSE      =%d, ",  (((val_1)&(0x0020))>>5) ); 
    printk("VSIM1       =%d, ",  (((val_1)&(0x0040))>>6) );         
    printk("VSIM2       =%d\n",  (((val_1)&(0x0080))>>7) );        
    
    printk("VCN28       =%d, ",  (((val_1)&(0x0100))>>8) );     
    printk("VMIPI       =%d, ",  (((val_1)&(0x0200))>>9) ); 
    printk("VIBR        =%d, ",  (((val_1)&(0x0400))>>10) );         
    printk("VCAMD       =%d\n",  (((val_1)&(0x0800))>>11) );    
    
    printk("VUSB33      =%d, ",  (((val_1)&(0x1000))>>12) );    
    printk("VCAM_IO     =%d, ",  (((val_1)&(0x2000))>>13) );
    printk("VSRAM_DVFS1 =%d, ",  (((val_1)&(0x4000))>>14) );         
    printk("VGP2        =%d\n",  (((val_1)&(0x8000))>>15) ); 

    printk("VGP3        =%d, ",  (((val_2)&(0x0001))>>0) ); 
    printk("VBIASN      =%d, ",  (((val_2)&(0x0002))>>1) );
    printk("VCN33       =%d, ",  (((val_2)&(0x0004))>>2) ); 
    printk("VCN18       =%d\n",  (((val_2)&(0x0008))>>3) );
    
    printk("VRF18_1     =%d, ",  (((val_2)&(0x0010))>>4) );    
#endif
}

static int proc_utilization_show(struct seq_file *m, void *v)
{
#if 1
    kal_uint32 val_0=0, val_1=0, val_2=0;

    //MT6325
    val_0 = upmu_get_reg_value(MT6325_EN_STATUS0);
    val_1 = upmu_get_reg_value(MT6325_EN_STATUS1);
    val_2 = upmu_get_reg_value(MT6325_EN_STATUS2);
    
    seq_printf(m, "********** BUCK/LDO status dump seq_printf [1:ON,0:OFF]**********\n");

    seq_printf(m, "VDVFS11     =%d, ",  (((val_0)&(0x0001))>>0) ); 
    seq_printf(m, "VDVFS12     =%d, ",  (((val_0)&(0x0002))>>1) );
    seq_printf(m, "VDRAM       =%d, ",  (((val_0)&(0x0004))>>2) ); 
    seq_printf(m, "VRF18_0     =%d\n",  (((val_0)&(0x0008))>>3) );
    
    seq_printf(m, "VGPU        =%d, ",  (((val_0)&(0x0010))>>4) );    
    seq_printf(m, "VCORE1      =%d, ",  (((val_0)&(0x0020))>>5) ); 
    seq_printf(m, "VCORE2      =%d, ",  (((val_0)&(0x0040))>>6) );         
    seq_printf(m, "VIO18       =%d\n",  (((val_0)&(0x0080))>>7) );        
    
    seq_printf(m, "VPA         =%d, ",  (((val_0)&(0x0100))>>8) );     
    seq_printf(m, "VRTC        =%d, ",  (((val_0)&(0x0200))>>9) ); 
    seq_printf(m, "VTCXO0      =%d, ",  (((val_0)&(0x0400))>>10) );         
    seq_printf(m, "VTCXO1      =%d\n",  (((val_0)&(0x0800))>>11) );    
    
    seq_printf(m, "VAUD28      =%d, ",  (((val_0)&(0x1000))>>12) );    
    seq_printf(m, "VAUXA28     =%d, ",  (((val_0)&(0x2000))>>13) );
    seq_printf(m, "VCAMA       =%d, ",  (((val_0)&(0x4000))>>14) );         
    seq_printf(m, "VIO28       =%d\n",  (((val_0)&(0x8000))>>15) ); 

    seq_printf(m, "VCAM_AF     =%d, ",  (((val_1)&(0x0001))>>0) ); 
    seq_printf(m, "VMC         =%d, ",  (((val_1)&(0x0002))>>1) );
    seq_printf(m, "VMCH        =%d, ",  (((val_1)&(0x0004))>>2) ); 
    seq_printf(m, "VEMC33      =%d\n",  (((val_1)&(0x0008))>>3) );
    
    seq_printf(m, "VGP1        =%d, ",  (((val_1)&(0x0010))>>4) );    
    seq_printf(m, "VEFUSE      =%d, ",  (((val_1)&(0x0020))>>5) ); 
    seq_printf(m, "VSIM1       =%d, ",  (((val_1)&(0x0040))>>6) );         
    seq_printf(m, "VSIM2       =%d\n",  (((val_1)&(0x0080))>>7) );        
    
    seq_printf(m, "VCN28       =%d, ",  (((val_1)&(0x0100))>>8) );     
    seq_printf(m, "VMIPI       =%d, ",  (((val_1)&(0x0200))>>9) ); 
    seq_printf(m, "VIBR        =%d, ",  (((val_1)&(0x0400))>>10) );         
    seq_printf(m, "VCAMD       =%d\n",  (((val_1)&(0x0800))>>11) );    
    
    seq_printf(m, "VUSB33      =%d, ",  (((val_1)&(0x1000))>>12) );    
    seq_printf(m, "VCAM_IO     =%d, ",  (((val_1)&(0x2000))>>13) );
    seq_printf(m, "VSRAM_DVFS1 =%d, ",  (((val_1)&(0x4000))>>14) );         
    seq_printf(m, "VGP2        =%d\n",  (((val_1)&(0x8000))>>15) ); 

    seq_printf(m, "VGP3        =%d, ",  (((val_2)&(0x0001))>>0) ); 
    seq_printf(m, "VBIASN      =%d, ",  (((val_2)&(0x0002))>>1) );
    seq_printf(m, "VCN33       =%d, ",  (((val_2)&(0x0004))>>2) ); 
    seq_printf(m, "VCN18       =%d\n",  (((val_2)&(0x0008))>>3) );
    
    seq_printf(m, "VRF18_1     =%d, ",  (((val_2)&(0x0010))>>4) );
#endif    
    
    return 0;
}

static int proc_utilization_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_utilization_show, NULL);
}

static const struct file_operations pmic_debug_proc_fops = { 
    .open  = proc_utilization_open, 
    .read  = seq_read,
};

void pmic_debug_init(void)
{
    //struct proc_dir_entry *entry;
    struct proc_dir_entry *mt_pmic_dir;

    mt_pmic_dir = proc_mkdir("mt_pmic", NULL);
    if (!mt_pmic_dir) {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "fail to mkdir /proc/mt_pmic\n" );
        return;
    }

    #if 1
    proc_create("dump_ldo_status", S_IRUGO | S_IWUSR, mt_pmic_dir, &pmic_debug_proc_fops);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "proc_create pmic_debug_proc_fops\n" );
    #else
    entry = create_proc_entry("dump_ldo_status", 00640, mt_pmic_dir);
    if (entry) {
        entry->read_proc = dump_ldo_status_read;
    }
    #endif
}

//==============================================================================
// low battery protect UT
//==============================================================================
static ssize_t show_low_battery_protect_ut(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_low_battery_protect_ut] g_low_battery_level=%d\n", g_low_battery_level);
    return sprintf(buf, "%u\n", g_low_battery_level);
}
static ssize_t store_low_battery_protect_ut(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
    U32 val = 0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_ut] \n");
    
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_ut] buf is %s and size is %d \n",buf,size);
        val = simple_strtoul(buf,&pvalue,16);
        if(val<=2)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_ut] your input is %d\n", val);
            exec_low_battery_callback(val);
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_ut] wrong number (%d)\n", val);
        }
    }
    return size;
}
static DEVICE_ATTR(low_battery_protect_ut, 0664, show_low_battery_protect_ut, store_low_battery_protect_ut); //664

//==============================================================================
// low battery protect stop
//==============================================================================
static ssize_t show_low_battery_protect_stop(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_low_battery_protect_stop] g_low_battery_stop=%d\n", g_low_battery_stop);
    return sprintf(buf, "%u\n", g_low_battery_stop);
}
static ssize_t store_low_battery_protect_stop(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
    U32 val = 0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_stop] \n");
    
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_stop] buf is %s and size is %d \n",buf,size);
        val = simple_strtoul(buf,&pvalue,16);
        if( (val!=0) && (val!=1) )
            val=0;
        g_low_battery_stop = val;
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_stop] g_low_battery_stop=%d\n", g_low_battery_stop);
    }
    return size;
}
static DEVICE_ATTR(low_battery_protect_stop, 0664, show_low_battery_protect_stop, store_low_battery_protect_stop); //664

//==============================================================================
// low battery protect level
//==============================================================================
static ssize_t show_low_battery_protect_level(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_low_battery_protect_level] g_low_battery_level=%d\n", g_low_battery_level);
    return sprintf(buf, "%u\n", g_low_battery_level);
}
static ssize_t store_low_battery_protect_level(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_low_battery_protect_level] g_low_battery_level=%d\n", g_low_battery_level);
   
    return size;
}
static DEVICE_ATTR(low_battery_protect_level, 0664, show_low_battery_protect_level, store_low_battery_protect_level); //664

//==============================================================================
// battery OC protect UT
//==============================================================================
static ssize_t show_battery_oc_protect_ut(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_battery_oc_protect_ut] g_battery_oc_level=%d\n", g_battery_oc_level);
    return sprintf(buf, "%u\n", g_battery_oc_level);
}
static ssize_t store_battery_oc_protect_ut(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
    U32 val = 0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_ut] \n");
    
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_ut] buf is %s and size is %d \n",buf,size);
        val = simple_strtoul(buf,&pvalue,16);
        if(val<=1)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_ut] your input is %d\n", val);
            exec_battery_oc_callback(val);
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_ut] wrong number (%d)\n", val);
        }
    }
    return size;
}
static DEVICE_ATTR(battery_oc_protect_ut, 0664, show_battery_oc_protect_ut, store_battery_oc_protect_ut); //664

//==============================================================================
// battery OC protect stop
//==============================================================================
static ssize_t show_battery_oc_protect_stop(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_battery_oc_protect_stop] g_battery_oc_stop=%d\n", g_battery_oc_stop);
    return sprintf(buf, "%u\n", g_battery_oc_stop);
}
static ssize_t store_battery_oc_protect_stop(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
    U32 val = 0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_stop] \n");
    
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_stop] buf is %s and size is %d \n",buf,size);
        val = simple_strtoul(buf,&pvalue,16);
        if( (val!=0) && (val!=1) )
            val=0;
        g_battery_oc_stop = val;
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_stop] g_battery_oc_stop=%d\n", g_battery_oc_stop);
    }
    return size;
}
static DEVICE_ATTR(battery_oc_protect_stop, 0664, show_battery_oc_protect_stop, store_battery_oc_protect_stop); //664

//==============================================================================
// battery OC protect level
//==============================================================================
static ssize_t show_battery_oc_protect_level(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_battery_oc_protect_level] g_battery_oc_level=%d\n", g_battery_oc_level);
    return sprintf(buf, "%u\n", g_battery_oc_level);
}
static ssize_t store_battery_oc_protect_level(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_oc_protect_level] g_battery_oc_level=%d\n", g_battery_oc_level);
   
    return size;
}
static DEVICE_ATTR(battery_oc_protect_level, 0664, show_battery_oc_protect_level, store_battery_oc_protect_level); //664

//==============================================================================
// battery percent protect UT
//==============================================================================
static ssize_t show_battery_percent_protect_ut(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_battery_percent_protect_ut] g_battery_percent_level=%d\n", g_battery_percent_level);
    return sprintf(buf, "%u\n", g_battery_percent_level);
}
static ssize_t store_battery_percent_protect_ut(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
    U32 val = 0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_ut] \n");
    
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_ut] buf is %s and size is %d \n",buf,size);
        val = simple_strtoul(buf,&pvalue,16);
        if(val<=1)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_ut] your input is %d\n", val);
            exec_battery_percent_callback(val);
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_ut] wrong number (%d)\n", val);
        }
    }
    return size;
}
static DEVICE_ATTR(battery_percent_protect_ut, 0664, show_battery_percent_protect_ut, store_battery_percent_protect_ut); //664

//==============================================================================
// battery percent protect stop
//==============================================================================
static ssize_t show_battery_percent_protect_stop(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_battery_percent_protect_stop] g_battery_percent_stop=%d\n", g_battery_percent_stop);
    return sprintf(buf, "%u\n", g_battery_percent_stop);
}
static ssize_t store_battery_percent_protect_stop(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
    U32 val = 0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_stop] \n");
    
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_stop] buf is %s and size is %d \n",buf,size);
        val = simple_strtoul(buf,&pvalue,16);
        if( (val!=0) && (val!=1) )
            val=0;
        g_battery_percent_stop = val;
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_stop] g_battery_percent_stop=%d\n", g_battery_percent_stop);
    }
    return size;
}
static DEVICE_ATTR(battery_percent_protect_stop, 0664, show_battery_percent_protect_stop, store_battery_percent_protect_stop); //664

//==============================================================================
// battery percent protect level
//==============================================================================
static ssize_t show_battery_percent_protect_level(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[show_battery_percent_protect_level] g_battery_percent_level=%d\n", g_battery_percent_level);
    return sprintf(buf, "%u\n", g_battery_percent_level);
}
static ssize_t store_battery_percent_protect_level(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_battery_percent_protect_level] g_battery_percent_level=%d\n", g_battery_percent_level);
   
    return size;
}
static DEVICE_ATTR(battery_percent_protect_level, 0664, show_battery_percent_protect_level, store_battery_percent_protect_level); //664

//==============================================================================
// DVT entry
//==============================================================================
kal_uint8 g_reg_value_pmic=0;

static ssize_t show_pmic_dvt(struct device *dev,struct device_attribute *attr, char *buf)
{
    printk("[show_pmic_dvt] 0x%x\n", g_reg_value_pmic);
    return sprintf(buf, "%u\n", g_reg_value_pmic);
}
static ssize_t store_pmic_dvt(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
    unsigned int test_item = 0;
    
    printk("[store_pmic_dvt] \n");
    
    if(buf != NULL && size != 0)
    {
        printk("[store_pmic_dvt] buf is %s and size is %d \n",buf,size);
        test_item = simple_strtoul(buf,&pvalue,10);
        printk("[store_pmic_dvt] test_item=%d\n", test_item);

        #ifdef MTK_PMIC_DVT_SUPPORT 
        pmic_dvt_entry(test_item);
        #else
        printk("[store_pmic_dvt] no define MTK_PMIC_DVT_SUPPORT\n");
        #endif
    }    
    return size;
}
static DEVICE_ATTR(pmic_dvt, 0664, show_pmic_dvt, store_pmic_dvt);

//==============================================================================
// Enternal SWCHR
//==============================================================================
#ifdef MTK_BQ24261_SUPPORT
extern int is_bq24261_exist(void);
#endif

int is_ext_swchr_exist(void)
{
    #ifdef MTK_BQ24261_SUPPORT
        if( (is_bq24261_exist()==1) )
            return 1;    
        else
            return 0;    
    #else
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[is_ext_swchr_exist] no define any HW\n");
        return 0;
    #endif    
}

//==============================================================================
// Enternal VBAT Boost status
//==============================================================================
extern int is_tps6128x_sw_ready(void);
extern int is_tps6128x_exist(void);

int is_ext_vbat_boost_sw_ready(void)
{    
    if( (is_tps6128x_sw_ready()==1) )
        return 1;    
    else
        return 0;    
}

int is_ext_vbat_boost_exist(void)
{
    if( (is_tps6128x_exist()==1) )
        return 1;
    else
        return 0;    
}

#if 1
//==============================================================================
// Enternal BUCK status
//==============================================================================
extern int is_mt6311_sw_ready(void);
extern int is_mt6311_exist(void);
extern int get_mt6311_i2c_ch_num(void);

int get_ext_buck_i2c_ch_num(void)
{
    if(is_mt6311_exist()==1)
    {
        return get_mt6311_i2c_ch_num();
    }
    else
    {
        return -1;
    }
}

int is_ext_buck_sw_ready(void)
{    
    if( (is_mt6311_sw_ready()==1) )
        return 1;    
    else
        return 0;    
}

int is_ext_buck_exist(void)
{
    if( (is_mt6311_exist()==1) )
        return 1;
    else
        return 0;    
}

#endif // Enternal BUCK

//==============================================================================
// HW Setting 
//==============================================================================
void pmic_dig_reset(void)
{
#if 1
    U32 ret_val=0;
    
    //PMIC Digital reset
    ret_val=pmic_config_interface(MT6325_TOP_RST_MISC_CLR, 0x0002, 0xFFFF, 0); //[1]=0, RG_WDTRSTB_MODE
    ret_val=pmic_config_interface(MT6325_TOP_RST_MISC_SET, 0x0001, 0xFFFF, 0); //[0]=1, RG_WDTRSTB_EN
    printk("[pmic_dig_reset] Reg[0x%x]=0x%x\n", MT6325_TOP_RST_MISC, upmu_get_reg_value(MT6325_TOP_RST_MISC));        
#endif    
}

void pmic_full_reset(void)
{
#if 1
    U32 ret_val=0;

    //PMIC HW Full reset
    ret_val=pmic_config_interface(MT6325_TOP_RST_MISC_SET, 0x0002, 0xFFFF, 0); //[1]=1, RG_WDTRSTB_MODE
    ret_val=pmic_config_interface(MT6325_TOP_RST_MISC_SET, 0x0001, 0xFFFF, 0); //[0]=1, RG_WDTRSTB_EN
    printk("[pmic_full_reset] Reg[0x%x]=0x%x\n", MT6325_TOP_RST_MISC, upmu_get_reg_value(MT6325_TOP_RST_MISC));        
#endif    
}

void PMIC_INIT_SETTING_V1(void)
{
    U32 mt6325_chip_version = 0;
    U32 ret = 0;
    
    mt6325_chip_version = get_pmic_mt6325_cid();

    //--------------------------------------------------------
    if(mt6325_chip_version >= PMIC6325_E5_CID_CODE)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] 6325 PMIC Chip = 0x%x\n",mt6325_chip_version);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] 2014-08-20\n");

        //put init setting from DE/SA
        ret = pmic_config_interface(0x1E,0x0,0x1,11); // [11:11]: RG_TESTMODE_SWEN; CC: Test mode, first command
        ret = pmic_config_interface(0x4,0x1,0x1,4); // [4:4]: RG_EN_DRVSEL; Ricky
        ret = pmic_config_interface(0xA,0x1,0x1,0); // [0:0]: DDUVLO_DEB_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,0); // [0:0]: VDVFS11_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,1); // [1:1]: VDVFS12_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,4); // [4:4]: VCORE1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,5); // [5:5]: VCORE2_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,6); // [6:6]: VGPU_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,7); // [7:7]: VIO18_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,8); // [8:8]: VAUD28_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,9); // [9:9]: VTCXO_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,10); // [10:10]: VUSB_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,11); // [11:11]: VSRAM_DVFS1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,12); // [12:12]: VIO28_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,13); // [13:13]: VDRAM_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x10,0x1,0x1,5); // [5:5]: UVLO_L2H_DEB_EN; Ricky
        ret = pmic_config_interface(0x16,0x1,0x1,0); // [0:0]: STRUP_PWROFF_SEQ_EN; Ricky
        ret = pmic_config_interface(0x16,0x1,0x1,1); // [1:1]: STRUP_PWROFF_PREOFF_EN; Ricky
        ret = pmic_config_interface(0x2E,0x1,0x1,12); // [12:12]: RG_RST_DRVSEL; Simon
        ret = pmic_config_interface(0x204,0x1,0x1,4); // [4:4]: RG_SRCLKEN_IN0_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x204,0x1,0x1,5); // [5:5]: RG_SRCLKEN_IN1_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x204,0x1,0x1,6); // [6:6]: RG_OSC_SEL_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x222,0x1,0x1,0); // [0:0]: RG_SMT_WDTRSTB_IN; Ricky
        ret = pmic_config_interface(0x222,0x1,0x1,2); // [2:2]: RG_SMT_SRCLKEN_IN0; Ricky
        ret = pmic_config_interface(0x222,0x1,0x1,3); // [3:3]: RG_SMT_SRCLKEN_IN1; Ricky
        ret = pmic_config_interface(0x224,0x1,0x1,0); // [0:0]: RG_SMT_SPI_CLK; Ricky 7/8 enable SPI smith trigger
        ret = pmic_config_interface(0x224,0x1,0x1,1); // [1:1]: RG_SMT_SPI_CSN; Ricky 7/8 enable SPI smith trigger
        ret = pmic_config_interface(0x224,0x1,0x1,2); // [2:2]: RG_SMT_SPI_MOSI; Ricky 7/8 enable SPI smith trigger
        ret = pmic_config_interface(0x224,0x1,0x1,3); // [3:3]: RG_SMT_SPI_MISO; Ricky 7/8 enable SPI smith trigger
        ret = pmic_config_interface(0x23E,0x1,0x1,2); // [2:2]: RG_RTC_75K_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x23E,0x1,0x1,3); // [3:3]: RG_RTCDET_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x244,0x1,0x1,13); // [13:13]: RG_RTC_EOSC32_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x244,0x1,0x1,14); // [14:14]: RG_TRIM_75K_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x250,0x1,0x1,11); // [11:11]: RG_75K_32K_SEL; Angela
        ret = pmic_config_interface(0x250,0x2,0x3,14); // [15:14]: RG_OSC_SEL_HW_SRC_SEL; Juinn-Ting 7/8 for voice/sensor hub wake up
        ret = pmic_config_interface(0x268,0x1,0x1,10); // [10:10]: RG_AUXADC_CK_CKSEL_HWEN; ZF
        ret = pmic_config_interface(0x2BC,0x1,0x1,0); // [0:0]: RG_SLP_RW_EN; 
        ret = pmic_config_interface(0x42C,0x1,0x1,3); // [3:3]: VPA_EN_OC_SDN_SEL; 7/28 Chihao: disable VPA OC shutdown
        ret = pmic_config_interface(0x43A,0x2,0x7,5); // [7:5]: RG_VDRAM_RZSEL; Chihao: E3 only, 07/22
        ret = pmic_config_interface(0x43A,0x2,0x7,10); // [12:10]: RG_VDRAM_CSR; Chihao: E3 only, 07/22
        ret = pmic_config_interface(0x446,0x3,0x7,4); // [6:4]: RG_VCORE1_SLP; Chihao: E1 only, E2/E3 keep, johnson
        ret = pmic_config_interface(0x464,0x7,0x7,0); // [2:0]: RG_VDVFS11_CSR; Johsnon, performance tune. (E2/E3)
        ret = pmic_config_interface(0x464,0x7,0x7,3); // [5:3]: RG_VDVFS12_CSR; Johsnon, performance tune. (E2/E3)
        ret = pmic_config_interface(0x466,0x4,0x7,4); // [6:4]: RG_VDVFS11_SLP; Johsnon, performance tune. (E2/E3)
        ret = pmic_config_interface(0x466,0x4,0x7,7); // [9:7]: RG_VDVFS12_SLP; Johsnon, performance tune. (E2/E3)
        ret = pmic_config_interface(0x46A,0x0,0x1,8); // [8:8]: RG_VDVFS11_UVP_EN; Johsnon, E3 only. For spike of in phase
        ret = pmic_config_interface(0x46A,0x0,0x1,9); // [9:9]: RG_VDVFS12_UVP_EN; Johsnon, E3 only. For spike of in phase
        ret = pmic_config_interface(0x470,0x7,0xF,10); // [13:10]: RG_VDVFS11_PHS_SHED_TRIM; 7/28 Sam, for phase shieldding threshold fine tune. (E3)
        ret = pmic_config_interface(0x472,0x2,0x7,5); // [7:5]: RG_VGPU_RZSEL; Chihao: for compensation fine tune ,07/22
        ret = pmic_config_interface(0x474,0x5,0x7,4); // [6:4]: RG_VGPU_SLP; Chihao:  for compensation fine tune ,07/22
        ret = pmic_config_interface(0x484,0x1,0x7,5); // [7:5]: RG_VCORE2_RZSEL; Chihao: E1 only(100), E2/E3 (001) johnson
        ret = pmic_config_interface(0x486,0x3,0x7,4); // [6:4]: RG_VCORE2_SLP; Chihao
        ret = pmic_config_interface(0x490,0x2,0x7,4); // [6:4]: RG_VIO18_SLP; 7/28 Johnson, E3 for regulation improvement
        ret = pmic_config_interface(0x4B0,0x1,0x1,1); // [1:1]: VDVFS11_VOSEL_CTRL; Johnson: after VDVFS11_VOSEL_SLEEP
        ret = pmic_config_interface(0x4B6,0x7F,0x7F,0); // [6:0]: VDVFS11_SFCHG_FRATE; Johnson, update to avoide negative current when DVS down, 8/6.
        ret = pmic_config_interface(0x4B6,0x4,0x7F,8); // [14:8]: VDVFS11_SFCHG_RRATE; Johnson
        ret = pmic_config_interface(0x4BC,0x10,0x7F,0); // [6:0]: VDVFS11_VOSEL_SLEEP; 7/28 Johnson, E2/E3 Sleep mode =0.7V (follow AP sleep owner setting)
        ret = pmic_config_interface(0x4C6,0x3,0x3,0); // [1:0]: VDVFS11_TRANS_TD; ShangYing
        ret = pmic_config_interface(0x4C6,0x1,0x3,4); // [5:4]: VDVFS11_TRANS_CTRL; ShangYing
        ret = pmic_config_interface(0x502,0x11,0x7F,0); // [6:0]: VSRAM_DVFS1_SFCHG_FRATE; ShangYing
        ret = pmic_config_interface(0x502,0x4,0x7F,8); // [14:8]: VSRAM_DVFS1_SFCHG_RRATE; ShangYing
        ret = pmic_config_interface(0x538,0x1,0x1,8); // [8:8]: VDRAM_VSLEEP_EN; 7/8 Juinn-Ting/Chihao E2/E3 only Into low power mode when SRCLKEN=0
        ret = pmic_config_interface(0x54A,0x1,0x3,0); // [1:0]: VRF18_0_EN_SEL; Juinn-Ting; Put the settings before VRF18_0_EN_CTRL
        ret = pmic_config_interface(0x548,0x1,0x1,0); // [0:0]: VRF18_0_EN_CTRL; Juinn-Ting
        ret = pmic_config_interface(0x614,0x11,0x7F,0); // [6:0]: VGPU_SFCHG_FRATE; Chihao
        ret = pmic_config_interface(0x614,0x4,0x7F,8); // [14:8]: VGPU_SFCHG_RRATE; Chihao
        ret = pmic_config_interface(0x63A,0x11,0x7F,0); // [6:0]: VCORE1_SFCHG_FRATE; Johnson
        ret = pmic_config_interface(0x63A,0x4,0x7F,8); // [14:8]: VCORE1_SFCHG_RRATE; Johnson
        ret = pmic_config_interface(0x640,0x10,0x7F,0); // [6:0]: VCORE1_VOSEL_SLEEP; ShangYing: 0.7V
        ret = pmic_config_interface(0x634,0x1,0x1,1); // [1:1]: VCORE1_VOSEL_CTRL; Johnson: after VCORE1_VOSEL_SLEEP
        ret = pmic_config_interface(0x64A,0x1,0x1,8); // [8:8]: VCORE1_VSLEEP_EN; TY 7/8 E2/E3 only
        ret = pmic_config_interface(0x660,0x11,0x7F,0); // [6:0]: VCORE2_SFCHG_FRATE; Johnson
        ret = pmic_config_interface(0x660,0x4,0x7F,8); // [14:8]: VCORE2_SFCHG_RRATE; Johnson
        ret = pmic_config_interface(0x666,0x10,0x7F,0); // [6:0]: VCORE2_VOSEL_SLEEP; ShangYing: 0.7V
        ret = pmic_config_interface(0x65A,0x1,0x1,1); // [1:1]: VCORE2_VOSEL_CTRL; Johnson: after VCORE2_VOSEL_SLEEP
        ret = pmic_config_interface(0x670,0x1,0x1,8); // [8:8]: VCORE2_VSLEEP_EN; ShangYing: E2/E3 only
        ret = pmic_config_interface(0x6B0,0x4,0x7F,0); // [6:0]: VPA_SFCHG_FRATE; Chihao
        ret = pmic_config_interface(0x6B0,0x0,0x1,7); // [7:7]: VPA_SFCHG_FEN; Chihao
        ret = pmic_config_interface(0x6B0,0x4,0x7F,8); // [14:8]: VPA_SFCHG_RRATE; Chihao
        ret = pmic_config_interface(0x6B0,0x0,0x1,15); // [15:15]: VPA_SFCHG_REN; Chihao
        ret = pmic_config_interface(0x6CA,0x3,0x3,4); // [5:4]: VPA_DVS_TRANS_CTRL; Chihao
        ret = pmic_config_interface(0xA00,0x1,0x1,3); // [3:3]: RG_VTCXO0_ON_CTRL; TY
        ret = pmic_config_interface(0xA00,0x0,0x3,12); // [13:12]: RG_VTCXO0_SRCLK_EN_SEL; TY
        ret = pmic_config_interface(0xA04,0x1,0x1,2); // [2:2]: RG_VAUD28_MODE_CTRL; 7/28 TY set VAUD28 to low power mode control by SRCLKEN
        ret = pmic_config_interface(0xA06,0x1,0x1,2); // [2:2]: RG_VAUXA28_MODE_CTRL; TY
        ret = pmic_config_interface(0xA06,0x0,0x3,4); // [5:4]: RG_VAUXA28_SRCLK_MODE_SEL; TY
        ret = pmic_config_interface(0xA08,0x1,0x1,3); // [3:3]: RG_VBIF28_ON_CTRL; Ricky
        ret = pmic_config_interface(0xA08,0x0,0x3,12); // [13:12]: RG_VBIF28_SRCLK_EN_SEL; Ricky
        ret = pmic_config_interface(0xA12,0x1,0x1,2); // [2:2]: RG_VUSB33_MODE_CTRL; TY
        ret = pmic_config_interface(0xA12,0x0,0x3,4); // [5:4]: RG_VUSB33_SRCLK_MODE_SEL; TY
        ret = pmic_config_interface(0xA18,0x1,0x1,2); // [2:2]: RG_VEMC33_MODE_CTRL; TY
        ret = pmic_config_interface(0xA18,0x0,0x3,4); // [5:4]: RG_VEMC33_SRCLK_MODE_SEL; TY
        ret = pmic_config_interface(0xA1A,0x1,0x1,2); // [2:2]: RG_VIO28_MODE_CTRL; TY for sensor hub feature need to set 1'b0
        ret = pmic_config_interface(0xA1A,0x0,0x3,4); // [5:4]: RG_VIO28_SRCLK_MODE_SEL; TY
        ret = pmic_config_interface(0xA20,0x0,0x1,1); // [1:1]: RG_VEFUSE_EN; Fandy:Disable VEFUSE
        ret = pmic_config_interface(0xA38,0x0,0x1,1); // [1:1]: RG_VBIASN_EN; Fandy, disable
        ret = pmic_config_interface(0xA48,0x0,0x1,9); // [9:9]: RG_VMCH_VOSEL; Fandy: changed to 3V
        ret = pmic_config_interface(0xA4A,0x0,0x1,9); // [9:9]: RG_VEMC_3V3_VOSEL; Fandy: changed to 3V
        ret = pmic_config_interface(0xA60,0x0,0x1F,0); // [4:0]: RG_DLDO_2_RSV_L; Fandy:Enhance VCN33 fast transient
        ret = pmic_config_interface(0xA64,0xF,0x1F,11); // [15:11]: RG_ADLDO_RSV_H; Fandy: change to 3V A48[1]:A64[15] 2'b00: 1.5 V 2'b01: 1.8 V 2'b10: 3.0 V 2'b11: 3.3 V E2/E3 only
        ret = pmic_config_interface(0xCBC,0x1,0x1,8); // [8:8]: FG_SLP_EN; Ricky
        ret = pmic_config_interface(0xCBC,0x1,0x1,9); // [9:9]: FG_ZCV_DET_EN; Ricky
        ret = pmic_config_interface(0xCBC,0x1,0x1,10); // [10:10]: RG_FG_AUXADC_R; Ricky
        ret = pmic_config_interface(0xCC0,0x24,0xFFFF,0); // [15:0]: FG_SLP_CUR_TH; Ricky
        ret = pmic_config_interface(0xCC2,0x14,0xFF,0); // [7:0]: FG_SLP_TIME; Ricky
        ret = pmic_config_interface(0xCC4,0xFF,0xFF,8); // [15:8]: FG_DET_TIME; Ricky
        ret = pmic_config_interface(0xCEE,0x1,0x1,1); // [1:1]: RG_AUDGLB_PWRDN_VA28; Eugene 7/8 Disable audio globe bias
        ret = pmic_config_interface(0xE12,0x0,0x1,15); // [15:15]: AUXADC_CK_AON; ZF
        ret = pmic_config_interface(0xE9C,0x2,0x3,0); // [1:0]: RG_ADC_TRIM_CH7_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x1,0x3,2); // [3:2]: RG_ADC_TRIM_CH6_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x1,0x3,4); // [5:4]: RG_ADC_TRIM_CH5_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x1,0x3,6); // [7:6]: RG_ADC_TRIM_CH4_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x3,0x3,8); // [9:8]: RG_ADC_TRIM_CH3_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x1,0x3,10); // [11:10]: RG_ADC_TRIM_CH2_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x0,0x3,14); // [15:14]: RG_ADC_TRIM_CH0_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xEBE,0x1,0x1,15); // [15:15]: RG_VREF18_ENB_MD; KH 7/8 For K2 no GPS co-clock
        
        #if defined(MTK_PUMP_EXPRESS_PLUS_SUPPORT)
        ret = pmic_config_interface(0xEF4,0xF,0xF,4); // [7:4]: RG_VCDT_HV_VTH; Tim:VCDT_HV_th=10.5V
        #else
        ret = pmic_config_interface(0xEF4,0xB,0xF,4); // [7:4]: RG_VCDT_HV_VTH; Tim:VCDT_HV_th=7V                
        #endif         

        ret = pmic_config_interface(0xEFE,0x4,0xF,1); // [4:1]: RG_VBAT_OV_VTH; Tim:for 4.35 battery
        ret = pmic_config_interface(0xF0C,0x3,0xF,0); // [3:0]: RG_CHRWDT_TD; Tim:WDT=32s
        ret = pmic_config_interface(0xF1A,0x1,0x1,1); // [1:1]: RG_BC11_RST; Tim:Disable BC1.1 timer
        ret = pmic_config_interface(0xF1E,0x0,0x7,4); // [6:4]: RG_CSDAC_STP_DEC; Tim:Reduce ICHG current ripple (align 6323)
        ret = pmic_config_interface(0xF24,0x1,0x1,2); // [2:2]: RG_CSDAC_MODE; Tim:Align 6323
        ret = pmic_config_interface(0xF24,0x1,0x1,6); // [6:6]: RG_HWCV_EN; Tim:Align 6323
        ret = pmic_config_interface(0xF24,0x1,0x1,7); // [7:7]: RG_ULC_DET_EN; Tim:Align 6323
        ret = pmic_config_interface(0x00E,0x3,0x3,0); // [1:0]: DVFS11_PG_ENB, DVFS12_PG_ENB; 8/11 ShangYing, disable DVFS11_DVFS12 PGOOD detection.
    }
    else if(mt6325_chip_version >= PMIC6325_E3_CID_CODE)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] 6325 PMIC Chip = 0x%x\n",mt6325_chip_version);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] 2014-08-20..\n");

        //put init setting from DE/SA
        ret = pmic_config_interface(0x1E,0x0,0x1,11); // [11:11]: RG_TESTMODE_SWEN; CC: Test mode, first command
        ret = pmic_config_interface(0x4,0x1,0x1,4); // [4:4]: RG_EN_DRVSEL; Ricky
        ret = pmic_config_interface(0xA,0x1,0x1,0); // [0:0]: DDUVLO_DEB_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,0); // [0:0]: VDVFS11_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,1); // [1:1]: VDVFS12_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,4); // [4:4]: VCORE1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,5); // [5:5]: VCORE2_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,6); // [6:6]: VGPU_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,7); // [7:7]: VIO18_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,8); // [8:8]: VAUD28_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,9); // [9:9]: VTCXO_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,10); // [10:10]: VUSB_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,11); // [11:11]: VSRAM_DVFS1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,12); // [12:12]: VIO28_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,13); // [13:13]: VDRAM_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xE,0x1,0x1,0); // [0:0]: VDVFS11_PG_ENB; 8/11 ShangYing, disable VDVFS power good detection
        ret = pmic_config_interface(0xE,0x1,0x1,1); // [1:1]: VDVFS12_PG_ENB; 8/11 ShangYing, disable VDVFS power good detection
        ret = pmic_config_interface(0x10,0x1,0x1,5); // [5:5]: UVLO_L2H_DEB_EN; Ricky
        ret = pmic_config_interface(0x16,0x1,0x1,0); // [0:0]: STRUP_PWROFF_SEQ_EN; Ricky
        ret = pmic_config_interface(0x16,0x1,0x1,1); // [1:1]: STRUP_PWROFF_PREOFF_EN; Ricky
        ret = pmic_config_interface(0x2E,0x1,0x1,12); // [12:12]: RG_RST_DRVSEL; Simon
        ret = pmic_config_interface(0x204,0x1,0x1,4); // [4:4]: RG_SRCLKEN_IN0_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x204,0x1,0x1,5); // [5:5]: RG_SRCLKEN_IN1_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x204,0x1,0x1,6); // [6:6]: RG_OSC_SEL_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x222,0x1,0x1,0); // [0:0]: RG_SMT_WDTRSTB_IN; Ricky
        ret = pmic_config_interface(0x222,0x1,0x1,2); // [2:2]: RG_SMT_SRCLKEN_IN0; Ricky
        ret = pmic_config_interface(0x222,0x1,0x1,3); // [3:3]: RG_SMT_SRCLKEN_IN1; Ricky
        ret = pmic_config_interface(0x224,0x1,0x1,0); // [0:0]: RG_SMT_SPI_CLK; Ricky 7/8 enable SPI smith trigger
        ret = pmic_config_interface(0x224,0x1,0x1,1); // [1:1]: RG_SMT_SPI_CSN; Ricky 7/8 enable SPI smith trigger
        ret = pmic_config_interface(0x224,0x1,0x1,2); // [2:2]: RG_SMT_SPI_MOSI; Ricky 7/8 enable SPI smith trigger
        ret = pmic_config_interface(0x224,0x1,0x1,3); // [3:3]: RG_SMT_SPI_MISO; Ricky 7/8 enable SPI smith trigger
        ret = pmic_config_interface(0x23E,0x1,0x1,2); // [2:2]: RG_RTC_75K_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x23E,0x1,0x1,3); // [3:3]: RG_RTCDET_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x244,0x1,0x1,13); // [13:13]: RG_RTC_EOSC32_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x244,0x1,0x1,14); // [14:14]: RG_TRIM_75K_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x250,0x1,0x1,11); // [11:11]: RG_75K_32K_SEL; Angela
        ret = pmic_config_interface(0x250,0x2,0x3,14); // [15:14]: RG_OSC_SEL_HW_SRC_SEL; Juinn-Ting 7/8 for voice/sensor hub wake up
        ret = pmic_config_interface(0x268,0x1,0x1,10); // [10:10]: RG_AUXADC_CK_CKSEL_HWEN; ZF
        ret = pmic_config_interface(0x2BC,0x1,0x1,0); // [0:0]: RG_SLP_RW_EN; 
        ret = pmic_config_interface(0x42C,0x1,0x1,3); // [3:3]: VPA_EN_OC_SDN_SEL; 7/28 Chihao: disable VPA OC shutdown
        ret = pmic_config_interface(0x43A,0x2,0x7,5); // [7:5]: RG_VDRAM_RZSEL; Chihao: E3 only, 07/22
        ret = pmic_config_interface(0x43A,0x2,0x7,10); // [12:10]: RG_VDRAM_CSR; Chihao: E3 only, 07/22
        ret = pmic_config_interface(0x446,0x3,0x7,4); // [6:4]: RG_VCORE1_SLP; Chihao: E1 only, E2/E3 keep, johnson
        ret = pmic_config_interface(0x464,0x7,0x7,0); // [2:0]: RG_VDVFS11_CSR; Johsnon, performance tune. (E2/E3)
        ret = pmic_config_interface(0x464,0x7,0x7,3); // [5:3]: RG_VDVFS12_CSR; Johsnon, performance tune. (E2/E3)
        ret = pmic_config_interface(0x466,0x4,0x7,4); // [6:4]: RG_VDVFS11_SLP; Johsnon, performance tune. (E2/E3)
        ret = pmic_config_interface(0x466,0x4,0x7,7); // [9:7]: RG_VDVFS12_SLP; Johsnon, performance tune. (E2/E3)
        ret = pmic_config_interface(0x46A,0x0,0x1,8); // [8:8]: RG_VDVFS11_UVP_EN; Johsnon, E3 only. For spike of in phase
        ret = pmic_config_interface(0x46A,0x0,0x1,9); // [9:9]: RG_VDVFS12_UVP_EN; Johsnon, E3 only. For spike of in phase
        ret = pmic_config_interface(0x470,0x7,0xF,10); // [13:10]: RG_VDVFS11_PHS_SHED_TRIM; 7/28 Sam, for phase shieldding threshold fine tune. (E3)
        ret = pmic_config_interface(0x472,0x4,0x7,5); // [7:5]: RG_VGPU_RZSEL; 8/19: Chihao: for compensation fine tune, reduce voltage drop from PFM to PWM.
        ret = pmic_config_interface(0x474,0x5,0x7,4); // [6:4]: RG_VGPU_SLP; Chihao:  for compensation fine tune ,07/22
        ret = pmic_config_interface(0x484,0x4,0x7,5); // [7:5]: RG_VCORE2_RZSEL; 8/19: Chihao: for compensation fine tune, reduce voltage drop from PFM to PWM.
        ret = pmic_config_interface(0x486,0x3,0x7,4); // [6:4]: RG_VCORE2_SLP; Chihao
        ret = pmic_config_interface(0x48E,0x3,0x7,5); // [7:5]: RG_VIO18_RZSEL; 8/19: Chihao: for compensation fine tune, reduce voltage drop from PFM to PWM.
        ret = pmic_config_interface(0x490,0x2,0x7,4); // [6:4]: RG_VIO18_SLP; 7/28 Johnson, E3 for regulation improvement
        ret = pmic_config_interface(0x4B0,0x1,0x1,1); // [1:1]: VDVFS11_VOSEL_CTRL; Johnson: after VDVFS11_VOSEL_SLEEP
        ret = pmic_config_interface(0x4B6,0x7F,0x7F,0); // [6:0]: VDVFS11_SFCHG_FRATE; Johnson, update to avoide negative current when DVS down, 8/6.
        ret = pmic_config_interface(0x4B6,0x4,0x7F,8); // [14:8]: VDVFS11_SFCHG_RRATE; Johnson
        ret = pmic_config_interface(0x4BC,0x10,0x7F,0); // [6:0]: VDVFS11_VOSEL_SLEEP; 7/28 Johnson, E2/E3 Sleep mode =0.7V (follow AP sleep owner setting)
        ret = pmic_config_interface(0x4C6,0x3,0x3,0); // [1:0]: VDVFS11_TRANS_TD; ShangYing
        ret = pmic_config_interface(0x4C6,0x1,0x3,4); // [5:4]: VDVFS11_TRANS_CTRL; ShangYing
        ret = pmic_config_interface(0x502,0x11,0x7F,0); // [6:0]: VSRAM_DVFS1_SFCHG_FRATE; ShangYing
        ret = pmic_config_interface(0x502,0x4,0x7F,8); // [14:8]: VSRAM_DVFS1_SFCHG_RRATE; ShangYing
        ret = pmic_config_interface(0x538,0x1,0x1,8); // [8:8]: VDRAM_VSLEEP_EN; 7/8 Juinn-Ting/Chihao E2/E3 only Into low power mode when SRCLKEN=0
        ret = pmic_config_interface(0x54A,0x1,0x3,0); // [1:0]: VRF18_0_EN_SEL; Juinn-Ting; Put the settings before VRF18_0_EN_CTRL
        ret = pmic_config_interface(0x548,0x1,0x1,0); // [0:0]: VRF18_0_EN_CTRL; Juinn-Ting
        ret = pmic_config_interface(0x614,0x11,0x7F,0); // [6:0]: VGPU_SFCHG_FRATE; Chihao
        ret = pmic_config_interface(0x614,0x4,0x7F,8); // [14:8]: VGPU_SFCHG_RRATE; Chihao
        ret = pmic_config_interface(0x63A,0x11,0x7F,0); // [6:0]: VCORE1_SFCHG_FRATE; Johnson
        ret = pmic_config_interface(0x63A,0x4,0x7F,8); // [14:8]: VCORE1_SFCHG_RRATE; Johnson
        ret = pmic_config_interface(0x640,0x10,0x7F,0); // [6:0]: VCORE1_VOSEL_SLEEP; ShangYing: 0.7V
        ret = pmic_config_interface(0x64A,0x1,0x1,8); // [8:8]: VCORE1_VSLEEP_EN; TY 7/8 E2/E3 only
        ret = pmic_config_interface(0x634,0x1,0x1,1); // [1:1]: VCORE1_VOSEL_CTRL; Johnson: after VCORE1_VOSEL_SLEEP
        ret = pmic_config_interface(0x660,0x11,0x7F,0); // [6:0]: VCORE2_SFCHG_FRATE; Johnson
        ret = pmic_config_interface(0x660,0x4,0x7F,8); // [14:8]: VCORE2_SFCHG_RRATE; Johnson
        ret = pmic_config_interface(0x666,0x10,0x7F,0); // [6:0]: VCORE2_VOSEL_SLEEP; ShangYing: 0.7V
        ret = pmic_config_interface(0x670,0x1,0x1,8); // [8:8]: VCORE2_VSLEEP_EN; ShangYing: E2/E3 only
        ret = pmic_config_interface(0x65A,0x1,0x1,1); // [1:1]: VCORE2_VOSEL_CTRL; Johnson: after VCORE2_VOSEL_SLEEP
        ret = pmic_config_interface(0x6B0,0x4,0x7F,0); // [6:0]: VPA_SFCHG_FRATE; Chihao
        ret = pmic_config_interface(0x6B0,0x0,0x1,7); // [7:7]: VPA_SFCHG_FEN; Chihao
        ret = pmic_config_interface(0x6B0,0x4,0x7F,8); // [14:8]: VPA_SFCHG_RRATE; Chihao
        ret = pmic_config_interface(0x6B0,0x0,0x1,15); // [15:15]: VPA_SFCHG_REN; Chihao
        ret = pmic_config_interface(0x6CA,0x3,0x3,4); // [5:4]: VPA_DVS_TRANS_CTRL; Chihao
        ret = pmic_config_interface(0xA00,0x1,0x1,3); // [3:3]: RG_VTCXO0_ON_CTRL; TY
        ret = pmic_config_interface(0xA00,0x0,0x3,12); // [13:12]: RG_VTCXO0_SRCLK_EN_SEL; TY
        ret = pmic_config_interface(0xA04,0x1,0x1,2); // [2:2]: RG_VAUD28_MODE_CTRL; 7/28 TY set VAUD28 to low power mode control by SRCLKEN
        ret = pmic_config_interface(0xA06,0x1,0x1,2); // [2:2]: RG_VAUXA28_MODE_CTRL; TY
        ret = pmic_config_interface(0xA06,0x0,0x3,4); // [5:4]: RG_VAUXA28_SRCLK_MODE_SEL; TY
        ret = pmic_config_interface(0xA08,0x1,0x1,3); // [3:3]: RG_VBIF28_ON_CTRL; Ricky
        ret = pmic_config_interface(0xA08,0x0,0x3,12); // [13:12]: RG_VBIF28_SRCLK_EN_SEL; Ricky
        ret = pmic_config_interface(0xA12,0x1,0x1,2); // [2:2]: RG_VUSB33_MODE_CTRL; TY
        ret = pmic_config_interface(0xA12,0x0,0x3,4); // [5:4]: RG_VUSB33_SRCLK_MODE_SEL; TY
        ret = pmic_config_interface(0xA18,0x1,0x1,2); // [2:2]: RG_VEMC33_MODE_CTRL; TY
        ret = pmic_config_interface(0xA18,0x0,0x3,4); // [5:4]: RG_VEMC33_SRCLK_MODE_SEL; TY
        ret = pmic_config_interface(0xA1A,0x1,0x1,2); // [2:2]: RG_VIO28_MODE_CTRL; TY for sensor hub feature need to set 1'b0
        ret = pmic_config_interface(0xA1A,0x0,0x3,4); // [5:4]: RG_VIO28_SRCLK_MODE_SEL; TY
        ret = pmic_config_interface(0xA20,0x0,0x1,1); // [1:1]: RG_VEFUSE_EN; Fandy:Disable VEFUSE
        ret = pmic_config_interface(0xA38,0x0,0x1,1); // [1:1]: RG_VBIASN_EN; Fandy, disable
        ret = pmic_config_interface(0xA48,0x0,0x1,9); // [9:9]: RG_VMCH_VOSEL; Fandy: changed to 3V
        ret = pmic_config_interface(0xA4A,0x0,0x1,9); // [9:9]: RG_VEMC_3V3_VOSEL; Fandy: changed to 3V
        ret = pmic_config_interface(0xA60,0x0,0x1F,0); // [4:0]: RG_DLDO_2_RSV_L; Fandy:Enhance VCN33 fast transient
        ret = pmic_config_interface(0xA64,0xF,0x1F,11); // [15:11]: RG_ADLDO_RSV_H; Fandy: change to 3V A48[1]:A64[15] 2'b00: 1.5 V 2'b01: 1.8 V 2'b10: 3.0 V 2'b11: 3.3 V E2/E3 only
        ret = pmic_config_interface(0xCBC,0x1,0x1,8); // [8:8]: FG_SLP_EN; Ricky
        ret = pmic_config_interface(0xCBC,0x1,0x1,9); // [9:9]: FG_ZCV_DET_EN; Ricky
        ret = pmic_config_interface(0xCBC,0x1,0x1,10); // [10:10]: RG_FG_AUXADC_R; Ricky
        ret = pmic_config_interface(0xCC0,0x24,0xFFFF,0); // [15:0]: FG_SLP_CUR_TH; Ricky
        ret = pmic_config_interface(0xCC2,0x14,0xFF,0); // [7:0]: FG_SLP_TIME; Ricky
        ret = pmic_config_interface(0xCC4,0xFF,0xFF,8); // [15:8]: FG_DET_TIME; Ricky
        ret = pmic_config_interface(0xCEE,0x1,0x1,1); // [1:1]: RG_AUDGLB_PWRDN_VA28; Eugene 7/8 Disable audio globe bias
        ret = pmic_config_interface(0xE12,0x0,0x1,15); // [15:15]: AUXADC_CK_AON; ZF
        ret = pmic_config_interface(0xE9C,0x2,0x3,0); // [1:0]: RG_ADC_TRIM_CH7_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x1,0x3,2); // [3:2]: RG_ADC_TRIM_CH6_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x1,0x3,4); // [5:4]: RG_ADC_TRIM_CH5_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x1,0x3,6); // [7:6]: RG_ADC_TRIM_CH4_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x3,0x3,8); // [9:8]: RG_ADC_TRIM_CH3_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x1,0x3,10); // [11:10]: RG_ADC_TRIM_CH2_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x0,0x3,14); // [15:14]: RG_ADC_TRIM_CH0_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xEBE,0x1,0x1,15); // [15:15]: RG_VREF18_ENB_MD; KH 7/8 For K2 no GPS co-clock
        
        #if defined(MTK_PUMP_EXPRESS_PLUS_SUPPORT)
        ret = pmic_config_interface(0xEF4,0xF,0xF,4); // [7:4]: RG_VCDT_HV_VTH; Tim:VCDT_HV_th=10.5V
        #else
        ret = pmic_config_interface(0xEF4,0xB,0xF,4); // [7:4]: RG_VCDT_HV_VTH; Tim:VCDT_HV_th=7V                
        #endif 
    
        ret = pmic_config_interface(0xEFE,0x4,0xF,1); // [4:1]: RG_VBAT_OV_VTH; Tim:for 4.35 battery
        ret = pmic_config_interface(0xF0C,0x3,0xF,0); // [3:0]: RG_CHRWDT_TD; Tim:WDT=32s
        ret = pmic_config_interface(0xF1A,0x1,0x1,1); // [1:1]: RG_BC11_RST; Tim:Disable BC1.1 timer
        ret = pmic_config_interface(0xF1E,0x0,0x7,4); // [6:4]: RG_CSDAC_STP_DEC; Tim:Reduce ICHG current ripple (align 6323)
        ret = pmic_config_interface(0xF24,0x1,0x1,2); // [2:2]: RG_CSDAC_MODE; Tim:Align 6323
        ret = pmic_config_interface(0xF24,0x1,0x1,6); // [6:6]: RG_HWCV_EN; Tim:Align 6323
        ret = pmic_config_interface(0xF24,0x1,0x1,7); // [7:7]: RG_ULC_DET_EN; Tim:Align 6323
    }
    else if(mt6325_chip_version >= PMIC6325_E1_CID_CODE)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] 6325 PMIC Chip = 0x%x\n",mt6325_chip_version);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] 2014-08-15\n");

        //put init setting from DE/SA      
        ret = pmic_config_interface(0x1E,0x0,0x1,11); // [11:11]: RG_TESTMODE_SWEN; CC: Test mode, first command
        ret = pmic_config_interface(0x462,0x1,0x1,3); // [3:3]: RG_VDVFS1_VRF18_SSTART_EN; Chihao, E2 only (force two phase)
        ret = pmic_config_interface(0x466,0x1,0x1,14); // [14:14]: RG_VDVFS11_MODESET; Chihao, E2 only force PWM
        ret = pmic_config_interface(0x466,0x1,0x1,15); // [15:15]: RG_VDVFS12_MODESET; Chihao, E2 only force PWM
        ret = pmic_config_interface(0x43C,0x1,0x1,15); // [15:15]: RG_VDRAM_MODESET; ShangYing: E2 only force PWM
        ret = pmic_config_interface(0x446,0x1,0x1,15); // [15:15]: RG_VCORE1_MODESET; ShangYing: E2 only force PWM
        ret = pmic_config_interface(0x474,0x1,0x1,15); // [15:15]: RG_VGPU_MODESET; ShangYing: E2 only force PWM
        ret = pmic_config_interface(0x486,0x1,0x1,15); // [15:15]: RG_VCORE2_MODESET; ShangYing: E2 only force PWM
        ret = pmic_config_interface(0x490,0x1,0x1,9); // [9:9]: RG_VIO18_MODESET; ShangYing: E2 only force PWM
        ret = pmic_config_interface(0x4,0x1,0x1,4); // [4:4]: RG_EN_DRVSEL; Ricky
        ret = pmic_config_interface(0xA,0x1,0x1,0); // [0:0]: DDUVLO_DEB_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,0); // [0:0]: VDVFS11_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,1); // [1:1]: VDVFS12_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,4); // [4:4]: VCORE1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,5); // [5:5]: VCORE2_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,6); // [6:6]: VGPU_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,7); // [7:7]: VIO18_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,8); // [8:8]: VAUD28_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,9); // [9:9]: VTCXO_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,10); // [10:10]: VUSB_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,11); // [11:11]: VSRAM_DVFS1_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,12); // [12:12]: VIO28_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0xC,0x1,0x1,13); // [13:13]: VDRAM_PG_H2L_EN; Ricky
        ret = pmic_config_interface(0x10,0x1,0x1,5); // [5:5]: UVLO_L2H_DEB_EN; Ricky
        ret = pmic_config_interface(0x16,0x1,0x1,0); // [0:0]: STRUP_PWROFF_SEQ_EN; Ricky
        ret = pmic_config_interface(0x16,0x1,0x1,1); // [1:1]: STRUP_PWROFF_PREOFF_EN; Ricky
        ret = pmic_config_interface(0x2E,0x1,0x1,12); // [12:12]: RG_RST_DRVSEL; Simon
        ret = pmic_config_interface(0x204,0x1,0x1,4); // [4:4]: RG_SRCLKEN_IN0_HW_MODE; Juinn-Ting
        ret = pmic_config_interface(0x204,0x1,0x1,5); // [5:5]: RG_SRCLKEN_IN1_HW_MODE; Juinn-Ting
        //ret = pmic_config_interface(0x204,0x1,0x1,6); // [6:6]: RG_OSC_SEL_HW_MODE; Juinn-Ting E2/E3 only
        ret = pmic_config_interface(0x222,0x1,0x1,0); // [0:0]: RG_SMT_WDTRSTB_IN; Ricky
        ret = pmic_config_interface(0x222,0x1,0x1,2); // [2:2]: RG_SMT_SRCLKEN_IN0; Ricky
        ret = pmic_config_interface(0x222,0x1,0x1,3); // [3:3]: RG_SMT_SRCLKEN_IN1; Ricky
        ret = pmic_config_interface(0x224,0x1,0x1,0); // [0:0]: RG_SMT_SPI_CLK; Ricky 7/8 enable SPI smith trigger
        ret = pmic_config_interface(0x224,0x1,0x1,1); // [1:1]: RG_SMT_SPI_CSN; Ricky 7/8 enable SPI smith trigger
        ret = pmic_config_interface(0x224,0x1,0x1,2); // [2:2]: RG_SMT_SPI_MOSI; Ricky 7/8 enable SPI smith trigger
        ret = pmic_config_interface(0x224,0x1,0x1,3); // [3:3]: RG_SMT_SPI_MISO; Ricky 7/8 enable SPI smith trigger
        ret = pmic_config_interface(0x23E,0x1,0x1,2); // [2:2]: RG_RTC_75K_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x23E,0x1,0x1,3); // [3:3]: RG_RTCDET_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x244,0x1,0x1,13); // [13:13]: RG_RTC_EOSC32_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x244,0x1,0x1,14); // [14:14]: RG_TRIM_75K_CK_PDN; Juinn-Ting
        ret = pmic_config_interface(0x250,0x1,0x1,11); // [11:11]: RG_75K_32K_SEL; Angela
        ret = pmic_config_interface(0x250,0x2,0x3,14); // [15:14]: RG_OSC_SEL_HW_SRC_SEL; Juinn-Ting 7/8 for voice/sensor hub wake up
        ret = pmic_config_interface(0x268,0x1,0x1,10); // [10:10]: RG_AUXADC_CK_CKSEL_HWEN; ZF
        ret = pmic_config_interface(0x446,0x3,0x7,4); // [6:4]: RG_VCORE1_SLP; Chihao: E1 only, E2 keep, johnson
        ret = pmic_config_interface(0x462,0xF,0xF,5); // [8:5]: RG_VDVFS11_RZSEL; Johnson, E2 performance fine tune
        ret = pmic_config_interface(0x464,0x7,0x7,0); // [2:0]: RG_VDVFS11_CSR; Johsnon, performance tune. (E2)
        ret = pmic_config_interface(0x464,0x7,0x7,3); // [5:3]: RG_VDVFS12_CSR; Johsnon, performance tune. (E2)
        ret = pmic_config_interface(0x466,0x4,0x7,4); // [6:4]: RG_VDVFS11_SLP; Johsnon, performance tune. (E2)
        ret = pmic_config_interface(0x466,0x4,0x7,7); // [9:7]: RG_VDVFS12_SLP; Johsnon, performance tune. (E2)
        ret = pmic_config_interface(0x468,0xF7,0xFF,2); // [9:2]: RG_VDVFS11_TRANS_BST; Johsnon, performance tune. (E2)
        ret = pmic_config_interface(0x472,0x2,0x7,5); // [7:5]: RG_VGPU_RZSEL; Chihao: E1 only, E2(010)
        ret = pmic_config_interface(0x474,0x5,0x7,4); // [6:4]: RG_VGPU_SLP; Chihao: E1 only, E2(101)
        ret = pmic_config_interface(0x484,0x1,0x7,5); // [7:5]: RG_VCORE2_RZSEL; Chihao: E1 only(100), E2 (001) johnson
        ret = pmic_config_interface(0x486,0x3,0x7,4); // [6:4]: RG_VCORE2_SLP; Chihao: E1 only, E2 same E1, Johnson
        ret = pmic_config_interface(0x4B0,0x1,0x1,1); // [1:1]: VDVFS11_VOSEL_CTRL; ShangYing
        ret = pmic_config_interface(0x4B6,0x11,0x7F,0); // [6:0]: VDVFS11_SFCHG_FRATE; Johnson
        ret = pmic_config_interface(0x4B6,0x4,0x7F,8); // [14:8]: VDVFS11_SFCHG_RRATE; Johnson
        ret = pmic_config_interface(0x4C6,0x3,0x3,0); // [1:0]: VDVFS11_TRANS_TD; ShangYing
        ret = pmic_config_interface(0x4C6,0x1,0x3,4); // [5:4]: VDVFS11_TRANS_CTRL; ShangYing
        ret = pmic_config_interface(0x502,0x11,0x7F,0); // [6:0]: VSRAM_DVFS1_SFCHG_FRATE; ShangYing
        ret = pmic_config_interface(0x502,0x4,0x7F,8); // [14:8]: VSRAM_DVFS1_SFCHG_RRATE; ShangYing
        ret = pmic_config_interface(0x538,0x1,0x1,8); // [8:8]: VDRAM_VSLEEP_EN; 7/8 Juinn-Ting/Chihao E2/E3 only Into low power mode when SRCLKEN=0
        ret = pmic_config_interface(0x54A,0x1,0x3,0); // [1:0]: VRF18_0_EN_SEL; Juinn-Ting; Put the settings before VRF18_0_EN_CTRL
        ret = pmic_config_interface(0x548,0x1,0x1,0); // [0:0]: VRF18_0_EN_CTRL; Juinn-Ting
        ret = pmic_config_interface(0x614,0x11,0x7F,0); // [6:0]: VGPU_SFCHG_FRATE; Chihao
        ret = pmic_config_interface(0x614,0x4,0x7F,8); // [14:8]: VGPU_SFCHG_RRATE; Chihao
        ret = pmic_config_interface(0x634,0x1,0x1,1); // [1:1]: VCORE1_VOSEL_CTRL; ShangYing
        ret = pmic_config_interface(0x63A,0x11,0x7F,0); // [6:0]: VCORE1_SFCHG_FRATE; Johnson
        ret = pmic_config_interface(0x63A,0x4,0x7F,8); // [14:8]: VCORE1_SFCHG_RRATE; Johnson
        ret = pmic_config_interface(0x640,0x10,0x7F,0); // [6:0]: VCORE1_VOSEL_SLEEP; ShangYing: 0.7V
        ret = pmic_config_interface(0x64A,0x1,0x1,8); // [8:8]: VCORE1_VSLEEP_EN; TY 7/8 E2/E3 only
        ret = pmic_config_interface(0x65A,0x1,0x1,1); // [1:1]: VCORE2_VOSEL_CTRL; ShangYing
        ret = pmic_config_interface(0x660,0x11,0x7F,0); // [6:0]: VCORE2_SFCHG_FRATE; Johnson
        ret = pmic_config_interface(0x660,0x4,0x7F,8); // [14:8]: VCORE2_SFCHG_RRATE; Johnson
        ret = pmic_config_interface(0x666,0x10,0x7F,0); // [6:0]: VCORE2_VOSEL_SLEEP; ShangYing: 0.7V
        ret = pmic_config_interface(0x670,0x1,0x1,8); // [8:8]: VCORE2_VSLEEP_EN; ShangYing: E2/E3 only
        ret = pmic_config_interface(0x6B0,0x4,0x7F,0); // [6:0]: VPA_SFCHG_FRATE; Chihao
        ret = pmic_config_interface(0x6B0,0x0,0x1,7); // [7:7]: VPA_SFCHG_FEN; Chihao
        ret = pmic_config_interface(0x6B0,0x4,0x7F,8); // [14:8]: VPA_SFCHG_RRATE; Chihao
        ret = pmic_config_interface(0x6B0,0x0,0x1,15); // [15:15]: VPA_SFCHG_REN; Chihao
        ret = pmic_config_interface(0x6CA,0x3,0x3,4); // [5:4]: VPA_DVS_TRANS_CTRL; Chihao
        ret = pmic_config_interface(0xA00,0x1,0x1,3); // [3:3]: RG_VTCXO0_ON_CTRL; TY
        ret = pmic_config_interface(0xA00,0x0,0x3,12); // [13:12]: RG_VTCXO0_SRCLK_EN_SEL; TY
        ret = pmic_config_interface(0xA06,0x1,0x1,2); // [2:2]: RG_VAUXA28_MODE_CTRL; TY
        ret = pmic_config_interface(0xA06,0x0,0x3,4); // [5:4]: RG_VAUXA28_SRCLK_MODE_SEL; TY
        ret = pmic_config_interface(0xA08,0x1,0x1,3); // [3:3]: RG_VBIF28_ON_CTRL; Ricky
        ret = pmic_config_interface(0xA08,0x0,0x3,12); // [13:12]: RG_VBIF28_SRCLK_EN_SEL; Ricky
        ret = pmic_config_interface(0xA12,0x1,0x1,2); // [2:2]: RG_VUSB33_MODE_CTRL; TY
        ret = pmic_config_interface(0xA12,0x0,0x3,4); // [5:4]: RG_VUSB33_SRCLK_MODE_SEL; TY
        ret = pmic_config_interface(0xA18,0x1,0x1,2); // [2:2]: RG_VEMC33_MODE_CTRL; TY
        ret = pmic_config_interface(0xA18,0x0,0x3,4); // [5:4]: RG_VEMC33_SRCLK_MODE_SEL; TY
        ret = pmic_config_interface(0xA1A,0x1,0x1,2); // [2:2]: RG_VIO28_MODE_CTRL; TY for sensor hub feature need to set 1'b0
        ret = pmic_config_interface(0xA1A,0x0,0x3,4); // [5:4]: RG_VIO28_SRCLK_MODE_SEL; TY
        ret = pmic_config_interface(0xA20,0x0,0x1,1); // [1:1]: RG_VEFUSE_EN; Fandy:Disable VEFUSE
        ret = pmic_config_interface(0xA38,0x0,0x1,1); // [1:1]: RG_VBIASN_EN; Fandy, disable
        ret = pmic_config_interface(0xA48,0x0,0x1,9); // [9:9]: RG_VMCH_VOSEL; Fandy: changed to 3V
        ret = pmic_config_interface(0xA4A,0x0,0x1,9); // [9:9]: RG_VEMC_3V3_VOSEL; Fandy: changed to 3V
        ret = pmic_config_interface(0xA60,0x0,0x1F,0); // [4:0]: RG_DLDO_2_RSV_L; Fandy:Enhance VCN33 fast transient
        ret = pmic_config_interface(0xA64,0xF,0x1F,11); // [15:11]: RG_ADLDO_RSV_H; Fandy: change to 3V A48[1]:A64[15] 2'b00: 1.5 V 2'b01: 1.8 V 2'b10: 3.0 V 2'b11: 3.3 V E2/E3 only
        ret = pmic_config_interface(0xC14,0x1,0x1,0); // [0:0]: RG_SKIP_OTP_OUT; Fandy: for CORE power(VDVFS1x, VCOREx, VSRAM_DVFS) max voltage limitation.
        ret = pmic_config_interface(0xCBC,0x1,0x1,8); // [8:8]: FG_SLP_EN; Ricky
        ret = pmic_config_interface(0xCBC,0x1,0x1,9); // [9:9]: FG_ZCV_DET_EN; Ricky
        ret = pmic_config_interface(0xCBC,0x1,0x1,10); // [10:10]: RG_FG_AUXADC_R; Ricky
        ret = pmic_config_interface(0xCC0,0x24,0xFFFF,0); // [15:0]: FG_SLP_CUR_TH; Ricky
        ret = pmic_config_interface(0xCC2,0x14,0xFF,0); // [7:0]: FG_SLP_TIME; Ricky
        ret = pmic_config_interface(0xCC4,0xFF,0xFF,8); // [15:8]: FG_DET_TIME; Ricky
        ret = pmic_config_interface(0xCEE,0x1,0x1,1); // [1:1]: RG_AUDGLB_PWRDN_VA28; Eugene 7/8 Disable audio globe bias
        ret = pmic_config_interface(0xE12,0x0,0x1,15); // [15:15]: AUXADC_CK_AON; ZF
        ret = pmic_config_interface(0xE9C,0x2,0x3,0); // [1:0]: RG_ADC_TRIM_CH7_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x1,0x3,2); // [3:2]: RG_ADC_TRIM_CH6_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x1,0x3,4); // [5:4]: RG_ADC_TRIM_CH5_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x1,0x3,6); // [7:6]: RG_ADC_TRIM_CH4_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x3,0x3,8); // [9:8]: RG_ADC_TRIM_CH3_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x1,0x3,10); // [11:10]: RG_ADC_TRIM_CH2_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xE9C,0x0,0x3,14); // [15:14]: RG_ADC_TRIM_CH0_SEL; Ricky 7/8 ADC trim
        ret = pmic_config_interface(0xEBE,0x1,0x1,15); // [15:15]: RG_VREF18_ENB_MD; KH 7/8 For K2 no GPS co-clock
        
        #if defined(MTK_PUMP_EXPRESS_PLUS_SUPPORT)
        ret = pmic_config_interface(0xEF4,0xF,0xF,4); // [7:4]: RG_VCDT_HV_VTH; Tim:VCDT_HV_th=10.5V
        #else
        ret = pmic_config_interface(0xEF4,0xB,0xF,4); // [7:4]: RG_VCDT_HV_VTH; Tim:VCDT_HV_th=7V
        #endif
        
        ret = pmic_config_interface(0xEFE,0x4,0xF,1); // [4:1]: RG_VBAT_OV_VTH; Tim:for 4.35 battery
        ret = pmic_config_interface(0xF0C,0x3,0xF,0); // [3:0]: RG_CHRWDT_TD; Tim:WDT=32s
        ret = pmic_config_interface(0xF16,0x2,0x1F,0); // [4:0]: RG_LBAT_INT_VTH; Ricky: E1 only
        ret = pmic_config_interface(0xF1A,0x1,0x1,1); // [1:1]: RG_BC11_RST; Tim:Disable BC1.1 timer
        ret = pmic_config_interface(0xF1E,0x0,0x7,4); // [6:4]: RG_CSDAC_STP_DEC; Tim:Reduce ICHG current ripple (align 6323)
        ret = pmic_config_interface(0xF24,0x1,0x1,2); // [2:2]: RG_CSDAC_MODE; Tim:Align 6323
        ret = pmic_config_interface(0xF24,0x1,0x1,6); // [6:6]: RG_HWCV_EN; Tim:Align 6323
        ret = pmic_config_interface(0xF24,0x1,0x1,7); // [7:7]: RG_ULC_DET_EN; Tim:Align 6323       
        ret = pmic_config_interface(0x00E,0x3,0x3,0); // [1:0]: DVFS11_PG_ENB, DVFS12_PG_ENB; 8/11 ShangYing, disable DVFS11_DVFS12 PGOOD detection.       
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[Kernel_PMIC_INIT_SETTING_V1] Unknown PMIC Chip (0x%x)\n",mt6325_chip_version);
    }
    
    #if 1
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x1E, upmu_get_reg_value(0x1E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x4, upmu_get_reg_value(0x4));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xA, upmu_get_reg_value(0xA));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xC, upmu_get_reg_value(0xC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xE, upmu_get_reg_value(0xE));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x10, upmu_get_reg_value(0x10));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x16, upmu_get_reg_value(0x16));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x2E, upmu_get_reg_value(0x2E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x204, upmu_get_reg_value(0x204));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x222, upmu_get_reg_value(0x222));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x224, upmu_get_reg_value(0x224));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x23E, upmu_get_reg_value(0x23E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x244, upmu_get_reg_value(0x244));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x250, upmu_get_reg_value(0x250));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x268, upmu_get_reg_value(0x268));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x2BC, upmu_get_reg_value(0x2BC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x42C, upmu_get_reg_value(0x42C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x43A, upmu_get_reg_value(0x43A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x43C, upmu_get_reg_value(0x43C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x444, upmu_get_reg_value(0x444));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x446, upmu_get_reg_value(0x446));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x462, upmu_get_reg_value(0x462));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x464, upmu_get_reg_value(0x464));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x466, upmu_get_reg_value(0x466));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x468, upmu_get_reg_value(0x468));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x46A, upmu_get_reg_value(0x46A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x470, upmu_get_reg_value(0x470));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x472, upmu_get_reg_value(0x472));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x474, upmu_get_reg_value(0x474));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x484, upmu_get_reg_value(0x484));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x486, upmu_get_reg_value(0x486));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x48E, upmu_get_reg_value(0x48E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x490, upmu_get_reg_value(0x490));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x4B0, upmu_get_reg_value(0x4B0));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x4B6, upmu_get_reg_value(0x4B6));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x4BC, upmu_get_reg_value(0x4BC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x4C6, upmu_get_reg_value(0x4C6));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x502, upmu_get_reg_value(0x502));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x538, upmu_get_reg_value(0x538));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x548, upmu_get_reg_value(0x548));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x54A, upmu_get_reg_value(0x54A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x614, upmu_get_reg_value(0x614));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x634, upmu_get_reg_value(0x634));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x63A, upmu_get_reg_value(0x63A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x640, upmu_get_reg_value(0x640));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x65A, upmu_get_reg_value(0x65A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x660, upmu_get_reg_value(0x660));        
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x666, upmu_get_reg_value(0x666));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x670, upmu_get_reg_value(0x670));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x69A, upmu_get_reg_value(0x69A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x6B0, upmu_get_reg_value(0x6B0));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0x6CA, upmu_get_reg_value(0x6CA));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xA00, upmu_get_reg_value(0xA00));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xA04, upmu_get_reg_value(0xA04));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xA06, upmu_get_reg_value(0xA06));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xA08, upmu_get_reg_value(0xA08));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xA12, upmu_get_reg_value(0xA12));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xA18, upmu_get_reg_value(0xA18));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xA1A, upmu_get_reg_value(0xA1A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xA20, upmu_get_reg_value(0xA20));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xA38, upmu_get_reg_value(0xA38));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xA48, upmu_get_reg_value(0xA48));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xA4A, upmu_get_reg_value(0xA4A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xA60, upmu_get_reg_value(0xA60));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xA64, upmu_get_reg_value(0xA64));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xC14, upmu_get_reg_value(0xC14));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xCBC, upmu_get_reg_value(0xCBC));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xCC0, upmu_get_reg_value(0xCC0));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xCC2, upmu_get_reg_value(0xCC2));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xCC4, upmu_get_reg_value(0xCC4));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xCEE, upmu_get_reg_value(0xCEE));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xE9C, upmu_get_reg_value(0xE9C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xE12, upmu_get_reg_value(0xE12));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xEBE, upmu_get_reg_value(0xEBE));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xEF4, upmu_get_reg_value(0xEF4));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xEFE, upmu_get_reg_value(0xEFE));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xF0C, upmu_get_reg_value(0xF0C));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xF16, upmu_get_reg_value(0xF16));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xF1A, upmu_get_reg_value(0xF1A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xF1E, upmu_get_reg_value(0xF1E));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[mt6325] [0x%x]=0x%x\n", 0xF24, upmu_get_reg_value(0xF24));        
    #endif
    //--------------------------------------------------------
}

void PMIC_CUSTOM_SETTING_V1(void)
{
    #if defined(CONFIG_MTK_FPGA)
    #else    
    pmu_drv_tool_customization_init(); //DCT
    #endif
}

void pmic_setting_depends_rtc(void)
{
}

//==============================================================================
// FTM 
//==============================================================================
#define PMIC_DEVNAME "pmic_ftm"
#define Get_IS_EXT_BUCK_EXIST _IOW('k', 20, int)
#define Get_IS_EXT_VBAT_BOOST_EXIST _IOW('k', 21, int)
#define Get_IS_EXT_SWCHR_EXIST _IOW('k', 22, int)

static struct class *pmic_class = NULL;
static struct cdev *pmic_cdev;
static int pmic_major = 0;
static dev_t pmic_devno;

static long pmic_ftm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int *user_data_addr;
    int ret = 0;
	int adc_in_data[2] = {1,1};
	int adc_out_data[2] = {1,1};

    switch(cmd)
    {
        //#if defined(FTM_EXT_BUCK_CHECK)
            case Get_IS_EXT_BUCK_EXIST:
                user_data_addr = (int *)arg;
                ret = copy_from_user(adc_in_data, user_data_addr, 8);
                adc_out_data[0] = is_ext_buck_exist();
                ret = copy_to_user(user_data_addr, adc_out_data, 8); 
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_ftm_ioctl] Get_IS_EXT_BUCK_EXIST:%d\n", adc_out_data[0]);
            break;
        //#endif

        //#if defined(FTM_EXT_VBAT_BOOST_CHECK)
            case Get_IS_EXT_VBAT_BOOST_EXIST:
                user_data_addr = (int *)arg;
                ret = copy_from_user(adc_in_data, user_data_addr, 8);
                adc_out_data[0] = is_ext_vbat_boost_exist();
                ret = copy_to_user(user_data_addr, adc_out_data, 8); 
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_ftm_ioctl] Get_IS_EXT_VBAT_BOOST_EXIST:%d\n", adc_out_data[0]);
            break;
        //#endif
        
        //#if defined(FEATURE_FTM_SWCHR_HW_DETECT)
            case Get_IS_EXT_SWCHR_EXIST:
                user_data_addr = (int *)arg;
                ret = copy_from_user(adc_in_data, user_data_addr, 8);
                adc_out_data[0] = is_ext_swchr_exist();
                ret = copy_to_user(user_data_addr, adc_out_data, 8); 
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_ftm_ioctl] Get_IS_EXT_SWCHR_EXIST:%d\n", adc_out_data[0]);
            break;
        //#endif
        
        default:
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_ftm_ioctl] Error ID\n");
            break;
    }
    
    return 0;
}

static int pmic_ftm_open(struct inode *inode, struct file *file)
{ 
   return 0;
}

static int pmic_ftm_release(struct inode *inode, struct file *file)
{
    return 0;
}


static struct file_operations pmic_ftm_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = pmic_ftm_ioctl,
    .open           = pmic_ftm_open,
    .release        = pmic_ftm_release,    
};

void pmic_ftm_init(void)
{
    struct class_device *class_dev = NULL;
    int ret=0;
    
    ret = alloc_chrdev_region(&pmic_devno, 0, 1, PMIC_DEVNAME);
    if (ret) 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_ftm_init] Error: Can't Get Major number for pmic_ftm\n");
    
    pmic_cdev = cdev_alloc();
    pmic_cdev->owner = THIS_MODULE;
    pmic_cdev->ops = &pmic_ftm_fops;

    ret = cdev_add(pmic_cdev, pmic_devno, 1);
    if(ret)
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_ftm_init] Error: cdev_add\n");
    
    pmic_major = MAJOR(pmic_devno);
    pmic_class = class_create(THIS_MODULE, PMIC_DEVNAME);
    
    class_dev = (struct class_device *)device_create(pmic_class, 
                                                   NULL, 
                                                   pmic_devno, 
                                                   NULL, 
                                                   PMIC_DEVNAME);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_ftm_init] Done\n");
}

//==============================================================================
// system function 
//==============================================================================
static int pmic_mt_probe(struct platform_device *dev)
{
    int ret_device_file = 0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "******** MT pmic driver probe!! ********\n" );
    
    //get PMIC CID
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "MT6325 PMIC CID=0x%x.\n", get_mt6325_pmic_chip_version());

    //pmic initial setting
    PMIC_INIT_SETTING_V1();
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_INIT_SETTING_V1] Done\n");
    PMIC_CUSTOM_SETTING_V1();
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_CUSTOM_SETTING_V1] Done\n");

#if 1
    if(get_mt6325_pmic_chip_version() < PMIC6325_E6_CID_CODE)
    {
        //0815_ShangYing
        pmic_config_interface(0x204,0x0,0x1,6); // [6:6]: PMIC OSC not off in sleep mode
        pmic_config_interface(0x538,0x0,0x1,8); // [8:8]: VDRAM not in sleep mode
        pmic_config_interface(0x64A,0x0,0x1,8); // [8:8]: VCORE1 maintain 1.0v in sleep mode
        pmic_config_interface(0x670,0x0,0x1,8); // [8:8]: VCORE2 maintain 1.0v in sleep mode
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[TMP.] [0x%x]=0x%x\n", 0x204, upmu_get_reg_value(0x204));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[TMP.] [0x%x]=0x%x\n", 0x538, upmu_get_reg_value(0x538));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[TMP.] [0x%x]=0x%x\n", 0x64A, upmu_get_reg_value(0x64A));
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[TMP.] [0x%x]=0x%x\n", 0x670, upmu_get_reg_value(0x670));
    }
#endif

#if defined(CONFIG_MTK_FPGA)
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_EINT_SETTING] disable when CONFIG_MTK_FPGA\n");
#else
    //PMIC Interrupt Service
    pmic_6325_thread_handle = kthread_create(pmic_thread_kthread_mt6325, (void *) NULL, "pmic_6325_thread");
    if (IS_ERR(pmic_6325_thread_handle)) 
    {
        pmic_6325_thread_handle = NULL;
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_thread_kthread_mt6325] creation fails\n");        
    }
    else
    {
        wake_up_process(pmic_6325_thread_handle);
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[pmic_thread_kthread_mt6325] kthread_create Done\n");
    } 

    PMIC_EINT_SETTING();
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_EINT_SETTING] Done\n");
#endif
    mt6325_upmu_set_rg_pwrkey_int_sel(1);
    mt6325_upmu_set_rg_homekey_int_sel(1);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC KEY] Reg[0x%x]=0x%x\n", MT6325_INT_MISC_CON, upmu_get_reg_value(MT6325_INT_MISC_CON));

#ifdef LOW_BATTERY_PROTECT
    low_battery_protect_init();
#else
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC] no define LOW_BATTERY_PROTECT\n" );
#endif

#ifdef BATTERY_OC_PROTECT
    battery_oc_protect_init();
#else
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC] no define BATTERY_OC_PROTECT\n" );
#endif

#ifdef BATTERY_PERCENT_PROTECT
    bat_percent_notify_init();
#else
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC] no define BATTERY_PERCENT_PROTECT\n" );
#endif

    dump_ldo_status_read_debug();
    pmic_debug_init();
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC] pmic_debug_init : done.\n" );

    pmic_ftm_init();

#if 1
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_pmic_access);

    //EM BUCK/LDO Status
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDVFS11_STATUS);  
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDVFS12_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDRAM_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VRF18_0_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VGPU_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VCORE1_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VCORE2_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VIO18_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VPA_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_11_VDVFS11_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_11_VDVFS12_STATUS);

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VBIF28_STATUS);	
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VTCXO0_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VTCXO1_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VAUD28_STATUS);         
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VAUXA28_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMA_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCN28_STATUS);   
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCN33_STATUS); 
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VRF18_1_STATUS);  
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VUSB33_STATUS);  
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMCH_STATUS);        
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMC_STATUS); 
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VEMC33_STATUS);    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VIO28_STATUS);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAM_AF_STATUS);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP1_STATUS);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VEFUSE_STATUS);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VSIM1_STATUS);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VSIM2_STATUS);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMIPI_STATUS);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCN18_STATUS);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP2_STATUS);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMD_STATUS);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAM_IO_STATUS);        
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VSRAM_DVFS1_STATUS);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP3_STATUS);           
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VBIASN_STATUS);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VRTC_STATUS);

    //EM BUCK/LDO Voltage
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDVFS11_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDVFS12_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VGPU_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VCORE1_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VCORE2_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VIO18_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDRAM_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_11_VDVFS11_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_11_VDVFS12_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VRF18_0_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VPA_VOLTAGE);

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VTCXO0_VOLTAGE);  
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VTCXO1_VOLTAGE);  
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VAUD28_VOLTAGE);  
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VAUXA28_VOLTAGE);  
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VBIF28_VOLTAGE);  
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCN28_VOLTAGE);  
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VUSB33_VOLTAGE);  
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VIO28_VOLTAGE);  
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VRTC_VOLTAGE);  
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMA_VOLTAGE);         
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCN33_VOLTAGE);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VRF18_1_VOLTAGE);         
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMCH_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMC_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VEMC33_VOLTAGE);          
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAM_AF_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP1_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VEFUSE_VOLTAGE);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VSIM1_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VSIM2_VOLTAGE);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMIPI_VOLTAGE);      
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCN18_VOLTAGE);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP2_VOLTAGE);    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMD_VOLTAGE);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAM_IO_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VSRAM_DVFS1_VOLTAGE);       
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP3_VOLTAGE);     
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VBIASN_VOLTAGE);      
    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_MT6325_BUCK_CURRENT_METER);
    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_pmic_dvt);

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_low_battery_protect_ut);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_low_battery_protect_stop);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_low_battery_protect_level);

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_oc_protect_ut);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_oc_protect_stop);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_oc_protect_level);

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_percent_protect_ut);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_percent_protect_stop);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_percent_protect_level);

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC] device_create_file for EM : done.\n" );
#endif

    return 0;
}

static int pmic_mt_remove(struct platform_device *dev)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "******** MT pmic driver remove!! ********\n" );

    return 0;
}

static void pmic_mt_shutdown(struct platform_device *dev)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "******** MT pmic driver shutdown!! ********\n" );
}

static int pmic_mt_suspend(struct platform_device *dev, pm_message_t state)
{
	U32 adc_busy;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "******** MT pmic driver suspend!! ********\n" );
    
#ifdef LOW_BATTERY_PROTECT
    lbat_min_en_setting(0);
    lbat_max_en_setting(0);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                MT6325_AUXADC_CON5, upmu_get_reg_value(MT6325_AUXADC_CON5),
                MT6325_AUXADC_CON6, upmu_get_reg_value(MT6325_AUXADC_CON6),
                MT6325_INT_CON0, upmu_get_reg_value(MT6325_INT_CON0)
                );
#endif

#ifdef BATTERY_OC_PROTECT
    bat_oc_h_en_setting(0);
    bat_oc_l_en_setting(0);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                MT6325_FGADC_CON23, upmu_get_reg_value(MT6325_FGADC_CON23),
                MT6325_FGADC_CON24, upmu_get_reg_value(MT6325_FGADC_CON24),
                MT6325_INT_CON2, upmu_get_reg_value(MT6325_INT_CON2)
                );
#endif

    mt6325_upmu_set_rg_auxadc_32k_ck_pdn(0x1);      
    
    //if (Enable_BATDRV_LOG==2) {
    //    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
    //            MT6325_TOP_CKPDN_CON0, upmu_get_reg_value(MT6325_TOP_CKPDN_CON0),
    //            MT6332_TOP_CKPDN_CON0, upmu_get_reg_value(MT6332_TOP_CKPDN_CON0)
    //            );
    //}
	mt6325_upmu_set_rg_vref18_enb(1);
	pmic_read_interface_nolock(MT6325_AUXADC_ADC19, &adc_busy, MT6325_PMIC_RG_ADC_BUSY_MASK, MT6325_PMIC_RG_ADC_BUSY_SHIFT); 
	if (adc_busy==0) {
		pmic_config_interface_nolock(MT6325_AUXADC_CON19, 0x1, MT6325_PMIC_RG_ADC_DECI_GDLY_MASK, MT6325_PMIC_RG_ADC_DECI_GDLY_SHIFT);
	}
    return 0;
}

static int pmic_mt_resume(struct platform_device *dev)
{
	U32 adc_busy;
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "******** MT pmic driver resume!! ********\n" );

#ifdef LOW_BATTERY_PROTECT
    lbat_min_en_setting(0);
    lbat_max_en_setting(0);
    mdelay(1);

    if(g_low_battery_level==1)
    {
        lbat_min_en_setting(1);
        lbat_max_en_setting(1);
    }
    else if(g_low_battery_level==2)
    {
        //lbat_min_en_setting(0);
        lbat_max_en_setting(1);
    }
    else //0
    {
        lbat_min_en_setting(1);
        //lbat_max_en_setting(0);
    }
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                MT6325_AUXADC_CON5, upmu_get_reg_value(MT6325_AUXADC_CON5),
                MT6325_AUXADC_CON6, upmu_get_reg_value(MT6325_AUXADC_CON6),
                MT6325_INT_CON0, upmu_get_reg_value(MT6325_INT_CON0)
                );
#endif

#ifdef BATTERY_OC_PROTECT
    bat_oc_h_en_setting(0);
    bat_oc_l_en_setting(0);
    mdelay(1);

    if(g_battery_oc_level==1)
        bat_oc_h_en_setting(1);
    else
        bat_oc_l_en_setting(1);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                MT6325_FGADC_CON23, upmu_get_reg_value(MT6325_FGADC_CON23),
                MT6325_FGADC_CON24, upmu_get_reg_value(MT6325_FGADC_CON24),
                MT6325_INT_CON2, upmu_get_reg_value(MT6325_INT_CON2)
                );
#endif

    mt6325_upmu_set_rg_auxadc_32k_ck_pdn(0x0);  
	mt6325_upmu_set_rg_vref18_enb(0);
	pmic_read_interface_nolock(MT6325_AUXADC_ADC19, &adc_busy, MT6325_PMIC_RG_ADC_BUSY_MASK, MT6325_PMIC_RG_ADC_BUSY_SHIFT); 
	if (adc_busy==0) {
		pmic_config_interface_nolock(MT6325_AUXADC_CON19, 0x0, MT6325_PMIC_RG_ADC_DECI_GDLY_MASK, MT6325_PMIC_RG_ADC_DECI_GDLY_SHIFT);
	}
    //if (Enable_BATDRV_LOG==2) {
    //    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
    //            MT6325_TOP_CKPDN_CON0, upmu_get_reg_value(MT6325_TOP_CKPDN_CON0),
    //            MT6332_TOP_CKPDN_CON0, upmu_get_reg_value(MT6332_TOP_CKPDN_CON0)
    //            );
    //}

    return 0;
}

struct platform_device pmic_mt_device = {
    .name   = "mt-pmic",
    .id        = -1,
};

static struct platform_driver pmic_mt_driver = {
    .probe        = pmic_mt_probe,
    .remove       = pmic_mt_remove,
    .shutdown     = pmic_mt_shutdown,
    //#ifdef CONFIG_PM
    .suspend      = pmic_mt_suspend,
    .resume       = pmic_mt_resume,
    //#endif
    .driver       = {
        .name = "mt-pmic",
    },
};

#if 0//#ifdef CONFIG_HAS_EARLYSUSPEND
static void pmic_early_suspend(struct early_suspend *h)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "******** MT pmic driver early suspend!! ********\n" );
    mt6325_upmu_set_rg_auxadc_32k_ck_pdn(0x1);  
    
    //if (Enable_BATDRV_LOG==2) {
    //    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
    //            MT6325_TOP_CKPDN_CON0, upmu_get_reg_value(MT6325_TOP_CKPDN_CON0),
    //            MT6332_TOP_CKPDN_CON0, upmu_get_reg_value(MT6332_TOP_CKPDN_CON0)
    //            );
    //}
	mt6325_upmu_set_rg_clksq_en_aux_ap(0);
}

static void pmic_early_resume(struct early_suspend *h)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "******** MT pmic driver early resume!! ********\n" );
    mt6325_upmu_set_rg_auxadc_32k_ck_pdn(0x0);  
    
    //if (Enable_BATDRV_LOG==2) {
    //    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
    //            MT6325_TOP_CKPDN_CON0, upmu_get_reg_value(MT6325_TOP_CKPDN_CON0),
    //            MT6332_TOP_CKPDN_CON0, upmu_get_reg_value(MT6332_TOP_CKPDN_CON0)
    //            );
    //}
	mt6325_upmu_set_rg_clksq_en_aux_ap(1);
}

static struct early_suspend pmic_early_suspend_desc = {
    .level      = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
    .suspend    = pmic_early_suspend,
    .resume     = pmic_early_resume,
};
#endif


//==============================================================================
// PMIC mudule init/exit
//==============================================================================
static int __init pmic_mt_init(void)
{
    int ret;
    
    wake_lock_init(&pmicThread_lock_mt6325, WAKE_LOCK_SUSPEND, "pmicThread_lock_mt6325 wakelock");
    wake_lock_init(&bat_percent_notify_lock, WAKE_LOCK_SUSPEND,"bat_percent_notify_lock wakelock");
            
    // PMIC device driver register
    ret = platform_device_register(&pmic_mt_device);
    if (ret) {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[pmic_mt_init] Unable to device register(%d)\n", ret);
        return ret;
    }
    ret = platform_driver_register(&pmic_mt_driver);
    if (ret) {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[pmic_mt_init] Unable to register driver (%d)\n", ret);
        return ret;
    }

#if 0//#ifdef CONFIG_HAS_EARLYSUSPEND
    //register_early_suspend(&pmic_early_suspend_desc);
#endif

    pmic_auxadc_init();

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[pmic_mt_init] Initialization : DONE !!\n");

    return 0;
}

static void __exit pmic_mt_exit (void)
{
}

fs_initcall(pmic_mt_init);

//module_init(pmic_mt_init);
module_exit(pmic_mt_exit);

MODULE_AUTHOR("James Lo");
MODULE_DESCRIPTION("MT PMIC Device Driver");
MODULE_LICENSE("GPL");


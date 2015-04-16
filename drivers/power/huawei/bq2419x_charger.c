/*
 * drivers/power/bq2419x_charger.c
 *
 * BQ24190/1/2/3/4 charging driver
 *
 * Copyright (C) 2012-2015 HUAWEI, Inc.
 * Author: HUAWEI, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/wakelock.h>
#include <linux/usb/otg.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <bq2419x_charger.h>
#include <bq27510_battery.h>
#include <bq_bci_battery.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/huawei/usb/hisi_usb.h>
#include <linux/hw_log.h>
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif
#include <hisi_coul_drv.h>
#include <linux/raid/pq.h>
#include <linux/huawei/dsm_pub.h>

#define HWLOG_TAG bq2419x_charger
#define POWER_SUPPLY_OVP_MONITOR_START_TIME	(200) //start time to monitor the power supply ovp

static struct dsm_dev dsm_charger = {
	.name = "dsm_charger",
	.fops = NULL,
	.buff_size = 1024,
};
static struct dsm_client *charger_dclient = NULL;

HWLOG_REGIST();
struct charge_params {
    unsigned long       currentmA;
    unsigned long       voltagemV;
    unsigned long       term_currentmA;
    unsigned long       enable_iterm;
    bool                enable;
};

struct bq2419x_device_info {
    struct device        *dev;
    struct i2c_client    *client;
    struct charge_params  params;
    struct delayed_work   bq2419x_charger_work;
    struct delayed_work   bq2419x_usb_otg_work;
    struct work_struct    usb_work;
    struct delayed_work   otg_int_work;
    unsigned int          otg_int_work_cnt;

    unsigned int      wakelock_enabled;
    unsigned short    input_source_reg00;
    unsigned short    power_on_config_reg01;
    unsigned short    charge_current_reg02;
    unsigned short    prechrg_term_current_reg03;
    unsigned short    charge_voltage_reg04;
    unsigned short    term_timer_reg05;
    unsigned short    thermal_regulation_reg06;
    unsigned short    misc_operation_reg07;
    unsigned short    system_status_reg08;
    unsigned short    charger_fault_reg09;
    unsigned short    bqchip_version;

    unsigned int      max_currentmA;
    unsigned int      max_voltagemV;
    unsigned int      max_cin_currentmA;

    unsigned int    cin_dpmmV;
    unsigned int    cin_limit;
    unsigned int    chrg_config;
    unsigned int    sys_minmV;
    unsigned int    currentmA;
    unsigned int    prechrg_currentmA;
    unsigned int    term_currentmA;
    unsigned int    voltagemV;
    unsigned int    watchdog_timer;
    unsigned int    chrg_timer;
    unsigned int    bat_compohm;
    unsigned int    comp_vclampmV;

    bool    hz_mode;
    bool    boost_lim;
    bool    enable_low_chg;
    bool    cfg_params;
    bool    enable_iterm;
    bool    enable_timer;
    bool    enable_batfet;
    bool    cd_active;
    bool    factory_flag;
    bool    calling_limit;
    bool    battery_present;
    bool    enable_dpdm;

    int     charger_source;
    int     timer_fault;
    unsigned int    battery_temp_status;
    unsigned long           event;
    struct notifier_block   nb;

    int     gpio_cd;
    int     gpio_int;
    int     irq_int;
    int     battery_voltage;
    int     temperature_cold;
    int     temperature_cool;
    int     temperature_warm;
    int     temperature_hot;
    bool    not_limit_chrg_flag;
    bool    not_stop_chrg_flag;
    bool    battery_full;
    int     temperature_5;
    int     temperature_10;
    u32    charge_full_count;

    /* these parameters are for charging between 0C-5C & 5C-10C, 1-0.1*capacity...
       charge_in_temp_5 means the parameter for charging between 0C-5C. */
    unsigned int design_capacity;
    unsigned int charge_in_temp_5;
    unsigned int charge_in_temp_10;

    int  japan_charger;
    int  is_two_stage_charger;
    int  two_stage_charger_status;
    int  first_stage_voltage;
    int  second_stage_voltage;
    int  is_disable_cool_temperature_charger;
    int  high_temp_para;
    int  bat_temp_ctl_type;
};

static struct wake_lock chrg_lock;
static struct wake_lock stop_chrg_lock;
u32 wakeup_timer_seconds;//h00121290 remove extern temply

/* backup the last cin_limit */
static struct device_node* np;
static bool udp_charge = false;
static unsigned int irq_int_active;

static unsigned int mhl_limit_cin_flag = 1;
static unsigned int input_current_iin;
static unsigned int input_current_ichg;
static unsigned int iin_temp = 0;
static unsigned int ichg_temp = 0;
static unsigned int iin_limit_temp = 1;
static unsigned int ichg_limit_temp = 1;

/**just for test**/
/*************/
enum{
    BATTERY_HEALTH_TEMPERATURE_NORMAL = 0,
    BATTERY_HEALTH_TEMPERATURE_OVERLOW,
    BATTERY_HEALTH_TEMPERATURE_LOW,
    BATTERY_HEALTH_TEMPERATURE_NORMAL_HIGH,
    BATTERY_HEALTH_TEMPERATURE_HIGH,
    BATTERY_HEALTH_TEMPERATURE_OVERHIGH,
    BATTERY_HEALTH_TEMPERATURE_10,
    BATTERY_HEALTH_TEMPERATURE_5,
    BATTERY_HEALTH_TEMPERATURE_HIGH_CP1,
    BATTERY_HEALTH_TEMPERATURE_HIGH_CP2,
    BATTERY_HEALTH_TEMPERATURE_HIGH_CP3,
    BATTERY_HEALTH_TEMPERATURE_HIGH_CP4,
    BATTERY_HEALTH_TEMPERATURE_HIGH_CP5,
};
/*define bat temp area*/
enum{
    /*for general product*/
    BAT_TEMP_FROM_10_TO_NEXT_POINT = 0,
    BAT_TEMP_LESS_THAN_MINUS10,
    BAT_TEMP_FROM_MINUS10_TO_0,
    BAT_TEMP_FROM_42_TO_45,
    BAT_TEMP_FROM_45_TO_50,
    BAT_TEMP_OVER_50,
    BAT_TEMP_FROM_5_TO_10,
    BAT_TEMP_FROM_0_TO_5,
    /*for japanese market*/
    BAT_TEMP_FROM_CP1_TO_CP2,
    BAT_TEMP_FROM_CP2_TO_CP3,
    BAT_TEMP_FROM_CP3_TO_CP4,
    BAT_TEMP_FROM_CP4_TO_CP5,
    BAT_TEMP_FROM_CP5_TO_CP6,
};
enum{
    NORMAL_TEMP_CONFIG = 0,  // (BATTERY_HEALTH_TEMPERATURE_NORMAL)
    NORMAL_HIGH_TEMP_CONFIG, // (BATTERY_HEALTH_TEMPERATURE_NORMAL_HIGH)
    HIGH_TEMP_CONFIG,        // (BATTERY_HEALTH_TEMPERATURE_HIGH)
    TEMP_CONFIG_10,
    TEMP_CONFIG_5,
    NORMAL_SECOND_STAGE,//for two charging stage
    NORMAL_HIGH_TEMP_CONFIG_CP1,
    NORMAL_HIGH_TEMP_CONFIG_CP2,
    NORMAL_HIGH_TEMP_CONFIG_CP3,
    NORMAL_HIGH_TEMP_CONFIG_CP4,
};

//tempture checkpoint
struct bq2419x_high_temp_cp{
    int cp1;
    int cp2;
    int cp3;
    int cp4;
    int cp5;
    int cp6;
};

enum{
    HIGH_TEMP_CP_U9701L = 0,
    HIGH_TEMP_CP_U9700L = 1,
};

//struct bq2419x_high_temp_cp japan_temp_cp[] ={
//    {40, 42, 43, 45, 53, 55}, //HIGH_TEMP_CP_U9701L
//    {37, 39, 40, 42, 43, 45},//HIGH_TEMP_CP_U9700L
//    {39, 41, 42, 45, 53, 55},//HIGH_TEMP_CP_U9700GVC China area
//    {0},
//};
#define MIN_INT                (1 << 31)
#define MAX_INT                (~(1 << 31))
#define TEMP_CTL_POINTS_NUM    (12)
#define TEMP_CTL_TYPE_NUM      (4)
#define TEMP_AREA_NUM          (13)
int bat_temp_ctl_points[TEMP_CTL_TYPE_NUM][TEMP_CTL_POINTS_NUM] =
{
    {MIN_INT,    -10,    0,   5,    10,    42,    45,    50, MAX_INT, MAX_INT, MAX_INT, MAX_INT},// default, for general product
    {MIN_INT,    -10,    0,   5,    10,    40,    42,    43,   45,      53,      55,    MAX_INT},// for japan product U9701L
    {MIN_INT,    -10,    0,   5,    10,    37,    39,    40,   42,      43,      45,    MAX_INT},// for japan product U9700L
    {MIN_INT,    -10,    0,   5,    10,    39,    41,    42,   45,      53,      55,    MAX_INT},// for japan product U9700GVC China area
};

/*for the meaning of these numbers, see the definition of temp area*/
int bat_temp_area[TEMP_CTL_TYPE_NUM][TEMP_CTL_POINTS_NUM - 1] =
{
    {1, 2, 7, 6, 0, 3, 4, 5,  0,  0,  0},
    {1, 2, 7, 6, 0, 8, 9, 10, 11, 12, 5},
    {1, 2, 7, 6, 0, 8, 9, 10, 11, 12, 5},
    {1, 2, 7, 6, 0, 8, 9, 10, 11, 12, 5},
};
static void bq2419x_get_boardid_japan_charge_parameter(struct bq2419x_device_info *di)
{
    int ret;
    ret = of_property_read_u32(np,"japan_charger",&di->japan_charger);
    ret |= of_property_read_u32(np,"is_two_stage_charger",&di->is_two_stage_charger);
    ret |= of_property_read_u32(np,"first_stage_voltage",&di->first_stage_voltage);
    ret |= of_property_read_u32(np,"second_stage_voltage",&di->second_stage_voltage);
    ret |= of_property_read_u32(np,"is_disable_cool_temperature_charger",&di->is_disable_cool_temperature_charger);
    ret |= of_property_read_u32(np,"high_temperature_checkpoint",&di->high_temp_para);
    if(ret)
        hwlog_err(" bq2419x get japan charge parameter from dts fail \n");
    if(!di->japan_charger)
	di->bat_temp_ctl_type = 0;
    else
    {
	di->bat_temp_ctl_type = di->high_temp_para + 1;
    }
}
static int bq2419x_get_boardid_charge_parameter(struct bq2419x_device_info *di)
{
    bool ret = 0;
    struct battery_charge_param_s charge_param = {0};
    ret = hisi_battery_charge_param(&charge_param);
    if (ret)
    {
        di->max_voltagemV = charge_param.max_voltagemV;
        di->max_currentmA = charge_param.max_currentmA;
        di->max_cin_currentmA = charge_param.max_cin_currentmA;
	return true;
    }
    else
        return false;
}

static int bq2419x_get_max_charge_voltage(struct bq2419x_device_info *di)
{
    bool ret = 0;

    struct battery_charge_param_s charge_param = {0};
    ret = hisi_battery_charge_param(&charge_param);
    if (ret)
    {
        di->max_voltagemV = charge_param.max_voltagemV;
	return true;
    }
    else
    {
        di->max_voltagemV = 4208;
        hwlog_err("boardid does not set limited charge voltage\n");
        return false;
    }
}

static int bq2419x_write_block(struct bq2419x_device_info *di, u8 *value,
                               u8 reg, unsigned num_bytes)
{
    struct i2c_msg msg[1];
    int ret = 0;

    *value = reg;

    msg[0].addr = di->client->addr;
    msg[0].flags = 0;
    msg[0].buf = value;
    msg[0].len = num_bytes + 1;

   ret = i2c_transfer(di->client->adapter, msg, 1);

    /* i2c_transfer returns number of messages transferred */
    if (ret != 1)
    {
        hwlog_err("i2c_write failed to transfer all messages\n");
        if (ret < 0)
            return ret;
        else
            return -EIO;
    }
    else
    {
        return 0;
    }
}

static int bq2419x_read_block(struct bq2419x_device_info *di, u8 *value,
                            u8 reg, unsigned num_bytes)
{
    struct i2c_msg msg[2];
    u8 buf = 0;
    int ret = 0;

    buf = reg;

    msg[0].addr = di->client->addr;
    msg[0].flags = 0;
    msg[0].buf = &buf;
    msg[0].len = 1;

    msg[1].addr = di->client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].buf = value;
    msg[1].len = num_bytes;

    ret = i2c_transfer(di->client->adapter, msg, 2);

    /* i2c_transfer returns number of messages transferred */
    if (ret != 2)
    {
        hwlog_err("i2c_write failed to transfer all messages\n");
        if (ret < 0)
            return ret;
        else
            return -EIO;
    }
    else
    {
        return 0;
    }
}

static int bq2419x_write_byte(struct bq2419x_device_info *di, u8 value, u8 reg)
{
    /* 2 bytes offset 1 contains the data offset 0 is used by i2c_write */
    u8 temp_buffer[2] = { 0 };

    /* offset 1 contains the data */
    temp_buffer[1] = value;
    return bq2419x_write_block(di, temp_buffer, reg, 1);
}

static int bq2419x_read_byte(struct bq2419x_device_info *di, u8 *value, u8 reg)
{
    return bq2419x_read_block(di, value, reg, 1);
}

static void bq2419x_config_input_source_reg(struct bq2419x_device_info *di)
{
      unsigned int vindpm = 0;
      u8 Vdpm = 0;
      u8 Iin_limit = 0;

      vindpm = di->cin_dpmmV;

      if(vindpm < VINDPM_MIN_3880)
          vindpm = VINDPM_MIN_3880;
      else if (vindpm > VINDPM_MAX_5080)
          vindpm = VINDPM_MAX_5080;

      if (di->cin_limit <= IINLIM_100)
          Iin_limit = 0;
      else if (di->cin_limit > IINLIM_100 && di->cin_limit <= IINLIM_150)
          Iin_limit = 1;
      else if (di->cin_limit > IINLIM_150 && di->cin_limit <= IINLIM_500)
          Iin_limit = 2;
      else if (di->cin_limit > IINLIM_500 && di->cin_limit <= IINLIM_900)
          Iin_limit = 3;
      else if (di->cin_limit > IINLIM_900 && di->cin_limit <= IINLIM_1200)
          Iin_limit = 4;
      else if (di->cin_limit > IINLIM_1200 && di->cin_limit <= IINLIM_1500)
          Iin_limit = 5;
      else if (di->cin_limit > IINLIM_1500 && di->cin_limit <= IINLIM_2000)
          Iin_limit = 6;
      else if (di->cin_limit > IINLIM_2000 && di->cin_limit <= IINLIM_3000)
          Iin_limit = 7;
      else
          Iin_limit = 4;

      Vdpm = (vindpm -VINDPM_MIN_3880)/VINDPM_STEP_80;

      di->input_source_reg00 = (di->hz_mode << BQ2419x_EN_HIZ_SHIFT)
                        | (Vdpm << BQ2419x_VINDPM_SHIFT) |Iin_limit;

      bq2419x_write_byte(di, di->input_source_reg00, INPUT_SOURCE_REG00);
      return;
}

static void bq2419x_config_power_on_reg(struct bq2419x_device_info *di)
{
      unsigned int sys_min = 0;
      u8 Sysmin = 0;

      sys_min = di->sys_minmV;

      if(sys_min < SYS_MIN_MIN_3000)
          sys_min = SYS_MIN_MIN_3000;
      else if (sys_min > SYS_MIN_MAX_3700)
          sys_min = SYS_MIN_MAX_3700;

      Sysmin = (sys_min -SYS_MIN_MIN_3000)/SYS_MIN_STEP_100;

      di->power_on_config_reg01 = WATCHDOG_TIMER_RST
           | (di->chrg_config << BQ2419x_EN_CHARGER_SHIFT)
           | (Sysmin << BQ2419x_SYS_MIN_SHIFT) | di->boost_lim;

       bq2419x_write_byte(di, di->power_on_config_reg01, POWER_ON_CONFIG_REG01);
       return;
}

static void bq2419x_config_current_reg(struct bq2419x_device_info *di)
{
    unsigned int currentmA = 0;
    u8 Vichrg = 0;

    currentmA = di->currentmA;

    /* 1.If currentmA is below ICHG_512, we can set the ICHG to 5*currentmA and
         set the FORCE_20PCT in REG02 to make the true current 20% of the ICHG
       2.To slove the OCP BUG of BQ2419X, we need set the ICHG(lower than 1024mA)
         to 5*currentmA and set the FORCE_20PCT in REG02.*/
    if (currentmA < ICHG_1024) {
        currentmA = currentmA * 5;
        di->enable_low_chg = EN_FORCE_20PCT;
    } else {
        di->enable_low_chg = DIS_FORCE_20PCT;
    }
    if (currentmA < ICHG_512)
        currentmA = ICHG_512;
    else if(currentmA > ICHG_MAX)
        currentmA = ICHG_MAX;
    Vichrg = (currentmA - ICHG_512)/ICHG_STEP_64;

    di->charge_current_reg02 = (Vichrg << BQ2419x_ICHG_SHIFT) | di->enable_low_chg;

     bq2419x_write_byte(di, di->charge_current_reg02, CHARGE_CURRENT_REG02);
     return;
}

static void bq2419x_config_prechg_term_current_reg(struct bq2419x_device_info *di)
{
    unsigned int precurrentmA = 0;
    unsigned int term_currentmA = 0;
    u8 Prechg = 0;
    u8 Viterm = 0;

    precurrentmA = di->prechrg_currentmA;
    term_currentmA = di->term_currentmA;

    if(precurrentmA < IPRECHRG_MIN_128)
        precurrentmA = IPRECHRG_MIN_128;
    if(term_currentmA < ITERM_MIN_128)
        term_currentmA = ITERM_MIN_128;

    if((di->bqchip_version & BQ24192I)) {
        if(precurrentmA > IPRECHRG_640)
            precurrentmA = IPRECHRG_640;
     }

    if ((di->bqchip_version & (BQ24190|BQ24191|BQ24192))) {
        if (precurrentmA > IPRECHRG_MAX_2048)
            precurrentmA = IPRECHRG_MAX_2048;
    }

    if (term_currentmA > ITERM_MAX_2048)
        term_currentmA = ITERM_MAX_2048;

    Prechg = (precurrentmA - IPRECHRG_MIN_128)/IPRECHRG_STEP_128;
    Viterm = (term_currentmA-ITERM_MIN_128)/ITERM_STEP_128;

    di->prechrg_term_current_reg03 = (Prechg <<  BQ2419x_IPRECHRG_SHIFT| Viterm);
    bq2419x_write_byte(di, di->prechrg_term_current_reg03, PRECHARGE_TERM_CURRENT_REG03);
    return;
}

static void bq2419x_config_voltage_reg(struct bq2419x_device_info *di)
{
    unsigned int voltagemV = 0;
    u8 Voreg = 0;

    voltagemV = di->voltagemV;
    if (voltagemV < VCHARGE_MIN_3504)
        voltagemV = VCHARGE_MIN_3504;
    else if (voltagemV > VCHARGE_MAX_4400)
        voltagemV = VCHARGE_MAX_4400;

    if(udp_charge)
    {
        voltagemV = VCHARGE_4100;
    }
    Voreg = (voltagemV - VCHARGE_MIN_3504)/VCHARGE_STEP_16;

    di->charge_voltage_reg04 = (Voreg << BQ2419x_VCHARGE_SHIFT) | BATLOWV_3000 |VRECHG_100;
    bq2419x_write_byte(di, di->charge_voltage_reg04, CHARGE_VOLTAGE_REG04);
    return;
}

static void bq2419x_config_term_timer_reg(struct bq2419x_device_info *di)
{
    di->term_timer_reg05 = (di->enable_iterm << BQ2419x_EN_TERM_SHIFT)
        | di->watchdog_timer | (di->enable_timer << BQ2419x_EN_TIMER_SHIFT)
        | di->chrg_timer | JEITA_ISET;

    bq2419x_write_byte(di, di->term_timer_reg05, CHARGE_TERM_TIMER_REG05);
    return;
}

static void bq2419x_config_thernal_regulation_reg(struct bq2419x_device_info *di)
{
    unsigned int batcomp_ohm = 0;
    unsigned int vclampmV = 0;
    u8 Batcomp = 0;
    u8 Vclamp = 0;

    batcomp_ohm = di->bat_compohm;
    vclampmV = di->comp_vclampmV;

    if(batcomp_ohm > BAT_COMP_MAX_70)
        batcomp_ohm = BAT_COMP_MAX_70;

    if(vclampmV > VCLAMP_MAX_112)
        vclampmV = VCLAMP_MAX_112;

    Batcomp = batcomp_ohm/BAT_COMP_STEP_10;
    Vclamp = vclampmV/VCLAMP_STEP_16;

    di->thermal_regulation_reg06 = (Batcomp << BQ2419x_BAT_COMP_SHIFT)
                           |(Vclamp << BQ2419x_VCLAMP_SHIFT) |TREG_120;

    bq2419x_write_byte(di, di->thermal_regulation_reg06, THERMAL_REGUALTION_REG06);
    return;
}

static void bq2419x_config_misc_operation_reg(struct bq2419x_device_info *di)
{
    di->misc_operation_reg07 = (di->enable_dpdm << BQ2419x_DPDM_EN_SHIFT)
          | TMR2X_EN |(di->enable_batfet<< BQ2419x_BATFET_EN_SHIFT)
          | CHRG_FAULT_INT_EN |BAT_FAULT_INT_EN;

    bq2419x_write_byte(di, di->misc_operation_reg07, MISC_OPERATION_REG07);
    return;
}

/*deal with poor charger when capacity is more than 90% ,if hardware does not
 use REGN for usb_int,pls delete the follow fuction*/
static void bq2419x_reset_vindpm(struct bq2419x_device_info *di)
{
    int battery_capacity = 0;

    if ((di->charger_source == POWER_SUPPLY_TYPE_MAINS)&&(di->cin_dpmmV != VINDPM_4600)){
         battery_capacity = hisi_battery_capacity();
         if(battery_capacity > CAPACITY_LEVEL_HIGH_THRESHOLD){
             di->cin_dpmmV = VINDPM_4600;
             bq2419x_config_input_source_reg(di);
         }
     }
     return;
}

/*0 = temperature less than 42,1 = temperature more than 42 and less 45,2 more than 45 */
static void bq2419x_calling_limit_ac_input_current(struct bq2419x_device_info *di,int flag)
{
    if (di->charger_source == POWER_SUPPLY_TYPE_MAINS){
        switch(flag){
        case NORMAL_TEMP_CONFIG:
            if(di->calling_limit){
                iin_temp = IINLIM_900;
                ichg_temp = ICHG_820;
            } else {
                iin_temp = di->max_cin_currentmA;
                ichg_temp = di->max_currentmA;
            }
            break;
        case NORMAL_HIGH_TEMP_CONFIG:
            if(di->calling_limit){
                iin_temp = IINLIM_900;
                ichg_temp = ICHG_820;
            } else {
                if(iin_temp == di->max_cin_currentmA)
                    ichg_temp =  di->max_currentmA;
                iin_temp = di->cin_limit;
                ichg_temp = di->currentmA;
            }
            break;
        case HIGH_TEMP_CONFIG:
            iin_temp = IINLIM_900;
            ichg_temp = ICHG_820;
            break;
        case TEMP_CONFIG_5:
            if(di->calling_limit){
                iin_temp = IINLIM_900;
                ichg_temp = di->design_capacity / 10 * di->charge_in_temp_5;
            } else {
                iin_temp = di->max_cin_currentmA;
                /* battery whose max_voltage is above 4.35V is easy to broken
                when the temperature is below 10¡æ.
                So we need set the Current below 0.x * Capacity. */
                ichg_temp = di->design_capacity / 10 * di->charge_in_temp_5;
            }
            break;
        case TEMP_CONFIG_10:
            if(di->calling_limit){
                iin_temp = IINLIM_900;
                ichg_temp = di->design_capacity / 10 * di->charge_in_temp_10;
            } else {
                iin_temp = di->max_cin_currentmA;
                ichg_temp = di->design_capacity / 10 * di->charge_in_temp_10;
            }
            break;
        case NORMAL_SECOND_STAGE:
            if(di->calling_limit){
                iin_temp = IINLIM_900;
                ichg_temp = ICHG_820;
            } else {
                iin_temp = IINLIM_1200;
                ichg_temp = ICHG_1024;
            }
            break;
        /*case NORMAL_HIGH_TEMP_CONFIG_CP1:
            iin_temp = di->cin_limit;
            ichg_temp = di->currentmA;
            break;*/
        case NORMAL_HIGH_TEMP_CONFIG_CP2:
            iin_temp = IINLIM_900;
            ichg_temp = ICHG_820;
            break;
        /*case NORMAL_HIGH_TEMP_CONFIG_CP3:
            iin_temp = di->cin_limit;
            ichg_temp = di->currentmA;
            break;*/
        case NORMAL_HIGH_TEMP_CONFIG_CP4:
            iin_temp = IINLIM_500;
            ichg_temp = ICHG_512;
            break;
        default:
            break;
        }
    }else{
        iin_temp = IINLIM_500;
        ichg_temp = ICHG_512;
    }
    if(iin_temp > input_current_iin)
        iin_temp = input_current_iin;
    if(ichg_temp > input_current_ichg)
        ichg_temp = input_current_ichg;
    if(!mhl_limit_cin_flag)
        iin_temp = IINLIM_100;
    return;
}

static void bq2419x_config_limit_temperature_parameter(struct bq2419x_device_info *di)
{
    di->temperature_cold = BQ2419x_COLD_BATTERY_THRESHOLD;
    di->temperature_cool = BQ2419x_COOL_BATTERY_THRESHOLD;
    di->temperature_warm = BQ2419x_WARM_BATTERY_THRESHOLD;
    di->temperature_hot  = BQ2419x_HOT_BATTERY_THRESHOLD;
    di->temperature_5    = BQ2419x_BATTERY_THRESHOLD_5;
    di->temperature_10   = BQ2419x_BATTERY_THRESHOLD_10;
    return;
}

static int bq2419x_get_bat_temp_area(struct bq2419x_device_info *di)
{
    int i;
    int bat_temp = hisi_battery_temperature();
    for(i = 0;i < TEMP_CTL_POINTS_NUM - 1;++i){
        if((bat_temp >= bat_temp_ctl_points[di->bat_temp_ctl_type][i])
            &&(bat_temp < bat_temp_ctl_points[di->bat_temp_ctl_type][i+1])){
            return bat_temp_area[di->bat_temp_ctl_type][i];
        }
    }
    return BAT_TEMP_FROM_10_TO_NEXT_POINT;
}
enum usb_charger_type get_charger_name(void)
{
    enum hisi_charger_type type = hisi_get_charger_type();
    hwlog_info("hisi_charger_type = %d\n",type);
    switch(type)
    {
    case CHARGER_TYPE_SDP:
        return CHARGER_TYPE_USB;
    case CHARGER_TYPE_CDP:
        return CHARGER_TYPE_BC_USB;
    case CHARGER_TYPE_DCP:
        return CHARGER_TYPE_STANDARD;
    case CHARGER_TYPE_UNKNOWN:
        return CHARGER_TYPE_NON_STANDARD;
    case CHARGER_TYPE_NONE:
        return CHARGER_REMOVED;
    case PLEASE_PROVIDE_POWER:
        return USB_EVENT_OTG_ID;
    default:
        return CHARGER_REMOVED;
    }
}
/*
static int bq2419x_check_battery_temperature_japan_threshold(struct bq2419x_device_info *di)
{
    int battery_temperature = 0;
    battery_temperature = hisi_battery_temperature();

    if (battery_temperature < BQ2419x_COLD_BATTERY_THRESHOLD) {
        return BATTERY_HEALTH_TEMPERATURE_OVERLOW;

    } else if((battery_temperature >= BQ2419x_COLD_BATTERY_THRESHOLD)
        && (battery_temperature <  BQ2419x_COOL_BATTERY_THRESHOLD)){
        return BATTERY_HEALTH_TEMPERATURE_LOW;

    } else if ((battery_temperature >= BQ2419x_COOL_BATTERY_THRESHOLD)
        && (battery_temperature < BQ2419x_BATTERY_THRESHOLD_5)){
       return BATTERY_HEALTH_TEMPERATURE_5;

    } else if ((battery_temperature >= BQ2419x_BATTERY_THRESHOLD_5)
        && (battery_temperature < BQ2419x_BATTERY_THRESHOLD_10)){
       return BATTERY_HEALTH_TEMPERATURE_10;

    } else if ((battery_temperature >= BQ2419x_BATTERY_THRESHOLD_10)
        && (battery_temperature < japan_temp_cp[di->high_temp_para].cp1)){
       return BATTERY_HEALTH_TEMPERATURE_NORMAL;

    } else if ((battery_temperature >= japan_temp_cp[di->high_temp_para].cp1)
        && (battery_temperature < japan_temp_cp[di->high_temp_para].cp2)){
        return BATTERY_HEALTH_TEMPERATURE_HIGH_CP1;

    } else if ((battery_temperature >= japan_temp_cp[di->high_temp_para].cp2)
        && (battery_temperature < japan_temp_cp[di->high_temp_para].cp3)){
        return BATTERY_HEALTH_TEMPERATURE_HIGH_CP2;

    } else if ((battery_temperature >= japan_temp_cp[di->high_temp_para].cp3)
        && (battery_temperature < japan_temp_cp[di->high_temp_para].cp4)){
        return BATTERY_HEALTH_TEMPERATURE_HIGH_CP3;

    } else if ((battery_temperature >= japan_temp_cp[di->high_temp_para].cp4)
        && (battery_temperature < japan_temp_cp[di->high_temp_para].cp5)){
        return BATTERY_HEALTH_TEMPERATURE_HIGH_CP4;

    } else if ((battery_temperature >= japan_temp_cp[di->high_temp_para].cp5)
        && (battery_temperature < japan_temp_cp[di->high_temp_para].cp6)){
        return BATTERY_HEALTH_TEMPERATURE_HIGH_CP5;

    }else if (battery_temperature >=  japan_temp_cp[di->high_temp_para].cp6){
       return BATTERY_HEALTH_TEMPERATURE_OVERHIGH;

    } else {
       return BATTERY_HEALTH_TEMPERATURE_NORMAL;
    }
}

static int bq2419x_check_battery_temperature_threshold(struct bq2419x_device_info *di)
{
    int battery_temperature = 0;

    battery_temperature = hisi_battery_temperature();

    if (battery_temperature < di->temperature_cold) {
        return BATTERY_HEALTH_TEMPERATURE_OVERLOW;

    } else if((battery_temperature >= di->temperature_cold)
        && (battery_temperature <  di->temperature_cool)){
        return BATTERY_HEALTH_TEMPERATURE_LOW;

    } else if ((battery_temperature >= di->temperature_cool)
        && (battery_temperature < di->temperature_5)){
       return BATTERY_HEALTH_TEMPERATURE_5;

    } else if ((battery_temperature >= di->temperature_5)
        && (battery_temperature < di->temperature_10)){
       return BATTERY_HEALTH_TEMPERATURE_10;

    } else if ((battery_temperature >= di->temperature_10)
        && (battery_temperature < (di->temperature_warm - BQ2419x_TEMPERATURE_OFFSET))){
       return BATTERY_HEALTH_TEMPERATURE_NORMAL;

    } else if ((battery_temperature >= (di->temperature_warm - BQ2419x_TEMPERATURE_OFFSET))
        && (battery_temperature < di->temperature_warm)){
        return BATTERY_HEALTH_TEMPERATURE_NORMAL_HIGH;

    } else if ((battery_temperature >= di->temperature_warm)
        && (battery_temperature < di->temperature_hot)){
        return BATTERY_HEALTH_TEMPERATURE_HIGH;

    } else if (battery_temperature >= di->temperature_hot){
       return BATTERY_HEALTH_TEMPERATURE_OVERHIGH;

    } else {
       return BATTERY_HEALTH_TEMPERATURE_NORMAL;
    }
}
*/

static void bq2419x_monitor_battery_ntc_japan_charging(struct bq2419x_device_info *di)
{
    int battery_status = 0;
    long int events = VCHRG_START_CHARGING_EVENT;
    u8 read_reg = 0;
    int batt_temp = hisi_battery_temperature();

    if(!di->battery_present)
        return;

    bq2419x_read_byte(di, &read_reg, SYS_STATUS_REG08);
    if((read_reg & BQ2419x_CHGR_STAT_CHAEGE_DONE) == BQ2419x_CHGR_STAT_CHAEGE_DONE)
        return;

    bq2419x_reset_vindpm(di);

    di->battery_voltage = hisi_battery_voltage();

    hwlog_debug("battery temp is %d,battery_voltage is %d , pre status is %d\n",
        batt_temp, di->battery_voltage,di->battery_temp_status);

    if(di->not_limit_chrg_flag){
        bq2419x_config_power_on_reg(di);
        return;
    }

    battery_status = bq2419x_get_bat_temp_area(di);

    switch (battery_status) {
    case BATTERY_HEALTH_TEMPERATURE_OVERLOW:
        di->chrg_config = DIS_CHARGER;
        break;

    case BATTERY_HEALTH_TEMPERATURE_LOW:
        if (di->battery_voltage <= BQ2419x_LOW_TEMP_TERM_VOLTAGE)
            di->chrg_config = EN_CHARGER;
        else if (di->battery_voltage > BQ2419x_LOW_TEMP_NOT_CHARGING_VOLTAGE)
            di->chrg_config = DIS_CHARGER;
        else
            di->chrg_config =di->chrg_config;

        if(di->battery_temp_status != battery_status){
            ichg_temp = ICHG_100;
        }
        if (di->is_disable_cool_temperature_charger == 1){
            di->chrg_config = DIS_CHARGER;
        }
        break;

    case BATTERY_HEALTH_TEMPERATURE_5:
        di->chrg_config = EN_CHARGER;
        if (di->battery_temp_status != battery_status){
            bq2419x_calling_limit_ac_input_current(di,TEMP_CONFIG_5);
        }
        break;

    case BATTERY_HEALTH_TEMPERATURE_10:
        di->chrg_config = EN_CHARGER;
        if (di->battery_temp_status != battery_status){
            bq2419x_calling_limit_ac_input_current(di,TEMP_CONFIG_10);
        }
        break;

    case BATTERY_HEALTH_TEMPERATURE_NORMAL:
        di->chrg_config = EN_CHARGER;
        if((di->is_two_stage_charger== 1)
            &&( (di->battery_voltage >= di->first_stage_voltage)
                    || ((di->battery_voltage >= di->second_stage_voltage)
                    &&(di->two_stage_charger_status  == TWO_STAGE_CHARGE_SECOND_STAGE)))){
             bq2419x_calling_limit_ac_input_current(di,NORMAL_SECOND_STAGE);
             di->two_stage_charger_status  = TWO_STAGE_CHARGE_SECOND_STAGE;
        }else if(di->battery_voltage > BQ2419x_NORNAL_ICHRG_VOLTAGE){
             bq2419x_calling_limit_ac_input_current(di,NORMAL_TEMP_CONFIG);
             di->two_stage_charger_status  = TWO_STAGE_CHARGE_FIRST_STAGE;
        } else {
             ichg_temp = ICHG_820;
        }
        break;

    case BATTERY_HEALTH_TEMPERATURE_HIGH_CP1:
        di->chrg_config = EN_CHARGER;
        if (di->battery_temp_status != battery_status){
            bq2419x_calling_limit_ac_input_current(di,NORMAL_HIGH_TEMP_CONFIG_CP1);
        }
        break;

     case BATTERY_HEALTH_TEMPERATURE_HIGH_CP2:
        di->chrg_config = EN_CHARGER;
        if (di->battery_temp_status != battery_status){
            bq2419x_calling_limit_ac_input_current(di,NORMAL_HIGH_TEMP_CONFIG_CP2);
        }
        break;

      case BATTERY_HEALTH_TEMPERATURE_HIGH_CP3:
        di->chrg_config = EN_CHARGER;
        if (di->battery_temp_status != battery_status){
            bq2419x_calling_limit_ac_input_current(di,NORMAL_HIGH_TEMP_CONFIG_CP3);
        }
        break;

     case BATTERY_HEALTH_TEMPERATURE_HIGH_CP4:
        di->chrg_config = EN_CHARGER;
        if (di->battery_temp_status != battery_status){
            bq2419x_calling_limit_ac_input_current(di,NORMAL_HIGH_TEMP_CONFIG_CP4);
        }
        break;

     case BATTERY_HEALTH_TEMPERATURE_HIGH_CP5:
        di->chrg_config = di->chrg_config;
        if ((di->battery_temp_status != battery_status) && (di->chrg_config == EN_CHARGER)){
            bq2419x_calling_limit_ac_input_current(di,NORMAL_HIGH_TEMP_CONFIG_CP4);
        }
        break;

    case BATTERY_HEALTH_TEMPERATURE_OVERHIGH:
        di->chrg_config = DIS_CHARGER;
        break;
    default:
        break;
    }
    di->battery_temp_status = battery_status;

    if(iin_limit_temp == 1){
        if(di->cin_limit != iin_temp){
            di->cin_limit = iin_temp;
            bq2419x_config_input_source_reg(di);
        }
    }else{
        di->cin_limit = iin_limit_temp;
        bq2419x_config_input_source_reg(di);
    }

    if(ichg_limit_temp == 1){
        if(di->currentmA != ichg_temp){
            di->currentmA = ichg_temp;
            bq2419x_config_current_reg(di);
        }
    }else{
        di->currentmA = ichg_limit_temp;
        bq2419x_config_current_reg(di);
    }

    di->chrg_config = (di->chrg_config & di->factory_flag) | di->not_stop_chrg_flag;

    if(di->chrg_config){
        events = VCHRG_START_CHARGING_EVENT;
    } else {
        events = VCHRG_NOT_CHARGING_EVENT;
    }
    bq2419x_config_power_on_reg(di);

    hisi_coul_charger_event_rcv(events);//blocking_notifier_call_chain(&notifier_list, events, NULL);

    return;
}

static void bq2419x_monitor_battery_ntc_charging(struct bq2419x_device_info *di)
{
    int battery_status = 0;
    long int events = VCHRG_START_CHARGING_EVENT;
    u8 read_reg = 0;
    int batt_temp = hisi_battery_temperature();

    if(di->japan_charger){
        return bq2419x_monitor_battery_ntc_japan_charging(di);
    }

    if(!di->battery_present)
        return;

    bq2419x_reset_vindpm(di);

    di->battery_voltage = hisi_battery_voltage();

    hwlog_debug("battery temp is %d,battery_voltage is %d , pre status is %d\n",
        batt_temp, di->battery_voltage,di->battery_temp_status);

    if(di->not_limit_chrg_flag){
        bq2419x_config_power_on_reg(di);
        hwlog_info("function return by not limit charge flag\n");
        return;
    }

    battery_status = bq2419x_get_bat_temp_area(di);

    switch (battery_status) {
    case BATTERY_HEALTH_TEMPERATURE_OVERLOW:
        di->chrg_config = DIS_CHARGER;
        break;
    case BATTERY_HEALTH_TEMPERATURE_LOW:
        if (di->battery_voltage <= BQ2419x_LOW_TEMP_TERM_VOLTAGE)
            di->chrg_config = EN_CHARGER;
        else if (di->battery_voltage > BQ2419x_LOW_TEMP_NOT_CHARGING_VOLTAGE)
            di->chrg_config = DIS_CHARGER;
        else
            di->chrg_config =di->chrg_config;

        if(di->battery_temp_status != battery_status){
            ichg_temp = ICHG_100;
       }
        break;
    case BATTERY_HEALTH_TEMPERATURE_5:
        di->chrg_config = EN_CHARGER;
       if (di->battery_temp_status != battery_status){
           bq2419x_calling_limit_ac_input_current(di,TEMP_CONFIG_5);
       }
        break;
    case BATTERY_HEALTH_TEMPERATURE_10:
        di->chrg_config = EN_CHARGER;
        if (di->battery_temp_status != battery_status){
            bq2419x_calling_limit_ac_input_current(di,TEMP_CONFIG_10);
        }
        break;
    case BATTERY_HEALTH_TEMPERATURE_NORMAL:
        di->chrg_config = EN_CHARGER;
        if(di->battery_voltage > BQ2419x_NORNAL_ICHRG_VOLTAGE){
             bq2419x_calling_limit_ac_input_current(di,NORMAL_TEMP_CONFIG);
        } else {
             ichg_temp = ICHG_820;
        }
        break;
    case BATTERY_HEALTH_TEMPERATURE_NORMAL_HIGH:
        di->chrg_config = EN_CHARGER;
        if (di->battery_temp_status != battery_status){
           bq2419x_calling_limit_ac_input_current(di,NORMAL_HIGH_TEMP_CONFIG);
        }
        break;
    case BATTERY_HEALTH_TEMPERATURE_HIGH:
        di->chrg_config = EN_CHARGER;
        if (di->battery_temp_status != battery_status){
            bq2419x_calling_limit_ac_input_current(di,HIGH_TEMP_CONFIG);
        }
        break;
    case BATTERY_HEALTH_TEMPERATURE_OVERHIGH:
        di->chrg_config = DIS_CHARGER;
        break;
    default:
        break;
    }

    if(di->battery_temp_status != battery_status){
        hwlog_info("battery temp status changed\n");
    }

    di->battery_temp_status = battery_status;

    if(iin_limit_temp == 1)
    {
        if(di->cin_limit != iin_temp)
        {
            di->cin_limit = iin_temp;
        }
    }
    else
    {
        di->cin_limit = iin_limit_temp;
    }
    bq2419x_config_input_source_reg(di);

    if(ichg_limit_temp == 1)
    {
        if(di->currentmA != ichg_temp)
        {
            di->currentmA = ichg_temp;
        }
    }
    else
    {
        di->currentmA = ichg_limit_temp;
    }
    bq2419x_config_current_reg(di);

    di->chrg_config = (di->chrg_config & di->factory_flag) | di->not_stop_chrg_flag;

    bq2419x_config_power_on_reg(di);

    bq2419x_read_byte(di, &read_reg, SYS_STATUS_REG08);
    if((read_reg & BQ2419x_CHGR_STAT_CHAEGE_DONE) == BQ2419x_CHGR_STAT_CHAEGE_DONE)
    {
        hwlog_info("function return by charge done\n");
        return;
    }

    if(di->chrg_config){
        events = VCHRG_START_CHARGING_EVENT;
    } else {
        events = VCHRG_NOT_CHARGING_EVENT;
    }

    hwlog_debug("i_in = %d, charge_current = %d\n", di->cin_limit, di->currentmA);

    hisi_coul_charger_event_rcv(events);//blocking_notifier_call_chain(&notifier_list, events, NULL);

    return;
}

static void bq2419x_start_usb_charger(struct bq2419x_device_info *di)
{
    long int  events = VCHRG_START_USB_CHARGING_EVENT;

    di->wakelock_enabled = 1;
    if (di->wakelock_enabled){
        wake_lock(&chrg_lock);
    }

    hisi_coul_charger_event_rcv(events);//blocking_notifier_call_chain(&notifier_list, events, NULL);
    di->charger_source = POWER_SUPPLY_TYPE_USB;
    bq2419x_get_max_charge_voltage(di);
    di->cin_dpmmV = VINDPM_4520;
    di->cin_limit = IINLIM_500;
    di->currentmA = ICHG_512;

    iin_temp = di->cin_limit;
    ichg_temp = di->currentmA;
    di->voltagemV = di->max_voltagemV;

    di->term_currentmA = ITERM_MIN_128;
    di->watchdog_timer = WATCHDOG_80;
    di->chrg_timer = CHG_TIMER_12;

    di->hz_mode = DIS_HIZ;
    di->chrg_config = EN_CHARGER;
    di->boost_lim = BOOST_LIM_500;
    di->enable_low_chg = DIS_FORCE_20PCT;
    di->enable_iterm = DIS_TERM;
    di->enable_timer = EN_TIMER;
    di->enable_batfet = EN_BATFET;
    di->factory_flag = EN_CHARGER;
    di->calling_limit = 0;
    di->battery_temp_status = -1;
    di->enable_dpdm = DPDM_EN;
    di->charge_full_count = 0;

    bq2419x_config_power_on_reg(di);
    msleep(500);
    bq2419x_config_input_source_reg(di);
    bq2419x_config_current_reg(di);
    bq2419x_config_prechg_term_current_reg(di);
    bq2419x_config_voltage_reg(di);
    bq2419x_config_term_timer_reg(di);
    bq2419x_config_thernal_regulation_reg(di);
    bq2419x_config_misc_operation_reg(di);

    gpio_set_value(di->gpio_cd, 0);

    schedule_delayed_work(&di->bq2419x_charger_work, msecs_to_jiffies(0));

    hwlog_info(" ---->START USB CHARGING,battery current = %d mA,battery voltage = %d mV,cin_limit_current = %d mA\n"
                         , di->currentmA, di->voltagemV,di->cin_limit);

    di->battery_present = is_hisi_battery_exist();
    if (!di->battery_present) {
            hwlog_debug( "BATTERY NOT DETECTED!\n");
            events = VCHRG_NOT_CHARGING_EVENT;
            hisi_coul_charger_event_rcv(events);//blocking_notifier_call_chain(&notifier_list, events, NULL);
    }

    return;
}

static void bq2419x_start_ac_charger(struct bq2419x_device_info *di)
{
    long int  events = VCHRG_START_AC_CHARGING_EVENT;

    di->wakelock_enabled = 1;
    if (di->wakelock_enabled){
        wake_lock(&chrg_lock);
    }

    hisi_coul_charger_event_rcv(events);//blocking_notifier_call_chain(&notifier_list, events, NULL);
    di->charger_source = POWER_SUPPLY_TYPE_MAINS;
    bq2419x_get_max_charge_voltage(di);
    di->cin_dpmmV = VINDPM_4520;
    di->cin_limit = di->max_cin_currentmA;
    di->voltagemV = di->max_voltagemV;
    di->currentmA = di->max_currentmA;
    iin_temp = di->cin_limit;
    ichg_temp = di->currentmA;
    di->term_currentmA = ITERM_MIN_128;
    di->watchdog_timer = WATCHDOG_80;
    di->chrg_timer = CHG_TIMER_12;

    di->hz_mode = DIS_HIZ;
    di->chrg_config = EN_CHARGER;
    di->boost_lim = BOOST_LIM_500;
    di->enable_low_chg = DIS_FORCE_20PCT;
    di->enable_iterm = DIS_TERM;
    di->enable_timer = EN_TIMER;
    di->enable_batfet = EN_BATFET;
    di->factory_flag = EN_CHARGER;
    di->enable_dpdm = DPDM_DIS;
    di->calling_limit = 0;
    di->battery_temp_status = -1;
    di->charge_full_count = 0;

    bq2419x_config_power_on_reg(di);
    msleep(500);
    bq2419x_config_input_source_reg(di);
    bq2419x_config_current_reg(di);
    bq2419x_config_prechg_term_current_reg(di);
    bq2419x_config_voltage_reg(di);
    bq2419x_config_term_timer_reg(di);
    bq2419x_config_thernal_regulation_reg(di);
    bq2419x_config_misc_operation_reg(di);

    gpio_set_value(di->gpio_cd, 0);

    schedule_delayed_work(&di->bq2419x_charger_work, msecs_to_jiffies(0));

    hwlog_info("---->START AC CHARGING,battery current = %d mA,battery voltage = %d mV,cin_limit_current = %d mA\n"
                         , di->currentmA, di->voltagemV,di->cin_limit);

    di->battery_present = is_hisi_battery_exist();
    if (!di->battery_present) {
            hwlog_debug( "BATTERY NOT DETECTED!\n");
            events = VCHRG_NOT_CHARGING_EVENT;
            hisi_coul_charger_event_rcv(events);//blocking_notifier_call_chain(&notifier_list, events, NULL);
    }

    return;
}

static void bq2419x_start_usb_otg(struct bq2419x_device_info *di)
{
    hwlog_info("%s,---->USB_EVENT_OTG_ID<----\n", __func__);

    if(di->charger_source != POWER_SUPPLY_TYPE_BATTERY){
        gpio_set_value(di->gpio_cd, 1);
        return;
    }

    di->wakelock_enabled = 1;
    if (di->wakelock_enabled){
        wake_lock(&chrg_lock);
    }

    di->hz_mode = DIS_HIZ;
    di->chrg_config = EN_CHARGER_OTG;
    di->boost_lim = BOOST_LIM_500;

    bq2419x_config_power_on_reg(di);
    bq2419x_config_input_source_reg(di);

    gpio_set_value(di->gpio_cd, 0);
    if(irq_int_active == 0) {
        irq_int_active = 1;
        enable_irq(di->irq_int);
    }
    schedule_delayed_work(&di->bq2419x_usb_otg_work, msecs_to_jiffies(0));

    return;
}

static void bq2419x_stop_charger(struct bq2419x_device_info *di)
{
    long int  events  = VCHRG_STOP_CHARGING_EVENT;

    if (!wake_lock_active(&chrg_lock)){
        wake_lock(&chrg_lock);
    }

    /*set gpio_074 high level for CE pin to disable bq2419x IC */
    gpio_set_value(di->gpio_cd, 1);
    if(irq_int_active == 1) {
        disable_irq(di->irq_int);
        irq_int_active = 0;
    }

    cancel_delayed_work_sync(&di->bq2419x_charger_work);
    hisi_coul_charger_event_rcv(events);//blocking_notifier_call_chain(&notifier_list, events, NULL);

    hwlog_info("%s,---->STOP CHARGING\n", __func__);
    di->charger_source = POWER_SUPPLY_TYPE_BATTERY;

    di->enable_batfet = EN_BATFET;
    di->calling_limit = 0;
    di->factory_flag = 0;
    di->hz_mode = DIS_HIZ;
    di->battery_temp_status = -1;
    di->chrg_config = DIS_CHARGER;
    di->enable_dpdm = DPDM_DIS;
    di->charge_full_count = 0;

    bq2419x_config_power_on_reg(di);
    bq2419x_config_input_source_reg(di);
    bq2419x_config_misc_operation_reg(di);


    cancel_delayed_work_sync(&di->bq2419x_usb_otg_work);
    cancel_delayed_work_sync(&di->otg_int_work);

    /*set gpio_074 high level for CE pin to disable bq2419x IC */
    gpio_set_value(di->gpio_cd, 1);
    msleep(1000);

    di->wakelock_enabled = 1;
    if (di->wakelock_enabled){
        wake_lock_timeout(&stop_chrg_lock, HZ);
        wake_unlock(&chrg_lock);
    }

    wakeup_timer_seconds = 0;
   return;
}

static void bq2419x_check_bq27510_charge_full(struct bq2419x_device_info *di)
{
    if(di->battery_present){

        if(di->battery_full){
            di->enable_iterm = EN_TERM;
            di->charge_full_count++;
            if(di->charge_full_count >= 20){
               di->charge_full_count = 20;
            }
        }else{
            di->enable_iterm = DIS_TERM;
        }
    }else{
        di->enable_iterm = EN_TERM;
    }

    bq2419x_config_term_timer_reg(di);

    return;
}

/*in order to avoid problems caused by suspend & resume(such as emmc related problem),
we do not release wakelock for general product when chargedone. this shceme is effective
only for product sold to USA*/
#if 0
static void bq2419x_charger_done_release_wakelock(struct bq2419x_device_info *di)
{
    if(di->charger_source == POWER_SUPPLY_TYPE_MAINS){
        if(!di->battery_present)
            return;
        if(di->battery_full){
            if (wake_lock_active(&chrg_lock)){
                if(di->charge_full_count >= 20){
                    wake_unlock(&chrg_lock);
                    hwlog_err("ac charge done wakelock release\n");
                }
            }
        }else{
            if (!wake_lock_active(&chrg_lock)){
                wake_lock(&chrg_lock);
                hwlog_err( "ac recharge wakelock add again\n");
            }
        }
    }
    return;
}
#endif
static void bq2419x_config_status_reg(struct bq2419x_device_info *di)
{
    di->power_on_config_reg01 = di->power_on_config_reg01 |WATCHDOG_TIMER_RST;
    bq2419x_write_byte(di, di->power_on_config_reg01, POWER_ON_CONFIG_REG01);
    return;
}

static void
bq2419x_charger_update_status(struct bq2419x_device_info *di)
{
    long int  events  = 0;
    int dsm_charger_error_found = -1;
    u8 read_reg[11] = {0};

    di->timer_fault = 0;
    bq2419x_read_block(di, &read_reg[0], 0, 11);
    bq2419x_read_byte(di, &read_reg[9], CHARGER_FAULT_REG09);
    msleep(700);
    bq2419x_read_byte(di, &read_reg[9], CHARGER_FAULT_REG09);

    if ((read_reg[8] & BQ2419x_CHGR_STAT_CHAEGE_DONE) == BQ2419x_CHGR_STAT_CHAEGE_DONE){
        hwlog_debug("CHARGE DONE\n");
        events  = VCHRG_CHARGE_DONE_EVENT;
        hisi_coul_charger_event_rcv(events);//blocking_notifier_call_chain(&notifier_list, events, NULL);
    }

    if ((read_reg[8] & BQ2419x_PG_STAT) == BQ2419x_NOT_PG_STAT){
        di->cfg_params = 1;
        hwlog_info("not power good\n");
        events  = VCHRG_POWER_SUPPLY_WEAKSOURCE;
        hisi_coul_charger_event_rcv(events);//blocking_notifier_call_chain(&notifier_list, events, NULL);
    }

    if (!dsm_client_ocuppy(charger_dclient)) {
	dsm_charger_error_found++;
    }

    if ((read_reg[9] & BQ2419x_POWER_SUPPLY_OVP) == BQ2419x_POWER_SUPPLY_OVP){
        if (dsm_charger_error_found >= 0 && jiffies > POWER_SUPPLY_OVP_MONITOR_START_TIME * HZ) {
		dsm_client_record(charger_dclient, "POWER_SUPPLY_OVERVOLTAGE!\
reg[9]:0x%x.\n", read_reg[9]);
		dsm_charger_error_found++;
	}

        hwlog_err("POWER_SUPPLY_OVERVOLTAGE = %x\n", read_reg[9]);
        events  = VCHRG_POWER_SUPPLY_OVERVOLTAGE;
        hisi_coul_charger_event_rcv(events);//blocking_notifier_call_chain(&notifier_list, events, NULL);
    }

    if ((read_reg[9] & BQ2419x_WATCHDOG_FAULT) == BQ2419x_WATCHDOG_FAULT) {
        if (dsm_charger_error_found >= 0) {
		dsm_client_record(charger_dclient, "WATCHDOG_FAULT!reg[9]: 0x%x.\n", read_reg[9]);
		dsm_charger_error_found++;
	}

	di->timer_fault = 1;
   }

    if (read_reg[9] & 0xFF) {
        di->cfg_params = 1;
        hwlog_err("CHARGER STATUS %x\n", read_reg[9]);
    }

    if ((read_reg[9] & BQ2419x_BAT_FAULT_OVP) == BQ2419x_BAT_FAULT_OVP) {
        if (!udp_charge) {
            di->hz_mode = EN_HIZ;
            bq2419x_config_input_source_reg(di);
            bq2419x_write_byte(di, di->charge_voltage_reg04, CHARGE_VOLTAGE_REG04);
            if (dsm_charger_error_found >= 0) {
                dsm_client_record(charger_dclient, "BATTERY OVP!reg[9]:0x%x.\n", read_reg[9]);
		  dsm_charger_error_found++;
	     }

            hwlog_err("BATTERY OVP = %x\n", read_reg[9]);
            msleep(700);
            di->hz_mode = DIS_HIZ;
            bq2419x_config_input_source_reg(di);
        }
    }

    if (dsm_charger_error_found > 0) {
        dsm_client_notify(charger_dclient, DSM_CHARGER_ERROR_NO);
    } else if (!dsm_charger_error_found) {
        dsm_client_unocuppy(charger_dclient);
    } else {
        /* dsm charger_dclient ocuppy failed. */
    }

    if ((di->timer_fault == 1) || (di->cfg_params == 1)) {
        bq2419x_write_byte(di, di->input_source_reg00, INPUT_SOURCE_REG00);
        bq2419x_write_byte(di, di->power_on_config_reg01, POWER_ON_CONFIG_REG01);
        bq2419x_write_byte(di, di->charge_current_reg02, CHARGE_CURRENT_REG02);
        bq2419x_write_byte(di, di->prechrg_term_current_reg03, PRECHARGE_TERM_CURRENT_REG03);
        bq2419x_write_byte(di, di->charge_voltage_reg04, CHARGE_VOLTAGE_REG04);
        bq2419x_write_byte(di, di->term_timer_reg05, CHARGE_TERM_TIMER_REG05);
        bq2419x_write_byte(di, di->thermal_regulation_reg06, THERMAL_REGUALTION_REG06);
        di->cfg_params = 0;
    }

    /* reset 30 second timer */
    bq2419x_config_status_reg(di);
}

static void bq2419x_charger_work(struct work_struct *work)
{
    struct bq2419x_device_info *di = container_of(work,
                 struct bq2419x_device_info, bq2419x_charger_work.work);

    di->battery_present = is_hisi_battery_exist();

    di->battery_full = is_hisi_battery_full();

    bq2419x_monitor_battery_ntc_charging(di);

    bq2419x_check_bq27510_charge_full(di);

    bq2419x_charger_update_status(di);

    /* do not release wakelock for general product*/
    //bq2419x_charger_done_release_wakelock(di);

    schedule_delayed_work(&di->bq2419x_charger_work,
                 msecs_to_jiffies(BQ2419x_WATCHDOG_TIMEOUT));
}
static void bq2419x_otg_int_work(struct work_struct *work) {
    struct bq2419x_device_info *di = container_of(work,struct bq2419x_device_info, otg_int_work.work);
    u8 read_reg = 0;

    if(di->event != USB_EVENT_OTG_ID)
        return;
    hwlog_info("bq2419x_otg_int_work\n");

    msleep(100);
    bq2419x_read_byte(di, &read_reg, CHARGER_FAULT_REG09);
    hwlog_info("reg09=0x%x first\n",read_reg);
    msleep(100);
    bq2419x_read_byte(di, &read_reg, CHARGER_FAULT_REG09);
    hwlog_info("reg09=0x%x then\n", read_reg);

    if(read_reg & BQ2419x_OTG_FAULT){
        if (!dsm_client_ocuppy(charger_dclient)) {
	     dsm_client_record(charger_dclient, "VBUS overloaded in OTG read_reg[9]= \
0x%x. \n", read_reg);
	     dsm_client_notify(charger_dclient, DSM_CHARGER_ERROR_NO);
        }
	 di->chrg_config = DIS_CHARGER;
        bq2419x_config_power_on_reg(di);
        dev_err(di->dev, "VBUS overloaded in OTG read_reg[9]= %x\n", read_reg);
        return;
    }

    if(irq_int_active == 0) {
        irq_int_active = 1;
        enable_irq(di->irq_int);
    }
}
static irqreturn_t bq2419x_irq_int_interrupt(int irq, void *_di) {
    struct bq2419x_device_info *di = _di;

    hwlog_info("OTG interrupt\n");
    if(irq_int_active == 1) {
        disable_irq_nosync(di->irq_int);
        irq_int_active = 0;
    }
    schedule_delayed_work(&di->otg_int_work,0);
    return IRQ_HANDLED;
}
static void bq2419x_usb_otg_work(struct work_struct *work)
{
    struct bq2419x_device_info *di = container_of(work,
        struct bq2419x_device_info, bq2419x_usb_otg_work.work);
    /* reset 30 second timer */
    bq2419x_config_power_on_reg(di);
    hwlog_debug("bq2419x_usb_otg_work\n");

    schedule_delayed_work(&di->bq2419x_usb_otg_work,
                 msecs_to_jiffies(BQ2419x_WATCHDOG_TIMEOUT));
}

static void bq2419x_usb_charger_work(struct work_struct *work)
{
    struct bq2419x_device_info	*di =
              container_of(work, struct bq2419x_device_info, usb_work);

    switch (di->event) {
    case CHARGER_TYPE_USB:
        hwlog_info("case = CHARGER_TYPE_USB-> \n");
        bq2419x_start_usb_charger(di);
        break;
    case CHARGER_TYPE_NON_STANDARD:
        hwlog_info("case = CHARGER_TYPE_NON_STANDARD -> \n");
        bq2419x_start_usb_charger(di);
        break;
    case CHARGER_TYPE_BC_USB:
        hwlog_info("case = CHARGER_TYPE_BC_USB -> \n");
        bq2419x_start_ac_charger(di);
        break;
    case CHARGER_TYPE_STANDARD:
        hwlog_info("case = CHARGER_TYPE_STANDARD\n");
        bq2419x_start_ac_charger(di);
        break;
    case CHARGER_REMOVED:
        hwlog_info("case = USB_EVENT_NONE\n");
        bq2419x_stop_charger(di);
        break;
    case USB_EVENT_OTG_ID:
        hwlog_info("case = USB_EVENT_OTG_ID\n");
        bq2419x_start_usb_otg(di);
        break;
    default:
        break;
    }
}

struct bq2419x_device_info *g_di;

void charger_usb_stub(int type)
{
    g_di->event = type;

    bq2419x_usb_charger_work(&g_di->usb_work);
}

static int bq2419x_usb_notifier_call(struct notifier_block *nb,
                        unsigned long event, void *data)
{
    struct bq2419x_device_info *di =
          container_of(nb, struct bq2419x_device_info, nb);

    switch(event) {
    case CHARGER_TYPE_SDP:
        di->event = CHARGER_TYPE_USB;
        break;
    case CHARGER_TYPE_CDP:
        di->event = CHARGER_TYPE_BC_USB;
        break;
    case CHARGER_TYPE_DCP:
        di->event = CHARGER_TYPE_STANDARD;
        break;
    case CHARGER_TYPE_UNKNOWN:
        di->event =  CHARGER_TYPE_NON_STANDARD;
        break;
    case CHARGER_TYPE_NONE:
        di->event = CHARGER_REMOVED;
        break;
    case PLEASE_PROVIDE_POWER:
        di->event = USB_EVENT_OTG_ID;
        break;
    default:
        return NOTIFY_OK;
    }

    schedule_work(&di->usb_work);
    return NOTIFY_OK;
}


/*
* set 1 --- hz_mode ; 0 --- not hz_mode
*/
static ssize_t bq2419x_set_enable_hz_mode(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    di->hz_mode= val;
    bq2419x_config_input_source_reg(di);

    return status;
}

static ssize_t bq2419x_show_enable_hz_mode(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->hz_mode;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

static ssize_t bq2419x_set_dppm_voltage(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < VINDPM_MIN_3880)
                      || (val > VINDPM_MAX_5080))
         return -EINVAL;

    di->cin_dpmmV = val;
    bq2419x_config_input_source_reg(di);

    return status;
}

static ssize_t bq2419x_show_dppm_voltage(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->cin_dpmmV;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

static ssize_t bq2419x_set_cin_limit(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if(strict_strtol(buf, 10, &val) < 0){
        return -EINVAL;
    }

    if(((val > 1) && (val < IINLIM_100)) || (val > IINLIM_3000)){
        return -EINVAL;
    }

    if(val == 1){
        iin_limit_temp = val;
        return status;
    }

    iin_limit_temp = val;
    if(di->cin_limit > iin_limit_temp){
        di->cin_limit = iin_limit_temp;
        bq2419x_config_input_source_reg(di);
    }

    hwlog_info("set cin_limit = %d\n", iin_limit_temp);

    return status;
}

static ssize_t bq2419x_show_cin_limit(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->cin_limit;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

static ssize_t bq2419x_set_regulation_voltage(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < VCHARGE_MIN_3504)
                     || (val > VCHARGE_MAX_4400))
        return -EINVAL;

    di->voltagemV = val;
    bq2419x_config_voltage_reg(di);

    return status;
}

static ssize_t bq2419x_show_regulation_voltage(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->voltagemV;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

static ssize_t bq2419x_set_charge_current(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if(strict_strtol(buf, 10, &val) < 0){
        return -EINVAL;
    }

    if(((val > 1) && (val < ICHG_512)) || (val > ICHG_3000)){
        return -EINVAL;
    }

    if(val == 1){
        ichg_limit_temp = val;
        return status;
    }

    ichg_limit_temp = val;
    if(di->currentmA > ichg_limit_temp){
        di->currentmA = ichg_limit_temp;
        bq2419x_config_current_reg(di);
    }

    hwlog_info("set currentmA = %d\n", ichg_limit_temp);

    return status;
}

static ssize_t bq2419x_show_charge_current(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->currentmA;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

static ssize_t bq2419x_set_precharge_current(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > IPRECHRG_MAX_2048))
    return -EINVAL;

    di->prechrg_currentmA = val;
    bq2419x_config_prechg_term_current_reg(di);

    return status;
}

static ssize_t bq2419x_show_precharge_current(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->prechrg_currentmA;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

static ssize_t bq2419x_set_termination_current(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > ITERM_MAX_2048))
        return -EINVAL;

    di->term_currentmA = val;
    bq2419x_config_prechg_term_current_reg(di);

    return status;
}

static ssize_t bq2419x_show_termination_current(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->term_currentmA;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

static ssize_t bq2419x_set_enable_itermination(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    di->enable_iterm = val;
    bq2419x_config_term_timer_reg(di);

    return status;
}

static ssize_t bq2419x_show_enable_itermination(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->enable_iterm;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

/* set 1 --- enable_charger; 0 --- disable charger */
static ssize_t bq2419x_set_enable_charger(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    long int events = VCHRG_START_CHARGING_EVENT;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    hwlog_info("set factory_flag = %ld not_stop_chrg_flag = 0 not_limit_chrg_flag = 0\n", val);

    di->chrg_config = val;
    di->factory_flag = val;
    di->not_stop_chrg_flag = 0;
    di->not_limit_chrg_flag = 0;
    if(di->factory_flag){
        events = VCHRG_START_CHARGING_EVENT;
    } else {
        events = VCHRG_NOT_CHARGING_EVENT;
    }
    bq2419x_config_power_on_reg(di);

    hisi_coul_charger_event_rcv(events);//blocking_notifier_call_chain(&notifier_list, events, NULL);
    return status;
}

static ssize_t bq2419x_show_enable_charger(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->chrg_config ;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

int bq2419x_get_factory_flag(void)
{
    struct bq2419x_device_info *di = g_di;

    if (g_di)
        return di->factory_flag;
    else
        return 0;
}


/* set 1 --- enable_batfet; 0 --- disable batfet */
static ssize_t bq2419x_set_enable_batfet(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    di->enable_batfet = val ^ 0x01;
    bq2419x_config_misc_operation_reg(di);

    return status;
}

static ssize_t bq2419x_show_enable_batfet(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->enable_batfet ^ 0x01;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

/*
* set 1 --- enable bq24192 IC; 0 --- disable bq24192 IC
*
*/
static ssize_t bq2419x_set_enable_cd(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    di->cd_active =val ^ 0x1;
    gpio_set_value(di->gpio_cd, di->cd_active);
    return status;
}

static ssize_t bq2419x_show_enable_cd(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->cd_active ^ 0x1;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

static ssize_t bq2419x_set_charging(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if (strncmp(buf, "startac", 7) == 0) {
        if (di->charger_source == POWER_SUPPLY_TYPE_USB)
            bq2419x_stop_charger(di);
        bq2419x_start_ac_charger(di);
    } else if (strncmp(buf, "startusb", 8) == 0) {
        if (di->charger_source == POWER_SUPPLY_TYPE_MAINS)
            bq2419x_stop_charger(di);
        bq2419x_start_usb_charger(di);
    } else if (strncmp(buf, "stop" , 4) == 0) {
        bq2419x_stop_charger(di);
    } else if (strncmp(buf, "otg" , 3) == 0) {
        if (di->charger_source == POWER_SUPPLY_TYPE_BATTERY){
            bq2419x_stop_charger(di);
            bq2419x_start_usb_otg(di);
        } else{
            return -EINVAL;
        }
    } else
        return -EINVAL;

    return status;
}

static ssize_t bq2419x_set_wakelock_enable(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    if ((val) && (di->charger_source != POWER_SUPPLY_TYPE_BATTERY))
        wake_lock(&chrg_lock);
    else
        wake_unlock(&chrg_lock);

    di->wakelock_enabled = val;
    return status;
}

static ssize_t bq2419x_show_wakelock_enable(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned int val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->wakelock_enabled;
    return snprintf(buf, PAGE_SIZE, "%u\n", val);
}

static ssize_t bq2419x_show_chargelog(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    int i = 0;
    u8 read_reg[11] = {0};
    u8 buf_temp[26] = {0};
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    bq2419x_read_block(di, &read_reg[0], 0, 11);
    bq2419x_read_byte(di, &read_reg[9], CHARGER_FAULT_REG09);
    for(i=0;i<11;i++)
    {
        snprintf(buf_temp,26,"0x%-8.2x",read_reg[i]);
        strncat(buf, buf_temp, strlen(buf_temp));
    }
    strncat(buf, "\n", 1);
   return strlen(buf);
}

static ssize_t bq2419x_set_calling_limit(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
       return -EINVAL;

    di->calling_limit = val;
    if (di->charger_source == POWER_SUPPLY_TYPE_MAINS)
    {
        if(di->calling_limit){
            di->cin_limit = IINLIM_900;
            di->currentmA = ICHG_820;
            bq2419x_config_input_source_reg(di);
            bq2419x_config_current_reg(di);
            hwlog_info("calling_limit_current = %d\n", di->cin_limit);
        } else {
            di->battery_temp_status = -1;
            di->cin_limit = di->max_cin_currentmA;
            di->currentmA = di->max_currentmA ;
            bq2419x_config_input_source_reg(di);
            bq2419x_config_current_reg(di);
        }
    } else {
        di->calling_limit = 0;
    }

    return status;
}

static ssize_t bq2419x_show_calling_limit(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->calling_limit;

    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}
static ssize_t bq2419x_set_compesation_resistor(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 70))
        return -EINVAL;

    di->bat_compohm = val;
    bq2419x_config_thernal_regulation_reg(di);

    return status;
}

static ssize_t bq2419x_show_compesation_resistor(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->bat_compohm;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

static ssize_t bq2419x_set_compesation_voltage(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 112))
        return -EINVAL;

    di->comp_vclampmV = val;
    bq2419x_config_thernal_regulation_reg(di);

    return status;
}

static ssize_t bq2419x_show_compesation_voltage(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->comp_vclampmV;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

static ssize_t bq2419x_set_not_limit_chrg_flag(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    hwlog_info("set not_limit_chrg_flag = %ld\n", val);

    di->not_limit_chrg_flag = val;

    di->currentmA = di->max_currentmA;
    di->cin_limit = di->max_cin_currentmA;
    bq2419x_config_current_reg(di);
    bq2419x_config_power_on_reg(di);

    return status;
}

static ssize_t bq2419x_show_not_limit_chrg_flag(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->not_limit_chrg_flag;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

static ssize_t bq2419x_set_not_stop_chrg_flag(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    hwlog_info("set not_stop_chrg_flag = %ld\n", val);

    di->not_stop_chrg_flag = val;

    return status;
}

static ssize_t bq2419x_show_not_stop_chrg_flag(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->not_stop_chrg_flag;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}


static ssize_t bq2419x_set_temperature_parameter(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 40)
                   || (val > 70))
        return -EINVAL;

     di->temperature_warm = val;
     di->temperature_hot  = val + 5;

    bq2419x_monitor_battery_ntc_charging(di);

    return status;
}

static ssize_t bq2419x_show_temperature_parameter(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    int read_reg[6] = {0};
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    read_reg[0] = di->temperature_cold;
    read_reg[1] = di->temperature_cool;
    read_reg[2] = di->temperature_5;
    read_reg[3] = di->temperature_10;
    read_reg[4] = di->temperature_warm;
    read_reg[5] = di->temperature_hot;

    snprintf(buf, PAGE_SIZE, "%-9d  %-9d  %-9d  %-9d  %-9d  %-9d",
    read_reg[0],read_reg[1],read_reg[2],read_reg[3],read_reg[4],read_reg[5]);

    return strlen(buf);
}

static ssize_t bq2419x_set_current(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < ICHG_512)
                   || (val > ICHG_3000))
        return -EINVAL;

    di->max_currentmA = val;
    di->max_cin_currentmA = val;

    return status;
}
/****************************************************************************
  Function:     bq2419x_mhl_set_iin
  Description:  MHL certification
  Input:        flag
                0:cin_limit = 500
                1:return normal charge cycle
  Output:       NA
  Return:       0:OK.
                Negtive:someting is wrong.
****************************************************************************/
int bq2419x_mhl_set_iin(int flag)
{
    struct bq2419x_device_info *di = g_di;
    static unsigned int mhl_iin_temp = 0;

    if (!di)
        return -EPERM;

    if(flag != 0 && flag != 1)
        return -EINVAL;

    hwlog_info("mhl_limit_flag = %d\n", flag);

    mhl_limit_cin_flag = flag;

    if(flag == 0 && di->cin_limit >= IINLIM_100) {
        mhl_iin_temp = di->cin_limit;
        di->cin_limit = IINLIM_100;
        bq2419x_config_input_source_reg(di);
        return 0;
    }

    if(flag == 1 && mhl_iin_temp != 0){
        di->cin_limit = mhl_iin_temp;
        bq2419x_config_input_source_reg(di);
        mhl_iin_temp = 0;
        return 0;
    }

    return -EINVAL;
}
static ssize_t bq2419x_set_temp_current_iin(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;

    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0)
                   || (val > ICHG_3000))
        return -EINVAL;

    if(val == 0)
        input_current_iin = di->max_cin_currentmA;
    else if(val <= IINLIM_100)
        input_current_iin = IINLIM_100;
    else
        input_current_iin = val;

    if(di->cin_limit > input_current_iin){
        di->cin_limit = input_current_iin;
        bq2419x_config_input_source_reg(di);
    }

    hwlog_info("set temp_current_iin = %d\n", input_current_iin);

    return status;
}

static ssize_t bq2419x_show_temp_current_iin(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    val = input_current_iin;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

static ssize_t bq2419x_set_temp_current_ichg(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > ICHG_3000))
        return -EINVAL;

    if(val == 0)
        input_current_ichg = di->max_currentmA;
    else if(val <= ICHG_100)
        input_current_ichg = ICHG_100;
    else
        input_current_ichg = val;

    if(di->currentmA > input_current_ichg){
        di->currentmA = input_current_ichg;
        bq2419x_config_current_reg(di);
    }

    hwlog_info("set temp_current_ichg = %d\n", input_current_ichg);

    return status;
}

static ssize_t bq2419x_show_temp_current_ichg(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    val = input_current_ichg;
    return snprintf(buf, PAGE_SIZE, "%lu\n", val);
}

static DEVICE_ATTR(enable_hz_mode, S_IWUSR | S_IRUGO,
                bq2419x_show_enable_hz_mode,
                bq2419x_set_enable_hz_mode);
static DEVICE_ATTR(dppm_voltage, S_IWUSR | S_IRUGO,
                bq2419x_show_dppm_voltage,
                bq2419x_set_dppm_voltage);
static DEVICE_ATTR(cin_limit, S_IWUSR | S_IRUGO,
                bq2419x_show_cin_limit,
                bq2419x_set_cin_limit);
static DEVICE_ATTR(regulation_voltage, S_IWUSR | S_IRUGO,
                bq2419x_show_regulation_voltage,
                bq2419x_set_regulation_voltage);
static DEVICE_ATTR(charge_current, S_IWUSR | S_IRUGO,
                bq2419x_show_charge_current,
                bq2419x_set_charge_current);
static DEVICE_ATTR(termination_current, S_IWUSR | S_IRUGO,
                bq2419x_show_termination_current,
                bq2419x_set_termination_current);
static DEVICE_ATTR(precharge_current, S_IWUSR | S_IRUGO,
                bq2419x_show_precharge_current,
                bq2419x_set_precharge_current);
static DEVICE_ATTR(enable_itermination, S_IWUSR | S_IRUGO,
                bq2419x_show_enable_itermination,
                bq2419x_set_enable_itermination);
static DEVICE_ATTR(enable_charger, S_IWUSR | S_IRUGO,
                bq2419x_show_enable_charger,
                bq2419x_set_enable_charger);
static DEVICE_ATTR(enable_batfet, S_IWUSR | S_IRUGO,
                bq2419x_show_enable_batfet,
                bq2419x_set_enable_batfet);
static DEVICE_ATTR(enable_cd, S_IWUSR | S_IRUGO,
                bq2419x_show_enable_cd,
                bq2419x_set_enable_cd);
static DEVICE_ATTR(charging, S_IWUSR | S_IRUGO,
                NULL,
                bq2419x_set_charging);
static DEVICE_ATTR(wakelock_enable, S_IWUSR | S_IRUGO,
                bq2419x_show_wakelock_enable,
                bq2419x_set_wakelock_enable);
static DEVICE_ATTR(chargelog, S_IWUSR | S_IRUGO,
                bq2419x_show_chargelog,
                NULL);
static DEVICE_ATTR(calling_limit, S_IWUSR | S_IRUGO,
                bq2419x_show_calling_limit,
                bq2419x_set_calling_limit);
static DEVICE_ATTR(compesation_resistor, S_IWUSR | S_IRUGO,
                bq2419x_show_compesation_resistor,
                bq2419x_set_compesation_resistor);
static DEVICE_ATTR(compesation_voltage, S_IWUSR | S_IRUGO,
                bq2419x_show_compesation_voltage,
                bq2419x_set_compesation_voltage);
static DEVICE_ATTR(temperature_parameter, S_IWUSR | S_IRUGO,
                bq2419x_show_temperature_parameter,
                bq2419x_set_temperature_parameter);
static DEVICE_ATTR(set_current, S_IWUSR | S_IRUGO,
                NULL,
                bq2419x_set_current);
static DEVICE_ATTR(temp_current_iin, S_IWUSR | S_IRUGO,
                bq2419x_show_temp_current_iin,
                bq2419x_set_temp_current_iin);
static DEVICE_ATTR(temp_current_ichg, S_IWUSR | S_IRUGO,
                bq2419x_show_temp_current_ichg,
                bq2419x_set_temp_current_ichg);
static DEVICE_ATTR(not_limit_chrg_flag, S_IWUSR | S_IRUGO,
                bq2419x_show_not_limit_chrg_flag,
                bq2419x_set_not_limit_chrg_flag);
static DEVICE_ATTR(not_stop_chrg_flag, S_IWUSR | S_IRUGO,
                bq2419x_show_not_stop_chrg_flag,
                bq2419x_set_not_stop_chrg_flag);

static struct attribute *bq2419x_attributes[] = {
    &dev_attr_enable_hz_mode.attr,
    &dev_attr_dppm_voltage.attr,
    &dev_attr_cin_limit.attr,
    &dev_attr_regulation_voltage.attr,
    &dev_attr_charge_current.attr,
    &dev_attr_precharge_current.attr,
    &dev_attr_termination_current.attr,
    &dev_attr_enable_itermination.attr,
    &dev_attr_enable_charger.attr,
    &dev_attr_enable_batfet.attr,
    &dev_attr_enable_cd.attr,
    &dev_attr_charging.attr,
    &dev_attr_wakelock_enable.attr,
    &dev_attr_chargelog.attr,
    &dev_attr_calling_limit.attr,
    &dev_attr_compesation_resistor.attr,
    &dev_attr_compesation_voltage.attr,
    &dev_attr_temperature_parameter.attr,
    &dev_attr_set_current.attr,
    &dev_attr_temp_current_iin.attr,
    &dev_attr_temp_current_ichg.attr,
    &dev_attr_not_limit_chrg_flag.attr,
    &dev_attr_not_stop_chrg_flag.attr,
    NULL,
};

static const struct attribute_group bq2419x_attr_group = {
    .attrs = bq2419x_attributes,
};

static int bq2419x_charger_probe(struct i2c_client *client,
                           const struct i2c_device_id *id)
{
    struct bq2419x_device_info *di;
    enum usb_charger_type plugin_stat = CHARGER_REMOVED;
    int ret = 0;
    u8 read_reg = 0;
    struct class * charge_class;
    struct device * new_dev = NULL;
    struct battery_charge_param_s charge_param = {0};

    di = kzalloc(sizeof(*di), GFP_KERNEL);
    if (!di) {
        hwlog_err("bq2419x_device_info is NULL!\n");
        return -ENOMEM;
    }

    di->dev = &client->dev;
    np = di->dev->of_node;
    di->client = client;
    i2c_set_clientdata(client, di);

    ret = bq2419x_read_byte(di, &read_reg, PART_REVISION_REG0A);
    if (ret < 0) {
        hwlog_err("chip not present at address %x\n",client->addr);
        ret = -EINVAL;
        goto err_kfree;
    }

    if (((read_reg & 0x38) == BQ2419x || (read_reg & 0x38) == BQ24192)
         && (client->addr == 0x6B))
        di->bqchip_version = BQ24192;

    if (di->bqchip_version == 0) {
        hwlog_debug("unknown bq chip\n");
        hwlog_debug("Chip address %x", client->addr);
        hwlog_debug("bq chip version reg value %x", read_reg);
        ret = -EINVAL;
        goto err_kfree;
    }

    ret = bq2419x_get_boardid_charge_parameter(di);
    if(!ret){
        di->max_voltagemV = 4208; //pdata->max_charger_voltagemV;
        di->max_currentmA = 1000;//pdata->max_charger_currentmA;
        di->max_cin_currentmA = 1200;//pdata->max_cin_limit_currentmA;
    }

    INIT_DELAYED_WORK(&di->otg_int_work, bq2419x_otg_int_work);
    di->gpio_cd = of_get_named_gpio(np,"gpio_cd",0);
    if(!gpio_is_valid(di->gpio_cd))
    {
        hwlog_err("gpio_cd is not valid\n");
        ret = -EINVAL;
        goto err_kfree;
    }
    di->gpio_int = of_get_named_gpio(np,"gpio_int",0);
    if(!gpio_is_valid(di->gpio_int))
    {
        hwlog_err("gpio_int is not valid\n");
        ret = -EINVAL;
        goto err_kfree;
    }
    udp_charge = of_property_read_bool(np,"udp_charge");

    if(udp_charge)
    {
        di->max_currentmA = 1000;
        di->max_cin_currentmA = 1200;
    }
    hwlog_info("max charge current check: max_currentmA = %d, max_cin_currentmA = %d.\n", di->max_currentmA, di->max_cin_currentmA);

    /*set gpio_074 to control CD pin to disable/enable bq24161 IC*/
    ret = gpio_request(di->gpio_cd, "charger_cd");
    if (ret) {
          hwlog_err("could not request gpio_cd\n");
          ret = -ENOMEM;
          goto err_io;
    }
    gpio_direction_output(di->gpio_cd, 0);

    ret = gpio_request(di->gpio_int,"charger_int");
    if(ret) {
        hwlog_err("could not request gpio_int\n");
        goto err_gpio_int;
    }
    gpio_direction_input(di->gpio_int);

    di->irq_int = gpio_to_irq(di->gpio_int);
    if(di->irq_int < 0) {
        hwlog_err("could not map gpio_int to irq\n");
        goto err_int_map_irq;
    }
    ret = request_irq(di->irq_int,bq2419x_irq_int_interrupt,IRQF_TRIGGER_FALLING,"charger_int_irq",di);
    if(ret) {
        hwlog_err("could not request irq_int\n");
        di->irq_int = -1;
        goto err_irq_int;
    }

    disable_irq(di->irq_int);
    irq_int_active = 0;

    di->voltagemV = di->max_voltagemV;
    di->currentmA = ICHG_512 ;
    di->cin_dpmmV = VINDPM_4360;
    di->cin_limit = IINLIM_500;
    di->sys_minmV = SYS_MIN_3500;
    di->prechrg_currentmA = IPRECHRG_256;
    di->term_currentmA = ITERM_MIN_128;
    di->watchdog_timer = WATCHDOG_80;
    di->chrg_timer = CHG_TIMER_12;
    di->bat_compohm = BAT_COMP_40;
    di->comp_vclampmV = VCLAMP_48;

    di->hz_mode = DIS_HIZ;
    di->chrg_config = EN_CHARGER;
    di->boost_lim = BOOST_LIM_500;
    di->enable_low_chg = DIS_FORCE_20PCT;
    di->enable_iterm = EN_TERM;
    di->enable_timer = EN_TIMER;
    di->enable_batfet = EN_BATFET;

    di->cd_active = 0;
    di->params.enable = 1;
    di->cfg_params = 1;
    di->enable_dpdm = DPDM_DIS;
    di->battery_full = 0;
    di->charge_full_count = 0;

    di->japan_charger = 0;
    di->is_two_stage_charger = 0;
    di->two_stage_charger_status = TWO_STAGE_CHARGE_FIRST_STAGE;
    di->first_stage_voltage = 4200;
    di->second_stage_voltage = 4100;
    di->is_disable_cool_temperature_charger = 0;
    di->high_temp_para =HIGH_TEMP_CP_U9701L;
    input_current_iin = di->max_cin_currentmA;
    input_current_ichg = di->max_currentmA;
    di->not_limit_chrg_flag = 0;
    di->not_stop_chrg_flag = 0;
    di->factory_flag = 0;

    bq2419x_get_boardid_japan_charge_parameter(di);

    bq2419x_config_power_on_reg(di);
    bq2419x_config_current_reg(di);
    bq2419x_config_prechg_term_current_reg(di);
    bq2419x_config_voltage_reg(di);
    bq2419x_config_term_timer_reg(di);
    bq2419x_config_thernal_regulation_reg(di);
    bq2419x_config_limit_temperature_parameter(di);

    wake_lock_init(&chrg_lock, WAKE_LOCK_SUSPEND, "bq2419x_chrg_wakelock");
    wake_lock_init(&stop_chrg_lock, WAKE_LOCK_SUSPEND, "bq2419x_stop_chrg_wakelock");

    /* Configuration parameters for 0C-10C, set the default to the parameters,
       and get the values from boardid files */
    di->design_capacity = BQ2419x_DEFAULT_CAPACITY;
    di->charge_in_temp_5 = DEFAULT_CHARGE_PARAM_LOW_TEMP;
    di->charge_in_temp_10 = DEFAULT_CHARGE_PARAM_LOW_TEMP;
    ret = hisi_battery_charge_param(&charge_param);
    if(ret){
        di->charge_in_temp_5 = charge_param.charge_in_temp_5;
        di->charge_in_temp_10 = charge_param.charge_in_temp_10;
        di->design_capacity = charge_param.design_capacity;
    }
    hwlog_info("%s: capacity is %d, charge_in_temp_5 is %d,"
           "charge_in_temp_10 is %d.\n",__FUNCTION__,di->design_capacity,
           di->charge_in_temp_5,di->charge_in_temp_10);

    INIT_DELAYED_WORK(&di->bq2419x_charger_work,
        bq2419x_charger_work);

    INIT_DELAYED_WORK(&di->bq2419x_usb_otg_work,
        bq2419x_usb_otg_work);

    INIT_WORK(&di->usb_work, bq2419x_usb_charger_work);

    ret = sysfs_create_group(&client->dev.kobj, &bq2419x_attr_group);
    if (ret) {
        hwlog_debug("could not create sysfs files\n");
        goto err_sysfs;
    }

    charge_class = class_create(THIS_MODULE, "hw_power");
    new_dev = device_create(charge_class, NULL, 0, "%s",
                            "charger");
    ret = sysfs_create_link(&new_dev->kobj, &client->dev.kobj, "charge_data");
    if(ret < 0){
        hwlog_err("create link to charge data fail.\n");
    }
    di->nb.notifier_call = bq2419x_usb_notifier_call;

    ret = hisi_charger_type_notifier_register(&di->nb);
    if (ret < 0) {
       hwlog_err("hisi_charger_type_notifier_register failed\n");
       goto err_hiusb;
    }

    plugin_stat = get_charger_name();
    if ((CHARGER_TYPE_USB == plugin_stat) || (CHARGER_TYPE_NON_STANDARD == plugin_stat)) {
        di->event = plugin_stat;
    } else if (CHARGER_TYPE_BC_USB == plugin_stat) {
        di->event = CHARGER_TYPE_BC_USB;
    } else if (CHARGER_TYPE_STANDARD == plugin_stat) {
        di->event = CHARGER_TYPE_STANDARD;
    } else if (USB_EVENT_OTG_ID == plugin_stat) {
        di->charger_source = POWER_SUPPLY_TYPE_BATTERY;
        di->event = USB_EVENT_OTG_ID;
    } else {
        di->event = CHARGER_REMOVED;
    }
    schedule_work(&di->usb_work);

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_CHARGER);
#endif

    if (!charger_dclient) {
        charger_dclient = dsm_register_client(&dsm_charger);
    }

    hwlog_err("bq2419x probe ok!\n");
    g_di = di; //h00121290 add for usb stub
    return 0;

err_hiusb:
    sysfs_remove_group(&client->dev.kobj, &bq2419x_attr_group);
err_sysfs:
    wake_lock_destroy(&chrg_lock);
    wake_lock_destroy(&stop_chrg_lock);
    free_irq(di->irq_int,di);
err_irq_int:
err_int_map_irq:
    gpio_free(di->gpio_int);
err_gpio_int:
    gpio_free(di->gpio_cd);
err_io:
err_kfree:
    kfree(di);
    di = NULL;
    np = NULL;

    return ret;
}

static int bq2419x_charger_remove(struct i2c_client *client)
{
    struct bq2419x_device_info *di = i2c_get_clientdata(client);

    hisi_charger_type_notifier_unregister(&di->nb);
    sysfs_remove_group(&client->dev.kobj, &bq2419x_attr_group);
    wake_lock_destroy(&chrg_lock);
    wake_lock_destroy(&stop_chrg_lock);
    cancel_delayed_work_sync(&di->bq2419x_charger_work);
    cancel_delayed_work_sync(&di->bq2419x_usb_otg_work);
    cancel_delayed_work_sync(&di->otg_int_work);
    gpio_set_value(di->gpio_cd, 1);
    gpio_free(di->gpio_cd);
    free_irq(di->irq_int,di);
    gpio_free(di->gpio_int);
    kfree(di);

    return 0;
}

static void bq2419x_charger_shutdown(struct i2c_client *client)
{
    struct bq2419x_device_info *di = i2c_get_clientdata(client);

    cancel_delayed_work_sync(&di->bq2419x_charger_work);
    cancel_delayed_work_sync(&di->bq2419x_usb_otg_work);
    cancel_delayed_work_sync(&di->otg_int_work);

    return;
}

static const struct i2c_device_id bq2419x_id[] = {
    { "bq2419x_charger", 0 },
    {},
};

#ifdef CONFIG_PM
static int bq2419x_charger_suspend(struct i2c_client *client,
              pm_message_t state)
{
    struct bq2419x_device_info *di = i2c_get_clientdata(client);

    if(di->charger_source == POWER_SUPPLY_TYPE_MAINS){
        if(di->battery_full){
            if (!wake_lock_active(&chrg_lock)){
                cancel_delayed_work(&di->bq2419x_charger_work);
                if((wakeup_timer_seconds > 300) || !wakeup_timer_seconds)
                    wakeup_timer_seconds = 300;
            }
        }
    }

    bq2419x_config_power_on_reg(di);
    return 0;
}

static int bq2419x_charger_resume(struct i2c_client *client)
{
    struct bq2419x_device_info *di = i2c_get_clientdata(client);

     if(di->charger_source == POWER_SUPPLY_TYPE_MAINS){
        bq2419x_config_voltage_reg(di);
        schedule_delayed_work(&di->bq2419x_charger_work, msecs_to_jiffies(0));
    }

    bq2419x_config_power_on_reg(di);
   return 0;
}
#else
#define bq2419x_charger_suspend       NULL
#define bq2419x_charger_resume        NULL
#endif /* CONFIG_PM */

MODULE_DEVICE_TABLE(i2c, bq24192);
static struct of_device_id bq2419x_charger_match_table[] =
{
    {
	.compatible = "huawei,bq2419x_charger",
	.data = NULL,
    },
    {
    },
};
static struct i2c_driver bq2419x_charger_driver = {
    .probe = bq2419x_charger_probe,
    .remove = bq2419x_charger_remove,
    .suspend = bq2419x_charger_suspend,
    .resume = bq2419x_charger_resume,
    .shutdown = bq2419x_charger_shutdown,
    .id_table = bq2419x_id,
    .driver = {
         .owner = THIS_MODULE,
         .name = "bq2419x_charger",
         .of_match_table = of_match_ptr(bq2419x_charger_match_table),
    },
};

static int __init bq2419x_charger_init(void)
{
    return i2c_add_driver(&bq2419x_charger_driver);
}
device_initcall_sync(bq2419x_charger_init);

static void __exit bq2419x_charger_exit(void)
{
    i2c_del_driver(&bq2419x_charger_driver);
}
module_exit(bq2419x_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HW Inc");

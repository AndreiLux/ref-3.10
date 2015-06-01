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
#include <mach/pmic_mt6325_sw.h>
#include <mach/mt6311.h>
#include <cust_pmic.h>
#include <cust_battery_meter.h>
//==============================================================================
// Extern
//==============================================================================
extern int Enable_BATDRV_LOG;
extern int g_R_BAT_SENSE;
extern int g_R_I_SENSE;
extern int g_R_CHARGER_1;
extern int g_R_CHARGER_2;
extern int g_bat_init_flag;


//==============================================================================
// PMIC-AUXADC related define
//==============================================================================
#define VOLTAGE_FULL_RANGE     	1800
#define VOLTAGE_FULL_RANGE_6311	3200
#define ADC_PRECISE		32768 	// 15 bits
#define ADC_PRECISE_CH7		131072 	// 17 bits
#define ADC_PRECISE_6311	4096 	// 12 bits

//==============================================================================
// PMIC-AUXADC global variable
//==============================================================================
kal_int32 count_time_out=10000;
struct wake_lock pmicAuxadc_irq_lock;
//static DEFINE_SPINLOCK(pmic_adc_lock);
static DEFINE_MUTEX(pmic_adc_mutex);

#if 1
//==============================================================================
// PMIC-AUXADC related API
//==============================================================================
void pmic_auxadc_init(void)
{
	kal_int32 adc_busy;
	wake_lock_init(&pmicAuxadc_irq_lock, WAKE_LOCK_SUSPEND, "pmicAuxadc irq wakelock");
    
	mt6325_upmu_set_rg_vref18_enb(0);
	mutex_lock(&pmic_adc_mutex);
	//change to suspend/resume
	pmic_read_interface_nolock(MT6325_AUXADC_ADC19, &adc_busy, MT6325_PMIC_RG_ADC_BUSY_MASK, MT6325_PMIC_RG_ADC_BUSY_SHIFT); 
	if (adc_busy==0) {
		pmic_config_interface_nolock(MT6325_AUXADC_CON19, 0x0, MT6325_PMIC_RG_ADC_DECI_GDLY_MASK, MT6325_PMIC_RG_ADC_DECI_GDLY_SHIFT);
	}
	mutex_unlock(&pmic_adc_mutex);
	mt6325_upmu_set_rg_adc_gps_status(1);
	mt6325_upmu_set_rg_adc_md_status(1);
	mt6325_upmu_set_rg_deci_gdly_vref18_selb(1);
	mt6325_upmu_set_rg_deci_gdly_sel_mode(1);

	//*set CLK as 26MHz CLK
	mt6325_upmu_set_rg_auxadc_ck_cksel(1);
	//1. set AUXADC CLK HW control
	mt6325_upmu_set_rg_auxadc_ck_pdn_hwen(1);
	//2. turn on AP & MD
	mt6325_upmu_set_rg_clksq_en_aux_ap(1);
	mt6325_upmu_set_auxadc_ck_aon(1);

	mt6325_upmu_set_strup_auxadc_rstb_sw(1);
	mt6325_upmu_set_strup_auxadc_rstb_sel(1);
	xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[pmic_auxadc_init] DONE\n");
}

kal_uint32  pmic_is_auxadc_ready(kal_int32 channel_num, upmu_adc_chip_list_enum chip_num, upmu_adc_user_list_enum user_num)
{
#if 1	
	kal_uint32 ret=0;
	kal_uint32 int_status_val_0=0;
	//unsigned long flags;
	
	//spin_lock_irqsave(&pmic_adc_lock, flags);
	if (chip_num ==MT6325_CHIP) {
		if (user_num == GPS ) {
			ret=mt6325_upmu_get_rg_adc_rdy_gps();
		} else if (user_num == MD ) {
			ret=mt6325_upmu_get_rg_adc_rdy_md();
		} else if (user_num == AP ) {
			pmic_read_interface(MT6325_AUXADC_ADC0+channel_num * 2,(&int_status_val_0),0x8000,0x0);
			ret = int_status_val_0 >> 15;
		}
	} else if (chip_num == MT6311_CHIP) {
		if (channel_num == 0)
			int_status_val_0 = mt6311_get_auxadc_adc_rdy_ch0();
		else if (channel_num == 1)
			int_status_val_0 = mt6311_get_auxadc_adc_rdy_ch1();
	}
	//spin_unlock_irqrestore(&pmic_adc_lock, flags);
	
	return ret;
#else
    return 0;
#endif    
}

kal_uint32  pmic_get_adc_output(kal_int32 channel_num, upmu_adc_chip_list_enum chip_num, upmu_adc_user_list_enum user_num)
{
#if 1	
	kal_uint32 ret=0;
	kal_uint32 int_status_val_0=0;
	//unsigned long flags;
	
	//spin_lock_irqsave(&pmic_adc_lock, flags);
	if (chip_num ==MT6325_CHIP) {
		if (user_num == GPS ) {
			int_status_val_0 = mt6325_upmu_get_rg_adc_out_gps();
			int_status_val_0 = (int_status_val_0 << 1) + mt6325_upmu_get_rg_adc_out_gps_lsb();
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "adc_out_gps 16_1 = %d, 0 = %d\n", mt6325_upmu_get_rg_adc_out_gps(), mt6325_upmu_get_rg_adc_out_gps_lsb());
		} else if (user_num == MD ) {
			int_status_val_0 = mt6325_upmu_get_rg_adc_out_md();
			int_status_val_0 = (int_status_val_0 << 1) + mt6325_upmu_get_rg_adc_out_md_lsb();
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "adc_out_md 16_1 = %d, 0 = %d\n", mt6325_upmu_get_rg_adc_out_md(), mt6325_upmu_get_rg_adc_out_md_lsb());
		} else if (user_num == AP ) {
			ret=pmic_read_interface(MT6325_AUXADC_ADC0+channel_num * 2,(&int_status_val_0),0x7fff,0x0);
		}
	} else if (chip_num == MT6311_CHIP) {
		if (channel_num == 0)
			int_status_val_0 = mt6311_get_auxadc_adc_out_ch0();
		else if (channel_num == 1)
			int_status_val_0 = mt6311_get_auxadc_adc_out_ch1();
	}

	//spin_unlock_irqrestore(&pmic_adc_lock, flags);
	return int_status_val_0;
#else
    return 0;
#endif    
}
extern void upmu_set_reg_value(kal_uint32 reg, kal_uint32 reg_val);
kal_uint32 PMIC_IMM_RequestAuxadcChannel(upmu_adc_chl_list_enum dwChannel)
{
#if 1
	kal_uint32 ret = 0;
	
		switch(dwChannel){ 
		case AUX_BATSNS_AP:
			mt6325_upmu_set_rg_ap_rqst_list(1<<7);
			break;
		case AUX_ISENSE_AP:
			mt6325_upmu_set_rg_ap_rqst_list(1<<6);
			break;
		case AUX_VCDT_AP:
			mt6325_upmu_set_rg_ap_rqst_list(1<<4);
			break;          
		case AUX_BATON_AP:
			upmu_set_reg_value(0x0a08,0x010b);
			upmu_set_reg_value(0x0f00,0x0005);
			//upmu_set_reg_value(0x0aba,0x500f);
			mt6325_upmu_set_rg_ap_rqst_list(1<<5);
			break;    
		case AUX_TSENSE_AP:
			mt6325_upmu_set_rg_ap_rqst_list(1<<3);
			break;
		case AUX_TSENSE_MD:
			mt6325_upmu_set_rg_ap_rqst_list(1<<2);
			break;
		case AUX_VACCDET_AP:
			//mt6325_upmu_set_rg_ap_rqst_list(1<<8);
			break;
		case AUX_VISMPS_AP:
			mt6325_upmu_set_rg_ap_rqst_list(1<<1);
			break;
		case AUX_ICLASSAB_AP:
			mt6325_upmu_set_rg_ap_rqst_list_rsv(1<<7);
			break;
		case AUX_HP_AP:
			mt6325_upmu_set_rg_ap_rqst_list_rsv(1<<6);
			break;
		case AUX_CH10_AP:
			mt6325_upmu_set_rg_ap_rqst_list_rsv(1<<5);
			break;
		case AUX_VBIF_AP:
			mt6325_upmu_set_rg_ap_rqst_list_rsv(1<<4);
			break;
		case AUX_CH12:
			mt6325_upmu_set_rg_ap_rqst_list_rsv(1<<3);
			break;
		case AUX_CH13:
			mt6325_upmu_set_rg_ap_rqst_list_rsv(1<<2);
			break;
		case AUX_CH14:
			mt6325_upmu_set_rg_ap_rqst_list_rsv(1<<1);
			break;
		case AUX_CH15:
			mt6325_upmu_set_rg_ap_rqst_list_rsv(1<<0);
			break;
		case AUX_ADCVIN0_MD:
			mt6325_upmu_set_rg_md_rqst(1);
			break;
		case AUX_ADCVIN0_GPS:
			mt6325_upmu_set_rg_gps_rqst(1);
			break;
		case AUX_CH0_6311:
			mt6311_set_auxadc_rqst_ch0(1);
			break;
		case AUX_CH1_6311:
			mt6311_set_auxadc_rqst_ch1(1);
			break;
			
		default:
		        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUXADC] Invalid channel value(%d)\n", dwChannel);
		        wake_unlock(&pmicAuxadc_irq_lock);
		        return -1;
		}

	return ret;
#else
	return 0;
#endif   
}

int PMIC_IMM_GetChannelNumber(upmu_adc_chl_list_enum dwChannel)
{
	kal_int32 channel_num;
	channel_num = (dwChannel & (AUXADC_CHANNEL_MASK << AUXADC_CHANNEL_SHIFT)) >> AUXADC_CHANNEL_SHIFT ;
	
	return channel_num;	
}

upmu_adc_chip_list_enum PMIC_IMM_GetChipNumber(upmu_adc_chl_list_enum dwChannel)
{
	upmu_adc_chip_list_enum chip_num;
	chip_num = (upmu_adc_chip_list_enum)(dwChannel & (AUXADC_CHIP_MASK << AUXADC_CHIP_SHIFT)) >> AUXADC_CHIP_SHIFT ;
	
	return chip_num;	
}

upmu_adc_user_list_enum PMIC_IMM_GetUserNumber(upmu_adc_chl_list_enum dwChannel)
{
	upmu_adc_user_list_enum user_num;
	user_num = (upmu_adc_user_list_enum)(dwChannel & (AUXADC_USER_MASK << AUXADC_USER_SHIFT)) >> AUXADC_USER_SHIFT ;
	
	return user_num;		
}




//==============================================================================
// PMIC-AUXADC 
//==============================================================================
int PMIC_IMM_GetOneChannelValue(upmu_adc_chl_list_enum dwChannel, int deCount, int trimd)
{
#if 1
	kal_int32 ret_data;    
	kal_int32 count=0;
	kal_int32 u4Sample_times = 0;
	kal_int32 u4channel=0;    
	kal_int32 adc_result_temp=0;
	kal_int32 r_val_temp=0;   
	kal_int32 adc_result=0;   
	kal_int32 channel_num;
	upmu_adc_chip_list_enum chip_num;
	upmu_adc_user_list_enum user_num;
	
	/*
	AUX_BATSNS_AP =		0x000,
	AUX_ISENSE_AP,
	AUX_VCDT_AP,
	AUX_BATON_AP,
	AUX_TSENSE_AP,
	AUX_TSENSE_MD =		0x005,
	AUX_VACCDET_AP =	0x007,
	AUX_VISMPS_AP =		0x00B,
	AUX_ICLASSAB_AP =	0x016,
	AUX_HP_AP =		0x017,
	AUX_VBIF_AP =		0x019,

	AUX_ADCVIN0_GPS = 	0x10C,
	AUX_ADCVIN0_MD =	0x10F,
	CH12-15 shared
	*/
	wake_lock(&pmicAuxadc_irq_lock);
	
	do
	{
		count=0;
	        ret_data=0;

		channel_num 	= PMIC_IMM_GetChannelNumber(dwChannel);
		chip_num 	= PMIC_IMM_GetChipNumber(dwChannel);
		user_num 	= PMIC_IMM_GetUserNumber(dwChannel);

		//xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "\n[PMIC_IMM_GetOneChannelValue] %d ch=%d, chip= %d, user= %d \n",dwChannel, channel_num, chip_num, user_num);

		if (chip_num == MT6325_CHIP) {
			if (user_num == MD ) {
				//Prerequisite settings before accessing AuxADC 
				mt6325_upmu_set_rg_clksq_en_aux_md(1);
				mt6325_upmu_set_rg_vref18_enb_md(0);	
			} else if (user_num == GPS) {
				mt6325_upmu_set_rg_clksq_en_aux_gps(1);
			}
		} else if (chip_num == MT6311_CHIP) {
			
		}
		
		if (chip_num == MT6325_CHIP) {
			mt6325_upmu_set_rg_ap_rqst_list(0);
			mt6325_upmu_set_rg_ap_rqst_list_rsv(0);
			mt6325_upmu_set_rg_md_rqst(0);
			mt6325_upmu_set_rg_gps_rqst(0);
		} else if (chip_num == MT6311_CHIP) {
			mt6311_set_auxadc_rqst_ch0(0);
			mt6311_set_auxadc_rqst_ch1(0);
		}

		PMIC_IMM_RequestAuxadcChannel(dwChannel);
		//udelay(10);
		while( pmic_is_auxadc_ready(channel_num, chip_num, user_num) != 1 ) {
			if( (count++) > count_time_out)
			{
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[IMM_GetOneChannelValue_PMIC] (%d) Time out!\n", dwChannel);
			break;
			}            
		}
		ret_data = pmic_get_adc_output(channel_num, chip_num, user_num);                
		
		//clear
		
		if (chip_num == MT6325_CHIP) {
			mt6325_upmu_set_rg_ap_rqst_list(0);
			mt6325_upmu_set_rg_ap_rqst_list_rsv(0);
			mt6325_upmu_set_rg_md_rqst(0);
			mt6325_upmu_set_rg_gps_rqst(0);
		} else if (chip_num == MT6311_CHIP) {
			mt6311_set_auxadc_rqst_ch0(0);
			mt6311_set_auxadc_rqst_ch1(0);
		}
	        u4channel += ret_data;
	
	        u4Sample_times++;
	
		//debug
		//xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[AUXADC] output data[%d]=%d\n", 
//			dwChannel, ret_data);
		if (chip_num == MT6325_CHIP) {
			if (user_num == MD) {
				//Prerequisite settings before accessing AuxADC 
				mt6325_upmu_set_rg_clksq_en_aux_md(0);
				mt6325_upmu_set_rg_vref18_enb_md(1);	
			} else if (user_num == GPS) {
				mt6325_upmu_set_rg_clksq_en_aux_gps(0);
			}
		}
	}while (u4Sample_times < deCount);

	/* Value averaging  */ 
	adc_result_temp = u4channel / deCount;
	
	switch(dwChannel){
	case AUX_BATSNS_AP:
		if (Enable_BATDRV_LOG == 2) {
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_BATSNS_AP]");
		}
		r_val_temp = 4;
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
		break;
	case AUX_ISENSE_AP:
		if (Enable_BATDRV_LOG == 2) {
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_ISENSE_AP]");
		}
		r_val_temp = 4;           
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
		break;
	case AUX_VCDT_AP:
		if (Enable_BATDRV_LOG == 2) {
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_VCDT_AP]");
		}
		r_val_temp = 1;
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
		break;          
	case AUX_BATON_AP:
		if (Enable_BATDRV_LOG == 2) {
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_BATON_AP]");
		}
		r_val_temp = 2;           
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
		break;    
	case AUX_TSENSE_AP:
		if (Enable_BATDRV_LOG == 2) {
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_TSENSE_AP]");
		}
		r_val_temp = 1;           
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
		break;
	case AUX_TSENSE_MD:
		if (Enable_BATDRV_LOG == 2) {
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_TSENSE_MD]");
		}
		r_val_temp = 1;           
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
		break;
	case AUX_VACCDET_AP:
		if (Enable_BATDRV_LOG == 2) {
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_VACCDET_AP]");
		}
		r_val_temp = 1;           
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
		break;
	case AUX_VISMPS_AP:
		if (Enable_BATDRV_LOG == 2) {
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_VISMPS_AP]");
		}
		r_val_temp = 1;           
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
		break;
	case AUX_ICLASSAB_AP:
		if (Enable_BATDRV_LOG == 2) {
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_ICLASSAB_AP]");
		}
		//r_val_temp = 1;           
		adc_result = adc_result_temp;
		break;
	case AUX_HP_AP:
		xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_HP_AP]");
		r_val_temp = 1;
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
		break;
	case AUX_CH10_AP:
		//xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_CH10_AP]");
		r_val_temp = 1;
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
		break;
	case AUX_VBIF_AP:
		//xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_VBIF_AP]");
		r_val_temp = 2;
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
		break;
	case AUX_ADCVIN0_MD:
		//xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_CH7_MD]");
		r_val_temp = 1;
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE_CH7;
		break;
	case AUX_ADCVIN0_GPS:
		//xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_CH7_GPS]");
		r_val_temp = 1;           
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE_CH7;
		break;
	case AUX_CH0_6311:
		//xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_CH0_6311]");
		r_val_temp = 1;           
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE_6311)/ADC_PRECISE_6311;
		break;
	case AUX_CH1_6311:
		//xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUX_CH1_6311]");
		r_val_temp = 1;       
		adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE_6311)/ADC_PRECISE_6311;
		break;
	default:
	    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUXADC] Invalid channel value(%d,%d)\n", dwChannel, trimd);
	    wake_unlock(&pmicAuxadc_irq_lock);
	    return -1;
	}
	if (Enable_BATDRV_LOG == 2) {
		xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "outputdata=%d, transfer data=%d, r_val=%d.\n", 
			adc_result_temp, adc_result, r_val_temp);
	}

	
	wake_unlock(&pmicAuxadc_irq_lock);
	
	return adc_result;
#else
	return 0;
#endif   
}

#else

void pmic_auxadc_init(void)
{
}

int PMIC_IMM_GetOneChannelValue(upmu_adc_chl_list_enum dwChannel, int deCount, int trimd)
{
    return 1234;
}

#endif

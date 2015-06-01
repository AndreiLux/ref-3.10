/******************************************************************************
 * mt6575_vibrator.c - MT6575 Android Linux Vibrator Device Driver
 * 
 * Copyright 2009-2010 MediaTek Co.,Ltd.
 * 
 * DESCRIPTION:
 *     This file provid the other drivers vibrator relative functions
 *
 ******************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <mach/mt_typedefs.h>
#include <cust_vibrator.h>
#include <mach/upmu_common_sw.h>
#include <mach/upmu_hw.h>
#include <linux/delay.h>
static int vibe_mode = 0;

extern S32 pwrap_read( U32  adr, U32 *rdata );
extern S32 pwrap_write( U32  adr, U32  wdata );

void vibr_Enable_HW(void)
{
	
	//printk("[vibrator]vibr_Enable \n");

	mt6325_upmu_set_rg_vibr_sw_mode(0);// [bit 3]: VIBR_SW_MODE   1=HW, 0=SW
	//mt6325_upmu_set_rg_vibr_vosel(7);  // VIBR_SEL,  101=2.8V, 110=3.0V, 111=3.3V
	mt6325_upmu_set_rg_vibr_fr_ori(1);  // VIBR_FR_ORI,  00=float, 01=forward, 10=braking, 11=backward
	//upmu_set_rg_vibr_mst_time();    //VIBR_MST_TIME,  00=1us, 01=2us, 10=4us, 11=8us

	mt6325_upmu_set_rg_vibr_en(1);     //[bit 1]: VIBR_EN,  1=enable
}

void vibr_Disable_HW(void)
{
	//printk("[vibrator]vibr_Disable \n");

	switch(vibe_mode)
    {
        case 1:
            mt6325_upmu_set_rg_vibr_fr_ori(2);  //VIBR_FR_ORI,  00=float, 01=forward, 10=braking, 11=backward
            mt6325_upmu_set_rg_vibr_fr_ori(3);  // VIBR_FR_ORI,  00=float, 01=forward, 10=braking, 11=backward

            msleep(30); //delay 30ms
            mt6325_upmu_set_rg_vibr_fr_ori(2);  // VIBR_FR_ORI,  00=float, 01=forward, 10=braking, 11=backward
            mt6325_upmu_set_rg_vibr_en(0);     //[bit 1]: VIBR_EN,  1=enable
        case 0:
        default:
            mt6325_upmu_set_rg_vibr_en(0);     //[bit 1]: VIBR_EN,  1=enable
            break;
    }
    vibe_mode = 0;
}


/******************************************
* Set RG_VIBR_VOSEL	Output voltage select
*  hw->vib_vol:  Voltage selection
* 3'b000: 1.3V
* 3'b001: 1.5V
* 3'b010: 1.8V
* 3'b011: 2.0V
* 3'b100: 2.5V
* 3'b101: 2.8V
* 3'b110: 3.0V
* 3'b111: 3.3V
*******************************************/

void vibr_power_set(void)
{
#ifdef CUST_VIBR_VOL
	struct vibrator_hw* hw = get_cust_vibrator_hw();	
	printk("[vibrator]vibr_init: vibrator set voltage = %d\n", hw->vib_vol);
	mt6325_upmu_set_rg_vibr_vosel(hw->vib_vol);
#endif
}

struct vibrator_hw* mt_get_cust_vibrator_hw(void)
{
	return get_cust_vibrator_hw();
}

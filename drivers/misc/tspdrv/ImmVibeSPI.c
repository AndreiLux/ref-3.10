/*
** =========================================================================
** File:
**     ImmVibeSPI.c
**
** Description:
**     Device-dependent functions called by Immersion TSP API
**     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
** Portions Copyright (c) 2008-2010 Immersion Corporation. All Rights Reserved.
**
** This file contains Original Code and/or Modifications of Original Code
** as defined in and that are subject to the GNU Public License v2 -
** (the 'License'). You may not use this file except in compliance with the
** License. You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
** TouchSenseSales@immersion.com.
**
** The Original Code and all software distributed under the License are
** distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
** EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
** INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
** FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
** the License for the specific language governing rights and limitations
** under the License.
** =========================================================================
*/

/* Debug Mask setting */
#define VIBRATOR_DEBUG_PRINT   (0)
#define VIBRATOR_ERROR_PRINT   (1)
#define VIBRATOR_INFO_PRINT    (0)

#if (VIBRATOR_INFO_PRINT)
#define INFO_MSG(fmt, args...) \
			printk(KERN_INFO "vib: %s() " \
				fmt, __FUNCTION__, ##args);
#else
#define INFO_MSG(fmt, args...)
#endif

#if (VIBRATOR_DEBUG_PRINT)
#define DEBUG_MSG(fmt, args...) \
			printk(KERN_INFO "vib: %s() " \
				fmt, __FUNCTION__, ##args);
#else
#define DEBUG_MSG(fmt, args...)
#endif

#if (VIBRATOR_ERROR_PRINT)
#define ERR_MSG(fmt, args...) \
			printk(KERN_ERR "vib: %s() " \
				fmt, __FUNCTION__, ##args);
#else
#define ERR_MSG(fmt, args...)
#endif

/*
/*USE THE QPNP-VIBRATOR START*/
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/regmap.h>
#include <linux/mfd/max77807/max77807.h>
#include <linux/mfd/max77807/max77807-vibrator.h>
#include "../../staging/android/timed_output.h"
#include <linux/board_lge.h>
#include <linux/platform_data/gpio-odin.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/regulator/machine.h>
#include <linux/io.h>
#include <linux/stat.h>
#include <linux/mfd/max77807/max77807-charger.h>

#include <linux/pwm.h>
#include <linux/odin_pwm.h>

#define PCLK_NS  30			/*50MHz*/
#define VIB_PERIOD	 0x400

#define DRIVER_NAME    MAX77807_MOTOR_NAME

#define VIB_ON 1
#define VIB_OFF 0

#define M2SH  __CONST_FFS

/* HAPTIC REGISTERS */
#define REG_STAUTS			0x00
#define REG_CONFIG1			0x01
#define REG_CONFIG2			0x02
#define REG_CONFIG_CHNL		0x03
#define REG_CONFIG_CYC1		0x04
#define REG_CONFIG_CYC2		0x05
#define REG_CONFIG_PER1		0x06
#define REG_CONFIG_PER2		0x07
#define REG_CONFIG_PER3		0x08
#define REG_CONFIG_PER4		0x09
#define REG_CONFIG_DUTY1		0x0A
#define REG_CONFIG_DUTY2		0X0B
#define REG_CONFIG_PWM1		0x0C
#define REG_CONFIG_PWM2		0x0D
#define REG_CONFIG_PWM3		0x0E
#define REG_CONFIG_PWM4		0x0F
#define REG_CONFIG_REV		0x10
/*Haptic driver on*/
#define REG_CONFIG_LSCNFG				0x2B
#define REG_CONFIG_HAPTIC_ENABLE		0x01
#define REG_CONFIG_HAPTIC_DISABLE		0x00


/* HAPTIC REGISTER BITS */
#define STATUS_HBUSY			BIT (0)

#define CONFIG1_INV			BIT (7)
#define CONFIG1_CONT			BIT (6)
#define CONFIG1_MSU			BITS(5,3)
#define CONFIG1_SCF			BITS(2,0)

#define CONFIG2_MODE			BIT (7)
#define CONFIG2_MEN			BIT (6)
#define CONFIG2_HTYP			BIT (5)
#define CONFIG2_PDIV			BITS(1,0)

#define CONFIG_CHNL_CYCA		BITS(7,6)
#define CONFIG_CHNL_SIGPA	BITS(5,4)
#define CONFIG_CHNL_SIGDCA	BITS(3,2)
#define CONFIG_CHNL_PWMDCA	BITS(1,0)

#define CONFIG_CYC1_CYC0		BITS(7,4)
#define CONFIG_CYC1_CYC1		BITS(3,0)
#define CONFIG_CYC2_CYC2		BITS(7,4)
#define CONFIG_CYC2_CYC3		BITS(3,0)

#define CONFIG_PER1_SIGP0	BITS(7,0)
#define CONFIG_PER2_SIGP1	BITS(7,0)
#define CONFIG_PER3_SIGP2	BITS(7,0)
#define CONFIG_PER4_SIGP3	BITS(7,0)

#define CONFIG_DUTY1_SIGDC0	BITS(7,4)
#define CONFIG_DUTY1_SIGDC1	BITS(3,0)
#define CONFIG_DUTY2_SIGDC2	BITS(7,4)
#define CONFIG_DUTY2_SIGDC3	BITS(3,0)

#define CONFIG_PWM1_PWMDC0	BITS(5,0)
#define CONFIG_PWM2_PWMDC1	BITS(5,0)
#define CONFIG_PWM3_PWMDC2	BITS(5,0)
#define CONFIG_PWM4_PWMDC3	BITS(5,0)

#define REV_MTR_REV			BITS(7,0)

/*Haptic driver on*/
#define LSCNFG_LSEN			BIT (7)
#define HAPTIC_ENABLE		BIT (6)

#define DEVICE_NAME		"lge_sm100"

#ifdef IMMVIBESPIAPI
#undef IMMVIBESPIAPI
#endif
#define IMMVIBESPIAPI static

/*
** This SPI supports only one actuator.
*/
#define NUM_ACTUATORS 1

static bool g_bAmpEnabled = false;

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex);
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer);


#include "touch_fops.c"

extern struct max77807_charger *global_charger;

struct max77807_vib
{
	struct timed_output_dev timed_dev;
	struct regmap *regmap;
	char *regulator_name;
	struct regulator *vreg_l12;
	struct pwm_device *pd;
	struct delayed_work vibrator_off_work;
};

struct max77807_vib *g_Vib;

static DEFINE_MUTEX(vib_lock);

bool sm100_flag = false; //default is QPNP(PMIC)

static int pre_force = -1;

#define INIT_DURATION 1000
#define DEFAULT_PWM 100
static int sm100_pwm_set(struct max77807_vib  *max77807_vib, int duty)
{
	struct pwm_device *pd = NULL;
	unsigned int duty_ns=0;
	unsigned int period_ns=0;
	unsigned int nforce = duty;
	int ret=0;
	int con_force =0;

	if(pre_force != duty)
	{
//		printk("start  duty : %d\n", duty);
		pre_force = duty;
	}

	pd=g_Vib->pd;

	if(pd == NULL) 	{
		ERR_MSG("pwm device is null\n");
		return;
	}

	con_force = nforce * 47 / 100;

	if(nforce == -127)
		duty_ns = 1;
	else if(nforce <= 0)
		duty_ns = 0;
	else
		duty_ns = (256*4*15) + (con_force * 256) +1;

//		duty_ns = (nforce+109)*4;
//	else if(nforce != 0)
//		nforce = ((float)(nforce+127))*0.92;
//	if(nforce == 0)
//		duty_ns = 0;
//	else
//		duty_ns = (nforce+1) * 4; // 100 ~ 255
//	duty_ns = duty_ns * PCLK_NS;

	if(force_data != -128)
	{
		printk("[VIB] Debug On : %d\n", force_data);
		con_force = force_data * 47 / 100;
		duty_ns = (256*4*15) + (con_force * 256) +1;
	}

	period_ns = VIB_PERIOD * PCLK_NS;

	pwm_config(pd,duty_ns, period_ns); //set duty_cycle

	if (nforce)
	    ret=pwm_enable(pd);
	else
		pwm_disable(pd);

	if( ret ){
		ERR_MSG("pwm_enable fail\n");
		return;
	}
}

static int sm100_power_set(struct max77807_vib *max77807_vib, int enable)
{
	int ret=0;

	DEBUG_MSG("start  enable : %d\n",enable);

	mutex_lock(&vib_lock);
	if (enable) {
		ret = regulator_enable(g_Vib->vreg_l12);
		if (ret < 0)
			ERR_MSG("regulator_enable failed\n");
	} else {
		if (regulator_is_enabled(g_Vib->vreg_l12) > 0) {
			ret = regulator_disable(g_Vib->vreg_l12);
			if (ret < 0)
				ERR_MSG("regulator_disable failed\n");
		}
	}
	mutex_unlock(&vib_lock);
	return ret;
}

static int sm100_ic_enable_set(struct max77807_vib *max77807_vib, int on)
{
	DEBUG_MSG("enable:%d\n", enable);

	if(on) {
		regmap_update_bits(g_Vib->regmap, REG_CONFIG2,
						HAPTIC_ENABLE,  REG_CONFIG_HAPTIC_ENABLE << M2SH(HAPTIC_ENABLE));
	}else {
	       regmap_update_bits(g_Vib->regmap, REG_CONFIG2,
							HAPTIC_ENABLE,	REG_CONFIG_HAPTIC_DISABLE << M2SH(HAPTIC_ENABLE));
	}
	return 0;
}
void sm100_haptic_set(int on)
{
	DEBUG_MSG("start  on : %d\n",on);

	if(!global_charger) return;

	if(on) {
		/* Haptic drvier on */
		regmap_update_bits(global_charger->io->regmap, REG_CONFIG_LSCNFG,
						LSCNFG_LSEN, 1 << M2SH(LSCNFG_LSEN));

	}else {
		regmap_update_bits(global_charger->io->regmap, REG_CONFIG_LSCNFG,
						LSCNFG_LSEN, 0 << M2SH(LSCNFG_LSEN));
	}
}

static void vibrator_off(struct work_struct *work)
{
	printk("vibrator off");
	sm100_ic_enable_set( g_Vib, 0);
	sm100_pwm_set(0,0);
	sm100_haptic_set(0);
	sm100_power_set(g_Vib,0);

}

static void sm100_enable(struct timed_output_dev *dev, int value)
{
	printk("vibrator on\n");

	int vib_duration = 0;

	if(value == 0)
		return;
	else if(value == 1)
		vib_duration = INIT_DURATION;
	else
		vib_duration = value;

	cancel_delayed_work( &g_Vib->vibrator_off_work);

	sm100_haptic_set(1);
	sm100_power_set(g_Vib,1);
	sm100_ic_enable_set( g_Vib, 1);
	sm100_pwm_set(0,DEFAULT_PWM);

	schedule_delayed_work( &g_Vib->vibrator_off_work, msecs_to_jiffies(vib_duration));
}

static int sm100_get_status(struct timed_output_dev *dev)
{
	return sm100_flag;
}

static int sm100_hw_init(struct max77807_vib *max77807_vib, struct max77807_vib_platform_data *pdata)
{
	unsigned int value;

	value = pdata->mode << M2SH(CONFIG2_MODE) |
			pdata->htype << M2SH(CONFIG2_HTYP) |
			pdata->pdiv << M2SH(CONFIG2_PDIV);

	return regmap_write(g_Vib->regmap, REG_CONFIG2, value);
}


#ifdef CONFIG_OF
static struct max77807_vib_platform_data * sm100_parse_dt(struct device *dev, struct max77807_vib *vib_data)
{
	struct device_node *nproot = dev->parent->of_node;
	struct device_node *np;
	struct max77807_vib_platform_data *pdata;
	int ret = 0;

	DEBUG_MSG("start\n");
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);

	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	np = of_find_node_by_name(nproot, "vibrator");

	if (unlikely(np == NULL))
	{
		dev_err(dev, "vibrator node not found\n");
		return ERR_PTR(-EINVAL);
	}

	ret = of_property_read_u32(np, "mode",&pdata->mode);
	ret = of_property_read_u32(np, "htype",&pdata->htype);
	ret = of_property_read_u32(np, "pdiv", &pdata->pdiv);
	ret = of_property_read_string(np, "regulator-name", &vib_data->regulator_name);
	return pdata;
}

static struct of_device_id sm100_match_table[] = {
    { .compatible = "maxim,"MAX77807_MOTOR_NAME },
    { },
};
#endif
struct max77807_vib sm100_vibrator = {
	.timed_dev.name = "vibrator",
	.timed_dev.enable = sm100_enable,
	.timed_dev.get_time = sm100_get_status,
};


static int sm100_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77807_dev *chip = dev_get_drvdata(dev->parent);
        struct max77807_io *io = max77807_get_io(chip);
	struct max77807_vib_platform_data *pdata;
	//struct max77807_vib *max77807_vib;
	int ret;

	printk("immersion vibrator driver probe\n");

	platform_set_drvdata(pdev, &sm100_vibrator);
	g_Vib = (struct max77807_vib *)platform_get_drvdata(pdev);

#ifdef CONFIG_OF
	pdata = sm100_parse_dt(dev, g_Vib);
	g_Vib->pd= pwm_get(dev, "pwm-vib");

	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
#else
	pdata = dev_get_platdata(dev);
#endif
	g_Vib->vreg_l12 = regulator_get(dev, g_Vib->regulator_name);

	if (IS_ERR(g_Vib->vreg_l12)) {
		ERR_MSG("%s: regulator get of Dialog failed (%ld)\n",
				__func__, PTR_ERR(g_Vib->vreg_l12));
		g_Vib->vreg_l12 = NULL;
	}

	pdev->dev.init_name = g_Vib->timed_dev.name;

	g_Vib->regmap = io->regmap;

	ret = sm100_hw_init(g_Vib, pdata);

	INIT_DELAYED_WORK(&g_Vib->vibrator_off_work, vibrator_off);

	ret = timed_output_dev_register(&g_Vib->timed_dev);

	if (IS_ERR_VALUE(ret)){
		timed_output_dev_unregister(&g_Vib->timed_dev);
		return -ENODEV;
	}

	sm100_flag = true;

	return 0;
}

static int sm100_remove(struct platform_device *pdev)
{
	sm100_flag = false;
	return 0;
}

static void sm100_shutdown(struct platform_device *pdev)
{
	if (g_bAmpEnabled)
			ImmVibeSPI_ForceOut_AmpDisable(0);

}

static int sm100_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (g_bAmpEnabled)
		ImmVibeSPI_ForceOut_AmpDisable(0);
	return 0;
}

static int sm100_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver sm100_driver = {
	.probe = sm100_probe,
	.remove = sm100_remove,
	.shutdown = sm100_shutdown,
	.suspend = sm100_suspend,
	.resume = sm100_resume,
	.driver = {
		.name = DEVICE_NAME,
#ifdef CONFIG_OF
		.of_match_table = sm100_match_table,
#endif
	},
};
/*USE THE SM100 END*/



/*
** Called to disable amp (disable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
    if (g_bAmpEnabled)
    {
		if(sm100_flag) {
		    printk("g_bAmpEnabled:%d\n", g_bAmpEnabled);
		    sm100_ic_enable_set( g_Vib, 0);
	            sm100_pwm_set(0, 0);
	            sm100_haptic_set(0);
	            sm100_power_set(g_Vib,0);
		} else {
			INFO_MSG("\n");
		}

		g_bAmpEnabled = false;
    }

    return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex, VibeInt8 nForce)
{
    if (!g_bAmpEnabled)
    {
		if(sm100_flag) {
			printk("g_bAmpEnabled:%d\n", g_bAmpEnabled);
			sm100_haptic_set(1);
			sm100_power_set(g_Vib,1);
			//sm100_pwm_set(1, 0); //MSM GP CLK update bit issue.
                        sm100_ic_enable_set( g_Vib, 1);

			pre_force = -1;

		}
        g_bAmpEnabled = true;
    }

    return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to set PWM freq, disable amp, etc...
*/

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{

	int rc;
	rc = platform_driver_register(&sm100_driver);

    INFO_MSG("\n");

    g_bAmpEnabled = true;   /* to force ImmVibeSPI_ForceOut_AmpDisable disabling the amp */

    /*
    ** Disable amp.
    ** If multiple actuators are supported, please make sure to call
    ** ImmVibeSPI_ForceOut_AmpDisable for each actuator (provide the actuator index as
    ** input argument).
    */
    ImmVibeSPI_ForceOut_AmpDisable(0);

	touch_fops_init();

    return VIBE_S_SUCCESS;
}

/*
** Called at termination time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
    INFO_MSG("\n");

    /*
    ** Disable amp.
    ** If multiple actuators are supported, please make sure to call
    ** ImmVibeSPI_ForceOut_AmpDisable for each actuator (provide the actuator index as
    ** input argument).
    */
    ImmVibeSPI_ForceOut_AmpDisable(0);

    platform_driver_unregister(&sm100_driver);
    return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set PWM duty cycle
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer)
{
    VibeInt8 nForce;

//    g_bStarted = true;

    switch (nOutputSignalBitDepth)
    {
        case 8:
            /* pForceOutputBuffer is expected to contain 1 byte */
            if (nBufferSizeInBytes != 1) return VIBE_E_FAIL;

            nForce = pForceOutputBuffer[0];
            break;
        case 16:
            /* pForceOutputBuffer is expected to contain 2 byte */
            if (nBufferSizeInBytes != 2) return VIBE_E_FAIL;

            /* Map 16-bit value to 8-bit */
            nForce = ((VibeInt16*)pForceOutputBuffer)[0] >> 8;
            break;
        default:
            /* Unexpected bit depth */
            return VIBE_E_FAIL;
    }
	// nForce range: SM100: -127~127,  PMIC:0~127
    if (nForce <= 0)
    {
		if(sm100_flag && nForce < 0)
		{
			sm100_pwm_set(1, nForce); //MSM GP CLK update bit issue.
		}
	    else ImmVibeSPI_ForceOut_AmpDisable(nActuatorIndex);
    }
    else
    {
	//nForce = 127 - nForce;
		ImmVibeSPI_ForceOut_AmpEnable(nActuatorIndex, nForce);

		if(sm100_flag) {
	        sm100_pwm_set(1, nForce); //MSM GP CLK update bit issue.
		} else {
			INFO_MSG("\n");
		}
    }
    return VIBE_S_SUCCESS;
}

/*
** Called to set force output frequency parameters
*/
#if 0
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
    /* This function is not called for ERM device */

    return VIBE_S_SUCCESS;
}
#endif

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
#if 0   /* The following code is provided as a sample. Please modify as required. */
	INFO_MSG("\n");
    if ((!szDevName) || (nSize < 1)) return VIBE_E_FAIL;

    strncpy(szDevName, "W7", nSize-1);
    szDevName[nSize - 1] = '\0';    /* make sure the string is NULL terminated */
#endif

    return VIBE_S_SUCCESS;
}

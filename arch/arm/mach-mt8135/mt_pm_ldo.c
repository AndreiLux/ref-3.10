
#include <asm/io.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/xlog.h>

#include <asm/uaccess.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_pm_ldo.h>
#include <drivers/misc/mediatek/power/mt8135/pmic.h>

/****************************************************************
 * GLOBAL DEFINATION
 ****************************************************************/
ROOTBUS_HW g_MT_PMIC_BusHW;

/*******************************************************************
 * Extern Variable DEFINATIONS
 *******************************************************************/

/**********************************************************************
 * Extern FUNCTION DEFINATIONS
 *******************************************************************/

/**********************************************************************
* Debug Message Settings
*****************************************************************/
#if 1
#define MSG(evt, fmt, args...) \
do {    \
    if ((DBG_PMAPI_##evt) & DBG_PMAPI_MASK) { \
	xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", fmt, ##args); \
    } \
} while (0)

#define MSG_FUNC_ENTRY(f)    MSG(ENTER, "<PMAPI FUNC>: %s\n", __func__)
#else
#define MSG(evt, fmt, args...) do {} while (0)
#define MSG_FUNC_ENTRY(f)       do {} while (0)
#endif

/****************************************************************
 * FUNCTION DEFINATIONS
 *******************************************************************/
int first_power_on_flag = 1;

static kal_int32(*pmic_power_controller)(enum PMIC_POWER_CTRL_CMD cmd, void *data);
bool power_control_init_flag = FALSE;
static struct power_vosel_data ldo_vosel;
static struct power_vocal_data ldo_vocal;
static struct power_enable_data ldo_enable;
static DEFINE_MUTEX(pm_ldo_lock);

static void get_pmic_control(void)
{
	pmic_power_controller = power_control_interface;
	power_control_init_flag = TRUE;
}

bool hwPowerOn(MT65XX_POWER powerId, MT65XX_POWER_VOLTAGE powerVolt, char *mode_name)
{
	UINT32 i = 0;
	int j = 0, k = 0;

	if (power_control_init_flag == FALSE) {
		get_pmic_control();
		if (pmic_power_controller == NULL)
			return FALSE;
	}

	mutex_lock(&pm_ldo_lock);
	if (first_power_on_flag == 1) {
		for (j = 0; j < MT65XX_POWER_COUNT_END; j++) {
			for (k = 0; k < MAX_DEVICE; k++) {
				sprintf(g_MT_PMIC_BusHW.Power[j].mod_name[k], "%s", NON_OP);
			}
			g_MT_PMIC_BusHW.Power[j].dwPowerCount = 0;
		}
		first_power_on_flag = 0;
		xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[hwPowerOn] init done.\r\n");
	}
#if 1
	if (powerId >= MT65XX_POWER_COUNT_END) {
		MSG(PMIC, "[MT65XX PMU] Error!! powerId is wrong\r\n");
		mutex_unlock(&pm_ldo_lock);
		return FALSE;
	}
	for (i = 0; i < MAX_DEVICE; i++) {
		/* xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[hwPowerOn] %d,%s,%d\r\n", i, g_MT_PMIC_BusHW.Power[powerId].mod_name[i], g_MT_PMIC_BusHW.Power[powerId].dwPowerCount); */

		pr_debug("[hwPowerOn] %d,%s(%d),%d\r\n",
			    i, g_MT_PMIC_BusHW.Power[powerId].mod_name[i], powerId,
			    g_MT_PMIC_BusHW.Power[powerId].dwPowerCount);

		if (!strcmp(g_MT_PMIC_BusHW.Power[powerId].mod_name[i], NON_OP)) {
			/* MSG(PMIC,"[%s] acquire powerId:%d index:%d mod_name: %s\r\n", */
			/* __FUNCTION__,powerId, i, mode_name); */
			sprintf(g_MT_PMIC_BusHW.Power[powerId].mod_name[i], "%s", mode_name);
			break;
		}
		/* already it */
#if 0
		else if (!strcmp(g_MT_PMIC_BusHW.Power[powerId].mod_name[i], mode_name)) {
			MSG(CG, "[%d] Power already register\r\n", powerId);
		}
#endif
	}
	g_MT_PMIC_BusHW.Power[powerId].dwPowerCount++;
	/* We've already enable this LDO before */
	if (g_MT_PMIC_BusHW.Power[powerId].dwPowerCount > 1) {
		xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC",
			    "[hwPowerOn] g_MT_PMIC_BusHW.Power[powerId].dwPowerCount (%d) > 1\r\n",
			    g_MT_PMIC_BusHW.Power[powerId].dwPowerCount);
		mutex_unlock(&pm_ldo_lock);
		return TRUE;
	}
#endif
	/* Turn on PMU LDO */
	/* MSG(CG,"[%d] PMU LDO Enable\r\n",powerId ); */
	/* xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[hwPowerOn] enable %d by %s \r\n", powerId, mode_name); */

	if ((powerId == MT65XX_POWER_LDO_VMC)
	    || (powerId == MT65XX_POWER_LDO_VMCH)
	    || (powerId == MT65XX_POWER_LDO_VEMC_3V3)
	    || (powerId == MT65XX_POWER_LDO_VCAMD)
	    || (powerId == MT65XX_POWER_LDO_VCAMIO)
	    || (powerId == MT65XX_POWER_LDO_VCAMAF)
	    || (powerId == MT65XX_POWER_LDO_VGP4)
	    || (powerId == MT65XX_POWER_LDO_VGP5)
	    || (powerId == MT65XX_POWER_LDO_VGP6)
	    || (powerId == MT65XX_POWER_LDO_VIBR)
	    || (powerId == MT65XX_POWER_LDO_VAST)
	    || (powerId == MT65XX_POWER_LDO_VRF28)
	    || (powerId == MT65XX_POWER_LDO_VA)
	    || (powerId == MT65XX_POWER_LDO_VCAMA)
	    ) {
		ldo_vosel.powerId = powerId;
		ldo_vosel.powerVolt = powerVolt;
		pmic_power_controller(PMIC_POWER_CMD_VOSEL, &ldo_vosel);
	}

	ldo_enable.powerId = powerId;
	ldo_enable.powerEnable = KAL_TRUE;
	pmic_power_controller(PMIC_POWER_CMD_ENABLE, &ldo_enable);
	mutex_unlock(&pm_ldo_lock);

	return TRUE;
}
EXPORT_SYMBOL(hwPowerOn);

bool hwPowerDown(MT65XX_POWER powerId, char *mode_name)
{
	UINT32 i;
#if 1
	BOOL bFind = FALSE;
	if (power_control_init_flag == FALSE) {
		get_pmic_control();
		if (pmic_power_controller == NULL)
			return FALSE;
	}

	if (powerId >= MT65XX_POWER_COUNT_END) {
		MSG(PMIC, "%s:%s:%d powerId:%d is wrong\r\n", __FILE__, __func__,
		    __LINE__, powerId);
		return FALSE;
	}

	mutex_lock(&pm_ldo_lock);
	if (g_MT_PMIC_BusHW.Power[powerId].dwPowerCount == 0) {
		MSG(PMIC, "%s:%s:%d powerId:%d (g_MT_PMIC_BusHW.dwPowerCount[powerId] = 0)\r\n",
		    __FILE__, __func__, __LINE__, powerId);
		mutex_unlock(&pm_ldo_lock);
		return FALSE;
	}
	for (i = 0; i < MAX_DEVICE; i++) {
		/* xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[hwPowerDown] %d,%s,%d\r\n", i, g_MT_PMIC_BusHW.Power[powerId].mod_name[i], g_MT_PMIC_BusHW.Power[powerId].dwPowerCount); */

		pr_debug("[hwPowerDown] %d,%s(%d),%d\r\n",
			i, g_MT_PMIC_BusHW.Power[powerId].mod_name[i], powerId,
			g_MT_PMIC_BusHW.Power[powerId].dwPowerCount);

		if (!strcmp(g_MT_PMIC_BusHW.Power[powerId].mod_name[i], mode_name)) {
			/* MSG(PMIC,"[%s] powerId:%d index:%d mod_name: %s\r\n", */
			/* __FUNCTION__,powerId, i, mode_name); */
			sprintf(g_MT_PMIC_BusHW.Power[powerId].mod_name[i], "%s", NON_OP);
			bFind = TRUE;
			break;
		}
	}
	if (!bFind) {
		MSG(PMIC, "[%s] Cannot find [%d] master is [%s]\r\n", __func__, powerId,
		    mode_name);
		mutex_unlock(&pm_ldo_lock);
		return TRUE;
	}
	g_MT_PMIC_BusHW.Power[powerId].dwPowerCount--;
	if (g_MT_PMIC_BusHW.Power[powerId].dwPowerCount > 0) {
		xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC",
			    "[hwPowerDown] g_MT_PMIC_BusHW.Power[powerId].dwPowerCount (%d) > 0\r\n",
			    g_MT_PMIC_BusHW.Power[powerId].dwPowerCount);
		mutex_unlock(&pm_ldo_lock);
		return TRUE;
	}
#endif
	/* Turn off PMU LDO */
	/* MSG(CG,"[%d] PMU LDO Disable\r\n",powerId ); */
	/* xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC", "[hwPowerDown] disable %d by %s \r\n", powerId, mode_name); */
	ldo_enable.powerId = powerId;
	ldo_enable.powerEnable = KAL_FALSE;
	pmic_power_controller(PMIC_POWER_CMD_ENABLE, &ldo_enable);
	mutex_unlock(&pm_ldo_lock);

	return TRUE;
}
EXPORT_SYMBOL(hwPowerDown);

bool hwPowerCal(MT65XX_POWER powerId, enum MT65XX_POWER_VOLCAL powermVolt, char *mode_name)
{
	bool ret;
	if (power_control_init_flag == FALSE) {
		get_pmic_control();
		if (pmic_power_controller == NULL)
			return FALSE;
	}

	if (powerId >= MT65XX_POWER_COUNT_END) {
		MSG(PMIC, "%s:%s:%d powerId:%d is wrong\r\n", __FILE__, __func__,
		    __LINE__, powerId);
		return FALSE;
	}

	switch (powerId) {
	case MT65XX_POWER_LDO_VGP4:
		ldo_vocal.powerId = powerId;
		ldo_vocal.powermVolt = powermVolt;
		pmic_power_controller(PMIC_POWER_CMD_VOCAL, &ldo_vocal);
		ret = TRUE;
		break;
	case MT65XX_POWER_LDO_VGP6:
		ldo_vocal.powerId = powerId;
		ldo_vocal.powermVolt = powermVolt;
		pmic_power_controller(PMIC_POWER_CMD_VOCAL, &ldo_vocal);
		ret = TRUE;
		break;
	default:
		xlog_printk(ANDROID_LOG_INFO, "Power/PMIC",
			"Not support [hwPowerCal] %d,%s,%d\r\n",
			powerId, mode_name, powermVolt);
		ret = FALSE;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(hwPowerCal);


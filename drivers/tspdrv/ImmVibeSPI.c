/*
** =============================================================================
**
** File: ImmVibeSPI.c
**
** Description:
**     Device-dependent functions called by Immersion TSP API
**     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
** $Revision$
**
** Copyright (c) 2012 Immersion Corporation. All Rights Reserved.
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
**
** =============================================================================
*/

#include <linux/i2c.h>
#include <linux/wakelock.h>
#include <linux/io.h>
#include <linux/pwm.h>
#include <linux/gpio.h>
//#include <mach/gpio-herring.h>  // kwang test

#ifdef IMMVIBESPIAPI
#undef IMMVIBESPIAPI
#endif
#define IMMVIBESPIAPI static

/*
** This SPI supports only one actuator.
*/
#define NUM_ACTUATORS       1

static bool g_bAmpEnabled = false;
extern void imm_vibrator_en(bool en);
extern void imm_vibrator_pwm(int force);

/*
** Called to disable amp (disable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
    if (g_bAmpEnabled)
    {
        g_bAmpEnabled = false;

	imm_vibrator_en(g_bAmpEnabled);
    }

    return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
    if (!g_bAmpEnabled)
    {
        g_bAmpEnabled = true;

	imm_vibrator_en(g_bAmpEnabled);
    }

    return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to set PWM freq, disable amp, etc...
*/
#if 0
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
    //DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_Initialize.\n"));
    /* Init LRA controller */
    if (gpio_request(GPIO_VIBTONE_EN1, "vibrator-en") >= 0)
    {
        s3c_gpio_cfgpin(GPIO_VIBTONE_PWM, GPD0_TOUT_1);

        g_pPWMDev = pwm_request(1, "vibrator-pwm");
        if (IS_ERR(g_pPWMDev))
        {
            gpio_free(GPIO_VIBTONE_EN1);
            return VIBE_E_FAIL;
        }
    } else {
        return VIBE_E_FAIL;
    }

    /* Disable amp */
    g_bAmpEnabled = true;   /* to force ImmVibeSPI_ForceOut_AmpDisable disabling the amp */
    ImmVibeSPI_ForceOut_AmpDisable(0);

    return VIBE_S_SUCCESS;
}
#endif

/*
** Called at termination time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
    if(g_bAmpEnabled)
    {
	    g_bAmpEnabled = false;
	    /* Disable amp */
	    imm_vibrator_en(false);
    }

    return VIBE_S_SUCCESS;
}

/*
** Called to set force output frequency parameters
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
    return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set force output, and enable amp if required
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer)
{
    /* Empty buffer is okay */
    if (0 == nBufferSizeInBytes)
		return VIBE_S_SUCCESS;

    if ((0 == nActuatorIndex) && (8 == nOutputSignalBitDepth) && (1 == nBufferSizeInBytes))
    {
        VibeInt8 force = pForceOutputBuffer[0];

        if (force == 0)
        {
            imm_vibrator_pwm(0);
        } else
        {
            if(ImmVibeSPI_ForceOut_AmpEnable(0))
                return VIBE_E_FAIL;
            imm_vibrator_pwm(force);
        }
    } else
    {
        return VIBE_E_FAIL;
    }

    return VIBE_S_SUCCESS;
}

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
    char szDeviceName[VIBE_MAX_DEVICE_NAME_LENGTH] = "TS3000 Device";

    //DbgOut((KERN_DEBUG "ImmVibeSPI_Device_GetName.\n"));

    if ((strlen(szDeviceName) + 8) >= nSize)
        return VIBE_E_FAIL;

    sprintf(szDevName, "%s TS3000 Devoce", szDeviceName);

    return VIBE_S_SUCCESS;
}

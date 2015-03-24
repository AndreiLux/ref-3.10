#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>

#include <mach/devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include "tz_cross/trustzone.h"
#include "tz_cross/ta_test.h"
#include "tz_cross/ta_mem.h"
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
/* #include "kree_int.h" */

#include "tz_cross/ta_drmkey.h"
#include "tz_cross/keyblock.h"

#include "tz_cross/hdmi_ta.h"
#include "hdmi_ca.h"
#include "mhl_dbg.h"

KREE_SESSION_HANDLE ca_hdmi_handle = NULL;

bool fgCaHDMICreate(void)
{
	TZ_RESULT tz_ret = 0;

	tz_ret = KREE_CreateSession(TZ_TA_HDMI_UUID, &ca_hdmi_handle);
	if (tz_ret != TZ_RESULT_SUCCESS) {
		/* Should provide strerror style error string in UREE. */
		pr_info("Create ca_hdmi_handle Error: %d\n", tz_ret);
		return FALSE;
	}
	pr_info("[HDMI]Create ca_hdmi_handle ok: %d\n", tz_ret);
	return TRUE;
}

bool fgCaHDMIClose(void)
{
	TZ_RESULT tz_ret = 0;

	tz_ret = KREE_CloseSession(ca_hdmi_handle);
	if (tz_ret != TZ_RESULT_SUCCESS) {
		/* Should provide strerror style error string in UREE. */
		pr_info("Close ca_hdmi_handle Error: %d\n", tz_ret);
		return FALSE;
	}
	pr_info("[HDMI]Close ca_hdmi_handle ok: %d\n", tz_ret);
	return TRUE;
}

void vCaHDMIWriteReg(unsigned int u4addr, unsigned int u4data)
{
	TZ_RESULT tz_ret = 0;
	MTEEC_PARAM param[2];

	if (ca_hdmi_handle == NULL) {
		pr_info("[HDMI] TEE ca_hdmi_handle=NULL\n");
		return;
	}

	param[0].value.a = u4addr & 0x3FF;
	param[0].value.b = 0;
	param[1].value.a = u4data;
	param[1].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_handle, HDMI_TA_WRITE_REG,
				     TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT), param);
	MHL_DRV_LOG("W:%X=%X\n", param[0].value.a, param[1].value.a);

	if (tz_ret != TZ_RESULT_SUCCESS) {
		pr_info("[HDMI]CA HDMI_TA_WRITE_REG err:%X\n", tz_ret);
	}

}

void vCaDPI1WriteReg(unsigned int u4addr, unsigned int u4data)
{
	TZ_RESULT tz_ret = 0;
	MTEEC_PARAM param[2];

	if (ca_hdmi_handle == NULL) {
		pr_info("[HDMI_DPI1] TEE ca_dpi1_handle=NULL\n");
		return;
	}

	param[0].value.a = u4addr & 0x3FF;
	param[0].value.b = 0;
	param[1].value.a = u4data;
	param[1].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_handle, HDMI_TA_DPI1_WRITE_REG,
				     TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT), param);
	MHL_DRV_LOG("W:%X=%X\n", param[0].value.a, param[1].value.a);

	if (tz_ret != TZ_RESULT_SUCCESS) {
		pr_info("[HDMI_DPI1]CA HDMI_TA_DPI1_WRITE_REG err:%X\n", tz_ret);
	}

}

bool fgCaHDMIInstallHdcpKey(unsigned char *pdata, unsigned int u4Len)
{
	TZ_RESULT tz_ret = 0;
	MTEEC_PARAM param[2];
	unsigned char *ptr;
	unsigned int i;

	if (ca_hdmi_handle == NULL) {
		pr_info("[HDMI] TEE ca_hdmi_handle=NULL\n");
		return FALSE;
	}

	pr_info("[HDMI]fgCaHDMIInstallHdcpKey,%d\n", u4Len);

	if (u4Len >= 512) {
		return FALSE;
	}
	ptr = (unsigned char *)kmalloc(u4Len, GFP_KERNEL);

	for (i = 0; i < u4Len; i++)
		ptr[i] = pdata[i];

	param[0].mem.buffer = ptr;
	param[0].mem.size = u4Len;
	param[1].value.a = u4Len;
	param[1].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_handle, HDMI_TA_INSTALL_HDCP_KEY,
				     TZ_ParamTypes2(TZPT_MEM_INPUT, TZPT_VALUE_INPUT), param);
	if (tz_ret != TZ_RESULT_SUCCESS) {
		pr_info("[HDMI]CA HDMI_TA_INSTALL_HDCP_KEY err:%X\n", tz_ret);
		return FALSE;
	}

	kfree(ptr);
	return TRUE;
}

bool fgCaHDMIGetAKsv(unsigned char *pdata)
{
	TZ_RESULT tz_ret = 0;
	MTEEC_PARAM param[1];
	unsigned char *ptr;
	unsigned char i;

	if (ca_hdmi_handle == NULL) {
		pr_info("[HDMI] TEE ca_hdmi_handle=NULL\n");
		return FALSE;
	}

	ptr = (unsigned char *)kmalloc(5, GFP_KERNEL);
	param[0].mem.buffer = ptr;
	param[0].mem.size = 5;
	tz_ret = KREE_TeeServiceCall(ca_hdmi_handle, HDMI_TA_GET_HDCP_AKSV,
				     TZ_ParamTypes1(TZPT_MEM_OUTPUT), param);
	if (tz_ret != TZ_RESULT_SUCCESS) {
		pr_info("[HDMI]CA HDMI_TA_GET_HDCP_AKSV err:%X\n", tz_ret);
		return FALSE;
	}
	for (i = 0; i < 5; i++)
		pdata[i] = ptr[i];

	pr_info("[HDMI]hdcp aksv : %x %x %x %x %x\n",
		pdata[0], pdata[1], pdata[2], pdata[3], pdata[4]);
	kfree(ptr);
	return TRUE;
}

bool fgCaHDMILoadHDCPKey(void)
{
	TZ_RESULT tz_ret = 0;
	MTEEC_PARAM param[1];

	if (ca_hdmi_handle == NULL) {
		pr_info("[HDMI] TEE ca_hdmi_handle=NULL\n");
		return FALSE;
	}

	param[0].value.a = 0;
	param[0].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_handle, HDMI_TA_LOAD_HDCP_KEY,
				     TZ_ParamTypes1(TZPT_VALUE_INPUT), param);

	if (tz_ret != TZ_RESULT_SUCCESS) {
		pr_info("[HDMI]CA HDMI_TA_LOAD_HDCP_KEY err:%X\n", tz_ret);
		return FALSE;
	}
	return TRUE;
}

bool fgCaHDMIHDCPReset(bool fgen)
{
	TZ_RESULT tz_ret = 0;
	MTEEC_PARAM param[1];

	if (ca_hdmi_handle == NULL) {
		pr_info("[HDMI] TEE ca_hdmi_handle=NULL\n");
		return FALSE;
	}

	if (fgen)
		param[0].value.a = 1;
	else
		param[0].value.a = 0;
	param[0].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_handle, HDMI_TA_HDCP_RST,
				     TZ_ParamTypes1(TZPT_VALUE_INPUT), param);

	if (tz_ret != TZ_RESULT_SUCCESS) {
		pr_info("[HDMI]CA HDMI_TA_HDCP_RST err:%X\n", tz_ret);
		return FALSE;
	}
	return TRUE;
}

bool fgCaHDMIHDCPEncEn(bool fgen)
{
	TZ_RESULT tz_ret = 0;
	MTEEC_PARAM param[1];

	if (ca_hdmi_handle == NULL) {
		pr_info("[HDMI] TEE ca_hdmi_handle=NULL\n");
		return FALSE;
	}

	if (fgen)
		param[0].value.a = 1;
	else
		param[0].value.a = 0;
	param[0].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_handle, HDMI_TA_HDCP_ENC_EN,
				     TZ_ParamTypes1(TZPT_VALUE_INPUT), param);

	if (tz_ret != TZ_RESULT_SUCCESS) {
		pr_info("[HDMI]CA HDMI_TA_HDCP_ENC_EN err:%X\n", tz_ret);
		return FALSE;
	}
	return TRUE;
}

bool fgCaHDMIVideoUnMute(bool fgen)
{
	TZ_RESULT tz_ret = 0;
	MTEEC_PARAM param[1];

	if (ca_hdmi_handle == NULL) {
		pr_info("[HDMI] TEE ca_hdmi_handle=NULL\n");
		return FALSE;
	}

	if (fgen)
		param[0].value.a = 1;
	else
		param[0].value.a = 0;
	param[0].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_handle, HDMI_TA_VID_UNMUTE,
				     TZ_ParamTypes1(TZPT_VALUE_INPUT), param);

	if (tz_ret != TZ_RESULT_SUCCESS) {
		pr_info("[HDMI]CA HDMI_TA_VID_UNMUTE err:%X\n", tz_ret);
		return FALSE;
	}
	return TRUE;
}

bool fgCaHDMIAudioUnMute(bool fgen)
{
	TZ_RESULT tz_ret = 0;
	MTEEC_PARAM param[1];

	if (ca_hdmi_handle == NULL) {
		pr_info("[HDMI] TEE ca_hdmi_handle=NULL\n");
		return FALSE;
	}

	if (fgen)
		param[0].value.a = 1;
	else
		param[0].value.a = 0;
	param[0].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_handle, HDMI_TA_AUD_UNMUTE,
				     TZ_ParamTypes1(TZPT_VALUE_INPUT), param);

	if (tz_ret != TZ_RESULT_SUCCESS) {
		pr_info("[HDMI]CA HDMI_TA_AUD_UNMUTE err:%X\n", tz_ret);
		return FALSE;
	}
	return TRUE;
}

#endif

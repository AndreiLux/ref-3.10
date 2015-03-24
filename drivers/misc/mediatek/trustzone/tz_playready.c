#include "tz_playready.h"

#ifdef TZ_PLAYREADY_SECURETIME_SUPPORT

uint32_t TEE_update_pr_time_intee(KREE_SESSION_HANDLE session, KREE_SESSION_HANDLE mem_session)
{
uint32_t *shm_p = NULL;
KREE_SHAREDMEM_PARAM shm_param;
KREE_SHAREDMEM_HANDLE shm_handle;
uint32_t *shm_p_cur = NULL;
KREE_SHAREDMEM_PARAM shm_param_cur;
KREE_SHAREDMEM_HANDLE shm_handle_cur;
MTEEC_PARAM param[4];
uint32_t paramTypes;
TZ_RESULT ret;
uint32_t file_result = PR_TIME_FILE_OK_SIGN;
struct file *file = NULL;
UINT64 u8Offset = 0;

int err = -ENODEV;
struct rtc_time tm;
DRM_UINT64 time_count64;
unsigned long time_count;
struct rtc_device *rtc;


shm_p = kmalloc(sizeof(struct TZ_JG_SECURECLOCK_INFO), GFP_KERNEL);
shm_p_cur = kmalloc(sizeof(DRM_UINT64), GFP_KERNEL);

shm_param.buffer = shm_p;
shm_param.size = sizeof(struct TZ_JG_SECURECLOCK_INFO);
shm_param_cur.buffer = shm_p_cur;
shm_param_cur.size = sizeof(DRM_UINT64);



ret = KREE_RegisterSharedmem(mem_session, &shm_handle, &shm_param);
if (ret != TZ_RESULT_SUCCESS) {
	pr_err("    KREE_RegisterSharedmem shm_handle Error: %s\n", TZ_GetErrorString(ret));
	return ret;
}
ret = KREE_RegisterSharedmem(mem_session, &shm_handle_cur, &shm_param_cur);
if (ret != TZ_RESULT_SUCCESS) {
	pr_err("    KREE_RegisterSharedmem shm_handle_cur Error: %s\n", TZ_GetErrorString(ret));
	KREE_UnregisterSharedmem(mem_session, shm_handle);
	return ret;
}

rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);

if (rtc == NULL) {
	pr_err("%s: unable to open rtc device (%s)\n",
			__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
	goto err_open;
}

paramTypes = TZ_ParamTypes3(TZPT_MEMREF_INPUT, TZPT_MEMREF_INPUT, TZPT_VALUE_OUTPUT);
param[0].memref.handle = (uint32_t) shm_handle;
param[0].memref.offset = 0;
param[0].memref.size = sizeof(struct TZ_JG_SECURECLOCK_INFO);
param[1].memref.handle = (uint32_t) shm_handle_cur;
param[1].memref.offset = 0;
param[1].memref.size = sizeof(DRM_UINT64);

file = FILE_Open(PR_TIME_FILE_SAVE_PATH, O_RDWR, S_IRWXU);
if (file) {
	FILE_Read(file, (void *)shm_p, sizeof(struct TZ_JG_SECURECLOCK_INFO), &u8Offset);
	filp_close(file, NULL);
} else {
	file_result = NO_SECURETD_FILE;
	goto err_read;
}

err = rtc_read_time(rtc, &tm);
if (err) {
	dev_err(rtc->dev.parent,
			"hctosys: unable to read the hardware clock\n");
	goto err_read;
}

err = rtc_valid_tm(&tm);
if (err) {
	dev_err(rtc->dev.parent, "hctosys: invalid date/time\n");
	goto err_invalid;
}

rtc_tm_to_time(&tm, &time_count);
time_count64 = (DRM_UINT64)time_count;
memcpy(shm_p_cur, (char *) &time_count64, sizeof(DRM_UINT64));

ret = KREE_TeeServiceCall(session, TZCMD_PLAYREADY_SET_CURRENT_COUNTER, paramTypes, param);
if (ret != TZ_RESULT_SUCCESS)
	pr_err("ServiceCall TZCMD_PLAYREADY_SET_CURRENT_COUNTER error %d\n", ret);

if (param[2].value.a == PR_TIME_FILE_ERROR_SIGN) {
	file_result = PR_TIME_FILE_ERROR_SIGN;
	pr_err("ServiceCall TZCMD_PLAYREADY_SET_CURRENT_COUNTER file_result %d\n", file_result);
}
else if (param[2].value.a == PR_TIME_FILE_OK_SIGN) {
	file_result = PR_TIME_FILE_OK_SIGN;
	pr_err("ServiceCall TZCMD_PLAYREADY_SET_CURRENT_COUNTER file_result %d\n", file_result);
}

err_invalid:
err_read:
rtc_class_close(rtc);
err_open:
ret = KREE_UnregisterSharedmem(mem_session, shm_handle);
if (ret != TZ_RESULT_SUCCESS)
	pr_err("    KREE_UnregisterSharedmem Error: %s\n", TZ_GetErrorString(ret));

ret = KREE_UnregisterSharedmem(mem_session, shm_handle_cur);
if (ret != TZ_RESULT_SUCCESS)
	pr_err("    KREE_UnregisterSharedmem Error: %s\n", TZ_GetErrorString(ret));

if (shm_p != NULL)
	kfree(shm_p);
if (shm_p_cur != NULL)
	kfree(shm_p_cur);

if (file_result == PR_TIME_FILE_OK_SIGN)
	return ret;
else
	return file_result;

}

uint32_t TEE_update_pr_time_infile(KREE_SESSION_HANDLE session, KREE_SESSION_HANDLE mem_session)
{
uint32_t *shm_p;
KREE_SHAREDMEM_PARAM shm_param;
KREE_SHAREDMEM_HANDLE shm_handle;
MTEEC_PARAM param[4];
uint32_t paramTypes;
TZ_RESULT ret;
struct file *file = NULL;
UINT64 u8Offset = 0;

shm_p = kmalloc(sizeof(struct TZ_JG_SECURECLOCK_INFO), GFP_KERNEL);

shm_param.buffer = shm_p;
shm_param.size = sizeof(struct TZ_JG_SECURECLOCK_INFO);
ret = KREE_RegisterSharedmem(mem_session, &shm_handle, &shm_param);
if (ret != TZ_RESULT_SUCCESS) {
	pr_err("    KREE_RegisterSharedmem Error: %s\n", TZ_GetErrorString(ret));
	return ret;
}
paramTypes = TZ_ParamTypes3(TZPT_MEMREF_OUTPUT, TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT);
param[0].memref.handle = (uint32_t) shm_handle;
param[0].memref.offset = 0;
param[0].memref.size = sizeof(struct TZ_JG_SECURECLOCK_INFO);
param[1].value.a = 0;

ret = KREE_TeeServiceCall(session, TZCMD_PLAYREADY_GET_CURRENT_COUNTER, paramTypes, param);
if (ret != TZ_RESULT_SUCCESS) {
	pr_err("ServiceCall error %d\n", ret);
	goto tz_error;
}

file = FILE_Open(PR_TIME_FILE_SAVE_PATH, O_RDWR|O_CREAT, S_IRWXU);
if (file) {
	FILE_Write(file, (void *)shm_p, sizeof(struct TZ_JG_SECURECLOCK_INFO), &u8Offset);
	filp_close(file, NULL);
} else {
	pr_err("FILE_Open PR_TIME_FILE_SAVE_PATH return NULL\n");
}

tz_error:

ret = KREE_UnregisterSharedmem(mem_session, shm_handle);
if (ret != TZ_RESULT_SUCCESS)
	pr_err("    KREE_UnregisterSharedmem Error: %s\n", TZ_GetErrorString(ret));

if (shm_p != NULL)
	kfree(shm_p);

return ret;

}



uint32_t TEE_Icnt_pr_time(KREE_SESSION_HANDLE session, KREE_SESSION_HANDLE mem_session)
{
uint32_t *shm_p;
KREE_SHAREDMEM_PARAM shm_param;
KREE_SHAREDMEM_HANDLE shm_handle;
MTEEC_PARAM param[4];
uint32_t paramTypes;
TZ_RESULT ret;
unsigned long time_count = 1392967151;

struct rtc_device *rtc = NULL;
struct rtc_time tm;
int err = -ENODEV;


shm_p = kmalloc(sizeof(struct TM_PR), GFP_KERNEL);

shm_param.buffer = shm_p;
shm_param.size = sizeof(struct TM_PR);
ret = KREE_RegisterSharedmem(mem_session, &shm_handle, &shm_param);
if (ret != TZ_RESULT_SUCCESS) {
	pr_err("    KREE_RegisterSharedmem Error: %s\n", TZ_GetErrorString(ret));
	return ret;
}
paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_MEMREF_OUTPUT);
param[1].memref.handle = (uint32_t) shm_handle;
param[1].memref.offset = 0;
param[1].memref.size = sizeof(struct TM_PR);
rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);

if (rtc == NULL) {
	pr_err("%s: unable to open rtc device (%s)\n",
				__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
	goto err_open;
}

err = rtc_read_time(rtc, &tm);
if (err) {
	dev_err(rtc->dev.parent,
				"hctosys: unable to read the hardware clock\n");
	goto err_read;

}

err = rtc_valid_tm(&tm);
if (err) {
	dev_err(rtc->dev.parent,
				"hctosys: invalid date/time\n");
	goto err_invalid;
}

rtc_tm_to_time(&tm, &time_count);
#if 0
pr_info("securetime increase result: %d %d %d %d %d %d %d\n", tm.tm_yday, tm.tm_year, tm.tm_mon
	, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
#endif	
param[0].value.a = time_count;

ret = KREE_TeeServiceCall(session, TZCMD_PLAYREADY_INC_CURRENT_COUNTER, paramTypes, param);
if (ret != TZ_RESULT_SUCCESS)
	pr_err("ServiceCall error %d\n", ret);

#if 0
pr_info("securetime increase result: %d %d %d %d %d %d %d\n", ((struct TM_PR *) shm_p)->tm_yday
	, ((struct TM_PR *) shm_p)->tm_year, ((struct TM_PR *) shm_p)->tm_mon, ((struct TM_PR *) shm_p)->tm_mday
	, ((struct TM_PR *) shm_p)->tm_hour, ((struct TM_PR *) shm_p)->tm_min, ((struct TM_PR *) shm_p)->tm_sec);
#endif

err_read:
err_invalid:
rtc_class_close(rtc);
err_open:

ret = KREE_UnregisterSharedmem(mem_session, shm_handle);
if (ret != TZ_RESULT_SUCCESS)
	pr_err("    KREE_UnregisterSharedmem Error: %s\n", TZ_GetErrorString(ret));


if (shm_p != NULL)
	kfree(shm_p);

return ret;
}

#define THREAD_COUNT_FREQ 10
#define THREAD_SAVEFILE_VALUE (1*60*60)
static int check_count;
static KREE_SESSION_HANDLE icnt_session;
static KREE_SESSION_HANDLE mem_session;

int update_securetime_thread(void *data)
{
TZ_RESULT ret;
unsigned int update_ret = 0;
uint32_t nsec = THREAD_COUNT_FREQ;

ret = KREE_CreateSession(TZ_TA_PLAYREADY_UUID, &icnt_session);
if (ret != TZ_RESULT_SUCCESS) {
	pr_err("CreateSession error %d\n", ret);
	return 1;
}
ret = KREE_CreateSession(TZ_TA_MEM_UUID, &mem_session);
if (ret != TZ_RESULT_SUCCESS) {
	pr_err("Create memory session error %d\n", ret);
	ret = KREE_CloseSession(icnt_session);
	return ret;
}

set_freezable();

schedule_timeout_interruptible(HZ * nsec);

update_ret = TEE_update_pr_time_intee(icnt_session, mem_session);
if (update_ret == NO_SECURETD_FILE || update_ret == PR_TIME_FILE_ERROR_SIGN) {
	TEE_update_pr_time_infile(icnt_session, mem_session);
	TEE_update_pr_time_intee(icnt_session, mem_session);
}

for (;;) {
		if (kthread_should_stop())
			break;
		if (try_to_freeze())
			continue;
		schedule_timeout_interruptible(HZ * nsec);
		if (icnt_session && mem_session) {
			TEE_Icnt_pr_time(icnt_session, mem_session);
			check_count += THREAD_COUNT_FREQ;
		if ((check_count < 0) || (check_count > THREAD_SAVEFILE_VALUE)) {
			TEE_update_pr_time_infile(icnt_session, mem_session);
			check_count = 0;
		}
}


}

ret = KREE_CloseSession(icnt_session);
if (ret != TZ_RESULT_SUCCESS)
	pr_err("CloseSession error %d\n", ret);


ret = KREE_CloseSession(mem_session);
if (ret != TZ_RESULT_SUCCESS)
	pr_err("Close memory session error %d\n", ret);

return 0;
}
#endif


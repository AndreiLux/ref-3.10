/*
 * mutex.c
 *
 *  Created on: Jun 25, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "mutex.h"
#include "../util/log.h"
#include "../util/error.h"
#ifdef __XMK__
#include <sys/process.h>
#include "errno.h"
#endif

int mutex_initialize(void* pHandle)
{
#ifdef __XMK__
	/* *pHandle = (void *) PTHREAD_MUTEX_INITIALIZER; */
	if (pthread_mutex_init((pthread_mutex_t*)(pHandle), NULL) != 0)
	{
		error_set(ERR_CANNOT_CREATE_MUTEX);
		LOG_ERROR("Cannot create mutex");
		return FALSE; /* kernel error */
	}
	return TRUE;
#else
	error_set(ERR_NOT_IMPLEMENTED);
	return FALSE;
#endif
}

int mutex_destruct(void* pHandle)
{
#ifdef __XMK__

	if (pthread_mutex_destroy((pthread_mutex_t*)(pHandle)) != 0)
	{
		error_set(ERR_CANNOT_DESTROY_MUTEX);
		LOG_ERROR("Cannot destroy mutex");
		return FALSE; /* kernel error */
	}
	return TRUE;
#else
	error_set(ERR_NOT_IMPLEMENTED);
	LOG_ERROR("Not implemented");
	return FALSE;
#endif
}

int mutex_lock_(void* pHandle)
{
#ifdef __XMK__
	if (pthread_mutex_lock_((pthread_mutex_t*)(pHandle)) != 0)
	{
		error_set(ERR_CANNOT_LOCK_MUTEX);
		LOG_ERROR("Cannot lock mutex");
		return FALSE;
	}
	return TRUE;
#else
	error_set(ERR_NOT_IMPLEMENTED);
	LOG_ERROR("Not implemented");
	return FALSE;
#endif
}

int mutex_unlock_(void* pHandle)
{
#ifdef __XMK__
	if (pthread_mutex_unlock_((pthread_mutex_t*)(pHandle)) != 0)
	{
		error_set(ERR_CANNOT_UNLOCK_MUTEX);
		LOG_ERROR("Cannot unlock mutex");
		return FALSE;
	}
	return TRUE;
#else
	error_set(ERR_NOT_IMPLEMENTED);
	LOG_ERROR("Not implemented");
	return FALSE;
#endif
}

/*
 * mutex.h
 *
 *  Created on: Jun 25, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef MUTEX_H_
#define MUTEX_H_

#ifdef __XMK__
/*#include "xmk.h"*/
/*#include "pthread.h"*/
#endif
#include "../util/types.h"
/*
 * The following should be modified if another type of mutex is needed
 */

#ifdef __cplusplus
extern "C"
{
#endif
/**
 * @param pHandle pointer to hold the value of the handle
 * @return TRUE if successful
 */
int mutex_initialize(void* pHandle);
/**
 * @param pHandle pointer mutex handle
 * @return TRUE if successful
 */
int mutex_destruct(void* pHandle);
/**
 * @param pHandle pointer mutex handle
 * @return TRUE if successful
 */
int mutex_lock_(void* pHandle);
/**
 * @param pHandle pointer mutex handle
 * @return TRUE if successful
 */
int mutex_unlock_(void* pHandle);

#ifdef __cplusplus
}
#endif

#endif /* MUTEX_H_ */

/*
 * system.h
 *
 *  Created on: Jun 25, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 *
 *
 * 	@note: this file should be re-written to match the environment the
 * 	API is running on
 */

#ifndef SYSTEM_H_
#define SYSTEM_H_

#include "../util/types.h"
#ifdef __cplusplus
extern "C"
{
#endif
typedef enum
{
	RX_INT = 1,
	TX_WAKEUP_INT,
	TX_INT
} interrupt_id_t;

void system_sleepms(unsigned ms);
int system_threadcreate(void* handle, thread_t pFunc, void* param);
int system_threaddestruct(void* handle);

int system_start(thread_t thread);

int system_intrruptdisable(interrupt_id_t id);
int system_interruptenable(interrupt_id_t id);
int system_interruptacknowledge(void);
int system_interrupthandlerregister(
	interrupt_id_t id, handler_t handler, void * param);
int system_interrupthandlerunregister(interrupt_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_H_ */

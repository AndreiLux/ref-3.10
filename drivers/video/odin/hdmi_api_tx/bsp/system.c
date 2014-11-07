/*
 * system.c
 *
 *  Created on: Jun 25, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 *
 *
 * 	@note: this file should be re-written to match the environment the
 * 	API is running on
 */
#ifdef __cplusplus
extern "C"
{
#endif
#include "system.h"
#include "../util/log.h"
#include "../util/error.h"
#ifdef __XMK__
#include "xmk.h"
#include "xparameters.h"
#include "xstatus.h"
#include "sys/time.h"
#include "sys/init.h"
#include "sys/intr.h"
#include "pthread.h"
#include "sys/ipc.h"
#include "sys/msg.h"
#include "errno.h"
void xilkernel_init();
void xilkernel_start();
#else
#ifdef WIN32
#include <windows.h>
#include <process.h>
#else
#include <linux/delay.h>
#endif
#endif

void system_sleepms(unsigned ms)
{
#ifdef __XMK__
	sleep(ms);
#else
#ifdef WIN32
	Sleep(ms);
#else
	udelay(ms * 1000);
#endif
#endif
}

int system_threadcreate(void* handle, thread_t pFunc, void* param)
{
#ifdef __XMK__
	pthread_t timerId;
	return pthread_create(&timerId, NULL, pFunc, param);
#else
	error_set(ERR_NOT_IMPLEMENTED);
	return FALSE;
#endif
}
int system_threaddestruct(void* handle)
{
	error_set(ERR_NOT_IMPLEMENTED);
	return FALSE;
}

int system_start(thread_t thread)
{
#ifdef __XMK__
	xilkernel_init();
	xmk_add_static_thread(thread, 1);
	xilkernel_start();
#else
	thread(NULL);
#endif
	return 0;
}

static unsigned system_interruptmap(interrupt_id_t id)
{
#ifdef __XMK__
	switch (id)
	{
		case RX_INT:
			return XPAR_XPS_INTC_0_SYSTEM_INT3_INTR;
		case TX_WAKEUP_INT:
			return XPAR_XPS_INTC_0_SYSTEM_INT2_INTR;
		case TX_INT:
			return XPAR_XPS_INTC_0_SYSTEM_INT1_INTR;
		default:
			return XPAR_XPS_INTC_0_XPS_TIMER_0_INTERRUPT_INTR;
	}
#endif
	return ~0;
}

int system_intrruptdisable(interrupt_id_t id)
{
	if (system_interruptmap(id) != (unsigned) (~0))
	{
#ifdef __XMK__
		disable_interrupt(system_interruptmap(id));
		return TRUE;
#else
		error_set(ERR_NOT_IMPLEMENTED);
		return FALSE;
#endif
	}
	error_set(ERR_INTERRUPT_NOT_MAPPED);
	LOG_ERROR("Interrupt not mapped");
	return FALSE;
}

int system_interruptenable(interrupt_id_t id)
{
	if (system_interruptmap(id) != (unsigned) (~0))
	{
#ifdef __XMK__
		enable_interrupt(system_interruptmap(id));
		return TRUE;
#else
		error_set(ERR_NOT_IMPLEMENTED);
		return FALSE;
#endif
	}
	error_set(ERR_INTERRUPT_NOT_MAPPED);
	LOG_ERROR("Interrupt not mapped");
	return FALSE;
}

int system_interruptacknowledge(void)
{

	return TRUE;
}
int system_interrupthandlerregister(interrupt_id_t id, handler_t handler,
		void * param)
{
#ifdef __XMK__
	if (register_int_handler(
		system_interruptmap(id), handler, param) != XST_SUCCESS)
	{
		LOG_ERROR("register_int_handler");
		return FALSE;
	}
	return TRUE;
#else
	error_set(ERR_NOT_IMPLEMENTED);
	return FALSE;
#endif
}

int system_interrupthandlerunregister(interrupt_id_t id)
{
#ifdef __XMK__
	unregister_int_handler(system_interruptmap(id));
	return TRUE;
#else
	error_set(ERR_NOT_IMPLEMENTED);
	return FALSE;
#endif
}

#ifdef __cplusplus
}
#endif

/*-----------------------------------------------------------------------------
 * $RCSfile: x_os.h,v $
 * $Revision: #10 $
 * $Date: 2012/05/02 $
 * $Author: mike.zhang $
 *
 * Description:
 *         This header file contains OS function prototypes, which are
 *         exported.
 *---------------------------------------------------------------------------*/

#ifndef _X_OS_H_
#define _X_OS_H_


/*-----------------------------------------------------------------------------
		    include files
-----------------------------------------------------------------------------*/

#include "u_os.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_ALLOCATE_FROM_REAR 1

/*-----------------------------------------------------------------------------
		    functions declarations
 ----------------------------------------------------------------------------*/

	extern CRIT_STATE_T(*x_crit_start) (VOID);

	extern VOID(*x_crit_end) (CRIT_STATE_T t_old_level);


/* MsgQ API's */
	extern INT32 x_msg_q_create(HANDLE_T * ph_msg_q,
				    const CHAR * ps_name, SIZE_T z_msg_size, UINT16 ui2_msg_count);

	extern INT32 x_msg_q_attach(HANDLE_T * ph_msg_q, const CHAR * ps_name);

	extern INT32 x_msg_q_delete(HANDLE_T h_msg_q);

	extern INT32 x_msg_q_send(HANDLE_T h_msg_q,
				  const VOID *pv_msg, SIZE_T z_size, UINT8 ui1_priority);

	extern INT32 x_msg_q_receive(UINT16 * pui2_index,
				     VOID *pv_msg,
				     SIZE_T * pz_size,
				     HANDLE_T * ph_msg_q_mon_list,
				     UINT16 ui2_msg_q_mon_count, MSGQ_OPTION_T e_options);

	extern INT32 x_msg_q_receive_timeout(UINT16 * pui2_index,
					     VOID *pv_msg,
					     SIZE_T * pz_size,
					     HANDLE_T * ph_msg_q_mon_list,
					     UINT16 ui2_msg_q_mon_count, UINT32 ui4_time);

	extern INT32 x_msg_q_num_msgs(HANDLE_T h_msg_q, UINT16 * pui2_num_msgs);

	extern INT32 x_msg_q_get_max_msg_size(HANDLE_T h_msg_q, SIZE_T * pz_maxsize);
	extern INT32 x_msg_q_flush(HANDLE_T h_msg_hdl);

/* Event_group API's */
	extern INT32 x_ev_group_create(HANDLE_T * ph_hdl,
				       const CHAR * ps_name, EV_GRP_EVENT_T e_init_events);

	extern INT32 x_ev_group_attach(HANDLE_T * ph_hdl, const CHAR * ps_name);

	extern INT32 x_ev_group_delete(HANDLE_T h_hdl);

	extern INT32 x_ev_group_set_event(HANDLE_T h_hdl,
					  EV_GRP_EVENT_T e_events, EV_GRP_OPERATION_T e_operation);

	extern INT32 x_ev_group_wait_event(HANDLE_T h_hdl,
					   EV_GRP_EVENT_T e_events_req,
					   EV_GRP_EVENT_T * pe_events_got,
					   EV_GRP_OPERATION_T e_operation);

	extern INT32 x_ev_group_wait_event_timeout(HANDLE_T h_hdl,
						   EV_GRP_EVENT_T e_events_req,
						   EV_GRP_EVENT_T * pe_events_got,
						   EV_GRP_OPERATION_T e_operation, UINT32 ui4_time);

	extern INT32 x_ev_group_get_info(HANDLE_T h_hdl,
					 EV_GRP_EVENT_T * pe_cur_events,
					 UINT8 * pui1_num_thread_waiting,
					 CHAR * ps_ev_group_name, CHAR * ps_first_wait_thread);


/* Thread API's */
	extern INT32 x_thread_create(HANDLE_T * ph_th_hdl,
				     const CHAR * ps_name,
				     SIZE_T z_stack_size,
				     UINT8 ui1_priority,
				     x_os_thread_main_fct pf_main_rtn,
				     SIZE_T z_arg_size, VOID *pv_arg);

	extern VOID x_thread_exit(VOID);

	extern VOID x_thread_delay(UINT32 ui4_delay);

	extern INT32 x_thread_set_pri(HANDLE_T h_th_hdl, UINT8 ui1_new_pri);

	extern INT32 x_thread_get_name(HANDLE_T h_th_hdl, UINT32 * s_name);

	extern INT32 x_thread_get_pri(HANDLE_T h_th_hdl, UINT8 * pui1_pri);

	extern VOID x_thread_suspend(VOID);

	extern INT32 x_thread_resume(HANDLE_T h_th_hdl);

	extern INT32 x_thread_self(HANDLE_T * ph_th_hdl);

	extern INT32 x_thread_stack_stats(HANDLE_T h_th_hdl,
					  SIZE_T * pz_alloc_stack, SIZE_T * pz_max_used_stack);

	extern INT32 x_thread_set_pvt(UINT32 ui4_key,
				      x_os_thread_pvt_del_fct pf_pvt_del, VOID *pv_pvt);
	extern INT32 x_thread_get_pvt(UINT32 ui4_key, VOID **ppv_pvt);
	extern INT32 x_thread_del_pvt(UINT32 ui4_key);

/* Semaphhore API's */
	extern INT32 x_sema_create(HANDLE_T * ph_sema_hdl,
				   SEMA_TYPE_T e_types, UINT32 ui4_init_value);

	extern INT32 x_sema_delete(HANDLE_T h_sema_hdl);

	extern INT32 x_sema_force_delete(HANDLE_T h_sema_hdl);

	extern INT32 x_sema_lock(HANDLE_T h_sema_hdl, SEMA_OPTION_T e_options);

	extern INT32 x_sema_lock_timeout(HANDLE_T h_sema_hdl, UINT32 ui4_time);

	extern INT32 x_sema_unlock(HANDLE_T h_sema_hdl);


/* ISR API's */
	extern INT32 x_reg_isr_ex(UINT16 ui2_vector_id,
				  x_os_isr_fct pf_isr,
				  x_os_isr_fct * ppf_old_isr, ISR_FLAG_T e_flags);

	extern INT32 x_reg_isr(UINT16 ui2_vector_id,
			       x_os_isr_fct pf_isr, x_os_isr_fct * ppf_old_isr);

	extern INT32 x_reg_isr_shared(UINT16 ui2_vec_id,
				      x_os_isr_fct pf_isr,
				      x_os_isr_fct * ppf_old_isr, void *dev_id);

/* Timer API's */
	extern INT32 x_timer_create(HANDLE_T * ph_timer);

	extern INT32 x_timer_start(HANDLE_T h_timer,
				   UINT32 ui4_delay,
				   TIMER_FLAG_T e_flags,
				   x_os_timer_cb_fct pf_callback, VOID *pv_tag);

	extern INT32 x_timer_stop(HANDLE_T h_timer);

	extern INT32 x_timer_delete(HANDLE_T h_timer);

	extern INT32 x_timer_resume(HANDLE_T h_timer);

/* Memory API's */
	extern VOID *x_mem_alloc(SIZE_T z_size);
/*
#if CONFIG_ONEZONE_MEM_ALLOC_NEW_POLICY
extern VOID* x_mem_alloc_onezone_pool (SIZE_T  z_size);
extern VOID x_mem_free_onezone_pool (VOID*  pv_mem_block);

#endif

#if CONFIG_ALLOCATE_FROM_REAR && !CONFIG_SYS_MEM_PHASE3
extern VOID* x_mem_alloc_from_rear (SIZE_T z_size);
#endif
*/
	extern VOID *x_mem_calloc(UINT32 ui4_num_element, SIZE_T z_size_element);

	extern VOID *x_mem_realloc(VOID *pv_mem_block, SIZE_T z_new_size);

	extern VOID x_mem_free(VOID *pv_mem_block);

/*
#if !CONFIG_SYS_MEM_PHASE3
extern VOID* x_mem_ch2_alloc (SIZE_T  z_size);

extern VOID* x_mem_ch2_calloc (UINT32  ui4_num_element,
			       SIZE_T  z_size_element);

extern VOID* x_mem_ch2_realloc (VOID*  pv_mem_block,
				SIZE_T z_new_size);
#endif
*/
	extern INT32 x_mem_part_create(HANDLE_T * ph_part_hdl,
				       const CHAR * ps_name,
				       VOID *pv_addr, SIZE_T z_size, SIZE_T z_alloc_size);

	extern INT32 x_mem_part_delete(HANDLE_T);

	extern INT32 x_mem_part_attach(HANDLE_T * ph_part_hdl, const CHAR * ps_name);

	extern VOID *x_mem_part_alloc(HANDLE_T h_part_hdl, SIZE_T z_size);
/*
#if CONFIG_ALLOCATE_FROM_REAR && !CONFIG_SYS_MEM_PHASE3
extern VOID* x_mem_part_alloc_from_rear (HANDLE_T  h_part_hdl,
					 SIZE_T    z_size);
#endif
*/
	extern VOID *x_mem_part_calloc(HANDLE_T h_part_hdl,
				       UINT32 ui4_num_element, SIZE_T z_size_element);

	extern VOID *x_mem_part_realloc(HANDLE_T h_part_hdl,
					VOID *pv_mem_block, SIZE_T z_new_size);


/* Utility API's */
	extern VOID *x_memcpy(VOID *pv_to, const VOID *pv_from, SIZE_T z_len);

	extern INT32 x_memcmp(const VOID *pv_mem1, const VOID *pv_mem2, SIZE_T z_len);

	extern VOID *x_memmove(VOID *pv_to, const VOID *pv_from, SIZE_T z_len);

	extern VOID *x_memset(VOID *pui1_mem, UINT8 ui1_c, SIZE_T z_len);

	extern VOID *x_memchr(const VOID *pv_mem, UINT8 ui1_c, SIZE_T z_len);

	extern VOID *x_memrchr(const VOID *pv_mem, UINT8 ui1_c, SIZE_T z_len);

	extern CHAR *x_strdup(const CHAR *ps_str);

	extern CHAR *x_strcpy(CHAR *ps_to, const CHAR *ps_from);

	extern CHAR *x_strncpy(CHAR *ps_to, const CHAR *ps_from, SIZE_T z_len);

	extern INT32 x_strcmp(const CHAR *ps_str1, const CHAR *ps_str2);

	extern INT32 x_strncmp(const CHAR *ps_str1, const CHAR *ps_str2, SIZE_T z_len);

	extern INT32 x_strcasecmp(const CHAR *ps_str1, const CHAR *ps_str2);

	extern INT32 x_strncasecmp(const CHAR *ps_str1, const CHAR *ps_str2, SIZE_T z_len);

	extern CHAR *x_strcat(CHAR *ps_to, const CHAR *ps_append);

	extern CHAR *x_strncat(CHAR *ps_to, const CHAR *ps_append, SIZE_T z_len);

	extern CHAR *x_strchr(const CHAR *ps_str, CHAR c_char);

	extern CHAR *x_strrchr(const CHAR *ps_str, CHAR c_char);

	extern CHAR *x_strstr(const CHAR *ps_str, const CHAR *ps_find);

	extern UINT64 x_strtoull(const CHAR *pc_beg_ptr, CHAR **ppc_end_ptr, UINT8 ui1_base);

	extern INT64 x_strtoll(const CHAR *pc_beg_ptr, CHAR **ppc_end_ptr, UINT8 ui1_base);

	extern SIZE_T x_strlen(const CHAR *ps_str);

	extern SIZE_T x_strspn(const CHAR *ps_str, const CHAR *ps_accept);

	extern SIZE_T x_strcspn(const CHAR *ps_str, const CHAR *ps_reject);

	extern CHAR *x_str_toupper(CHAR *ps_str);

	extern CHAR *x_str_tolower(CHAR *ps_str);

	extern CHAR *x_strtok(CHAR *ps_str,
			      const CHAR *ps_delimiters, CHAR **pps_str, SIZE_T *pz_token_len);

	extern INT32 x_sprintf(CHAR *ps_str, const CHAR *ps_format, ...);

	extern INT32 x_vsprintf(CHAR *ps_str, const CHAR *ps_format, VA_LIST va_list);

	extern INT32 x_snprintf(CHAR *ps_str, SIZE_T z_size, const CHAR *ps_format, ...);

	extern INT32 x_vsnprintf(CHAR *ps_str,
				 SIZE_T z_size, const CHAR *ps_format, VA_LIST va_list);

	extern INT32 x_sscanf(const CHAR *ps_buf, const CHAR *ps_fmt, ...);

	extern INT32 x_vsscanf(const CHAR *ps_buf, const CHAR *ps_fmt, VA_LIST t_ap);

/*
  System clock tick function.
*/
	extern UINT32 x_os_get_sys_tick(VOID);

	extern UINT32 x_os_get_sys_tick_period(VOID);

#ifdef __cplusplus
}
#endif
#endif				/* _X_OS_H_ */

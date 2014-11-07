/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See th
 * GNU General Public License for more details.
 */

/*       
  
                                 
  
                                                  
                
                    
  
                           
     
 */

#ifndef	_GFX_PROC_H_
#define	_GFX_PROC_H_

/*------------------------------------------------------------------
	Control Constants
--------------------------------------------------------------------*/

/*------------------------------------------------------------------
    File Inclusions
--------------------------------------------------------------------*/
#include <linux/proc_fs.h>
#include "base_types.h"

#ifdef	__cplusplus
extern "C"
{
#endif /* __cplusplus */

/*------------------------------------------------------------------
	Constant Definitions
--------------------------------------------------------------------*/
/** @name Type Defininition for Proc Utility
 * os functions for file access service in device driver
 *
 * @{
 *
 * default buffer size for write proc function
 */
#define LX_MAX_WRITE_PROC_BUFSZ	100

/*------------------------------------------------------------------
	Macro Definitions
--------------------------------------------------------------------*/

/*------------------------------------------------------------------
    Type Definitions
--------------------------------------------------------------------*/
/**
 * Proc attribute flag for OS_PROC_DESC_TABLE_T.
 * READ/WRITE flag can be ORed.
 *
 * @see OS_PROC_DESC_TABLE_T
 */
enum
{
	OS_PROC_FLAG_READ		= 0x0001,	/* read attribute enabled */
	OS_PROC_FLAG_WRITE		= 0x0002,	/* write attribute eanbled */
	OS_PROC_FLAG_FOP		= 0x0010,	/* OS_PROC_DESC_TABLE_T.fop shall
									be used and proc_create shall be used. */
};

/**
 * Proc description table.
 * device driver using proc_util should define its own proc description.
 * by convention, OS_PROC_DESC_TABLE_t is defined at [module_name]_proc.c
 *
 * @code
 * static OS_PROC_DESC_TABLE_t	_g_template_device_proc_table[] =
 * {
 * 	 { "author", PROC_ID_AUTHOR , OS_PROC_FLAG_READ },
 * 	 { "command", PROC_ID_COMMAND , OS_PROC_FLAG_WRITE},
 *   { "debug", PROC_ID_DEBUG , OS_PROC_FLAG_READ | OS_PROC_FLAG_WRITE },
 *   { "big_debug", PROC_ID_BIG_DEBUG,OS_PROC_FLAG_READ | OS_PROC_FLAG_FOP },
 *   { "big_debug2",PROC_ID_BIG_2 , OS_PROC_FLAG_READ | OS_PROC_FLAG_FOP,
 *		&module_private_data },
 * 	 { NULL, 		PROC_ID_MAX		, 0 }
 * };
 * @endcode
 *
 * @see template_proc.c
*/
typedef struct
{
	char *name;	/* proc name */
	UINT32 id:16,	/* unique id used at device driver */
		  flag:16; /* read/write/seq_file(fop) flag */
	void *fop;	/* if fop should be overred per-proc_entry base. */
	void *data;	/* proc_entry->data shall be set by this. */
}
OS_PROC_DESC_TABLE_T;

/**
 * user-defined read proc function
 * proc_util checks basic error conditions, so you don't need to
 * check any error.
 * basic usage is the same as linux read proc function.
 * you should write something to buf.
 *
 * @see os_proc_createentryex
*/
typedef int	(*OS_PROC_READ_FUNC_T)( UINT32 procId, char* buffer );

/**
 * user-defined write proc function
 * proc_util checks basic error conditions, so you don't need to check
 * any error.
 * basic usage is the same as linux read proc function.
 * proc_util passes command which is null terminated string and you
 * need to parse command string.
 *
 * @see os_proc_createentryex
*/
typedef int (*OS_PROC_WRITE_FUNC_T)( UINT32 procId, char* command );

/** @} */

/*------------------------------------------------------------------
	Extern Function Prototype Declaration
--------------------------------------------------------------------*/
extern void os_proc_init ( void );
extern void os_proc_cleanup ( void );
/*extern int os_proc_createentry ( char* base_name,
					  OS_PROC_DESC_TABLE_T*	pTable,
					  read_proc_t* read_function,
					  write_proc_t* write_function );
*/
extern int os_proc_createentryex ( char* base_name,
					  OS_PROC_DESC_TABLE_T*	pTable,
					  OS_PROC_READ_FUNC_T	read_function,
					  OS_PROC_WRITE_FUNC_T	write_function );
extern int os_proc_removeentry ( char* base_name );
extern void os_proc_flushentry ( void );

/*------------------------------------------------------------------
	Extern Variables
--------------------------------------------------------------------*/

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _GFX_PROC_H_ */

/** @} */


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

/*-------------------------------------------------------------------
	Control Constants
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	File Inclusions
--------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include "gfx_impl.h"
#include "os_util.h"

/*-------------------------------------------------------------------
	Constant Definitions
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Macro Definitions
--------------------------------------------------------------------*/
#define	BASE_PROC_NAME		"lg"

#define MK_PROC_DATA(idx,id)	(((idx)<<16)|(id))
#define GET_MODULE_ID(data)		(((UINT32)(data))>>16)
#define GET_PROC_ID(data)		(((UINT32)(data))&0xffff)

/*-------------------------------------------------------------------
	Type Definitions
--------------------------------------------------------------------*/
enum {
	PROC_ID_AUTHOR	= 0,
	PROC_ID_CMD_DELAY,
	PROC_ID_MEM_STAT,
	PROC_ID_REG_DUMP,
	PROC_ID_SURFACE,
	PROC_ID_GFX_STATUS,
/*	PROC_ID_PERF_LOG,	*/
	PROC_ID_RESET,
	PROC_ID_SYNC_TMOUT,
	PROC_ID_MAX,
};

typedef struct
{
	char module_name[LX_MAX_DEVICE_NAME];
	OS_PROC_DESC_TABLE_T *module_proc_table;
	struct proc_dir_entry *module_proc_dir;
	int max_proc_num;
	OS_PROC_READ_FUNC_T ext_read_proc;
	OS_PROC_WRITE_FUNC_T ext_write_proc;
}
PROC_MANAGE_TABLE_T;


/*-------------------------------------------------------------------
	External Function Prototype Declarations
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	External Variables
--------------------------------------------------------------------*/
extern GFX_SURFACE_OBJ_T 		*g_gfx_surf_list;
/* extern int	g_gfx_perf_log; */
/* extern GFX_SCALER_FILTER_DATA_T	g_gfx_scaler_filter; */

/*-------------------------------------------------------------------
	global Variables
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Static Function Prototypes Declarations
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Static Variables
--------------------------------------------------------------------*/

static OS_SEM_T _g_proc_sem;
static struct proc_dir_entry *_g_proc_base_dir;
static PROC_MANAGE_TABLE_T _g_base_manage_table[LX_MAX_DEVICE_NUM];

static OS_PROC_DESC_TABLE_T	_g_gfx_device_proc_table[] =
{
	{ "author", PROC_ID_AUTHOR , OS_PROC_FLAG_READ },
	{ "cmd_delay", PROC_ID_CMD_DELAY, OS_PROC_FLAG_READ|OS_PROC_FLAG_WRITE },
	{ "mem_stat", PROC_ID_MEM_STAT ,  OS_PROC_FLAG_READ },
	{ "reg_dump", PROC_ID_REG_DUMP, OS_PROC_FLAG_READ },
	{ "surface", PROC_ID_SURFACE, OS_PROC_FLAG_READ },
	{ "status", PROC_ID_GFX_STATUS, OS_PROC_FLAG_READ },
/*	{ "perflog", PROC_ID_PERF_LOG, OS_PROC_FLAG_WRITE }, */
	{ "reset", PROC_ID_RESET, OS_PROC_FLAG_WRITE },
	{ "synctm", PROC_ID_SYNC_TMOUT, OS_PROC_FLAG_READ|OS_PROC_FLAG_WRITE },
	{ NULL, PROC_ID_MAX, 0 }
};




/*-------------------------------------------------------------------
	Implementation Group
--------------------------------------------------------------------*/
/** @name Function Definition for Proc Utility
 * function list for proc utility
 *
 * @{
*/

/**
 * Initialize proc utility system.
 * This function should be called at main device initialization
 *
 * @see base_device_init
*/
void os_proc_init	( void )
{
	OS_InitMutex(&_g_proc_sem, OS_SEM_ATTR_DEFAULT);

	memset( _g_base_manage_table, 0x0,
		sizeof(PROC_MANAGE_TABLE_T) * LX_MAX_DEVICE_NUM );
	_g_proc_base_dir = proc_mkdir ( BASE_PROC_NAME, NULL );

	OS_LockMutex(&_g_proc_sem);


	OS_UnlockMutex(&_g_proc_sem);
}

/**
 * Cleanup proc utility system
 * This function should be called at main deivce cleanup
 *
 * @see base_device_shutdown
*/
void os_proc_cleanup	( void )
{
	int	ret;

	os_proc_flushentry( );

	ret = OS_LockMutex(&_g_proc_sem);
	remove_proc_entry ( BASE_PROC_NAME, NULL );
	OS_UnlockMutex(&_g_proc_sem);

	/* remove mutex ? */
}

/* internal read proc function
 * checks basic error condition and calls simple user-defined read proc
 *
 */
static int _os_proc_reader ( char* buffer, char** buffer_location,
				off_t offset, int buffer_length, int* eof, void* data )
{
	UINT32	moduleid= GET_MODULE_ID(data);
	UINT32	procid  = GET_PROC_ID(data);

	OS_PROC_READ_FUNC_T	ext_read_proc;

	if ( offset > 0 )
	{
    	/* we've finished to read, return 0 */ goto read_proc_exit;
	}

	if ( moduleid >= LX_MAX_DEVICE_NUM )
	{
    	printk("unknown module (%d)\n", moduleid );
    	/* we've finished to read, return 0 */ goto read_proc_exit;
	}

    if ( procid >= _g_base_manage_table[moduleid].max_proc_num )
    {
    	printk("unknown proc (%d)\n", procid );
    	/* we've finished to read, return 0 */ goto read_proc_exit;
    }

	ext_read_proc = _g_base_manage_table[moduleid].ext_read_proc;

	if ( ext_read_proc ) return ext_read_proc ( procid, buffer );

read_proc_exit:
	return 0;
}

/* internal read proc function
 * checks basic error condition and calls simple user-defined write proc
 *
 */
static int _os_proc_writer( struct file* file, const char* buffer,
					unsigned long count, void* data )
{
    int		i;
    char	msg[LX_MAX_WRITE_PROC_BUFSZ];
	UINT32	moduleid= GET_MODULE_ID(data);
	UINT32	procid  = GET_PROC_ID(data);

   	OS_PROC_WRITE_FUNC_T	ext_write_proc;

	if ( moduleid >= LX_MAX_DEVICE_NUM )
	{
    	printk("unknown module (%d)\n", moduleid );
    	/* we've finished to write, return 0 */ goto write_proc_exit;
	}

    if ( procid >= _g_base_manage_table[moduleid].max_proc_num )
    {
    	printk("unknown proc (%d)\n", procid );
    	/* we've finished to write, return 0 */ goto write_proc_exit;
    }

    /* copy buffer to internal msg */
    if ( count > (LX_MAX_WRITE_PROC_BUFSZ - 1))
         count = LX_MAX_WRITE_PROC_BUFSZ - 1;

    for ( i = 0; i <count; i++)
    {
    	get_user(msg[i], buffer+i);
    }
    msg[i] = '\0';

	ext_write_proc = _g_base_manage_table[moduleid].ext_write_proc;

	if (ext_write_proc) return ext_write_proc ( procid, msg );

write_proc_exit:
	return 0;
}

/**
 * create proc entry for each module
 *
 * @param base_name [IN] module name
 * @param pTable [IN] pointer to proc description table
 * @param read_function [IN] read proc callback for read operation
 * ( application reads something from driver )
 * @param write_function [IN] write proc callback for write operation
 * ( application writes something to driver )
 * @returns RET_OK(0) if success, none zero for otherwise
 *
 * @see os_proc_createentry
 * @see os_proc_removeentry
 * @see OS_PROC_fush_entry
 *
*/

static int _os_proc_createentry(char* module_name,
				OS_PROC_DESC_TABLE_T*	pTable,
				void*		read_function,
				void*		write_function,
				BOOLEAN		fExtMode )
{
	int		i, j;
	int		ret;
	mode_t	entry_mode;
#if 0
	struct	proc_dir_entry *proc_entry;

	read_proc_t *read_proc  = NULL;
	write_proc_t *write_proc = NULL;

	if ( NULL == module_name || NULL == pTable || NULL == read_function )
	{
		printk("null argument (%c,%c,%c)\n", (module_name)? 'O':'-',
			(pTable)? 'O':'-', (read_function)? 'O':'-' );
		return RET_INVALID_PARAMS;
	}

	ret = OS_LockMutex(&_g_proc_sem);

	for (i=0; i< LX_MAX_DEVICE_NUM; i++ )
	{
		if ( NULL == _g_base_manage_table[i].module_proc_table )
			{ /* found empty */ break; }
	}

	if ( i >= LX_MAX_DEVICE_NUM )
	{
		printk("proc register failed. no empty slot.\n" );
		ret = RET_INVALID_PARAMS; goto function_exit;
	}

	/* if ext mode, internal proc function will be used */
	if ( fExtMode )
	{
		_g_base_manage_table[i].ext_read_proc = read_function;
		_g_base_manage_table[i].ext_write_proc= write_function;
	}
	else
	{
		_g_base_manage_table[i].ext_read_proc = NULL;
		_g_base_manage_table[i].ext_write_proc= NULL;
	}

	read_proc = ( fExtMode )? _os_proc_reader : (read_proc_t*)read_function;
	write_proc= ( fExtMode )? _os_proc_writer: (write_proc_t*)write_function;

	/* make proc structure */
	_g_base_manage_table[i].module_proc_table = pTable;
	snprintf( _g_base_manage_table[i].module_name, LX_MAX_DEVICE_NAME-1,
		"%s", module_name );

	_g_base_manage_table[i].module_proc_dir =
		proc_mkdir ( module_name, _g_proc_base_dir );

	for ( j=0; pTable[j].name ; j++ )
	{
		entry_mode = ( pTable[j].flag & OS_PROC_FLAG_WRITE )? 0644: 0444;

		if ( pTable[j].flag & OS_PROC_FLAG_FOP )
		{
			proc_entry = create_proc_entry( pTable[j].name, entry_mode,
					_g_base_manage_table[i].module_proc_dir );

			/* override operation pointer and private data of proc_entry */
			if ( proc_entry )
			{
				proc_entry->proc_fops = (struct file_operations *) pTable[j].fop;
				proc_entry->data = (void*)pTable[j].data;
			}
		}
		else
		{
			proc_entry = create_proc_entry( pTable[j].name, entry_mode,
				_g_base_manage_table[i].module_proc_dir );

			proc_entry->read_proc = ( pTable[j].flag & OS_PROC_FLAG_READ )?
								read_proc : NULL;
			proc_entry->write_proc = ( pTable[j].flag & OS_PROC_FLAG_WRITE)?
								write_proc: NULL;
/*			proc_entry->owner = THIS_MODULE; */
/*			proc_entry->mode = S_IFREG | S_IRUGO; */
/*			proc_entry->uid = 0; */
/*			proc_entry->gid = 0; */
/*			proc_entry->size = 80; */
			proc_entry->data = (void*)( (fExtMode)?
				MK_PROC_DATA( i, pTable[j].id ) : pTable[j].id );
		}
	}

	_g_base_manage_table[i].max_proc_num = j;

function_exit:
	OS_UnlockMutex(&_g_proc_sem);
#endif

	return RET_OK;
}


int	os_proc_createentryex ( char*	module_name,
					OS_PROC_DESC_TABLE_T*	pTable,
					OS_PROC_READ_FUNC_T		read_function,
					OS_PROC_WRITE_FUNC_T	write_function )
{
	return _os_proc_createentry( module_name, pTable, read_function,
			write_function, TRUE );
}


/**
 * remove proc entry for each module
 *
 * @param	base_name [IN] base module name
 * @returns RET_OK(0) if success, none zero for otherwise
 *
*/
int os_proc_removeentry ( char*		module_name )
{
	int		i, j;
	int		ret;
	PROC_MANAGE_TABLE_T	manage_table;

	if ( NULL == module_name )
	{
		printk("null argument\n");
		return RET_INVALID_PARAMS;
	}

	ret = OS_LockMutex(&_g_proc_sem);

	for ( i=0 ; i<LX_MAX_DEVICE_NUM ; i++ )
	{
		if ( !_g_base_manage_table[i].module_name ) continue;

		if ( !strcmp ( _g_base_manage_table[i].module_name, module_name ) )
		{
			manage_table = _g_base_manage_table[i];
			memset( &_g_base_manage_table[i], 0x0,
					sizeof(PROC_MANAGE_TABLE_T) );
			break;
		}
	}

	if ( i >= LX_MAX_DEVICE_NUM )
	{
		ret = RET_INVALID_PARAMS; goto function_exit;
	}

	for ( j=0; manage_table.module_proc_table[j].name ; j++ )
	{
		remove_proc_entry( manage_table.module_proc_table[j].name,
			manage_table.module_proc_dir );
	}

	remove_proc_entry ( manage_table.module_name, _g_proc_base_dir );

	ret = 0;

function_exit:
	OS_UnlockMutex(&_g_proc_sem);

	return ret;
}

/**
 * flush all proc entries
 *
*/
void os_proc_flushentry	( void )
{
	int	i,j;
	int	ret;
	PROC_MANAGE_TABLE_T	manage_table;

    ret = OS_LockMutex(&_g_proc_sem);

	for ( i=0; i<LX_MAX_DEVICE_NUM ; i++ )
	{
		manage_table = _g_base_manage_table[i];

		if ( !manage_table.module_proc_table ) continue;

		manage_table = _g_base_manage_table[i];

		for ( j=0; manage_table.module_proc_table[j].name ; j++ )
		{
			remove_proc_entry( manage_table.module_proc_table[j].name,
				manage_table.module_proc_dir );
		}

		remove_proc_entry ( manage_table.module_name, _g_proc_base_dir );

		memset( &_g_base_manage_table[i], 0x0, sizeof(PROC_MANAGE_TABLE_T) );
	}

    OS_UnlockMutex(&_g_proc_sem);
}



/*==================================================================
	Implementation Group
===================================================================*/

/*
 * read_proc implementation of gfx device
 *
*/
static int _gfx_readprocfunction(u32 procid, char* buffer )
{
	int		ret;

	/* TODO: add your proc_write implementation */
	switch ( procid )
	{
		case PROC_ID_AUTHOR:
		{
			ret = sprintf( buffer, "%s\n", "raxis.lim (raxis.lim@lge.com)" );
		}
		break;

		case PROC_ID_CMD_DELAY:
		{
			u32	cmd_delay;

			(void)gfx_getcommanddelay( &cmd_delay );
			ret = sprintf( buffer, "-- cmd delay : 0x%x (%d)\n",
					cmd_delay, cmd_delay );
		}
		break;

		/* dump GFX memory status */
		case PROC_ID_MEM_STAT:
		{
			int	len;
			LX_GFX_MEM_STAT_T	stat;
			(void)gfx_getsurfacememorystat( &stat );

			len =0;
			len += sprintf( buffer +len, "-mem base :%6dMB (%p - physical)\n",
				((u32)stat.surface_mem_base)>>10, stat.surface_mem_base );
			len += sprintf( buffer +len, "-mem len   : %6dKB (%9d)\n",
				stat.surface_mem_length>>10, stat.surface_mem_length );
			len += sprintf( buffer +len, "-alloc size: %6dKB (%9d)\n",
				stat.surface_mem_alloc_size>>10, stat.surface_mem_alloc_size );
			len += sprintf( buffer +len, "-free  size: %6dKB (%9d)\n",
				stat.surface_mem_free_size>>10, stat.surface_mem_free_size );
			len += sprintf( buffer +len, "-alloc cnt : %6d\n",
				stat.surface_alloc_num );

			ret = len;
		}
		break;

		/* dump surface info */
		case PROC_ID_SURFACE:
		{
			int		i;
			int		len = 0;
			int		surface_cnt = 0;

			for ( i=0; i< GFX_MAX_SURFACE; i++ )
			{
				GFX_SURFACE_OBJ_T *surface = &g_gfx_surf_list[i];


				if ( surface->balloc )
				{
					surface_cnt++;
					printk( "[%3x(%3d)] %d:%3d:%4d:%08x - %d:%2x:%4dx%4d \
						- %d:%4d - %d - %8p:%8x:%7d %s\n",
						i, i, surface->bpalette, surface->palsize,
						surface->cidx, surface->ctick, surface->surf.type,
						surface->surf.pixel_format, surface->surf.width,
						surface->surf.height, surface->surf.alignment,
						surface->surf.stride, (surface->pal)? 1:0,
						surface->mem.phys_addr, surface->mem.offset,
						surface->mem.length, surface->psname );
				}
			}

			len += sprintf( buffer+len, "%d surfaces created\n", surface_cnt );

			ret = len;
		}
		break;

		case PROC_ID_GFX_STATUS:
		{
			int	len;
			GFX_CMD_QUEUE_CTRL_T status;
			gfx_getcomqueuestatus( &status );

			len = 0;

			len += sprintf( buffer+len, " -usLine : 0x%x\n", status.usline );
			len += sprintf( buffer+len, " -bstatus: 0x%x\n", status.bstatus );
			len += sprintf( buffer+len, " -usremainspace: 0x%x\n",
				status.usremainspace );
			len += sprintf( buffer+len, " -bFull : 0x%x\n",status.bfull );
			len += sprintf( buffer+len, " -usremainparam: 0x%x\n",
				status.usremainparam );
			len += sprintf( buffer+len, " -bbatchstatus : 0x%x\n",
				status.bbatchstatus );

			ret = len;
		}
		break;

		/* dump GFX register */
		case PROC_ID_REG_DUMP:
		{
			gfx_dumpregister( );
			ret = sprintf( buffer, "ok\n");
		}
		break;

		case PROC_ID_SYNC_TMOUT:
		{
			int len = 0;
			len += sprintf( buffer+len, "sync wait timeout = %d ms\n",
				g_gfx_cfg.sync_wait_timeout );
			len += sprintf( buffer+len, "fail retry count  = %d \n",
				g_gfx_cfg.sync_fail_retry_count );
			ret = len;
		}
		break;

		default:
		{
			ret = sprintf( buffer, "%s(%d)\n", "unimplemented read proc",
				procid );
		}
	}

	return ret;
}

/*
 * write_proc implementation of gfx device
 *
*/
static int _gfx_writeprocfunction( u32 procid, char* command )
{
	/* TODO: add your proc_write implementation */
	switch ( procid )
	{
		case PROC_ID_CMD_DELAY:
		{
			char *endp;
			u32	cmd_delay;

			cmd_delay = simple_strtoul( command, &endp, 16 );
/*			sscanf( command, " %x", &cmd_delay ); */

			(void)gfx_setcommanddelay( (cmd_delay>0x7ff)? 0x7ff: 0x0 );
			(void)gfx_getcommanddelay( &cmd_delay );
			printk( "-- cmd delay : 0x%x (%d)\n", cmd_delay, cmd_delay );
		}
		break;

#if 0
		case PROC_ID_PERF_LOG:
		{
			char*	endp;
			u32	val;
			val = simple_strtoul( command, &endp, 10 );
			g_gfx_perf_log = val;
		}
		break;
#endif

		case PROC_ID_RESET:
		{

		}
		break;

		case PROC_ID_SYNC_TMOUT:
		{
			int	timeout, retry;
			sscanf( command, " %d %d", &timeout, &retry );

			g_gfx_cfg.sync_wait_timeout = timeout;
			g_gfx_cfg.sync_fail_retry_count = retry;
		}
		break;

		default:
		{
			/* do nothing */
		}
		break;
	}

	return strlen(command);
}

/*
 * initialize proc utility for gfx device
 *
 * @see gfx_proc_init
*/
void gfx_proc_init (void)
{
	os_proc_createentryex ( GFX0_MODULE, _g_gfx_device_proc_table,
						_gfx_readprocfunction,_gfx_writeprocfunction );
}

/*
 * cleanup proc utility for gfx device
 *
 * @see gfx_proc_cleanup
*/
void gfx_proc_cleanup (void)
{
	os_proc_removeentry( GFX0_MODULE );
}

/** @} */


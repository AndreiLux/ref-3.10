/*
 * mdm_ctrl.h
 *
 * Intel Mobile Communication modem boot driver
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Contact: Faouaz Tenoutit <faouazx.tenoutit@intel.com>
 *          Frederic Berat <fredericx.berat@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#ifndef _MDM_CTRL_H
#define _MDM_CTRL_H

#include <linux/ioctl.h>

/* For intel control driver work. */
#define	CONFIG_MIGRATION_CTL_DRV
#define RECOVERY_BY_USB_DISCONNECTION

/* Modem state */
enum {
	MDM_CTRL_STATE_UNKNOWN			= 0x0000,
	MDM_CTRL_STATE_OFF			= 0x0001,
	MDM_CTRL_STATE_COLD_BOOT		= 0x0002,
	MDM_CTRL_STATE_WARM_BOOT		= 0x0004,
	MDM_CTRL_STATE_COREDUMP			= 0x0008,
	MDM_CTRL_STATE_IPC_READY		= 0x0010,
	MDM_CTRL_STATE_FW_DOWNLOAD_READY	= 0x0020,
#if defined(CONFIG_MIGRATION_CTL_DRV)
	MDM_CTRL_STATE_BD_DISCONNECT		= 0x1000
#endif
};

/* Backward compatibility with previous patches */
#define MDM_CTRL_STATE_NONE MDM_CTRL_STATE_UNKNOWN

/* Modem hanging up reasons */
enum {
	MDM_CTRL_NO_HU		= 0x00,
	MDM_CTRL_HU_RESET	= 0x01,
	MDM_CTRL_HU_COREDUMP	= 0x02,
};

enum {
	MDM_TIMER_FLASH_ENABLE,
	MDM_TIMER_FLASH_DISABLE,
	MDM_TIMER_DEFAULT
};

/**
 * struct mdm_ctrl_cmd - Command parameters
 *
 * @timeout: the command timeout duration
 * @curr_state: the current modem state
 * @expected_state: the modem state to wait for
 */
struct mdm_ctrl_cmd {
	unsigned int param;
	unsigned int timeout;
};


#if defined(CONFIG_LGE_CRASH_HANDLER)
#define PATH_MAX 4096

#define SAH_FILENAME_SIZE 32
#define SAH_TASKNAME_SIZE 8
#define SAH_SIZEOF_LOG_DATA 512
#define COREDUMP_FILENAME_SIZE 128
#define SAH_SW_TRAP_ID_SIZE 8
#define SAH_FILE_LINE_STR_SIZE 8
#define SAH_ABORT_CLASS_STR_SIZE 8

typedef enum SAH_ABORT_CLASS{
    SAH_ABORT_UNDEFINED           = 0x0000, /*Unknown abort class*/
    SAH_ABORT_SW_TRAP             = 0xDDDD, /* SW generated trap*/
    SAH_ABORT_DATA_ABORT          = 0xAAAA, /* data abort*/
    SAH_ABORT_PREFETCH_ABORT          = 0xBBBB, /* prefetch abort*/
    SAH_ABORT_UNDEFINED_INSTRUCTION   = 0xCCCC, /* Undefined instruction fault*/
    SAH_ABORT_NONE_CRITICAL_EXCEPTION = 0xEEEE  /* SW none-critical exceptions - no abort*/
} T_SAH_ABORT_CLASS;

typedef struct trap_info {
    char mdm_version[PATH_MAX];
	unsigned char cpu_core;
    T_SAH_ABORT_CLASS abort_class;
    char task_name[SAH_TASKNAME_SIZE];
    char file_name[SAH_FILENAME_SIZE];
    unsigned int file_line;
//    char log_data[SAH_SIZEOF_LOG_DATA];
    char coredump_file_name[COREDUMP_FILENAME_SIZE];
	char sw_trap_id[SAH_SW_TRAP_ID_SIZE];
	char file_line_str[SAH_FILE_LINE_STR_SIZE];
	char abort_class_str[SAH_ABORT_CLASS_STR_SIZE];
} trap_info_t;

trap_info_t* get_trap_info_data(void);
unsigned char get_modem_crash_status(void);
#endif

#define MDM_CTRL_MAGIC	0x87 /* FIXME: Revisit */

/* IOCTL commands list */
#define MDM_CTRL_POWER_OFF		_IO(MDM_CTRL_MAGIC, 0)
#define MDM_CTRL_POWER_ON		_IO(MDM_CTRL_MAGIC, 1)
#define MDM_CTRL_WARM_RESET		_IO(MDM_CTRL_MAGIC, 2)
#define MDM_CTRL_COLD_RESET		_IO(MDM_CTRL_MAGIC, 3)
#define MDM_CTRL_SET_STATE		_IO(MDM_CTRL_MAGIC, 4)
#define MDM_CTRL_GET_STATE		_IO(MDM_CTRL_MAGIC, 5)
#define MDM_CTRL_WAIT_FOR_STATE		_IO(MDM_CTRL_MAGIC, 6)
#define MDM_CTRL_FLASHING_WARM_RESET	_IO(MDM_CTRL_MAGIC, 7)
#define MDM_CTRL_GET_HANGUP_REASONS	_IO(MDM_CTRL_MAGIC, 8)
#define MDM_CTRL_CLEAR_HANGUP_REASONS	_IO(MDM_CTRL_MAGIC, 9)
#define MDM_CTRL_SET_POLLED_STATES	_IO(MDM_CTRL_MAGIC, 10)
#if defined(CONFIG_LGE_CRASH_HANDLER)
#define MDM_CTRL_SET_TRAPINFO _IO(MDM_CTRL_MAGIC, 11)
#endif

#if defined(CONFIG_MIGRATION_CTL_DRV)
#define MDM_CTRL_CHECK_CP_BD_CONN	_IO(MDM_CTRL_MAGIC, 20)
#define MDM_CTRL_SLEEP_STATE	_IO(MDM_CTRL_MAGIC, 21)
#endif
#endif /* _MDM_CTRL_H */


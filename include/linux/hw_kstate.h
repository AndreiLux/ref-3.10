/*
 * Copyright (c) Huawei Technologies Co., Ltd. 1998-2013. All rights reserved.
 *
 * File name: hw_kstate.h
 * Description: This file use to send kernel state.
 * Author: chenyouzhen@huawei.com
 * Version: 0.1
 * Date: 2013/11/14
 */

#ifndef _LINUX_KSTATE_H
#define _LINUX_KSTATE_H

// message mask
#define KSTATE_CLOSE_ALL_MASK      ( 0 )
#define KSTATE_FPS_MASK            ( 1 << 0 )
#define KSTATE_LOG_MASK            ( 1 << 1 )
#define KSTATE_SUSPEND_MASK        ( 1 << 2 )
#define KSTATE_FREEZER_MASK        ( 1 << 3 )

// message type
typedef enum {
    KSTATE_OFF_TYPE = 0, // disable
    KSTATE_ON_TYPE,      // enable
    KSTATE_REPORT_TYPE,  // report data
    KSTATE_COMMAND_TYPE, // sync cmd data
    KSTATE_END_TYPE,
}kstate_type;

// message cmd
typedef enum {
    KSTATE_ISNOT_CMD = -1, // default is not command
    KSTATE_BEGIN_CMD = 0,
    KSTATE_EDC_CMD = 1,    // edc overlay count (KSTATE_FPS_MASK)
    KSTATE_END_CMD,
}kstate_cmd;


int kstate(int mask, const char *fmt, ...);

#endif // _LINUX_KSTATE_H

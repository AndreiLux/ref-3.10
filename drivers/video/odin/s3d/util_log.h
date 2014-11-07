/*
 * linux/drivers/video/odin/s3d/util_log.h
 *
 * Copyright (C) 2012 LG Electronics
 * Author:
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __UTIL_LOG_H__
#define __UTIL_LOG_H__

#include "drv_customer.h"

#define MSG_LOG_ERROR        (0x0000 + (1<<0))
#define MSG_LOG_INFO         (0x0000 + (1<<1))
#define MSG_LOG_FILE         (0x0000 + (1<<2))

int32 util_log (int32 parm, char * fmt, ...);

#ifdef FEATURE_LOG_FILE
#define LOGF(a, ...)         util_log(MSG_LOG_FILE, a, ##__VA_ARGS__)
#else
#define LOGF(a, ...)
#endif

#ifdef FEATURE_LOG_ERROR
#define LOGE(a, ...)         util_log(MSG_LOG_ERROR, a, ##__VA_ARGS__)
#else
#define LOGE(a, ...)
#endif

#ifdef FEATURE_LOG_INFO
#define LOGI(a, ...)         util_log(MSG_LOG_INFO, a, ##__VA_ARGS__)
#else
#define LOGI(a, ...)
#endif

#ifdef FEATURE_LOG_ASSERT
#define ASSERT(x) if (!(x)) { __assert(__FILE__, __LINE__, #x); }
#else
#define ASSERT(X)
#endif

#define RET_CHECK(ret)\
   if (ret != RET_OK)	\
   	{ LOGE("%s, ret=%d error !!! \n",__FUNCTION__, ret); return ret; }

#endif


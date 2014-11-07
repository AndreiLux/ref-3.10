/*
 * linux/drivers/video/odin/s3d/drv_customer.h
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


#ifndef __DRV_CUSTOMER_H__
#define __DRV_CUSTOMER_H__

#include "drv_feature.h"
#include "drv_data_type.h"
#include "drv_platform.h"
#include "util_log.h"
#ifndef FEATURE_FLOATING_SUPPORT
#include "util_math.h"
#endif

/*---------------------------------------------------------------------------
  TYPE
---------------------------------------------------------------------------*/
#define DRV_NULL               (0x0000 + 0)


/*---------------------------------------------------------------------------
  RETURN TYPE
---------------------------------------------------------------------------*/
#define RET_ERROR              (0x0000 + 0)
#define RET_OK                 (0x0000 + 1)
#define RET_ERROR_VECTOR       (0x0000 + 2) /* Vector file error */
#define RET_SKIP               (0x0000 + 3) /* function skip      */

/* driver	*/
#define RET_ERROR_INIT         (0x1000 + 1) /* aready init completed  */
#define RET_MEMORY_FAIL        (0x1000 + 2) /* memory alloc fail      */
#define RET_FILE_OPEN_ERROR    (0x1000 + 3) /* file open fail         */

/*	util	*/
#define RET_ERROR_CODEC        (0x2000 + 1)

#endif

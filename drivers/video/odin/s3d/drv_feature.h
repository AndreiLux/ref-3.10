/*
 * linux/drivers/video/odin/s3d/drv_feature.h
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

#ifndef __DRV_FEATURE_H__
#define __DRV_FEATURE_H__

/*========================================================

MODEL
  - MEPSI F

DESCRIPTION
  - NexusChips    : FEATURE_

==========================================================*/

/*---------------------------------------------------------------
  FEATURE_COMMON
-----------------------------------------------------------------*/

#define FEATURE_BUILD_LINUX       /*LINUX build */
#define FEATURE_FLOATING_REPLACE  /*float Replace */
#define FEATURE_BUILD_RELEASE       /*Release Version */
#define FETURE_HAL_API_USED 	/*Used HAL API */

/*---------------------------------------------------------------
  LOG
-----------------------------------------------------------------*/
#define FEATURE_LOG_ERROR   /* log error */
#define FEATURE_LOG_INFO    /* log info */
#define FEATURE_LOG_FILE    /* file write */
#define FEATURE_LOG_ASSERT  /* assert */

/*---------------------------------------------------------------
  UTIL - BMP
-----------------------------------------------------------------*/
#define FEATURE_BMP_VFLIP_ENABLE
#endif

/*
 * linux/drivers/video/odin/s3d/drv_register_map.h
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

#ifndef __DRV_RESISTER_MAP_H__
#define __DRV_RESISTER_MAP_H__

#include "drv_customer.h"

/*#define __ADDR(a,b)                           (a|(b>>2))  FPGA*/
#define __ADDR(a,b)                           (a|(b>>0)) /*ODIN*/
#define __ADDR16(a,b)                         (a|(b>>0)) /*FPGA*/
/*#define __ADDR(a,b,c)	(a|c) IP, (((a&0xf000)|(b&0x0fff)<<2))*/

/*addressing*/
#define __CORE_ADDR(a)    (a>>0) /*8bit  addressing BLT << 1*/
#define __BLT_ADDR(a)  (a>>1) /*16bit addressing*/
#define __LCD_ADDR(a)   (a>>2) /*32bit addressing*/


#ifndef FEATURE_BUILD_RELEASE
#define BASE_TOP 	(0x0000)
#define BASE_BLT	(0x3000)    /*(Send/GetPixel)16bit base*/
#endif

#if 0
#define FORMATTER_BASE                        (0x2000)
#else
#define FORMATTER_BASE                        (0x0000)
#endif

#define BASE_ECT                              (0x1000)


/*---------------------------------------------------------------------------
  MACRO
---------------------------------------------------------------------------*/

#define BIT_MASK1    (uint16)0x1
#define BIT_MASK2    (uint16)0x3
#define BIT_MASK3    (uint16)0x7
#define BIT_MASK4    (uint16)0xf
#define BIT_MASK5    (uint16)0x1f
#define BIT_MASK6    (uint16)0x3f
#define BIT_MASK7    (uint16)0x7f
#define BIT_MASK8    (uint16)0xff
#define BIT_MASK9    (uint16)0x1ff
#define BIT_MASK10   (uint16)0x3ff
#define BIT_MASK11   (uint16)0x7ff
#define BIT_MASK12   (uint16)0xfff
#define BIT_MASK13   (uint16)0x1fff
#define BIT_MASK14   (uint16)0x3fff
#define BIT_MASK15   (uint16)0x7fff
#define BIT_MASK16   (uint16)0xffff

#define BIT_MASK_H1  (uint32)0x10000
#define BIT_MASK_H2  (uint32)0x30000
#define BIT_MASK_H3  (uint32)0x70000
#define BIT_MASK_H4  (uint32)0xf0000
#define BIT_MASK_H5  (uint32)0x1f0000
#define BIT_MASK_H6  (uint32)0x3f0000
#define BIT_MASK_H7  (uint32)0x7f0000
#define BIT_MASK_H8  (uint32)0xff0000
#define BIT_MASK_H9  (uint32)0x1ff0000
#define BIT_MASK_H10 (uint32)0x3ff0000
#define BIT_MASK_H11 (uint32)0x7ff0000
#define BIT_MASK_H12 (uint32)0xfff0000
#define BIT_MASK_H13 (uint32)0x1fff0000
#define BIT_MASK_H14 (uint32)0x3fff0000
#define BIT_MASK_H15 (uint32)0x7fff0000
#define BIT_MASK_H16 (uint32)0xffff0000


/*---------------------------------------------------------------------------
  MEPSI F DRIVER
---------------------------------------------------------------------------*/

#include "drv_core.h"

#endif

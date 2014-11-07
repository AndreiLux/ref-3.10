/*
 * linux/drivers/video/odin/s3d/drv_port.h
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

#ifndef __DRV_PORT_H__
#define __DRV_PORT_H__

#include "drv_customer.h"

/*
#ifdef __cplusplus
extern "C" {
#endif
*/

int32 drv_port_open   ( int32 parm );
int32 drv_port_exit   ( int32 parm );

int32 drv_io_polling   ( void* parm1, uint32 addr, uint8 bit, boolean cond );

uint32 drv_io_read     ( void* parm1, uint32 a );
int32 drv_io_write     ( void* parm1, uint32 a, uint32 d );

int32 drv_loop_delay   ( int32 ms  );


/*---------------------------------------------------------------------------
  io redefine
---------------------------------------------------------------------------*/

#define set_reg16(a,b,c)        drv_io_write(0,b,c)
#define get_reg16(a,b)          drv_io_read(0,b)
#define poll_reg16(a,b,c,d)     drv_io_polling(0,b,c,d)

/*
#ifdef __cplusplus
}
#endif
*/
#endif


/*
 * linux/drivers/video/odin/s3d/drv_platform.c
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


#include "drv_platform.h"


/*==========================================================
  drv_malloc
===========================================================*/
void* drv_malloc(int32 size)
{
  return kmalloc(size, GFP_KERNEL);
}


/*==========================================================
  drv_free
===========================================================*/
void  drv_free(void* ptr)
{
  kfree(ptr);
}

/*==========================================================
  drv_memcpy
===========================================================*/
void* drv_memcpy(void* dst,const void* src, int count)
{
  return memcpy(dst, src, count);
}


/*==========================================================
  drv_memset
===========================================================*/
void* drv_memset(void* s, int c, int n)
{
  return memset(s,c,n);
}

/*==========================================================
  drv_delay
===========================================================*/
int32 drv_delay(int32 parm, uint32 ms)
{
  int32 ret=RET_OK;

  if (parm == 1)	/* loop delay	*/
  {

  }
  else
  {
    ndelay(ms);
  }

  return ret;
}


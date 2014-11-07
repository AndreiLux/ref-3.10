/*
 * linux/drivers/video/odin/s3d/util_math.c
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

/*===========================================================================
  INCLUDE
===========================================================================*/
#include "util_math.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/ioport.h>
#include <linux/dnotify.h>
#include <linux/file.h>

#include <asm/uaccess.h> /* copy_from_user, copy_to_user */
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/unistd.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <linux/kthread.h>


/*===========================================================================
  DEFINITIONS
===========================================================================*/

/*===========================================================================
  DATA
===========================================================================*/



/*===========================================================================
  FUNCTION     util_math_init

===========================================================================*/
int32 util_math_init  (int32 parm)
{

    return 0;
}


/*===========================================================================
  FUNCTION     util_math_mul

  float a * float b = int32 c
===========================================================================*/
int32 util_math_mul   ( float a, float b )
{
  double c;

  c = __extendsfdf2(__mulsf3(a, b));

  return (int32)__fixdfsi(c);
}

/*===========================================================================
  FUNCTION     util_math_f2i

  float to int
===========================================================================*/
int32 util_math_f2i   ( float a )
{
  double c;

  c = __extendsfdf2(a);

  return (int32)__fixdfsi(c);
}

/*===========================================================================
  FUNCTION     util_math_f2i

  int to float
===========================================================================*/
float util_math_i2f   ( int32 a )
{
  double c;

  c = __floatsidf((long)a);

  return __truncdfsf2(c);
}

/*===========================================================================
  FUNCTION     float2fixed

  float to fixed
===========================================================================*/
int32 float2fixed     ( float data, int fraction_bit )
{
  int32 ret;

  ret = (int32)(data * pow(2,fraction_bit));

  return ret;
}

/*===========================================================================
  FUNCTION     double2fixed

  double to fixed
===========================================================================*/
int64 double2fixed    ( double data, int fraction_bit )
{
  int64 ret;

  ret = (int64)(data * pow(2,fraction_bit));

  return ret;
}

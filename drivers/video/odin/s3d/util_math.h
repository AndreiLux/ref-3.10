/*
 * linux/drivers/video/odin/s3d/util_math.h
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
#ifndef __UTIL_MATH_H__
#define __UTIL_MATH_H__

#include "drv_customer.h"
#include "floatlib.h"

#define INT2FIXED(X)  ( (int)((X) << 16) )
#define FIXED2INT(X)	\
	( (int)(((X) >= 0) ? ((((X) >> 15) + 1) >> 1) : -((((-(X)) >> 15) + 1)


/*---------------------------------------------------------------------------
  EXTERNAL FUNCTION
---------------------------------------------------------------------------*/

int32 util_math_init  ( int32 parm );

int32 util_math_mul   ( float a, float b ); /* float a * float b = int32 c */

int32 util_math_f2i   ( float a );   /* float to int */

float util_math_i2f   ( int32 a );   /* int to float */

int32 float2fixed     ( float data, int fraction_bit );
int64 double2fixed    ( double data, int fraction_bit );
#endif


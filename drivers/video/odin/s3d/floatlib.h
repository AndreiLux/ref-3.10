/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef __FLOATLIB_H__
#define __FLOATLIB_H__

float __addsf3 (float a1, float a2);
float __subsf3 (float a1, float a2);
long __cmpsf2 (float a1, float a2);
float __mulsf3 (float a1, float a2);
float __divsf3 (float a1, float a2);
double __floatsidf (register long a1);
float __negsf2 (float a1);
double __negdf2 (double a1);
double __extendsfdf2 (float a1);
float __truncdfsf2 (double a1);
long __cmpdf2 (double a1, double a2);
long __fixdfsi (double a1);
unsigned long __fixunsdfsi (double a1);

double __adddf3 (double a1, double a2);
double __subdf3 (double a1, double a2);
double __muldf3 (double a1, double a2);
double __divdf3 (double a1, double a2);

#endif

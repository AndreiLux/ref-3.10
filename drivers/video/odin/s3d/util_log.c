/*
 * linux/drivers/video/odin/s3d/util_log.c
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

#include "util_log.h"


/*===========================================================================
  util_log

===========================================================================*/
int32 util_log (int32 parm, char * fmt, ...)
{
  int32 ret=RET_OK;

  char msg[256];

  va_list   (listp);
  va_start  (listp, fmt);
  vsprintf  (msg, fmt, listp);
  va_end    (listp);

#ifdef FEATURE_LOG_ERROR
  if (parm & MSG_LOG_ERROR)
  {
    DRV_PRINT(msg);
  }
#endif
#ifdef FEATURE_LOG_INFO
  if (parm & MSG_LOG_INFO)
  {
    DRV_PRINT(msg);
  }
#endif
#ifdef FEATURE_LOG_FILE
  if (parm & MSG_LOG_FILE)
  {
#if 0
  ret = util_vector_write_file(msg);
#endif
  }
#endif

  return ret;
}


/*===========================================================================
FUNCTION     util_uint16_hex_to_bin
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
===========================================================================*/
char * util_uint16_hex_to_bin(uint16 data)
{
    int i,j;
    static char num[128];

    j=0;
    for (i=0 ; i<16 ; i++)
    {
        if ((uint16)data & (0x8000 >> i))
            num[j++]='1';
        else
            num[j++]='0';

        if (((i+1)%4) == 0)
            num[j++]='.';
    }

    return &num[0];
}


#ifndef FEATURE_BUILD_RELEASE
/*===========================================================================
FUNCTION     util_memory_dump
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
===========================================================================*/
int32 util_memory_dump  (uint64 in_size, void*  in)
{
  int32 ret=RET_OK;
  FILE* fp;
  const uint8 * data_ptr = (const uint8 *)in;
  uint64 i;
  SYSTEMTIME st;
  char name_string[128];

  GetLocalTime(&st);

  sprintf(
      name_string,
      "memory_dump_%04d%02d%02d_%02d%02d%02d.txt",
      st.wYear,
      st.wMonth,
      st.wDay,
      st.wHour,
      st.wMinute,
      st.wSecond);


  fp = fopen(name_string, "wb");

  fprintf( fp, "unsigned char data[] = //size:%d = \r\n { \r\n",in_size );


  for (i=0 ; i < in_size ; i++)
  {
    /* fprintf( fp, "0x%02x,", *data_ptr++ ); */
    fprintf( fp, "%02x ", *data_ptr++ );

    if (i%80 == (80-1))
      fprintf( fp, "\r\n");
  }

  fprintf( fp, " }; \r\n");
  fclose(fp);

  return ret;
}
#endif


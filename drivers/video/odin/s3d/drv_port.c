/*
 * linux/drivers/video/odin/s3d/drv_port.c
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
#include "drv_port.h"
#include "formatter.h"

/*===========================================================================
  DEFINITIONS
===========================================================================*/

/*===========================================================================
  DATA
===========================================================================*/
static boolean init = 0;


/*===========================================================================
  drv_port_open

===========================================================================*/
int32 drv_port_open ( int32 parm )
{
	int32 ret=RET_OK;

	LOGI("drv_port_open \n");

	if (init)
	  return 0;

	init = 1;

	return ret;
}

/*===========================================================================
  drv_port_exit

===========================================================================*/
int32 drv_port_exit ( int32 parm )
{
	int32 ret=RET_OK;

	LOGI("mepsi_port_close \n");

	init = 0;

	return ret;
}

/*===========================================================================
drv_loop_delay

===========================================================================*/
int32 drv_loop_delay ( int32 ms )
{
    int32 ret=RET_OK;

#if 0
    int32 i=0xffff;

    while (ms--)
        while (i--);
#else
#if 0	/* reserved	*/
#include <linux/delay.h>
    mdelay(); /* ms delay */
    udelay(); /* us delay */
    ndelay(); /* us delay */
#endif
#endif

    return ret;
}

/*===========================================================================
  drv_io_polling

===========================================================================*/
int32 drv_io_polling ( void* parm1, uint32 addr, uint8 bit, boolean cond )
{
	int32 ret=RET_OK;

	uint32 reg;
	int i = 10;
	reg = readl(parm1 + addr);
	LOGI("[%s] 0x%x\n",__func__, reg);

	while (i > 0)
	{
		if (((reg >> bit) & 0x0001) == cond)
		{
			break;
		}
		else
		{
			ndelay(100000);
			i--;
		}
	}

	return ret;
}



/*===========================================================================
  drv_io_read

===========================================================================*/
uint32 drv_io_read ( void* parm1, uint32 a )
{
	/* int32 ret=RET_OK;  ret type exception */
	int32 ret=0;

	ret = fmt_read_reg(parm1 + a);
	LOGI("[%s] [0x%x] [0x%x]\n",__func__, a, ret);

	return ret;
}

/*===========================================================================
  drv_io_write

===========================================================================*/
int32 drv_io_write ( void* parm1, uint32 a, uint32 d )
{
	int32 ret=RET_OK;

	fmt_write_reg(a, d);
	LOGI("[%s] [0x%x] [0x%x]\n",__func__, a, d);

	return ret;
}


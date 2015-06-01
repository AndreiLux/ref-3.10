 /* 
  *
  *
  * Copyright (C) 2008,2009 MediaTek <www.mediatek.com>
  * Authors: Infinity Chen <infinity.chen@mediatek.com>  
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
  */

#ifndef __MT_CHIP_H__
#define __MT_CHIP_H__

typedef enum {
    CHIP_SW_VER_01 = 0x0000,
    CHIP_SW_VER_02 = 0x0001
} CHIP_SW_VER;

extern unsigned int get_chip_code(void);
extern unsigned int get_chip_hw_subcode(void);
extern unsigned int get_chip_hw_ver_code(void);
extern unsigned int get_chip_sw_ver_code(void);
extern unsigned int mt_get_chip_id(void);
extern CHIP_SW_VER  mt_get_chip_sw_ver(void);

typedef enum {
    CHIP_INFO_NONE = 0,
    CHIP_INFO_HW_CODE,
    CHIP_INFO_HW_SUBCODE,
    CHIP_INFO_HW_VER,
    CHIP_INFO_SW_VER,
    CHIP_INFO_FUNCTION_CODE,
    CHIP_INFO_PROJECT_CODE,
    CHIP_INFO_DATE_CODE,
    CHIP_INFO_FAB_CODE,
    CHIP_INFO_MAX,
    CHIP_INFO_ALL,
} CHIP_INFO;

extern unsigned int mt_get_chip_info(CHIP_INFO id);

#endif 


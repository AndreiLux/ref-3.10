/*
 *  drivers/video/odin/dss/du.h
 *
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

#ifndef __ODIN_DU_H__
#define __ODIN_DU_H__

enum odin_dss_dest_path {
	ODIN_DEST_OVL0	= 0,	/* Display Interface */
	ODIN_DEST_OVL1	= 1,
	ODIN_DEST_OVL2	= 2,
	ODIN_DEST_WB0	= 3,	/* Memory Write back */
};

enum odin_burst_size {
	ODIN_DSS_BURST_4X32 = 0,
	ODIN_DSS_BURST_8X32 = 1,
	ODIN_DSS_BURST_16X32 = 2,
};

enum odin_interrupt {
	ODIN_VSYNC_MIPI0_INT		= 0,
	ODIN_VSYNC_MIPI1_INT		= 1,
	ODIN_VSYNC_HDMI_INT		= 2,
	ODIN_FMT_INT			= 3,
	ODIN_XD_INT			= 4,
	ODIN_RTT_INT			= 5,
	ODIN_DIP0_INT			= 6,
	ODIN_MIPI0_INT			= 7,
	ODIN_DIP1_INT			= 8,
	ODIN_MIPI1_INT			= 9,
	ODIN_DIP2_INT			= 10,
	ODIN_HDMI_INT			= 11,
	ODIN_HDMI_WAKEUP_INT		= 12,
	ODIN_CABC0_INT			= 13,
	ODIN_CABC1_INT			= 14,
};


struct seq_file;
struct platform_device;

#endif



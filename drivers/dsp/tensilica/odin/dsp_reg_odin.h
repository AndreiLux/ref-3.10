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

/*----------------------------------------------------------
  Audio Control Register 0x34660000
----------------------------------------------------------*/

/*----------------------------------------------------------
  0x0 DSP_Control
----------------------------------------------------------*/
typedef struct
{
	unsigned int ocd_halt_on_reset		:1;		/* 0  */
	unsigned int run_stall				:1;		/* 1  */
	unsigned int stat_vecter_sel		:1;		/* 2  */
	unsigned int dsp_reset				:1;		/* 3  */
} DSP_CONTROL;

/*----------------------------------------------------------
  0x4 DSP_Status0
----------------------------------------------------------*/
typedef struct
{
	unsigned int pwait_mode				:1;		/* 0 */
	unsigned int xocd_mode				:1;		/* 1 */
} DSP_STATUS0;

/*----------------------------------------------------------
  0x8 DSP_Status1
----------------------------------------------------------*/
typedef struct
{
	unsigned int dsp_status1			:32;	/* 0:31 */
} DSP_STATUS1;

/*----------------------------------------------------------
  0xC DSP Address Remap
----------------------------------------------------------*/
typedef struct
{
	unsigned int dsp_remap				:7;		/* 0:6	*/
} DSP_REMAP;

/*----------------------------------------------------------
  0x10 DSP Version
----------------------------------------------------------*/
typedef struct
{
	unsigned int dsp_version			:32;	/* 0:31 */
} DSP_VERSION;

/*----------------------------------------------------------
  0x14 Cache Memory Control
----------------------------------------------------------*/
typedef struct
{
	unsigned int cache_mux_control			:1;	/* 0	*/
	unsigned int cache_pwr_control			:2;	/* 1:2  */
} CACHE_MEMORY_CONTROL;

/*----------------------------------------------------------
  0x18 IRAM Memory Control
----------------------------------------------------------*/
typedef struct
{
	unsigned int iram_mux_control			:2;	/* 0:1  */
	unsigned int iram_pwr_control			:4;	/* 2:5  */
} IRAM_MEMORY_CONTROL;

/*----------------------------------------------------------
  0x1C DRAM Memory Control
----------------------------------------------------------*/
typedef struct
{
	unsigned int dram_mux_control			:8;		/* 0:7   */
	unsigned int dram_pwr_control			:16;	/* 8:23  */
} DRAM_MEMORY_CONTROL;

/*----------------------------------------------------------
  0x20 Audio SRAM Memory Control
----------------------------------------------------------*/
typedef struct
{
	unsigned int sram_pwr_control		:2;		/* 0:1 */
} AUD_SRAM_MEMORY_CONTROL;

/*----------------------------------------------------------
  0x24 DSP Interrupt Mux Control0
----------------------------------------------------------*/
typedef struct
{
	unsigned int dsp_intr_mux_control0	:5;	/* 0:4    */
	unsigned int reserved0				:3;	/* 5:7    */
	unsigned int dsp_intr_mux_control1	:5;	/* 8:12   */
	unsigned int reserved1				:3;	/* 13:15  */
	unsigned int dsp_intr_mux_control2	:5;	/* 16:20  */
	unsigned int reserved2				:3;	/* 21:23  */
	unsigned int dsp_intr_mux_control3	:5;	/* 24:28  */
	unsigned int reserved3				:3;	/* 29:31  */
} DSP_INTR_MUX_CONTROL0;

/*----------------------------------------------------------
  0x28 DSP Interrupt Mux Control4
----------------------------------------------------------*/
typedef struct
{
	unsigned int dsp_intr_mux_control4	:5;	/* 0:4    */
	unsigned int reserved4				:3;	/* 5:7    */
	unsigned int dsp_intr_mux_control5	:5;	/* 8:12   */
	unsigned int reserved5				:3;	/* 13:15  */
	unsigned int dsp_intr_mux_control6	:5;	/* 16:20  */
	unsigned int reserved6				:3;	/* 21:23  */
	unsigned int dsp_intr_mux_control7	:5;	/* 24:28  */
	unsigned int reserved7				:3;	/* 29:31  */
} DSP_INTR_MUX_CONTROL4;

/*----------------------------------------------------------
  0x2C DSP Interrupt Mux Control8
----------------------------------------------------------*/
typedef struct
{
	unsigned int dsp_intr_mux_control8	:5;	/* 0:4    */
	unsigned int reserved8				:3;	/* 5:7    */
	unsigned int dsp_intr_mux_control9	:5;	/* 8:12   */
	unsigned int reserved9				:3;	/* 13:15  */
	unsigned int dsp_intr_mux_control10	:5;	/* 16:20  */
	unsigned int reserved10				:3;	/* 21:23  */
	unsigned int dsp_intr_mux_control11	:5;	/* 24:28  */
	unsigned int reserved11				:3;	/* 29:31  */
} DSP_INTR_MUX_CONTROL8;

/*----------------------------------------------------------
  0x30 DSP Interrupt Mux Control12
----------------------------------------------------------*/
typedef struct
{
	unsigned int dsp_intr_mux_control12	:5;	/* 0:4   */
	unsigned int reserved12				:3;	/* 5:7   */
	unsigned int dsp_intr_mux_control13	:5;	/* 8:12  */
	unsigned int reserved13				:3;	/* 13:15 */
	unsigned int dsp_intr_mux_control14	:5;	/* 16:20 */
	unsigned int reserved14				:3;	/* 21:23 */
	unsigned int dsp_intr_mux_control15	:5;	/* 24:28 */
	unsigned int reserved15				:3;	/* 29:31 */
} DSP_INTR_MUX_CONTROL12;

/*----------------------------------------------------------
  0x34 DSP Interrupt Mux Control16
----------------------------------------------------------*/
typedef struct
{
	unsigned int dsp_intr_mux_control16	:5;	/* 0:4    */
	unsigned int reserved16				:3;	/* 5:7    */
	unsigned int dsp_intr_mux_control17	:5;	/* 8:12   */
	unsigned int reserved17				:3;	/* 13:15  */
	unsigned int dsp_intr_mux_control18	:5;	/* 16:20  */
	unsigned int reserved18				:3;	/* 21:23  */
	unsigned int dsp_intr_mux_control19	:5;	/* 24:28  */
	unsigned int reserved19				:3;	/* 29:31  */
} DSP_INTR_MUX_CONTROL16;

/*----------------------------------------------------------
  0x38 DSP Interrupt Mux Control20
----------------------------------------------------------*/
typedef struct
{
	unsigned int dsp_intr_mux_control20	:5;	/* 0:4    */
	unsigned int reserved20				:3;	/* 5:7    */
	unsigned int dsp_intr_mux_control21	:5;	/* 8:12   */
	unsigned int reserved21				:3;	/* 13:15  */
	unsigned int dsp_intr_mux_control22	:5;	/* 16:20  */
	unsigned int reserved22				:3;	/* 21:23  */
} DSP_INTR_MUX_CONTROL20;

/*----------------------------------------------------------
  0x3C DMA Mux
----------------------------------------------------------*/
typedef struct
{
	unsigned int sel_dma_conn0			:2;	/* 0:1 */
	unsigned int reserved				:2; /* 2:3 */
	unsigned int sel_dma_conn1			:2;	/* 4:5 */
} DMA_MUX;

/*----------------------------------------------------------
  0x40 I2S_CONN0
----------------------------------------------------------*/
typedef struct
{
	unsigned int i2s_conn0				:32;	/* 0:31 */
} I2S_CONN0;

/*----------------------------------------------------------
  0x44 I2S_CONN1
----------------------------------------------------------*/
typedef struct
{
	unsigned int i2s_conn1				:32;	/* 0:31 */
} I2S_CONN1;

/*----------------------------------------------------------
  0x48 I2S_CONN2
----------------------------------------------------------*/
typedef struct
{
	unsigned int i2s_conn2				:32;	/* 0:31 */
} I2S_CONN2;

/*----------------------------------------------------------
  0x4C I2S_CONN3
----------------------------------------------------------*/
typedef struct
{
	unsigned int i2s_conn3				:32;	/* 0:31	*/
} I2S_CONN3;

/*----------------------------------------------------------
  0x50 I2S_CONN4
----------------------------------------------------------*/
typedef struct
{
	unsigned int i2s_conn4				:32;	/* 0:31	*/
} I2S_CONN4;

/*----------------------------------------------------------
  0x58 Memory Conflict Interrupt Mask
----------------------------------------------------------*/
typedef struct
{
	unsigned int mem_conflict_intr_mask	:28;	/* 0:31 */
} MEM_CONFLICT_INTR_MASK;

/*----------------------------------------------------------
  0x5C Memory Conflict Interrupt Status
----------------------------------------------------------*/
typedef struct
{
	unsigned int mem_conflict_intr_status:28;	/* 0:31 */
} MEM_CONFLICT_INTR_STATUS;

/*----------------------------------------------------------
  0x60 Memory Conflict Interrupt Clear
----------------------------------------------------------*/
typedef struct
{
	unsigned int mem_conflict_intr_clear:1;		/* 0 */
} MEM_CONFLICT_INTR_CLEAR;

/*----------------------------------------------------------
  0x64 JTAG Select Signal
----------------------------------------------------------*/
typedef struct
{
	unsigned int jtags_sel				:1;		/* 0 */
} JTAGS_SEL;

/*----------------------------------------------------------
  0x68 DSP Temp Register 1
----------------------------------------------------------*/
typedef struct
{
	unsigned int temp_reg1				:32;	/* 0:31 */
} DSP_TEMP_REG1;

/*----------------------------------------------------------
  0x6C DSP Temp Register 2
----------------------------------------------------------*/
typedef struct
{
	unsigned int temp_reg2				:32;	/* 0:31 */
} DSP_TEMP_REG2;

/*----------------------------------------------------------
  0x70 DSP Temp Register 3
----------------------------------------------------------*/
typedef struct
{
	unsigned int temp_reg3				:32;	/* 0:31 */
} DSP_TEMP_REG3;



typedef struct {
	DSP_CONTROL						dsp_control;
	DSP_STATUS0						dsp_status0;
	DSP_STATUS1						dsp_status1;
	DSP_REMAP						dsp_remap;
	DSP_VERSION						dsp_version;
	CACHE_MEMORY_CONTROL			cache_mem_ctl;
	IRAM_MEMORY_CONTROL				iram_mem_ctl;
	DRAM_MEMORY_CONTROL				dram_mem_ctl;
	AUD_SRAM_MEMORY_CONTROL			sram_mem_ctl;
	DSP_INTR_MUX_CONTROL0			dsp_intr_mux_ctl0;
	DSP_INTR_MUX_CONTROL4			dsp_intr_mux_ctl4;
	DSP_INTR_MUX_CONTROL8			dsp_intr_mux_ctl8;
	DSP_INTR_MUX_CONTROL12			dsp_intr_mux_ctl12;
	DSP_INTR_MUX_CONTROL16			dsp_intr_mux_ctl16;
	DSP_INTR_MUX_CONTROL20			dsp_intr_mux_ctl20;
	DMA_MUX							dma_mux;
	I2S_CONN0						i2s_conn0;
	I2S_CONN1						i2s_conn1;
	I2S_CONN2						i2s_conn2;
	I2S_CONN3						i2s_conn3;
	I2S_CONN4						i2s_conn4;
	unsigned int					reserved;
	MEM_CONFLICT_INTR_MASK			mem_conflict_intr_mask;
	MEM_CONFLICT_INTR_STATUS		mem_conflict_intr_status;
	MEM_CONFLICT_INTR_CLEAR			mem_conflict_intr_clear;
	JTAGS_SEL						jtags_sel;		
	DSP_TEMP_REG1					dsp_temp_reg1;
	DSP_TEMP_REG2					dsp_temp_reg2;
	DSP_TEMP_REG3					dsp_temp_reg3;
} AUD_CONTROL_ODIN;


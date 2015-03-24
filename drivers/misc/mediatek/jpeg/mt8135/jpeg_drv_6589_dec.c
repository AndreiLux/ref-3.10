#include <mach/mt_typedefs.h>
#include <mach/sync_write.h>

#include "jpeg_drv_6589_reg.h"


#include "jpeg_drv_6589_common.h"
#include "jpeg_drv_6589_reg.h"

/* #define DUMP_REG_CMD */

#define ALIGN_MASK(BIT)		(((unsigned int)(BIT) >> 3) - 1)

#define CHECK_ALIGN(value, align, addr) {\
	if (value & (align-1))\
		pr_info("WriteREG: Try to write %d to REG(%x) without %d align!!\n ", value, addr, align);\
}


#define TEST_JPEG_DEBUG_EN

/* unsigned int _jpeg_dec_int_status = 0; */
unsigned int _jpeg_dec_dump_reg_en = 0;

kal_uint32 _jpeg_dec_int_status = 0;

kal_uint32 _jpeg_dec_mode = 0;

int jpeg_isr_dec_lisr(void)
{
	unsigned int tmp, tmp1;

	tmp1 = REG_JPGDEC_INTERRUPT_STATUS;
	tmp = tmp1 & BIT_INQST_MASK_ALLIRQ;
	/* pr_info("enter jpeg_isr_dec_lisr %d, mode %d!!\n", _jpeg_dec_int_status, _jpeg_dec_mode); */

	if (tmp) {
		_jpeg_dec_int_status = tmp;

		if (_jpeg_dec_mode == 1 && (tmp & BIT_INQST_MASK_PAUSE)) {
			return 0;
		} else {
			/* / clear the interrupt status register */
			REG_JPGDEC_INTERRUPT_STATUS = tmp;
			return 0;
		}
	}

	return -1;
}





void jpeg_drv_dec_start(void)
{
	/* REG_JPEG_DEC_TRIG = 1; */
	/* mt65xx_reg_sync_writel(0x1, REG_ADDR_JPGDEC_TRIG); */
	/* pr_info("trigger the decoder HW!!\n"); */
	REG_JPGDEC_TRIG = 0;
}


/**
 * Call this function to reset the JPEG decoder.
 */



void jpeg_drv_dec_soft_reset(void)
{
	REG_JPGDEC_RESET = 0x00;
	REG_JPGDEC_RESET = 0x01;
	/* REG_JPGDEC_RESET = 0x00; */

	_jpeg_dec_int_status = 0;

}

void jpeg_drv_dec_reset(void)
{
	jpeg_drv_dec_soft_reset();

	jpeg_drv_dec_hard_reset();

}

void jpeg_drv_dec_hard_reset(void)
{
	REG_JPGDEC_RESET = 0x00;
	REG_JPGDEC_RESET = 0x10;
	/* REG_JPGDEC_RESET = 0x00; */

	_jpeg_dec_int_status = 0;

}



void wait_pr(void)
{
	unsigned int timeout1 = 0xF;
	unsigned int timeout2 = 0xFFFFF;
	unsigned int timeout3 = 0xFFFFFFF;


	while (timeout1 > 0) {
		while (timeout2 > 0) {
			while (timeout3 > 0) {
				timeout3--;
			}
			timeout2--;
		}
		timeout1--;
	}
}

void jpeg_drv_dec_set_brz_factor(unsigned char yHScale, unsigned char yVScale,
				 unsigned char cbcrHScale, unsigned char cbcrVScale)
{
	unsigned int u4Value;


	/* yHScale =  yHScale; */
	/* yVScale =  yVScale; */
	/* cbcrHScale = cbcrHScale; */
	/* cbcrVScale = cbcrVScale; */
#if 0
	if (srcFormat == JPG_COLOR_444 ||
	    srcFormat == JPG_COLOR_422V || srcFormat == JPG_COLOR_422Vx2) {

		cbcrHScale++;
	}
#endif
	u4Value = (cbcrVScale << BIT_BRZ_CV_SHIFT) | (cbcrHScale << BIT_BRZ_CH_SHIFT) |
	    (yVScale << BIT_BRZ_YV_SHIFT) | (yHScale << BIT_BRZ_YH_SHIFT);

	REG_JPGDEC_BRZ_FACTOR = u4Value;

	/* pr_info("[JPEGDRV] BRZ 0x%08x!!\n", u4Value); */




}


void jpeg_drv_dec_set_dst_bank0(unsigned int addr_Y, unsigned int addr_U, unsigned int addr_V)
{


	REG_JPGDEC_DEST_ADDR0_Y = addr_Y;
	REG_JPGDEC_DEST_ADDR0_U = addr_U;
	REG_JPGDEC_DEST_ADDR0_V = addr_V;

}


void jpeg_drv_dec_set_dst_bank1(unsigned int addr_Y, unsigned int addr_U, unsigned int addr_V)
{
/* unsigned int u4Value; */

	REG_JPGDEC_DEST_ADDR1_Y = addr_Y;
	REG_JPGDEC_DEST_ADDR1_U = addr_U;
	REG_JPGDEC_DEST_ADDR1_V = addr_V;

}


int jpeg_drv_dec_set_memStride(unsigned int CompMemStride_Y, unsigned int CompMemStride_UV)
{

	REG_JPGDEC_STRIDE_Y = CompMemStride_Y & 0xFFFF;
	REG_JPGDEC_STRIDE_UV = CompMemStride_UV & 0xFFFF;

	return (int)E_HWJPG_OK;
}






int jpeg_drv_dec_set_imgStride(unsigned int CompStride_Y, unsigned int CompStride_UV)
{
/* unsigned int u4Reg; */

	REG_JPGDEC_IMG_STRIDE_Y = CompStride_Y & 0xFFFF;
	REG_JPGDEC_IMG_STRIDE_UV = CompStride_UV & 0xFFFF;

	return (int)E_HWJPG_OK;
}





void jpeg_drv_dec_set_pause_mcu_idx(unsigned int McuIdx)
{

	REG_JPGDEC_PAUSE_MCU_NUM = McuIdx & 0x0003FFFFFF;

}



void jpeg_drv_dec_set_dec_mode(int i4DecMode)
{

	unsigned int u4Value = i4DecMode;

	/* 0: full frame, 1: direct couple mode, 2: pause/resume mode, 3: Reserved */

	if (u4Value > 0x02)
		pr_info("Warning : try to set invalid decode mode, %d!!\n", u4Value);
	REG_JPGDEC_OPERATION_MODE = u4Value & 0x03;




	/* pr_info("[JPEGDRV] DECMODE %x!!\n", u4Value); */
}

void jpeg_drv_dec_set_debug_mode(void)
{
	unsigned int u4Value;

	u4Value = REG_JPGDEC_DEBUG_MODE;

	u4Value |= 0x80000000;
	REG_JPGDEC_DEBUG_MODE = u4Value;
	/* pr_info("WriteREG(%x, 32'h%08x);\n", 0x00c8, u4Value); */

	return;
}



void jpeg_drv_dec_set_bs_writePtr(unsigned int writePtr)
{

	CHECK_ALIGN(writePtr, 16, REG_ADDR_JPGDEC_FILE_BRP);

	REG_JPGDEC_FILE_BRP = writePtr;


}


void jpeg_drv_dec_set_bs_info(unsigned int bsBase, unsigned int bsSize)
{

	CHECK_ALIGN(bsBase, 16, REG_ADDR_JPGDEC_FILE_ADDR);
	CHECK_ALIGN(bsSize, 128, REG_ADDR_JPGDEC_FILE_TOTAL_SIZE);

	REG_JPGDEC_FILE_ADDR = bsBase;

	REG_JPGDEC_FILE_TOTAL_SIZE = bsSize;


}

/* void jpeg_drv_dec_set_total_bs_size_align128(unsigned int bsSize) */
/* { */
/* unsigned int u4tmp; */
/*  */
/* if(bsSize & 127){ */
/* pr_info("WriteREG: REG_JPGDEC_FILE_TOTAL_SIZE Waring %x !!\n", bsSize); */
/* u4tmp = bsSize & (~127) ; */
/* }else */
/* u4tmp = bsSize ; */
/* REG_JPGDEC_FILE_TOTAL_SIZE  =  u4tmp; */
/*  */
/*  */
/* } */



void jpeg_drv_dec_set_comp_id(unsigned int Y_ID, unsigned int U_ID, unsigned int V_ID)
{
	unsigned int u4Value;

	u4Value = ((Y_ID & 0x00FF) << 24) | ((U_ID & 0x00FF) << 16) | ((V_ID & 0x00FF) << 8);
	REG_JPGDEC_COMP_ID = u4Value;


}

void jpeg_drv_dec_set_total_mcu(unsigned int TotalMcuNum)
{
	unsigned int u4Value;
	u4Value = TotalMcuNum - 1;
	REG_JPGDEC_TOTAL_MCU_NUM = u4Value;

}


void jpeg_drv_dec_set_comp0_du(unsigned int GrayDuNum)
{
	unsigned int u4Value;
	u4Value = GrayDuNum - 1;
	REG_JPGDEC_COMP0_DATA_UNIT_NUM = u4Value;

}


void jpeg_drv_dec_set_du_membership(unsigned int u4Membership, unsigned int GMC_en,
				    unsigned int IsGray)
{
#if 0
	/* u4Membership = u4Membership | 0x3FFFFF24 ;//111_111_111_111_111_111_111_100_100_100 */
	u4Membership = u4Membership | 0x24924924;	/* 100_100_100_100_100_100_100_100_100_100 */
#endif

	if (IsGray)
		u4Membership = (IsGray << 31) | (GMC_en << 30) | 0x3FFFFFFC;
	else
		u4Membership = (IsGray << 31) | (GMC_en << 30) | u4Membership;
	REG_JPGDEC_DU_CTRL = u4Membership;


	/* pr_info("[JPEGDRV] membership 0x%08x!!\n", u4Membership); */
}


/* set q table for each component */
void jpeg_drv_dec_set_q_table(kal_uint32 id0, kal_uint32 id1, kal_uint32 id2)
{
	REG_JPGDEC_QT_ID = ((id0 & 0x0f) << 8) | ((id1 & 0x0f) << 4) | ((id2 & 0x0f) << 0);
#ifdef DUMP_REG_CMD
	pr_info("WriteREG(VLD_REG_OFST , 32'h%08x);\n",
		((id0 & 0x0f) << 8) | ((id1 & 0x0f) << 4) | ((id2 & 0x0f) << 0));
#endif
}


unsigned int jpeg_drv_dec_get_irqState(void)
{
	unsigned int u4Value;
	u4Value = REG_JPGDEC_INTERRUPT_STATUS;
	REG_JPGDEC_INTERRUPT_STATUS = u4Value;	/* write clear */
	return u4Value;

}

unsigned int jpeg_drv_dec_get_decState(void)
{
	unsigned int u4Value;
	u4Value = REG_JPGDEC_STATUS;
	pr_info("JPED_DEC_DRV: STATUS %x!!\n", u4Value);
	return u4Value;

}


unsigned int jpeg_drv_dec_get_decMCU(void)
{
	unsigned int u4Value;
	u4Value = REG_JPGDEC_MCU_CNT;
	pr_info("JPED_DEC_DRV: MCU_CNT %x!!\n", u4Value);
	return u4Value;

}




void jpeg_drv_dec_set_dma_group(unsigned int McuInGroup, unsigned int GroupNum,
				unsigned int LastMcuNum)
{
	unsigned int McuInGroup_1 = McuInGroup - 1;
	unsigned int GroupNum_1 = GroupNum - 1;
	unsigned int LastMcuNum_1 = LastMcuNum - 1;

	REG_JPGDEC_WDMA_CTRL =
	    ((McuInGroup_1 & 0x00FF) << 16) | ((GroupNum_1 & 0x007F) << 8) | (LastMcuNum_1 &
									      0x00FF);

}




void jpeg_drv_dec_set_sampling_factor(unsigned int compNum, unsigned int u4Y_H, unsigned int u4Y_V,
				      unsigned int u4U_H, unsigned int u4U_V, unsigned int u4V_H,
				      unsigned int u4V_V)
{
	unsigned int u4Value = 0;
	unsigned int u4Y_HV = (DUNUM_MAPPING(u4Y_H) << 2) | DUNUM_MAPPING(u4Y_V);
	unsigned int u4U_HV = (DUNUM_MAPPING(u4U_H) << 2) | DUNUM_MAPPING(u4U_V);
	unsigned int u4V_HV = (DUNUM_MAPPING(u4V_H) << 2) | DUNUM_MAPPING(u4V_V);
	/* unsigned int MCU_HV[3] ; */


	if (compNum == 1)
		u4Value = 0;	/* u4Y_HV << 8; */
	else
		u4Value = (u4Y_HV << 8) | (u4U_HV << 4) | u4V_HV;

	REG_JPGDEC_DU_SAMPLE = u4Value;

}



int jpeg_drv_dec_set_config_data(JPEG_DEC_DRV_IN *config)
{


	jpeg_drv_dec_set_sampling_factor(config->componentNum,
					 config->hSamplingFactor[0], config->vSamplingFactor[0],
					 config->hSamplingFactor[1], config->vSamplingFactor[1],
					 config->hSamplingFactor[2], config->vSamplingFactor[2]);

	/* set BRZ factor */
	jpeg_drv_dec_set_brz_factor(config->lumaHorDecimate, config->lumaVerDecimate,
				    config->cbcrHorDecimate, config->cbcrVerDecimate);

	/* set group DMA */
	jpeg_drv_dec_set_dma_group(config->dma_McuInGroup, config->dma_GroupNum,
				   config->dma_LastMcuNum);

	/* set componet ID */
	jpeg_drv_dec_set_comp_id(config->componentID[0], config->componentID[1],
				 config->componentID[2]);


	/* set BLK membership */
	/* pr_info("[JPEGDRV] Membership List = 0x%08x\n", config->membershipList); */
	jpeg_drv_dec_set_du_membership(config->membershipList, config->gmcEn,
				       (config->componentNum == 1) ? 1 : 0);

	/* set q table id */
	jpeg_drv_dec_set_q_table(config->qTableSelector[0], config->qTableSelector[1],
				 config->qTableSelector[2]);

	/* set dst image stride  */
	jpeg_drv_dec_set_imgStride(config->compImgStride[0], config->compImgStride[1]);

	/* set dst Memory stride  */
	/* if( config->pauseRow_en ){ */
	/* jpeg_drv_dec_set_memStride(config->compTileBufStride[0], config->compTileBufStride[1]); */
	/* }else{ */
	jpeg_drv_dec_set_memStride(config->compMemStride[0], config->compMemStride[1]);
	/* } */

	/* set total MCU number */
	jpeg_drv_dec_set_total_mcu(config->totalMCU);

	/* set Gray DU number */
	jpeg_drv_dec_set_comp0_du(config->comp0_DU);

	/* set pause MCU index */
	jpeg_drv_dec_set_pause_mcu_idx(config->pauseMCU - 1);

	/* set bitstream base, size */
	pr_info("[JPEGDRV] mode %d, Buf Base 0x%08x, Limit 0x%08x, Size 0x%08x!!\n",
		config->reg_OpMode, config->srcStreamAddrBase, config->srcStreamAddrWritePtr,
		config->srcStreamSize);
	jpeg_drv_dec_set_bs_info(config->srcStreamAddrBase, config->srcStreamSize);

	/* set bitstream write pointer */
	jpeg_drv_dec_set_bs_writePtr(config->srcStreamAddrWritePtr);

	/* set Decode Operation Mode */
	/* pr_info("[JPEGDRV]SET OPMODE %d, DecMODE %d!!", config->reg_OpMode, config->decodeMode); */
	jpeg_drv_dec_set_dec_mode(config->reg_OpMode);	/* set full frame or pause/resume */


	/* output bank 0 */
	jpeg_drv_dec_set_dst_bank0(config->outputBuffer0[0], config->outputBuffer0[1],
				   config->outputBuffer0[2]);

	/* output bank 1 */
	jpeg_drv_dec_set_dst_bank1(config->outputBuffer1[0], config->outputBuffer1[1],
				   config->outputBuffer1[2]);


#ifdef TEST_JPEG_DEBUG_EN
	jpeg_drv_dec_set_debug_mode();
#endif


	return (int)E_HWJPG_OK;
}


void jpeg_drv_dec_resume(unsigned int resume)
{


	REG_JPGDEC_INTERRUPT_STATUS = resume;


}

int jpeg_drv_dec_wait_one_row(JPEG_DEC_DRV_IN *config)
{
	unsigned int timeout = 0x2FFFFF;
	unsigned int irq_status;

	unsigned int tri_cnt = ++config->pauseRowCnt;	/* 1; */
	unsigned int MCU_cnt = 0;
	unsigned int base_Y = config->buffer_Y_PA;	/* 0x89080000; */
	unsigned int base_CB = config->buffer_Cb_PA;	/* 0x89040000; */
	unsigned int base_CR = config->buffer_Cr_PA;	/* 0x89100000; */
	unsigned int ring_row_index = tri_cnt % config->tileBufRowNum;

	/* for( tri_cnt = 1 ; tri_cnt <= 60 ; tri_cnt++) */
	{
		/* wait done */
		if (config->decodeMode == JPEG_DEC_MODE_MCU_ROW) {
			while ((REG_JPGDEC_INTERRUPT_STATUS & BIT_INQST_MASK_ALLIRQ) == 0) {
				timeout--;
				if (timeout == 0)
					break;
			}
		} else {
			while ((REG_JPGDEC_INTERRUPT_STATUS & BIT_INQST_MASK_ALLIRQ) == 0) {
				timeout--;
				if (timeout == 0)
					break;
			}

		}
		irq_status = REG_JPGDEC_INTERRUPT_STATUS;

		MCU_cnt = config->mcuPerRow * (tri_cnt + 1);
		/* MCU_cnt = config->u4McuNumInRow * (tri_cnt+1) ; */

		pr_info
		    ("JPEG_DEC_WAIT_DONE: tri_cnt %d, irq %x, MCUinRow %d, p_idx %d, %x %x %x!!\n",
		     tri_cnt, irq_status, config->mcuPerRow, MCU_cnt,
		     base_Y + ring_row_index * (config->buffer_Y_row_size),
		     base_CB + ring_row_index * (config->buffer_C_row_size),
		     base_CR + ring_row_index * (config->buffer_C_row_size));

		jpeg_drv_dec_set_dst_bank0(base_Y + ring_row_index * (config->buffer_Y_row_size),
					   base_CB + ring_row_index * (config->buffer_C_row_size),
					   base_CR + ring_row_index * (config->buffer_C_row_size));

		jpeg_drv_dec_set_pause_mcu_idx(MCU_cnt - 1);




		REG_JPGDEC_INTERRUPT_STATUS = irq_status;	/*write clear */

		/* Debug: jpeg_drv_dec_dump_reg(); */
		if (timeout == 0) {
			pr_info("Error! Decode Timeout.\n");
			jpeg_drv_dec_dump_reg();
			return 0;
		}

		pr_info("JPEG Decode Success, st %x!!\n", irq_status);


	}
	return 1;
}


int jpeg_drv_dec_wait(JPEG_DEC_DRV_IN *config)
{
	unsigned int timeout = 0x2FFFFF;
	unsigned int irq_status;
	/* wait done */
	if (config->decodeMode == JPEG_DEC_MODE_MCU_ROW) {
		while ((REG_JPGDEC_INTERRUPT_STATUS & BIT_INQST_MASK_ALLIRQ) == 0) {
			timeout--;
			if (timeout == 0)
				break;
		}
	} else {
		while ((REG_JPGDEC_INTERRUPT_STATUS & BIT_INQST_MASK_ALLIRQ) == 0) {
			timeout--;
			if (timeout == 0)
				break;
		}

	}
	irq_status = REG_JPGDEC_INTERRUPT_STATUS;

	REG_JPGDEC_INTERRUPT_STATUS = irq_status;	/*write clear */

	/* Debug: jpeg_drv_dec_dump_reg(); */
	if (timeout == 0) {
		pr_info("Error! Decode Timeout.\n");
		jpeg_drv_dec_dump_reg();
		return 0;
	}

	pr_info("JPEG Decode Success, st %x!!\n", irq_status);
	return 1;
}

kal_uint32 jpeg_drv_dec_get_result(void)
{
	if (_jpeg_dec_int_status & BIT_INQST_MASK_EOF) {
		return 0;
	} else if (_jpeg_dec_int_status & BIT_INQST_MASK_PAUSE) {
		return 1;
	} else if (_jpeg_dec_int_status & BIT_INQST_MASK_UNDERFLOW) {
		return 2;
	} else if (_jpeg_dec_int_status & BIT_INQST_MASK_OVERFLOW) {
		return 3;
	}

	else if (_jpeg_dec_int_status & BIT_INQST_MASK_ERROR_BS) {
		return 4;
	}

	return 5;
}

int jpeg_drv_dec_break(void)
{
	unsigned int timeout = 0xFFFFF;

	jpeg_drv_dec_soft_reset();

	while (((REG_JPGDEC_STATUS & BIT_DEC_ST_STATE_MASK) == BIT_DEC_ST_STATE_IDLE)) {
		timeout--;
		if (timeout == 0)
			break;
	}
	if (timeout == 0)
		return -1;
	return 0;
}

void jpeg_drv_dec_dump_reg(void)
{
	unsigned int reg_value = 0;
	unsigned int index = 0;

	pr_info("<<<<<= JPEG DEC DUMP =>>>>>\n");
	for (index = 0x8C; index <= 0x3FC; index += 4) {
#ifdef FPGA_VERSION
		reg_value = *(volatile kal_uint32 *)(JPEG_DEC_BASE + index);
#else
		reg_value = ioread32((void *)(JPEG_DEC_BASE + index));
#endif
		pr_info("+0x%x(%d) 0x%08x\n", index, index / 4, reg_value);
		wait_pr();
	}
}

void jpeg_drv_dec_rw_reg(void)
{
	kal_uint32 i;
	kal_uint32 addr = 0;
	/* kal_uint32 restore = 0; */


	pr_info("=======================================\n\r");
	pr_info("   JPEG decoder register RW test!!!!\n\r");

	/* for (i = 0x8C; i < 0x3FC; i+=4) */
	for (i = 0x090; i <= 0x294; i += 4) {
		addr = JPEG_DEC_BASE + i;
		pr_info("addr %03x(%03d) ", i, i / 4);

#if 0
		/* power down related register */
		if ((i == 0xC4) || (i == 0xC8))
			restore = *((volatile unsigned int *)addr);
#endif

		*((volatile unsigned int *)addr) = 0x00000000;
		pr_info("write 0x00000000 read: 0x%08x\n", *((volatile unsigned int *)addr));

		*((volatile unsigned int *)addr) = 0xffffffff;
		pr_info("              write 0xffffffff read: 0x%08x\n",
			*((volatile unsigned int *)addr));
#if 0
		/* power down related register */
		if ((i == 0xC4) || (i == 0xC8))
			*((volatile unsigned int *)addr) = restore;
#endif

		wait_pr();
	}

	pr_info("=======================================\n\r\n\r");

}

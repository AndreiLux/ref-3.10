#include <mach/mt_clkmgr.h>
#include <mach/sync_write.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "mt_smi.h"

static void __iomem *(SMILarbBaseAddr[SMI_LARB_NUMBER]);
static void __iomem *(SMICommBaseAddr[SMI_COMM_NUMBER]);

static char *(of_smi_larb_desc[]) = {
	"mediatek,SMI_LARB0",
	"mediatek,SMI_LARB1",
	"mediatek,SMI_LARB2",
	"mediatek,SMI_LARB3",
	"mediatek,SMI_LARB5"
};

static char *(of_smi_comm_desc[]) = {
	"mediatek,SMI_COMMON"
};

int MET_SMI_Init(void)
{
	struct device_node	*node = NULL;
	int			i;
	int			err = 0;

	if ((ARRAY_SIZE(of_smi_larb_desc) != SMI_LARB_NUMBER)
	    || (ARRAY_SIZE(of_smi_comm_desc) != SMI_COMM_NUMBER))
		return -1;

	/* map address for SMI_LARBs */
	for (i = 0; i < SMI_LARB_NUMBER; i++) {
		node = of_find_compatible_node(NULL, NULL, of_smi_larb_desc[i]);
		if (node == NULL) {
			err = -2;
			break;
		}
		SMILarbBaseAddr[i] = of_iomap(node, 0);
		of_node_put(node);
		if (SMILarbBaseAddr[i] == 0) {
			err = -3;
			break;
		}
		printk("MET SMI: of_iomap <%s> to %p\n",
			of_smi_larb_desc[i], SMILarbBaseAddr[i]);
	}
	if (err) {
		while (i) {
			i--;
			iounmap(SMILarbBaseAddr[i]);
		}
		return err;
	}

	/* map address for SMI_COMMs */
	for (i = 0; i < SMI_COMM_NUMBER; i++) {
		node = of_find_compatible_node(NULL, NULL, of_smi_comm_desc[i]);
		if (node == NULL) {
			err = -4;
			break;
		}
		SMICommBaseAddr[i] = of_iomap(node, 0);
		of_node_put(node);
		if (SMICommBaseAddr[i] == 0) {
			err = -5;
			break;
		}
		printk("MET SMI: of_iomap <%s> to %p\n",
			of_smi_comm_desc[i], SMICommBaseAddr[i]);
	}
	if (err) {
		while (i) {
			i--;
			iounmap(SMICommBaseAddr[i]);
		}
		return err;
	}

	return 0;
}

void MET_SMI_Fini(void)
{
	int	i;

	/* un-map address for SMI_LARBs */
	for (i = SMI_LARB_NUMBER; i;) {
		i--;
		iounmap(SMILarbBaseAddr[i]);
	}

	/* un-map address for SMI_COMMs */
	for (i = SMI_COMM_NUMBER; i;) {
		i--;
		iounmap(SMICommBaseAddr[i]);
	}
}

//void SMI_SetSMIBMCfg(unsigned long larbno, unsigned long portno,
//                                         unsigned long desttype, unsigned long rwtype)
//{
//	mt_reg_sync_writel(
//			portno,
//			SMI_LARB_MON_PORT(SMILarbBaseAddr[larbno]));
//	mt_reg_sync_writel(
//			((rwtype << 2) | desttype),
//			SMI_LARB_MON_TYPE(SMILarbBaseAddr[larbno]));
//}

int MET_SMI_LARB_SetCfg(int larbno,
			unsigned int pm,
			unsigned int reqtype,
			unsigned int rwtype,
			unsigned int dsttype)
{
	unsigned int u4RegVal;

	/* reqtype, rwtype, dsttype would be don't care
	 * in parallel mode
	 */
	if ((pm & ~0x1) != 0)
		return -1;
	if ((reqtype & ~0x3) != 0)
		return -1;
	if ((rwtype & ~0x3) != 0)
		return -1;
	if ((dsttype & ~0x3) != 0)
		return -1;

	u4RegVal = (pm << 10) |
			(reqtype << 4) |
			(rwtype << 2) |
			(dsttype << 0);

	mt_reg_sync_writel(u4RegVal,
		SMI_LARB_MON_CON(SMILarbBaseAddr[larbno]));

	return 0;
}

int MET_SMI_LARB_SetPortNo(int larbno, unsigned int idx, unsigned int port)
{
	unsigned int u4RegVal;

	if ((idx & ~0x3) != 0)
		return -1;
	if ((port & ~0x3f) != 0)
		return -1;

	u4RegVal = readl(IOMEM(SMI_LARB_MON_PORT(SMILarbBaseAddr[larbno])));

	// Reset
	// Idx = 0: bit0~bit5
	// Idx = 1: bit8~bit13
	// Idx = 2: bit16~bit21
	// Idx = 3: bit24~bit29
	u4RegVal &= ~(0x3f << (idx*8));
	// Set
	u4RegVal |= (port << (idx*8));

	mt_reg_sync_writel(
		u4RegVal,
		SMI_LARB_MON_PORT(SMILarbBaseAddr[larbno]));

	return 0;
}

//void SMI_SetMonitorControl (SMIBMCfg_Ext *cfg_ex) {
//
//	int i;
//	unsigned long u4RegVal;
//
//	if(cfg_ex != NULL) {
//		u4RegVal = (((unsigned long)cfg_ex->uStarvationTime << 16) | ((unsigned long)cfg_ex->bStarvationEn << 15) |
//		((unsigned long)cfg_ex->bRequestSelection << 6) |
//		((unsigned long)cfg_ex->bMaxPhaseSelection << 5) | ((unsigned long)cfg_ex->bDPSelection << 4) |
//		((unsigned long)cfg_ex->uIdleOutStandingThresh << 1) | cfg_ex->bIdleSelection);
////        printk("Ex configuration %lx\n", u4RegVal);
//	} else {// default
//		u4RegVal = (((unsigned long)8 << 16) | ((unsigned long)1 << 15) |
//		((unsigned long)1 << 5) | ((unsigned long)1 << 4) |
//		((unsigned long)3 << 1) | 1);
////        printk("default configuration %lx\n", u4RegVal);
//	}
//
//	for (i=0; i<SMI_LARB_NUMBER; i++) {
//		mt_reg_sync_writel(u4RegVal , SMI_LARB_MON_CON(SMILarbBaseAddr[i]));
//	}
//
//}

void MET_SMI_Disable(int larbno) {

	unsigned int u4RegVal;

	u4RegVal = readl(IOMEM(SMI_LARB_MON_ENA(SMILarbBaseAddr[larbno])));

	// Stop counting
	mt_reg_sync_writel(
		u4RegVal & ~0x1,
		SMI_LARB_MON_ENA(SMILarbBaseAddr[larbno]));
}

void MET_SMI_Enable(int larbno) {

	unsigned int u4RegVal;

	u4RegVal = readl(IOMEM(SMI_LARB_MON_ENA(SMILarbBaseAddr[larbno])));

	// Start counting
	mt_reg_sync_writel(
		u4RegVal | 0x1,
		SMI_LARB_MON_ENA(SMILarbBaseAddr[larbno]));
}

void MET_SMI_Comm_Disable(int commonno) {

	unsigned int u4RegVal;

	u4RegVal = readl(IOMEM(SMI_COMM_MON_ENA(SMICommBaseAddr[commonno])));

	// Stop counting
	mt_reg_sync_writel(
		u4RegVal & ~0x1,
		SMI_COMM_MON_ENA(SMICommBaseAddr[commonno]));
}

void MET_SMI_Comm_Enable(int commonno) {

	unsigned int u4RegVal;

	u4RegVal = readl(IOMEM(SMI_COMM_MON_ENA(SMICommBaseAddr[commonno])));

	// Start counting
	mt_reg_sync_writel(
		u4RegVal | 0x1,
		SMI_COMM_MON_ENA(SMICommBaseAddr[commonno]));
}

void MET_SMI_Comm_Clear(int commonno)
{
//Clear Counter
	unsigned int u4RegVal;

	u4RegVal = readl(IOMEM(SMI_COMM_MON_CLR(SMICommBaseAddr[commonno])));

	// Enable
	mt_reg_sync_writel(
		u4RegVal | 0x1,
		SMI_COMM_MON_CLR(SMICommBaseAddr[commonno]));
	// Disable
	mt_reg_sync_writel(
		u4RegVal & ~0x1,
		SMI_COMM_MON_CLR(SMICommBaseAddr[commonno]));
}

//void SMI_SetCommBMCfg(unsigned long commonno, unsigned long portno,
//                      unsigned long desttype, unsigned long rwtype)
//{
//	unsigned long u4RegVal;
//
//	mt_reg_sync_writel( ((rwtype<< 2) | desttype) , SMI_COMM_MON_TYPE(SMICommBaseAddr[commonno]));
//
////	u4RegVal = (((unsigned long)commonno << 18) |
//	u4RegVal = (((unsigned long)portno << 18) |
//          ((unsigned long)1 << 4) | ((unsigned long)3 << 1) | 1);
//	mt_reg_sync_writel(u4RegVal , SMI_COMM_MON_CON(SMICommBaseAddr[commonno]));
//
//}

int MET_SMI_COMM_SetCfg(int commonno, unsigned int pm, unsigned int reqtype)
{
	unsigned int u4RegVal;

	if ((pm & ~0x1) != 0)
		return -1;
	if ((reqtype & ~0x3) != 0)
		return -1;

	u4RegVal = readl(IOMEM(SMI_COMM_MON_ENA(SMICommBaseAddr[commonno])));

	// Parallel mode
	mt_reg_sync_writel(
		u4RegVal | (pm << 1),
		SMI_COMM_MON_ENA(SMICommBaseAddr[commonno]));

	// Reset
	u4RegVal &= ~(0x3 << 4);
	// Set
	u4RegVal |= (reqtype << 4);

	mt_reg_sync_writel(u4RegVal,
		SMI_COMM_MON_TYPE(SMICommBaseAddr[commonno]));

	return 0;
}

int MET_SMI_COMM_SetPortNo(int commonno, unsigned int idx, unsigned int port)
{
	unsigned int u4RegVal;

	if ((idx & ~0x3) != 0)
		return -1;
	if ((port & ~0xf) != 0)
		return -1;

	u4RegVal = readl(IOMEM(SMI_COMM_MON_CON(SMICommBaseAddr[commonno])));
	// Reset
	// idx = 0: bit4~bit7
	// idx = 1: bit8~bit11
	// idx = 2: bit12~bit15
	// idx = 3: bit16~bit19
	u4RegVal &= ~(0xf << ((idx+1) * 4));

	// Set
	u4RegVal |= (port) << ((idx+1) * 4);

	mt_reg_sync_writel(
		u4RegVal,
		SMI_COMM_MON_CON(SMICommBaseAddr[commonno]));

	return 0;
}

int MET_SMI_COMM_SetRWType(int commonno, unsigned int idx, unsigned int rw)
{
	unsigned int u4RegVal;

	if ((idx & ~0x3) != 0)
		return -1;
	if ((rw & ~0x3) != 0)
		return -1;

	u4RegVal = readl(IOMEM(SMI_COMM_MON_TYPE(SMICommBaseAddr[commonno])));
	// Reset
	// idx = 0: bit0
	// idx = 1: bit1
	// idx = 2: bit2
	// idx = 3: bit3
	u4RegVal &= ~(0x1 << idx);

	// Set
	// TODO: need to re-defined user interface
	// R: 0x1 (get from user)
	// W: 0x2 (get from user)
	if (rw == 0x2)
		u4RegVal |= 0x1 << idx;

	mt_reg_sync_writel(
		u4RegVal,
		SMI_COMM_MON_TYPE(SMICommBaseAddr[commonno]));

	return 0;
}

void MET_SMI_Pause(int larbno)
{
//Pause Counter
	unsigned int u4RegVal;

	u4RegVal = readl(IOMEM(SMI_LARB_MON_ENA(SMILarbBaseAddr[larbno])));

	// Stop counting
	mt_reg_sync_writel(
		u4RegVal & ~0x1,
		SMI_LARB_MON_ENA(SMILarbBaseAddr[larbno]));
}

void MET_SMI_Clear(int larbno)
{
//Clear Counter
	unsigned int u4RegVal;

	u4RegVal = readl(IOMEM(SMI_LARB_MON_CLR(SMILarbBaseAddr[larbno])));

	// Enable
	mt_reg_sync_writel(
		u4RegVal | 0x1,
		SMI_LARB_MON_CLR(SMILarbBaseAddr[larbno]));
	// Disable
	mt_reg_sync_writel(
		u4RegVal & ~0x1,
		SMI_LARB_MON_CLR(SMILarbBaseAddr[larbno]));
}

//Get SMI result
/* result->u4ActiveCnt = ioread32(MT6575SMI_MON_ACT_CNT(u4SMIBaseAddr));
    result->u4RequestCnt = ioread32(MT6575SMI_MON_REQ_CNT(u4SMIBaseAddr));
    result->u4IdleCnt = ioread32(MT6575SMI_MON_IDL_CNT(u4SMIBaseAddr));
    result->u4BeatCnt = ioread32(MT6575SMI_MON_BEA_CNT(u4SMIBaseAddr));
    result->u4ByteCnt = ioread32(MT6575SMI_MON_BYT_CNT(u4SMIBaseAddr));
    result->u4CommPhaseAccum = ioread32(MT6575SMI_MON_CP_CNT(u4SMIBaseAddr));
    result->u4DataPhaseAccum = ioread32(MT6575SMI_MON_DP_CNT(u4SMIBaseAddr));
    result->u4MaxCommOrDataPhase = ioread32(MT6575SMI_MON_CDP_MAX(u4SMIBaseAddr));
    result->u4MaxOutTransaction = ioread32(MT6575SMI_MON_COS_MAX(u4SMIBaseAddr));
*/

// get larb config
int MET_SMI_GetEna(int larbno) {
	return ioread32(SMI_LARB_MON_ENA(SMILarbBaseAddr[larbno]));
}
int MET_SMI_GetClr(int larbno) {
	return ioread32(SMI_LARB_MON_CLR(SMILarbBaseAddr[larbno]));
}
int MET_SMI_GetPortNo(int larbno) {
	return ioread32(SMI_LARB_MON_PORT(SMILarbBaseAddr[larbno]));
}
int MET_SMI_GetCon(int larbno) {
	return ioread32(SMI_LARB_MON_CON(SMILarbBaseAddr[larbno]));
}

/* === get counter === */
int MET_SMI_GetActiveCnt(int larbno) {
	return ioread32(SMI_LARB_MON_ACT_CNT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetRequestCnt(int larbno) {
	return ioread32(SMI_LARB_MON_REQ_CNT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetBeatCnt(int larbno) {
	return ioread32(SMI_LARB_MON_BEA_CNT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetByteCnt(int larbno) {
	return ioread32(SMI_LARB_MON_BYT_CNT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetCPCnt(int larbno) {
	return ioread32(SMI_LARB_MON_CP_CNT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetDPCnt(int larbno) {
	return ioread32(SMI_LARB_MON_DP_CNT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetOSTDCnt(int larbno) {
	return ioread32(SMI_LARB_MON_OSTD_CNT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetCP_MAX(int larbno) {
	return ioread32(SMI_LARB_MON_CP_MAX(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetOSTD_MAX(int larbno) {
	return ioread32(SMI_LARB_MON_OSTD_MAX(SMILarbBaseAddr[larbno]));
}

// get common config
int MET_SMI_Comm_GetEna(int commonno) {
	return ioread32(SMI_COMM_MON_ENA(SMICommBaseAddr[commonno]));
}
int MET_SMI_Comm_GetClr(int commonno) {
	return ioread32(SMI_COMM_MON_CLR(SMICommBaseAddr[commonno]));
}
int MET_SMI_Comm_GetType(int commonno) {
	return ioread32(SMI_COMM_MON_TYPE(SMICommBaseAddr[commonno]));
}
int MET_SMI_Comm_GetCon(int commonno) {
	return ioread32(SMI_COMM_MON_CON(SMICommBaseAddr[commonno]));
}

//get common counter
int MET_SMI_Comm_GetActiveCnt(int commonno) {
	return ioread32(SMI_COMM_MON_ACT_CNT(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetRequestCnt(int commonno) {
	return ioread32(SMI_COMM_MON_REQ_CNT(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetBeatCnt(int commonno) {
	return ioread32(SMI_COMM_MON_BEA_CNT(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetByteCnt(int commonno) {
	return ioread32(SMI_COMM_MON_BYT_CNT(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetCPCnt(int commonno) {
	return ioread32(SMI_COMM_MON_CP_CNT(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetDPCnt(int commonno) {
	return ioread32(SMI_COMM_MON_DP_CNT(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetOSTDCnt(int commonno) {
	return ioread32(SMI_COMM_MON_OSTD_CNT(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetCP_MAX(int commonno) {
	return ioread32(SMI_COMM_MON_CP_MAX(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetOSTD_MAX(int commonno) {
	return ioread32(SMI_COMM_MON_OSTD_MAX(SMICommBaseAddr[commonno]));
}

static int larb_clock_on(int larb_id) 
{

#ifndef CONFIG_MTK_FPGA
    char name[30];
    sprintf(name, "smi+%d", larb_id);  

    switch(larb_id)
    {
    case 0: 
        enable_clock(MT_CG_DISP0_SMI_COMMON, name);
        enable_clock(MT_CG_DISP0_SMI_LARB0, name);
        break;
    case 1:
        enable_clock(MT_CG_DISP0_SMI_COMMON, name);
        enable_clock(MT_CG_VDEC1_LARB, name);
        break;
    case 2: 
        enable_clock(MT_CG_DISP0_SMI_COMMON, name);
        enable_clock(MT_CG_IMAGE_LARB2_SMI, name);
        break;
    case 3: 
        enable_clock(MT_CG_DISP0_SMI_COMMON, name);
        enable_clock(MT_CG_VENC_LARB, name);
        break;
    case 4: 
        enable_clock(MT_CG_DISP0_SMI_COMMON, name);
        enable_clock(MT_CG_MJC_SMI_LARB, name);
        break;
    default: 
        break;
    }
#endif /* CONFIG_MTK_FPGA */

  return 0;
}

static int larb_clock_off(int larb_id) 
{

#ifndef CONFIG_MTK_FPGA
    char name[30];
    sprintf(name, "smi+%d", larb_id);


    switch(larb_id)
    {
    case 0: 
        disable_clock(MT_CG_DISP0_SMI_LARB0, name);
        disable_clock(MT_CG_DISP0_SMI_COMMON, name);
        break;
    case 1:
        disable_clock(MT_CG_VDEC1_LARB, name);
        disable_clock(MT_CG_DISP0_SMI_COMMON, name);
        break;
    case 2: 
        disable_clock(MT_CG_IMAGE_LARB2_SMI, name);
        disable_clock(MT_CG_DISP0_SMI_COMMON, name);
        break;
    case 3: 
        disable_clock(MT_CG_VENC_LARB, name);
        disable_clock(MT_CG_DISP0_SMI_COMMON, name);
        break;
    case 4: 
        disable_clock(MT_CG_MJC_SMI_LARB, name);
        disable_clock(MT_CG_DISP0_SMI_COMMON, name);
        break;
    default: 
        break;
    }
#endif /* CONFIG_MTK_FPGA */

    return 0;
}

void MET_SMI_PowerOn(void)
{
	int i;

	for (i=0; i<SMI_LARB_NUMBER; i++) {
		larb_clock_on(i);
	}
}

void MET_SMI_PowerOff(void)
{
	int i;

	for (i=0; i<SMI_LARB_NUMBER; i++) {
		larb_clock_off(i);
	}
}

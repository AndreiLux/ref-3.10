#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/list.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#include <linux/aee.h>
#endif

#include <mach/mt_reg_base.h>
#include <mach/emi_mpu.h>
#include <mach/mt_device_apc.h>
#include <mach/sync_write.h>
#include <mach/irqs.h>
#include <mach/dma.h>
#include <mach/mt_emi.h>

#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
#include "trustzone/kree/system.h"
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_emi.h"
#endif

#define NR_REGION_ABORT 8
#define MAX_EMI_MPU_STORE_CMD_LEN 128
#define ABORT_EMI 0x08000008
#define TIMEOUT 100

struct mst_tbl_entry {
	u32 master;
	u32 port;
	u32 id_mask;
	u32 id_val;
	char *name;
};

struct emi_mpu_notifier_block {
	struct list_head list;
	emi_mpu_notifier notifier;
	void *data;
};

static const struct mst_tbl_entry mst_tbl[] =
{
	/* apmcu0 */
	{ .master = MST_ID_APMCU_0, .port = 0x0, .id_mask = 0b0000000111, .id_val = 0b0000000001, .name = "APMCU: GPU" },
	{ .master = MST_ID_APMCU_1, .port = 0x0, .id_mask = 0b1000000111, .id_val = 0b0000000010, .name = "APMCU: smi coherance" },
	{ .master = MST_ID_APMCU_2, .port = 0x0, .id_mask = 0b1000000111, .id_val = 0b0000000011, .name = "APMCU: CA15" },
	{ .master = MST_ID_APMCU_3, .port = 0x0, .id_mask = 0b1000000111, .id_val = 0b0000000101, .name = "APMCU: barrier or write from a WriteUnique or a WriteLineUnique transaction" },
	{ .master = MST_ID_APMCU_4, .port = 0x0, .id_mask = 0b1000000111, .id_val = 0b0000000111, .name = "APMCU: CCI-400 write back" },
	{ .master = MST_ID_APMCU_5, .port = 0x0, .id_mask = 0b1111100111, .id_val = 0b0000000100, .name = "APMCU: CA7 Processor Non-Cacheable or STREX or LDREX" },
	{ .master = MST_ID_APMCU_6, .port = 0x0, .id_mask = 0b1111100111, .id_val = 0b0000100100, .name = "APMCU: CA7 Processor write to device and Strongly_ordered memory or CA7 Processor read TLB" },
	{ .master = MST_ID_APMCU_7, .port = 0x0, .id_mask = 0b1111100111, .id_val = 0b0001000100, .name = "APMCU: CA7 processor write/read portion of the barrier transactions" },
	{ .master = MST_ID_APMCU_8, .port = 0x0, .id_mask = 0b1111111111, .id_val = 0b0001111100, .name = "APMCU: CA7 Write/read portion of barrier transaction caused by external DVM synchronization" },
	{ .master = MST_ID_APMCU_9, .port = 0x0, .id_mask = 0b1111100111, .id_val = 0b0010000100, .name = "APMCU: CA7 Write to cachenable memory from write address buffer or Read Line-Fill Buffer 0" },

	{ .master = MST_ID_APMCU_15, .port = 0x0, .id_mask = 0b1111100111, .id_val = 0b0010100100, .name = "APMCU: CA7 Processor read Line-Fill Buffer 1" },
	{ .master = MST_ID_APMCU_16, .port = 0x0, .id_mask = 0b1111100111, .id_val = 0b0011000100, .name = "APMCU: CA7 Processor read instruction" },
	{ .master = MST_ID_APMCU_17, .port = 0x0, .id_mask = 0b1111100111, .id_val = 0b0100000100, .name = "APMCU: CA7 Processor read STB0" },
	{ .master = MST_ID_APMCU_18, .port = 0x0, .id_mask = 0b1111100111, .id_val = 0b0100100100, .name = "APMCU: CA7 Processor read STB1" },
	{ .master = MST_ID_APMCU_19, .port = 0x0, .id_mask = 0b1111100111, .id_val = 0b0101000100, .name = "APMCU: CA7 Processor read STB2" },
	{ .master = MST_ID_APMCU_20, .port = 0x0, .id_mask = 0b1111100111, .id_val = 0b0101100100, .name = "APMCU: CA7 Processor read STB3" },
	{ .master = MST_ID_APMCU_21, .port = 0x0, .id_mask = 0b1111100111, .id_val = 0b0110000100, .name = "APMCU: CA7 Processor read DVM request" },
	{ .master = MST_ID_APMCU_22, .port = 0x0, .id_mask = 0b1111100111, .id_val = 0b0110100100, .name = "APMCU: CA7 DVM complete message" },
	{ .master = MST_ID_APMCU_23, .port = 0x0, .id_mask = 0b1111000111, .id_val = 0b0111000100, .name = "APMCU: CA7 L2 linefill from Line-Fill buffer 0111mmm100" },

	/* apmcu1 */
	{ .master = MST_ID_APMCU_0, .port = 0x1, .id_mask = 0b0000000111, .id_val = 0b0000000001, .name = "APMCU1: GPU" },
	{ .master = MST_ID_APMCU_1, .port = 0x1, .id_mask = 0b1000000111, .id_val = 0b0000000010, .name = "APMCU1: smi coherance" },
	{ .master = MST_ID_APMCU_2, .port = 0x1, .id_mask = 0b1000000111, .id_val = 0b0000000011, .name = "APMCU1: CA15" },
	{ .master = MST_ID_APMCU_3, .port = 0x1, .id_mask = 0b1000000111, .id_val = 0b0000000101, .name = "APMCU1: barrier or write from a WriteUnique or a WriteLineUnique transaction" },
	{ .master = MST_ID_APMCU_4, .port = 0x1, .id_mask = 0b1000000111, .id_val = 0b0000000111, .name = "APMCU1: CCI-400 write back" },
	{ .master = MST_ID_APMCU_5, .port = 0x1, .id_mask = 0b1111100111, .id_val = 0b0000000100, .name = "APMCU: CA7 Processor Non-Cacheable or STREX or LDREX" },
	{ .master = MST_ID_APMCU_6, .port = 0x1, .id_mask = 0b1111100111, .id_val = 0b0000100100, .name = "APMCU: CA7 Processor write to device and Strongly_ordered memory or CA7 Processor read TLB" },
	{ .master = MST_ID_APMCU_7, .port = 0x1, .id_mask = 0b1111100111, .id_val = 0b0001000100, .name = "APMCU: CA7 processor write/read portion of the barrier transactions" },
	{ .master = MST_ID_APMCU_8, .port = 0x1, .id_mask = 0b1111111111, .id_val = 0b0001111100, .name = "APMCU: CA7 Write/read portion of barrier transaction caused by external DVM synchronization" },
	{ .master = MST_ID_APMCU_9, .port = 0x1, .id_mask = 0b1111100111, .id_val = 0b0010000100, .name = "APMCU: CA7 Write to cachenable memory from write address buffer or Read Line-Fill Buffer 0" },

	{ .master = MST_ID_APMCU_15, .port = 0x1, .id_mask = 0b1111100111, .id_val = 0b0010100100, .name = "APMCU: CA7 Processor read Line-Fill Buffer 1" },
	{ .master = MST_ID_APMCU_16, .port = 0x1, .id_mask = 0b1111100111, .id_val = 0b0011000100, .name = "APMCU: CA7 Processor read instruction" },
	{ .master = MST_ID_APMCU_17, .port = 0x1, .id_mask = 0b1111100111, .id_val = 0b0100000100, .name = "APMCU: CA7 Processor read STB0" },
	{ .master = MST_ID_APMCU_18, .port = 0x1, .id_mask = 0b1111100111, .id_val = 0b0100100100, .name = "APMCU: CA7 Processor read STB1" },
	{ .master = MST_ID_APMCU_19, .port = 0x1, .id_mask = 0b1111100111, .id_val = 0b0101000100, .name = "APMCU: CA7 Processor read STB2" },
	{ .master = MST_ID_APMCU_20, .port = 0x1, .id_mask = 0b1111100111, .id_val = 0b0101100100, .name = "APMCU: CA7 Processor read STB3" },
	{ .master = MST_ID_APMCU_21, .port = 0x1, .id_mask = 0b1111100111, .id_val = 0b0110000100, .name = "APMCU: CA7 Processor read DVM request" },
	{ .master = MST_ID_APMCU_22, .port = 0x1, .id_mask = 0b1111100111, .id_val = 0b0110100100, .name = "APMCU: CA7 DVM complete message" },
	{ .master = MST_ID_APMCU_23, .port = 0x1, .id_mask = 0b1111000111, .id_val = 0b0111000100, .name = "APMCU: CA7 L2 linefill from Line-Fill buffer 0111mmm100" },

	/* MFG */
	{ .master = MST_ID_MFG_0, .port = 0x4, .id_mask = 0b1110000000, .id_val = 0b0000000000, .name = "MFG: GPU" },

	/* MM0 + Periperal (MCI port) */
	{ .master = MST_ID_MMPERI_0, .port = 0x5, .id_mask = 0b1111111111, .id_val = 0b0000000100, .name = "Debug System" },
	{ .master = MST_ID_MMPERI_1, .port = 0x5, .id_mask = 0b1111111111, .id_val = 0b0000000010, .name = "USB20_0, NFI, MSDC0, MSDC4" },
	{ .master = MST_ID_MMPERI_2, .port = 0x5, .id_mask = 0b1111111111, .id_val = 0b0000000110, .name = "SPI, SPM, USB20_1, MSDC2" },
	{ .master = MST_ID_MMPERI_3, .port = 0x5, .id_mask = 0b1111111111, .id_val = 0b0000001010, .name = "MSDC1, PWM, MSDC3, THERM" },
	{ .master = MST_ID_MMPERI_4, .port = 0x5, .id_mask = 0b1111111111, .id_val = 0b0000001110, .name = "APDMA" },
	{ .master = MST_ID_MMPERI_5, .port = 0x5, .id_mask = 0b1111111111, .id_val = 0b0000010010, .name = "FHCTRL" },
	{ .master = MST_ID_MMPERI_6, .port = 0x5, .id_mask = 0b1111111111, .id_val = 0b0000010110, .name = "CA15_THERM" },
	{ .master = MST_ID_MMPERI_7, .port = 0x5, .id_mask = 0b1111111111, .id_val = 0b0000000001, .name = "GCPU" },
	{ .master = MST_ID_MMPERI_8, .port = 0x5, .id_mask = 0b1111111111, .id_val = 0b0000100001, .name = "Audio" },
	{ .master = MST_ID_MMPERI_9, .port = 0x5, .id_mask = 0b1111100001, .id_val = 0b0001000001, .name = "Larb4 MM Master, 00010nnnn1 = 0000~1001" },
	{ .master = MST_ID_MMPERI_10, .port = 0x5, .id_mask = 0b1111100001, .id_val = 0b0001100001, .name = "Larb3 MM Master, 00011nnnn1 = 0000~1101" },
	{ .master = MST_ID_MMPERI_11, .port = 0x5, .id_mask = 0b1111100001, .id_val = 0b0010000001, .name = "Larb2 MM Master, 00100nnnn1 = 0000~1011" },
	{ .master = MST_ID_MMPERI_12, .port = 0x5, .id_mask = 0b1111100001, .id_val = 0b0010100001, .name = "Larb1 MM Master, 00101nnnn1 = 0000~0110" },
	{ .master = MST_ID_MMPERI_13, .port = 0x5, .id_mask = 0b1111100001, .id_val = 0b0011000001, .name = "Larb0 MM Master, 00110nnnn1 = 0000~1001" },
	{ .master = MST_ID_MMPERI_14, .port = 0x5, .id_mask = 0b1111111111, .id_val = 0b0011100001, .name = "M4U L2 Prefetch" },
	{ .master = MST_ID_MMPERI_15, .port = 0x5, .id_mask = 0b1111100001, .id_val = 0b0011100001, .name = "M4U nb_mmu0, 00111nnnn1=0001,0101" },
	{ .master = MST_ID_MMPERI_16, .port = 0x5, .id_mask = 0b1111100001, .id_val = 0b0011100001, .name = "M4U nb_mmu1, 00111nnnn1=0010,0110" },


	/* MM1 */
	{ .master = MST_ID_MM1_0, .port = 0x6, .id_mask = 0b1111111111, .id_val = 0b0000000000, .name = "GCPU" },
	{ .master = MST_ID_MM1_1, .port = 0x6, .id_mask = 0b1111111111, .id_val = 0b0000010000, .name = "Audio" },
	{ .master = MST_ID_MM1_2, .port = 0x6, .id_mask = 0b1111110000, .id_val = 0b0000100000, .name = "Larb4 MM Master, 000010nnnn = 0000~1001" },
	{ .master = MST_ID_MM1_3, .port = 0x6, .id_mask = 0b1111110000, .id_val = 0b0000110000, .name = "Larb3 MM Master, 000011nnnn = 0000~1101" },
	{ .master = MST_ID_MM1_4, .port = 0x6, .id_mask = 0b1111110000, .id_val = 0b0001000000, .name = "Larb2 MM Master, 000100nnnn = 0000~1011" },
	{ .master = MST_ID_MM1_5, .port = 0x6, .id_mask = 0b1111110000, .id_val = 0b0001010000, .name = "Larb1 MM Master, 000101nnnn = 0000~0110" },
	{ .master = MST_ID_MM1_6, .port = 0x6, .id_mask = 0b1111110000, .id_val = 0b0001100000, .name = "Larb0 MM Master, 000110nnnn = 0000~1001" },
	{ .master = MST_ID_MM1_7, .port = 0x6, .id_mask = 0b1111111111, .id_val = 0b0001110000, .name = "M4U L2 Prefetch" },
	{ .master = MST_ID_MM1_8, .port = 0x6, .id_mask = 0b1111110000, .id_val = 0b0001110000, .name = "M4U nb_mmu0, 000111nnnn=0001,0101" },
	{ .master = MST_ID_MM1_9, .port = 0x6, .id_mask = 0b1111110000, .id_val = 0b0001110000, .name = "M4U nb_mmu1, 000111nnnn=0010,0110" },

};

struct list_head emi_mpu_notifier_list[NR_MST];
static const char *UNKNOWN_MASTER = "unknown";
static spinlock_t emi_mpu_lock;

char *smi_larb0_port[10] =
	{ "venc_rcpu", "venc_ref_luma", "venc_ref_chroma", "venc_db_read", "venc_db_write",
	  "venc_cur_luma", "venc_cur_chroma", "venc_rd_comv", "venc_sv_comv", "venc_bsdma" };
char *smi_larb1_port[7] =
	{ "hw_vdec_mc_ext", "hw_vdec_pp_ext", "hw_vdec_avc_mv_ext", "hw_vdec_pred_rd_ext",
	  "hw_vdec_pred_wr_ext", "hw_vdec_vld_ext", "hw_vdec_vld2_ext" };
char *smi_larb2_port[12] =
	{ "rot_ext", "ovl_ch0", "ovl_ch1", "ovl_ch2", "ovl_ch3", "wdma0", "wdma1", "rdma0", "rdma1",
	  "cmdq", "dbi", "g2d" };
char *smi_larb3_port[15] =
	{ "jpgdec_wdma", "jpgenc_rdma", "vipi", "vip2i", "dispo", "dispco", "dispvo", "vido", "vidco",
	  "vidvo", "gdma_smi_rd", "gdma_smi_wr", "jpgdec_bsdma", "jpgenc_bsdma", "vido_rot0" };
char *smi_larb4_port[12] =
	{ "imgi", "imgci", "imgo", "img2o", "lsci", "flki", "lcei", "lcso", "esfko", "aao",
	  "vidco_rot0", "fpc" };

static int __match_id(u32 axi_id, int tbl_idx, u32 port_ID)
{
	u32 mm_larb;
	u32 smi_port;

	if (((axi_id & mst_tbl[tbl_idx].id_mask) == mst_tbl[tbl_idx].id_val)
	    && (port_ID == mst_tbl[tbl_idx].port)) {
		switch (port_ID) {
		case 0:	/* ARM */
		case 1:	/* ARM */
		case 4:	/* MD HW (2G/3G) */
			pr_crit("Violation master name is %s.\n", mst_tbl[tbl_idx].name);
			break;
		case 5:	/* MM0 + periperal */
			mm_larb = axi_id >> 5;
			smi_port = (axi_id >> 1) & 0xF;
			if (mm_larb == 0x2) {
				if (smi_port >= ARRAY_SIZE(smi_larb4_port)) {
					pr_crit
					    ("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit("Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb4_port[smi_port]);
			} else if (mm_larb == 0x3) {
				if (smi_port >= ARRAY_SIZE(smi_larb3_port)) {
					pr_crit
					    ("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit("Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb3_port[smi_port]);
			} else if (mm_larb == 0x4) {
				if (smi_port >= ARRAY_SIZE(smi_larb2_port)) {
					pr_crit
					    ("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit("Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb2_port[smi_port]);
			} else if (mm_larb == 0x5) {
				if (smi_port >= ARRAY_SIZE(smi_larb1_port)) {
					pr_crit
					    ("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit("Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb1_port[smi_port]);
			} else if (mm_larb == 0x6) {
				if (smi_port >= ARRAY_SIZE(smi_larb0_port)) {
					pr_crit
					    ("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit("Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb0_port[smi_port]);
			} else {	/*Peripheral */

				pr_crit("Violation master name is %s.\n", mst_tbl[tbl_idx].name);
			}
			break;
		case 6:	/* MM1 */
			mm_larb = axi_id >> 4;
			smi_port = axi_id & 0xF;
			if (mm_larb == 0x2) {
				if (smi_port >= ARRAY_SIZE(smi_larb4_port)) {
					pr_crit
					    ("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit("Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb4_port[smi_port]);
			} else if (mm_larb == 0x3) {
				if (smi_port >= ARRAY_SIZE(smi_larb3_port)) {
					pr_crit
					    ("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit("Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb3_port[smi_port]);
			} else if (mm_larb == 0x4) {
				if (smi_port >= ARRAY_SIZE(smi_larb2_port)) {
					pr_crit
					    ("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit("Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb2_port[smi_port]);
			} else if (mm_larb == 0x5) {
				if (smi_port >= ARRAY_SIZE(smi_larb1_port)) {
					pr_crit
					    ("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit("Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb1_port[smi_port]);
			} else if (mm_larb == 0x6) {
				if (smi_port >= ARRAY_SIZE(smi_larb0_port)) {
					pr_crit
					    ("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit("Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb0_port[smi_port]);
			} else {
				 /*ERROR*/
				    pr_crit
				    ("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
				return 0;
			}
			break;
		}
		return 1;
	} else {
		return 0;
	}
}

static u32 __id2mst(u32 id)
{
	int i;
	u32 axi_ID;
	u32 port_ID;

	axi_ID = (id >> 3) & 0x000003FF;
	port_ID = id & 0x00000007;

/* pr_info("[EMI MPU] axi_id = %x, port_id = %x\n", axi_ID, port_ID); */

	for (i = 0; i < ARRAY_SIZE(mst_tbl); i++) {
		if (__match_id(axi_ID, i, port_ID)) {
			return mst_tbl[i].master;
		}
	}
	return MST_INVALID;
}

static char *__id2name(u32 id)
{
	int i;
	u32 axi_ID;
	u32 port_ID;

	axi_ID = (id >> 3) & 0x000003FF;
	port_ID = id & 0x00000007;

/* pr_info("[EMI MPU] axi_id = %x, port_id = %x\n", axi_ID, port_ID); */

	for (i = 0; i < ARRAY_SIZE(mst_tbl); i++) {
		if (__match_id(axi_ID, i, port_ID)) {
			return mst_tbl[i].name;
		}
	}

	return (char *)UNKNOWN_MASTER;
}

static void __clear_emi_mpu_vio(void)
{
#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
	KREE_SESSION_HANDLE emi_session;
	MTEEC_PARAM param[4];
	TZ_RESULT ret;
#else
	u32 dbg_s, dbg_t;
	unsigned int reg_value = 0;
	int counter = 0;
#endif

#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
	ret = KREE_CreateSession(TZ_TA_EMI_UUID, &emi_session);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("Error: create emi_session error %d\n", ret);
		return;
	}

	ret =
	    KREE_TeeServiceCall(emi_session, TZCMD_EMI_CLR, TZ_ParamTypes1(TZPT_VALUE_OUTPUT),
				param);

	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("%s error: %s\n", __func__, TZ_GetErrorString(ret));
	}

	ret = KREE_CloseSession(emi_session);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("[EMI] Error: close emi_session error %d\n", ret);
		return;
	}
#else
	/* clear violation status */
	emi_writel(0x000003FF, EMI_MPUP);
	emi_writel(0x000003FF, EMI_MPUQ);
	emi_writel(0x000003FF, EMI_MPUR);
	emi_writel(0x000003FF, EMI_MPUY);

	/* clear debug info */
	emi_reg_sync_writel(0x80000000, EMI_MPUS);
	dbg_s = emi_readl(EMI_MPUS);
	dbg_t = emi_readl(EMI_MPUT);
	if (dbg_s || dbg_t) {
		pr_crit("Fail to clear EMI MPU violation\n");
		pr_crit("EMI_MPUS = %x, EMI_MPUT = %x", dbg_s, dbg_t);
	}

	/* clesr Device APC */
	emi_writel(emi_readl(DEVAPC1_D0_VIO_STA) | ABORT_EMI, DEVAPC1_D0_VIO_STA);
	do {
		reg_value = emi_readl(DEVAPC1_D0_VIO_STA) & ABORT_EMI;
		counter++;
		if (counter >= TIMEOUT) {
			counter = 0;
			pr_crit("Fail to clear DEVAPC1_D0_VIO_STA(EMI) violation\n");
			break;
		}
	} while (reg_value != 0);

	emi_writel(0x2, DEVAPC0_DXS_VIO_STA);
	do {
		reg_value = emi_readl(DEVAPC0_DXS_VIO_STA) & 0x2;
		counter++;
		if (counter >= TIMEOUT) {
			counter = 0;
			pr_crit("Fail to clear DEVAPC0_DXS_VIO_STA(EMI) violation\n");
			break;
		}
	} while (reg_value != 0);
#endif
}

#define DAPC0    0
#define DAPC1    1
#define MPUS      2
#define MPUT      3
#define MPUP      4
#define MPUQ      5
#define MPUR      6
#define MPUY      7
#define MPUMAX   8

/*EMI MPU violation handler*/
static irqreturn_t mpu_violation_irq(int irq, void *dev_id)
{
	u32 mpu_reg[MPUMAX] = { 0 };
	u32 dbg_s, dbg_t, dbg_pqry;
	u32 master_ID, domain_ID, wr_vio;
	s32 region;
	int i;
	char *master_name;
	struct list_head *p;
	struct emi_mpu_notifier_block *block;
#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
	KREE_SESSION_HANDLE emi_session;
	MTEEC_PARAM param[4];
	TZ_RESULT ret;
#endif

#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
	ret = KREE_CreateSession(TZ_TA_EMI_UUID, &emi_session);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("Error: create emi_session error %d\n", ret);
		return -1;
	}

	ret =
	    KREE_TeeServiceCall(emi_session, TZCMD_EMI_REG,
				TZ_ParamTypes4(TZPT_VALUE_OUTPUT, TZPT_VALUE_OUTPUT,
					       TZPT_VALUE_OUTPUT, TZPT_VALUE_OUTPUT), param);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("%s error: %s\n", __func__, TZ_GetErrorString(ret));
	}

	mpu_reg[DAPC0] = (uint32_t) (param[0].value.a);
	mpu_reg[DAPC1] = (uint32_t) (param[0].value.b);
	mpu_reg[MPUS] = (uint32_t) (param[1].value.a);
	mpu_reg[MPUT] = (uint32_t) (param[1].value.b);
	mpu_reg[MPUP] = (uint32_t) (param[2].value.a);
	mpu_reg[MPUQ] = (uint32_t) (param[2].value.b);
	mpu_reg[MPUR] = (uint32_t) (param[3].value.a);
	mpu_reg[MPUY] = (uint32_t) (param[3].value.b);

	ret = KREE_CloseSession(emi_session);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("[EMI] Error: close emi_session error %d\n", ret);
		return -1;
	}
#else
	mpu_reg[DAPC0] = emi_readl(DEVAPC0_DXS_VIO_STA);
	mpu_reg[DAPC1] = emi_readl(DEVAPC1_D0_VIO_STA);
	mpu_reg[MPUS] = emi_readl(EMI_MPUS);
	mpu_reg[MPUT] = emi_readl(EMI_MPUT);
	mpu_reg[MPUP] = emi_readl(EMI_MPUP);
	mpu_reg[MPUQ] = emi_readl(EMI_MPUQ);
	mpu_reg[MPUR] = emi_readl(EMI_MPUR);
	mpu_reg[MPUY] = emi_readl(EMI_MPUY);
#endif

	if ((mpu_reg[DAPC0] & 0x2) != 0x2 || (mpu_reg[DAPC1] & ABORT_EMI) == 0) {
		pr_info("Not EMI MPU violation.\n");
		return IRQ_NONE;
	}

	dbg_s = mpu_reg[MPUS];
	dbg_t = mpu_reg[MPUT];

	master_ID = dbg_s & 0x00001FFF;
	domain_ID = (dbg_s >> 14) & 0x00000003;
	wr_vio = (dbg_s >> 28) & 0x00000003;
	region = (dbg_s >> 16) & 0xFF;

	for (i = 0; i < NR_REGION_ABORT; i++) {
		if ((region >> i) & 1) {
			break;
		}
	}
	region = (i >= NR_REGION_ABORT) ? -1 : i;

	switch (domain_ID) {
	case 0:
		dbg_pqry = mpu_reg[MPUP];
		break;
	case 1:
		dbg_pqry = mpu_reg[MPUQ];
		break;
	case 2:
		dbg_pqry = mpu_reg[MPUR];
		break;
	case 3:
		dbg_pqry = mpu_reg[MPUY];
		break;
	default:
		dbg_pqry = 0;
		break;
	}

	/*print the abort region */
	pr_crit("EMI MPU violation.\n");
	pr_crit("[EMI MPU] Debug info start ----------------------------------------\n");

	pr_crit("EMI_MPUS = %x, EMI_MPUT = %x.\n", dbg_s, dbg_t);
	pr_crit("Current process is \"%s \" (pid: %i).\n", current->comm, current->pid);
	pr_crit("Violation address is 0x%x.\n", dbg_t);
	pr_crit("Violation master ID is 0x%x.\n", master_ID);
	/*print out the murderer name */
	master_name = __id2name(master_ID);
	pr_crit("Violation domain ID is 0x%x.\n", domain_ID);
	pr_crit("%s violation.\n", (wr_vio == 1) ? "Write" : "Read");
	pr_crit("Corrupted region is %d\n\r", region);
	if (dbg_pqry & OOR_VIO) {
		pr_crit("Out of range violation.\n");
	}
	pr_crit("[EMI MPU] Debug info end------------------------------------------\n");

#ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_exception("EMI MPU",
			     "EMI MPU violation.\n Touch Address = 0x%x, module is %s. EMP_MPUS = 0x%x, EMI_MPU(PQRY) = 0x%x\n",
			     dbg_t, master_name, dbg_s, dbg_pqry);
#endif

	__clear_emi_mpu_vio();
	/* pr_info("[EMI MPU] _id2mst = %d\n", __id2mst(master_ID)); */

#if 1
	list_for_each(p, &(emi_mpu_notifier_list[__id2mst(master_ID)])) {
		block = list_entry(p, struct emi_mpu_notifier_block, list);
		block->notifier(block->data, dbg_t + EMI_PHY_OFFSET, wr_vio);
	}
#endif
	return IRQ_HANDLED;
}

/*
 * emi_mpu_set_region_protection: protect a region.
 * @start: start address of the region
 * @end: end address of the region
 * @region: EMI MPU region id
 * @access_permission: EMI MPU access permission
 * Return 0 for success, otherwise negative status code.
 */
int emi_mpu_set_region_protection(unsigned int start, unsigned int end, int region,
				  unsigned int access_permission)
{
	int ret = 0;
	unsigned int tmp;

	if ((end != 0) || (start != 0)) {
		/*Address 64KB alignment */
		start -= EMI_PHY_OFFSET;
		end -= EMI_PHY_OFFSET;
		start = start >> 16;
		end = end >> 16;

		if (end <= start) {
			return -EINVAL;
		}
	}

	spin_lock(&emi_mpu_lock);

	switch (region) {
	case 0:
		emi_writel((start << 16) | end, EMI_MPUA);
		tmp = emi_readl(EMI_MPUI) & 0xFFFF0000;
		emi_writel(tmp | access_permission, EMI_MPUI);
		break;

	case 1:
		emi_writel((start << 16) | end, EMI_MPUB);
		tmp = emi_readl(EMI_MPUI) & 0x0000FFFF;
		emi_writel(tmp | (access_permission << 16), EMI_MPUI);
		break;

	case 2:
		emi_writel((start << 16) | end, EMI_MPUC);
		tmp = emi_readl(EMI_MPUJ) & 0xFFFF0000;
		emi_writel(tmp | access_permission, EMI_MPUJ);
		break;

	case 3:
		emi_writel((start << 16) | end, EMI_MPUD);
		tmp = emi_readl(EMI_MPUJ) & 0x0000FFFF;
		emi_writel(tmp | (access_permission << 16), EMI_MPUJ);
		break;

	case 4:
		emi_writel((start << 16) | end, EMI_MPUE);
		tmp = emi_readl(EMI_MPUK) & 0xFFFF0000;
		emi_writel(tmp | access_permission, EMI_MPUK);
		break;

	case 5:
		emi_writel((start << 16) | end, EMI_MPUF);
		tmp = emi_readl(EMI_MPUK) & 0x0000FFFF;
		emi_writel(tmp | (access_permission << 16), EMI_MPUK);
		break;

	case 6:
		emi_writel((start << 16) | end, EMI_MPUG);
		tmp = emi_readl(EMI_MPUL) & 0xFFFF0000;
		emi_writel(tmp | access_permission, EMI_MPUL);
		break;

	case 7:
		emi_writel((start << 16) | end, EMI_MPUH);
		tmp = emi_readl(EMI_MPUL) & 0x0000FFFF;
		emi_writel(tmp | (access_permission << 16), EMI_MPUL);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock(&emi_mpu_lock);

	return ret;
}

/*
 * emi_mpu_notifier_register: register a notifier.
 * master: MST_ID_xxx
 * notifier: the callback function
 * Return 0 for success, otherwise negative error code.
 */
int emi_mpu_notifier_register(int master, emi_mpu_notifier notifier, void *data)
{
	struct emi_mpu_notifier_block *block;
	static int emi_mpu_notifier_init;
	int i;

	if (master >= MST_INVALID) {
		return -EINVAL;
	}

	block = kmalloc(sizeof(struct emi_mpu_notifier_block), GFP_KERNEL);
	if (!block) {
		return -ENOMEM;
	}

	if (!emi_mpu_notifier_init) {
		for (i = 0; i < NR_MST; i++) {
			INIT_LIST_HEAD(&(emi_mpu_notifier_list[i]));
		}
		emi_mpu_notifier_init = 1;
	}

	block->notifier = notifier;
	block->data = data;
	list_add(&(block->list), &(emi_mpu_notifier_list[master]));

	return 0;
}


static ssize_t emi_mpu_show(struct device_driver *driver, char *buf)
{
	char *ptr = buf;
	unsigned int start, end;
	unsigned int reg_value;
	unsigned int d0, d1, d2, d3;
	static const char *permission[6] = {
		"No protect",
		"Only R/W for secure access",
		"Only R/W for secure access, and non-secure read access",
		"Only R/W for secure access, and non-secure write access",
		"Only R for secure/non-secure",
		"Both R/W are forbidden"
	};

	reg_value = emi_readl(EMI_MPUA);
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 0 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_readl(EMI_MPUB);
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 1 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_readl(EMI_MPUC);
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 2 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_readl(EMI_MPUD);
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 3 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_readl(EMI_MPUE);
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 4 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_readl(EMI_MPUF);
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 5 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_readl(EMI_MPUG);
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 6 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_readl(EMI_MPUH);
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 7 --> 0x%x to 0x%x\n", start, end);

	ptr += sprintf(ptr, "\n");

	reg_value = emi_readl(EMI_MPUI);
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 0 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	d0 = ((reg_value >> 16) & 0x7);
	d1 = ((reg_value >> 16) >> 3) & 0x7;
	d2 = ((reg_value >> 16) >> 6) & 0x7;
	d3 = ((reg_value >> 16) >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 1 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	reg_value = emi_readl(EMI_MPUJ);
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 2 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	d0 = ((reg_value >> 16) & 0x7);
	d1 = ((reg_value >> 16) >> 3) & 0x7;
	d2 = ((reg_value >> 16) >> 6) & 0x7;
	d3 = ((reg_value >> 16) >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 3 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	reg_value = emi_readl(EMI_MPUK);
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 4 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	d0 = ((reg_value >> 16) & 0x7);
	d1 = ((reg_value >> 16) >> 3) & 0x7;
	d2 = ((reg_value >> 16) >> 6) & 0x7;
	d3 = ((reg_value >> 16) >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 5 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	reg_value = emi_readl(EMI_MPUL);
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 6 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	d0 = ((reg_value >> 16) & 0x7);
	d1 = ((reg_value >> 16) >> 3) & 0x7;
	d2 = ((reg_value >> 16) >> 6) & 0x7;
	d3 = ((reg_value >> 16) >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 7 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	return strlen(buf);
}

static ssize_t emi_mpu_store(struct device_driver *driver, const char *buf, size_t count)
{
	int i;
	unsigned int start_addr;
	unsigned int end_addr;
	unsigned int region;
	unsigned int access_permission;
	char *command;
	char *ptr;
	char *token[5];

	if ((strlen(buf) + 1) > MAX_EMI_MPU_STORE_CMD_LEN) {
		pr_crit("emi_mpu_store command overflow.");
		return count;
	}
	pr_crit("emi_mpu_store: %s\n", buf);

	command = kmalloc((size_t) MAX_EMI_MPU_STORE_CMD_LEN, GFP_KERNEL);
	if (!command) {
		return count;
	}
	strcpy(command, buf);
	ptr = (char *)buf;

	if (!strncmp(buf, EN_MPU_STR, strlen(EN_MPU_STR))) {
		i = 0;
		while (ptr != NULL) {
			ptr = strsep(&command, " ");
			token[i] = ptr;
			pr_debug("token[%d] = %s\n", i, token[i]);
			i++;
		}
		for (i = 0; i < 5; i++) {
			pr_debug("token[%d] = %s\n", i, token[i]);
		}

		start_addr = simple_strtoul(token[1], &token[1], 16);
		end_addr = simple_strtoul(token[2], &token[2], 16);
		region = simple_strtoul(token[3], &token[3], 16);
		access_permission = simple_strtoul(token[4], &token[4], 16);
		emi_mpu_set_region_protection(start_addr, end_addr, region, access_permission);
		pr_crit("Set EMI_MPU: start: 0x%x, end: 0x%x, region: %d, permission: 0x%x.\n",
			start_addr, end_addr, region, access_permission);
	} else if (!strncmp(buf, DIS_MPU_STR, strlen(DIS_MPU_STR))) {
		i = 0;
		while (ptr != NULL) {
			ptr = strsep(&command, " ");
			token[i] = ptr;
			pr_debug("token[%d] = %s\n", i, token[i]);
			i++;
		}
		for (i = 0; i < 5; i++) {
			pr_debug("token[%d] = %s\n", i, token[i]);
		}

		start_addr = simple_strtoul(token[1], &token[1], 16);
		end_addr = simple_strtoul(token[2], &token[2], 16);
		region = simple_strtoul(token[3], &token[3], 16);
		emi_mpu_set_region_protection(0x0, 0x0, region,
					      SET_ACCESS_PERMISSON(NO_PROTECTION, NO_PROTECTION,
								   NO_PROTECTION, NO_PROTECTION));
		pr_info("set EMI MPU: start: 0x%x, end: 0x%x, region: %d, permission: 0x%x\n", 0, 0,
			region, SET_ACCESS_PERMISSON(NO_PROTECTION, NO_PROTECTION, NO_PROTECTION,
						     NO_PROTECTION));
	} else {
		pr_crit("Unkown emi_mpu command.\n");
	}

	kfree(command);

	return count;
}

DRIVER_ATTR(mpu_config, 0644, emi_mpu_show, emi_mpu_store);

static struct platform_driver emi_mpu_ctrl = {
	.driver = {
		   .name = "emi_mpu_ctrl",
		   .bus = &platform_bus_type,
		   .owner = THIS_MODULE,
		   }
};


static int __init emi_mpu_mod_init(void)
{
	int ret;

	pr_info("Initialize EMI MPU.\n");

	spin_lock_init(&emi_mpu_lock);

	mpu_violation_irq(0, NULL);

	/*
	 * NoteXXX: Interrupts of vilation (including SPC in SMI, or EMI MPU)
	 *          are triggered by the device APC.
	 *          Need to share the interrupt with the SPC driver.
	 */
	ret =
	    request_irq(MT_APARM_DOMAIN_IRQ_ID, (irq_handler_t) mpu_violation_irq,
			IRQF_TRIGGER_LOW | IRQF_SHARED, "mt_emi_mpu", &emi_mpu_ctrl.driver);
	if (ret != 0) {
		pr_crit("Fail to request EMI_MPU interrupt. Error = %d.\n", ret);
		return ret;
	}
#if !defined(USER_BUILD_KERNEL)
	/* register driver and create sysfs files */
	ret = driver_register(&emi_mpu_ctrl.driver);
	if (ret) {
		pr_crit("Fail to register EMI_MPU driver.\n");
	}
	ret = driver_create_file(&emi_mpu_ctrl.driver, &driver_attr_mpu_config);
	if (ret) {
		pr_crit("Fail to create EMI_MPU sysfs file.\n");
	}
#endif

	/*Init for testing */
        /*MD_IMG_REGION_LEN*/
	/*emi_mpu_set_region_protection(0x80000000,    0x815FFFFF,
	0,
	SET_ACCESS_PERMISSON(SEC_R_NSEC_R, SEC_R_NSEC_R, SEC_R_NSEC_R, SEC_R_NSEC_R));*/

	return 0;
}

static void __exit emi_mpu_mod_exit(void)
{
}
module_init(emi_mpu_mod_init);
module_exit(emi_mpu_mod_exit);

EXPORT_SYMBOL(emi_mpu_set_region_protection);
/* EXPORT_SYMBOL(start_mm_mau_protect); */

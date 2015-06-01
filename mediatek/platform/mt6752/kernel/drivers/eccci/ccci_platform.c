#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <mach/emi_mpu.h>
#include <mach/sync_write.h>
#include <mach/memory.h>
#include <mach/upmu_sw.h>
#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <mach/ccci_config.h>
#include <mach/mt_ccci_common.h>

#include "ccci_core.h"
#include "ccci_debug.h"
#include "ccci_bm.h"
#include "ccci_platform.h"
#include "modem_reg_base.h"
#include "modem_cldma.h"
#include "modem_ccif.h"

#ifdef ENABLE_DRAM_API
extern unsigned int get_max_DRAM_size (void);
extern unsigned int get_phys_offset (void);
#endif

#define TAG "plat"

static int is_4g_memory_size_support(void)
{
	#ifdef FEATURE_USING_4G_MEMORY_API
	return enable_4G();
	#else
	return 0;
	#endif
}

//===================================================
// MPU Region defination
//===================================================
#define MPU_REGION_ID_SEC_OS        0
#define MPU_REGION_ID_MD32          1
#define MPU_REGION_ID_MD32_SMEM     2
#define MPU_REGION_ID_MD1_SEC_SMEM  3
#define MPU_REGION_ID_MD2_SEC_SMEM  4
#define MPU_REGION_ID_MD1_ROM       5
#define MPU_REGION_ID_MD1_DSP		6
#define MPU_REGION_ID_MD2_ROM       7
#define MPU_REGION_ID_MD2_RW        8
#define MPU_REGION_ID_MD1_SMEM      9
#define MPU_REGION_ID_MD2_SMEM      10
#define MPU_REGION_ID_MD1_RW        14
#define MPU_REGION_ID_AP            15

unsigned long infra_ao_base;
//-- MD1 Bank 0
#define MD1_BANK0_MAP0 ((unsigned int*)(infra_ao_base+0x300))
#define MD1_BANK0_MAP1 ((unsigned int*)(infra_ao_base+0x304))
//-- MD1 Bank 4
#define MD1_BANK4_MAP0 ((unsigned int*)(infra_ao_base+0x308))
#define MD1_BANK4_MAP1 ((unsigned int*)(infra_ao_base+0x30C))

//-- MD2 Bank 0
#define MD2_BANK0_MAP0 ((unsigned int*)(infra_ao_base+0x310))
#define MD2_BANK0_MAP1 ((unsigned int*)(infra_ao_base+0x314))
//-- MD2 Bank 4
#define MD2_BANK4_MAP0 ((unsigned int*)(infra_ao_base+0x318))
#define MD2_BANK4_MAP1 ((unsigned int*)(infra_ao_base+0x31C))

void ccci_clear_md_region_protection(struct ccci_modem *md)
{
#ifdef ENABLE_EMI_PROTECTION
	unsigned int rom_mem_mpu_id, rw_mem_mpu_id;

	CCCI_INF_MSG(md->index, CORE, "Clear MD region protect...\n");
	switch(md->index) {
	case MD_SYS1:
		rom_mem_mpu_id = MPU_REGION_ID_MD1_ROM;
		rw_mem_mpu_id = MPU_REGION_ID_MD1_RW;
		break;
	case MD_SYS2:
		rom_mem_mpu_id = MPU_REGION_ID_MD2_ROM;
		rw_mem_mpu_id = MPU_REGION_ID_MD2_RW;
		break;
	default:
		CCCI_INF_MSG(md->index, CORE, "[error]MD ID invalid when clear MPU protect\n");
		return;
	}
	
	CCCI_INF_MSG(md->index, CORE, "Clear MPU protect MD ROM region<%d>\n", rom_mem_mpu_id);
	emi_mpu_set_region_protection(0,	  				/*START_ADDR*/
								  0,      				/*END_ADDR*/
								  rom_mem_mpu_id,       /*region*/
								  SET_ACCESS_PERMISSON(NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION));

	CCCI_INF_MSG(md->index, CORE, "Clear MPU protect MD R/W region<%d>\n", rw_mem_mpu_id);
	emi_mpu_set_region_protection(0,		  			/*START_ADDR*/
								  0,       				/*END_ADDR*/
								  rw_mem_mpu_id,        /*region*/
								  SET_ACCESS_PERMISSON(NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION));
#endif
}

void ccci_clear_dsp_region_protection(struct ccci_modem *md)
{
#ifdef ENABLE_EMI_PROTECTION
	unsigned int dsp_mem_mpu_id;

	CCCI_INF_MSG(md->index, CORE, "Clear DSP region protect...\n");
	switch(md->index) {
	case MD_SYS1:
		dsp_mem_mpu_id = MPU_REGION_ID_MD1_DSP;
		break;
	default:
		CCCI_INF_MSG(md->index, CORE, "[error]MD ID invalid when clear MPU protect\n");
		return;
	}
	
	CCCI_INF_MSG(md->index, CORE, "Clear MPU protect DSP ROM region<%d>\n", dsp_mem_mpu_id);
	emi_mpu_set_region_protection(0,	  				/*START_ADDR*/
								  0,      				/*END_ADDR*/
								  dsp_mem_mpu_id,       /*region*/
								  SET_ACCESS_PERMISSON(NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION));
#endif
}

/*
 * for some unkonw reason on 6582 and 6572, MD will read AP's memory during boot up, so we
 * set AP region as MD read-only at first, and re-set it to portected after MD boot up.
 * this function should be called right before sending runtime data.
 */
void ccci_set_ap_region_protection(struct ccci_modem *md)
{
#ifdef ENABLE_EMI_PROTECTION
	unsigned int ap_mem_mpu_id, ap_mem_mpu_attr;
	unsigned int kernel_base;
	unsigned int dram_size;

	if(is_4g_memory_size_support())
		kernel_base = 0;
	else
		kernel_base = get_phys_offset();
#ifdef ENABLE_DRAM_API
	dram_size = get_max_DRAM_size();
#else
	dram_size = 256*1024*1024;
#endif
	ap_mem_mpu_id = MPU_REGION_ID_AP;
	ap_mem_mpu_attr = SET_ACCESS_PERMISSON(    FORBIDDEN, NO_PROTECTION, FORBIDDEN, NO_PROTECTION,     FORBIDDEN,     FORBIDDEN, FORBIDDEN, NO_PROTECTION);
	CCCI_INF_MSG(md->index, CORE, "MPU Start protect AP region<%d:%08x:%08x> %x\n",
								ap_mem_mpu_id, kernel_base, (kernel_base+dram_size-1), ap_mem_mpu_attr); 
	emi_mpu_set_region_protection(kernel_base,
									(kernel_base+dram_size-1),
									 ap_mem_mpu_id,
									 ap_mem_mpu_attr);

#endif
}
EXPORT_SYMBOL(ccci_set_ap_region_protection);

void ccci_set_dsp_region_protection(struct ccci_modem *md, int loaded)
{
#ifdef ENABLE_EMI_PROTECTION
	unsigned int dsp_mem_mpu_id, dsp_mem_mpu_attr;
	unsigned int dsp_mem_phy_start, dsp_mem_phy_end;
	struct ccci_image_info *img_info;

	switch(md->index) {
	case MD_SYS1:
		dsp_mem_mpu_id = MPU_REGION_ID_MD1_DSP;
		if(!loaded)
			dsp_mem_mpu_attr = SET_ACCESS_PERMISSON(FORBIDDEN, FORBIDDEN, FORBIDDEN,     FORBIDDEN, FORBIDDEN, FORBIDDEN, SEC_R_NSEC_R, SEC_R_NSEC_R);
		else
			dsp_mem_mpu_attr = SET_ACCESS_PERMISSON(FORBIDDEN, FORBIDDEN, FORBIDDEN,     FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION, FORBIDDEN);	
		break;

	default:
		CCCI_ERR_MSG(md->index, CORE, "[error]invalid when MPU protect\n");
		return;
	}

	dsp_mem_phy_start = (unsigned int)md->mem_layout.dsp_region_phy;
	dsp_mem_phy_end = ((dsp_mem_phy_start + md->mem_layout.dsp_region_size + 0xFFFF)&(~0xFFFF)) - 0x1;

	CCCI_INF_MSG(md->index, CORE, "MPU Start protect DSP region<%d:%08x:%08x> %x\n",
								dsp_mem_mpu_id, dsp_mem_phy_start, dsp_mem_phy_end, dsp_mem_mpu_attr); 
	emi_mpu_set_region_protection(dsp_mem_phy_start,
									dsp_mem_phy_end,
									dsp_mem_mpu_id,
									dsp_mem_mpu_attr);
#endif
}
EXPORT_SYMBOL(ccci_set_dsp_region_protection);

void ccci_set_mem_access_protection(struct ccci_modem *md)
{
#ifdef ENABLE_EMI_PROTECTION
	unsigned int shr_mem_phy_start, shr_mem_phy_end, shr_mem_mpu_id, shr_mem_mpu_attr;
	unsigned int rom_mem_phy_start, rom_mem_phy_end, rom_mem_mpu_id, rom_mem_mpu_attr;
	unsigned int rw_mem_phy_start, rw_mem_phy_end, rw_mem_mpu_id, rw_mem_mpu_attr;
	unsigned int ap_mem_mpu_id, ap_mem_mpu_attr;
	struct ccci_image_info *img_info;
	struct ccci_mem_layout *md_layout;
	unsigned int kernel_base;
	unsigned int dram_size;

	// For MT6752
	//===============================================================================================================
	//                  | Region |  D0(AP)    |  D1(MD1)   | D2(CONN) | D3(MD32)  | D4(MM)    | D5(MD2)  | D6(MFG)   |D7     |
	//--------------+------------------------------------------------------------------------------------------------
	// Secure OS     |    0   |RW(S)       |Forbidden   |Forbidden   |Forbidden  |Forbidden  |Forbidden   |Forbidden  |Forbidden  |
	//--------------+------------------------------------------------------------------------------------------------
	// MD32 Code    |    1   |Forbidden   |Forbidden   |Forbidden   |RW(S)      |Forbidden  |Forbidden   |Forbidden  |Forbidden  |
	//--------------+------------------------------------------------------------------------------------------------
	// MD32 SMEM   |    2   |No Protect   |Forbidden  |Forbidden   |No protect  |Forbidden  |Forbidden  |Forbidden  |Forbidden  |
	//--------------+------------------------------------------------------------------------------------------------
	// MD1SecSMEM |    3   |R/W(S)      |No protect  |Forbidden   |Forbidden  |Forbidden  |Forbidden  |Forbidden  |Forbidden  |
	//--------------+------------------------------------------------------------------------------------------------
	// MD2SecSMEM |    4   |RW(S)        |Forbidden   |Forbidden  |Forbidden  |Forbidden   |No protect |Forbidden  |Forbidden  |
	//--------------+------------------------------------------------------------------------------------------------
	// MD1 ROM       |    5   |R(S)           |R(S)          |Forbidden   |Forbidden  |Forbidden  |Forbidden  |Forbidden  |Forbidden  |
	//--------------+------------------------------------------------------------------------------------------------
	// MD1 DSP       |    6   |Forbidden   |No protect  |Forbidden   |Forbidden  |Forbidden  |Forbidden  |Forbidden  |Forbidden  |
	//--------------+------------------------------------------------------------------------------------------------
	// MD2 ROM       |    7   |R(S)           |Forbidden   |Forbidden  |Forbidden  |Forbidden  |R(S)          |Forbidden  |Forbidden  |
	//--------------+------------------------------------------------------------------------------------------------
	// MD2 R/W+     |    8   |Forbidden   |Forbidden   |Forbidden  |Forbidden  |Forbidden  |No protect  |Forbidden  |Forbidden  |
	//--------------+------------------------------------------------------------------------------------------------
	// MD1 SMEM     |    9    |No protect  |No protect  |Forbidden  |Forbidden  |Forbidden  |No protect  |Forbidden  |Forbidden  |
	//--------------+------------------------------------------------------------------------------------------------
	// MD2 SMEM     |    10  |No protect  |No protect  |Forbidden   |Forbidden  |Forbidden |No protect  |Forbidden  |Forbidden  |
	//--------------+------------------------------------------------------------------------------------------------
	// MD1 R/W+     |   14  |Forbidden   |No protect  |Forbidden   |Forbidden  |Forbidden  |Forbidden  |Forbidden  |Forbidden  |
	//--------------+------------------------------------------------------------------------------------------------
	// AP                 |    15  |No protect  |Forbidden  |Forbidden   |No protect |No protect |Forbidden   |No protect |Forbidden |
	//===============================================================================================================
	switch(md->index) {
	case MD_SYS1:
		img_info = &md->img_info[IMG_MD];
		md_layout = &md->mem_layout;
		rom_mem_mpu_id = MPU_REGION_ID_MD1_ROM;
		rw_mem_mpu_id = MPU_REGION_ID_MD1_RW;
		shr_mem_mpu_id = MPU_REGION_ID_MD1_SMEM;
		rom_mem_mpu_attr = SET_ACCESS_PERMISSON(FORBIDDEN, FORBIDDEN, FORBIDDEN,     FORBIDDEN, FORBIDDEN, FORBIDDEN, SEC_R_NSEC_R,  SEC_R_NSEC_R);
		rw_mem_mpu_attr =  SET_ACCESS_PERMISSON(FORBIDDEN, FORBIDDEN, FORBIDDEN,     FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION, FORBIDDEN);
		shr_mem_mpu_attr = SET_ACCESS_PERMISSON(FORBIDDEN, FORBIDDEN, NO_PROTECTION, FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION, NO_PROTECTION);			
		break;
	case MD_SYS2:
		img_info = &md->img_info[IMG_MD];
		md_layout = &md->mem_layout;
		rom_mem_mpu_id = MPU_REGION_ID_MD2_ROM;
		rw_mem_mpu_id = MPU_REGION_ID_MD2_RW;
		shr_mem_mpu_id = MPU_REGION_ID_MD2_SMEM;
		rom_mem_mpu_attr = SET_ACCESS_PERMISSON(FORBIDDEN, FORBIDDEN, SEC_R_NSEC_R,  FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,     SEC_R_NSEC_R);
		rw_mem_mpu_attr =  SET_ACCESS_PERMISSON(FORBIDDEN, FORBIDDEN, NO_PROTECTION, FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,     FORBIDDEN);
		shr_mem_mpu_attr = SET_ACCESS_PERMISSON(FORBIDDEN, FORBIDDEN, NO_PROTECTION, FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION, NO_PROTECTION);
		break;

	default:
		CCCI_ERR_MSG(md->index, CORE, "[error]invalid when MPU protect\n");
		return;
	}

	if(is_4g_memory_size_support())
		kernel_base = 0;
	else
		kernel_base = get_phys_offset();
#ifdef ENABLE_DRAM_API
	dram_size = get_max_DRAM_size();
#else
	dram_size = 256*1024*1024;
#endif
	ap_mem_mpu_id = MPU_REGION_ID_AP;
	ap_mem_mpu_attr = SET_ACCESS_PERMISSON(NO_PROTECTION, NO_PROTECTION, SEC_R_NSEC_R,  NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, SEC_R_NSEC_R, NO_PROTECTION);

	/*
	 * if set start=0x0, end=0x10000, the actural protected area will be 0x0-0x1FFFF,
	 * here we use 64KB align, MPU actually request 32KB align since MT6582, but this works...
	 * we assume emi_mpu_set_region_protection will round end address down to 64KB align.
	 */
	rom_mem_phy_start = (unsigned int)md_layout->md_region_phy;
	rom_mem_phy_end   = ((rom_mem_phy_start + img_info->size + 0xFFFF)&(~0xFFFF)) - 0x1;
	rw_mem_phy_start  = rom_mem_phy_end + 0x1;
	rw_mem_phy_end	  = rom_mem_phy_start + md_layout->md_region_size - 0x1;
	shr_mem_phy_start = (unsigned int)md_layout->smem_region_phy;
	shr_mem_phy_end   = ((shr_mem_phy_start + md_layout->smem_region_size + 0xFFFF)&(~0xFFFF)) - 0x1;
	
	CCCI_INF_MSG(md->index, CORE, "MPU Start protect MD ROM region<%d:%08x:%08x> %x\n", 
                              	rom_mem_mpu_id, rom_mem_phy_start, rom_mem_phy_end, rom_mem_mpu_attr);
	emi_mpu_set_region_protection(rom_mem_phy_start,	  /*START_ADDR*/
									rom_mem_phy_end,      /*END_ADDR*/
									rom_mem_mpu_id,       /*region*/
									rom_mem_mpu_attr);

	CCCI_INF_MSG(md->index, CORE, "MPU Start protect MD R/W region<%d:%08x:%08x> %x\n", 
                              	rw_mem_mpu_id, rw_mem_phy_start, rw_mem_phy_end, rw_mem_mpu_attr);
	emi_mpu_set_region_protection(rw_mem_phy_start,		  /*START_ADDR*/
									rw_mem_phy_end,       /*END_ADDR*/
									rw_mem_mpu_id,        /*region*/
									rw_mem_mpu_attr);

	CCCI_INF_MSG(md->index, CORE, "MPU Start protect MD Share region<%d:%08x:%08x> %x\n", 
                              	shr_mem_mpu_id, shr_mem_phy_start, shr_mem_phy_end, shr_mem_mpu_attr);
	emi_mpu_set_region_protection(shr_mem_phy_start,	  /*START_ADDR*/
									shr_mem_phy_end,      /*END_ADDR*/
									shr_mem_mpu_id,       /*region*/
									shr_mem_mpu_attr);

// This part need to move common part
#if 1
	CCCI_INF_MSG(md->index, CORE, "MPU Start protect AP region<%d:%08x:%08x> %x\n",
								ap_mem_mpu_id, kernel_base, (kernel_base+dram_size-1), ap_mem_mpu_attr); 
	emi_mpu_set_region_protection(kernel_base,
								  (kernel_base+dram_size-1),
								  ap_mem_mpu_id,
								  ap_mem_mpu_attr);
#endif
#endif
}
EXPORT_SYMBOL(ccci_set_mem_access_protection);

// This function has phase out!!!
int set_ap_smem_remap(struct ccci_modem *md, phys_addr_t src, phys_addr_t des)
{
	unsigned int remap1_val = 0;
	unsigned int remap2_val = 0;
	static int	smem_remapped = 0;
	
	if(!smem_remapped) {
		smem_remapped = 1;
		remap1_val =(((des>>24)|0x1)&0xFF)
				  + (((INVALID_ADDR>>16)|1<<8)&0xFF00)
				  + (((INVALID_ADDR>>8)|1<<16)&0xFF0000)
				  + (((INVALID_ADDR>>0)|1<<24)&0xFF000000);
		
		remap2_val =(((INVALID_ADDR>>24)|0x1)&0xFF)
				  + (((INVALID_ADDR>>16)|1<<8)&0xFF00)
				  + (((INVALID_ADDR>>8)|1<<16)&0xFF0000)
				  + (((INVALID_ADDR>>0)|1<<24)&0xFF000000);
		
		CCCI_INF_MSG(md->index, CORE, "AP Smem remap: [%llx]->[%llx](%08x:%08x)\n", (unsigned long long)des, (unsigned long long)src, remap1_val, remap2_val);

#ifdef 	ENABLE_MEM_REMAP_HW
		mt_reg_sync_writel(remap1_val, AP_BANK4_MAP0);
		mt_reg_sync_writel(remap2_val, AP_BANK4_MAP1);
		mt_reg_sync_writel(remap2_val, AP_BANK4_MAP1); // HW bug, write twice to activate setting
#endif					
	}
	return 0;
}


int set_md_smem_remap(struct ccci_modem *md, phys_addr_t src, phys_addr_t des, phys_addr_t invalid)
{
	unsigned int remap1_val = 0;
	unsigned int remap2_val = 0;

	if(is_4g_memory_size_support()) {
		des &= 0xFFFFFFFF;
	} else {
		des -= KERN_EMI_BASE;
	}
	
	switch(md->index) {
	case MD_SYS1:
		remap1_val =(((des>>24)|0x1)&0xFF)
				  + ((((invalid+0x2000000*0)>>16)|1<<8)&0xFF00)
				  + ((((invalid+0x2000000*1)>>8)|1<<16)&0xFF0000)
				  + ((((invalid+0x2000000*2)>>0)|1<<24)&0xFF000000);
		remap2_val =((((invalid+0x2000000*3)>>24)|0x1)&0xFF)
				  + ((((invalid+0x2000000*4)>>16)|1<<8)&0xFF00)
				  + ((((invalid+0x2000000*5)>>8)|1<<16)&0xFF0000)
				  + ((((invalid+0x2000000*6)>>0)|1<<24)&0xFF000000);
		
#ifdef 	ENABLE_MEM_REMAP_HW
        mt_reg_sync_writel(remap1_val, MD1_BANK4_MAP0);
        mt_reg_sync_writel(remap2_val, MD1_BANK4_MAP1);
#endif
		break;
    case MD_SYS2:
        remap1_val =(((des>>24)|0x1)&0xFF)
				  + ((((invalid+0x2000000*0)>>16)|1<<8)&0xFF00)
				  + ((((invalid+0x2000000*1)>>8)|1<<16)&0xFF0000)
				  + ((((invalid+0x2000000*2)>>0)|1<<24)&0xFF000000);
		remap2_val =((((invalid+0x2000000*3)>>24)|0x1)&0xFF)
				  + ((((invalid+0x2000000*4)>>16)|1<<8)&0xFF00)
				  + ((((invalid+0x2000000*5)>>8)|1<<16)&0xFF0000)
				  + ((((invalid+0x2000000*6)>>0)|1<<24)&0xFF000000);

#ifdef  ENABLE_MEM_REMAP_HW
        mt_reg_sync_writel(remap1_val, MD2_BANK4_MAP0);
        mt_reg_sync_writel(remap2_val, MD2_BANK4_MAP1);
#endif
        break;
	default:
		break;
	}

	CCCI_INF_MSG(md->index, CORE, "MD Smem remap:[%llx]->[%llx](%08x:%08x)\n", (unsigned long long)des, (unsigned long long)src, remap1_val, remap2_val);
	return 0;
}


int set_md_rom_rw_mem_remap(struct ccci_modem *md, phys_addr_t src, phys_addr_t des, phys_addr_t invalid)
{
	unsigned int remap1_val = 0;
	unsigned int remap2_val = 0;


	if(is_4g_memory_size_support()) {
		des &= 0xFFFFFFFF;
	} else {
		des -= KERN_EMI_BASE;
	}
	
	switch(md->index) {
	case MD_SYS1:
		remap1_val =(((des>>24)|0x1)&0xFF)
				  + ((((des+0x2000000*1)>>16)|1<<8)&0xFF00)
				  + ((((des+0x2000000*2)>>8)|1<<16)&0xFF0000)
				  + ((((invalid+0x02000000*7)>>0)|1<<24)&0xFF000000);
		remap2_val =((((invalid+0x02000000*8)>>24)|0x1)&0xFF)
				  + ((((invalid+0x02000000*9)>>16)|1<<8)&0xFF00)
				  + ((((invalid+0x02000000*10)>>8)|1<<16)&0xFF0000)
				  + ((((invalid+0x02000000*11)>>0)|1<<24)&0xFF000000);
		
#ifdef 	ENABLE_MEM_REMAP_HW
        mt_reg_sync_writel(remap1_val, MD1_BANK0_MAP0);
        mt_reg_sync_writel(remap2_val, MD1_BANK0_MAP1);
#endif
		break;
    case MD_SYS2:
        remap1_val =(((des>>24)|0x1)&0xFF)
                  + ((((des+0x2000000*1)>>16)|1<<8)&0xFF00)
                  + ((((des+0x2000000*2)>>8)|1<<16)&0xFF0000)
                  + ((((invalid+0x2000000*7)>>0)|1<<24)&0xFF000000);
        remap2_val =((((invalid+0x2000000*8)>>24)|0x1)&0xFF)
                  + ((((invalid+0x2000000*9)>>16)|1<<8)&0xFF00)
                  + ((((invalid+0x2000000*10)>>8)|1<<16)&0xFF0000)
                  + ((((invalid+0x2000000*11)>>0)|1<<24)&0xFF000000);

#ifdef  ENABLE_MEM_REMAP_HW
        mt_reg_sync_writel(remap1_val, MD2_BANK0_MAP0);
        mt_reg_sync_writel(remap2_val, MD2_BANK0_MAP1);
#endif
        break;
	default:
		break;
	}

	CCCI_INF_MSG(md->index, CORE, "MD ROM mem remap:[%llx]->[%llx](%08x:%08x)\n", (unsigned long long)des, (unsigned long long)src, remap1_val, remap2_val);
	return 0;
}

void ccci_set_mem_remap(struct ccci_modem *md, unsigned long smem_offset, phys_addr_t invalid)
{
	unsigned long remainder;


	if(is_4g_memory_size_support()) {
		invalid &= 0xFFFFFFFF;
		CCCI_INF_MSG(md->index, CORE, "4GB mode enabled, invalid_map=%llx\n", (unsigned long long)invalid);
	} else {
		invalid -= KERN_EMI_BASE;
		CCCI_INF_MSG(md->index, CORE, "4GB mode disabled, invalid_map=%llx\n", (unsigned long long)invalid);
	}
	
	// Set share memory remapping
#if 0 // no hardware AP remap after MT6592
	set_ap_smem_remap(md, 0x40000000, md->mem_layout.smem_region_phy_before_map);
	md->mem_layout.smem_region_phy = smem_offset + 0x40000000;
#endif
	/*
	 * always remap only the 1 slot where share memory locates. smem_offset is the offset between
	 * ROM start address(32M align) and share memory start address.
	 * (AP view smem address) - [(smem_region_phy) - (bank4 start address) - (un-32M-align space)]
	 * = (MD view smem address)
	 */
	remainder = smem_offset % 0x02000000;
	md->mem_layout.smem_offset_AP_to_MD = md->mem_layout.smem_region_phy - (remainder + 0x40000000);
	set_md_smem_remap(md, 0x40000000, md->mem_layout.md_region_phy + (smem_offset-remainder), invalid); 
	CCCI_INF_MSG(md->index, CORE, "AP to MD share memory offset 0x%X", md->mem_layout.smem_offset_AP_to_MD);

	// Set md image and rw runtime memory remapping
	set_md_rom_rw_mem_remap(md, 0x00000000, md->mem_layout.md_region_phy, invalid);
}

/*
 * when MD attached its codeviser for debuging, this bit will be set. so CCCI should disable some 
 * checkings and operations as MD may not respond to us.
 */
unsigned int ccci_get_md_debug_mode(struct ccci_modem *md)
{
	unsigned int dbg_spare;
	static unsigned int debug_setting_flag = 0;
	return 0;
	#if 0
	// this function does NOT distinguish modem ID, may be a risk point
	if((debug_setting_flag&DBG_FLAG_JTAG) == 0) {
		dbg_spare = ioread32((void __iomem *)MD_DEBUG_MODE);
		if(dbg_spare & MD_DBG_JTAG_BIT) {
			CCCI_INF_MSG(md->index, CORE, "Jtag Debug mode(%08x)\n", dbg_spare);
			debug_setting_flag |= DBG_FLAG_JTAG;
			mt_reg_sync_writel(dbg_spare & (~MD_DBG_JTAG_BIT), MD_DEBUG_MODE);
		}
	}
	return debug_setting_flag;
	#endif
}
EXPORT_SYMBOL(ccci_get_md_debug_mode);

void ccci_get_platform_version(char * ver)
{
#ifdef ENABLE_CHIP_VER_CHECK
	sprintf(ver, "MT%04x_S%02x", get_chip_hw_ver_code(), (get_chip_hw_subcode()&0xFF));
#else
	sprintf(ver, "MT6595_S00");
#endif
}

static int ccci_md_low_power_notify(struct ccci_modem *md, LOW_POEWR_NOTIFY_TYPE type, int level)
{
	#ifdef FEATURE_LOW_BATTERY_SUPPORT
	unsigned int reserve = 0xFFFFFFFF;
	int ret = 0;

	CCCI_INF_MSG(md->index, TAG, "low power notification type=%d, level=%d\n", type, level);
	/*
	 * byte3 byte2 byte1 byte0
	 *    0   4G   3G   2G
	 */
	switch(type) {
	case LOW_BATTERY:
		if(level == LOW_BATTERY_LEVEL_0) {
			reserve = 0; // 0
		} else if(level == LOW_BATTERY_LEVEL_1 || level == LOW_BATTERY_LEVEL_2) {
			reserve = (1<<6); // 64
		}
		ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_LOW_BATTERY_LEVEL, reserve, 1);
		if(ret)
			CCCI_ERR_MSG(md->index, TAG, "send low battery notification fail, ret=%d\n", ret);
		break;
	case BATTERY_PERCENT:
		if(level == BATTERY_PERCENT_LEVEL_0) {
			reserve = 0; // 0
		} else if(level == BATTERY_PERCENT_LEVEL_1) {
			reserve = (1<<6); // 64
		}
		ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_LOW_BATTERY_LEVEL, reserve, 1);
		if(ret)
			CCCI_ERR_MSG(md->index, TAG, "send battery percent notification fail, ret=%d\n", ret);
		break;
	default:
		break;
	};

	return ret;
	#endif
	return 0;
}


#ifdef FEATURE_LOW_BATTERY_SUPPORT
static void ccci_md_low_battery_cb(LOW_BATTERY_LEVEL level)
{
    int idx=0;
	struct ccci_modem *md;
    for(idx=0;idx<MAX_MD_NUM;idx++)
    {
        md = ccci_get_modem_by_id(idx);
        if(md!=NULL)
            ccci_md_low_power_notify(md, LOW_BATTERY, level);
    }
}
static void ccci_md_battery_percent_cb(BATTERY_PERCENT_LEVEL level)
{
    int idx=0;
	struct ccci_modem *md;
    for(idx=0;idx<MAX_MD_NUM;idx++)
    {
        md = ccci_get_modem_by_id(idx);
        if(md!=NULL)
            ccci_md_low_power_notify(md, BATTERY_PERCENT, level);
    }    
}
#endif

int ccci_platform_init(struct ccci_modem *md)
{
    static int init=0;
    unsigned int reg_value;
    struct device_node * node;
    if(init==0)
    {
        init = 1;
        // Get infra cfg ao base
        //node = of_find_compatible_node(NULL, NULL, "mediatek,INFRACFG_AO");
        //infra_ao_base = of_iomap(node, 0);
        //CCCI_INF_MSG(md->index, CORE, "infra_ao_base:%p\n", infra_ao_base);
#if (defined(CCCI_SMT_SETTING)||defined(MTK_ENABLE_MD2))
        CCCI_INF_MSG(md->index, CORE, "ccci_platform_init:set VTCXO_1 on,bit3=1\n");
        pmic_config_interface(0x0A02, 0x1, 0x1, 3); //bit:3: =>b1
        CCCI_INF_MSG(md->index, CORE, "ccci_platform_init:set SRCLK_EN_SEL on,bit(13,12)=(0,1)\n");
        pmic_config_interface(0x0A02, 0x1, 0x3, 12);//bit:13:12=>b01
#endif
#ifdef MTK_ENABLE_MD2
         //reg_value = *((volatile unsigned int*)0x10001F00);
        reg_value = ccci_read32(infra_ao_base,0xF00);
        reg_value &= ~(0x1E000000);
        reg_value |=0x12000000;
        // *((volatile unsigned int*)0x10001F00) = reg_value;
        ccci_write32(infra_ao_base,0xF00, reg_value);        
        CCCI_INF_MSG(md->index, CORE, "ccci_platform_init: md2 enable, set SRCLKEN infra_misc(0x1000_1F00), bit(28,27,26,25)=0x%x\n",(ccci_read32(infra_ao_base,0xF00)&0x1E000000));

        //reg_value = *((volatile unsigned int*)0x10001F08);
        reg_value = ccci_read32(infra_ao_base,0xF08);
        reg_value &= ~(0x001F0078);
        reg_value |=0x001B0048;
        // *((volatile unsigned int*)0x10001F00) = reg_value;
        ccci_write32(infra_ao_base,0xF08, reg_value);        
        CCCI_INF_MSG(md->index, CORE, "ccci_platform_init:set PLL misc_config(0x1000_1F08), bit(20,19,18,17,16,6,5,4,3)=0x%x\n",(ccci_read32(infra_ao_base,0xF08)&0x001F0078)); 
#else
         //reg_value = *((volatile unsigned int*)0x10001F00);
        reg_value = ccci_read32(infra_ao_base,0xF00);
        reg_value &= ~(0x1E000000);
        reg_value |=0x0A000000;
        //*((volatile unsigned int*)0x10001F00) = reg_value;
        ccci_write32(infra_ao_base,0xF00,reg_value);
        CCCI_INF_MSG(md->index, CORE, "ccci_platform_init: md2 disable, set SRCLKEN infra_misc(0x1000_1F00), bit(28,27,26,25)=0x%x\n",(ccci_read32(infra_ao_base,0xF00)&0x1E000000));
        //reg_value = *((volatile unsigned int*)0x10001F08);
        reg_value = ccci_read32(infra_ao_base,0xF08);
        reg_value &= ~(0x001F0078);
        reg_value |=0x00000000;
        // *((volatile unsigned int*)0x10001F00) = reg_value;
        ccci_write32(infra_ao_base,0xF08,reg_value);        
        CCCI_INF_MSG(md->index, CORE, "ccci_platform_init: set PLL misc_config(0x1000_1F08), bit(20,19,18,17,16,6,5,4,3)=0x%x\n",(ccci_read32(infra_ao_base,0xF08)&0x001F0078)); 
#endif
    }

	return 0;
}

int ccci_plat_common_init(void)
{
    struct device_node * node;
    // Get infra cfg ao base
    node = of_find_compatible_node(NULL, NULL, "mediatek,INFRACFG_AO");
    infra_ao_base = of_iomap(node, 0);
    CCCI_INF_MSG(-1, CORE, "infra_ao_base:%p\n", infra_ao_base);
#ifdef FEATURE_LOW_BATTERY_SUPPORT    
    register_low_battery_notify(&ccci_md_low_battery_cb, LOW_BATTERY_PRIO_MD);
    register_battery_percent_notify(&ccci_md_battery_percent_cb, BATTERY_PERCENT_PRIO_MD);
#endif
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mdcldma_of_ids[] = {
	{.compatible = "mediatek,MDCLDMA", },
	{}
};

static const struct of_device_id ccif_of_ids[] = {
	{.compatible = "mediatek,AP_CCIF1", },
	{}
};

void* get_ccci_of_match_table(const char str[])
{
	if(strcmp(str, "MDCLDMA")==0) {
		// Type: CLDMA at MD side driver
		return mdcldma_of_ids;
	} else if(strcmp(str, "CCIF")==0){
		// Type: only ccif hw driver
		return ccif_of_ids;
	}
	return NULL;
}

#endif

int get_modem_hw_info(void* dev, void *cfg, void *hw)
{
	struct platform_device *dev_ptr = (struct platform_device *)dev;
	struct ccci_dev_cfg *dev_cfg = (struct ccci_dev_cfg *)cfg;
	md_hw_info_t *hw_info = (md_hw_info_t *)hw;
	memset(dev_cfg, 0, sizeof(struct ccci_dev_cfg));
	memset(hw_info, 0, sizeof(md_hw_info_t));

	#ifdef CONFIG_OF
	if(dev_ptr->dev.of_node == NULL) {
		CCCI_INF_MSG(-1, TAG, "modem OF node NULL\n");
		return -1;
	}

	of_property_read_u32(dev_ptr->dev.of_node, "cell-index", &dev_cfg->index);
	CCCI_INF_MSG(-1, TAG, "modem hw info get idx:%d\n", dev_cfg->index);
	if(!get_modem_is_enabled(dev_cfg->index)) {
		CCCI_INF_MSG(-1, TAG, "modem %d not enable, exit\n", dev_cfg->index + 1);
		return -1;
	}
	#else
	struct ccci_dev_cfg* dev_cfg_ptr = (struct ccci_dev_cfg*)dev->dev.platform_data;
	dev_cfg->index = dev_cfg_ptr->index;

	CCCI_INF_MSG(-1, TAG, "modem hw info get idx:%d\n", dev_cfg->index);
	if(!get_modem_is_enabled(dev_cfg->index)) {
		CCCI_INF_MSG(-1, TAG, "modem %d not enable, exit\n", dev_cfg->index + 1);
		return -1;
	}
	#endif

	switch(dev_cfg->index) {
	#ifdef MTK_ENABLE_MD1
	case 0: //MD_SYS1
		#ifdef CONFIG_OF
		of_property_read_u32(dev_ptr->dev.of_node, "cldma,major", &dev_cfg->major);
		of_property_read_u32(dev_ptr->dev.of_node, "cldma,minor_base", &dev_cfg->minor_base);
		of_property_read_u32(dev_ptr->dev.of_node, "cldma,capability", &dev_cfg->capability);

		hw_info->cldma_ap_base = of_iomap(dev_ptr->dev.of_node, 0);
  	hw_info->cldma_md_base = of_iomap(dev_ptr->dev.of_node, 1);
  	hw_info->ap_ccif_base = of_iomap(dev_ptr->dev.of_node, 2);
		hw_info->md_ccif_base = hw_info->ap_ccif_base+0x1000;

		hw_info->cldma_irq_id = irq_of_parse_and_map(dev_ptr->dev.of_node, 0);
		hw_info->ap_ccif_irq_id = irq_of_parse_and_map(dev_ptr->dev.of_node, 1);
		hw_info->md_wdt_irq_id = irq_of_parse_and_map(dev_ptr->dev.of_node, 2);
		//hw_info->ap2md_bus_timeout_irq_id = 0;

		// Device tree using none flag to register irq, sensitivity has set at "irq_of_parse_and_map"
		hw_info->cldma_irq_flags = IRQF_TRIGGER_NONE;
		hw_info->ap_ccif_irq_flags = IRQF_TRIGGER_NONE;
		hw_info->md_wdt_irq_flags = IRQF_TRIGGER_NONE;
		hw_info->ap2md_bus_timeout_irq_flags = IRQF_TRIGGER_NONE;
		#else
		dev_cfg->major = dev_cfg_ptr->major;
		dev_cfg->minor_base = dev_cfg_ptr->minor_base;
		dev_cfg->capability = dev_cfg_ptr->capability;

		hw_info->cldma_ap_base = CLDMA_AP_BASE;
		hw_info->cldma_md_base = CLDMA_MD_BASE;
  	hw_info->ap_ccif_base = AP_CCIF0_BASE;
		hw_info->md_ccif_base = hw_info->ap_ccif_base+0x1000;

		hw_info->cldma_irq_id = CLDMA_AP_IRQ;
		hw_info->ap_ccif_irq_id = CCIF0_AP_IRQ;
		hw_info->md_wdt_irq_id = MD_WDT_IRQ;
		//hw_info->ap2md_bus_timeout_irq_id = AP2MD_BUS_TIMEOUT_IRQ;

		// Device tree using none flag to register irq, sensitivity has set at "irq_of_parse_and_map"
		hw_info->cldma_irq_flags = IRQF_TRIGGER_HIGH;
		hw_info->ap_ccif_irq_flags = IRQF_TRIGGER_LOW;
		hw_info->md_wdt_irq_flags = IRQF_TRIGGER_FALLING;
		//hw_info->ap2md_bus_timeout_irq_flags = IRQF_TRIGGER_NONE;
		#endif

		hw_info->sram_size = CCIF_SRAM_SIZE;
		hw_info->md_rgu_base = MD_RGU_BASE;
		hw_info->md_boot_slave_Vector = MD_BOOT_VECTOR;
		hw_info->md_boot_slave_Key = MD_BOOT_VECTOR_KEY;
		hw_info->md_boot_slave_En = MD_BOOT_VECTOR_EN;
		
		break;
		#endif

	#ifdef MTK_ENABLE_MD2
	case 1: //MD_SYS2
		#ifdef CONFIG_OF
		of_property_read_u32(dev_ptr->dev.of_node, "ccif,major", &dev_cfg->major);
		of_property_read_u32(dev_ptr->dev.of_node, "ccif,minor_base", &dev_cfg->minor_base);
		of_property_read_u32(dev_ptr->dev.of_node, "ccif,capability", &dev_cfg->capability);

  	hw_info->ap_ccif_base = of_iomap(dev_ptr->dev.of_node, 0);
		hw_info->md_ccif_base = hw_info->ap_ccif_base+0x1000;

		hw_info->ap_ccif_irq_id = irq_of_parse_and_map(dev_ptr->dev.of_node, 0);
		hw_info->md_wdt_irq_id = irq_of_parse_and_map(dev_ptr->dev.of_node, 1);

		// Device tree using none flag to register irq, sensitivity has set at "irq_of_parse_and_map"
		hw_info->ap_ccif_irq_flags = IRQF_TRIGGER_NONE;
		hw_info->md_wdt_irq_flags = IRQF_TRIGGER_NONE;
		#endif

		hw_info->sram_size = CCIF_SRAM_SIZE;
		hw_info->md_rgu_base = MD2_RGU_BASE;
		hw_info->md_boot_slave_Vector = MD2_BOOT_VECTOR;
		hw_info->md_boot_slave_Key = MD2_BOOT_VECTOR_KEY;
		hw_info->md_boot_slave_En = MD2_BOOT_VECTOR_EN;
		
		break;
		#endif

	default:
		return -1;
	}

	CCCI_INF_MSG(dev_cfg->index, TAG, "modem cldma of node get dev_major:%d\n", dev_cfg->major);
	CCCI_INF_MSG(dev_cfg->index, TAG, "modem cldma of node get minor_base:%d\n", dev_cfg->minor_base);
	CCCI_INF_MSG(dev_cfg->index, TAG, "modem cldma of node get capability:%d\n", dev_cfg->capability);

	CCCI_INF_MSG(dev_cfg->index, TAG, "ap_cldma_base:%p\n", hw_info->cldma_ap_base);
	CCCI_INF_MSG(dev_cfg->index, TAG, "md_cldma_base:%p\n", hw_info->cldma_md_base);
	CCCI_INF_MSG(dev_cfg->index, TAG, "ap_ccif_base:%p\n", hw_info->ap_ccif_base);
	CCCI_INF_MSG(dev_cfg->index, TAG, "cldma_irq_id:%d\n", hw_info->cldma_irq_id);
	CCCI_INF_MSG(dev_cfg->index, TAG, "ccif_irq_id:%d\n", hw_info->ap_ccif_irq_id);
	CCCI_INF_MSG(dev_cfg->index, TAG, "md_wdt_irq_id:%d\n", hw_info->md_wdt_irq_id);

	return 0;
}

struct md1_ext_reg{
	void __iomem *md_peer_wakeup;
	void __iomem *md_bus_status;
	void __iomem *md_pc_monitor;
	void __iomem *md_topsm_status;
	void __iomem *md_ost_status;
};

static struct md1_ext_reg cldma_dbg_reg;

int io_remap_md_side_register(int md_id, void *md_ptr)
{
	struct md_cd_ctrl * cldma_ctrl;
	struct md_ccif_ctrl* ccif_ctrl;
	unsigned long vet;
	if(md_id == 0) {
		// MD SYS1
		cldma_ctrl = (struct md_cd_ctrl *)md_ptr;
		//cldma_ctrl->cldma_ap_base = ioremap_nocache(cldma_ctrl->hw_info->cldma_ap_base, CLDMA_AP_LENGTH);
		//cldma_ctrl->cldma_md_base = ioremap_nocache(cldma_ctrl->hw_info->cldma_md_base, CLDMA_MD_LENGTH);
		cldma_ctrl->cldma_ap_base = (void __iomem *)(cldma_ctrl->hw_info->cldma_ap_base);
		cldma_ctrl->cldma_md_base = (void __iomem *)(cldma_ctrl->hw_info->cldma_md_base);
		cldma_ctrl->md_boot_slave_Vector = ioremap_nocache(cldma_ctrl->hw_info->md_boot_slave_Vector, 0x4);
		cldma_ctrl->md_boot_slave_Key = ioremap_nocache(cldma_ctrl->hw_info->md_boot_slave_Key, 0x4);
		cldma_ctrl->md_boot_slave_En = ioremap_nocache(cldma_ctrl->hw_info->md_boot_slave_En, 0x4);
		cldma_ctrl->md_rgu_base = ioremap_nocache(cldma_ctrl->hw_info->md_rgu_base, 0x40);
		cldma_ctrl->md_global_con0 = ioremap_nocache(MD_GLOBAL_CON0, 0x4);

		cldma_dbg_reg.md_bus_status = ioremap_nocache(MD_BUS_STATUS_BASE, MD_BUS_STATUS_LENGTH);
		cldma_dbg_reg.md_pc_monitor = ioremap_nocache(MD_PC_MONITOR_BASE, MD_PC_MONITOR_LENGTH);
		cldma_dbg_reg.md_topsm_status = ioremap_nocache(MD_TOPSM_STATUS_BASE, MD_TOPSM_STATUS_LENGTH);
		cldma_dbg_reg.md_ost_status = ioremap_nocache(MD_OST_STATUS_BASE, MD_OST_STATUS_LENGTH);
		#ifdef MD_PEER_WAKEUP
		cldma_dbg_reg.md_peer_wakeup = ioremap_nocache(MD_PEER_WAKEUP, 0x4);
		#endif
		cldma_ctrl->ext_reg_info = &cldma_dbg_reg;
		return 0;
	}

	if(md_id == 1) {
		// MD SYS2
		ccif_ctrl = (struct md_ccif_ctrl *)md_ptr;

		ccif_ctrl->md_boot_slave_Vector = ioremap_nocache(ccif_ctrl->hw_info->md_boot_slave_Vector, 0x4);
		ccif_ctrl->md_boot_slave_Key = ioremap_nocache(ccif_ctrl->hw_info->md_boot_slave_Key, 0x4);
		ccif_ctrl->md_boot_slave_En = ioremap_nocache(ccif_ctrl->hw_info->md_boot_slave_En, 0x4);
		ccif_ctrl->md_rgu_base = ioremap_nocache(ccif_ctrl->hw_info->md_rgu_base, 0x40);

		return 0; 
	}

	return -1;
}

//----------------------------------------------------------------
// At MT6752
//    resv_api_0, used to get md_global_con0 bit MD_GLOBAL_CON0_CLDMA_BIT 1->0
//    resv_api_1, used for bus debug 
//    resv_api_2, used for emi bus read before idle
//    resv_api_3, used for emi bus read end idle
//    resv_api_4,
//    resv_api_5,
//    resv_api_6,
//----------------------------------------------------------------

int plat_misc_api(void* data, unsigned int api_id, void* output)
{
	switch(api_id) {
	case resv_api_0:
		if(!(ccci_read32(((struct md_cd_ctrl*)data)->md_global_con0, 0) & (1<<MD_GLOBAL_CON0_CLDMA_BIT)))
			(*(unsigned int*)output) = 1;

		return 0;

	case resv_api_1:
		CCCI_INF_MSG((int)data, TAG, "Dump MD Bus status %x\n", MD_BUS_STATUS_BASE);
		ccci_mem_dump((int)data, cldma_dbg_reg.md_bus_status, MD_BUS_STATUS_LENGTH);
		CCCI_INF_MSG((int)data, TAG, "Dump MD PC monitor %x\n", MD_PC_MONITOR_BASE);
		ccci_write32(cldma_dbg_reg.md_pc_monitor, 0, 0x80000000); // stop MD PCMon
		ccci_mem_dump((int)data, cldma_dbg_reg.md_pc_monitor, MD_PC_MONITOR_LENGTH);
		CCCI_INF_MSG((int)data, TAG, "Dump MD TOPSM status %x\n", MD_TOPSM_STATUS_BASE);
		ccci_mem_dump((int)data, cldma_dbg_reg.md_topsm_status, MD_TOPSM_STATUS_LENGTH);
		CCCI_INF_MSG((int)data, TAG, "Dump MD OST status %x\n", MD_OST_STATUS_BASE);
		ccci_mem_dump((int)data, cldma_dbg_reg.md_ost_status, MD_OST_STATUS_LENGTH);
		break;

	case resv_api_2:
		// check EMI
		/*
		while((readl(IOMEM(EMI_IDLE_STATE_REG)) & EMI_MD_IDLE_MASK) != EMI_MD_IDLE_MASK) {
			CCCI_ERR_MSG((int)data, TAG, "wrong EMI idle state 0x%X, count=%d\n", readl(IOMEM(EMI_IDLE_STATE_REG)), count);
			msleep(1000);
			count++;
		}
		CCCI_INF_MSG((int)data, TAG, "before MD off, EMI idle state 0x%X\n", readl(IOMEM(EMI_IDLE_STATE_REG)));
		*/
		return 0;

	case resv_api_3:
		// check EMI
		/*
		CCCI_INF_MSG((int)data, TAG, "after MD off, EMI idle state 0x%X\n", readl(IOMEM(EMI_IDLE_STATE_REG)));
		if((readl(IOMEM(EMI_IDLE_STATE_REG)) & EMI_MD_IDLE_MASK) != EMI_MD_IDLE_MASK)
			CCCI_ERR_MSG((int)data, TAG, "wrong EMI idle state 0x%X\n", readl(IOMEM(EMI_IDLE_STATE_REG)));
		*/
		return 0;

	default:
		return -1;
	}

	return -2;
}

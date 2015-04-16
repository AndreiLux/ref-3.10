#include <linux/kernel.h>
#include <linux/io.h>

#include "hifi_loader.h"
#include "hifi_lpp.h"

static unsigned int* ddr_viraddr = NULL;
static unsigned int* tcm_viraddr = NULL;
static unsigned int* ocram_viraddr = NULL;

static void drv_hifi_phy2virt(unsigned int *sec_addr)
{

	pr_info("before*********0x%x \n", (unsigned int)*sec_addr);
	if (*sec_addr >= HIFI_DDR_PHY_BEGIN_ADDR && *sec_addr <= HIFI_DDR_PHY_END_ADDR) {
		*sec_addr -= HIFI_DDR_PHY_BEGIN_ADDR - (unsigned int)ddr_viraddr;
	} else if(*sec_addr >= HIFI_OCRAM_PHY_BEGIN_ADDR && *sec_addr <= HIFI_OCRAM_PHY_END_ADDR) {
		*sec_addr -= HIFI_OCRAM_PHY_BEGIN_ADDR - (unsigned int)ocram_viraddr;
	} else if(*sec_addr >= HIFI_TCM_PHY_BEGIN_ADDR && *sec_addr <= HIFI_TCM_PHY_END_ADDR) {
		*sec_addr -= HIFI_TCM_PHY_BEGIN_ADDR - (unsigned int)tcm_viraddr;
	}
	pr_info("after********0x%x \n", (unsigned int)*sec_addr);
}

static void drv_hifi_init_mem(unsigned int share_addr_base)
{
	struct drv_hifi_sec_load_info *sec_info;

	sec_info = (struct drv_hifi_sec_load_info*)share_addr_base;

	/* add magic number 0x55AA55AA start share memory section*/
	sec_info->sec_magic = MEM_BEGIN_CHECK32_DATA;
	/* add magic number 0xAA55AA55 end of share memory section*/
	*(unsigned int*)((char*)share_addr_base + HIFI_SHARE_ADDR_LENGTH - sizeof(int))
		= MEM_END_CHECK32_DATA;

}

static int drv_hifi_check_sections(struct drv_hifi_image_head *img_head,
								   struct drv_hifi_image_sec *img_sec)
{

	if ((img_sec->sn >= img_head->sections_num)
		|| (img_sec->src_offset + img_sec->size > img_head->image_size)
		|| (img_sec->type >= (unsigned char)DRV_HIFI_IMAGE_SEC_TYPE_BUTT)
		|| (img_sec->load_attib >= (unsigned char)DRV_HIFI_IMAGE_SEC_LOAD_BUTT)) {
		return -1;
	}

	return 0;
}

int drv_hifi_load_sec(u32 hifi_base_virt)
{
	struct drv_hifi_sec_load_info *sec_info;
	struct drv_hifi_image_head	  *head;
	struct drv_hifi_sec_addr	  *dynamic_sec;

	unsigned int  dynamic_sec_num = 0;
	unsigned long sec_data_num	  = 0;
	u32 img_buf = 0;
	u32 share_mem = 0;
	unsigned int i = 0;
	int ret;

	img_buf = hifi_base_virt + HIFI_OFFSET_IMG;
	share_mem = hifi_base_virt + HIFI_OFFSET_MUSIC_DATA + 0x100000;
	pr_info("share_mem viraddr = 0x%x phy = 0x%x\n", img_buf ,virt_to_phys((void *)img_buf));

	tcm_viraddr = (u32*)ioremap(HIFI_TCM_PHY_BEGIN_ADDR, HIFI_TCM_SIZE);
	ocram_viraddr = (u32*)ioremap(HIFI_OCRAM_PHY_BEGIN_ADDR, HIFI_OCRAM_SIZE);
	ddr_viraddr = (unsigned int*)((void*)hifi_base_virt + HIFI_OFFSET_RUN);

	head = (struct drv_hifi_image_head *)img_buf;

	pr_info("sections_num = 0x%x, image_size = 0x%x\n",\
			head->sections_num,head->image_size);

	/* initial share memery */
	drv_hifi_init_mem(share_mem);

	sec_info = (struct drv_hifi_sec_load_info*)share_mem;
	for (i = 0; i < head->sections_num; i++) {
		pr_info("head->sections_num = 0x%x, i = 0x%x\n",head->sections_num,i);
		pr_info("des_addr = 0x%x, load_attib = %d, size = %d, sn = %d, src_offset = %d, type = %d\n", \
				head->sections[i].des_addr,\
				head->sections[i].load_attib,\
				head->sections[i].size,\
				head->sections[i].sn,\
				head->sections[i].src_offset,\
				head->sections[i].type);

		/* check the sections */
		ret = drv_hifi_check_sections(head, &(head->sections[i]));
		if (ret != 0) {
			pr_info("Invalid hifi section.\n");
			return -1;
		}

		drv_hifi_phy2virt((unsigned int*)(&(head->sections[i].des_addr)));

		if ((unsigned char)DRV_HIFI_IMAGE_SEC_LOAD_STATIC == head->sections[i].load_attib) {
			pr_info("**************DRV_HIFI_IMAGE_SEC_LOAD_STATIC *******\n");

			/* copy the sections */
			memcpy((void*)(head->sections[i].des_addr),
				   (void*)((char*)head + head->sections[i].src_offset),
				   head->sections[i].size);

		} else if ((unsigned char)DRV_HIFI_IMAGE_SEC_LOAD_DYNAMIC == head->sections[i].load_attib) {
			pr_info("**************DRV_HIFI_IMAGE_SEC_LOAD_DYNAMIC *******\n");
			dynamic_sec = &(sec_info->sec_addr_info[dynamic_sec_num]);

			/* copy the sections */
			memcpy((void*)(head->sections[i].des_addr),
				   (void*)((char*)head + head->sections[i].src_offset),
				   head->sections[i].size);

			/* 填充段信息头 */
			dynamic_sec->sec_source_addr
				= (unsigned int)(&(sec_info->sec_data[sec_data_num]));
			dynamic_sec->sec_length    = head->sections[i].size;
			dynamic_sec->sec_dest_addr = head->sections[i].des_addr;

			/* 拷贝段数据到共享内存 */
			memcpy((void*)(dynamic_sec->sec_source_addr),
				   (void*)((char*)head + head->sections[i].src_offset),
				   head->sections[i].size);

			/* 更新段数据地址 */
			sec_data_num += head->sections[i].size;

			/* 更新非单次加载段的个数 */
			dynamic_sec_num++;

		} else if ((unsigned char)DRV_HIFI_IMAGE_SEC_UNLOAD == head->sections[i].load_attib) {

			pr_info("**************DRV_HIFI_IMAGE_SEC_UNLOAD *******\n");
		} else {
			/*just for pclint*/
		}

	}

	/* 填充段信息头，非单次加载段的个数 */
	sec_info->sec_num = dynamic_sec_num;
	return 0;
}

int drv_hifi_load_dynamic_sec(u32 hifi_base_virt)
{

	tcm_viraddr = (u32*)ioremap(HIFI_TCM_PHY_BEGIN_ADDR, HIFI_TCM_SIZE);
	//ocram_viraddr = (u32*)ioremap(HIFI_OCRAM_PHY_BEGIN_ADDR, HIFI_OCRAM_SIZE);

	if(tcm_viraddr == NULL /*||  ocram_viraddr == NULL*/){
		pr_err("hifi download fail ,remap tcm or onchipram error!\n");
		return -1;
	}

	memcpy((u32 *)(tcm_viraddr),(u32*)(hifi_base_virt + HIFI_TCMBAK_OFFSET),HIFI_TCM_SIZE);
	//memcpy((u32 *)(ocram_viraddr),(u32*)(hifi_base_virt + HIFI_OCRAMBAK_OFFSET),HIFI_OCRAM_SIZE);

	return 0;
}

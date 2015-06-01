#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <mach/mt_spm_sleep.h>
#include "ccci_core.h"
#include "ccci_platform.h"
#include "ccif_platform.h"
#include "modem_ccif.h"
#include "modem_reg_base.h"
#include <mach/mt_clkmgr.h>
#define TAG "cif"

int md_ccif_let_md_go(struct ccci_modem *md)
{
    struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;

	if(MD_IN_DEBUG(md))
    {
        CCCI_INF_MSG(md->index, TAG, "DBG_FLAG_JTAG is set\n");
        return -1;
    }
    CCCI_INF_MSG(md->index, TAG, "md_ccif_let_md_go\n"); 
    // set the start address to let modem to run
    ccif_write32(md_ctrl->md_boot_slave_Key, 0, MD2_BOOT_VECTOR_KEY_VALUE); // make boot vector programmable
    ccif_write32(md_ctrl->md_boot_slave_Vector, 0, MD2_BOOT_VECTOR_VALUE); // after remap, MD ROM address is 0 from MD's view, MT6595 uses Thumb code
    ccif_write32(md_ctrl->md_boot_slave_En, 0, MD2_BOOT_VECTOR_EN_VALUE); // make boot vector take effect
    return 0;
}


static unsigned int md2_hw_mode = 0;

int md_ccif_power_on(struct ccci_modem *md)
{
    int ret = 0;
    struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;
    switch(md->index)
    {
        case MD_SYS2:
        ret = md_power_on(SYS_MD2);
        break;
    }    
    CCCI_INF_MSG(md->index, TAG, "md_ccif_power_on:ret=%d\n",ret); 
    if(ret==0){
        // disable MD WDT
        ccif_write32(md_ctrl->md_rgu_base, WDT_MD_MODE, WDT_MD_MODE_KEY);
    }
    return ret;
}
int md_ccif_power_off(struct ccci_modem *md, unsigned int timeout)
{
    int ret = 0;
    switch(md->index)
    {
        case MD_SYS2:
        ret = md_power_off(SYS_MD2, timeout);
        break;
    }     
    CCCI_INF_MSG(md->index, TAG, "md_ccif_power_off:ret=%d\n",ret); 
    return ret;
}



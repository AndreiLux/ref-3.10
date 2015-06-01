#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <mach/mt_spm_sleep.h>
#include <mach/mt_gpio.h>
#include <mach/mt_clkbuf_ctl.h>
#include <mach/mt_clkmgr.h>

#include <mach/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>

#include "ccci_core.h"
#include "ccci_platform.h"
#include "modem_cldma.h"
#include "cldma_platform.h"
#include "cldma_reg.h"
#include "modem_reg_base.h"

extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);

extern unsigned long infra_ao_base;

#define TAG "mcd"

int md_cd_power_on(struct ccci_modem *md)
{
    int ret = 0;
    unsigned int reg_value;
    struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
    // turn on VLTE
#ifdef FEATURE_VLTE_SUPPORT
    if(!(mt6325_upmu_get_swcid()==PMIC6325_E1_CID_CODE ||
         mt6325_upmu_get_swcid()==PMIC6325_E2_CID_CODE))
    {
        CCCI_INF_MSG(md->index, CORE, "md_cd_power_on:set VLTE on,bit0,1\n");
        pmic_config_interface(0x0638, 0x1, 0x1, 0); //bit:0 =>1'b1
        udelay(200);
        //reg_value = *((volatile unsigned int*)0x10001F00);
        reg_value = ccci_read32(infra_ao_base,0xF00);
        reg_value &= ~(0x10000); //bit:16 =>1'b0
        //*((volatile unsigned int*)0x10001F00) = reg_value;
        ccci_write32(infra_ao_base,0xF00,reg_value);
        CCCI_INF_MSG(md->index, CORE, "md_cd_power_on: set infra_misc VLTE bit(0x1000_1F00)=0x%x, bit(16)=0x%x\n",ccci_read32(infra_ao_base,0xF00),(ccci_read32(infra_ao_base,0xF00)&0x10000));
    }
#endif
    //config RFICx as BSI
    mutex_lock(&clk_buf_ctrl_lock); // fixme,clkbuf, ->down(&clk_buf_ctrl_lock_2);
    CCCI_INF_MSG(md->index, TAG, "clock buffer, BSI mode ignore\n"); 

    mt_set_gpio_mode(GPIO_RFIC0_BSI_CK,  GPIO_MODE_01); 
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D0,  GPIO_MODE_01);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D1,  GPIO_MODE_01);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D2,  GPIO_MODE_01);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_CS,  GPIO_MODE_01);
	// power on MD_INFRA and MODEM_TOP
    switch(md->index)
    {
        case MD_SYS1:
       	CCCI_INF_MSG(md->index, TAG, "Call start md_power_on()\n"); 
        ret = md_power_on(SYS_MD1);
        CCCI_INF_MSG(md->index, TAG, "Call end md_power_on() ret=%d\n",ret); 
        break;
    } 
	mutex_unlock(&clk_buf_ctrl_lock); // fixme,clkbuf, ->delete
	if(ret)
		return ret;
	// disable MD WDT
	cldma_write32(md_ctrl->md_rgu_base, WDT_MD_MODE, WDT_MD_MODE_KEY);
	return 0;
}

int md_cd_bootup_cleanup(struct ccci_modem *md, int success)
{
	int ret = 0;
#if 0    
	// config RFICx as GPIO
	if(success) {
		CCCI_INF_MSG(md->index, TAG, "clock buffer, GPIO mode\n"); 
	    mt_set_gpio_mode(GPIO_RFIC0_BSI_CK,  GPIO_MODE_GPIO); 
	    mt_set_gpio_mode(GPIO_RFIC0_BSI_D0,  GPIO_MODE_GPIO);
	    mt_set_gpio_mode(GPIO_RFIC0_BSI_D1,  GPIO_MODE_GPIO);
	    mt_set_gpio_mode(GPIO_RFIC0_BSI_D2,  GPIO_MODE_GPIO);
	    mt_set_gpio_mode(GPIO_RFIC0_BSI_CS,  GPIO_MODE_GPIO);
	} else {
		CCCI_INF_MSG(md->index, TAG, "clock buffer, unlock when bootup fail\n"); 
	}
	up(&clk_buf_ctrl_lock_2); //mutex_unlock(&clk_buf_ctrl_lock);
#endif
	return ret;
}

int md_cd_let_md_go(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	if(MD_IN_DEBUG(md))
		return -1;
	CCCI_INF_MSG(md->index, TAG, "set MD boot slave\n"); 
	// set the start address to let modem to run
	cldma_write32(md_ctrl->md_boot_slave_Key, 0, 0x3567C766); // make boot vector programmable
	cldma_write32(md_ctrl->md_boot_slave_Vector, 0, 0x00000000); // after remap, MD ROM address is 0 from MD's view, MT6595 uses Thumb code
	cldma_write32(md_ctrl->md_boot_slave_En, 0, 0xA3B66175); // make boot vector take effect
	return 0;
}

int md_cd_power_off(struct ccci_modem *md, unsigned int timeout)
{
    int ret = 0;
    unsigned int reg_value;
    mutex_lock(&clk_buf_ctrl_lock); // fixme,clkbuf, ->down(&clk_buf_ctrl_lock_2);
    // power off MD_INFRA and MODEM_TOP
    switch(md->index)
    {
        case MD_SYS1:
        ret = md_power_off(SYS_MD1, timeout);
        break;
    }
    // config RFICx as GPIO
    CCCI_INF_MSG(md->index, TAG, "clock buffer, GPIO mode\n"); 
    mt_set_gpio_mode(GPIO_RFIC0_BSI_CK,  GPIO_MODE_GPIO); 
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D0,  GPIO_MODE_GPIO);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D1,  GPIO_MODE_GPIO);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D2,  GPIO_MODE_GPIO);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_CS,  GPIO_MODE_GPIO);
	mutex_unlock(&clk_buf_ctrl_lock); // fixme,clkbuf, ->up(&clk_buf_ctrl_lock_2);
	
#ifdef FEATURE_VLTE_SUPPORT
    // Turn off VLTE
    if(!(mt6325_upmu_get_swcid()==PMIC6325_E1_CID_CODE ||
         mt6325_upmu_get_swcid()==PMIC6325_E2_CID_CODE))
    {
        //reg_value = *((volatile unsigned int*)0x10001F00);
        reg_value = ccci_read32(infra_ao_base,0xF00);
        reg_value &= ~(0x10000); //bit:16 =>1'b0
        reg_value |= 0x10000;//bit:16 =>1'b1
        //*((volatile unsigned int*)0x10001F00) = reg_value;
        ccci_write32(infra_ao_base,0xF00,reg_value);
        CCCI_INF_MSG(md->index, CORE, "md_cd_power_off: set SRCLKEN infra_misc(0x1000_1F00)=0x%x, bit(16)=0x%x\n",ccci_read32(infra_ao_base,0xF00),(ccci_read32(infra_ao_base,0xF00)&0x10000));

        CCCI_INF_MSG(md->index, CORE, "md_cd_power_off:set VLTE on,bit0=0\n");
        pmic_config_interface(0x0638, 0x0, 0x1, 0); //bit:0 =>1'b0
    }
#endif
    return ret;
}

void md_cd_lock_cldma_clock_src(int locked)
{
	spm_ap_mdsrc_req(locked);
}

int ccci_modem_remove(struct platform_device *dev)
{
	return 0;
}

void ccci_modem_shutdown(struct platform_device *dev)
{
}

int ccci_modem_suspend(struct platform_device *dev, pm_message_t state)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	CCCI_INF_MSG(md->index, TAG, "AP_BUSY(%x)=%x\n", md_ctrl->ap_ccif_base+APCCIF_BUSY, cldma_read32(md_ctrl->ap_ccif_base, APCCIF_BUSY));
	CCCI_INF_MSG(md->index, TAG, "MD_BUSY(%x)=%x\n", md_ctrl->ap_ccif_base+0x1000+APCCIF_BUSY, cldma_read32(md_ctrl->ap_ccif_base+0x1000, APCCIF_BUSY));

	return 0;
}

int ccci_modem_resume(struct platform_device *dev)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	cldma_write32(md_ctrl->ap_ccif_base, APCCIF_CON, 0x01); // arbitration
	return 0;
}

int ccci_modem_pm_suspend(struct device *device)
{
    struct platform_device *pdev = to_platform_device(device);
    BUG_ON(pdev == NULL);

    return ccci_modem_suspend(pdev, PMSG_SUSPEND);
}

int ccci_modem_pm_resume(struct device *device)
{
    struct platform_device *pdev = to_platform_device(device);
    BUG_ON(pdev == NULL);

    return ccci_modem_resume(pdev);
}

int ccci_modem_pm_restore_noirq(struct device *device)
{
	struct ccci_modem *md = (struct ccci_modem *)device->platform_data;
    struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	// IPO-H
    // restore IRQ
#ifdef FEATURE_PM_IPO_H
    mt_irq_set_sens(md_ctrl->cldma_irq_id, MT_LEVEL_SENSITIVE);
    mt_irq_set_polarity(md_ctrl->cldma_irq_id, MT_POLARITY_HIGH);
    mt_irq_set_sens(md_ctrl->md_wdt_irq_id, MT_EDGE_SENSITIVE);
    mt_irq_set_polarity(md_ctrl->md_wdt_irq_id, MT_POLARITY_LOW);
#endif
	// set flag for next md_start
	md->config.setting |= MD_SETTING_RELOAD;
	md->config.setting |= MD_SETTING_FIRST_BOOT;
    return 0;
}


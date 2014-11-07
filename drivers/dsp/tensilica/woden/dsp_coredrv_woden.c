#include <linux/kernel.h>
#include <linux/interrupt.h>    /**< For isr */
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <asm/io.h>
#include <linux/slab.h>

#include "../mailbox/mailbox_hal.h"
#include "../dsp_cfg.h"
#include "dsp_coredrv_woden.h"

#define WODEN_REMAP_VALUE	0x4F	//0x9E00_0000

#define CHECK_DSP_DEBUG 1

AUD_CONTROL_WODEN *aud_control_woden;

int dsp_woden_register_device(struct platform_device *pdev);
void dsp_woden_unregister_device(void);
int dsp_woden_download_dram(void *firmwareBuffer);
int dsp_woden_download_iram(void *firmwareBuffer);
int dsp_woden_power_on(void);
void dsp_woden_power_off(void);

unsigned int kthread_mailbox_flag = 0;

DSP_MODE *dsp_mode;

DSP_WAIT *dsp_wait;

DSP_WODEN_MEM_CTL *dsp_woden_mem_ctl;

WORKQUEUE_FLAG workqueue_flag;

unsigned int dsp_irq;

unsigned int received_transaction_number;

int 
dsp_woden_register_device(struct platform_device *pdev)
{
	unsigned int ret= 0;
	struct resource *res; 
	//unsigned int irq;

	dsp_woden_mem_ctl = kmalloc(sizeof(DSP_WODEN_MEM_CTL), GFP_KERNEL);

	dsp_wait = kmalloc(sizeof(DSP_WAIT),GFP_KERNEL);
	if(dsp_wait == NULL)
	{
		printk("Can't allocate kernel memory\n");
		return -ENOMEM;
	}
	init_waitqueue_head(&(dsp_wait->wait));
	spin_lock_init(&(dsp_wait->dsp_lock));


	dsp_mode = kmalloc(sizeof(DSP_MODE), GFP_KERNEL);
	if(dsp_mode == NULL)
	{
		printk("Can't allocate kernel memory\n");
		return -ENOMEM;
	}

	memset(dsp_mode, 0, sizeof(DSP_MODE));

	res = kmalloc(sizeof(struct resource), GFP_KERNEL);

	if(of_address_to_resource(pdev->dev.of_node, WODEN_DSP_IOMEM_AUD_CTR, res))
	{
		dev_err(&pdev->dev, "No memory resource of WODEN_DSP_IOMEM_AUD_CTR\n");
		ret=-EINVAL;
	}
	
	aud_control_woden = (AUD_CONTROL_WODEN *)of_iomap(pdev->dev.of_node, WODEN_DSP_IOMEM_AUD_CTR);
	dsp_woden_mem_ctl->dsp_control_register_size = resource_size(res);
	dsp_woden_mem_ctl->dsp_control_register = (unsigned int *)aud_control_woden;
	
	if(of_address_to_resource(pdev->dev.of_node, WODEN_DSP_IOMEM_AUD_DRAM0, res))
	{
		dev_err(&pdev->dev, "No memory resource of WODEN_DSP_IOMEM_AUD_DRAM0\n");
		ret=-EINVAL;
	}
	dsp_woden_mem_ctl->dsp_dram0_address = (unsigned int *)of_iomap(pdev->dev.of_node, WODEN_DSP_IOMEM_AUD_DRAM0);
	dsp_woden_mem_ctl->dsp_dram0_size = resource_size(res);

	if(of_address_to_resource(pdev->dev.of_node, WODEN_DSP_IOMEM_AUD_DRAM1, res))
	{
		dev_err(&pdev->dev, "No memory resource of WODEN_DSP_IOMEM_AUD_DRAM1\n");
		ret=-EINVAL;
	}
	dsp_woden_mem_ctl->dsp_dram1_address = (unsigned int *)of_iomap(pdev->dev.of_node, WODEN_DSP_IOMEM_AUD_DRAM1);
	dsp_woden_mem_ctl->dsp_dram1_size = resource_size(res);

	if(of_address_to_resource(pdev->dev.of_node, WODEN_DSP_IOMEM_AUD_IRAM0, res))
	{
		dev_err(&pdev->dev, "No memory resource of WODEN_DSP_IOMEM_AUD_IRAM0\n");
		ret=-EINVAL;
	}
	dsp_woden_mem_ctl->dsp_iram0_address = (unsigned int *)of_iomap(pdev->dev.of_node, WODEN_DSP_IOMEM_AUD_IRAM0);
	dsp_woden_mem_ctl->dsp_iram0_size = resource_size(res);

	if(of_address_to_resource(pdev->dev.of_node, WODEN_DSP_IOMEM_AUD_IRAM1, res))
	{
		dev_err(&pdev->dev, "No memory resource of WODEN_DSP_IOMEM_AUD_IRAM1\n");
		ret=-EINVAL;
	}
	dsp_woden_mem_ctl->dsp_iram1_address = (unsigned int *)of_iomap(pdev->dev.of_node, WODEN_DSP_IOMEM_AUD_IRAM1);
	dsp_woden_mem_ctl->dsp_iram1_size = resource_size(res);

	if(of_address_to_resource(pdev->dev.of_node, WODEN_DSP_IOMEM_AUD_MAILBOX, res))
	{
		dev_err(&pdev->dev, "No memory resource of WODEN_DSP_IOMEM_AUD_MAILBOX\n");
		ret=-EINVAL;
	}
	
	pst_audio_mailbox = (AUD_MAILBOX *)of_iomap(pdev->dev.of_node, WODEN_DSP_IOMEM_AUD_MAILBOX);

	if(of_address_to_resource(pdev->dev.of_node, WODEN_DSP_IOMEM_AUD_INTERRUPT, res))
	{
		dev_err(&pdev->dev, "No memory resource of WODEN_DSP_IOMEM_AUD_INTERRUPT\n");
		ret=-EINVAL;
	}

	pst_audio_mailbox_interrupt = (AUD_MAILBOX_INTERRUPT *)of_iomap(pdev->dev.of_node, WODEN_DSP_IOMEM_AUD_INTERRUPT);

	dsp_irq = irq_of_parse_and_map(pdev->dev.of_node, 0);

	if(request_irq(dsp_irq, dsp_woden_interrupt,IRQF_SHARED,DSP_MODULE,pst_audio_mailbox))
	{
		printk("unable to allocate irq\n");
	}

	kfree(res);

	return 0;
}
 
void
dsp_woden_unregister_device(void)
{
	kfree(dsp_wait);

	free_irq(dsp_irq,pst_audio_mailbox); 
	
	kfree(dsp_woden_mem_ctl);
}

int 
dsp_woden_download_dram(void *firmwareBuffer)
{
	int ret = 1;
	DSP_BUFFER *buffer;
	unsigned int ADSP_FW_data_size = 0;
	
	buffer = (DSP_BUFFER *)firmwareBuffer;

#if CHECK_DSP_DEBUG
	printk("DSP_Woden_DownloadDataFirmware call\n");
#endif

	aud_control_woden->dsp_remap.dsp_remap = WODEN_REMAP_VALUE;	
	memset(&aud_control_woden->dsp_control, 0x0, sizeof(DSP_CONTROL));

	// Audio DSP DRAM Mux Enable
	aud_control_woden->dram_mem_ctl.dram_mux_control = 0xFF;

	//Download Data
	ADSP_FW_data_size = buffer->buffer_size;

	if(ADSP_FW_data_size > (dsp_woden_mem_ctl->dsp_dram0_size + dsp_woden_mem_ctl->dsp_dram1_size))
	{
		printk(KERN_ERR "overflow size of dsp data-ram binary : %s:%i \n", __FILE__,__LINE__);
		return -1;
	}
	if(ADSP_FW_data_size > dsp_woden_mem_ctl->dsp_dram1_size)
	{
		memcpy((unsigned char *)(dsp_woden_mem_ctl->dsp_dram1_address), (unsigned char *)buffer->buffer_virtual_address,dsp_woden_mem_ctl->dsp_dram1_size);
		memcpy((unsigned char *)(dsp_woden_mem_ctl->dsp_dram0_address), (unsigned char *)(buffer->buffer_virtual_address+dsp_woden_mem_ctl->dsp_dram1_size), ADSP_FW_data_size-dsp_woden_mem_ctl->dsp_dram1_size);
	}
	else
	{
		memcpy((unsigned char *)(dsp_woden_mem_ctl->dsp_dram0_address), (unsigned char *)buffer->buffer_virtual_address,ADSP_FW_data_size);
	}
#if CHECK_DSP_DEBUG
	printk("[%s] Data Download Complete\n",DSP_MODULE);
#endif

	// Audio DSP DRAM Mux Disable
	aud_control_woden->dram_mem_ctl.dram_mux_control = 0x0;
	
	return ret;
}

int 
dsp_woden_download_iram(void *firmwareBuffer)
{
	int ret = 1;
	DSP_BUFFER *buffer;
	unsigned int ADSP_FW_inst_size = 0;
 
	buffer = (DSP_BUFFER *)firmwareBuffer;
	
	// Audio DSP IRAM Mux Enable
	aud_control_woden->iram_mem_ctl.iram_mux_control = 0x1;

	//Download Instruction
	ADSP_FW_inst_size  = buffer->buffer_size;

#if CHECK_DSP_DEBUG
	printk("ADSP_FW_inst_size = %d\n",ADSP_FW_inst_size);
#endif
	if(ADSP_FW_inst_size > dsp_woden_mem_ctl->dsp_iram0_size)
	{
		memcpy((unsigned char *)(dsp_woden_mem_ctl->dsp_iram0_address),(unsigned char *)buffer->buffer_virtual_address, dsp_woden_mem_ctl->dsp_iram0_size);
		memcpy((unsigned char *)(dsp_woden_mem_ctl->dsp_iram1_address),(unsigned char *)(buffer->buffer_virtual_address+dsp_woden_mem_ctl->dsp_iram0_size),ADSP_FW_inst_size-dsp_woden_mem_ctl->dsp_iram0_size);
	}
	else
	{
		memcpy((unsigned char *)(dsp_woden_mem_ctl->dsp_iram0_address),(unsigned char *)buffer->buffer_virtual_address,ADSP_FW_inst_size);
	}
#if CHECK_DSP_DEBUG
	printk("[%s] Instruction Download Complete\n",DSP_MODULE);
#endif

	// Audio DSP IRAM Mux Disable
	aud_control_woden->iram_mem_ctl.iram_mux_control = 0x0;

	return ret;
}

int 
dsp_woden_power_on(void)
{
	unsigned int tmp;
	int i=0;

	aud_control_woden->dsp_control.dsp_reset = 0x1;

	if(dsp_mode->is_opened_lpa)
	{
		for(i=0;i<10;i++)
		{
			tmp = aud_control_woden->dsp_temp_reg3.temp_reg3;
			if(tmp == 0xFFFB0001)
				break;
			msleep(100);                                                          	
		}                                               
	}
	else if(dsp_mode->is_opened_alsa)
	{
		for(i=0;i<10;i++)
		{
			tmp = aud_control_woden->dsp_temp_reg3.temp_reg3;
			if(tmp == 0xABCD0000)
				break;
			msleep(100);                                                          	
		}
	}
	else if(dsp_mode->is_opened_omx)
	{
		for(i=0;i<10;i++)
		{                                                                        
			tmp = aud_control_woden->dsp_status1.dsp_status1;
			if(tmp == 0x53545544)
				break;
			msleep(100);                                                          	
		}
	}
	if(i==10)
	{
		return -1;
	}
	return 0;
}

void 
dsp_woden_power_off(void)
{
	aud_control_woden->dsp_control.dsp_reset = 0x0;
	aud_control_woden->dsp_control.ocd_halt_on_reset = 0x1;
}

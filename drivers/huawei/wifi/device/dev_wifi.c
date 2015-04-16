/*
 * Copyright (c) 2012-2013 Huawei Ltd.
 *
 * Author: 
 *
 * This	program	is free	software; you can redistribute it and/or modify
 * it under	the	terms of the GNU General Public	License	version	2 as
 * published by	the	Free Software Foundation.
 */

/* dev_wifi.c
 */

/*=========================================================================
 *
 * histoty
 *
 *=========================================================================
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/err.h>
#include <linux/random.h>
#include <linux/skbuff.h>
#include <generated/mach-types.h>
#include <linux/wifi_tiwlan.h>
#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/ctype.h>
#include <linux/export.h>
#include <linux/mmc/dw_mmc.h>

#include "dev_wifi.h"

#define DTS_COMP_WIFI_POWER_NAME "hisilicon,bcm_wifi"
#define  DEF_NVRAM "nvram4334_hw.txt"
#define  DEF_FIRMWARE "fw_bcm4334_hw.bin"
typedef struct wifi_mem_prealloc_struct {
	void *mem_ptr;
	unsigned long size;
} wifi_mem_prealloc_t;


struct wifi_host_s *wifi_host;
static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];
unsigned char g_wifimac[WLAN_MAC_LEN] = {0x00,0x00,0x00,0x00,0x00,0x00};
static wifi_mem_prealloc_t wifi_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

/*kmalloc memory for wifi*/
void *wifi_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_NUMBER_OF_SECTIONS)
		return wlan_static_skb;
	if ((section < 0) || (section > PREALLOC_WLAN_NUMBER_OF_SECTIONS)) {
		pr_err("%s: is error(section:%d).\n", __func__, section);
		return NULL;
	}
	if (wifi_mem_array[section].size < size) {
		pr_err("%s: is error(size:%lu).\n", __func__, size);
		return NULL;
	}
	return wifi_mem_array[section].mem_ptr;
}

/*init wifi buf*/
int init_wifi_mem(void)
{
	int i = 0;

	pr_info("init_wifi_mem.\n");
	for (i = 0; i < WLAN_SKB_BUF_NUM; i++) {
		if (i < (WLAN_SKB_BUF_NUM / 2))
			wlan_static_skb[i] = dev_alloc_skb(WLAN_SKB_BUF_MIN);
		else
			wlan_static_skb[i] = dev_alloc_skb(WLAN_SKB_BUF_MAX);
		if (wlan_static_skb[i] == NULL) {
			pr_err("%s: dev_alloc_skb is error(%d).\n", __func__, i);
			return -ENOMEM;
		}
	}

	for	(i = 0; i < PREALLOC_WLAN_NUMBER_OF_SECTIONS; i++) {
		wifi_mem_array[i].mem_ptr = kzalloc(wifi_mem_array[i].size,
			GFP_KERNEL);
		if (wifi_mem_array[i].mem_ptr == NULL) {
			pr_err("%s: alloc mem_ptr is error(%d).\n", __func__, i);
			return -ENOMEM;
		}
	}
	return 0;
}

/*deinit wifi buf*/
int deinit_wifi_mem(void)
{
	int i = 0;

	pr_info("deinit_wifi_mem.\n");
	for (i = 0; i < WLAN_SKB_BUF_NUM; i++) {
		if (wlan_static_skb[i] != NULL) {
			dev_kfree_skb(wlan_static_skb[i]);
			wlan_static_skb[i] = NULL;
		} else
			break;
	}
	for	(i = 0;	i < PREALLOC_WLAN_NUMBER_OF_SECTIONS; i++) {
		if (wifi_mem_array[i].mem_ptr != NULL) {
			kfree(wifi_mem_array[i].mem_ptr);
			wifi_mem_array[i].mem_ptr = NULL;
		} else
			break;
	}
	return 0;
}

static void read_from_global_buf(unsigned char * buf)
{
	memcpy(buf,g_wifimac,WLAN_MAC_LEN);
	printk("get MAC from g_wifimac: mac=%02x:%02x:%02x:%02x:%02x:%02x\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5]);
	return;
}

static int char2_byte( char* strori, char* outbuf )
{
	int i = 0;
	int temp = 0;
	int sum = 0;
	char *ptemp;
	char tempchar[20]={0};

	ptemp = tempchar;

	for (i = 0; i< WLAN_VALID_SIZE;i++){
		if(strori[i]!=':'){
			*ptemp = strori[i];
			 ptemp++;
		}
	}

	for( i = 0; i < NV_WLAN_VALID_SIZE; i++ ){

		switch (tempchar[i]) {
			case '0' ... '9':
				temp = tempchar[i] - '0';
				break;
			case 'a' ... 'f':
				temp = tempchar[i] - 'a' + 10;
				break;
			case 'A' ... 'F':
				temp = tempchar[i] - 'A' + 10;
				break;
			default:
				return 0;
		}
		sum += temp;
		if( i % 2 == 0 ){
			outbuf[i/2] |= temp << 4;
		}
		else{
			outbuf[i/2] |= temp;
		}
	}
	return sum;
}


static int read_from_mac_file(unsigned char * buf)
{
	struct file* filp = NULL;
	mm_segment_t old_fs;
	int result = 0;
	int sum = 0;
	char buf1[20] = {0};
	char buf2[6] = {0};

	if (NULL == buf) {
		pr_err("%s: buf is NULL\n", __func__);
		return -1;
	}

	filp = filp_open(MAC_ADDRESS_FILE, O_RDONLY,0);
	if(IS_ERR(filp)){
		pr_err("open mac address file error\n");
		return -1;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	filp->f_pos = 0;
	result = filp->f_op->read(filp,buf1,WLAN_VALID_SIZE,&filp->f_pos);
	if(WLAN_VALID_SIZE != result){
		pr_err("read mac address file error\n");
		set_fs(old_fs);
		filp_close(filp,NULL);
		return -1;
	}

	sum = char2_byte(buf1,buf2);
	if (0 != sum){
		pr_err("get MAC from file: mac=%02x:%02x:%02x:%02x:%02x:%02x\n",buf2[0],buf2[1],buf2[2],buf2[3],buf2[4],buf2[5]);
		memcpy(buf,buf2,WLAN_MAC_LEN);
	}else{
		set_fs(old_fs);
		filp_close(filp,NULL);
		return -1;
	}

	set_fs(old_fs);
	filp_close(filp,NULL);
	return 0;
}

int bcm_wifi_get_mac_addr(unsigned char *buf)
{
	int ret = -1;

	if (NULL == buf) {
		pr_err("%s: k3v2_wifi_get_mac_addr failed\n", __func__);
		return -1;
	}

	memset(buf, 0, WLAN_MAC_LEN);
	if (0 != g_wifimac[0] || 0 != g_wifimac[1] || 0 != g_wifimac[2] || 0 != g_wifimac[3]|| 0 != g_wifimac[4] || 0 != g_wifimac[5]){
		read_from_global_buf(buf);
		return 0;
	}

	ret = read_from_mac_file(buf);
	if(0 == ret){
		pr_err("%s:read from mac addr file success \n",__func__);
		memcpy(g_wifimac,buf,WLAN_MAC_LEN);
		return 0;
	}else{
		get_random_bytes(buf,WLAN_MAC_LEN);
		buf[0] = 0x0;
		pr_err("get MAC from Random: mac=%02x:%02x:%02x:%02x:%02x:%02x\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5]);
		memcpy(g_wifimac,buf,WLAN_MAC_LEN);
	}

	return 0;
}


int bcm_wifi_power(int on)
{
	int ret = 0;
	pr_info("%s: on:%d\n", __func__, on);
	if (wifi_host->chip_fpga) {
		if (NULL == wifi_host) {
			pr_err("%s: wifi_host is null\n", __func__);
			return -1;
		}
	}

	if (on) {

			if (wifi_host->bEnable) {
				pr_err("%s: wifi had power on.\n", __func__);
				return ret;
			}

			ret = clk_prepare_enable(wifi_host->clk);

			if (ret < 0) {
				pr_err("%s: clk_enable failed, ret:%d\n", __func__, ret);
				return ret;
			}
		gpio_set_value(wifi_host->enable, 0);
		msleep(100);
		gpio_set_value(wifi_host->enable, 1);
		msleep(110);
		wifi_host->bEnable = true;
		//hi_sdio_set_power(on); 
	} else {
		//hi_sdio_set_power(on);
		//dump_stack();
		gpio_set_value(wifi_host->enable, 0);
		msleep(20);
			clk_disable_unprepare(wifi_host->clk);
			wifi_host->bEnable = false;

	}
	
	return ret;
}

void bcm_wifi_get_nvram_fw_name(unsigned char *nvram_name, unsigned char *fw_name)
{
	if(NULL == nvram_name || NULL == fw_name)
	{
		pr_err("%s: invalid input parameter!\n", __FUNCTION__);
	}else{
		memcpy(nvram_name, wifi_host->nvram_name, NVRAM_NAME_LEN);
		memcpy(fw_name, wifi_host->fw_name, FIRMWARE_NAME_LEN);	
	}
}

static int bcm_wifi_reset(int on)
{
	pr_info("%s: on:%d.\n", __func__, on);
	if (on) {
		gpio_set_value(wifi_host->enable, 1);
	}
	else {
		gpio_set_value(wifi_host->enable, 0);
	}
	return 0;
}

struct dw_mci* sdio_host = NULL;

EXPORT_SYMBOL(sdio_host);

extern void dw_mci_sdio_card_detect(struct dw_mci *host);

int hi_sdio_detectcard_to_core(int val)
{
    if(NULL != sdio_host)
    {
        dw_mci_sdio_card_detect(sdio_host);
    }
    return 0;
}
extern int dhd_msg_level;

static ssize_t show_wifi_debug_level(struct device *dev,
        struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", dhd_msg_level);
}
static ssize_t restore_wifi_debug_level(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t size) {
    int value;
    if (sscanf(buf, "%d\n", &value) == 1) {
#ifdef WL_CFG80211
        pr_err("restore_wifi_debug_level\n");
		/* Enable DHD and WL logs in oneshot */
		if (value & DHD_WL_VAL2)
			wl_cfg80211_enable_trace(TRUE, value & (~DHD_WL_VAL2));
		else if (value & DHD_WL_VAL)
			wl_cfg80211_enable_trace(FALSE, WL_DBG_DBG);
		if (!(value & DHD_WL_VAL2))
#endif /* WL_CFG80211 */
        dhd_msg_level = value;
        return size;
    }
    return -1;
}

static DEVICE_ATTR(wifi_debug_level, S_IRUGO | S_IWUSR | S_IWGRP,
        show_wifi_debug_level, restore_wifi_debug_level);

static struct attribute *attr_debug_attributes[] = {
    &dev_attr_wifi_debug_level.attr,
    NULL
};

static const struct attribute_group attrgroup_debug_level = {
    .attrs = attr_debug_attributes,
};

static int wifi_open_err_code = 0;
static ssize_t show_wifi_open_state(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", wifi_open_err_code);
}
static ssize_t restore_wifi_open_state(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int value;
	if (sscanf(buf, "%d\n", &value) == 1) {
		wifi_open_err_code = value;
		return size;
	}
	return -1;
}

void set_wifi_open_err_code(int err_num)
{
    wifi_open_err_code = err_num;
}

static DEVICE_ATTR(wifi_open_state, S_IRUGO | S_IWUSR,
				   show_wifi_open_state, restore_wifi_open_state);

static struct attribute *wifi_state_attributes[] = {
	&dev_attr_wifi_open_state.attr,
	NULL
};
static const struct attribute_group wifi_state = {
	.attrs = wifi_state_attributes,
};

struct wifi_platform_data bcm_wifi_control = {
	.set_power = bcm_wifi_power,
	.set_reset = bcm_wifi_reset,
	.set_carddetect = hi_sdio_detectcard_to_core,
	.get_mac_addr = bcm_wifi_get_mac_addr,
	.mem_prealloc = wifi_mem_prealloc,
	.get_nvram_fw_name = bcm_wifi_get_nvram_fw_name,
};

static struct resource bcm_wifi_resources[] = {
	[0] = {
	.name  = "bcmdhd_wlan_irq",
	.flags = IORESOURCE_IRQ |IORESOURCE_IRQ_HIGHLEVEL
			| IRQF_NO_SUSPEND,
	},
};

static struct platform_device bcm_wifi_device = {
	.name = "bcmdhd_wlan",
	.id = 1,
	.num_resources = ARRAY_SIZE(bcm_wifi_resources),
	.resource = bcm_wifi_resources,
	.dev = {
		.platform_data = &bcm_wifi_control,
	},
};


int  wifi_power_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np;
	const char *nvram = NULL;
	const char *firmware = NULL;

	np = of_find_compatible_node(NULL, NULL, DTS_COMP_WIFI_POWER_NAME);	// should be the same as dts node compatible property
	if (np == NULL) {
		pr_err("Unable to find wifi_power\n");
		return -ENOENT;
	}

	ret = init_wifi_mem();
	if (ret) {
		pr_err("%s: init_wifi_mem failed.\n", __func__);
		goto err_malloc_wifi_host;
	}
	
	wifi_host = kzalloc(sizeof(struct wifi_host_s), GFP_KERNEL);
	if (!wifi_host) {
		pr_err("%s: malloc wifi_host failed.\n", __func__);
		ret = -ENOMEM;
		goto err_malloc_wifi_host;
	}
	wifi_host->bEnable = false;
       wifi_host->clk = devm_clk_get(&pdev->dev, "clk_pmu32kb");
	if (IS_ERR(wifi_host->clk)) {
		dev_err(&pdev->dev, "Get wifi 32k clk failed\n");
		ret = -1;
		goto err_clk_get;
       }
#if 0
	
	wifi_host->vdd = regulator_get(&pdev->dev,"wifiio-vcc");
	if (IS_ERR(wifi_host->vdd)) {
	    dev_err(&pdev->dev, "regulator_get failed\n");
           ret = -1;
	    goto err_regulator_get; 
	}

	wifi_host->pctrl = devm_pinctrl_get(&pdev->dev);
       if (!wifi_host->pctrl)
       {
           pr_err("%s: iomux_lookup_block failed.\n", __func__);
           ret = -1;
           goto err_pinctrl_get;
       }

	wifi_host->pins_normal = pinctrl_lookup_state(wifi_host->pctrl, "default");
       if (!wifi_host->pins_normal)
       {
           pr_err("%s: pinctrl_lookup_state default failed.\n", __func__);
           ret = -1;
           goto err_pinctrl_get;
       }

	wifi_host->pins_idle = pinctrl_lookup_state(wifi_host->pctrl, "idle");
       if (!wifi_host->pins_idle)
       {
           pr_err("%s: pinctrl_lookup_state idle failed.\n", __func__);
           ret = -1;
           goto err_pinctrl_get;
       }
#endif
	wifi_host->enable = of_get_named_gpio(pdev->dev.of_node, "wlan-on,gpio-enable", 0);
	if (wifi_host->enable<0) {
		ret = -1;
		goto err_gpio_get;
       }

	wifi_host->wifi_wakeup_irq= of_get_named_gpio(pdev->dev.of_node, "wlan-irq,gpio-irq", 0);
	if (wifi_host->wifi_wakeup_irq<0) {
		ret = -1;
		goto err_gpio_get;
       }
	
	ret = of_property_read_string(pdev->dev.of_node, "wifi_nvram_name", &nvram);
	if (ret < 0){
		pr_err("error get nvram name from dts, use default nvram\n");
		nvram = DEF_NVRAM;	
	}
	ret = of_property_read_string(pdev->dev.of_node, "wifi_fw_name", &firmware);
	if (ret < 0){
		pr_err("error get firmware name from dts, use default firmware\n");
		firmware = DEF_FIRMWARE;
	}
	pr_err("wifi_fw_name:%s, wifi_nvram_name:%s\n", firmware, nvram);
	strncpy(wifi_host->nvram_name, nvram, strlen(nvram));
	strncpy(wifi_host->fw_name, firmware, strlen(firmware));

	ret = gpio_request(wifi_host->enable, NULL);
	if (ret < 0) {
		pr_err("%s: gpio_request failed, ret:%d.\n", __func__,
			wifi_host->enable);
		goto err_enable_gpio_request;
	}
	gpio_direction_output(wifi_host->enable, 0);
	wifi_host->wifi_wakeup_irq = of_get_named_gpio(pdev->dev.of_node, "wlan-irq,gpio-irq", 0);
	/* set apwake gpio */
	ret = gpio_request(wifi_host->wifi_wakeup_irq, NULL);
	if (ret < 0) {
		pr_err("%s: gpio_request failed, ret:%d.\n", __func__,
			wifi_host->wifi_wakeup_irq);
		goto err_irq_gpio_request;
	}
	gpio_direction_input(wifi_host->wifi_wakeup_irq);
	bcm_wifi_resources[0].start = gpio_to_irq(wifi_host->wifi_wakeup_irq);
	bcm_wifi_resources[0].end = gpio_to_irq(wifi_host->wifi_wakeup_irq) ;
    
	ret = platform_device_register(&bcm_wifi_device);
	if (ret) {
		pr_err("%s: platform_device_register failed, ret:%d.\n",
			__func__, ret);
		goto err_platform_device_register;
	}
	ret = sysfs_create_group(&bcm_wifi_device.dev.kobj, &attrgroup_debug_level);
	if (ret) {
		pr_err("wifi_power_probe create debug level error ret =%d", ret);
	}
	ret = sysfs_create_group(&bcm_wifi_device.dev.kobj, &wifi_state);
	if (ret) {
		pr_err("wifi_power_probe sysfs_create_group error ret =%d", ret);
	}

	return 0;
err_platform_device_register:
	gpio_free(wifi_host->wifi_wakeup_irq);
err_irq_gpio_request:
	gpio_free(wifi_host->enable);
err_enable_gpio_request:
err_gpio_get:
#if 0
err_pinctrl_get:
err_regulator_get:
#endif
err_clk_get:
	kfree(wifi_host);
	wifi_host = NULL;
err_malloc_wifi_host:
	deinit_wifi_mem();
	return ret;
}



static struct of_device_id wifi_power_match_table[] = {
	{ .compatible = DTS_COMP_WIFI_POWER_NAME, 
		.data = NULL,
        	},
	{ },
};

static struct platform_driver wifi_power_driver = {
	.driver		= {
		.name		= "bcm4334_power",
		.owner = THIS_MODULE,
	       	.of_match_table	= wifi_power_match_table,
	},
	.probe          = wifi_power_probe,
	
	
};

static int __init wifi_power_init(void)
{
	int ret;
	ret = platform_driver_register(&wifi_power_driver);
	if (ret)
		pr_err("%s: platform_driver_register failed, ret:%d.\n",
			__func__, ret);
	return ret;
}

static void __exit wifi_power_exit(void)
{
	platform_driver_unregister(&wifi_power_driver);
}
device_initcall(wifi_power_init); 

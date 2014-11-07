/*
 * odin Memory Controller
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ratelimit.h>
#include <linux/platform_device.h>
#include <linux/platform_data/odin_tz.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/odin_pd.h>

#include <memory/odin-memc.h>

#define DRV_NAME "odin-mc"

static struct odin_mc *mc;
static unsigned int read_val = 0;

static ssize_t show_mc_reg_read(struct device *dev, struct device_attribute *attr,
                            char *buf)
{
	return sprintf(buf,"%x\n", read_val);
}

static ssize_t store_mc_reg_write(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
	/*TODO*/
	unsigned long write, ctrl, addr, val ;
    char *b = &buf[0];
	char *token;

	token = strsep(&b, " ");
	if (token != NULL)
		if (kstrtoul(token, 0 , &write)<0)
			return count;
	token = strsep(&b, " ");
	if (token != NULL)
		if (kstrtoul(token, 0 , &ctrl)<0)
			return count;
	token = strsep(&b, " ");
	if (token != NULL)
		if (kstrtoul(token, 0 , &addr)<0)
			return count;

	if(write == 1)
	{
		token = strsep(&b, " ");
		if (token != NULL)
			if (kstrtoul(token, 0 , &val)<0)
				return count;
		mc_writel(val, addr, ctrl);
	}else
		read_val = mc_readl(addr, ctrl);

	return count ;
}

static ssize_t show_auto_lpm(struct device *dev, struct device_attribute *attr,
                            char *buf)
{

	printk(KERN_INFO "0x%x    0x%x    0x%x\n",mc_readl(0x54,0),
										mc_readl(0x58,0),mc_readl(0x5c,0));
	printk(KERN_INFO "0x%x    0x%x    0x%x\n",mc_readl(0x54,1),
										mc_readl(0x58,1),mc_readl(0x5c,1));
	return 0;
}

static ssize_t store_auto_lpm(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
	unsigned long val;
	unsigned long clock_gate, pdown, selfref, self_gating ;
    char *b = &buf[0];
	char *token;

	/* TODO  : We need error check */

	token = strsep(&b, " ");
	if (token != NULL)
		if (kstrtoul(token, 0 , &clock_gate)<0)
			return count;
	token = strsep(&b, " ");
	if (token != NULL)
		if (kstrtoul(token, 0 , &pdown)<0)
			return count;
	token = strsep(&b, " ");
	if (token != NULL)
		if (kstrtoul(token, 0 , &selfref)<0)
			return count;
	token = strsep(&b, " ");
	if (token != NULL)
		if (kstrtoul(token, 0 , &self_gating)<0)
			return count;

	printk(KERN_INFO" gate %lu, pdown %lu, self %lu, gating %lu\n",
								clock_gate, pdown, selfref, self_gating);

	auto_lpm_enable(clock_gate ,pdown, selfref,self_gating);

	return count;
}
static ssize_t store_delay_tunning(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
	unsigned int i=0 , reg_val=0, offset=0;
	unsigned long  delayvalue =0 , controller = 0, wr_flag=0, slice=0;
    char *b = &buf[0], *token;

	token = strsep(&b, " ");
	if (0 == strncmp("get",token,3))
	{
		for(i=0; i<4; i++)
		{
			offset  = 0x814 + 0x80*i;
			printk(KERN_INFO" controller 0 silice %d  = %4x\n", i, mc_readl(offset, 0));
			printk(KERN_INFO" controller 1 silice %d  = %4x\n", i, mc_readl(offset, 1));
		}
		return count ;
	}

	if (token != NULL)
		if (kstrtoul(token, 0 , &delayvalue)<0)
			return count;
	token = strsep(&b, " ");
	if (token != NULL)
		if (kstrtoul(token, 0 , &controller)<0)
			return count;
	token = strsep(&b, " ");
	if (token != NULL)
		if (kstrtoul(token, 0 , &wr_flag)<0)
			return count;
	token = strsep(&b, " ");
	if (token != NULL)
		if (kstrtoul(token, 0 , &slice)<0)
			return count;
/*
	printk(KERN_INFO" delayvalue = %d, controller = %d , wr = %d, slice = %d\n",
										delayvalue, controller, wr_flag, slice);
*/
	offset = 0x814 + (0x80 * slice);
	reg_val = mc_readl(offset, controller);

	if(wr_flag == 1)
		reg_val = (reg_val & 0xFFFF00FF) | (delayvalue << 8);
	else
		reg_val = (reg_val & 0xFFFFFF00) | (delayvalue);

	mc_writel(reg_val, offset, controller);
	printk(KERN_INFO" reg val = %x\n", reg_val);

	return count;
}

static DEVICE_ATTR (mc_reg, 0644, show_mc_reg_read , store_mc_reg_write);
static DEVICE_ATTR (auto_lpm, 0644, show_auto_lpm, store_auto_lpm);
static DEVICE_ATTR (delay_tunning, 0333, NULL , store_delay_tunning);

u32 mc_readl(u32 offs, u32 sel_ctrl)
{
	u32 val = 0;
#ifdef CONFIG_ODIN_TEE
	val = tz_read(mc->base[sel_ctrl] + offs);
#else
	val = readl(mc->regs[sel_ctrl] + offs);
#endif

	return val;
}

void mc_writel(u32 val, u32 offs, u32 sel_ctrl)
{
#ifdef CONFIG_ODIN_TEE
	tz_write(val, mc->base[sel_ctrl] + offs);
#else
	writel(val, mc->regs[sel_ctrl] + offs);
#endif
}

void auto_lpm_disable()
{
	mc_writel(0x7000000 , MC_AUTOLPM0, 0);
	mc_writel(0x7000000 , MC_AUTOLPM0, 1);
}

static void exit_thermal_region()
{
	u32 val = 0;

	printk(KERN_INFO"%s\n",__func__);
	val = mc_readl(0x040,0) << 1;

	mc_writel(val, 0x040, 0);
	mc_writel(val, 0x040, 1);     /* refresh time change */

	odin_mem_thermal_ctl();
}

static void enter_thermal_region()
{
	u32 val = 0;

	printk(KERN_INFO"%s\n",__func__);
	val = mc_readl(0x040,0) >> 1;

	mc_writel(val, 0x040, 0);
	mc_writel(val, 0x040, 1);     /* refresh time change */

	odin_mem_thermal_ctl();
}

void temp_check_enable(u32 val)
{
	u32 temp;

	temp = mc_readl(0x70, 0);
	temp |= val << 16;

	mc_writel(temp, 0x70, 0);
}

void auto_lpm_enable(u32 clock_gate , u32 pdown, u32 selfref, u32 self_gating)
{

	u32 sr_pd_idle = 0, sr_mc_gate = 0;

	sr_pd_idle = selfref << 24 | pdown << 8 | clock_gate ;
	sr_mc_gate = self_gating;

	mc_writel(sr_pd_idle , MC_AUTOLPM1,0);
	mc_writel(sr_pd_idle , MC_AUTOLPM1,1);

	mc_writel(sr_mc_gate , MC_AUTOLPM2,0);
	mc_writel(sr_mc_gate , MC_AUTOLPM2,1);

	mc_writel(0x7070000 , MC_AUTOLPM0, 0);
	mc_writel(0x7070000 , MC_AUTOLPM0, 1);

}

#ifdef CONFIG_PM
static int odin_mc_suspend(struct device *dev)
{
	int i;
	struct odin_mc *mc = dev_get_drvdata(dev);

	return 0;
}

static int odin_mc_resume(struct device *dev)
{
	int i;
	struct odin_mc *mc = dev_get_drvdata(dev);

	return 0;
}
#endif


static UNIVERSAL_DEV_PM_OPS(odin_mc_pm,
			    odin_mc_suspend,
			    odin_mc_resume, NULL);

static const struct of_device_id odin_mc_of_match[] = {
	{ .compatible = "lge,odin-mc", },
	{},
};

static irqreturn_t odin_mc_isr(int irq, void *data)
{
	u32 stat, mask, val, val1;
	struct odin_mc *mc = data;

	mask = mc_readl(MC_INTMASK, 0);
	stat = mc_readl(MC_INTSTATUS, 0);

	stat = stat & mask;
	if (stat & TEMP_CHANGE_ALERT )
	{
		val =(mc_readl(0x6C,0) >> 8)&0xF;
		val1 =mc_readl(0x70,0)&0xF;

		if (!(val & 0x100 || val1 & 0x100))
		{
			exit_thermal_region();
			mask &= ~TEMP_CHANGE_ALERT;
		}

		mc_writel(TEMP_CHANGE_ALERT, MC_INTACK,0);
	}else if(stat & TEMP_DANGER_ALERT)
	{
		enter_thermal_region();

		mask |= TEMP_CHANGE_ALERT;
		mc_writel(TEMP_DANGER_ALERT, MC_INTACK,0);
	}

	mc_writel(mask, MC_INTMASK,0);

	return IRQ_HANDLED;
}

static int odin_mc_probe(struct platform_device *pdev)
{
	struct resource *irq;
	size_t bytes;
	int err, i;
	u32 intmask;

	bytes = sizeof(*mc);
	mc = devm_kzalloc(&pdev->dev, bytes, GFP_KERNEL);
	if (!mc)
		return -ENOMEM;
	mc->dev = &pdev->dev;

	for (i = 0; i < NUM_MC; i++) {
		struct resource *res;

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res)
			return -ENODEV;
		mc->base[i] = (u32)res->start;
		mc->regs[i] = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(mc->regs[i]))
			return PTR_ERR(mc->regs[i]);
	}

	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irq)
		return -ENODEV;

	printk(KERN_INFO"memory resource %x(%x) %x(%x) %d \n",	mc->base[0],
				(u32)mc->regs[0],mc->base[1],(u32)mc->regs[1],(u32)irq->start);

	platform_set_drvdata(pdev, mc);

	err = devm_request_irq(&pdev->dev, irq->start, odin_mc_isr,
			       IRQF_SHARED, dev_name(&pdev->dev), mc);
	if (err)
		return -ENODEV;

	device_create_file(&pdev->dev, &dev_attr_mc_reg);
	device_create_file(&pdev->dev, &dev_attr_auto_lpm);
	device_create_file(&pdev->dev, &dev_attr_delay_tunning);

	return 0;
}

static struct platform_driver odin_mc_driver = {
	.probe = odin_mc_probe,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = odin_mc_of_match,
		.pm = &odin_mc_pm,
	},
};
module_platform_driver(odin_mc_driver);

MODULE_AUTHOR("Jesse Jung  <jesse.jung@lge.com>");
MODULE_DESCRIPTION("odin MC driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);

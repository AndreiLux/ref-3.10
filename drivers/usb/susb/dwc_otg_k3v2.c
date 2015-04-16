#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/of_address.h>

#include "lm.h"



#define D(format, arg...)    \
	do {                 \
		printk(KERN_INFO "[USB]"format, ##arg); \
	} while (0)
#define E(format, arg...)    \
	do {                 \
		printk(KERN_ERR "[USB]"format, ##arg); \
	} while (0)


#define REG_BASE_PCTRL                     (0xFCA09000)
#define REG_BASE_SCTRL                     (0xFC802000)

#define REG_BASE_USB2DVC                   (0xFD240000)
#define REG_BASE_USB2DVC_VIRT              (0xFE776000)
#define REG_USB2DVC_IOSIZE                  PAGE_ALIGN(SZ_256K)

#define IRQ_GIC_START           32
#define IRQ_USB2DVC		(IRQ_GIC_START + 52)
#define IRQ_USBOTG		(IRQ_GIC_START + 52)

// 软复位寄存器1
#define PER_RST_EN_1_ADDR           0x08C
#define PER_RST_DIS_1_ADDR          0x090
#define PER_RST_STATUS_1_ADDR       0x094

#define IP_RST_HSICPHY              (1 << 25)
#define IP_RST_PICOPHY              (1 << 24)
#define IP_RST_NANOPHY              (1 << 23)

// 软复位寄存器3
#define PER_RST_EN_3_ADDR           0x0A4
#define PER_RST_DIS_3_ADDR          0x0A8
#define PER_RST_STATUS_3_ADDR       0x0AC

#define IP_PICOPHY_POR              (1 << 31)
#define IP_HSICPHY_POR              (1 << 30)
#define IP_NANOPHY_POR              (1 << 29)
#define IP_RST_USB2DVC_PHY          (1 << 28)
#define IP_RST_USB2DVC              (1 << 17)

// 时钟寄存器1
#define PER_CLK_EN_1_ADDR           0x30
#define PER_CLK_DIS_1_ADDR          0x34
#define PER_CLK_EN_STATUS_1_ADDR    0x38
#define PER_CLK_STATUS_1_ADDR       0x3C

#define GT_CLK_USBHSICPHY480        (1 << 26)
#define GT_CLK_USBHSICPHY           (1 << 25)
#define GT_CLK_USBPICOPHY           (1 << 24)
#define GT_CLK_USBNANOPHY           (1 << 23)

// 时钟寄存器3
#define PER_CLK_EN_3_ADDR           0x50
#define PER_CLK_DIS_3_ADDR          0x54
#define PER_CLK_EN_STATUS_3_ADDR    0x58
#define PER_CLK_STATUS_3_ADDR       0x5C

#define GT_CLK_USB2HST              (1 << 18)
#define GT_CLK_USB2DVC              (1 << 17)


// 通用外设控制器16
#define PERI_CTRL16                 0x40
#define PICOPHY_SIDDQ               (1 << 0)
#define PICOPHY_TXPREEMPHASISTUNE   (1 << 31)

// 通用外设控制器17
#define PERI_CTRL17                 0x44

// 通用外设控制器21
#define PERI_CTRL21                 0x01F4


enum k3v2_otg_status {
	K3V2_OTG_OFF = 0,
	K3V2_OTG_DEVICE,
	K3V2_OTG_HOST,
	K3V2_OTG_SUSPEND
};


struct k3v2_otg {
	enum k3v2_otg_status status;
	int dvc_clk_on;
	int pico_clk_on;
	int is_dvc_regu_on;
	int is_phy_regu_on;
	struct clk *dvc_clk;
	struct clk *pico_phy_clk;
	struct regulator_bulk_data    dvc_regu;
	struct regulator_bulk_data    phy_regu;
	struct lm_device    *k3v2_otg_lm_dev;


	void __iomem *pctrl_addr;
	void __iomem *sysctrl_addr;
	void __iomem *pmu_addr;

	void __iomem *usb_base_addr;
	unsigned long usb_resource_length;
};

static struct k3v2_otg *k3v2_otg_p;

static int enable_dvc_clk(void)
{

	int ret = 0;
	if (0 == (k3v2_otg_p->dvc_clk_on)) {
		ret = clk_prepare_enable(k3v2_otg_p->dvc_clk);
		if (!ret)
			k3v2_otg_p->dvc_clk_on = 1;
	}
	return ret;
}
static void disable_dvc_clk(void)
{
	if (k3v2_otg_p->dvc_clk_on) {
		clk_disable_unprepare(k3v2_otg_p->dvc_clk);
		k3v2_otg_p->dvc_clk_on = 0;
	}

}
static int enable_pico_clk(void)
{
	int ret = 0;
	if (0 == (k3v2_otg_p->pico_clk_on)) {
		ret = clk_prepare_enable(k3v2_otg_p->pico_phy_clk);
		if (!ret)
			k3v2_otg_p->pico_clk_on = 1;
	}
	return ret;
}
static void disable_pico_clk(void)
{
	if (k3v2_otg_p->pico_clk_on) {
		clk_disable_unprepare(k3v2_otg_p->pico_phy_clk);
		k3v2_otg_p->pico_clk_on = 0;
	}
}

static int enable_dvc_regu(void)
{
	int ret = 0;

	if (0 == k3v2_otg_p->is_dvc_regu_on) {
		if (regulator_bulk_enable(1, &(k3v2_otg_p->dvc_regu)))
			printk(KERN_ERR "[usbotg]regulator_enable dvc failed!\n");
		else
			k3v2_otg_p->is_dvc_regu_on = 1;
	}

	return ret;
}
static int enable_phy_regu(void)
{
	int ret = 0;

	if(0 == k3v2_otg_p->is_phy_regu_on) {
		if (regulator_bulk_enable(1, &(k3v2_otg_p->phy_regu)))
			printk(KERN_ERR "[usbotg]regulator_enable phy failed!\n");
		else
			k3v2_otg_p->is_phy_regu_on = 1;
	}

	return ret;
}
static void disable_dvc_regu(void)
{
	if(1 == k3v2_otg_p->is_dvc_regu_on) {
		regulator_bulk_disable(1, &(k3v2_otg_p->dvc_regu));
		k3v2_otg_p->is_dvc_regu_on = 0;
	}
}
static void disable_phy_regu(void)
{
	if(1 == k3v2_otg_p->is_phy_regu_on) {
		regulator_bulk_disable(1, &(k3v2_otg_p->phy_regu));
		k3v2_otg_p->is_phy_regu_on = 0;
	}
}


static int setup_dvc_and_phy(void)
{
	unsigned reg_value;
	void __iomem *sysctrl_base = k3v2_otg_p->sysctrl_addr;
	void __iomem *pctrl_base = k3v2_otg_p->pctrl_addr;

	enable_dvc_regu();
	enable_phy_regu();

	udelay(200);

	/* DVC Core reset */
	writel(IP_RST_USB2DVC, (sysctrl_base + PER_RST_EN_3_ADDR));
	/* DVC PHY reset */
	writel(IP_RST_USB2DVC_PHY, (sysctrl_base + PER_RST_EN_3_ADDR));
	/* pico phy reset */
	writel(IP_RST_PICOPHY, (sysctrl_base + PER_RST_EN_1_ADDR));
	/* pico_phy_por */
	writel(IP_PICOPHY_POR, (sysctrl_base + PER_RST_EN_3_ADDR));

	/* clk off */
	disable_pico_clk();
	disable_dvc_clk();

	/// PCTRL16
	reg_value = readl((pctrl_base + PERI_CTRL16));
	reg_value &= (~(PICOPHY_SIDDQ));
	reg_value |= PICOPHY_TXPREEMPHASISTUNE;
	/* usb1_phy_otgdisable */
	reg_value &= ~(0x1 << 9);
	/* usb1_phy_vbusvldextsel */
	reg_value &= ~(0x3 << 10);
	/* bit[19:17]: usb1_phy_compdistune[2:0], disconnect detect threshold tuning */
	reg_value &= ~(0x7 << 17);
	reg_value |= (0x6 << 17);
	writel(reg_value, (pctrl_base + PERI_CTRL16));

	/// PCTRL17
	reg_value = readl(pctrl_base + PERI_CTRL17);
	reg_value &= ~0x3F;
	reg_value |= 0x23;
	D("phy param: 0x%x\n", reg_value);
	writel(reg_value, (pctrl_base + PERI_CTRL17));

	/// PCTRL21
	reg_value = readl((pctrl_base + PERI_CTRL21));
	reg_value &= ~((0x3 << 1) | (0x3 << 8) | (0x3 << 10));
	reg_value |= ((0x1 << 1) | (0x3 << 8) | (0x1 << 10));
	writel(reg_value, (pctrl_base + PERI_CTRL21));

	enable_pico_clk();

	udelay(10);

	// undo phy reset
	writel(IP_RST_PICOPHY, (sysctrl_base + PER_RST_DIS_1_ADDR));
	writel(IP_PICOPHY_POR, (sysctrl_base + PER_RST_DIS_3_ADDR));

	udelay(1000);

	enable_dvc_clk();

	// undo dvc reset
	writel(IP_RST_USB2DVC_PHY, (sysctrl_base + PER_RST_DIS_3_ADDR));
	udelay(1);
	writel(IP_RST_USB2DVC, (sysctrl_base + PER_RST_DIS_3_ADDR));

	return 0;
}


static void release_plat_resouce(void)
{

	if (k3v2_otg_p->dvc_regu.consumer) {
		disable_dvc_regu();
		regulator_put(k3v2_otg_p->dvc_regu.consumer);
		}

	if (k3v2_otg_p->phy_regu.consumer) {
		disable_phy_regu();
		regulator_put(k3v2_otg_p->phy_regu.consumer);
		}

	if (k3v2_otg_p->pico_phy_clk)
		clk_put(k3v2_otg_p->pico_phy_clk);
	if (k3v2_otg_p->dvc_clk)
		clk_put(k3v2_otg_p->dvc_clk);
}

static int get_plat_resource(struct platform_device *pdev)
{
	struct clk          *clock;
	struct regulator    *regu;
	struct device_node *np;
	int ret;

	// get sysctrl base addr
	np = of_find_compatible_node(NULL, NULL, "hisilicon,sctrl");
	k3v2_otg_p->sysctrl_addr = of_iomap(np, 0);
	D("sysctrl_addr: %p\n", k3v2_otg_p->sysctrl_addr);

	// get pctrl base addr
	np = of_find_compatible_node(NULL, NULL, "hisilicon,pctrl");
	if (!np) {
		E("perictrl node is not found\n");
		return -EINVAL;
	}
	k3v2_otg_p->pctrl_addr= of_iomap(np, 0);
	D("perictrl_addr: %p\n", k3v2_otg_p->pctrl_addr);


	/* get clocks */
	clock = clk_get(NULL, "clk_usb2dvc");
	ret = (IS_ERR(clock));
	if (ret) {
		E("get dvc_clk failed!\n");
		goto error;
	}
	k3v2_otg_p->dvc_clk = clock;

	clock = clk_get(NULL, "clk_usbpicophy");
	ret = (IS_ERR(clock));
	if (ret) {
		E("get phy_clk failed!\n");
		goto error;
	}
	k3v2_otg_p->pico_phy_clk = clock;

	/* clock status record */
	k3v2_otg_p->dvc_clk_on = 0;
	k3v2_otg_p->pico_clk_on = 0;

	/* get retulator. */

	k3v2_otg_p->dvc_regu.supply = "otgphy";
	if (regulator_bulk_get(&pdev->dev, 1, &(k3v2_otg_p->dvc_regu))) {
		E("regulator_get dvc failed!\r\n");
		ret = -EBUSY;
		goto error;
	}
	k3v2_otg_p->is_dvc_regu_on = 0;


	k3v2_otg_p->phy_regu.supply = "dvc";
	if (regulator_bulk_get(&pdev->dev, 1, &(k3v2_otg_p->phy_regu))) {
		E("regulator_get phy failed!\r\n");
		ret = -EBUSY;
		goto error;
	}
	k3v2_otg_p->is_phy_regu_on = 0;
	return 0;
error:
	release_plat_resouce();
	return ret;
}

static void k3v2_otg_setup(void)
{
	setup_dvc_and_phy();
	k3v2_otg_p->status = K3V2_OTG_DEVICE;
	return;
}

static int dwc_otg_k3v2_probe(struct platform_device *pdev)
{
	int ret = 0;
	int irq = 0;
	struct device *dev = &pdev->dev;
	struct lm_device *lm_dev;
	struct k3v2_otg *k3v2_otg_device;
	struct resource *res;

D(">>>>[%s]+++\n", __func__);
	k3v2_otg_device = devm_kzalloc(dev, sizeof(struct k3v2_otg), GFP_KERNEL);
	if (!k3v2_otg_device) {
		dev_err(dev, "not enough memory\n");
		return -ENOMEM;
	}

	dev_set_name(&pdev->dev, "k3v2_otg_test");
	k3v2_otg_p = k3v2_otg_device;
	platform_set_drvdata(pdev, k3v2_otg_device);

	// get regulator, clock, sysctrl addr, pctrl addr
	ret = get_plat_resource(pdev);
	if (ret) {
		E("get clk or ldo resouce error!\n");
		return ret;
	}

	k3v2_otg_p->status = K3V2_OTG_OFF;

	// enable regulator, clock, phy...
	k3v2_otg_setup();

	/* register the dwc otg device */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		E("device: %s resource error\n", dev_name(&pdev->dev));
		return -EBUSY;
	}

	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		E("device: %s irq num error\n", dev_name(&pdev->dev));
		return -EBUSY;
	}

	lm_dev = devm_kzalloc(dev, sizeof(*lm_dev), GFP_KERNEL);
	if (!lm_dev) {
		dev_err(dev, "not enough memory\n");
		return -ENOMEM;
	}

	k3v2_otg_p->k3v2_otg_lm_dev = lm_dev;

	lm_dev->resource.start = res->start;
	lm_dev->resource.end = res->end;
	lm_dev->resource.flags = res->flags;
	lm_dev->irq = irq;
	lm_dev->id = -1;
	lm_dev->dev.init_name = "hisik3-usb-otg";

	dev_set_name(dev, "k3v2_otg_test");
	ret = lm_device_register(lm_dev);
	if (ret) {
		E("register lm device failed\n");
		return ret;
	}

D(">>>>[%s]---\n", __func__);
	return ret;
}

static int dwc_otg_k3v2_remove(struct platform_device *pdev)
{
	release_plat_resouce();
	return 0;
}

int dwc_otg_k3v2_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

int dwc_otg_k3v2_resume(struct platform_device *pdev)
{
	return 0;
}



#ifdef CONFIG_OF
static const struct of_device_id dwc_otg_k3v2_mach[] = {
	{ .compatible = "hisilicon,k3v2_otg" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_dwc3_match);
#else
#define dwc_otg_k3v2_mach NULL
#endif

static struct platform_driver dwc_otg_k3v2_driver = {
	.probe		= dwc_otg_k3v2_probe,
	.remove		= dwc_otg_k3v2_remove,
	.suspend	= dwc_otg_k3v2_suspend,
	.resume		= dwc_otg_k3v2_resume,
	.driver		= {
		.name	= "k3v2_dwc_otg",
		.of_match_table = of_match_ptr(dwc_otg_k3v2_mach),
	},
};

module_platform_driver(dwc_otg_k3v2_driver);

MODULE_AUTHOR("l00196665");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("DesignWare OTG2.0 k3v2 Glue Layer");

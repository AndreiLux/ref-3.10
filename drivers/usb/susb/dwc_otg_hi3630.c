#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>

#include <linux/huawei/usb/hisi_usb.h>

#include "lm.h"
#include "dwc_otg_pcd.h"
#include "dwc_otg_pcd_if.h"
#include "dwc_otg_core_if.h"
#include "dwc_otg_cil.h"
#include "dwc_otg_driver.h"
#include "dwc_otg_hcd.h"

#include "dwc_otg_hi3630.h"

atomic_t is_event_work_ready = ATOMIC_INIT(0);
atomic_t is_power_on = ATOMIC_INIT(0);

struct otg_dev * otg_dev_p;

#define ENABLE_USB_OTG_HI3630_TEST 1

#if ENABLE_USB_OTG_HI3630_TEST

#define USB_AHBIF_REGS_COUNT 20
static unsigned ahbif_regs_value[USB_AHBIF_REGS_COUNT];
static unsigned is_need_restore;

static ssize_t ahbif_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	ssize_t ret = 0;

	ret += sprintf((buf + ret), "usb ahbif regs value:\n");
	for (i = 0; i < USB_AHBIF_REGS_COUNT; i++) {
		ret += sprintf((buf + ret), "0x%02x - 0x%08x\n",
			i * sizeof(ahbif_regs_value[0]),
			ahbif_regs_value[i]);
	}
	return ret;
}

/**
 * return 0 means get option value sucess, other failed!
 */
static int get_opt_value(const char *buf, const char *opt,
		unsigned *value)
{
	char *p = NULL;

	p = strstr(buf, opt);
	if (!p)
		return -1;

	for (; *p; p++) {
		if (*p == '=') {
			if (sscanf(p + 1, "%x", value) == 0)
				return -1;
			break;
		}
	}

	return 0;
}
static ssize_t ahbif_reg_store(struct device *pdev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned off, value;

	if ((0 == get_opt_value(buf, "offset", &off))
			&& (0 == get_opt_value(buf, "value", &value))) {
		if (USB_AHBIF_REGS_COUNT >= (off >> 2)) {
			ahbif_regs_value[(off >> 2)] = value;
			printk(KERN_INFO "set ahbif_regs_value[%d] as 0x%08x\n",
				(off >> 2), ahbif_regs_value[(off >> 2)]);
			is_need_restore = 1;
		}
	} else {
		printk(KERN_INFO "USAGE: echo \"offset=0x???,value=0x???\" > ahbif_reg\n");
	}
	return size;
}

DEVICE_ATTR(ahbif_reg, S_IRUGO | S_IWUSR, ahbif_reg_show, ahbif_reg_store);

void ahbif_reg_save(void)
{
	struct otg_dev *p = otg_dev_p;
	int i;

	for (i = 0; i < USB_AHBIF_REGS_COUNT; i++) {
		ahbif_regs_value[i] = readl(p->usb_ahbif_base + i * 4);
	}
}

void ahbif_reg_restore(void)
{
	struct otg_dev *p = otg_dev_p;
	struct usb_ahbif_registers *ahbif
			= (struct usb_ahbif_registers *)p->usb_ahbif_base;
	int i;

	usb_dbg("[%s]\n", __func__);

#define AHBIF_REG_RESTORE(reg_addr)	\
	do {				\
		i = (((unsigned)(reg_addr))					\
			    - (unsigned)otg_dev_p->usb_ahbif_base) >> 2;	\
		writel(ahbif_regs_value[i], reg_addr);				\
		usb_dbg("addr: %p, value: 0x%x\n",				\
				reg_addr, ahbif_regs_value[i]);			\
	} while (0)

	AHBIF_REG_RESTORE(&(ahbif->usbotg2_ctrl0));
	AHBIF_REG_RESTORE(&(ahbif->usbotg2_ctrl2));
	AHBIF_REG_RESTORE(&(ahbif->usbotg2_ctrl3));
	AHBIF_REG_RESTORE(&(ahbif->usbotg2_ctrl4));

	/* update saved value from register */
	ahbif_reg_save();
}

static ssize_t plugusb_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *s;
	switch (otg_dev_p->status) {
	case OTG_DEV_OFF:
		s = "OTG_DEV_OFF";
		break;
	case OTG_DEV_DEVICE:
		s = "OTG_DEV_DEVICE";
		break;
	case OTG_DEV_HOST:
		s = "OTG_DEV_HOST";
		break;
	case OTG_DEV_SUSPEND:
		s = "OTG_DEV_SUSPEND";
		break;
	default :
		s = "unknown";
		break;
	}
	return sprintf(buf, "usb otg status: %s\n", s);
}
static ssize_t plugusb_store(struct device *pdev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int i;
	struct otg_dev *dev_p = dev_get_drvdata(pdev);
	sscanf(buf, "%d", &i);
	if (i) {
		if (OTG_DEV_OFF == dev_p->status) {
			hisi_usb_otg_event(CHARGER_CONNECT_EVENT);
		}
	} else {
		if (OTG_DEV_DEVICE == dev_p->status)
			hisi_usb_otg_event(CHARGER_DISCONNECT_EVENT);
	}
	return size;
}
DEVICE_ATTR(plugusb, S_IRUGO | S_IWUSR, plugusb_show, plugusb_store);

static ssize_t eyetest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "usage: echo x > eyetest\n");
}
static ssize_t eyetest_store(struct device *pdev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct otg_dev *dev_p = dev_get_drvdata(pdev);
	dwc_otg_device_t *dwc_otg_dev = lm_get_drvdata(dev_p->lm_dev);

	if (dwc_otg_is_host_mode(dwc_otg_dev->core_if)) {
		hprt0_data_t reg_value;
		reg_value.d32 = dwc_otg_get_hprt0(dwc_otg_dev->core_if);
		reg_value.b.prttstctl = 4;
		dwc_otg_set_hprt0(dwc_otg_dev->core_if, reg_value.d32);
		printk(KERN_INFO "[USB]ENTER HOST EYE DIAGRAM TEST!\n");
	} else {
		dctl_data_t reg_value;
		reg_value.d32 = DWC_READ_REG32(&dwc_otg_dev->core_if->dev_if->dev_global_regs->dctl);
		reg_value.b.tstctl = 4;
		DWC_WRITE_REG32(&dwc_otg_dev->core_if->dev_if->dev_global_regs->dctl, reg_value.d32);
		printk(KERN_INFO "[USB]ENTER DEVICE EYE DIAGRAM TEST!\n");
	}

	return size;
}
DEVICE_ATTR(eyetest, S_IRUGO | S_IWUSR, eyetest_show, eyetest_store);

int create_attr_file(struct device *dev)
{
	int ret = 0;
	ret |= device_create_file(dev, &dev_attr_plugusb);
	ret |= device_create_file(dev, &dev_attr_ahbif_reg);
	ret |= device_create_file(dev, &dev_attr_eyetest);
	if (ret) {
		usb_dbg("create attr file error!\n");
	}
	usb_dbg("create_attr_file %s!\n", ret ? "error" : "done");
	return ret;
}
#else
#define is_need_restore 0
static inline int create_attr_file(struct device *dev){return 0;}
static inline void ahbif_reg_save(void){}
static inline void ahbif_reg_restore(void){}
#endif

#if ENABLE_USB_OTG_HI3630_TEST
#define CHECK_USB_CABLE_STRICTLY 0
#else
#define CHECK_USB_CABLE_STRICTLY 1
#endif

int get_resource(struct otg_dev *dev)
{
	int ret = 0;
	struct device_node *np = NULL;

	/*
	 * get registers base address
	 */
	np = of_find_compatible_node(NULL, NULL, "hisilicon,hi3630-usb-otg-ahbif");
	if (np) {
		dev->usb_ahbif_base = of_iomap(np, 0);
	}
	np = of_find_compatible_node(NULL, NULL, "hisilicon,crgctrl");
	if (np) {
		dev->pericrg_base = of_iomap(np, 0);
	}
	np = of_find_compatible_node(NULL, NULL, "hisilicon,hi6421-pmurtc");
	if (np) {
		dev->pmu_reg_base = of_iomap(np, 0);
	}

	if (!dev->usb_ahbif_base || !dev->pericrg_base || !dev->pmu_reg_base) {
		usb_err("get registers base address failed!\n");
		return -ENODEV;
	}

	/*
	 * get ldo handlers
	 */
	dev->otgdebugsubsys_regu.supply = "otgdebugsubsys";
	ret = devm_regulator_bulk_get(&dev->pdev->dev, 1, &dev->otgdebugsubsys_regu);
	if (ret) {
		usb_err("couldn't get regulator otgdebugsubsys\n");
	}

	usb_dbg("get resource %s!\n", ret ? "error" : "done");
	return 0;
}

static void put_resource(struct otg_dev *dev)
{
	usb_dbg("put resource done.\n");
	return;
}

int open_ldo(struct otg_dev *otg_device)
{
	int ret;

	if (0 != otg_device->is_regu_on) {
		usb_dbg("ldo already opened!\n");
		return 0;
	}

	ret = regulator_bulk_enable(1, &otg_device->otgdebugsubsys_regu);
	if (ret) {
		usb_err("enable regulator otgdebugsubsys failed\n");
	}

	otg_device->is_regu_on = 1;

	return ret;
}

void close_ldo(struct otg_dev *otg_device)
{
	if (0 == otg_device->is_regu_on) {
		usb_dbg("ldo already closed!\n");
		return;
	}

	regulator_bulk_disable(1, &otg_device->otgdebugsubsys_regu);
	otg_device->is_regu_on = 0;
}

void init_usb_otg_phy(struct otg_dev *otg_device, int is_host_mode)
{
	void __iomem *pericrg_base = otg_device->pericrg_base;
	struct usb_ahbif_registers *ahbif
		= (struct usb_ahbif_registers *)otg_device->usb_ahbif_base;
	union usbotg2_ctrl0 ctrl0;

	/* unreset usb ahbif */
	writel(BIT_RST_USB2OTG_ADP | BIT_RST_USB2OTG_32K |
				BIT_RST_USB2OTG_MUX |
				BIT_RST_USB2OTG_AHBIF,
				pericrg_base + PERI_CRG_RSTDIS4);

	if (!is_need_restore) {

		ctrl0.reg = readl(&ahbif->usbotg2_ctrl0);
#ifndef FORCE_ID_STATE
		ctrl0.bits.id_sel = 1; /* 1: from phy iddig, 2: from phy aca*/
#else
		ctrl0.bits.id_sel = 0; /* 1: from phy iddig, 2: from phy aca*/
		if (is_host_mode) {
			ctrl0.bits.id = 0;
		} else {
			ctrl0.bits.id = 1;
		}
#endif

		ctrl0.bits.acaenb_sel = 1;
		ctrl0.bits.acaenb = 0;
		writel(ctrl0.reg, &ahbif->usbotg2_ctrl0);

		writel(0x01ee66e7, &ahbif->usbotg2_ctrl3);

		/* save regs value */
		ahbif_reg_save();
	} else {
		ahbif_reg_restore();
#ifdef FORCE_ID_STATE
		ctrl0.reg = readl(&ahbif->usbotg2_ctrl0);
		ctrl0.bits.id_sel = 0; /* 1: from phy iddig, 2: from phy aca*/
		if (is_host_mode) {
			ctrl0.bits.id = 0;
		} else {
			ctrl0.bits.id = 1;
		}
		writel(ctrl0.reg, &ahbif->usbotg2_ctrl0);
#endif
	}

	writel(USBPHY_QUIT_SIDDQ, &ahbif->usbotg2_ctrl4);
	udelay(10);
	/* reset phy */
	writel(BIT_RST_USB2OTGPHYPOR | BIT_RST_USB2OTGPHY,
			pericrg_base + PERI_CRG_RSTEN4);
	/* dealy 12us */
	udelay(12);
	/* unreset phy */
	writel(BIT_RST_USB2OTGPHYPOR | BIT_RST_USB2OTGPHY,
			pericrg_base + PERI_CRG_RSTDIS4);
	/* dealy 50us */
	udelay(50);
	/* unreset controller phy port */
	writel(BIT_RST_USB2OTG_PHY_N, pericrg_base + PERI_CRG_RSTDIS4);
	/* delay 100ns */
	udelay(100);
	/* unreset controller */
	writel(BIT_RST_USB2OTG, pericrg_base + PERI_CRG_RSTDIS4);
	msleep(1);
}

void close_usb_otg_phy(void)
{
	void __iomem *pericrg_base = otg_dev_p->pericrg_base;
	struct usb_ahbif_registers  *ahbif = otg_dev_p->usb_ahbif_base;

	/* reset controller */
	writel(BIT_RST_USB2OTG, pericrg_base + PERI_CRG_RSTEN4);
	udelay(1);
	writel(BIT_RST_USB2OTG_PHY_N, pericrg_base + PERI_CRG_RSTEN4);
	udelay(1);
	/* reset phy */
	writel(BIT_RST_USB2OTGPHYPOR | BIT_RST_USB2OTGPHY,
			pericrg_base + PERI_CRG_RSTEN4);

	writel(USBPHY_ENTER_SIDDQ, &ahbif->usbotg2_ctrl4);
	udelay(10);
	/* reset usb ahbif */
	writel(BIT_RST_USB2OTG_ADP | BIT_RST_USB2OTG_32K | BIT_RST_USB2OTG_MUX |
			BIT_RST_USB2OTG_AHBIF, pericrg_base + PERI_CRG_RSTEN4);
	msleep(1);
}

static void show_pericrg_status(void)
{
	void __iomem *pericrg_base = otg_dev_p->pericrg_base;
	printk(KERN_INFO "PERI_CRG_RSTEN4: 0x%08x, PERI_CRG_RSTDIS4: 0x%08x,"
		" PERI_CRG_RSTSTAT4: 0x%08x\n",
		readl(pericrg_base + PERI_CRG_RSTEN4),
		readl(pericrg_base + PERI_CRG_RSTDIS4),
		readl(pericrg_base + PERI_CRG_RSTSTAT4)
		);
}

static int hw_setup(struct otg_dev *otg_device, int is_host_mode)
{
	int ret;
	ret = open_ldo(otg_device);
	if (ret) {
		usb_err("open ldo error!");
		return ret;
	}

	init_usb_otg_phy(otg_device, is_host_mode);
	show_pericrg_status();
	atomic_set(&is_power_on, 1);

	otg_device->hcd_status = HCD_ON;

	if (otg_device->dwc_otg_irq_enabled == false) {
		enable_irq(otg_device->lm_dev->irq);
		otg_device->dwc_otg_irq_enabled = true;
	}

	return 0;
}

static void hw_shutdown(struct otg_dev *otg_device)
{
	dwc_otg_device_t *dwc_otg_dev;
	unsigned long flags;

	if ((!otg_device) || (!otg_device->lm_dev))
		return;

	dwc_otg_dev = lm_get_drvdata(otg_device->lm_dev);

	if (!dwc_otg_dev)
		return;

	/* get spin lock, wait for irq handler finish */
	DWC_SPINLOCK_IRQSAVE(dwc_otg_dev->core_if->lock, &flags);
	atomic_set(&is_power_on, 0);
	DWC_SPINUNLOCK_IRQRESTORE(dwc_otg_dev->core_if->lock, flags);

#ifdef DEBUG
	if (timer_pending(&dwc_otg_dev->core_if->wkp_timer)) {
		usb_dbg("wkp_timer pending\n");
	}
	if (work_pending(&dwc_otg_dev->core_if->id_change_work.work)) {
		usb_dbg("id_change_work pending\n");
	}
	if (work_pending(&dwc_otg_dev->hcd->hcd_start_work.work)) {
		usb_dbg("hcd_start_work pending\n");
	}
#endif

	del_timer_sync(&dwc_otg_dev->core_if->wkp_timer);
	cancel_delayed_work_sync(&dwc_otg_dev->core_if->id_change_work);
	cancel_delayed_work_sync(&dwc_otg_dev->hcd->hcd_start_work);
	/* work or timer may enable global interrupts, close it now */
	dwc_otg_disable_global_interrupts(dwc_otg_dev->core_if);

	close_usb_otg_phy();
	close_ldo(otg_device);
	show_pericrg_status();

	otg_device->hcd_status = HCD_OFF;

	if (otg_device->dwc_otg_irq_enabled == true) {
		disable_irq(otg_device->lm_dev->irq);
		otg_device->dwc_otg_irq_enabled = false;
	}

	return;
}

int dwc_otg_hcd_setup(void)
{
	struct otg_dev * p = otg_dev_p;
	mutex_lock(&p->lock);

	if ((OTG_DEV_OFF == p->status) && (HCD_OFF == p->hcd_status)) {
		usb_dbg("p->status: %d, hcd_status: %d\n", p->status, p->hcd_status);
		if (open_ldo(p)) {
			usb_err("open ldo failed!\n");
			mutex_unlock(&p->lock);
			return -1;
		}
		init_usb_otg_phy(p, 1);
		p->hcd_status = HCD_ON;;
	}

	mutex_unlock(&p->lock);
	return 0;
}

void dwc_otg_hcd_shutdown(void)
{
	struct otg_dev * p = otg_dev_p;
	mutex_lock(&p->lock);

	if ((OTG_DEV_OFF == p->status) && (HCD_ON == p->hcd_status)) {
		usb_dbg("p->status: %d, hcd_status: %d\n", p->status, p->hcd_status);
		close_usb_otg_phy();
		close_ldo(p);
		p->hcd_status = HCD_OFF;
	}

	mutex_unlock(&p->lock);
}

static char * charger_type_string(enum hisi_charger_type type)
{
	char *s = NULL;
	if (type == CHARGER_TYPE_SDP)
		s = "SDP";
	else if (type == CHARGER_TYPE_CDP)
		s = "CDP";
	else if (type == CHARGER_TYPE_DCP)
		s = "DCP";
	else if (type == CHARGER_TYPE_UNKNOWN)
		s = "UNKNOWN";
	else if (type == CHARGER_TYPE_NONE)
		s = "NONE";
	else
		s = "ilegal charger";
	return s;
}

static void detect_charger_type(struct otg_dev *dev_p)
{
	struct usb_ahbif_registers *ahbif
		= (struct usb_ahbif_registers *)otg_dev_p->usb_ahbif_base;
	enum hisi_charger_type type = CHARGER_TYPE_NONE;
	union bc_ctrl4 bc_ctrl4;
	union bc_ctrl5 bc_ctrl5;
	union bc_sts2 bc_sts2;
	unsigned long jiffies_expire;
	int i = 0;

	/* enable BC */
	writel(1, &ahbif->bc_ctrl3);

	/* phy suspend */
	bc_ctrl4.reg = readl(&ahbif->bc_ctrl4);
	bc_ctrl4.bits.bc_suspendm = 0;
	writel(bc_ctrl4.reg, &ahbif->bc_ctrl4);

	/* enable DCD */
	bc_ctrl5.reg = readl(&ahbif->bc_ctrl5);
	bc_ctrl5.bits.bc_dcd_en = 1;
	writel(bc_ctrl5.reg, &ahbif->bc_ctrl5);

	jiffies_expire = jiffies + msecs_to_jiffies(900);
	msleep(50);
	while (i < 10) {
		union bc_sts2 bc_sts2;

		bc_sts2.reg = readl(&ahbif->bc_sts2);
		if (bc_sts2.bits.bc_fs_vplus == 0) {
			i++;
		} else if (i) {
			usb_dbg("USB D+ D- not connected!\n");
			i = 0;
		}

		msleep(1);

		if (time_after(jiffies, jiffies_expire)) {
			type = CHARGER_TYPE_UNKNOWN;
			break;
		}
	}

	/* disable DCD */
	bc_ctrl5.reg = readl(&ahbif->bc_ctrl5);
	bc_ctrl5.bits.bc_dcd_en = 0;
	writel(bc_ctrl5.reg, &ahbif->bc_ctrl5);

	if (type == CHARGER_TYPE_NONE) {
		/* enable vdect */
		bc_ctrl5.reg = readl(&ahbif->bc_ctrl5);
		bc_ctrl5.bits.bc_chrg_sel = 0;
		bc_ctrl5.bits.bc_vdat_src_en = 1;
		bc_ctrl5.bits.bc_vdat_det_en = 1;
		writel(bc_ctrl5.reg, &ahbif->bc_ctrl5);

		msleep(10);

		/* we can detect sdp or cdp dcp */
		bc_sts2.reg = readl(&ahbif->bc_sts2);
		if (bc_sts2.bits.bc_chg_det == 0) {
			type = CHARGER_TYPE_SDP;
		}

		/* disable vdect */
		bc_ctrl5.reg = readl(&ahbif->bc_ctrl5);
		bc_ctrl5.bits.bc_vdat_src_en = 0;
		bc_ctrl5.bits.bc_vdat_det_en = 0;
		writel(bc_ctrl5.reg, &ahbif->bc_ctrl5);
	}

	/* bc_suspend = 1, nomal mode */
	bc_ctrl4.reg = readl(&ahbif->bc_ctrl4);
	bc_ctrl4.bits.bc_suspendm = 1;
	writel(bc_ctrl4.reg, &ahbif->bc_ctrl4);

	msleep(10);

	/* disable BC */
	writel(0, &ahbif->bc_ctrl3);

	if (type == CHARGER_TYPE_NONE) {
		type = CHARGER_TYPE_DCP;
	}

	otg_dev_p->charger_type = type;
	usb_dbg("charger type: %s\n", charger_type_string(type));
	return;
}

int hisi_charger_type_notifier_register(struct notifier_block *nb)
{
	int ret = -1;
	if (otg_dev_p)
		ret = atomic_notifier_chain_register(
			    &otg_dev_p->charger_type_notifier, nb);
	else
		printk(KERN_ERR "Notifier head not yet init.\n");
	return ret;
}

int hisi_charger_type_notifier_unregister(struct notifier_block *nb)
{
	int ret = -1;
	if (otg_dev_p)
		ret = atomic_notifier_chain_unregister(
			    &otg_dev_p->charger_type_notifier, nb);
	else
		printk(KERN_ERR "Notifier head not yet init.\n");
	return ret;
}


enum hisi_charger_type hisi_get_charger_type(void)
{
	struct otg_dev *dev_p = otg_dev_p;
	enum hisi_charger_type type = dev_p->charger_type;
	usb_dbg("charger type: %s\n", charger_type_string(type));
	return type;
}

static void notify_charger_type(struct otg_dev *dev_p)
{
	atomic_notifier_call_chain(&dev_p->charger_type_notifier,
				dev_p->charger_type, dev_p);
}

void update_charger_type(void)
{
	if (otg_dev_p->charger_type == CHARGER_TYPE_DCP) {
		otg_dev_p->charger_type = CHARGER_TYPE_CDP;
		notify_charger_type(otg_dev_p);
		usb_dbg("charger type: %s\n", charger_type_string(
				otg_dev_p->charger_type));
	}
}

void clear_charger_interrupt(void)
{
	void __iomem *reg_addr = otg_dev_p->pmu_reg_base + REG_PMU_SSI_IRQ1;

	/* vbus_comp_vbat_f	4	W1C	0x0
	 * vbus_comp_vbat_r	3	W1C	0x0 */
	iowrite32((VBUS_COMP_VBAT_F | VBUS_COMP_VBAT_R), reg_addr);
	return;
}

static int is_usb_cable_connected(void)
{
	void __iomem *reg_addr = otg_dev_p->pmu_reg_base + REG_PMU_SSI_STATUS0;
	return (ioread32(reg_addr) & (VBUS4P3_D10 | VBUS_COMP_VBAT_D20));
}

int dwc_otg_hi3630_is_power_on(void)
{
	return atomic_read(&is_power_on);
}

void dwc_otg_hi3630_set_prtpower(unsigned int val)
{
	struct otg_dev *p = otg_dev_p;
	enum hisi_charger_type new;

	if (0 == val) {
		new = CHARGER_TYPE_NONE;
	} else {
		new = PLEASE_PROVIDE_POWER;
	}
	if (p->charger_type != new) {
		usb_dbg("set port power %d\n", val);
		p->charger_type = new;
		notify_charger_type(p);
	}
}

static void event_work(struct work_struct *work)
{
	unsigned long flags;
	enum otg_dev_event_type event;
	int usb_cable_status;
	struct otg_dev *p = otg_dev_p;
	struct dwc_otg_device *dwc_otg_dev = lm_get_drvdata(p->lm_dev);

	mutex_lock(&p->lock);

	usb_dbg("+\n");

	/* get event type */
	spin_lock_irqsave(&(p->event_lock), flags);
	event = p->event;
	spin_unlock_irqrestore(&(p->event_lock), flags);
	usb_dbg("event type: %d\n", event);

	usb_cable_status = is_usb_cable_connected();

	switch (event) {
	case CHARGER_CONNECT_EVENT:
#if CHECK_USB_CABLE_STRICTLY
		if (0 == usb_cable_status) {
			usb_dbg("USB cable not connected.\n");
			goto error;
		}
#endif
		if (OTG_DEV_DEVICE == p->status) {
			usb_dbg("Already in device mode, do nothing.\n");
		} else if ((OTG_DEV_OFF == p->status)
				|| (OTG_DEV_SUSPEND == p->status)) {

			/* do not go to sleep */
			dwc_otg_hi3630_wake_lock();

			/* open ldo and clock */
			if (hw_setup(p, 0)) {
				dwc_otg_hi3630_wake_unlock();
				goto error;
			}

			/* disable usb core interrupt */
			dwc_otg_disable_global_interrupts(dwc_otg_dev->core_if);

			/* Get charger type. */
			detect_charger_type(p);
			notify_charger_type(p);

			/* reinit usb core */
			if (!dwc_otg_dev->core_if->core_params->dma_desc_enable) {
				usb_dbg("enable desc dma\n");
				dwc_otg_dev->core_if->core_params->dma_desc_enable = 1;
			}
			dwc_otg_core_init(dwc_otg_dev->core_if);
			if (dwc_otg_is_device_mode(dwc_otg_dev->core_if)) {
				cil_pcd_start(dwc_otg_dev->core_if);
				dwc_otg_dev->core_if->op_state = B_PERIPHERAL;
			} else {
				usb_dbg("error in host mode!\n");
			}

			/* enable usb core interrupt */
			dwc_otg_enable_global_interrupts(dwc_otg_dev->core_if);

			/* do software disconnect and connect for host enum again */
			dwc_otg_pcd_disconnect_us(dwc_otg_dev->pcd, 50);
			udelay(100);

			if (p->gadget_initialized)
				dwc_otg_pcd_connect_us(dwc_otg_dev->pcd, 50);
			else
				usb_dbg("gadget is not initialized!\n");

			p->status = OTG_DEV_DEVICE;

			usb_dbg("hisi usb status: OFF -> DEVICE\n");
		} else if (OTG_DEV_HOST == p->status) {
			usb_dbg("Charger connect intrrupt in HOST mode \n");
		}

		break;

	case CHARGER_DISCONNECT_EVENT:
#if CHECK_USB_CABLE_STRICTLY
		if (0 != usb_cable_status) {
			usb_dbg("Charger disconnect interrupt,"
				" but USB cable still connected!\n");
			goto error;
		}
#endif
		if (OTG_DEV_OFF == p->status) {
			usb_dbg("Charger disconnect interrupt, but Already in OFF mode.\n");
		} else if (OTG_DEV_DEVICE == p->status) {
			/* For send uevent of disconnect. */
			dwc_otg_dev->pcd->fops->disconnect(dwc_otg_dev->pcd);

			dwc_otg_disable_global_interrupts(dwc_otg_dev->core_if);
			dwc_otg_mask_all_interrupts(dwc_otg_dev->core_if);
			dwc_otg_clear_all_interrupts(dwc_otg_dev->core_if);

			msleep(100);

			hw_shutdown(p);

			p->charger_type = CHARGER_TYPE_NONE;
			notify_charger_type(p);

			p->status = OTG_DEV_OFF;

			/* allow system go to sleep */
			dwc_otg_hi3630_wake_unlock();

			usb_dbg("hisi usb status: DEVICE -> OFF\n");
		} else if (OTG_DEV_HOST == p->status) {
			usb_dbg("Charger disconnect intrrupt in HOST mode\n");
		} else if (OTG_DEV_SUSPEND == p->status) {
			usb_dbg("Charger disconnect intrrupt in SUSPEND mode\n");
		}
		break;

	case ID_FALL_EVENT:
		if ((OTG_DEV_OFF == p->status) || (OTG_DEV_SUSPEND == p->status)) {
			unsigned int count = 0;

			if (hw_setup(p, 1)) {
				goto error;
			}

			if (dwc_otg_dev->core_if->core_params->dma_desc_enable) {
				usb_dbg("disable desc dma\n");
				dwc_otg_dev->core_if->core_params->dma_desc_enable = 0;
			}
			dwc_otg_disable_global_interrupts(dwc_otg_dev->core_if);
			while (!dwc_otg_is_host_mode(dwc_otg_dev->core_if)) {
				msleep(10);
				if(++count > 100) {
					printk(KERN_ERR "wait host mode timeout\n");
					goto error;
				}
			}
			dwc_otg_core_init(dwc_otg_dev->core_if);

			msleep(100);
			if (dwc_otg_is_host_mode(dwc_otg_dev->core_if)) {
				dwc_otg_dev->core_if->op_state = A_HOST;
				usb_dbg("dwc otg is host mode.\n");
			} else {
				usb_dbg("error in device mode!\n");
			}

			cil_hcd_start(dwc_otg_dev->core_if);
			dwc_otg_hi3630_set_prtpower(1);
			p->status = OTG_DEV_HOST;

			usb_dbg("hisi usb_status: OFF -> HOST\n");
		} else if (OTG_DEV_DEVICE == p->status) {
			usb_dbg("hisi usb_status: DEVICE -> HOST\n");
			p->status = OTG_DEV_HOST;
		} else if (OTG_DEV_HOST == p->status) {
			usb_dbg("ID fall event, already in host mode\n");
		}
		break;
	case ID_RISE_EVENT:
		if (OTG_DEV_HOST == p->status) {

			extern atomic_t dwc_otg_hcd_bus_state;
			while (0 != atomic_read(&dwc_otg_hcd_bus_state)) {
				msleep(200);
				usb_dbg("wait for dwc_otg_bus_suspend\n");
			}

			dwc_otg_disable_global_interrupts(dwc_otg_dev->core_if);
			dwc_otg_mask_all_interrupts(dwc_otg_dev->core_if);
			dwc_otg_clear_all_interrupts(dwc_otg_dev->core_if);
			hw_shutdown(p);
			p->status = OTG_DEV_OFF;

			dwc_otg_hi3630_set_prtpower(0);
#ifndef FORCE_ID_STATE
#if defined (CONFIG_SWITCH_FSA9685)
			{
				extern int fsa9685_manual_detach(void);
				fsa9685_manual_detach();
			}
#endif
#endif

			usb_dbg("hiusb_status: HOST -> OFF\n");
		} else if (OTG_DEV_DEVICE == p->status) {
			usb_dbg("hiusb_status: HOST -> DEVICE\n");
		} else if (OTG_DEV_OFF == p->status) {
			usb_dbg("ID rise event, Already in OFF mode\n");
		} else if (OTG_DEV_SUSPEND == p->status) {
			usb_dbg("ID rise in SUSPEND mode\n");
			p->status = OTG_DEV_OFF;
		}
		break;
	default:
		usb_dbg("illegal event type!\n");
		break;
	}

error:
	usb_dbg("-\n");
	mutex_unlock(&p->lock);
	return;
}

static int event_type_check(enum otg_dev_event_type last_event,
		enum otg_dev_event_type current_event)
{
	int ret = 0;

	if (last_event == NONE_EVENT)
		return 1;

	switch (current_event) {
	case CHARGER_CONNECT_EVENT:
		if ((last_event == CHARGER_DISCONNECT_EVENT)
				|| (last_event == ID_RISE_EVENT))
			ret = 1;
		break;
	case CHARGER_DISCONNECT_EVENT:
		if (last_event == CHARGER_CONNECT_EVENT)
			ret = 1;
		break;
	case ID_FALL_EVENT:
		if ((last_event == CHARGER_DISCONNECT_EVENT)
				|| (last_event == ID_RISE_EVENT))
			ret = 1;
		break;
	case ID_RISE_EVENT:
		if (last_event == ID_FALL_EVENT)
			ret = 1;
		break;
	default:
		break;
	}
	return ret;
}

int hisi_usb_otg_event(enum otg_dev_event_type event)
{
	struct otg_dev *dev_p = otg_dev_p;
	unsigned long flags;
	int ret = 0;

	if (0 == atomic_read(&is_event_work_ready)) {
		return 0;
	}

	spin_lock_irqsave(&(dev_p->event_lock), flags);
	if (event_type_check(dev_p->event, event)) {
		usb_dbg("event: %d\n", event);
		dev_p->event = event;

		if (CHARGER_CONNECT_EVENT == event)
			dwc_otg_hi3630_wake_lock();

		ret = schedule_delayed_work(&dev_p->event_work, 0);
	}
	spin_unlock_irqrestore(&(dev_p->event_lock), flags);
	return ret;
}

void dwc_otg_hi3630_wake_lock(void)
{
	if (!wake_lock_active(&otg_dev_p->wake_lock)) {
		usb_dbg("usb otg wake lock\n");
		wake_lock(&otg_dev_p->wake_lock);
	}
}

void dwc_otg_hi3630_wake_unlock(void)
{
	if (wake_lock_active(&otg_dev_p->wake_lock)) {
		usb_dbg("usb otg wake unlock\n");
		wake_unlock(&otg_dev_p->wake_lock);
	}
}

int dwc_otg_hi3630_pullup(struct usb_gadget *gadget, int is_on)
{
	dwc_otg_device_t *dwc_otg_dev;

	if (gadget == 0) {
		return -ENODEV;
	}

	dwc_otg_dev = lm_get_drvdata(otg_dev_p->lm_dev);

	/* lock */
	mutex_lock(&otg_dev_p->lock);

	if (is_on) {
		otg_dev_p->gadget_initialized = 1;
	} else {
		otg_dev_p->gadget_initialized = 0;
	}

	if ((OTG_DEV_DEVICE == otg_dev_p->status)) {
		dwc_otg_pcd_pullup(dwc_otg_dev->pcd, is_on);
	}

	/* unlock */
	mutex_unlock(&otg_dev_p->lock);
	return 0;
}

int dwc_otg_hi3630_probe_stage2(void)
{
	struct otg_dev * otg_device = otg_dev_p;
	dwc_otg_device_t *dwc_otg_dev = lm_get_drvdata(otg_device->lm_dev);

	usb_dbg("[%s]+\n", __func__);

	mutex_lock(&otg_device->lock);

	atomic_set(&is_event_work_ready, 1);
	usb_dbg("is_event_work_ready: %d\n", atomic_read(&is_event_work_ready));

	if (dwc_otg_is_host_mode(dwc_otg_dev->core_if)) {
		usb_dbg("host mode\n");
		if (dwc_otg_dev->core_if->core_params->dma_desc_enable) {
		        usb_dbg("disable desc dma\n");
		        dwc_otg_dev->core_if->core_params->dma_desc_enable = 0;
			/* reinit controller with buffer dma */
			dwc_otg_core_init(dwc_otg_dev->core_if);
			cil_hcd_start(dwc_otg_dev->core_if);
			dwc_otg_hi3630_set_prtpower(1);
		}
		otg_device->status = OTG_DEV_HOST;
	} else if (0 == is_usb_cable_connected()) {
		usb_dbg("USB cable disconnected, hw shutdown.\n");
		hw_shutdown(otg_device);
		otg_device->status = OTG_DEV_OFF;
	} else {
		usb_dbg("USB cable connected.\n");

		/*
		 * detect charger type
		 */
		detect_charger_type(otg_device);
		notify_charger_type(otg_device);

		/*
		 * detect charger type will cause controller abnormal
		 * so, we need initialize it again!
		 */
		dwc_otg_disable_global_interrupts(dwc_otg_dev->core_if);
		dwc_otg_core_init(dwc_otg_dev->core_if);
		if (dwc_otg_is_device_mode(dwc_otg_dev->core_if)) {
			cil_pcd_start(dwc_otg_dev->core_if);
			dwc_otg_dev->core_if->op_state = B_PERIPHERAL;
		}

		dwc_otg_pcd_disconnect_us(dwc_otg_dev->pcd, 50);
		udelay(100);

		if (otg_device->gadget_initialized) {
			usb_dbg("gadget is initialized\n");
			dwc_otg_pcd_connect_us(dwc_otg_dev->pcd, 50);
		}

		usb_dbg("enable global interrupts\n");
		dwc_otg_enable_global_interrupts(dwc_otg_dev->core_if);
		otg_device->status = OTG_DEV_DEVICE;

		/* do not go to sleep when USB connected */
		dwc_otg_hi3630_wake_lock();
	}

	mutex_unlock(&otg_device->lock);

	usb_dbg("[%s]-\n", __func__);
	return 0;
}

static int dwc_otg_hi3630_probe(struct platform_device *pdev)
{
	int ret = 0;
	int irq = 0;
	struct lm_device *lm_dev;
	struct otg_dev *dev_p;
	struct resource *res;

	usb_dbg("[%s]+\n", __func__);

	dev_p = devm_kzalloc(&pdev->dev, sizeof(*dev_p), GFP_KERNEL);
	if (!dev_p) {
		usb_err("alloc otg_dev failed! not enough memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, dev_p);
	dev_p->pdev = pdev;
	otg_dev_p = dev_p;

	dev_p->event = NONE_EVENT;
	dev_p->charger_type = CHARGER_TYPE_NONE;
	dev_p->status = OTG_DEV_OFF;
	dev_p->hcd_status = HCD_OFF;
	dev_p->gadget_initialized = 0;
	dev_p->dwc_otg_irq_enabled = true;
	dev_p->is_regu_on = 0;

	INIT_DELAYED_WORK(&dev_p->event_work, event_work);
	wake_lock_init(&dev_p->wake_lock, WAKE_LOCK_SUSPEND, "usb_wake_lock");
	spin_lock_init(&dev_p->event_lock);
	mutex_init(&dev_p->lock);
	ATOMIC_INIT_NOTIFIER_HEAD(&dev_p->charger_type_notifier);

	ret = get_resource(dev_p);
	if (ret) {
		usb_err("get resource failed!\n");
		return ret;
	}
	ret = hw_setup(dev_p, 0);
	if (ret) {
		usb_err("hw setup failed!\n");
		put_resource(dev_p);
		return ret;
	}
	ret = create_attr_file(&pdev->dev);
	if (ret) {
		usb_err("create_attr_file failed!\n");
		goto error;
	}

	/*
	 * register the dwc otg device
	 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		usb_err("get resource failed!\n");
		ret = -EBUSY;
		goto error;
	}

	usb_dbg("res start: 0x%x, res end: 0x%x, res flags: %ld\n",
		res->start, res->end, res->flags);

	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		usb_err("get irq failed!\n");
		ret = -EBUSY;
		goto error;
	}

	usb_dbg("res irq: %d\n", irq);

	lm_dev = devm_kzalloc(&pdev->dev, sizeof(*lm_dev), GFP_KERNEL);
	if (!lm_dev) {
		usb_err("alloc lm_dev failed! not enough memory\n");
		ret = -ENOMEM;
		goto error;
	}

	dev_p->lm_dev = lm_dev;

	lm_dev->resource.start = res->start;
	lm_dev->resource.end = res->end;
	lm_dev->resource.flags = res->flags;
	lm_dev->irq = irq;
	lm_dev->id = -1;
	lm_dev->dev.init_name = "hisi-usb-otg";
	//lm_dev->dev.parent = &pdev->dev;

	ret = lm_device_register(lm_dev);
	if (ret) {
		usb_err("register dwc otg device failed\n");
		goto error;
	}
	usb_dbg("register dwc otg device done\n");

	usb_dbg("[%s]-\n", __func__);

	return 0;
error:
	hw_shutdown(dev_p);
	put_resource(dev_p);
	return ret;

}

static int dwc_otg_hi3630_remove(struct platform_device *pdev)
{
	struct otg_dev *dev_p = platform_get_drvdata(pdev);

	if (!dev_p)
        return 0;

	if ((dev_p->status == OTG_DEV_DEVICE) || (dev_p->status == OTG_DEV_HOST)) {
		hw_shutdown(dev_p);
	}
	put_resource(dev_p);
	return 0;
}

static int dwc_otg_hi3630_suspend(struct device *dev)
{
	struct otg_dev *dev_p = dev_get_drvdata(dev);
	usb_dbg("\n");
	mutex_lock(&dev_p->lock);
	return 0;
}

static int dwc_otg_hi3630_resume(struct device *dev)
{
	struct otg_dev *dev_p = dev_get_drvdata(dev);
	usb_dbg("\n");
	mutex_unlock(&dev_p->lock);
	return 0;
}

#ifdef CONFIG_PM
struct dev_pm_ops dwc_otg_dev_pm_ops = {
	.prepare = NULL,
	.complete = NULL,
	.suspend = dwc_otg_hi3630_suspend,
	.resume = dwc_otg_hi3630_resume,

	.runtime_suspend = NULL,
	.runtime_resume = NULL,
	.runtime_idle = NULL,
};
#define dwc_otg_dev_pm_ops_p (&dwc_otg_dev_pm_ops)
#else
#define dwc_otg_dev_pm_ops_p NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id dwc_otg_hi3630_mach[] = {
	{ .compatible = "hisilicon,hi3630-usb-otg" },
	{},
};
MODULE_DEVICE_TABLE(of, dwc_otg_hi3630_mach);
#else
#define dwc_otg_hi3630_mach NULL
#endif

static struct platform_driver dwc_otg_hi3630_driver = {
	.probe		= dwc_otg_hi3630_probe,
	.remove		= dwc_otg_hi3630_remove,
	.driver		= {
		.name	= "hi3630_otg",
		.of_match_table = of_match_ptr(dwc_otg_hi3630_mach),
		.pm = dwc_otg_dev_pm_ops_p,
	},
};

module_platform_driver(dwc_otg_hi3630_driver);

MODULE_AUTHOR("l00196665");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("DesignWare OTG2.0 hi3630 Glue Layer");

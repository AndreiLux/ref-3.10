#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/syscore_ops.h>

#include "hisi_noc.h"
#include "hisi_err_probe.h"
#include "hisi_noc_transcation.h"
#include "hisi_noc_packet.h"

#define MODULE_NAME	"hisi_noc"

struct hisi_noc_device {
	struct device		*dev;
	void __iomem		*pctrl_base;
	void __iomem		*pmctrl_base;
	void __iomem		*pwrctrl_reg;
	unsigned int		irq;
#ifdef CONFIG_HISI_NOC_TIMEOUT
	unsigned int		timeout_irq;
#endif
};

#define MAX_NODES_NR	33
struct noc_node *nodes_array[MAX_NODES_NR] = {NULL};
unsigned int nodes_array_idx = 0;

enum NOC_IRQ_TPYE {
	NOC_ERR_PROBE_IRQ,
	NOC_PACKET_PROBE_IRQ,
	NOC_TRANS_PROBE_IRQ,
};

struct hisi_noc_irqd {
	enum NOC_IRQ_TPYE type;
	struct noc_node *node;
};
struct hisi_noc_irqd noc_irqdata[NOC_MAX_IRQ_NR];

#include "hisi_noc_bus.c"

#ifdef CONFIG_HISI_BALONG_MODEM
extern void bsp_modem_error_handler(u32  p1,  void* p2, void* p3, void* p4);
#endif
extern void regulator_address_info_list(void);

#ifdef CONFIG_HISI_NOC_TIMEOUT
#define PMCTRL_PERI_INT_STAT0_OFFSET	0x3A4
static irqreturn_t hisi_noc_timeout_irq_handler(int irq, void *data)
{
	void __iomem *pmctrl_base;
	struct hisi_noc_device *noc_dev = (struct hisi_noc_device *)data;
	unsigned int pending;

	pmctrl_base = noc_dev->pmctrl_base;

	pending = readl_relaxed(pmctrl_base + PMCTRL_PERI_INT_STAT0_OFFSET);
	if (pending)
		pr_err("NoC timeout IRQ occured, PERI_INT_STAT0 = 0x%x\n", pending);
	else
		pr_err("PMCTRL other interrupt source occurs! \n");

	return IRQ_HANDLED;
}
#endif

static irqreturn_t hisi_noc_irq_handler(int irq, void *data)
{
	unsigned long pending;
	int offset;
	void __iomem *pctrl_base;
#ifdef CONFIG_HISI_NOC_ERR_PROBE
	struct noc_node *node;
	void __iomem *porbe_base;
#endif

	struct hisi_noc_device *noc_dev = (struct hisi_noc_device *)data;
	pctrl_base = noc_dev->pctrl_base;

#ifdef CONFIG_HISI_BALONG_MODEM
	bsp_modem_error_handler(0,(void*)0,(void*)0,(void*)0);
#endif
	regulator_address_info_list();

	pending = readl_relaxed(pctrl_base + PCTRL_NOC_IRQ_STAT1);
	if (pending) {
		for_each_set_bit(offset, &pending, BITS_PER_LONG) {
			node = noc_irqdata[offset].node;
			porbe_base = node->eprobe_offset + node->base;

			switch (noc_irqdata[offset].type) {
			case NOC_ERR_PROBE_IRQ:
#ifdef CONFIG_HISI_NOC_ERR_PROBE
				pr_err("NoC Error Probe:\n");
				pr_err("noc_bus: %s\n", noc_buses_info[node->bus_id].name);
				pr_err("noc_node: %s\n", node->name);

				noc_err_probe_hanlder(porbe_base, node->bus_id);
#endif
				break;
			case NOC_PACKET_PROBE_IRQ:
#ifdef CONFIG_HISI_NOC_ERR_PROBE
				/* FixMe: packet probe irq handler */
				pr_err("NoC PACKET Probe:\n");
				pr_err("noc_bus: %s\n", noc_buses_info[node->bus_id].name);
				pr_err("noc_node: %s\n", node->name);

				noc_packet_probe_hanlder(node, porbe_base, node->bus_id);
#endif
				break;
			case NOC_TRANS_PROBE_IRQ:
#ifdef CONFIG_HISI_NOC_ERR_PROBE
				/* FixMe: trans probe irq handler */
				pr_err("NoC TRANSCATION Probe:\n");
				pr_err("noc_bus: %s\n", noc_buses_info[node->bus_id].name);
				pr_err("noc_node: %s\n", node->name);

				noc_transcation_probe_hanlder(node, porbe_base, node->bus_id);
#endif
				break;
			default:
				pr_err("NoC IRQ type is wrong!\n");

			}
		}
	}

	pending = readl_relaxed(pctrl_base + PCTRL_NOC_IRQ_STAT2);
	if ((pending & BIT(0)) != 0) {
#ifdef CONFIG_HISI_NOC_ERR_PROBE
		/* FixMe: packet probe irq handler */
		node = noc_irqdata[32].node;
		pr_err("NoC PACKET Probe:\n");
		pr_err("noc_bus: %s\n", noc_buses_info[node->bus_id].name);
		pr_err("noc_node: %s\n", node->name);

		porbe_base = node->eprobe_offset + node->base;
		noc_packet_probe_hanlder(node, porbe_base, node->bus_id);
#endif
	}

	return IRQ_HANDLED;
}

static int find_bus_id_by_name(const char *bus_name)
{
	int i;

	for (i = 0; i < MAX_NOC_BUSES_NR; i++) {
		if (strcmp(bus_name, noc_buses_info[i].name) == 0)
			return i;
	}
	return -ENODEV;
}

static int  register_noc_node(struct device_node *np)
{
	struct noc_node *node;
	int ret = 0;
	const char *bus_name;

	node = kzalloc(sizeof(struct noc_node), GFP_KERNEL);
	if (!node) {
		pr_err("fail to alloc memory, noc_node=%s!\n", np->name);
		ret = -ENOMEM;
		goto err;
	}

	node->base = of_iomap(np, 0);
	if (!node->base) {
		WARN_ON(1);
		goto err_iomap;
	}

	/* err probe property */
#ifdef CONFIG_HISI_NOC_ERR_PROBE
	if (of_property_read_bool(np, "eprobe-autoenable"))
		node->eprobe_autoenable = true;

	ret = of_property_read_u32(np, "eprobe-hwirq", &node->eprobe_hwirq);
	if (ret < 0) {
		node->eprobe_hwirq = -1;
		pr_debug("the node doesnot have err probe!\n");
	}

	if (node->eprobe_hwirq >= 0) {
		ret = of_property_read_u32(np, "eprobe-offset", &node->eprobe_offset);
		if (ret < 0)
			node->eprobe_hwirq = -1;
			pr_debug("the node doesnot have err probe!\n");
	}
#endif
	ret = of_property_read_u32(np, "pwrack-bit", &node->pwrack_bit);
	if (ret < 0) {
		node->pwrack_bit = -1;
		pr_debug("the node doesnot have pwrack_bit property!\n");
		ret = -ENODEV;
		goto err_iomap;
	}

	node->name = kstrdup(np->name, GFP_KERNEL);
	if (!node->name) {
		ret = -ENOMEM;
		goto err_iomap;
	}

	ret = of_property_read_string(np, "bus-name", &bus_name);
	if (ret < 0) {
		WARN_ON(1);
		goto err_property;
	}

	ret = find_bus_id_by_name(bus_name);
	if (ret < 0) {
		WARN_ON(1);
		goto err_property;
	}
	node->bus_id = ret;

	/* FIXME: handle the transprobe & packet probe property */
	/* Debug info */
#ifdef HISI_NOC_DEBUG
	pr_debug("[%s]: nodes_array_idx = %d\n", __func__, nodes_array_idx);
	pr_debug("np->name = %s\n", np->name);
	pr_debug("node->base = 0x%p\n", node->base);
	pr_debug("node->eprobe_hwirq = %d\n", node->eprobe_hwirq);
	pr_debug("node->eprobe_offset = 0x%x\n", node->eprobe_offset);
#endif

	/* put the node into nodes_arry */
	nodes_array[nodes_array_idx] = node;
	nodes_array_idx++;

	/* FIXME: handle the other irq */

	return 0;

err_property:
	kfree(node->name);
	iounmap(node->base);
err_iomap:
	kfree(node);
err:
	return ret;
}

static void register_irqdata(void)
{
	struct noc_node *node;
	unsigned int i;

	for (i = 0; i < nodes_array_idx; i++) {
		node = nodes_array[i];
		if (!node) {
			pr_err("[%s]: nodes_array index %d not found.\n", __func__, i);
			continue;
		}

		if ((node->eprobe_hwirq >= 0) && (node->eprobe_hwirq <= 9)) {
			noc_irqdata[node->eprobe_hwirq].type =  NOC_ERR_PROBE_IRQ;
			noc_irqdata[node->eprobe_hwirq].node = node;
		} else if ((node->eprobe_hwirq >= 10) && (node->eprobe_hwirq <= 26)) {
			noc_irqdata[node->eprobe_hwirq].type =  NOC_TRANS_PROBE_IRQ;
			noc_irqdata[node->eprobe_hwirq].node = node;
			init_transcation_info(node);
		} else if ((node->eprobe_hwirq >= 27) && (node->eprobe_hwirq <= 32)) {
			noc_irqdata[node->eprobe_hwirq].type =  NOC_PACKET_PROBE_IRQ;
			noc_irqdata[node->eprobe_hwirq].node = node;
			init_packet_info(node);
		} else {
			pr_err("[%s]: the node type error!!!\n", __func__);
		}
	}
}

static void register_noc_nodes(void)
{
	struct device_node *np;

	for_each_compatible_node(np, NULL, "hisilicon,hi3630-noc-node") {
		register_noc_node(np);
	}

	/* register_irqdata */
	register_irqdata();
}

static void unregister_noc_nodes(void)
{
	struct noc_node *node;
	int i;

	for (i = 0; i < nodes_array_idx; i++) {
		node = nodes_array[i];
		if (!node) {
			pr_err("[%s]: nodes_array index %d not found.\n", __func__, i);
			continue;
		}
#ifdef CONFIG_HISI_NOC_ERR_PROBE
		if ((node->eprobe_hwirq >= 0) && (node->eprobe_hwirq <= 9)) {
			disable_err_probe(node->base + node->eprobe_offset);
		} else if ((node->eprobe_hwirq >= 10) && (node->eprobe_hwirq <= 26)) {
			disable_transcation_probe(node->base + node->eprobe_offset);
		} else if ((node->eprobe_hwirq >= 27) && (node->eprobe_hwirq <= 32)) {
			disable_packet_probe(node->base + node->eprobe_offset);
		} else {
			pr_err("[%s]: the node type error!!!\n", __func__);
		}
#endif
		iounmap(node->base);
		kfree(node);
	}

	nodes_array_idx = 0;
}


#ifdef CONFIG_HISI_NOC_ERR_PROBE
void __iomem * get_errprobe_base(const char *name)
{
	struct noc_node *node;
	int i;

	for (i = 0; i < nodes_array_idx; i++) {
		node = nodes_array[i];
		if (!node) {
			pr_err("[%s]: nodes_array index %d not found.\n", __func__, i);
			continue;
		}

		if (!strcmp(name, node->name))
			return node->base + node->eprobe_offset;
	}

	pr_warn("[%s]  cannot get node base name = %s\n", __func__, name);
	return NULL;
}
EXPORT_SYMBOL(get_errprobe_base);

struct noc_node * get_probe_node(const char *name)
{
	struct noc_node *node;
	int i;

	for (i = 0; i < nodes_array_idx; i++) {
		node = nodes_array[i];
		if (!node) {
			pr_err("[%s]: nodes_array index %d not found.\n", __func__, i);
			continue;
		}

		if (!strcmp(name, node->name))
			return node;
	}

	pr_warn("[%s]  cannot get node base name = %s\n", __func__, name);
	return NULL;
}
EXPORT_SYMBOL(get_probe_node);

static void enable_errprobe(struct device *dev)
{
	struct noc_node *node;
	unsigned int i;
	struct platform_device *pdev = to_platform_device(dev);
	struct hisi_noc_device *noc_dev = platform_get_drvdata(pdev);
	unsigned long status = readl_relaxed(noc_dev->pwrctrl_reg);

	for (i = 0; i < nodes_array_idx; i++) {
		node = nodes_array[i];
		if (!node) {
			pr_err("[%s]: nodes_array index %d not found.\n", __func__, i);
			continue;
		}

		if ((node->eprobe_hwirq >= 0) && (node->eprobe_hwirq <= 9) && (node->eprobe_autoenable) && !(status & (1 << node->pwrack_bit)))
			enable_err_probe(node->base + node->eprobe_offset);
	}
}
static void disable_errprobe(struct device *dev)
{
	struct noc_node *node;
	unsigned int i;
	struct platform_device *pdev = to_platform_device(dev);
	struct hisi_noc_device *noc_dev = platform_get_drvdata(pdev);
	unsigned long status = readl_relaxed(noc_dev->pwrctrl_reg);

	for (i = 0; i < nodes_array_idx; i++) {
		node = nodes_array[i];
		if (!node) {
			pr_err("[%s]: nodes_array index %d not found.\n", __func__, i);
			continue;
		}

		if ((node->eprobe_hwirq >= 0) && (node->eprobe_hwirq <= 9) && (node->eprobe_autoenable) && !(status & (1 << node->pwrack_bit)))
			disable_err_probe(node->base + node->eprobe_offset);
	}
}
#ifdef DEFAULT_ENABLE_NOC_PROBE
static void enable_noc_transcation_probe(struct device *dev)
{
	struct noc_node *node;
	unsigned int i;
	struct platform_device *pdev = to_platform_device(dev);
	struct hisi_noc_device *noc_dev = platform_get_drvdata(pdev);
	unsigned long status = readl_relaxed(noc_dev->pwrctrl_reg);

	for (i = 0; i < nodes_array_idx; i++) {
		node = nodes_array[i];
		if (!node) {
			pr_err("[%s]: nodes_array index %d not found.\n", __func__, i);
			continue;
		}

		if ((node->eprobe_hwirq >= 10) && (node->eprobe_hwirq <= 26) && (node->eprobe_autoenable) && !(status & (1 << node->pwrack_bit)))
			enable_transcation_probe(node, node->base + node->eprobe_offset);
	}
}

static void disable_noc_transcation_probe(struct device *dev)
{
	struct noc_node *node;
	unsigned int i;
	struct platform_device *pdev = to_platform_device(dev);
	struct hisi_noc_device *noc_dev = platform_get_drvdata(pdev);
	unsigned long status = readl_relaxed(noc_dev->pwrctrl_reg);

	for (i = 0; i < nodes_array_idx; i++) {
		node = nodes_array[i];
		if (!node) {
			pr_err("[%s]: nodes_array index %d not found.\n", __func__, i);
			continue;
		}

		if ((node->eprobe_hwirq >= 10) && (node->eprobe_hwirq <= 26) && (node->eprobe_autoenable) && !(status & (1 << node->pwrack_bit)))
			disable_transcation_probe(node->base + node->eprobe_offset);
	}
}

static void enable_noc_packet_probe(struct device *dev)
{
	struct noc_node *node;
	unsigned int i;
	struct platform_device *pdev = to_platform_device(dev);
	struct hisi_noc_device *noc_dev = platform_get_drvdata(pdev);
	unsigned long status = readl_relaxed(noc_dev->pwrctrl_reg);

	for (i = 0; i < nodes_array_idx; i++) {
		node = nodes_array[i];
		if (!node) {
			pr_err("[%s]: nodes_array index %d not found.\n", __func__, i);
			continue;
		}

		if ((node->eprobe_hwirq >= 27) && (node->eprobe_hwirq <= 32) && (node->eprobe_autoenable) && !(status & (1 << node->pwrack_bit)))
			enable_packet_probe(node, node->base + node->eprobe_offset);
	}
}

static void disable_noc_packet_probe(struct device *dev)
{
	struct noc_node *node;
	unsigned int i;
	struct platform_device *pdev = to_platform_device(dev);
	struct hisi_noc_device *noc_dev = platform_get_drvdata(pdev);
	unsigned long status = readl_relaxed(noc_dev->pwrctrl_reg);

	for (i = 0; i < nodes_array_idx; i++) {
		node = nodes_array[i];
		if (!node) {
			pr_err("[%s]: nodes_array index %d not found.\n", __func__, i);
			continue;
		}

		if ((node->eprobe_hwirq >= 27) && (node->eprobe_hwirq <= 32) && (node->eprobe_autoenable) && !(status & (1 << node->pwrack_bit)))
			disable_packet_probe(node->base + node->eprobe_offset);
	}
}
#endif
#else
static void enable_errprobe(struct device *dev)
{
}

static void disable_errprobe(struct device *dev)
{
}
#ifdef DEFAULT_ENABLE_NOC_PROBE
static void enable_noc_transcation_probe(struct device *dev)
{
}

static void disable_noc_transcation_probe(struct device *dev)
{
}

static void enable_noc_packet_probe(struct device *dev)
{
}

static void disable_noc_packet_probe(struct device *dev)
{
}
#endif
#endif

#ifdef CONFIG_PM_SLEEP
static int hisi_noc_suspend(struct device *dev)
{
	pr_err("%s+\n", __func__);
#ifdef CONFIG_HISI_NOC_ERR_PROBE
	disable_errprobe(dev);
#ifdef DEFAULT_ENABLE_NOC_PROBE
	disable_noc_transcation_probe(dev);
	disable_noc_packet_probe(dev);
#endif
#endif
	pr_err("%s-\n", __func__);

	return 0;
}


static int hisi_noc_resume(struct device *dev)
{
	pr_err("%s+\n", __func__);
#ifdef CONFIG_HISI_NOC_ERR_PROBE
	enable_errprobe(dev);
#ifdef DEFAULT_ENABLE_NOC_PROBE
	enable_noc_transcation_probe(dev);
	enable_noc_packet_probe(dev);
#endif
#endif
	pr_err("%s-\n", __func__);

	return 0;
}

static SIMPLE_DEV_PM_OPS(noc_pm_ops, hisi_noc_suspend, hisi_noc_resume);
#endif

static int hisi_noc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hisi_noc_device *noc_dev;
	struct device_node *np;
	u32 offset;
	int ret;

	noc_dev = devm_kzalloc(&pdev->dev, sizeof(struct hisi_noc_device), GFP_KERNEL);
	if (!noc_dev) {
		dev_err(dev, "cannot get memory\n");
		ret = -ENOMEM;
		goto err;
	}

	platform_set_drvdata(pdev, noc_dev);

	np = of_find_node_by_name(NULL, "pctrl");
	BUG_ON(!np);

	noc_dev->pctrl_base = of_iomap(np, 0);
	BUG_ON(!noc_dev->pctrl_base);

	np = of_find_compatible_node(NULL, NULL, "hisilicon,pmctrl");

	noc_dev->pmctrl_base = of_iomap(np, 0);
	if (!noc_dev->pmctrl_base) {
		BUG_ON(1);
		goto err_pmctrl;
	}

	/* get the pwrctrl_reg offset  */
	np = dev->of_node;
	ret = of_property_read_u32(np, "pwrack-reg", &offset);
	if (ret < 0) {
		pr_err("cannot found pwrack-reg property!\n");
		goto err_irq;
	}
	noc_dev->pwrctrl_reg = noc_dev->pmctrl_base + offset;

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "cannot find IRQ\n");
		goto err_irq;
	}
	noc_dev->irq = ret;

	ret = devm_request_irq(&pdev->dev, noc_dev->irq, hisi_noc_irq_handler,
		IRQF_TRIGGER_HIGH | IRQF_DISABLED, "hisi_noc", noc_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "request_irq fail!\n");
		goto err_irq;
	}

#ifdef CONFIG_HISI_NOC_TIMEOUT
	ret = platform_get_irq(pdev, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "cannot find noc timeout IRQ\n");
		goto err_irq;
	}
	noc_dev->timeout_irq = ret;
	pr_debug("[%s] timeout_irq = %d\n", __func__, noc_dev->timeout_irq);

	ret = devm_request_irq(&pdev->dev, noc_dev->timeout_irq, hisi_noc_timeout_irq_handler,
		IRQF_TRIGGER_HIGH | IRQF_DISABLED, "hisi_noc_timeout_irq", noc_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "request timeout irq fail!\n");
		goto err_irq;
	}
#endif

	/* process each noc node */
	register_noc_nodes();

	/* enable err probe */
	enable_errprobe(dev);

#ifdef DEFAULT_ENABLE_NOC_PROBE
	/* enable noc transcation probe*/
	enable_noc_transcation_probe(dev);

	/* enable noc packet probe */
	enable_noc_packet_probe(dev);
#endif

	return 0;

err_irq:
	iounmap(noc_dev->pmctrl_base);
err_pmctrl:
	iounmap(noc_dev->pctrl_base);
err:
	return ret;
}

static int hisi_noc_remove(struct platform_device *pdev)
{
	struct hisi_noc_device *noc_dev = platform_get_drvdata(pdev);

	unregister_noc_nodes();

	free_irq(noc_dev->irq, noc_dev);
	iounmap(noc_dev->pctrl_base);

	return 0;
}


#ifdef CONFIG_OF
static const struct of_device_id hisi_noc_match[] = {
	{ .compatible = "hisilicon,hi3630-noc" },
	{},
};
MODULE_DEVICE_TABLE(of, hisi_noc_match);
#endif

static struct platform_driver hisi_noc_driver = {
	.probe		= hisi_noc_probe,
	.remove		= hisi_noc_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm	= &noc_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(hisi_noc_match),
#endif
	},
};

static int __init hisi_noc_init(void)
{
	return platform_driver_register(&hisi_noc_driver);
}

static void __exit hisi_noc_exit(void)
{
	platform_driver_unregister(&hisi_noc_driver);
}

fs_initcall(hisi_noc_init);
module_exit(hisi_noc_exit);

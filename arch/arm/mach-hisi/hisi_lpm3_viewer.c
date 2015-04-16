#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>

#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/ctype.h>

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#define MODULE_NAME					"hisilicon,lpm3_viewer"
#define WORD_REF(address)			(* ((unsigned int *) (address)))
#define AP_LOG_CACHE_SIZE			(1024)
#define M3_LOG_CACHE_SIZE			(0x1FC00)
#define M3_LOG_SIZE_MASK			(0x1FC00)
#define M3_LOG_INFO_OFFSET			(0x1FC00 + 0x50)
#define M3_SIM_LOG_OFFSET			(0x1FD10)
#define MAX_SIM_STATUS				(4)
#define MAX_SIM_LOG					(50)

char *sim_state_str[MAX_SIM_STATUS+1] = {"      out", "   in pos", "       in", "leave pos", "    undef"};

struct lpm3_log_info {
	unsigned int wp;
	unsigned int reserved;
	unsigned int flags;
};

struct acpu_log_cache {
	char *data;
	unsigned int index;
};

struct hisi_lpm3_viewer {
	char *log_base;
	unsigned int rp;
	unsigned int period_time;
	unsigned int sim_log_idx;
	struct acpu_log_cache cache;
	struct lpm3_log_info *info;
	struct platform_device *pdev;
	struct delayed_work show_log_work;
};

typedef struct {
	u32 time;
	u8 sim_no;
	u8 sim_status;
	u8 hpd_level;
	u8 det_level;
	u8 trace;
	u8 sim_mux[3];
} sim_log;

static ssize_t show_period(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct platform_device *pdev = (struct platform_device *)container_of(dev, struct platform_device, dev);
	struct hisi_lpm3_viewer *viewer = (struct hisi_lpm3_viewer *)platform_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE - 2,"period = %us\n", viewer->period_time / 1000);
}

static ssize_t store_period(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct platform_device *pdev = (struct platform_device *)container_of(dev, struct platform_device, dev);
	struct hisi_lpm3_viewer *viewer = (struct hisi_lpm3_viewer *)platform_get_drvdata(pdev);
	unsigned int time_val;
	unsigned int ret;
	unsigned int prev_period = viewer->period_time;

	ret = sscanf(buf, "%u", &time_val);
	if (ret != 1)
		return -EINVAL;

	viewer->period_time = time_val * 1000;

	if (prev_period)
		cancel_delayed_work(&viewer->show_log_work);

	if (viewer->period_time)
		schedule_delayed_work(&viewer->show_log_work,
				msecs_to_jiffies(viewer->period_time));

	return count;
}

static DEVICE_ATTR(period, 0644, show_period, store_period);

static ssize_t show_console(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct platform_device *pdev = (struct platform_device *)container_of(dev, struct platform_device, dev);
	struct hisi_lpm3_viewer *viewer = (struct hisi_lpm3_viewer *)platform_get_drvdata(pdev);

	char *base = viewer->log_base;
	unsigned int index = viewer->info->wp;
	unsigned int flags = viewer->info->flags;
	ssize_t retval;

	if (index >= PAGE_SIZE) {
		retval = snprintf(buf, PAGE_SIZE, "%s", base + index - PAGE_SIZE);
	} else {
		if (flags & BIT(3)) {
			retval = snprintf(buf, PAGE_SIZE - index, "%s", base + M3_LOG_CACHE_SIZE - PAGE_SIZE + index);
			retval += snprintf(buf + retval, index, "%s", base);
		} else {
			retval = snprintf(buf, index, "%s", base);
		}
	}

	buf[PAGE_SIZE - 2] = '\n';
	buf[PAGE_SIZE - 1] = '\0';

	return retval;
}

static DEVICE_ATTR(console, 0444, show_console, NULL);

static ssize_t show_sim(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct platform_device *pdev = (struct platform_device *)container_of(dev, struct platform_device, dev);
	struct hisi_lpm3_viewer *viewer = (struct hisi_lpm3_viewer *)platform_get_drvdata(pdev);

	ssize_t retval = 0;
	unsigned int i, idx = viewer->sim_log_idx;
	sim_log *base = (sim_log *) (viewer->log_base + M3_SIM_LOG_OFFSET);
	sim_log *sim;

	for (i = 0; i < MAX_SIM_LOG; i++) {
		sim = &base[(idx + i) % MAX_SIM_LOG];
		if (sim->sim_no > 1 || sim->hpd_level > 1
			|| sim->det_level > 1 || sim->sim_status > MAX_SIM_STATUS)
			break;
		retval += snprintf(buf + retval, 200, "[%08x]sim%d:%s hpd:%d det:%d trace:%02d mux:%d,%d,%d\n",
				sim->time, sim->sim_no, sim_state_str[sim->sim_status], sim->hpd_level, sim->det_level,
				sim->trace, sim->sim_mux[0], sim->sim_mux[1], sim->sim_mux[2]);
	}
	buf[retval] = '\0';

	return retval + 1;
}

static ssize_t store_sim(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct platform_device *pdev = (struct platform_device *)container_of(dev, struct platform_device, dev);
	struct hisi_lpm3_viewer *viewer = (struct hisi_lpm3_viewer *)platform_get_drvdata(pdev);
	unsigned int i, idx = viewer->sim_log_idx;
	sim_log *base = (sim_log *) (viewer->log_base + M3_SIM_LOG_OFFSET);
	sim_log *sim;

	for (i = 0; i < MAX_SIM_LOG; i++) {
		sim = &base[(idx + i) % MAX_SIM_LOG];
		if (sim->sim_no > 1 || sim->hpd_level > 1
			|| sim->det_level > 1 || sim->sim_status > MAX_SIM_STATUS)
			break;
	}

	viewer->sim_log_idx = (idx + i) % MAX_SIM_LOG;

	memset((void *) (viewer->log_base + M3_SIM_LOG_OFFSET), 0xFF, MAX_SIM_LOG * sizeof(sim_log));
	return count;
}

static DEVICE_ATTR(sim, 0644, show_sim, store_sim);

static struct attribute *lpm3_viewer_attrs[] = {
	&dev_attr_period.attr,
	&dev_attr_console.attr,
	&dev_attr_sim.attr,
	NULL,
};

static struct attribute_group lpm3_viewer_attr_grp = {
	.attrs = lpm3_viewer_attrs,
};

static unsigned int dump_lpm3_log(char *dst, char *src, unsigned int size)
{
	unsigned int i;
	for (i = size; i > 0; i--) {
		if (src[i - 1] == '\n')
			break;
	}

	memcpy(dst, src, i);
	dst[i] = '\0';

	return i;
}

static unsigned int stepback_read_pointer(struct lpm3_log_info *info, unsigned int steps)
{
	return ((info->wp + M3_LOG_CACHE_SIZE) - steps) & M3_LOG_SIZE_MASK;
}

static unsigned int remain_log_length(unsigned int wp, unsigned int rp)
{
	unsigned int len;
	len = (wp + M3_LOG_CACHE_SIZE) - rp;
	return len & M3_LOG_SIZE_MASK;
}

static unsigned int print_lpm3_log(struct acpu_log_cache *cache, char* base, unsigned int rp)
{
	unsigned int size;
	unsigned int index = cache->index;
	char *data = cache->data;

	size = dump_lpm3_log(data + index, base + rp, AP_LOG_CACHE_SIZE >> 1);
	pr_info("LPM3 LOG:\n%s", cache->data);
	size += dump_lpm3_log(data + 0, base + rp + size, AP_LOG_CACHE_SIZE - size);
	pr_info("LPM3 LOG:\n%s", cache->data);
	memcpy(data, base + rp + size, AP_LOG_CACHE_SIZE - size);

	if (size > (AP_LOG_CACHE_SIZE >> 1)) {
		cache->index = AP_LOG_CACHE_SIZE - size;
	} else {
		pr_err("[lpm3_viewer] can not access ddr region, ignore this 1KB! size = 0x%08x\n", size);
		cache->index = 0;
	}

	return (rp + AP_LOG_CACHE_SIZE) & M3_LOG_SIZE_MASK;
}

static void print_lpm3_log_work(struct work_struct *work)
{
	struct hisi_lpm3_viewer *viewer = container_of(work, struct hisi_lpm3_viewer, show_log_work.work);
	struct lpm3_log_info *info = viewer->info;
	struct acpu_log_cache *cache = &viewer->cache;
	unsigned int wp = info->wp;
	unsigned int rp = viewer->rp;;

	if (wp > M3_LOG_SIZE_MASK || rp > M3_LOG_SIZE_MASK)
		pr_err("[lpm3_viewer] invalid pointers! rp = 0x%08x, wp = 0x%08x\n", rp, wp);

	if (wp != rp) {
		if (remain_log_length(wp, rp) > (AP_LOG_CACHE_SIZE * 4)) {
			rp = stepback_read_pointer(info, AP_LOG_CACHE_SIZE * 4);
			cache->index = 0;
		}

		viewer->rp = print_lpm3_log(cache, viewer->log_base, rp);
	}

	schedule_delayed_work(&viewer->show_log_work, msecs_to_jiffies(viewer->period_time));
}

static int hisi_lpm3_viewer_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = NULL;
	struct hisi_lpm3_viewer *viewer = NULL;
	int ret;

	viewer = devm_kzalloc(dev, sizeof(struct hisi_lpm3_viewer), GFP_KERNEL);
	if (viewer == NULL) {
		dev_err(dev, "Failed to create viewer\n");
		return -ENOMEM;
	}

	np = of_find_compatible_node(NULL, NULL, MODULE_NAME);
	if (np == NULL) {
		dev_err(dev, "Failed to find device node!\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "period_time", &viewer->period_time);
	if (ret) {
		dev_err(dev, "Failed to read period_time\n");
		return ret;
	}

	viewer->cache.data = devm_kzalloc(dev, AP_LOG_CACHE_SIZE, GFP_KERNEL);
	if (viewer->cache.data == NULL) {
		dev_err(dev, "Failed to alloc log buf!\n");
		return -ENOMEM;
	}
	viewer->cache.index = 0;

	viewer->log_base = of_iomap(np, 0);
	if (viewer->log_base == NULL) {
		dev_err(dev, "Failed to iomap log base!\n");
		devm_kfree(dev, viewer->cache.data);
		return -ENXIO;
	}
	viewer->info = (struct lpm3_log_info *)(viewer->log_base + M3_LOG_INFO_OFFSET);
	viewer->rp = 0;
	viewer->sim_log_idx = 0;

	ret = sysfs_create_group(&dev->kobj, &lpm3_viewer_attr_grp);
	if (ret) {
		dev_err(dev, "Failed to create sysfs lpm3_viewer_attr_grp\n");
		devm_kfree(dev, viewer->cache.data);
		return ret;
	}

	INIT_DELAYED_WORK(&viewer->show_log_work, print_lpm3_log_work);

	viewer->pdev = pdev;
	platform_set_drvdata(pdev, viewer);

	if (viewer->period_time)
		schedule_delayed_work(&viewer->show_log_work,
				msecs_to_jiffies(viewer->period_time));

	dev_info(&pdev->dev, "Probe success!\n");

	return 0;
}

static int hisi_lpm3_viewer_remove(struct platform_device *pdev)
{
	struct hisi_lpm3_viewer *viewer = (struct hisi_lpm3_viewer *)platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	sysfs_remove_group(&dev->kobj, &lpm3_viewer_attr_grp);

	devm_kfree(dev, viewer->cache.data);
	devm_kfree(dev, viewer);

	return 0;
}

static const struct of_device_id lpm3_viewer_match[] = {
	{ .compatible = MODULE_NAME },
	{},
};
MODULE_DEVICE_TABLE(of, lpm3_viewer_match);

static struct platform_driver hisi_lpm3_viewer_driver = {
	.driver = {
		.name = MODULE_NAME,
		.of_match_table = of_match_ptr(lpm3_viewer_match),
	},
	.probe = hisi_lpm3_viewer_probe,
	.remove = hisi_lpm3_viewer_remove,
};

static int __init hisi_lpm3_viewer_init(void)
{
	return platform_driver_register(&hisi_lpm3_viewer_driver);
}
module_init(hisi_lpm3_viewer_init);

static void __exit hisi_lpm3_viewer_exit(void)
{
	platform_driver_unregister(&hisi_lpm3_viewer_driver);
}
module_exit(hisi_lpm3_viewer_exit);


MODULE_AUTHOR("cuiyong1@huawei.com>");
MODULE_DESCRIPTION("LOW POWER M3 VIEWER DRIVER");
MODULE_LICENSE("GPL");

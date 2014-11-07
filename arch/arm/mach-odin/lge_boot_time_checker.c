#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/io.h>

static ssize_t
msbl_time_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len;
	char *delta;

	/* For now, we cannot estimate MPBL, MSBL, CPBL booting time.
	 * So, we put measured data in ms. */
	delta = "1605";

	len = snprintf(buf, PAGE_SIZE, "\n******** display msbl time! ********\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=============================\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "delta : %s\n", delta);

	return len;
}

extern char* lge_get_boot_time(void);

static ssize_t
lk_time_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len;
	char *delta;

	delta = lge_get_boot_time();

	len = snprintf(buf, PAGE_SIZE, "\n******** display lk time! ********\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=============================\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "delta : %s\n", delta);

	return len;
}

static struct device_attribute boot_time_device_attrs[] = {
	__ATTR(sbl3_log, S_IRUGO, msbl_time_show, NULL),
	__ATTR(lk_time, S_IRUGO, lk_time_show, NULL),
};

static int __init lge_boot_time_checker_probe(struct platform_device *pdev)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(boot_time_device_attrs); i++) {
		ret = device_create_file(&pdev->dev, &boot_time_device_attrs[i]);
		if (ret)
			return -1;
	}

	return 0;
}

static int lge_boot_time_checker_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_device boot_time_device = {
	.name = "boot_time",
	.id = -1,
};

void __init lge_add_boot_time_checker(void)
{
	platform_device_register(&boot_time_device);
}

static struct platform_driver lge_boot_time_driver __refdata = {
	.probe = lge_boot_time_checker_probe,
	.remove = lge_boot_time_checker_remove,
	.driver = {
		.name = "boot_time",
		.owner = THIS_MODULE,
	}
};

static int __init lge_boot_time_checker_init(void)
{

	return platform_driver_register(&lge_boot_time_driver);
}

static void __exit lge_boot_time_checker_exit(void)
{
	platform_driver_unregister(&lge_boot_time_driver);
}

module_init(lge_boot_time_checker_init);
module_exit(lge_boot_time_checker_exit);

MODULE_DESCRIPTION("LGE Boot Time Checker Driver");
MODULE_AUTHOR("Fred Cho <fred.cho@lge.com>");
MODULE_LICENSE("GPL");

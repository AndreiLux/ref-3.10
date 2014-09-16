#include <linux/init.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>

#include <linux/sec_sysfs.h>

static struct device *gps_dev;
static unsigned int gps_pwr_on = 0;

int check_gps_op(void)
{
	/* This pin is high when gps is working */
	int gps_is_running = gpio_get_value(gps_pwr_on);

	pr_debug("LPA : check_gps_op(%d)\n", gps_is_running);

	return gps_is_running;
}
EXPORT_SYMBOL(check_gps_op);

static int __init gps_bcm4753_init(void)
{
	const char *gps_node = "samsung,exynos54xx-bcm4753";

	struct device_node *root_node = NULL;

	gps_dev = sec_device_create(NULL, "gps");
	BUG_ON(!gps_dev);

	root_node = of_find_compatible_node(NULL, NULL, gps_node);
	if (!root_node) {
		WARN(1, "failed to get device node of bcm4753\n");
		return -ENODEV;
	}

	gps_pwr_on = of_get_gpio(root_node, 0);
	if (!gpio_is_valid(gps_pwr_on)) {
		WARN(1, "Invalied gpio pin : %d\n", gps_pwr_on);
		return -ENODEV;
	}

	if (gpio_request(gps_pwr_on, "GPS_PWR_EN")) {
		WARN(1, "fail to request gpio(GPS_PWR_EN)\n");
		return -ENODEV;
	}
	gpio_direction_output(gps_pwr_on, 0);
	gpio_export(gps_pwr_on, 1);
	gpio_export_link(gps_dev, "GPS_PWR_EN", gps_pwr_on);

	return 0;
}

device_initcall(gps_bcm4753_init);

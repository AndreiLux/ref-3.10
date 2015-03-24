#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <mach/tmp103_cooler.h>

static struct tmp103_cooler_pdev tmp103_cooler_devs[] = {
	{TMP103_COOLER_CPU, MAX_CPU_POWER, MAX_CPU_POWER, "tmp103-cpu-cool-0"},
	{TMP103_COOLER_CPU, MAX_CPU_POWER, MAX_CPU_POWER, "tmp103-cpu-cool-1"},
	{TMP103_COOLER_CPU, MAX_CPU_POWER, MAX_CPU_POWER, "tmp103-cpu-cool-2"},
	{TMP103_COOLER_CPU, MAX_CPU_POWER, MAX_CPU_POWER, "tmp103-cpu-cool-3"},
	{TMP103_COOLER_CPU, MAX_CPU_POWER, MAX_CPU_POWER, "tmp103-cpu-cool-4"},
	{TMP103_COOLER_CPU, MAX_CPU_POWER, MAX_CPU_POWER, "tmp103-cpu-cool-5"},
	{TMP103_COOLER_BL, LED_HALF, LED_HALF, "tmp103-bl-cool-0"},
	{TMP103_COOLER_BC, USHRT_MAX, USHRT_MAX, "tmp103-bc-cool-0"},
	{TMP103_COOLER_BC, USHRT_MAX, USHRT_MAX, "tmp103-bc-cool-1"},
	{TMP103_COOLER_BC, USHRT_MAX, USHRT_MAX, "tmp103-bc-cool-2"},
};

static struct tmp103_cooler_pdata tmp103_cooler_pdata = {
	.count = ARRAY_SIZE(tmp103_cooler_devs),
	.list = tmp103_cooler_devs,
};

static struct platform_device tmp103_cooler_device = {
	.name   = "tmp103-cooling",
	.id     = -1,
	.dev    = {
		.platform_data = &tmp103_cooler_pdata,
	},
};

static int __init board_common_tmp103_init(void)
{
	int err;

	err = platform_device_register(&tmp103_cooler_device);
	if (err) {
		pr_err("%s: Failed to register device %s\n", __func__,
			tmp103_cooler_device.name);
		return err;
	}

	return err;
}

static void __exit board_common_tmp103_exit(void)
{
	platform_device_unregister(&tmp103_cooler_device);
}

module_init(board_common_tmp103_init);
module_exit(board_common_tmp103_exit);

MODULE_DESCRIPTION("TMP103 pcb cpu cooling device driver");
MODULE_AUTHOR("Akwasi Boateng <boatenga@lab126.com>");
MODULE_LICENSE("GPL");


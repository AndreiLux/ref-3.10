#include <mach/board-common-serial.h>
#include <mach/irqs.h>
#include <mach/mt_reg_base.h>
#include <linux/uart/mtk_uart.h>
#include <mach/mt_gpio_def.h>

#define MTK_UART_SIZE 0x100

/*=======================================================================*/
/* MT8135 UART Ports                                                     */
/*=======================================================================*/
static struct resource mtk_resource_uart1[] = {
	{
	 .start = UART1_BASE,
	 .end = UART1_BASE + MTK_UART_SIZE - 1,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_UART1_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct resource mtk_resource_uart2[] = {
	{
	 .start = UART2_BASE,
	 .end = UART2_BASE + MTK_UART_SIZE - 1,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_UART2_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct resource mtk_resource_uart3[] = {
	{
	 .start = UART3_BASE,
	 .end = UART3_BASE + MTK_UART_SIZE - 1,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_UART3_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct resource mtk_resource_uart4[] = {
	{
	 .start = UART4_BASE,
	 .end = UART4_BASE + MTK_UART_SIZE - 1,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_UART4_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct platform_device mtk_device_uart[] = {

	{
	 .name = "mtk-uart",
	 .id = 0,
	 .num_resources = ARRAY_SIZE(mtk_resource_uart1),
	 .resource = mtk_resource_uart1,
	 .dev = {
		 .platform_data = NULL,
		}
	 },
	{
	 .name = "mtk-uart",
	 .id = 1,
	 .num_resources = ARRAY_SIZE(mtk_resource_uart2),
	 .resource = mtk_resource_uart2,
	 .dev = {
		 .platform_data = NULL,
		}
	},
	{
	 .name = "mtk-uart",
	 .id = 2,
	 .num_resources = ARRAY_SIZE(mtk_resource_uart3),
	 .resource = mtk_resource_uart3,
	 .dev = {
		 .platform_data = NULL,
		}
	},
	{
	 .name = "mtk-uart",
	 .id = 3,
	 .num_resources = ARRAY_SIZE(mtk_resource_uart4),
	 .resource = mtk_resource_uart4,
	 .dev = {
		 .platform_data = NULL,
		}
	},
};

#ifdef CONFIG_PM
static int mt_serial_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct mtk_uart *uart = platform_get_drvdata(pdev);
	struct mtk_uart_mode_data *umd = uart->port.dev->platform_data;
	struct mt_pin_info gpio;
	int ret = 0;

	if (!umd)
		return -1;

	if (uart->deep_idle)
		gpio.w = umd->uart.txd.w;
	else
		gpio.w = umd->uart.rxd.w;

	if (gpio.valid) {
		if (uart->deep_idle)
			gpio_direction_output(gpio.pin, 1);
		ret = mt_pin_set_mode_gpio(gpio.pin);
	}

	return ret;
}

static int mt_serial_resume(struct platform_device *pdev)
{
	struct mtk_uart *uart = platform_get_drvdata(pdev);
	struct mtk_uart_mode_data *umd = uart->port.dev->platform_data;
	struct mt_pin_info gpio;
	const char *mode;
	int ret = 0;

	if (!umd)
		return -1;

	if (uart->deep_idle) {
		gpio.w = umd->uart.txd.w;
		mode = "UTXD";
	} else {
		gpio.w = umd->uart.rxd.w;
		mode = "URXD";
	}

	if (gpio.valid)
		ret = mt_pin_set_mode_by_name(gpio.pin, mode);

	return ret;
}
#endif

int __init mt_register_serial(int id, struct mtk_uart_mode_data *uart_mode_data)
{
	if (id < 0 || id >= ARRAY_SIZE(mtk_device_uart) || !uart_mode_data)
		return -EINVAL;

#ifdef CONFIG_PM
	uart_mode_data->suspend = mt_serial_suspend;
	uart_mode_data->resume = mt_serial_resume;
#endif

	mtk_device_uart[id].dev.platform_data = uart_mode_data;

	return platform_device_register(&mtk_device_uart[id]);
}


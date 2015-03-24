#ifndef __BOARD_COMMON_SERIAL_H__
#define __BOARD_COMMON_SERIAL_H__

#include <linux/platform_device.h>
#include <linux/pm.h>
#include <mach/mt_gpio_def.h>

struct mtk_uart_mode_data {
	struct mtk_uart_data uart;
#ifdef CONFIG_PM
	int (*suspend)(struct platform_device *, pm_message_t);
	int (*resume)(struct platform_device *);
#endif
};

int mt_register_serial(int id, struct mtk_uart_mode_data *uart_mode_data);

#endif				/* __BOARD_COMMON_SERIAL_H__ */

#ifndef INCLUDE_ODIN_GPIO_H
#define INCLUDE_ODIN_GPIO_H

#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#include <asm/mach/irq.h>

/* define register addr */
#define CONF_GPIO		0x000
#define SET_PDATA		0x030
#define READ_PDATA		0x034
#define CONF_DEBOUNCE		0x040
#define EINT_SRC		0x070
#define EINT_PEND		0x080
#define EINT_MASK		0x090

#define ODIN_GPIO_PULL_DOWN	4
#define ODIN_GPIO_PULL_UP	5
/* define total GPIO number */
#define ODIN_GPIO_NR		8
#define ODIN_GPIO_SMS_NR    	48

#define ODIN_GPIO_PERI_START	0
#define ODIN_GPIO_PERI_END	111
#define ODIN_GPIO_AUD_START	184
#define ODIN_GPIO_AUD_END	231
#define ODIN_GPIO_ICS_START	232
#define ODIN_GPIO_ICS_END	271

#define ODIN_SMS_GID_START	198
#define ODIN_SMS_GID_MID	206
#define ODIN_SMS_GID_END	213

#define ODIN_SMS_GID_TOTAL	16

enum gpio_pinmux {
	GPIO_PIN_INPUT = 0,
	GPIO_PIN_OUTPUT,
	GPIO_PIN_IP0,
	GPIO_PIN_IP1,
};

enum gpio_deb_type {
	DEBOUNCE_TYPE_REGISTER = 0,
	DEBOUNCE_TYPE_HOLDBUF,
};

enum gpio_deb_clk {
	DEBOUNCE_CLK_ALIVE = 0,
	DEBOUNCE_CLK_EXT,
	DEBOUNCE_CLK_PCLK,
};

#define pr_gpio(fmt, ... ) \
	printk("[GPIO_DEBUG] " pr_fmt(fmt), ##__VA_ARGS__)

int odin_gpio_set_retention(unsigned gpio, unsigned enable);
int odin_gpio_get_pinmux(unsigned gpio);
int odin_gpio_set_pinmux(unsigned gpio, unsigned type);
int odin_gpio_save_regs(unsigned gpio_base);
int odin_gpio_restore_regs(unsigned gpio_base);
int odin_gpio_pull_up(unsigned gpio, int enable);
int odin_gpio_pull_down(unsigned gpio, int enable);

int woden_gpio_save_regs(unsigned gpio_base);
int woden_gpio_restore_regs(unsigned gpio_base);

void odin_gpio_regs_suspend(int start, int end, int size);
void odin_gpio_regs_resume(int start, int end, int size);

void odin_gpio_debounce(unsigned int gpio);

int odin_gpio_sms_request_irq(unsigned int irq, irq_handler_t handler,
			unsigned long flags, const char *name, void *dev);
int odin_gpio_sms_request_threaded_irq(unsigned int irq, irq_handler_t handler,
			irq_handler_t thread_fn, unsigned long irqflags,
			const char *devname, void *dev_id);
int odin_gpio_sms_request_any_context_irq(unsigned int irq,
			irq_handler_t handler, unsigned long flags,
			const char *name, void *dev_id);
int is_gic_direct_irq(unsigned int irq);
int odin_gpio_smsgic_irq_enable(unsigned int irq);
int odin_gpio_smsgic_irq_disable(unsigned int irq);
int odin_gpio_smsgic_irq_unmask(unsigned int irq);
int odin_gpio_smsgic_irq_mask(unsigned int irq);
int odin_gpio_smsgic_irq_ack(unsigned int irq);

#ifdef CONFIG_PM
struct odin_gpio_save_reg {
	unsigned int set_pdata;
	unsigned int conf_gpio[ODIN_GPIO_NR];
	unsigned int conf_debounce[ODIN_GPIO_NR];
};
#endif

struct odin_gpio {
	struct list_head list;

	spinlock_t	lock;
	spinlock_t	irq_lock;

	void __iomem	*base;
	int 	irq;
	int	gpio_base;
	int	irq_base;
	struct gpio_chip	gc;
	struct device	*dev;
	struct irq_domain	*domain;
#ifdef CONFIG_PM
	struct odin_gpio_save_reg	save_regs;
#endif
	struct clk *pclk;
};

struct odin_gpio_platform_data {
	int	gpio_base;
	int	irq_base;
};

struct odin_gpio_sms_irq {
       irq_handler_t handler;
};

static irq_handler_t odin_gpio_sms_handler[ODIN_SMS_GID_TOTAL];
static struct odin_gpio* odin_gpio_chip[ODIN_SMS_GID_TOTAL];

u32 odin_gpio_read(void __iomem *addr);
void odin_gpio_write(u32 b, void __iomem *addr);
#endif

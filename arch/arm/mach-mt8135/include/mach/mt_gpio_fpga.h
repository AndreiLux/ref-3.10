#ifndef _MT_GPIO_FPGA_H_
#define _MT_GPIO_FPGA_H_

/* #if (defined(CONFIG_MT6589_FPGA)) */
#if 1

#define  MT_GPIO_BASE_START 0
typedef enum GPIO_PIN {
	GPIO_UNSUPPORTED = -1,
	GPIO0 = MT_GPIO_BASE_START,
	GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO6, GPIO7,
	MT_GPIO_BASE_MAX
} GPIO_PIN;

/*---------------------------------------------------------------------------*/
/* int mt_set_gpio_dir_base(unsigned long pin, unsigned long dir); */
/* int mt_get_gpio_dir_base(unsigned long pin); */
/* int mt_set_gpio_pull_enable_base(unsigned long pin, unsigned long enable); */
/* int mt_get_gpio_pull_enable_base(unsigned long pin); */
/* int mt_set_gpio_ies_base(unsigned long pin, unsigned long enable); */
/* int mt_get_gpio_ies_base(unsigned long pin); */
/* int mt_set_gpio_pull_select_base(unsigned long pin, unsigned long select); */
/* int mt_get_gpio_pull_select_base(unsigned long pin); */
/* int mt_set_gpio_inversion_base(unsigned long pin, unsigned long enable); */
/* int mt_get_gpio_inversion_base(unsigned long pin); */
/* int mt_set_gpio_out_base(unsigned long pin, unsigned long output); */
/* int mt_get_gpio_out_base(unsigned long pin); */
/* int mt_get_gpio_in_base(unsigned long pin); */
/* int mt_set_gpio_mode_base(unsigned long pin, unsigned long mode); */
/* int mt_get_gpio_mode_base(unsigned long pin); */
/*---------------------------------------------------------------------------*/
#endif
#endif

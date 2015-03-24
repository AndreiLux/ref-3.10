#ifndef __CUST_GT8XX_H
#define __CUST_GT8XX_H

/* The platform data for the GT8xx touchscreen driver */
struct gt8xx_platform_data {
	int gtp_rst_port;	/* #define GTP_RST_PORT    GPIO_CTP_RST_PIN */
	int gtp_int_port;	/* #define GTP_INT_PORT    GPIO_CTP_EINT_PIN */
	int gtp_rst_m_gpio;
	int gtp_int_m_gpio;
	int gtp_int_m_eint;
	int irq;
};

#endif				/* _CUST_GT8XX_H */

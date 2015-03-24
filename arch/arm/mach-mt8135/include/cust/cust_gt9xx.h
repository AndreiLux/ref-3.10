#ifndef __CUST_GT9XX_H
#define __CUST_GT9XX_H

/* The platform data for the GT9xx touchscreen driver */
struct gt9xx_platform_data {
	int gtp_debug_on;	/* #define GTP_DEBUG_ON          0 */
	int gtp_debug_array_on;	/* #define GTP_DEBUG_ARRAY_ON    0 */
	int gtp_debug_func_on;	/* #define GTP_DEBUG_FUNC_ON     0 */

	void *ctp_cfg_group1;	/* CTP_CFG_GROUP1 */

	int gtp_rst_port;	/* #define GTP_RST_PORT    GPIO_CTP_RST_PIN */
	int gtp_int_port;	/* #define GTP_INT_PORT    GPIO_CTP_EINT_PIN */
	int gtp_rst_m_gpio;
	int gtp_int_m_gpio;
	int gtp_int_m_eint;

	int gtp_max_height;	/* #define GTP_MAX_HEIGHT   1280 */
	int gtp_max_width;	/* #define GTP_MAX_WIDTH    800 */
	int gtp_int_trigger;	/* #define GTP_INT_TRIGGER  1 */

	int gtp_esd_check_circle;	/* #define GTP_ESD_CHECK_CIRCLE  2000 */
	int tpd_power_source_custom;	/* #define TPD_POWER_SOURCE_CUSTOM       MT65XX_POWER_LDO_VGP6 */

	int tpd_velocity_custom_x;	/* #define TPD_VELOCITY_CUSTOM_X 20 */
	int tpd_velocity_custom_y;	/* #define TPD_VELOCITY_CUSTOM_Y 20 */

	int i2c_master_clock;	/* #define I2C_MASTER_CLOCK              300 */

	void *tpd_calibration_matrix_rotation_normal;	/* #define TPD_CALIBRATION_MATRIX_ROTATION_NORMAL  {0, -4096, 3272704, -4096, 0, 5238784, 0, 0}; */
	void *tpd_calibration_matrix_rotation_factory;	/* #define TPD_CALIBRATION_MATRIX_ROTATION_FACTORY {0, -4096, 3272704, -4096, 0, 5238784, 0, 0}; */
};

#endif				/* _CUST_GT9XX_H */

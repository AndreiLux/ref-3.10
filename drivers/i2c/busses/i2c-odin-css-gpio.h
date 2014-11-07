
#ifndef	__I2C_ODIN_CSS_GPIO__
#define	__I2C_ODIN_CSS_GPIO__

struct i2c_gpio_bit_data {
	void *data;		/* private data for lowlevel routines */
	void (*setsda) (void *data, int state);
	void (*setscl) (void *data, int state);
	int  (*getsda) (void *data);
	int  (*getscl) (void *data);
	int  (*pre_xfer)  (struct i2c_adapter *);
	void (*post_xfer) (struct i2c_adapter *);

	/* local settings */
	int udelay;		/* half clock cycle time in us,
				   minimum 2 us for fast-mode I2C,
				   minimum 5 us for standard-mode I2C and SMBus,
				   maximum 50 us for SMBus */
	int timeout;		/* in jiffies */
	int scl_pin;
	int sda_pin;
};

int i2c_gpio_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num);
//void i2c_gpio_set(int scl_gpio, int sda_gpio);


#endif	/*__I2C_ODIN_CSS_GPIO__*/

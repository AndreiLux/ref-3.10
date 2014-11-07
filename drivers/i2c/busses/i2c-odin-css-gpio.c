#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_data/gpio-odin.h>

#include "i2c-odin-css-gpio.h"


/* --- setting states on the bus with the right timing: ---------------	*/

/* Toggle SDA by changing the direction of the pin */
static void i2c_gpio_setsda_dir(void *data, int state)
{
	struct i2c_gpio_bit_data *pdata = data;

	if (state)
		gpio_direction_input(pdata->sda_pin);
	else
		gpio_direction_output(pdata->sda_pin, 0);
}

/*
 * Toggle SDA by changing the output value of the pin. This is only
 * valid for pins configured as open drain (i.e. setting the value
 * high effectively turns off the output driver.)
 */
static void i2c_gpio_setsda_val(void *data, int state)
{
	struct i2c_gpio_bit_data *pdata = data;

	gpio_set_value(pdata->sda_pin, state);
}

/* Toggle SCL by changing the direction of the pin. */
static void i2c_gpio_setscl_dir(void *data, int state)
{
	struct i2c_gpio_bit_data *pdata = data;

	if (state)
		gpio_direction_input(pdata->scl_pin);
	else
		gpio_direction_output(pdata->scl_pin, 0);
}

/*
 * Toggle SCL by changing the output value of the pin. This is used
 * for pins that are configured as open drain and for output-only
 * pins. The latter case will break the i2c protocol, but it will
 * often work in practice.
 */
static void i2c_gpio_setscl_val(void *data, int state)
{
	struct i2c_gpio_bit_data *pdata = data;

	gpio_set_value(pdata->scl_pin, state);
}

static int i2c_gpio_getsda(void *data)
{
	struct i2c_gpio_bit_data *pdata = data;

	return gpio_get_value(pdata->sda_pin);
}

static int i2c_gpio_getscl(void *data)
{
	struct i2c_gpio_bit_data *pdata = data;

	return gpio_get_value(pdata->scl_pin);
}

static int i2c_gpio_prexfer(struct i2c_adapter *adap)
{
	return 0;
}

static void i2c_gpio_postxfer(struct i2c_adapter *adap)
{
}

static const struct i2c_gpio_bit_data g_adap = {
	.pre_xfer	= i2c_gpio_prexfer,
	.post_xfer	= i2c_gpio_postxfer,
	.setsda		= i2c_gpio_setsda_dir,
	.setscl		= i2c_gpio_setscl_dir,
	.getsda		= i2c_gpio_getsda,
//	.getscl		= i2c_gpio_getscl,
	.udelay		= 5,
	.timeout	= 200,
	.scl_pin	= 274,	// CSS GPIO Number depend on dtsi
	.sda_pin	= 275,	// CSS GPIO Number depend on dtsi
};


static inline void sdalo(struct i2c_gpio_bit_data *adap)
{
	adap->setsda(adap, 0);
	udelay((adap->udelay + 1) / 2);
}

static inline void sdahi(struct i2c_gpio_bit_data *adap)
{
	adap->setsda(adap, 1);
	udelay((adap->udelay + 1) / 2);
}

static inline void scllo(struct i2c_gpio_bit_data *adap)
{
	adap->setscl(adap, 0);
	udelay(adap->udelay / 2);
}

/*
 * Raise scl line, and do checking for delays. This is necessary for slower
 * devices.
 */
static int sclhi(struct i2c_gpio_bit_data *adap)
{
	unsigned long start;

	adap->setscl(adap, 1);

	/* Not all adapters have scl sense line... */
	if (!adap->getscl)
		goto done;

	start = jiffies;
	while (!adap->getscl(adap)) {
		/* This hw knows how to read the clock line, so we wait
		 * until it actually gets high.  This is safer as some
		 * chips may hold it low ("clock stretching") while they
		 * are processing data internally.
		 */
		if (time_after(jiffies, start + adap->timeout)) {
			/* Test one last time, as we may have been preempted
			 * between last check and timeout test.
			 */
			if (adap->getscl(adap))
				break;
			return -ETIMEDOUT;
		}
		cpu_relax();
	}

done:
	udelay(adap->udelay);
	return 0;
}

/* --- other auxiliary functions --------------------------------------	*/
static void i2c_start(struct i2c_gpio_bit_data *adap)
{
	/* assert: scl, sda are high */
	adap->setsda(adap, 0);
	udelay(adap->udelay);
	scllo(adap);
}

static void i2c_repstart(struct i2c_gpio_bit_data *adap)
{
	/* assert: scl is low */
	sdahi(adap);
	sclhi(adap);
	adap->setsda(adap, 0);
	udelay(adap->udelay);
	scllo(adap);
}


static void i2c_stop(struct i2c_gpio_bit_data *adap)
{
	/* assert: scl is low */
	sdalo(adap);
	sclhi(adap);
	adap->setsda(adap, 1);
	udelay(adap->udelay);
}

/* send a byte without start cond., look for arbitration,
   check ackn. from slave */
/* returns:
 * 1 if the device acknowledged
 * 0 if the device did not ack
 * -ETIMEDOUT if an error occurred (while raising the scl line)
 */
static int i2c_outb(struct i2c_adapter *i2c_adap, unsigned char c)
{
	int i;
	int sb;
	int ack;
	struct i2c_gpio_bit_data *adap = &g_adap;

	/* assert: scl is low */
	for (i = 7; i >= 0; i--) {
		sb = (c >> i) & 1;
		adap->setsda(adap, sb);
		udelay((adap->udelay + 1) / 2);
		if (sclhi(adap) < 0) { /* timed out */
			printk("i2c_outb: 0x%02x, timeout at bit #%d\n", (int)c, i);
			return -ETIMEDOUT;
		}
		/* FIXME do arbitration here:
		 * if (sb && !getsda(adap)) -> ouch! Get out of here.
		 *
		 * Report a unique code, so higher level code can retry
		 * the whole (combined) message and *NOT* issue STOP.
		 */
		scllo(adap);
	}
	sdahi(adap);
	if (sclhi(adap) < 0) { /* timeout */
		printk("i2c_outb: 0x%02x, timeout at ack\n", (int)c);
		return -ETIMEDOUT;
	}

	/* read ack: SDA should be pulled down by slave, or it may
	 * NAK (usually to report problems with the data we wrote).
	 */
	ack = !adap->getsda(adap);    /* ack: sda is pulled low -> success */

	scllo(adap);
	return ack;
	/* assert: scl is low (sda undef) */
}


static int i2c_inb(struct i2c_adapter *i2c_adap)
{
	/* read byte via i2c port, without start/stop sequence	*/
	/* acknowledge is sent in i2c_read.			*/
	int i;
	unsigned char indata = 0;
	struct i2c_gpio_bit_data *adap = &g_adap;

	/* assert: scl is low */
	sdahi(adap);
	for (i = 0; i < 8; i++) {
		if (sclhi(adap) < 0) { /* timeout */
			printk("i2c_inb: timeout at bit #%d\n", 7 - i);
			return -ETIMEDOUT;
		}
		indata *= 2;
		if (adap->getsda(adap))
			indata |= 0x01;
		adap->setscl(adap, 0);
		udelay(i == 7 ? adap->udelay / 2 : adap->udelay);
	}
	/* assert: scl is low */
	return indata;
}

/*
 * Sanity check for the adapter hardware - check the reaction of
 * the bus lines only if it seems to be idle.
 */
static int test_bus(struct i2c_adapter *i2c_adap)
{
	struct i2c_gpio_bit_data *adap = &g_adap;
	const char *name = i2c_adap->name;
	int scl, sda, ret;

	if (adap->pre_xfer) {
		ret = adap->pre_xfer(i2c_adap);
		if (ret < 0)
			return -ENODEV;
	}

	if (adap->getscl == NULL)
		pr_info("%s: Testing SDA only, SCL is not readable\n", name);

	sda = adap->getsda(adap);
	scl = (adap->getscl == NULL) ? 1 : adap->getscl(adap);
	if (!scl || !sda) {
		printk(KERN_WARNING
		       "%s: bus seems to be busy (scl=%d, sda=%d)\n",
		       name, scl, sda);
		goto bailout;
	}

	sdalo(adap);
	sda = adap->getsda(adap);
	scl = (adap->getscl == NULL) ? 1 : adap->getscl(adap);
	if (sda) {
		printk(KERN_WARNING "%s: SDA stuck high!\n", name);
		goto bailout;
	}
	if (!scl) {
		printk(KERN_WARNING "%s: SCL unexpected low "
		       "while pulling SDA low!\n", name);
		goto bailout;
	}

	sdahi(adap);
	sda = adap->getsda(adap);
	scl = (adap->getscl == NULL) ? 1 : adap->getscl(adap);
	if (!sda) {
		printk(KERN_WARNING "%s: SDA stuck low!\n", name);
		goto bailout;
	}
	if (!scl) {
		printk(KERN_WARNING "%s: SCL unexpected low "
		       "while pulling SDA high!\n", name);
		goto bailout;
	}

	scllo(adap);
	sda = adap->getsda(adap);
	scl = (adap->getscl == NULL) ? 0 : adap->getscl(adap);
	if (scl) {
		printk(KERN_WARNING "%s: SCL stuck high!\n", name);
		goto bailout;
	}
	if (!sda) {
		printk(KERN_WARNING "%s: SDA unexpected low "
		       "while pulling SCL low!\n", name);
		goto bailout;
	}

	sclhi(adap);
	sda = adap->getsda(adap);
	scl = (adap->getscl == NULL) ? 1 : adap->getscl(adap);
	if (!scl) {
		printk(KERN_WARNING "%s: SCL stuck low!\n", name);
		goto bailout;
	}
	if (!sda) {
		printk(KERN_WARNING "%s: SDA unexpected low "
		       "while pulling SCL high!\n", name);
		goto bailout;
	}

	if (adap->post_xfer)
		adap->post_xfer(i2c_adap);

	pr_info("%s: Test OK\n", name);
	return 0;
bailout:
	sdahi(adap);
	sclhi(adap);

	if (adap->post_xfer)
		adap->post_xfer(i2c_adap);

	return -ENODEV;
}

/* ----- Utility functions
 */

/* try_address tries to contact a chip for a number of
 * times before it gives up.
 * return values:
 * 1 chip answered
 * 0 chip did not answer
 * -x transmission error
 */
static int try_address(struct i2c_adapter *i2c_adap,
		       unsigned char addr, int retries)
{
	struct i2c_gpio_bit_data *adap = &g_adap;
	int i, ret = 0;

	for (i = 0; i <= retries; i++) {
		ret = i2c_outb(i2c_adap, addr);
		if (ret == 1 || i == retries)
			break;
		i2c_stop(adap);
		udelay(adap->udelay);
		yield();
		i2c_start(adap);
	}
//	if (i && ret)
//		printk("Used %d tries to %s client at "
//			"0x%02x: %s\n", i + 1,
//			addr & 1 ? "read from" : "write to", addr >> 1,
//			ret == 1 ? "success" : "failed, timeout?");
	return ret;
}

static int sendbytes(struct i2c_adapter *i2c_adap, struct i2c_msg *msg)
{
	const unsigned char *temp = msg->buf;
	int count = msg->len;
	unsigned short nak_ok = msg->flags & I2C_M_IGNORE_NAK;
	int retval;
	int wrcount = 0;

	while (count > 0) {
		retval = i2c_outb(i2c_adap, *temp);

		/* OK/ACK; or ignored NAK */
		if ((retval > 0) || (nak_ok && (retval == 0))) {
			count--;
			temp++;
			wrcount++;

		/* A slave NAKing the master means the slave didn't like
		 * something about the data it saw.  For example, maybe
		 * the SMBus PEC was wrong.
		 */
		} else if (retval == 0) {
			dev_err(&i2c_adap->dev, "sendbytes: NAK bailout.\n");
			return -EIO;

		/* Timeout; or (someday) lost arbitration
		 *
		 * FIXME Lost ARB implies retrying the transaction from
		 * the first message, after the "winning" master issues
		 * its STOP.  As a rule, upper layer code has no reason
		 * to know or care about this ... it is *NOT* an error.
		 */
		} else {
			dev_err(&i2c_adap->dev, "sendbytes: error %d\n",
					retval);
			return retval;
		}
	}
	return wrcount;
}

static int acknak(struct i2c_adapter *i2c_adap, int is_ack)
{
	struct i2c_gpio_bit_data *adap = &g_adap;

	/* assert: sda is high */
	if (is_ack)		/* send ack */
		adap->setsda(adap, 0);
	udelay((adap->udelay + 1) / 2);
	if (sclhi(adap) < 0) {	/* timeout */
		printk("readbytes: ack/nak timeout\n");
		return -ETIMEDOUT;
	}
	scllo(adap);
	return 0;
}

static int readbytes(struct i2c_adapter *i2c_adap, struct i2c_msg *msg)
{
	int inval;
	int rdcount = 0;	/* counts bytes read */
	unsigned char *temp = msg->buf;
	int count = msg->len;
	const unsigned flags = msg->flags;

	while (count > 0) {
		inval = i2c_inb(i2c_adap);
		if (inval >= 0) {
			*temp = inval;
			rdcount++;
		} else {   /* read timed out */
			break;
		}

		temp++;
		count--;

		/* Some SMBus transactions require that we receive the
		   transaction length as the first read byte. */
		if (rdcount == 1 && (flags & I2C_M_RECV_LEN)) {
			if (inval <= 0 || inval > I2C_SMBUS_BLOCK_MAX) {
				if (!(flags & I2C_M_NO_RD_ACK))
					acknak(i2c_adap, 0);
				printk("readbytes: invalid block length (%d)\n", inval);
				return -EPROTO;
			}
			/* The original count value accounts for the extra
			   bytes, that is, either 1 for a regular transaction,
			   or 2 for a PEC transaction. */
			count += inval;
			msg->len += inval;
		}

//		printk("readbytes: 0x%02x %s\n",
//			inval,
//			(flags & I2C_M_NO_RD_ACK)
//				? "(no ack/nak)"
//				: (count ? "A" : "NA"));

		if (!(flags & I2C_M_NO_RD_ACK)) {
			inval = acknak(i2c_adap, count);
			if (inval < 0)
				return inval;
		}
	}
	return rdcount;
}

/* doAddress initiates the transfer by generating the start condition (in
 * try_address) and transmits the address in the necessary format to handle
 * reads, writes as well as 10bit-addresses.
 * returns:
 *  0 everything went okay, the chip ack'ed, or IGNORE_NAK flag was set
 * -x an error occurred (like: -ENXIO if the device did not answer, or
 *	-ETIMEDOUT, for example if the lines are stuck...)
 */
static int bit_doAddress(struct i2c_adapter *i2c_adap, struct i2c_msg *msg)
{
	unsigned short flags = msg->flags;
	unsigned short nak_ok = msg->flags & I2C_M_IGNORE_NAK;
	struct i2c_gpio_bit_data *adap = &g_adap;

	unsigned char addr;
	int ret, retries;

	retries = nak_ok ? 0 : i2c_adap->retries;

	if (flags & I2C_M_TEN) {
		/* a ten bit address */
		addr = 0xf0 | ((msg->addr >> 7) & 0x06);
		/* try extended address code...*/
		ret = try_address(i2c_adap, addr, retries);
		if ((ret != 1) && !nak_ok)  {
			dev_err(&i2c_adap->dev,
				"died at extended address code\n");
			return -ENXIO;
		}
		/* the remaining 8 bit address */
		ret = i2c_outb(i2c_adap, msg->addr & 0xff);
		if ((ret != 1) && !nak_ok) {
			/* the chip did not ack / xmission error occurred */
			dev_err(&i2c_adap->dev, "died at 2nd address code\n");
			return -ENXIO;
		}
		if (flags & I2C_M_RD) {
			i2c_repstart(adap);
			/* okay, now switch into reading mode */
			addr |= 0x01;
			ret = try_address(i2c_adap, addr, retries);
			if ((ret != 1) && !nak_ok) {
				dev_err(&i2c_adap->dev,
					"died at repeated address code\n");
				return -EIO;
			}
		}
	} else {		/* normal 7bit address	*/
		addr = msg->addr << 1;
		if (flags & I2C_M_RD)
			addr |= 1;
		if (flags & I2C_M_REV_DIR_ADDR)
			addr ^= 1;
		ret = try_address(i2c_adap, addr, retries);
		if ((ret != 1) && !nak_ok)
			return -ENXIO;
	}

	return 0;
}

int i2c_gpio_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num)
{
	struct i2c_msg *pmsg;
	struct i2c_gpio_bit_data *adap = &g_adap;
	int i, ret;
	unsigned short nak_ok;
	int nbytes;

	if (adap->pre_xfer) {
		ret = adap->pre_xfer(i2c_adap);
		if (ret < 0)
			return ret;
	}

	/* GPIO initialize */
	gpio_request(adap->sda_pin, "gpio_i2c_sda");
	gpio_request(adap->scl_pin, "gpio_i2c_scl");
	gpio_direction_input(adap->sda_pin);
	gpio_direction_input(adap->scl_pin);


	i2c_start(adap);
	for (i = 0; i < num; i++) {
		pmsg = &msgs[i];
		pmsg->flags &= ~(I2C_M_TEN); /* added by hckim */
		pmsg->addr >>= 1;

		nak_ok = pmsg->flags & I2C_M_IGNORE_NAK;
		if (!(pmsg->flags & I2C_M_NOSTART)) {
			if (i) {
				i2c_repstart(adap);
			}
			ret = bit_doAddress(i2c_adap, pmsg);
			if ((ret != 0) && !nak_ok) {
				goto bailout;
			}
		}
		if (pmsg->flags & I2C_M_RD) {
			/* read bytes into buffer*/
			ret = readbytes(i2c_adap, pmsg);
			if (ret < pmsg->len) {
				if (ret >= 0)
					ret = -EIO;
				goto bailout;
			}
			nbytes = ret;
		} else {
			/* write bytes from buffer */
			ret = sendbytes(i2c_adap, pmsg);
			if (ret < pmsg->len) {
				if (ret >= 0)
					ret = -EIO;
				goto bailout;
			}
			nbytes = ret;
		}
	}
	ret = i;
	ret = nbytes;;
bailout:
	i2c_stop(adap);

	/* GPIO deinit */
	odin_gpio_set_pinmux(adap->sda_pin, 0x2);
	odin_gpio_set_pinmux(adap->scl_pin, 0x2);
	gpio_free(adap->sda_pin);
	gpio_free(adap->scl_pin);

	if (adap->post_xfer)
		adap->post_xfer(i2c_adap);

	return ret;
}



#include <linux/rmi.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio.h>

#ifdef CONFIG_OF
struct syna_gpio_data {
    u16 gpio_number;
	u16 rst_gpio_number;
    char touch_pin_name[];
};

static int synaptics_touchpad_gpio_setup(void *gpio_data, bool configure)
{
	int retval=0;
    struct syna_gpio_data *data = gpio_data;

    if (configure) {
        retval = gpio_request(data->gpio_number, "rmi4_attn");
        if (retval<0) {
            pr_err("%s: Failed to get attn gpio %d. Code: %d.",
                   __func__, data->gpio_number, retval);
            return retval;
        }

        retval = gpio_direction_input(data->gpio_number);
        if (retval<0) {
            pr_err("%s: Failed to setup direction attn gpio %d. Code: %d.",
                   __func__, data->gpio_number, retval);
            gpio_free(data->gpio_number);
        }
		
		retval = gpio_request(data->rst_gpio_number, "rmi4_rst");
        if (retval < 0)
        {
            pr_err("%s: Failed to get reset gpio %d. Code: %d",__func__,
				data->rst_gpio_number, retval);
            return retval;
        }

        retval = gpio_direction_output(data->rst_gpio_number, 1);
        if (retval < 0)
        {
            pr_err("%s: Failed to setup direction reset gpio %d. Code: %d",
					__func__, data->rst_gpio_number, retval);
            return retval;
        }
    } else {
        pr_warn("%s: No way to deconfigure gpio %d.",
               __func__, data->gpio_number);
    }

    return retval;
};

static struct syna_gpio_data tm1940_gpiodata = {
    .gpio_number = 55,
	.rst_gpio_number = 241,
    .touch_pin_name = "SYS_EINT21.TOUCH_INT_N",
};

static unsigned char tm1940_f1a_button_codes[] = {KEY_BACK,KEY_MENU};

static struct rmi_f1a_button_map tm1940_f1a_button_map = {
    .nbuttons = ARRAY_SIZE(tm1940_f1a_button_codes),
    .map = tm1940_f1a_button_codes,
};

static struct rmi_device_platform_data tm1940_platformdata = {
    .driver_name = "rmi_generic",
/*  .attn_gpio = 55, */
    .attn_polarity = RMI_ATTN_ACTIVE_LOW,
    .gpio_data = &tm1940_gpiodata,
    .gpio_config = synaptics_touchpad_gpio_setup,
    .reset_delay_ms = 400,
	.level_triggered = 1,
    .f1a_button_map = &tm1940_f1a_button_map,
	.power_management = {
		.wakeup_threshold = 30,
    	.doze_holdoff = 5,		/* holdoff = 0.5s */
    	.doze_interval = 3,		/* interval = 30ms */
		.nosleep = RMI_F01_NOSLEEP_OFF,
		},
};

static struct of_device_id touch_i2c_dt_ids[] = {
    {.compatible = "TouchIC,s7020_odin",.data=&tm1940_platformdata,},
    {},
};

#endif

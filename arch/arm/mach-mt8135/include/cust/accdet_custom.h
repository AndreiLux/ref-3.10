struct headset_mode_settings {
	int pwm_width;		/* pwm frequence */
	int pwm_thresh;		/* pwm duty */
	int fall_delay;		/* falling stable time */
	int rise_delay;		/* rising stable time */
	int debounce0;		/* hook switch or double check debounce */
	int debounce1;		/* mic bias debounce */
	int debounce3;		/* plug out debounce */
};

/* key press customization: long press time */
struct headset_key_custom {
	int headset_long_press_time;
};

struct headset_mode_data {
	unsigned int cust_eint_accdet_num;
	unsigned int cust_eint_accdet_debounce_cn;
	unsigned int cust_eint_accdet_type;
	unsigned int cust_eint_accdet_debounce_en;
	unsigned int gpio_accdet_eint_pin;
	unsigned int gpio_accdet_eint_pin_m_eint;
};

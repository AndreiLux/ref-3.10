extern struct headset_mode_settings *get_cust_headset_settings(void);
extern struct headset_key_custom *get_headset_key_custom_setting(void);
extern void accdet_eint_func(void);
extern struct headset_mode_data *get_cust_headset_data(void);

extern void accdet_workqueue_func(void);
extern void clear_accdet_interrupt(void);
extern void accdet_auxadc_switch_on(void);
extern U32 accdet_get_irq_state(void);


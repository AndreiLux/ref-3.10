#include <accdet_hal.h>
#include <mach/mt_boot.h>
#include <mach/mt_gpio.h>
#include "accdet_drv.h"

#define SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE

/*----------------------------------------------------------------------
IOCTL
----------------------------------------------------------------------*/
#define ACCDET_DEVNAME "accdet"
#define ACCDET_IOC_MAGIC 'A'
#define ACCDET_INIT _IO(ACCDET_IOC_MAGIC, 0)
#define SET_CALL_STATE _IO(ACCDET_IOC_MAGIC, 1)
#define GET_BUTTON_STATUS _IO(ACCDET_IOC_MAGIC, 2)

/*define for phone call state*/

#define CALL_IDLE 0
#define CALL_RINGING 1
#define CALL_ACTIVE 2
#define KEY_CALL	KEY_SEND
#define KEY_ENDCALL	KEY_HANGEUL
/* extern struct headset_key_custom* get_headset_key_custom_setting(void); */

/*----------------------------------------------------------------------
static variable defination
----------------------------------------------------------------------*/
#define REGISTER_VALUE(x)   (x - 1)

static int debug_enable_drv = 1;
#define ACCDET_DEBUG(format, args...) do { \
	if (debug_enable_drv) {\
		pr_warn(format, ##args);\
	} \
} while (0)

static struct switch_dev accdet_data;
static struct input_dev *kpd_accdet_dev;
static struct cdev *accdet_cdev;
static struct class *accdet_class;
static struct device *accdet_nor_device;

static dev_t accdet_devno;

static int pre_status;
static int pre_state_swctrl;
static int accdet_status = PLUG_OUT;
static int cable_type;
#ifdef ACCDET_PIN_RECOGNIZATION
/* add for new feature PIN recognition */
static int cable_pin_recognition;
static int show_icon_delay;
#endif

/* ALPS443614: EINT trigger during accdet irq flow running, need add sync method */
/* between both */
static int eint_accdet_sync_flag;

static s64 long_press_time_ns;

static int g_accdet_first = 1;
static bool IRQ_CLR_FLAG = FALSE;
static volatile int call_status;
static volatile int button_status;


struct wake_lock accdet_suspend_lock;
struct wake_lock accdet_irq_lock;
struct wake_lock accdet_key_lock;
struct wake_lock accdet_timer_lock;


static struct work_struct accdet_work;
static struct workqueue_struct *accdet_workqueue;

static int long_press_time;

static DEFINE_MUTEX(accdet_eint_irq_sync_mutex);

#ifdef ACCDET_EINT
/* static int g_accdet_working_in_suspend =0; */

static struct work_struct accdet_eint_work;
static struct workqueue_struct *accdet_eint_workqueue;

static inline void accdet_init(void);

#ifdef ACCDET_LOW_POWER

#include <linux/timer.h>
#define MICBIAS_DISABLE_TIMER   (6 * HZ)
struct timer_list micbias_timer;
static void disable_micbias(unsigned long a);
/* Used to let accdet know if the pin has been fully plugged-in */
#define EINT_PIN_PLUG_IN        (1)
#define EINT_PIN_PLUG_OUT       (0)
int cur_eint_state = EINT_PIN_PLUG_OUT;
static struct work_struct accdet_disable_work;
static struct workqueue_struct *accdet_disable_workqueue;

#endif
#else
/* detect if remote button is short pressed or long pressed */
static bool is_long_press(void)
{
	int current_status = 0;
	int index = 0;
	int count = long_press_time / 100;
	while (index++ < count) {
		current_status = accdet_get_AB();
		if (current_status != 0)
			return false;

		msleep(100);
	}

	return true;
}

#endif


/* static int button_press_debounce = 0x400; */

#define DEBUG_THREAD 1
static struct platform_driver accdet_driver;


/****************************************************************/
/***        export function                                                                        **/
/****************************************************************/
void accdet_auxadc_switch_pmic(int enable)
{
	if (enable) {
#ifndef ACCDET_28V_MODE
		accdet_auxadc_19_switch(1);
		ACCDET_DEBUG("ACCDET enable switch in 1.9v mode\n");
#else
		accdet_auxadc_28_switch(1);
		ACCDET_DEBUG("ACCDET enable switch in 2.8v mode\n");
#endif
	} else {
#ifndef ACCDET_28V_MODE
		accdet_auxadc_19_switch(0);
		ACCDET_DEBUG("ACCDET diable switch in 1.9v mode\n");
#else
		accdet_auxadc_28_switch(0);
		ACCDET_DEBUG("ACCDET diable switch in 2.8v mode\n");
#endif
	}

}

void accdet_auxadc_switch_on(void)
{
#ifdef ACCDET_MULTI_KEY_FEATURE
	if (accdet_status == MIC_BIAS) {
		accdet_auxadc_switch(1);
		accdet_set_pwm_always_on();
	}
#endif
}

void accdet_workqueue_func(void)
{
	int ret;
	ret = queue_work(accdet_workqueue, &accdet_work);
	if (!ret)
		ACCDET_DEBUG("[Accdet]accdet_work return:%d!\n", ret);

}


void accdet_detect(void)
{
	int ret = 0;

	ACCDET_DEBUG("[Accdet]accdet_detect\n");

	accdet_status = PLUG_OUT;
	ret = queue_work(accdet_workqueue, &accdet_work);
	if (!ret)
		ACCDET_DEBUG("[Accdet]accdet_detect:accdet_work return:%d!\n", ret);

	return;
}
EXPORT_SYMBOL(accdet_detect);


void headset_plug_out(void)
{
	accdet_status = PLUG_OUT;
	cable_type = NO_DEVICE;
	/* update the cable_type */
	switch_set_state((struct switch_dev *)&accdet_data, cable_type);
	ACCDET_DEBUG(" [accdet] set state in cable_type = NO_DEVICE\n");

}



void accdet_state_reset(void)
{

	ACCDET_DEBUG("[Accdet]accdet_state_reset\n");

	accdet_status = PLUG_OUT;
	cable_type = NO_DEVICE;

	return;
}
EXPORT_SYMBOL(accdet_state_reset);

/****************************************************************/
/*******static function defination                             **/
/****************************************************************/

#ifdef ACCDET_PIN_RECOGNIZATION
void headset_standard_judge_message(void)
{
	ACCDET_DEBUG
	    ("[Accdet]Dear user: You plug in a headset which this phone doesn't support!!\n");

}
#endif

#ifdef ACCDET_EINT

void disable_accdet(void)
{
	/* int irq_temp = 0; */
	/* sync with accdet_irq_handler set clear accdet irq bit to avoid  set clear
	   accdet irq bit after disable accdet */
	/* disable accdet irq */
	accdet_disable_int();
	clear_accdet_interrupt();
	udelay(200);
	mutex_lock(&accdet_eint_irq_sync_mutex);
	while (accdet_get_irq_state()) {
		ACCDET_DEBUG("[Accdet]check_cable_type: Clear interrupt on-going....\n");
		msleep(20);
	}
	accdet_clear_irq_setbit();
	mutex_unlock(&accdet_eint_irq_sync_mutex);
	ACCDET_DEBUG("[Accdet]disable_accdet:Clear interrupt:Done[0x%x]!\n", accdet_get_irq());
	/* disable ACCDET unit */
	ACCDET_DEBUG("accdet: disable_accdet\n");
	pre_state_swctrl = accdet_get_swctrl();

	accdet_disable_hal();
	/* disable clock */
	accdet_disable_clk();
	/* unmask EINT */
	/* accdet_eint_unmask(); */
}

void enable_accdet(u32 state_swctrl)
{
	accdet_enable_hal(state_swctrl);
}


static void disable_micbias(unsigned long a)
{
	int ret = 0;
	ret = queue_work(accdet_disable_workqueue, &accdet_disable_work);
	if (!ret)
		ACCDET_DEBUG("[Accdet]disable_micbias:accdet_work return:%d!\n", ret);
}


static void disable_micbias_callback(struct work_struct *work)
{

	if (cable_type == HEADSET_NO_MIC) {
#ifdef ACCDET_PIN_RECOGNIZATION
		show_icon_delay = 0;
		cable_pin_recognition = 0;
		ACCDET_DEBUG("[Accdet] cable_pin_recognition = %d\n", cable_pin_recognition);
		accdet_set_pwm_width();
		accdet_set_pwm_thresh();
#endif
		/* setting pwm idle; */
#ifdef ACCDET_MULTI_KEY_FEATURE
		accdet_set_pwm_idle_off();
#endif
		disable_accdet();
		/* #ifdef ACCDET_LOW_POWER */
		/* wake_unlock(&accdet_timer_lock); */
		/* #endif */
		ACCDET_DEBUG("[Accdet] more than 5s MICBIAS : Disabled\n");
	}
#ifdef ACCDET_PIN_RECOGNIZATION
	else if (cable_type == HEADSET_MIC) {
		accdet_set_pwm_width();
		accdet_set_pwm_thresh();
		/* ACCDET_DEBUG("[Accdet]disable_micbias ACCDET_PWM_WIDTH=0x%x!\n",
		   pmic_pwrap_read(ACCDET_PWM_WIDTH)); */
		/* ACCDET_DEBUG("[Accdet]disable_micbias ACCDET_PWM_THRESH=0x%x!\n",
		   pmic_pwrap_read(ACCDET_PWM_THRESH)); */
		ACCDET_DEBUG("[Accdet]pin recog after 5s recover micbias polling!\n");
	}
#endif
}




void accdet_eint_work_callback(struct work_struct *work)
{
	/* KE under fastly plug in and plug out */
	accdet_eint_mask();

	if (cur_eint_state == EINT_PIN_PLUG_IN) {
		ACCDET_DEBUG("[Accdet]EINT func :plug-in\n");
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = 1;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
#ifdef ACCDET_LOW_POWER
		wake_lock_timeout(&accdet_timer_lock, 7 * HZ);
#endif
#ifdef ACCDET_28V_MODE
		accdet_set_28_mode();
		ACCDET_DEBUG("ACCDET use in 2.8V mode!!\n");
#endif
#ifdef ACCDET_PIN_SWAP
		accdet_enable_vrf28_power_on();
		msleep(800);
		accdet_FSA8049_enable();	/* enable GPIO209 for PIN swap */
		ACCDET_DEBUG("[Accdet] FSA8049 enable!\n");
		msleep(250);	/* PIN swap need ms */
#endif

		accdet_init();	/* do set pwm_idle on in accdet_init */

#ifdef ACCDET_PIN_RECOGNIZATION
		show_icon_delay = 1;
		/* micbias always on during detected PIN recognition */
		accdet_set_pwm_always_on();
		/* ACCDET_DEBUG("[Accdet]Pin recog ACCDET_PWM_WIDTH=0x%x!\n", pmic_pwrap_read(ACCDET_PWM_WIDTH)); */
		/* ACCDET_DEBUG("[Accdet]Pin recog ACCDET_PWM_THRESH=0x%x!\n", pmic_pwrap_read(ACCDET_PWM_THRESH)); */
		ACCDET_DEBUG("[Accdet]pin recog start!  micbias always on!\n");
#endif
		/* set PWM IDLE  on */
#ifdef ACCDET_MULTI_KEY_FEATURE
		accdet_set_pwm_idle_on();
		ACCDET_DEBUG
		    ("[Accdet]accdet_eint_work_callback plug in ACCDET PWM IDLE on ACCDET_STATE_SWCTRL = 0x%x!\n",
		     accdet_get_swctrl());
#endif
		/* enable ACCDET unit */
		enable_accdet(ACCDET_SWCTRL_EN);
	} else {
/* EINT_PIN_PLUG_OUT */
		/* Disable ACCDET */
		ACCDET_DEBUG("[Accdet]EINT func :plug-out\n");
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = 0;
		mutex_unlock(&accdet_eint_irq_sync_mutex);

#ifdef ACCDET_LOW_POWER
		del_timer_sync(&micbias_timer);
#endif
#ifdef ACCDET_PIN_RECOGNIZATION
		show_icon_delay = 0;
		cable_pin_recognition = 0;
#endif
#ifdef ACCDET_PIN_SWAP
		accdet_enable_vrf28_power_off();
		accdet_FSA8049_disable();	/* disable GPIO209 for PIN swap */
		ACCDET_DEBUG("[Accdet] FSA8049 disable!\n");
#endif
		accdet_auxadc_switch(0);
		disable_accdet();
		headset_plug_out();
#ifdef ACCDET_28V_MODE
		accdet_set_19_mode();
		ACCDET_DEBUG("ACCDET use in 1.9V mode!!\n");
#endif

	}
	/* unmask EINT */
	/* msleep(500); */
	accdet_eint_unmask();
	ACCDET_DEBUG("[Accdet]eint unmask  !!!!!!\n");

}


void accdet_eint_func(void)
{
	int ret = 0;
	if (cur_eint_state == EINT_PIN_PLUG_IN) {
		/*
		   To trigger EINT when the headset was plugged in
		   We set the polarity back as we initialed.
		 */
		accdet_eint_polarity_reverse_plugout();
		/* ACCDET_DEBUG("[Accdet]EINT func :plug-out\n"); */
		/* update the eint status */
		cur_eint_state = EINT_PIN_PLUG_OUT;
/* #ifdef ACCDET_LOW_POWER */
/* del_timer_sync(&micbias_timer); */
/* #endif */
	} else {
		/*
		   To trigger EINT when the headset was plugged out
		   We set the opposite polarity to what we initialed.
		 */
		accdet_eint_polarity_reverse_plugin();
		cur_eint_state = EINT_PIN_PLUG_IN;

#ifdef ACCDET_LOW_POWER
		/* INIT the timer to disable micbias. */
		init_timer(&micbias_timer);
		micbias_timer.expires = jiffies + MICBIAS_DISABLE_TIMER;
		micbias_timer.function = &disable_micbias;
		micbias_timer.data = ((unsigned long)0);
		add_timer(&micbias_timer);
#endif
	}
	ret = queue_work(accdet_eint_workqueue, &accdet_eint_work);
}


static inline int accdet_setup_eint(void)
{

	/*configure to GPIO function, external interrupt */
	ACCDET_DEBUG("[Accdet]accdet_setup_eint\n");
	accdet_eint_set();

	accdet_eint_registration();

	accdet_eint_unmask();


	return 0;

}


#endif

#ifdef ACCDET_MULTI_KEY_FEATURE

#define KEY_SAMPLE_PERIOD        (60)	/* ms */
/* #define MULTIKEY_ADC_CHANNEL   (5) */

#define NO_KEY			 (0x0)
#define UP_KEY			 (0x01)
#define MD_KEY			 (0x02)
#define DW_KEY			 (0x04)

#define SHORT_PRESS		 (0x0)
#define LONG_PRESS		 (0x10)
#define SHORT_UP                 ((UP_KEY) | SHORT_PRESS)
#define SHORT_MD		 ((MD_KEY) | SHORT_PRESS)
#define SHORT_DW                 ((DW_KEY) | SHORT_PRESS)
#define LONG_UP                  ((UP_KEY) | LONG_PRESS)
#define LONG_MD                  ((MD_KEY) | LONG_PRESS)
#define LONG_DW                  ((DW_KEY) | LONG_PRESS)

#define KEYDOWN_FLAG 1
#define KEYUP_FLAG 0
/* static int g_adcMic_channel_num =0; */


static DEFINE_MUTEX(accdet_multikey_mutex);



/*

	MD              UP                DW
|---------|-----------|----------|
0V<=MD< 0.09V<= UP<0.24V<=DW <0.5V

*/

#define DW_KEY_HIGH_THR	 (500)	/* 0.50v=500000uv */
#define DW_KEY_THR		 (240)	/* 0.24v=240000uv */
#define UP_KEY_THR       (90)	/* 0.09v=90000uv */
#define MD_KEY_THR		 (0)

static int key_check(int b)
{
	/* ACCDET_DEBUG("adc_data: %d v\n",b); */

	/* 0.24V ~ */
	ACCDET_DEBUG("accdet: come in key_check!!\n");
	if ((b < DW_KEY_HIGH_THR) && (b >= DW_KEY_THR)) {
		ACCDET_DEBUG("adc_data: %d mv\n", b);
		return DW_KEY;
	} else if ((b < DW_KEY_THR) && (b >= UP_KEY_THR)) {
		ACCDET_DEBUG("adc_data: %d mv\n", b);
		return UP_KEY;
	} else if ((b < UP_KEY_THR) && (b >= MD_KEY_THR)) {
		ACCDET_DEBUG("adc_data: %d mv\n", b);
		return MD_KEY;
	}
	ACCDET_DEBUG("accdet: leave key_check!!\n");
	return NO_KEY;
}

void send_key_event(int keycode, int flag)
{
#ifndef ACCDET_MULTI_KEY_ONLY_FOR_VOLUME
	if (call_status == 0) {
		switch (keycode) {
		case DW_KEY:
			input_report_key(kpd_accdet_dev, KEY_NEXTSONG, flag);
			input_sync(kpd_accdet_dev);
			ACCDET_DEBUG("KEY_NEXTSONG %d\n", flag);
			break;
		case UP_KEY:
			input_report_key(kpd_accdet_dev, KEY_PREVIOUSSONG, flag);
			input_sync(kpd_accdet_dev);
			ACCDET_DEBUG("KEY_PREVIOUSSONG %d\n", flag);
			break;
		}
	} else {
		switch (keycode) {
		case DW_KEY:
			input_report_key(kpd_accdet_dev, KEY_VOLUMEDOWN, flag);
			input_sync(kpd_accdet_dev);
			ACCDET_DEBUG("KEY_VOLUMEDOWN %d\n", flag);
			break;
		case UP_KEY:
			input_report_key(kpd_accdet_dev, KEY_VOLUMEUP, flag);
			input_sync(kpd_accdet_dev);
			ACCDET_DEBUG("KEY_VOLUMEUP %d\n", flag);
			break;
		}
	}
#else
	switch (keycode) {
	case DW_KEY:
		input_report_key(kpd_accdet_dev, KEY_VOLUMEDOWN, flag);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("KEY_VOLUMEDOWN %d\n", flag);
		break;
	case UP_KEY:
		input_report_key(kpd_accdet_dev, KEY_VOLUMEUP, flag);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("KEY_VOLUMEUP %d\n", flag);
		break;
	}

#endif
}

static int multi_key_detection(void)
{
	int current_status = 0;
	int index = 0;
	int count = long_press_time / (KEY_SAMPLE_PERIOD + 40);	/* ADC delay */
	int m_key = 0;
	int cur_key = 0;
	int cali_voltage = 0;

	ACCDET_DEBUG("[Accdet]adc before cali_voltage1\n");
	cali_voltage = accdet_PMIC_IMM_GetOneChannelValue();
	ACCDET_DEBUG("[Accdet]adc cali_voltage1 = %d mv\n", cali_voltage);
	/* ACCDET_DEBUG("[Accdet]adc cali_voltage final = %d mv\n", cali_voltage); */
	m_key = cur_key = key_check(cali_voltage);

	/*  */
	send_key_event(m_key, KEYDOWN_FLAG);

	while (index++ < count) {

		/* Check if the current state has been changed */
		current_status = accdet_get_AB();
		ACCDET_DEBUG("[Accdet]accdet current_status = %d\n", current_status);
		if (current_status != 0) {
			send_key_event(m_key, KEYUP_FLAG);
			return m_key | SHORT_PRESS;
		}

		/* Check if the voltage has been changed (press one key and another) */
		/* IMM_GetOneChannelValue(g_adcMic_channel_num, adc_data, &adc_raw); */
		cali_voltage = accdet_PMIC_IMM_GetOneChannelValue();
		ACCDET_DEBUG("[Accdet]adc in while loop [%d]= %d mv\n", index, cali_voltage);
		cur_key = key_check(cali_voltage);
		if (m_key != cur_key) {
			send_key_event(m_key, KEYUP_FLAG);
			ACCDET_DEBUG("[Accdet]accdet press one key and another happen!!\n");
			return m_key | SHORT_PRESS;
		} else {
			m_key = cur_key;
		}

		msleep(KEY_SAMPLE_PERIOD);
	}

	return m_key | LONG_PRESS;
}

#endif

/* clear ACCDET IRQ in accdet register */
void clear_accdet_interrupt(void)
{

	/* it is safe by using polling to adjust when to clear IRQ_CLR_BIT */
	accdet_set_irq();
	/* pmic_pwrap_write(INT_CON_ACCDET_CLR, (RG_ACCDET_IRQ_CLR)); */
	ACCDET_DEBUG("[Accdet]clear_accdet_interrupt: ACCDET_IRQ_STS = 0x%x\n", accdet_get_irq());
}

#ifdef SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE


#define    ACC_ANSWER_CALL      1
#define    ACC_END_CALL         2

#ifdef ACCDET_MULTI_KEY_FEATURE

#define    ACC_MEDIA_PLAYPAUSE  3
#define    ACC_MEDIA_STOP       4
#define    ACC_MEDIA_NEXT       5
#define    ACC_MEDIA_PREVIOUS   6
#define    ACC_VOLUMEUP   7
#define    ACC_VOLUMEDOWN   8

#else
#define    ACC_MEDIA_PLAYPAUSE  3

#endif

static atomic_t send_event_flag = ATOMIC_INIT(0);

static DECLARE_WAIT_QUEUE_HEAD(send_event_wq);


static int accdet_key_event;

static int sendKeyEvent(void *unuse)
{
	while (1) {
		ACCDET_DEBUG(" accdet:sendKeyEvent wait\n");
		/* wait for signal */
		wait_event_interruptible(send_event_wq, (atomic_read(&send_event_flag) != 0));

		wake_lock_timeout(&accdet_key_lock, 2 * HZ);	/* set the wake lock. */
		ACCDET_DEBUG(" accdet:going to send event %d\n", accdet_key_event);
#if 0
/* don't need this workaround because it could aviod press key during plug out from MT6589 */
/*
	Workaround to avoid holding the call when pluging out.
	Added a customized value to in/decrease the delay time.
	The longer delay, the more time key event would take.
*/
#ifdef KEY_EVENT_ISSUE_DELAY
		if (KEY_EVENT_ISSUE_DELAY >= 0)
			msleep(KEY_EVENT_ISSUE_DELAY);
		else
			msleep(500);
#else
		msleep(500);
#endif
#endif
		if (PLUG_OUT != accdet_status) {
			/* send key event */
			if (ACC_END_CALL == accdet_key_event) {
				ACCDET_DEBUG("[Accdet] end call!\n");
				input_report_key(kpd_accdet_dev, KEY_ENDCALL, 1);
				input_report_key(kpd_accdet_dev, KEY_ENDCALL, 0);
				input_sync(kpd_accdet_dev);
			}
#ifdef ACCDET_MULTI_KEY_FEATURE
			if (ACC_MEDIA_PLAYPAUSE == accdet_key_event) {
				ACCDET_DEBUG("[Accdet] PLAY_PAUSE !\n");
				input_report_key(kpd_accdet_dev, KEY_PLAYPAUSE, 1);
				input_report_key(kpd_accdet_dev, KEY_PLAYPAUSE, 0);
				input_sync(kpd_accdet_dev);
			}
/* next, previous, volumeup, volumedown send key in multi_key_detection() */
			if (ACC_MEDIA_NEXT == accdet_key_event)
				ACCDET_DEBUG("[Accdet] NEXT ..\n");

			if (ACC_MEDIA_PREVIOUS == accdet_key_event)
				ACCDET_DEBUG("[Accdet] PREVIOUS..\n");

			if (ACC_VOLUMEUP == accdet_key_event)
				ACCDET_DEBUG("[Accdet] VOLUMUP ..\n");

			if (ACC_VOLUMEDOWN == accdet_key_event)
				ACCDET_DEBUG("[Accdet] VOLUMDOWN..\n");

#else
			if (ACC_MEDIA_PLAYPAUSE == accdet_key_event) {
				ACCDET_DEBUG("[Accdet] PLAY_PAUSE !\n");
				input_report_key(kpd_accdet_dev, KEY_PLAYPAUSE, 1);
				input_report_key(kpd_accdet_dev, KEY_PLAYPAUSE, 0);
				input_sync(kpd_accdet_dev);
			}
#endif
		}
		atomic_set(&send_event_flag, 0);

		/* wake_unlock(&accdet_key_lock); //unlock wake lock */
	}
	return 0;
}

static ssize_t notify_sendKeyEvent(int event)
{

	accdet_key_event = event;
	atomic_set(&send_event_flag, 1);
	wake_up(&send_event_wq);
	ACCDET_DEBUG(" accdet:notify_sendKeyEvent !\n");
	return 0;
}


#endif

static inline void check_cable_type(void)
{
	int current_status = 0;
	int wait_clear_irq_times = 0;
	/* int irq_temp = 0; //for clear IRQ_bit */
#ifdef ACCDET_PIN_RECOGNIZATION
	int pin_adc_value = 0;
#define PIN_ADC_CHANNEL 5
#endif

	current_status = accdet_get_AB();	/* A=bit1; B=bit0 */
	ACCDET_DEBUG("[Accdet]accdet interrupt happen:[%s]current AB = %d\n",
		     accdet_status_string[accdet_status], current_status);

	button_status = 0;
	pre_status = accdet_status;

	ACCDET_DEBUG("[Accdet]check_cable_type: ACCDET_IRQ_STS = 0x%x\n", accdet_get_irq());
	IRQ_CLR_FLAG = FALSE;
	switch (accdet_status) {
	case PLUG_OUT:
#ifdef ACCDET_PIN_RECOGNIZATION
		accdet_set_debounce1();
#endif
		if (current_status == 0) {

			/* cable_type = HEADSET_NO_MIC; */
			/* accdet_status = HOOK_SWITCH; */
#ifdef ACCDET_PIN_RECOGNIZATION
			/* micbias always on during detected PIN recognition */
			accdet_set_pwm_always_on();
			ACCDET_DEBUG("[Accdet]PIN recognition micbias always on!\n");


			ACCDET_DEBUG("[Accdet]before adc read, pin_adc_value = %d mv!\n",
				     pin_adc_value);
			msleep(1000);
			current_status = accdet_get_AB();	/* A=bit1; B=bit0 */
			if (current_status == 0 && show_icon_delay != 0) {
				/* while(i<10) { */
				/* msleep(200); */

				accdet_auxadc_switch(1);	/* switch on when need to use auxadc read voltage */

				pin_adc_value = accdet_PMIC_IMM_GetOneChannelValue();

				accdet_auxadc_switch(0);

				/* ACCDET_DEBUG("[Accdet]pin recognition times, pin_adc_value = %d mv!\n",
				   pin_adc_value); */
				/* i++; */
				/* } */
				/* i = 0; */
				if (200 > pin_adc_value && pin_adc_value > 100)	/* 100mv   ilegal headset */
					/* if (pin_adc_value> 100) */
				{
					headset_standard_judge_message();
					/* cable_type = HEADSET_ILEGAL; */
					/* accdet_status = MIC_BIAS_ILEGAL; */
					mutex_lock(&accdet_eint_irq_sync_mutex);
					if (1 == eint_accdet_sync_flag) {
						cable_type = HEADSET_NO_MIC;
						accdet_status = HOOK_SWITCH;
						cable_pin_recognition = 1;
						ACCDET_DEBUG
						    ("[Accdet] cable_pin_recognition = %d\n",
						     cable_pin_recognition);
					} else {
						ACCDET_DEBUG("[Accdet] Headset has plugged out\n");
					}
					mutex_unlock(&accdet_eint_irq_sync_mutex);
				} else {
					mutex_lock(&accdet_eint_irq_sync_mutex);
					if (1 == eint_accdet_sync_flag) {
						cable_type = HEADSET_NO_MIC;
						accdet_status = HOOK_SWITCH;
					} else {
						ACCDET_DEBUG("[Accdet] Headset has plugged out\n");
					}
					mutex_unlock(&accdet_eint_irq_sync_mutex);
				}
			}
#else
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				cable_type = HEADSET_NO_MIC;
				accdet_status = HOOK_SWITCH;
			} else {
				ACCDET_DEBUG("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#endif
		} else if (current_status == 1) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
			} else {
				ACCDET_DEBUG("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
			/* ALPS00038030:reduce the time of remote button pressed during incoming call */
			/* solution: reduce hook switch debounce time to 0x400 */
			accdet_set_debounce0_reduce();
			/* recover polling set AB 00-01 */
#ifdef ACCDET_PIN_RECOGNIZATION
			accdet_set_pwm_width();
			accdet_set_pwm_thresh();
#endif
			/* #ifdef ACCDET_LOW_POWER */
			/* wake_unlock(&accdet_timer_lock);//add for suspend disable accdet more than 5S */
			/* #endif */
		} else if (current_status == 3) {
			ACCDET_DEBUG("[Accdet]PLUG_OUT state not change!\n");
#ifdef ACCDET_MULTI_KEY_FEATURE
			ACCDET_DEBUG("[Accdet] do not send plug out event in plug out\n");
#else
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				accdet_status = PLUG_OUT;
				cable_type = NO_DEVICE;
			} else {
				ACCDET_DEBUG("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#ifdef ACCDET_EINT
			disable_accdet();
#endif
#endif
		} else {
			ACCDET_DEBUG("[Accdet]PLUG_OUT can't change to this state!\n");
		}
		break;

	case MIC_BIAS:
		/* ALPS00038030:reduce the time of remote button pressed during incoming call */
		/* solution: resume hook switch debounce time */
		accdet_set_debounce0();

		if (current_status == 0) {
			/* ACCDET_DEBUG("[Accdet]remote button pressed,call_status:%d\n", call_status); */
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				while (accdet_get_irq_state() && wait_clear_irq_times < 3) {
					wait_clear_irq_times++;
					ACCDET_DEBUG
					    ("[Accdet]check_cable_type: MIC BIAS clear IRQ on-going1....\n");
					msleep(20);
				}
				accdet_clear_irq_setbit();
				IRQ_CLR_FLAG = TRUE;
				accdet_status = HOOK_SWITCH;
			} else {
				ACCDET_DEBUG("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
			button_status = 1;
			if (button_status) {
#ifdef SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE

#ifdef ACCDET_MULTI_KEY_FEATURE

				int multi_key = NO_KEY;

				mdelay(10);
				/* ACCDET_DEBUG("[Accdet] delay 10ms to wait cap charging!!\n"); */
				/* if plug out don't send key */
				mutex_lock(&accdet_eint_irq_sync_mutex);
				if (1 == eint_accdet_sync_flag) {
					multi_key = multi_key_detection();
				} else {
					ACCDET_DEBUG
					    ("[Accdet] multi_key_detection: Headset has plugged out\n");
				}
				mutex_unlock(&accdet_eint_irq_sync_mutex);
				accdet_auxadc_switch(0);
				/* init  pwm frequency and duty */
				accdet_set_pwm_width();
				accdet_set_pwm_thresh();
				/* ACCDET_DEBUG("[Accdet]accdet recover ACCDET_PWM_WIDTH=0x%x!\n",
				   pmic_pwrap_read(ACCDET_PWM_WIDTH)); */
				/* ACCDET_DEBUG("[Accdet]accdet recover ACCDET_PWM_THRESH=0x%x!\n",
				   pmic_pwrap_read(ACCDET_PWM_THRESH)); */


				switch (multi_key) {
				case SHORT_UP:
					ACCDET_DEBUG("[Accdet] Short press up (0x%x)\n", multi_key);
					if (call_status == 0)
						notify_sendKeyEvent(ACC_MEDIA_PREVIOUS);
					else
						notify_sendKeyEvent(ACC_VOLUMEUP);

					break;
				case SHORT_MD:
					ACCDET_DEBUG("[Accdet] Short press middle (0x%x)\n",
						     multi_key);
					notify_sendKeyEvent(ACC_MEDIA_PLAYPAUSE);
					break;
				case SHORT_DW:
					ACCDET_DEBUG("[Accdet] Short press down (0x%x)\n",
						     multi_key);
					if (call_status == 0)
						notify_sendKeyEvent(ACC_MEDIA_NEXT);
					else
						notify_sendKeyEvent(ACC_VOLUMEDOWN);

					break;
				case LONG_UP:
					ACCDET_DEBUG("[Accdet] Long press up (0x%x)\n", multi_key);

					send_key_event(UP_KEY, KEYUP_FLAG);

					break;
				case LONG_MD:
					ACCDET_DEBUG("[Accdet] Long press middle (0x%x)\n",
						     multi_key);
					notify_sendKeyEvent(ACC_END_CALL);
					break;
				case LONG_DW:
					ACCDET_DEBUG("[Accdet] Long press down (0x%x)\n",
						     multi_key);

					send_key_event(DW_KEY, KEYUP_FLAG);

					break;
				default:
					ACCDET_DEBUG("[Accdet] unkown key (0x%x)\n", multi_key);
					break;
				}
#else
				if (call_status != 0) {
					if (is_long_press()) {
						ACCDET_DEBUG
						    ("[Accdet]send long press remote button event %d\n",
						     ACC_END_CALL);
						notify_sendKeyEvent(ACC_END_CALL);
					} else {
						ACCDET_DEBUG
						    ("[Accdet]send short press remote button event %d\n",
						     ACC_ANSWER_CALL);
						notify_sendKeyEvent(ACC_MEDIA_PLAYPAUSE);
					}
				}
#endif				/* //end  ifdef ACCDET_MULTI_KEY_FEATURE else */

#else
				if (call_status != 0) {
					if (is_long_press()) {
						ACCDET_DEBUG
						    ("[Accdet]long press remote button to end call!\n");
						input_report_key(kpd_accdet_dev, KEY_ENDCALL, 1);
						input_report_key(kpd_accdet_dev, KEY_ENDCALL, 0);
						input_sync(kpd_accdet_dev);
					} else {
						ACCDET_DEBUG
						    ("[Accdet]short press remote button to accept call!\n");
						input_report_key(kpd_accdet_dev, KEY_PLAYPAUSE, 1);
						input_report_key(kpd_accdet_dev, KEY_PLAYPAUSE, 0);
						input_sync(kpd_accdet_dev);
					}
				}
#endif				/* #ifdef SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE */
			}
		} else if (current_status == 1) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
				ACCDET_DEBUG("[Accdet]MIC_BIAS state not change!\n");
			} else {
				ACCDET_DEBUG("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (current_status == 3) {
#ifdef ACCDET_MULTI_KEY_FEATURE
			ACCDET_DEBUG("[Accdet]do not send plug ou in micbiast\n");
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag)
				accdet_status = PLUG_OUT;
			else
				ACCDET_DEBUG("[Accdet] Headset has plugged out\n");

			mutex_unlock(&accdet_eint_irq_sync_mutex);
#else
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				accdet_status = PLUG_OUT;
				cable_type = NO_DEVICE;
			} else
				ACCDET_DEBUG("[Accdet] Headset has plugged out\n");

			mutex_unlock(&accdet_eint_irq_sync_mutex);
#ifdef ACCDET_EINT
			disable_accdet();
#endif
#endif
		} else {
			ACCDET_DEBUG("[Accdet]MIC_BIAS can't change to this state!\n");
		}
		break;

	case HOOK_SWITCH:
		if (current_status == 0) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				/* for avoid 01->00 framework of Headset will report press key info for Audio */
				/* cable_type = HEADSET_NO_MIC; */
				/* accdet_status = HOOK_SWITCH; */
				ACCDET_DEBUG("[Accdet]HOOK_SWITCH state not change!\n");
			} else {
				ACCDET_DEBUG("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (current_status == 1) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
			} else {
				ACCDET_DEBUG("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#ifdef ACCDET_PIN_RECOGNIZATION
			cable_pin_recognition = 0;
			ACCDET_DEBUG("[Accdet] cable_pin_recognition = %d\n",
				     cable_pin_recognition);
			accdet_set_pwm_width();
			accdet_set_pwm_thresh();
#endif
			/* ALPS00038030:reduce the time of remote button pressed during incoming call */
			/* solution: reduce hook switch debounce time to 0x400 */
			accdet_set_debounce0_reduce();
			/* #ifdef ACCDET_LOW_POWER */
			/* wake_unlock(&accdet_timer_lock);//add for suspend disable accdet more than 5S */
			/* #endif */
		} else if (current_status == 3) {

#ifdef ACCDET_PIN_RECOGNIZATION
			cable_pin_recognition = 0;
			ACCDET_DEBUG("[Accdet] cable_pin_recognition = %d\n",
				     cable_pin_recognition);
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag)
				accdet_status = PLUG_OUT;
			else
				ACCDET_DEBUG("[Accdet] Headset has plugged out\n");

			mutex_unlock(&accdet_eint_irq_sync_mutex);
#endif
#ifdef ACCDET_MULTI_KEY_FEATURE
			ACCDET_DEBUG("[Accdet] do not send plug out event in hook switch\n");
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag)
				accdet_status = PLUG_OUT;
			else
				ACCDET_DEBUG("[Accdet] Headset has plugged out\n");

			mutex_unlock(&accdet_eint_irq_sync_mutex);
#else
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				accdet_status = PLUG_OUT;
				cable_type = NO_DEVICE;
			} else {
				ACCDET_DEBUG("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#ifdef ACCDET_EINT
			disable_accdet();
#endif
#endif
		} else {
			ACCDET_DEBUG("[Accdet]HOOK_SWITCH can't change to this state!\n");
		}
		break;


	case STAND_BY:
		if (current_status == 3) {
#ifdef ACCDET_MULTI_KEY_FEATURE
			ACCDET_DEBUG("[Accdet]accdet do not send plug out event in stand by!\n");
#else
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				accdet_status = PLUG_OUT;
				cable_type = NO_DEVICE;
			} else {
				ACCDET_DEBUG("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#ifdef ACCDET_EINT
			disable_accdet();
#endif
#endif
		} else {
			ACCDET_DEBUG("[Accdet]STAND_BY can't change to this state!\n");
		}
		break;

	default:
		ACCDET_DEBUG("[Accdet]check_cable_type: accdet current status error!\n");
		break;

	}

	if (!IRQ_CLR_FLAG) {
		mutex_lock(&accdet_eint_irq_sync_mutex);
		if (1 == eint_accdet_sync_flag) {
			while (accdet_get_irq_state() && wait_clear_irq_times < 3) {
				wait_clear_irq_times++;
				ACCDET_DEBUG
				    ("[Accdet]check_cable_type: Clear interrupt on-going2....\n");
				msleep(20);
			}
		}
		accdet_clear_irq_setbit();
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		IRQ_CLR_FLAG = TRUE;
		ACCDET_DEBUG("[Accdet]check_cable_type:Clear interrupt:Done[0x%x]!\n",
			     accdet_get_irq());
	} else {
		IRQ_CLR_FLAG = FALSE;
	}

	ACCDET_DEBUG("[Accdet]cable type:[%s], status switch:[%s]->[%s]\n",
		     accdet_report_string[cable_type], accdet_status_string[pre_status],
		     accdet_status_string[accdet_status]);
}

void accdet_work_callback(struct work_struct *work)
{

	wake_lock(&accdet_irq_lock);
	check_cable_type();

	mutex_lock(&accdet_eint_irq_sync_mutex);
	if (1 == eint_accdet_sync_flag)
		switch_set_state((struct switch_dev *)&accdet_data, cable_type);
	else
		ACCDET_DEBUG("[Accdet] Headset has plugged out don't set accdet state\n");

	mutex_unlock(&accdet_eint_irq_sync_mutex);
	ACCDET_DEBUG(" [accdet] set state in cable_type  status:%d\n", cable_type);

	wake_unlock(&accdet_irq_lock);
}

#if 0
int accdet_irq_handler(void)
{
	int ret = 0;
	/* sync with disable_accdet */
	/* mutex_lock(&accdet_eint_irq_sync_mutex); */
	/* if(1 == eint_accdet_sync_flag) { */
	clear_accdet_interrupt();
	/* }else { */
	/* ACCDET_DEBUG("[Accdet] Headset has plugged out don't clear irq bit\n"); */
	/* } */
	/* mutex_unlock(&accdet_eint_irq_sync_mutex); */
#ifdef ACCDET_MULTI_KEY_FEATURE
	if (accdet_status == MIC_BIAS) {
		accdet_auxadc_switch(1);
		accdet_set_pwm_always_on();
	}
#endif
	ret = queue_work(accdet_workqueue, &accdet_work);
	if (!ret)
		ACCDET_DEBUG("[Accdet]accdet_irq_handler:accdet_work return:%d!\n", ret);

	return 1;
}
#endif
/* ACCDET hardware initial */
static inline void accdet_init(void)
{
	/* unsigned int val1 = 0; */
	ACCDET_DEBUG("[Accdet]accdet hardware init\n");

	/* enable_clock(MT65XX_PDN_PERI_ACCDET,"ACCDET"); */
	accdet_enable_clk();
	accdet_get_clk_log();
	/* reset the accdet unit */

	ACCDET_DEBUG("ACCDET reset : reset start!!\n\r");
	accdet_RST_set();
	ACCDET_DEBUG("ACCDET reset function test: reset finished!!\n\r");
	accdet_RST_clr();

	/* accdet IRQ enable,  PMIC driver has done this for accdet driver!!(pmic_mt6320.c) */
	accdet_enable_int();
	accdet_int_log();

	/* init  pwm frequency and duty */
	accdet_set_pwm_width();
	accdet_set_pwm_thresh();

	accdet_set_pwm_enable();


	accdet_set_pwm_delay();

	/* init the debounce time */
#ifdef ACCDET_PIN_RECOGNIZATION
	accdet_set_debounce0();
	accdet_set_debounce1_pin_recognition();
	accdet_set_debounce3();
#else
	accdet_set_debounce0();
	accdet_set_debounce1();
	accdet_set_debounce3();
#endif

	accdet_clear_irq_setbit();

#ifdef ACCDET_EINT
	/* disable ACCDET unit */
	pre_state_swctrl = accdet_get_swctrl();
	accdet_disable_hal();
	accdet_disable_clk();
#else

	/* enable ACCDET unit */
	/* pmic_pwrap_write(ACCDET_STATE_SWCTRL, ACCDET_SWCTRL_EN); */
	accdet_enable_RG();
#endif

#ifdef GPIO_FSA8049_PIN
	/* mt_set_gpio_out(GPIO_FSA8049_PIN, GPIO_OUT_ONE); */
#endif
#ifdef FSA8049_V_POWER
	accdet_enable_power();
#endif

}

static long accdet_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
/* bool ret = true; */

	switch (cmd) {
	case ACCDET_INIT:
		break;

	case SET_CALL_STATE:
		call_status = (int)arg;
		ACCDET_DEBUG("[Accdet]accdet_ioctl : CALL_STATE=%d\n", call_status);
		break;

	case GET_BUTTON_STATUS:
		ACCDET_DEBUG("[Accdet]accdet_ioctl : Button_Status=%d (state:%d)\n", button_status,
			     accdet_data.state);
		return button_status;

	default:
		ACCDET_DEBUG("[Accdet]accdet_ioctl : default\n");
		break;
	}
	return 0;
}

static int accdet_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int accdet_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations accdet_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = accdet_unlocked_ioctl,
	.open = accdet_open,
	.release = accdet_release,
};

static ssize_t accdet_store_call_state(struct device_driver *ddri, const char *buf, size_t count)
{
	/* int state; */
/* int error; */

	if (sscanf(buf, "%u", &call_status) != 1) {
		ACCDET_DEBUG("accdet: Invalid values\n");
		return -EINVAL;
	}

	switch (call_status) {
	case CALL_IDLE:
		ACCDET_DEBUG("[Accdet]accdet call: Idle state!\n");
		break;

	case CALL_RINGING:

		ACCDET_DEBUG("[Accdet]accdet call: ringing state!\n");
		break;

	case CALL_ACTIVE:
		ACCDET_DEBUG("[Accdet]accdet call: active or hold state!\n");
		ACCDET_DEBUG("[Accdet]accdet_ioctl : Button_Status=%d (state:%d)\n", button_status,
			     accdet_data.state);
		/* return button_status; */
		break;

	default:
		ACCDET_DEBUG("[Accdet]accdet call : Invalid values\n");
		break;
	}


	return count;
}

/* #ifdef ACCDET_PIN_RECOGNIZATION */

static ssize_t show_pin_recognition_state(struct device_driver *ddri, char *buf)
{
#ifdef ACCDET_PIN_RECOGNIZATION
	ACCDET_DEBUG("ACCDET show_pin_recognition_state = %d\n", cable_pin_recognition);
	return sprintf(buf, "%u\n", cable_pin_recognition);
#else
	return sprintf(buf, "%u\n", 0);
#endif
}

static DRIVER_ATTR(accdet_pin_recognition, 0664, show_pin_recognition_state, NULL);
/* #endif */

static DRIVER_ATTR(accdet_call_state, 0664, NULL, accdet_store_call_state);
#if DEBUG_THREAD
int g_start_debug_thread = 0;
static struct task_struct *thread;
int g_dump_register = 0;


static int dbug_thread(void *unused)
{
	while (g_start_debug_thread) {
		/* ACCDET_DEBUG("dbug_thread INREG32(ACCDET_BASE + 0x0008)=%x\n",INREG32(ACCDET_BASE + 0x0008)); */
		/* ACCDET_DEBUG("[Accdet]dbug_thread:sample_in:%x!\n", INREG32(ACCDET_STATE_RG)&(0x03<<4)); */
		/* ACCDET_DEBUG("[Accdet]dbug_thread:curr_in:%x!\n", INREG32(ACCDET_STATE_RG)&(0x03<<2)); */
		/* ACCDET_DEBUG("[Accdet]dbug_thread:mem_in:%x!\n", INREG32(ACCDET_STATE_RG)&(0x03<<6)); */
		/* ACCDET_DEBUG("[Accdet]dbug_thread:0x059C=0x%x!\n", pmic_pwrap_read(ACCDET_STATE_RG)); */
		/* ACCDET_DEBUG("[Accdet]dbug_thread:IRQ:%x!\n", pmic_pwrap_read(ACCDET_IRQ_STS)); */
		if (g_dump_register) {
			dump_register();
			/* dump_pmic_register(); */
		}

		msleep(500);

	}
	return 0;
}

/* static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count) */


static ssize_t store_accdet_start_debug_thread(struct device_driver *ddri, const char *buf,
					       size_t count)
{

	unsigned int start_flag;
	int error;

	if (sscanf(buf, "%u", &start_flag) != 1) {
		ACCDET_DEBUG("accdet: Invalid values\n");
		return -EINVAL;
	}

	ACCDET_DEBUG("[Accdet] start flag =%d\n", start_flag);

	g_start_debug_thread = start_flag;

	if (1 == start_flag) {
		thread = kthread_run(dbug_thread, 0, "ACCDET");
		if (IS_ERR(thread)) {
			error = PTR_ERR(thread);
			ACCDET_DEBUG(" failed to create kernel thread: %d\n", error);
		}
	}

	return count;
}

static ssize_t store_accdet_set_headset_mode(struct device_driver *ddri, const char *buf,
					     size_t count)
{

	unsigned int value;
	/* int error; */

	if (sscanf(buf, "%u", &value) != 1) {
		ACCDET_DEBUG("accdet: Invalid values\n");
		return -EINVAL;
	}

	ACCDET_DEBUG("[Accdet]store_accdet_set_headset_mode value =%d\n", value);

	return count;
}

static ssize_t store_accdet_dump_register(struct device_driver *ddri, const char *buf, size_t count)
{
	unsigned int value;
/* int error; */

	if (sscanf(buf, "%u", &value) != 1) {
		ACCDET_DEBUG("accdet: Invalid values\n");
		return -EINVAL;
	}

	g_dump_register = value;

	ACCDET_DEBUG("[Accdet]store_accdet_dump_register value =%d\n", value);

	return count;
}




/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(dump_register, S_IWUSR | S_IRUGO, NULL, store_accdet_dump_register);

static DRIVER_ATTR(set_headset_mode, S_IWUSR | S_IRUGO, NULL, store_accdet_set_headset_mode);

static DRIVER_ATTR(start_debug, S_IWUSR | S_IRUGO, NULL, store_accdet_start_debug_thread);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *accdet_attr_list[] = {
	&driver_attr_start_debug,
	&driver_attr_set_headset_mode,
	&driver_attr_dump_register,
	&driver_attr_accdet_call_state,
	/* #ifdef ACCDET_PIN_RECOGNIZATION */
	&driver_attr_accdet_pin_recognition,
	/* #endif */
};

static int accdet_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(accdet_attr_list) / sizeof(accdet_attr_list[0]));
	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, accdet_attr_list[idx]);
		if (err) {
			ACCDET_DEBUG("driver_create_file (%s) = %d\n",
				     accdet_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

#endif

static int accdet_probe(struct platform_device *dev)
{
	int ret = 0;
#ifdef SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE
	struct task_struct *keyEvent_thread = NULL;
	int error = 0;
#endif

	struct headset_key_custom *press_key_time = accdet_get_headset_key_custom_setting();

	/* ACCDET_DEBUG("[Accdet]accdet_probe begin!\n"); */


	/* ------------------------------------------------------------------ */
	/* below register accdet as switch class */
	/* ------------------------------------------------------------------ */
	accdet_data.name = "h2w";
	accdet_data.index = 0;
	accdet_data.state = NO_DEVICE;

	set_cust_headset_settings_hal();
	set_cust_headset_data_hal();

	ret = switch_dev_register(&accdet_data);
	if (ret) {
		ACCDET_DEBUG("[Accdet]switch_dev_register returned:%d!\n", ret);
		return 1;
	}
	/* ------------------------------------------------------------------ */
	/* Create normal device for auido use */
	/* ------------------------------------------------------------------ */
	ret = alloc_chrdev_region(&accdet_devno, 0, 1, ACCDET_DEVNAME);
	if (ret)
		ACCDET_DEBUG("[Accdet]alloc_chrdev_region: Get Major number error!\n");

	accdet_cdev = cdev_alloc();
	accdet_cdev->owner = THIS_MODULE;
	accdet_cdev->ops = &accdet_fops;
	ret = cdev_add(accdet_cdev, accdet_devno, 1);
	if (ret)
		ACCDET_DEBUG("[Accdet]accdet error: cdev_add\n");

	accdet_class = class_create(THIS_MODULE, ACCDET_DEVNAME);

	/* if we want auto creat device node, we must call this */
	accdet_nor_device = device_create(accdet_class, NULL, accdet_devno, NULL, ACCDET_DEVNAME);

	/* ------------------------------------------------------------------ */
	/* Create input device */
	/* ------------------------------------------------------------------ */
	kpd_accdet_dev = input_allocate_device();
	if (!kpd_accdet_dev) {
		ACCDET_DEBUG("[Accdet]kpd_accdet_dev : fail!\n");
		return -ENOMEM;
	}
	/* define multi-key keycode */
	__set_bit(EV_KEY, kpd_accdet_dev->evbit);
	__set_bit(KEY_CALL, kpd_accdet_dev->keybit);
	__set_bit(KEY_ENDCALL, kpd_accdet_dev->keybit);
	__set_bit(KEY_NEXTSONG, kpd_accdet_dev->keybit);
	__set_bit(KEY_PREVIOUSSONG, kpd_accdet_dev->keybit);
	__set_bit(KEY_PLAYPAUSE, kpd_accdet_dev->keybit);
	__set_bit(KEY_STOPCD, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOLUMEDOWN, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOLUMEUP, kpd_accdet_dev->keybit);

	kpd_accdet_dev->id.bustype = BUS_HOST;
	kpd_accdet_dev->name = "ACCDET";
	if (input_register_device(kpd_accdet_dev))
		ACCDET_DEBUG("[Accdet]kpd_accdet_dev register : fail!\n");
	else
		ACCDET_DEBUG("[Accdet]kpd_accdet_dev register : success!!\n");

	/* ------------------------------------------------------------------ */
	/* Create workqueue */
	/* ------------------------------------------------------------------ */
	accdet_workqueue = create_singlethread_workqueue("accdet");
	INIT_WORK(&accdet_work, accdet_work_callback);


	/* ------------------------------------------------------------------ */
	/* wake lock */
	/* ------------------------------------------------------------------ */
	wake_lock_init(&accdet_suspend_lock, WAKE_LOCK_SUSPEND, "accdet wakelock");
	wake_lock_init(&accdet_irq_lock, WAKE_LOCK_SUSPEND, "accdet irq wakelock");
	wake_lock_init(&accdet_key_lock, WAKE_LOCK_SUSPEND, "accdet key wakelock");
	wake_lock_init(&accdet_timer_lock, WAKE_LOCK_SUSPEND, "accdet timer wakelock");
#ifdef SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE
	init_waitqueue_head(&send_event_wq);
	/* start send key event thread */
	keyEvent_thread = kthread_run(sendKeyEvent, 0, "keyEvent_send");
	if (IS_ERR(keyEvent_thread)) {
		error = PTR_ERR(keyEvent_thread);
		ACCDET_DEBUG(" failed to create kernel thread: %d\n", error);
	}
#endif

#if DEBUG_THREAD
	ret = accdet_create_attr(&accdet_driver.driver);
	if (ret)
		ACCDET_DEBUG("create attribute err = %d\n", ret);

#endif


	long_press_time = press_key_time->headset_long_press_time;

	ACCDET_DEBUG("[Accdet]accdet_probe : ACCDET_INIT\n");
	if (g_accdet_first == 1) {
		long_press_time_ns = (s64) long_press_time * NSEC_PER_MSEC;

		eint_accdet_sync_flag = 1;

		/* Accdet Hardware Init */
		accdet_set_19_mode();
		ACCDET_DEBUG("ACCDET use in 1.9V mode!!\n");
		accdet_init();
		accdet_set_irq();
		accdet_irq_register();
		queue_work(accdet_workqueue, &accdet_work);	/* schedule a work for the first detection */
#ifdef ACCDET_EINT

		accdet_eint_workqueue = create_singlethread_workqueue("accdet_eint");
		INIT_WORK(&accdet_eint_work, accdet_eint_work_callback);
		accdet_setup_eint();
		accdet_disable_workqueue = create_singlethread_workqueue("accdet_disable");
		INIT_WORK(&accdet_disable_work, disable_micbias_callback);

#endif
		g_accdet_first = 0;
	}

	ACCDET_DEBUG("[Accdet]accdet_probe done!\n");
/* #ifdef ACCDET_PIN_SWAP */
	/* pmic_pwrap_write(0x0400, 0x1000); */
	/* ACCDET_DEBUG("[Accdet]accdet enable VRF28 power!\n"); */
/* #endif */

	return 0;
}

static int accdet_remove(struct platform_device *dev)
{
	ACCDET_DEBUG("[Accdet]accdet_remove begin!\n");

	/* cancel_delayed_work(&accdet_work); */
#ifdef ACCDET_EINT
	destroy_workqueue(accdet_eint_workqueue);
#endif
	destroy_workqueue(accdet_workqueue);
	switch_dev_unregister(&accdet_data);
	device_del(accdet_nor_device);
	class_destroy(accdet_class);
	cdev_del(accdet_cdev);
	unregister_chrdev_region(accdet_devno, 1);
	input_unregister_device(kpd_accdet_dev);
	ACCDET_DEBUG("[Accdet]accdet_remove Done!\n");

	return 0;
}

static int accdet_suspend(struct device *device)	/* only one suspend mode */
{

/* #ifdef ACCDET_PIN_SWAP */
/* pmic_pwrap_write(0x0400, 0x0); */
/* accdet_FSA8049_disable(); */
/* #endif */

#ifdef ACCDET_MULTI_KEY_FEATURE
	/* ACCDET_DEBUG("[Accdet] in suspend1: ACCDET_IRQ_STS = 0x%x\n", pmic_pwrap_read(ACCDET_IRQ_STS)); */
#else
	/* int i=0; */
	/* int irq_temp_resume =0; */
#if 0
	while (pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT) {
		ACCDET_DEBUG("[Accdet]: Clear interrupt on-going3....\n");
		msleep(20);
	}
	while (pmic_pwrap_read(ACCDET_IRQ_STS)) {
		msleep(20);
		irq_temp_resume = pmic_pwrap_read(ACCDET_IRQ_STS);
		irq_temp_resume = irq_temp_resume & (~IRQ_CLR_BIT);
		ACCDET_DEBUG("[Accdet]check_cable_type in mic_bias irq_temp_resume = 0x%x\n",
			     irq_temp_resume);
		pmic_pwrap_write(ACCDET_IRQ_STS, irq_temp_resume);
		/* CLRREG32(INT_CON_ACCDET_CLR, RG_ACCDET_IRQ_CLR); //clear 6320 irq status */
		IRQ_CLR_FLAG = TRUE;
		ACCDET_DEBUG("[Accdet]:Clear interrupt:Done[0x%x]!\n",
			     pmic_pwrap_read(ACCDET_IRQ_STS));
	}

	while (i < 50) {
		/* wake lock */
		wake_lock(&accdet_suspend_lock);
		msleep(20);	/* wait for accdet finish IRQ generation */
		g_accdet_working_in_suspend = 1;
		ACCDET_DEBUG("suspend wake lock %d\n", i);
		i++;
	}
	if (1 == g_accdet_working_in_suspend) {
		wake_unlock(&accdet_suspend_lock);
		g_accdet_working_in_suspend = 0;
		ACCDET_DEBUG("suspend wake unlock\n");
	}
#endif
/*
	ACCDET_DEBUG("[Accdet]suspend:sample_in:%x, curr_in:%x, mem_in:%x, FSM:%x\n"
		, INREG32(ACCDET_SAMPLE_IN)
		,INREG32(ACCDET_CURR_IN)
		,INREG32(ACCDET_MEMORIZED_IN)
		,INREG32(ACCDET_BASE + 0x0050));
*/

#ifdef ACCDET_EINT

	if (accdet_get_enable_RG() && call_status == 0) {
		/* record accdet status */
		/* ACCDET_DEBUG("[Accdet]accdet_working_in_suspend\n"); */
		pr_debug("[Accdet]accdet_working_in_suspend\n");
		g_accdet_working_in_suspend = 1;
		pre_state_swctrl = accdet_get_swctrl();
		/* disable ACCDET unit */
		accdet_disable_hal();
		/* disable_clock */
		accdet_disable_clk();
	}
#else
	/* disable ACCDET unit */
	if (call_status == 0) {
		pre_state_swctrl = accdet_get_swctrl();
		accdet_disable_hal();
		/* disable_clock */
		accdet_disable_clk();
	}
#endif

	/* ACCDET_DEBUG("[Accdet]accdet_suspend: ACCDET_CTRL=[0x%x], STATE=[0x%x]->[0x%x]\n",
	   pmic_pwrap_read(ACCDET_CTRL), pre_state_swctrl, pmic_pwrap_read(ACCDET_STATE_SWCTRL)); */
	pr_debug("[Accdet]accdet_suspend: ACCDET_CTRL=[0x%x], STATE=[0x%x]->[0x%x]\n",
		 accdet_get_ctrl(), pre_state_swctrl, accdet_get_swctrl());
	/* ACCDET_DEBUG("[Accdet]accdet_suspend ok\n"); */
#endif

	return 0;
}

static int accdet_resume(struct device *device)	/* wake up */
{
/* #ifdef ACCDET_PIN_SWAP */
/* pmic_pwrap_write(0x0400, 0x1000); */
/* accdet_FSA8049_enable(); */
/* #endif */

#ifdef ACCDET_MULTI_KEY_FEATURE
	/* ACCDET_DEBUG("[Accdet] in resume1: ACCDET_IRQ_STS = 0x%x\n", pmic_pwrap_read(ACCDET_IRQ_STS)); */
#else
#ifdef ACCDET_EINT

	if (1 == g_accdet_working_in_suspend && 0 == call_status) {

		/* enable_clock */
		accdet_enable_hal(pre_state_swctrl);
		/* clear g_accdet_working_in_suspend */
		g_accdet_working_in_suspend = 0;
		ACCDET_DEBUG("[Accdet]accdet_resume : recovery accdet register\n");

	}
#else
	if (call_status == 0)
		accdet_enable_hal(pre_state_swctrl);

#endif
	/* ACCDET_DEBUG("[Accdet]accdet_resume: ACCDET_CTRL=[0x%x], STATE_SWCTRL=[0x%x]\n",
	   pmic_pwrap_read(ACCDET_CTRL), pmic_pwrap_read(ACCDET_STATE_SWCTRL)); */
	pr_debug("[Accdet]accdet_resume: ACCDET_CTRL=[0x%x], STATE_SWCTRL=[0x%x]\n",
		 accdet_get_ctrl(), accdet_get_swctrl());
/*
    ACCDET_DEBUG("[Accdet]resum:sample_in:%x, curr_in:%x, mem_in:%x, FSM:%x\n"
		,INREG32(ACCDET_SAMPLE_IN)
		,INREG32(ACCDET_CURR_IN)
		,INREG32(ACCDET_MEMORIZED_IN)
		,INREG32(ACCDET_BASE + 0x0050));
*/
	/* ACCDET_DEBUG("[Accdet]accdet_resume ok\n"); */
#endif

	return 0;
}

/**********************************************************************
//add for IPO-H need update headset state when resume

***********************************************************************/
#ifdef CONFIG_PM

#ifdef ACCDET_PIN_RECOGNIZATION
struct timer_list accdet_disable_ipoh_timer;
static void accdet_pm_disable(unsigned long a)
{
	if (cable_type == NO_DEVICE && eint_accdet_sync_flag == 0) {
		/* disable accdet */
		pre_state_swctrl = accdet_get_swctrl();
		accdet_disable_hal();
		/* disable clock */
		accdet_disable_clk();
		pr_info("[Accdet]daccdet_pm_disable: disable!\n");
	} else {
		pr_info("[Accdet]daccdet_pm_disable: enable!\n");
	}
}
#endif
static int accdet_pm_restore_noirq(struct device *device)
{
	int current_status_restore = 0;
	pr_info("[Accdet]accdet_pm_restore_noirq start!\n");
	/* enable accdet */
	accdet_set_pwm_idle_on();
	accdet_enable_hal(ACCDET_SWCTRL_EN);
	eint_accdet_sync_flag = 1;
	current_status_restore = accdet_get_AB();	/* AB */

	switch (current_status_restore) {
	case 0:		/* AB=0 */
		cable_type = HEADSET_NO_MIC;
		accdet_status = HOOK_SWITCH;
		break;
	case 1:		/* AB=1 */
		cable_type = HEADSET_MIC;
		accdet_status = MIC_BIAS;
		break;
	case 3:		/* AB=3 */
		cable_type = NO_DEVICE;
		accdet_status = PLUG_OUT;
		break;
	default:
		pr_info("[Accdet]accdet_pm_restore_noirq: accdet current status error!\n");
		break;
	}
	switch_set_state((struct switch_dev *)&accdet_data, cable_type);
	if (cable_type == NO_DEVICE) {
#ifdef ACCDET_PIN_RECOGNIZATION
		init_timer(&accdet_disable_ipoh_timer);
		accdet_disable_ipoh_timer.expires = jiffies + 3 * HZ;
		accdet_disable_ipoh_timer.function = &accdet_pm_disable;
		accdet_disable_ipoh_timer.data = ((unsigned long)0);
		add_timer(&accdet_disable_ipoh_timer);
		pr_info("[Accdet]enable! pm timer\n");

#else
		/* eint_accdet_sync_flag = 0; */
		/* disable accdet */
		pre_state_swctrl = accdet_get_swctrl();
		accdet_disable_hal();
		/* disable clock */
		accdet_disable_clk();
#endif
	}
	return 0;
}

static const struct dev_pm_ops accdet_pm_ops = {
	.suspend = accdet_suspend,
	.resume = accdet_resume,
	.restore_noirq = accdet_pm_restore_noirq,
};
#endif

static struct platform_driver accdet_driver = {
	.probe = accdet_probe,
	/* .suspend      = accdet_suspend, */
	/* .resume               = accdet_resume, */
	.remove = accdet_remove,
	.driver = {
		   .name = "Accdet_Driver",
#ifdef CONFIG_PM
		   .pm = &accdet_pm_ops,
#endif
		   },
};

static int accdet_mod_init(void)
{
	int ret = 0;

	/* ACCDET_DEBUG("[Accdet]accdet_mod_init begin!\n"); */

	/* ------------------------------------------------------------------ */
	/* Accdet PM */
	/* ------------------------------------------------------------------ */

	ret = platform_driver_register(&accdet_driver);
	if (ret) {
		ACCDET_DEBUG("[Accdet]platform_driver_register error:(%d)\n", ret);
		return ret;
	}

	/* ACCDET_DEBUG("[Accdet]accdet_mod_init done!\n"); */
	return 0;

}

static void accdet_mod_exit(void)
{
	ACCDET_DEBUG("[Accdet]accdet_mod_exit\n");
	platform_driver_unregister(&accdet_driver);

	ACCDET_DEBUG("[Accdet]accdet_mod_exit Done!\n");
}

/*Patch for CR ALPS00804150 & ALPS00804802 PMIC temp not correct issue*/
int accdet_cable_type_state(void)
{
	ACCDET_DEBUG("[ACCDET] accdet_cable_type_state=%d\n", cable_type);
	return cable_type;
}
EXPORT_SYMBOL(accdet_cable_type_state);
/*Patch for CR ALPS00804150 & ALPS00804802 PMIC temp not correct issue*/


module_init(accdet_mod_init);
module_exit(accdet_mod_exit);


module_param(debug_enable_drv, int, 0644);

MODULE_DESCRIPTION("MTK MT6588 ACCDET driver");
MODULE_AUTHOR("Anny <Anny.Hu@mediatek.com>");
MODULE_LICENSE("GPL");

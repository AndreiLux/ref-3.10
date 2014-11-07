/*
 * ST-Ericsson M7450 HPM device driver. HSIC power management.
 *
 * Copyright (C) ST-Ericsson AB 2012
 * Authors: Dmitry Tarnyagin <dmitry.tarnyagin@stericsson.com>
 * License terms: GNU General Public License (GPL) version 2
 */

/*--------------------------------------------------
* #define DEBUG_INFO
* #define PM_TIME
* #define PM_STATE
*--------------------------------------------------*/

#include <linux/resource.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/regulator/consumer.h>

#include <linux/usbdevice_fs.h>
#include <linux/usb.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/timer.h>

#include <linux/of_device.h>
#include <linux/platform_data/ste_m7450_plat.h>
#include <linux/module.h>
#include <linux/pm_wakeup.h>

#include <linux/platform_data/gpio-odin.h>
#include <linux/wakelock.h>


#define STE_USB_PID_IPC 0x230f
#define m7450_fmt "m7450: "
#define m7450_err(...) pr_err(m7450_fmt __VA_ARGS__)

#if defined(DEBUG)
#define m7450_dbg(...) pr_debug(m7450_fmt __VA_ARGS__)
#elif defined(DEBUG_INFO)
#define m7450_dbg(...) pr_info(m7450_fmt __VA_ARGS__)
#else
#define m7450_dbg(...) do {} while (false)
#endif

#if defined(DEBUG)
#define m7450_spam(...) pr_debug(m7450_fmt __VA_ARGS__)
#elif defined(DEBUG_INFO)
#define m7450_spam(...) pr_info(m7450_fmt __VA_ARGS__)
#else
#define m7450_spam(...) do {} while (false)
#endif

/* Change ISR logic to remove wait_queue because interrupt is working well.
 * But, this ISR is depend on GIC in SMS, if it's unstable again, we have to
 * go back to wait_queue.
 */
#define		USE_DEFAULT_ISR_INSTEAD_OF_WAITQUEUE

MODULE_AUTHOR("Dmitry Tarnyagin <dmitry.tarnyagin@stericsson.com>");
MODULE_DESCRIPTION("M7450 HPM device driver");
MODULE_LICENSE("GPL");

struct class *modem;
struct device *m7450_sysfs;

struct m7450_wq_gpio_irq {
	struct work_struct irq_work;
	struct platform_device *pdev;
};

static struct m7450_wq_gpio_irq m7450_wq_irq;

static int t_wakeup_tmo = 1000;
module_param(t_wakeup_tmo, int, S_IRUGO);
MODULE_PARM_DESC(t_wakeup_tmo, "Wakeup timeout, ms.");

static int t_l2_disc = 5;
module_param(t_l2_disc, int, S_IRUGO);
MODULE_PARM_DESC(t_l2_disc, "Time interval between AWR low "
		"and USB disconnect, ms.");

static int t_l0_l2 = 20;
module_param(t_l0_l2, int, S_IRUGO);
MODULE_PARM_DESC(t_l0_l2, "Minimum amount of time host should stay "
		"in L2 before switching to L3, ms.");

static int t_l3_cwr = 50;
module_param(t_l3_cwr, int, S_IRUGO);
MODULE_PARM_DESC(t_l3_cwr, "Guard interval after switching to L3 "
		"when wakeup is not allowed, ms.");

static int t_arr_l2 = 15;
module_param(t_arr_l2, int, S_IRUGO);
MODULE_PARM_DESC(t_arr_l2, "Guard interval after ARR low "
		"and before USB suspend, ms.");

/* 10 og lesser are known BAD valued. 30 seems to be stable. */
static int t_gpio = 30;
module_param(t_gpio, int, S_IRUGO);
MODULE_PARM_DESC(t_gpio, "Minimal width of any pulse on a GPIO line, ms.");

/* In order to prevent suspend when modem wants to interact with AP,
    HPM should get a wake_lock until interaction is done. */
#define		PREVENT_SUSPEND_DURING_RESUME

#if defined(PREVENT_SUSPEND_DURING_RESUME)
static int m7450_wake_lock_counter = 0; /* wake_lock counter */
struct wake_lock m7450_wl; /* wake_lock to prevent suspend while resuming */
#endif

enum m7450_bits {
	TERM,
	CWR_RISE,
	CWR_FALL,
	USB_ATTACH,
	USB_DETACH,
	USB_PORT_SUSPEND,
	USB_PORT_RESUME,
	USB_BUS_SUSPEND,
	USB_BUS_RESUME,
	FAST_DETACH,
};

enum m7450_pm_state {
	STATE_DETACHED,
	STATE_L0,
	STATE_L0_AWAKE,
	STATE_L2,
	STATE_L3,

	STATE_ATTACH,

	STATE_L2_WAKEDOWN,
	STATE_L2_AC_WAKEUP,
	STATE_L2_CA_WAKEUP,

	STATE_L3_WAKEDOWN,
	STATE_L3_AC_WAKEUP,
	STATE_L3_CA_WAKEUP,
	STATE_L3_POST_WAKEUP,
	STATE_MAX,
};

struct m7450_states {
	const char *descr;
	unsigned long bits;
};

struct m7450_states states[STATE_MAX] = {
	[STATE_L0] = {
		.descr = "L0",
		.bits = BIT(TERM) |		/* -> term		*/
			BIT(CWR_RISE) |		/* -> DETACHED		*/
			BIT(USB_DETACH) |	/* -> DETACHED		*/
			BIT(FAST_DETACH) |	/* -> DETACHED		*/
			BIT(USB_PORT_SUSPEND),	/* -> L2		*/
	},
	[STATE_L0_AWAKE] = {
		.descr = "L0 wake-locked",
		.bits = BIT(TERM) |		/* -> term		*/
			BIT(CWR_FALL) |		/* -> L0		*/
			BIT(USB_DETACH) |	/* -> DETACHED		*/
			BIT(USB_PORT_SUSPEND),	/* -> L2		*/
	},
	[STATE_L2] = {
		.descr = "L2",
		.bits = BIT(TERM) |		/* -> term		*/
			BIT(CWR_RISE) |		/* -> L2_CA_WAKEUP	*/
			BIT(USB_PORT_RESUME) |	/* -> L2_AC_WAKEUP	*/
			BIT(USB_BUS_SUSPEND) |	/* -> L3_WAKEDOWN	*/
			BIT(FAST_DETACH) |	/* -> DETACHED		*/
			BIT(USB_DETACH),	/* -> DETACHED		*/
	},
	[STATE_L3] = {
		.descr = "L3",
		.bits = BIT(TERM) |		/* -> term		*/
			BIT(CWR_RISE) |		/* -> L3_CA_WAKEUP	*/
			BIT(FAST_DETACH) |	/* -> DETACHED		*/
			BIT(USB_BUS_RESUME),	/* -> L3_POST_WAKEUP	*/
	},
	[STATE_L3_POST_WAKEUP] = {
		.descr = "L3 post-wake",
		.bits = BIT(TERM) |		/* -> term		*/
			BIT(USB_DETACH) |	/* -> DETACHED		*/
			BIT(USB_BUS_SUSPEND) |	/* -> DETACHED		*/
			BIT(USB_PORT_RESUME),	/* -> L0_AWAKE		*/
	},
	[STATE_L2_WAKEDOWN] = {
		.descr = "L2 wakedown",
		.bits = BIT(TERM) |		/* -> term		*/
			BIT(USB_PORT_SUSPEND),	/* -> L0		*/
	},
	[STATE_L2_AC_WAKEUP] = {
		.descr = "L2 AC wakeup",
		.bits = BIT(TERM) |		/* -> term		*/
			BIT(CWR_RISE) |		/* -> L0_AWAKE		*/
			BIT(USB_PORT_RESUME) |	/* -> DETACHED		*/
			BIT(FAST_DETACH) |	/* -> DETACHED		*/
			BIT(USB_BUS_SUSPEND),	/* -> L3_AC_WAKEUP	*/
	},
	[STATE_L2_CA_WAKEUP] = {
		.descr = "L2 CA wakeup",
		.bits = BIT(TERM) |		/* -> term		*/
			BIT(CWR_FALL) |		/* -> DETACHED		*/
			BIT(USB_PORT_RESUME) |	/* -> L0_AWAKE		*/
			BIT(USB_BUS_SUSPEND),	/* -> L3_CA_WAKEUP	*/
	},
	[STATE_L3_WAKEDOWN] = {
		.descr = "L3 wakedown",
		.bits = BIT(TERM) |		/* -> term		*/
			BIT(USB_BUS_SUSPEND) |	/* -> L2		*/
			BIT(USB_DETACH),	/* -> DETACHED		*/
	},
	[STATE_L3_AC_WAKEUP] = {
		.descr = "L3 AC wakeup",
		.bits = BIT(TERM) |		/* -> term		*/
			BIT(FAST_DETACH) |	/* -> DETACHED		*/
			BIT(CWR_RISE),		/* -> L3_CA_WAKEUP	*/
	},
	[STATE_L3_CA_WAKEUP] = {
		.descr = "L3 CA wakeup",
		.bits = BIT(TERM) |		/* -> term		*/
			BIT(CWR_FALL) |		/* -> DETACHED		*/
			BIT(USB_DETACH) |	/* -> DETACHED		*/
			BIT(USB_BUS_RESUME),	/* -> L3_POST_WAKEUP	*/
	},
	[STATE_DETACHED] = {
		.descr = "Device detached",
		.bits = BIT(TERM) |		/* -> term		*/
			BIT(CWR_RISE),		/* -> ATTACH		*/
	},
	[STATE_ATTACH] = {
		.descr = "Device attach",
		.bits = BIT(TERM) |		/* -> term		*/
			BIT(CWR_RISE) |		/* -> DETACHED		*/
			BIT(FAST_DETACH) |	/* -> DETACHED		*/
			BIT(USB_ATTACH),	/* -> L0_AWAKE		*/
	},
};

struct m7450_hsic {
	struct list_head link;
	struct platform_device *pdev;

	unsigned long bits;
	enum m7450_pm_state pm_state;
	int cwr_state;
	unsigned long arr_timestamp;

	struct usb_device *usb_dev;
	struct notifier_block usb_nfb;
	struct workqueue_struct *workqueue;
	struct work_struct work;
	wait_queue_head_t wait_evt;
	wait_queue_head_t wait_state;
	spinlock_t lock;
};

static spinlock_t m7450_hsic_devices_lock;
struct list_head m7450_hsic_devices;
static int (*usb_suspend) (struct usb_device *udev, pm_message_t message);
static int (*usb_resume) (struct usb_device *udev, pm_message_t message);

struct m7450_gpio_descriptor {
	bool output;
	bool export;
	bool wake_capable;
};

#define M7450_DECLARE_GPIO(id, n) \
	[id] = {			\
		.flags	= IORESOURCE_IO,\
		.name	= n,		\
	},
static struct resource m7450_resources[M7450_RESOURCE_COUNT] = {
	M7450_DECLARE_GPIO(M7450_RESOURCE_ONSW,	"onsw_gpio")
		M7450_DECLARE_GPIO(M7450_RESOURCE_PST, "pwrrst_gpio")
		M7450_DECLARE_GPIO(M7450_RESOURCE_RO2, "resout2_gpio")
		M7450_DECLARE_GPIO(M7450_RESOURCE_CWR,	"cwr_gpio")
		M7450_DECLARE_GPIO(M7450_RESOURCE_AWR,	"awr_gpio")
		M7450_DECLARE_GPIO(M7450_RESOURCE_ARR,	"arr_gpio")
};

#if defined(CONFIG_ODIN_DWC_USB_HOST)
extern void odin_dwc_host_set_idle(int release);
#else
#define odin_dwc_host_set_idle(...) do {} while (false)
#endif

#if defined(PM_TIME)
static void m7450_pm_show_time(ktime_t starttime, char *info)
{
	ktime_t calltime;
	u64 usecs64;
	int usecs;

	calltime = ktime_get();
	usecs64 = ktime_to_ns(ktime_sub(calltime, starttime));
	do_div(usecs64, NSEC_PER_USEC);
	usecs = usecs64;
	if (usecs == 0)
		usecs = 1;
	m7450_dbg("PM: %s complete after %ld.%03ld msecs\n",
		info,
		usecs / USEC_PER_MSEC, usecs % USEC_PER_MSEC);
}
#else
#define m7450_pm_show_time(...) do {} while (false)
#endif

static int m7450_dev_power(struct platform_device *pdev,
		enum m7450_hsic_power_mode mode)
{
	int ret = 0;
	struct m7450_platform_data *pdata = pdev->dev.platform_data;

	m7450_dbg("dev power \n");
	if (pdata->power_mode != M7450_HSIC_POWER_ON &&
			mode == M7450_HSIC_POWER_ON &&
			pdata->usb_dev)
		pm_runtime_get_sync(&pdata->usb_dev->dev);

	switch (mode) {
	case M7450_HSIC_POWER_OFF:
		if (pdata->power_mode > M7450_HSIC_POWER_OFF) {
			odin_dwc_host_set_idle(0);
		}
		break;
	case M7450_HSIC_POWER_L3:
		break;
	case M7450_HSIC_POWER_AUTO:
	case M7450_HSIC_POWER_ON:
		if (pdata->power_mode == M7450_HSIC_POWER_OFF) {
			odin_dwc_host_set_idle(1);
		}
		break;
	}

	if (pdata->power_mode == M7450_HSIC_POWER_ON &&
			mode != M7450_HSIC_POWER_ON &&
			pdata->usb_dev)
		pm_runtime_put_sync(&pdata->usb_dev->dev);

	pdata->power_mode = mode;

	return ret;

}

static const inline char *m7450_state_name(enum m7450_pm_state state)
{
	return states[state].descr;
}

static void m7450_state_change(struct m7450_hsic *priv, enum m7450_pm_state state)
{
	enum m7450_pm_state old_state;
	old_state = priv->pm_state;
	priv->pm_state = state;
#if defined(PM_STATE)
	m7450_dbg("State changed: %s \t-> %s \n", states[old_state].descr, states[priv->pm_state].descr);
#endif
}

#if defined(PREVENT_SUSPEND_DURING_RESUME)
static int m7450_wake_lock(void) {
	if (!m7450_wake_lock_counter) {
		wake_lock(&m7450_wl);
		m7450_wake_lock_counter++;
		m7450_dbg("%s, M7450_WAKE_LOCK. counter = %d\n",
					__func__, m7450_wake_lock_counter);
	} else {
		m7450_dbg("%s, don't need wake_lock. counter = %d\n",
					__func__, m7450_wake_lock_counter);
	}
	return m7450_wake_lock_counter;
}

static int m7450_wake_unlock(void) {
	if (m7450_wake_lock_counter) {
		wake_unlock(&m7450_wl);
		m7450_wake_lock_counter--;
		m7450_dbg("%s, M7450_WAKE_UNLOCK. counter = %d\n",
					__func__, m7450_wake_lock_counter);
	} else {
		m7450_dbg("%s, don't need wake_unlock. counter = %d\n",
					__func__, m7450_wake_lock_counter);
	}
	return m7450_wake_lock_counter;
}
#endif

static inline void __m7450_notify_evt(struct m7450_hsic *priv,
		unsigned long bits)
{
	m7450_dbg("Event NOTIFY:Modem states %s bits %ld, evt %ld : Match %ld > 0\n",
			states[priv->pm_state].descr,
			priv->bits, bits, states[priv->pm_state].bits & bits );

	priv->bits |= bits;

	if (states[priv->pm_state].bits & bits) {
		wake_up_all(&priv->wait_evt);
		queue_work(priv->workqueue, &priv->work);
	}
}

static inline void m7450_notify_evt(struct m7450_hsic *priv,
		unsigned long bits)
{
	unsigned long flags;
	spin_lock_irqsave(&priv->lock, flags);
	__m7450_notify_evt(priv, bits);
	spin_unlock_irqrestore(&priv->lock, flags);
}

static inline bool __m7450_has_evt(struct m7450_hsic *priv, unsigned long bits)
{
	return !!(priv->bits & bits);
}

static inline bool m7450_has_evt(struct m7450_hsic *priv, unsigned long bits)
{
	bool ret;
	unsigned long flags;
	spin_lock_irqsave(&priv->lock, flags);
	ret = __m7450_has_evt(priv, bits);
	spin_unlock_irqrestore(&priv->lock, flags);
	return ret;
}

static inline void __m7450_clear_evt(struct m7450_hsic *priv,
		unsigned long bits)
{
	priv->bits &= ~bits;
}

static inline void m7450_clear_evt(struct m7450_hsic *priv, unsigned long bits)
{
	unsigned long flags;
	spin_lock_irqsave(&priv->lock, flags);
	__m7450_clear_evt(priv, bits);
	spin_unlock_irqrestore(&priv->lock, flags);
}

/* Caller should hold the lock */
static void m7450_detach_fallback(struct m7450_hsic *priv)
{
	struct m7450_platform_data *pdata = priv->pdev->dev.platform_data;

	m7450_dbg("Detach fallback in %s state, hsic state %d\n",
			m7450_state_name(priv->pm_state), pdata->power_mode);

#if defined(PREVENT_SUSPEND_DURING_RESUME)
	m7450_wake_unlock();
#endif

	m7450_state_change(priv, STATE_DETACHED);
	pdata->arr_set(priv->pdev, false);
	pdata->awr_set(priv->pdev, false);

	spin_unlock_irq(&priv->lock);
	pdata->power(priv->pdev, M7450_HSIC_POWER_OFF);
	spin_lock_irq(&priv->lock);
}

/* Caller should hold the lock */
static void m7450_disable_autosuspend(struct m7450_hsic *priv)
{
	WARN_ON(!priv->usb_dev);
	if (priv->usb_dev)
		pm_runtime_get(&priv->usb_dev->dev);
}

/* Caller should hold the lock */
static void m7450_enable_autosuspend(struct m7450_hsic *priv)
{
	WARN_ON(!priv->usb_dev);
	if (priv->usb_dev) {
		pm_runtime_mark_last_busy(&priv->usb_dev->dev);
		pm_runtime_put_autosuspend(&priv->usb_dev->dev);
	}
}

/* Caller should hold the lock */
static inline bool m7450_do_state(struct m7450_hsic *priv, bool nowakeup)
{
	struct m7450_platform_data *pdata = priv->pdev->dev.platform_data;

	m7450_dbg("Start Do state : %s\n",
			m7450_state_name(priv->pm_state));

	switch (priv->pm_state) {
		case STATE_DETACHED:
			__m7450_clear_evt(priv,
					BIT(CWR_FALL) |
					BIT(USB_ATTACH) | BIT(USB_DETACH) |
					BIT(FAST_DETACH) |
					BIT(USB_BUS_SUSPEND) |
					BIT(USB_BUS_RESUME) |
					BIT(USB_PORT_SUSPEND) |
					BIT(USB_PORT_RESUME));

			/* DETACHED -> ATTACH */
			if (__m7450_has_evt(priv, BIT(CWR_RISE))) {
				__m7450_clear_evt(priv, BIT(CWR_RISE));
				m7450_state_change(priv, STATE_ATTACH);
				m7450_dbg("Modem states: DETACHED -> ATTACH awr, arr set \n");

				pdata->awr_set(priv->pdev, true);
				pdata->arr_set(priv->pdev, true);

				spin_unlock_irq(&priv->lock);
				pdata->power(priv->pdev, M7450_HSIC_POWER_ON);
				spin_lock_irq(&priv->lock);
				return true;
			}
			break;

		case STATE_ATTACH:
			if (__m7450_has_evt(priv, BIT(USB_DETACH))) {
				/* ATTACH -> DETACH */
				__m7450_clear_evt(priv, BIT(USB_ATTACH) |
						BIT(USB_DETACH));
				m7450_detach_fallback(priv);
				return true;
			} else if (__m7450_has_evt(priv, BIT(FAST_DETACH))) {
				/* ATTACH -> DETACH */
				__m7450_clear_evt(priv, BIT(FAST_DETACH));
				m7450_detach_fallback(priv);
				return true;
			} else if (__m7450_has_evt(priv, BIT(CWR_RISE))) {
				/* ATTACH -> DETACH */
				__m7450_clear_evt(priv, BIT(CWR_FALL));
				m7450_detach_fallback(priv);
				return true;
			} else if (__m7450_has_evt(priv, BIT(USB_ATTACH))) {
				/* ATTACH -> L0_AWAKE */
				__m7450_clear_evt(priv, BIT(USB_ATTACH));
				m7450_state_change(priv, STATE_L0_AWAKE);
				if (likely(priv->usb_dev)) {
					m7450_disable_autosuspend(priv);
					usb_enable_autosuspend(priv->usb_dev);
				}
				pdata->power(priv->pdev,
						M7450_HSIC_POWER_AUTO);
				return true;
			}
			break;

		case STATE_L3:
			if (__m7450_has_evt(priv, BIT(FAST_DETACH))) {
				/* ATTACH -> DETACH */
				__m7450_clear_evt(priv, BIT(FAST_DETACH));
				m7450_detach_fallback(priv);
				return true;
			} else if (!nowakeup && __m7450_has_evt(priv,
						BIT(CWR_RISE))) {
				/* L3 -> L3_CA_WAKEUP */
				__m7450_clear_evt(priv, BIT(CWR_RISE));
				m7450_state_change(priv, STATE_L3_CA_WAKEUP);
				m7450_disable_autosuspend(priv);
				return true;
			} else if (__m7450_has_evt(priv, BIT(USB_BUS_RESUME))) {
				/* L3 -> L3_AC_WAKEUP -> L0_AWAKE */
				__m7450_clear_evt(priv, BIT(USB_BUS_RESUME));
				m7450_state_change(priv, STATE_L3_POST_WAKEUP);
				return true;
			}
			break;

		case STATE_L0_AWAKE:
			if (__m7450_has_evt(priv, BIT(USB_DETACH))) {
				/* L0_AWAKE -> L3 */
				__m7450_clear_evt(priv, BIT(USB_DETACH));
				m7450_detach_fallback(priv);
				return true;
			} else if (__m7450_has_evt(priv, BIT(CWR_FALL))) {
				/* L0_AWAKE -> L0 */
				__m7450_clear_evt(priv, BIT(CWR_FALL));
				m7450_state_change(priv, STATE_L0);
				m7450_enable_autosuspend(priv);
#if defined(PREVENT_SUSPEND_DURING_RESUME)
				m7450_wake_unlock();
#endif
				return true;
			}
			break;

		case STATE_L0:
			__m7450_clear_evt(priv, BIT(CWR_FALL));
			if (__m7450_has_evt(priv, BIT(FAST_DETACH))) {
				/* ATTACH -> DETACH */
				__m7450_clear_evt(priv, BIT(FAST_DETACH));
				m7450_detach_fallback(priv);
				return true;
			} else if (__m7450_has_evt(priv, BIT(USB_DETACH))) {
				/* L0 -> L3 */
				__m7450_clear_evt(priv, BIT(USB_DETACH));
				m7450_detach_fallback(priv);
				return true;
			} else if (__m7450_has_evt(priv, BIT(USB_PORT_SUSPEND))) {
				/* L0 -> L2 */
				__m7450_clear_evt(priv, BIT(USB_PORT_SUSPEND));
				m7450_state_change(priv, STATE_L2);
				return true;
			} else if (__m7450_has_evt(priv, BIT(CWR_RISE))) {
				/* L0 -> DETACH */
				/* Do not reset CWB bit: reenumeration */
				m7450_dbg("Modem states: L0 -> DETACH \n");
				m7450_detach_fallback(priv);
				return true;
			}
			break;

		case STATE_L2:
			if (__m7450_has_evt(priv, BIT(FAST_DETACH))) {
				/* ATTACH -> DETACH */
				__m7450_clear_evt(priv, BIT(FAST_DETACH));
				m7450_detach_fallback(priv);
				return true;
			} else if (__m7450_has_evt(priv, BIT(USB_DETACH))) {
				/* L2 -> L3 */
				__m7450_clear_evt(priv, BIT(USB_DETACH));
				m7450_detach_fallback(priv);
				return true;
			} else if (__m7450_has_evt(priv, BIT(USB_BUS_SUSPEND))) {
				/* L2 -> L3 */
				__m7450_clear_evt(priv, BIT(USB_BUS_SUSPEND));
				m7450_state_change(priv, STATE_L3);
				return true;
			} else if (!nowakeup && __m7450_has_evt(priv,
						BIT(CWR_RISE))) {
				/* L2 -> L2_CA_WAKEUP */
				__m7450_clear_evt(priv, BIT(CWR_RISE));
				m7450_state_change(priv, STATE_L2_CA_WAKEUP);
				m7450_disable_autosuspend(priv);
				return true;
			} else if (__m7450_has_evt(priv, BIT(USB_PORT_RESUME))) {
				/* L2 -> L2_AC_WAKEUP -> L0_AWAKE */
				/* Handled in m7450_hsic_usb_resume() */
				WARN_ON(!__m7450_has_evt(priv, BIT(CWR_RISE)));
				__m7450_clear_evt(priv, BIT(CWR_RISE));
				__m7450_clear_evt(priv, BIT(USB_PORT_RESUME));
				m7450_state_change(priv, STATE_L0_AWAKE);
				m7450_disable_autosuspend(priv);
				return true;
			}
			break;

		case STATE_L2_WAKEDOWN:
			if (__m7450_has_evt(priv, BIT(USB_PORT_SUSPEND))) {
				/* L2_WAKEDOWN -> L0 */
				__m7450_clear_evt(priv, BIT(USB_PORT_SUSPEND));
				m7450_state_change(priv, STATE_L0);
#if defined(PREVENT_SUSPEND_DURING_RESUME)
				m7450_wake_unlock();
#endif
				return true;
			}
			break;

		case STATE_L3_WAKEDOWN:
			if (__m7450_has_evt(priv, BIT(USB_BUS_SUSPEND))) {
				/* L3_WAKEDOWN -> L2 */
				__m7450_clear_evt(priv, BIT(USB_BUS_SUSPEND));
				m7450_state_change(priv, STATE_L2);
				return true;
			}
			break;

		case STATE_L2_CA_WAKEUP:
			if (__m7450_has_evt(priv, BIT(USB_PORT_RESUME))) {
				/* L2_CA_WAKEUP -> L0_AWAKE */
				__m7450_clear_evt(priv, BIT(USB_PORT_RESUME));
				m7450_state_change(priv, STATE_L0_AWAKE);
				return true;
			} else if (__m7450_has_evt(priv, BIT(USB_BUS_SUSPEND))) {
				/* L2_CA_WAKEUP -> L3_CA_WAKEUP */
				__m7450_clear_evt(priv, BIT(USB_BUS_SUSPEND));
				m7450_state_change(priv, STATE_L3_CA_WAKEUP);
				return true;
			} else if (__m7450_has_evt(priv, BIT(CWR_FALL))) {
				/* L2_CA_WAKEUP -> L3 */
				__m7450_clear_evt(priv, BIT(CWR_FALL));
				m7450_detach_fallback(priv);
				return true;
			}
			break;

		case STATE_L3_CA_WAKEUP:
			if (__m7450_has_evt(priv, BIT(USB_BUS_RESUME))) {
				/* L2_CA_WAKEUP -> L0_AWAKE */
				__m7450_clear_evt(priv, BIT(USB_BUS_RESUME));
				m7450_state_change(priv, STATE_L3_POST_WAKEUP);
				return true;
			} else if (__m7450_has_evt(priv, BIT(CWR_FALL))) {
				/* L2_CA_WAKEUP -> L3 */
				__m7450_clear_evt(priv, BIT(CWR_FALL));
				m7450_detach_fallback(priv);
				return true;
			}
			break;

		case STATE_L2_AC_WAKEUP:
			if (__m7450_has_evt(priv, BIT(FAST_DETACH))) {
				/* L2_AC_WAKEUP -> DETACH */
				__m7450_clear_evt(priv, BIT(FAST_DETACH));
				m7450_detach_fallback(priv);
				return true;
			} else if (__m7450_has_evt(priv, BIT(USB_PORT_RESUME))) {
				/* L2_AC_WAKEUP -> L3 */
				__m7450_clear_evt(priv, BIT(USB_PORT_RESUME));
				m7450_detach_fallback(priv);
				return true;
			} else if (__m7450_has_evt(priv, BIT(USB_BUS_SUSPEND))) {
				/* L2_AC_WAKEUP -> L3_AC_WAKEUP */
				__m7450_clear_evt(priv, BIT(USB_BUS_SUSPEND));
				m7450_state_change(priv, STATE_L3_AC_WAKEUP);
				return true;
			}
			break;

		case STATE_L3_AC_WAKEUP:
			if (__m7450_has_evt(priv, BIT(FAST_DETACH))) {
				/* L3_AC_WAKEUP -> DETACH */
				__m7450_clear_evt(priv, BIT(FAST_DETACH));
				m7450_detach_fallback(priv);
				return true;
			} else if (__m7450_has_evt(priv, BIT(USB_BUS_RESUME))) {
				/* Lx_AC_WAKEUP -> DETACH */
				__m7450_clear_evt(priv, BIT(USB_BUS_RESUME));
				m7450_detach_fallback(priv);
				return true;
			}
			break;

		case STATE_L3_POST_WAKEUP:
			if (__m7450_has_evt(priv, BIT(USB_DETACH))) {
				/* L3_POST_WAKEUP -> DETACHED */
				__m7450_clear_evt(priv, BIT(USB_DETACH));
				__m7450_notify_evt(priv, BIT(CWR_RISE));
				m7450_detach_fallback(priv);
				return true;
			} else if (__m7450_has_evt(priv, BIT(USB_PORT_RESUME))) {
				/* L3_POST_WAKEUP -> L0_AWAKE */
				__m7450_clear_evt(priv, BIT(USB_PORT_RESUME));
				m7450_state_change(priv, STATE_L0_AWAKE);
				return true;
			} else if (__m7450_has_evt(priv, BIT(USB_BUS_SUSPEND))) {
				/* L3_POST_WAKEUP -> DETACHED */
				__m7450_clear_evt(priv, BIT(USB_BUS_SUSPEND));
				m7450_detach_fallback(priv);
				return true;
			}
			break;

		default:
			break;
	}

	return false;
}

/* Caller should hold the lock */
static bool m7450_run_sm(struct m7450_hsic *priv, bool nowakeup)
{
	bool ret = false;

	m7450_dbg("Enter SM: %s\n",
			m7450_state_name(priv->pm_state));

	while (!__m7450_has_evt(priv, BIT(TERM))) {
		if (!m7450_do_state(priv, nowakeup)) {
			ret = true;
			break;
		}
	}

	if (nowakeup && __m7450_has_evt(priv, BIT(CWR_RISE)))
		queue_work(priv->workqueue, &priv->work);

	m7450_dbg("Exit SM: %s\n",
			m7450_state_name(priv->pm_state));
	return ret;
}

static void m7450_hsic_work(struct work_struct *work)
{
	struct m7450_hsic *priv =
		container_of(work, struct m7450_hsic, work);

	spin_lock_irq(&priv->lock);
	m7450_run_sm(priv, false);
	spin_unlock_irq(&priv->lock);
}

static bool m7450_hsic_known_device(struct m7450_hsic *priv,
		const struct usb_device_descriptor *desc)
{
	struct m7450_platform_data *pdata = priv->pdev->dev.platform_data;
	int i;

	for (i = 0; i < pdata->num_ids; ++i) {
		if (pdata->ids[i].vid == desc->idVendor &&
				pdata->ids[i].pid == desc->idProduct)
			return true;
	}
	return false;
}

struct m7450_hsic *m7450_from_usb_device(struct usb_device *udev)
{
	struct m7450_hsic *priv = NULL;
	spin_lock_bh(&m7450_hsic_devices_lock);
	list_for_each_entry(priv, &m7450_hsic_devices, link) {
		if (priv->usb_dev == udev ||
				(priv->usb_dev && priv->usb_dev->parent == udev)) {
			spin_unlock_bh(&m7450_hsic_devices_lock);
			return priv;
		}
	}
	spin_unlock_bh(&m7450_hsic_devices_lock);
	return NULL;
}

static int m7450_hsic_usb_suspend(struct usb_device *udev,
		pm_message_t message)
{
	ktime_t starttime = ktime_get();
	ktime_t starttime_usb;
	struct m7450_hsic *priv = m7450_from_usb_device(udev);
	struct m7450_platform_data *pdata;
	int ret = 0;
	enum m7450_pm_state pm_state;
	enum m7450_bits evt;

	m7450_dbg("USB suspend , hsic_usb_suspend\n");

	if (WARN_ON(!priv))
		return -EINVAL;

	pdata = priv->pdev->dev.platform_data;
	evt = (priv->usb_dev == udev) ? USB_PORT_SUSPEND : USB_BUS_SUSPEND;

	spin_lock_irq(&priv->lock);
	m7450_run_sm(priv, true);
	pm_state = priv->pm_state;

	if (evt == USB_BUS_SUSPEND) {
		m7450_dbg("USB BUS suspend\n");

		m7450_state_change(priv, STATE_L3_WAKEDOWN);
		pdata->awr_set(priv->pdev, false);
		spin_unlock_irq(&priv->lock);
		msleep(t_l2_disc);
		pdata->power(priv->pdev, M7450_HSIC_POWER_L3);
		starttime_usb = ktime_get();
		ret = usb_suspend(udev, message);
		m7450_pm_show_time(starttime_usb, "usb1_bus_suspend");
		msleep(t_l3_cwr);
		spin_lock_irq(&priv->lock);
	} else {
		m7450_dbg("USB PORT suspend\n");

		m7450_state_change(priv, STATE_L2_WAKEDOWN);
		pdata->arr_set(priv->pdev, false);
		priv->arr_timestamp = jiffies;
		spin_unlock_irq(&priv->lock);
		msleep(t_arr_l2);
		starttime_usb = ktime_get();
		ret = usb_suspend(udev, message);
		m7450_pm_show_time(starttime_usb, "usb1_port_suspend");
		spin_lock_irq(&priv->lock);
	}

	if (!ret) {
		m7450_state_change(priv, pm_state);
		m7450_dbg("USB suspend success: fall back to %s\n",
				m7450_state_name(pm_state));
	}
	else {
		m7450_dbg("USB suspend fail: now modem state %s \n",
				m7450_state_name(priv->pm_state));
	}

	__m7450_notify_evt(priv, BIT(evt));
	m7450_run_sm(priv, true);
	spin_unlock_irq(&priv->lock);
	m7450_dbg("USB suspend done\n");

	m7450_pm_show_time(starttime, "usb_suspend");
	return ret;
}


static int m7450_hsic_usb_resume(struct usb_device *udev, pm_message_t message)
{
	ktime_t starttime = ktime_get();
	ktime_t starttime_usb;
	struct m7450_hsic *priv = m7450_from_usb_device(udev);
	struct m7450_platform_data *pdata;
	int ret = 0;
	int val = 0;

	enum m7450_pm_state pm_state;
	enum m7450_bits evt;
	unsigned long gpio_timestamp;

	m7450_dbg("USB resume, hsic_usb_resume\n");

	if (WARN_ON(!priv))
		return -EINVAL;

	pdata = priv->pdev->dev.platform_data;
	evt = (priv->usb_dev == udev) ? USB_PORT_RESUME : USB_BUS_RESUME;
	gpio_timestamp = priv->arr_timestamp + t_gpio * HZ / 1000 + 1;

	spin_lock_irq(&priv->lock);
	m7450_run_sm(priv, false);
	pm_state = priv->pm_state;
	if (evt == USB_PORT_RESUME) {
		m7450_state_change(priv, STATE_L2_AC_WAKEUP);
		m7450_dbg("USB PORT resume, L2_AC_WAKEUP\n");
	} else {
		m7450_state_change(priv, STATE_L3_AC_WAKEUP);
		m7450_dbg("USB BUS resume, L3_AC_WAKEUP\n");
	}
	spin_unlock_irq(&priv->lock);

	if (evt == USB_PORT_RESUME && time_before(jiffies, gpio_timestamp))
		wait_event_timeout(priv->wait_evt, false,
				gpio_timestamp - jiffies);

	switch (pm_state) {
		case STATE_L3_POST_WAKEUP:
			break;

		case STATE_L3:
			pdata->awr_set(priv->pdev, true);
		case STATE_L2:
			pdata->arr_set(priv->pdev, true);

			if (!wait_event_timeout(priv->wait_evt,
					m7450_has_evt(priv, BIT(TERM) | BIT(CWR_RISE)),
					t_wakeup_tmo * HZ / 1000))
			{
#if defined(USE_DEFAULT_ISR_INSTEAD_OF_WAITQUEUE)
				m7450_err("\nwakeup from %s: CWR negotiation "
							"failed, fall back to L3. CWR is %s.\n\n",
							m7450_state_name(pm_state),
							pdata->cwr_get(priv->pdev) ? "HIGH" : "LOW");
				m7450_notify_evt(priv, BIT(evt));
				return -EBUSY;
			}
#else
				val = pdata->cwr_get(priv->pdev);
				if(val) {
					m7450_dbg("No Interrupt, CWR GPIO value high\n");
					priv->cwr_state = val;
					m7450_notify_evt(priv, BIT(CWR_RISE));
				}
				else {
					m7450_err("wakeup from %s: CWR negotiation "
							"failed, fall back to L3\n",
							m7450_state_name(pm_state));

					m7450_notify_evt(priv, BIT(evt));
					return -EBUSY;
				}
			}
#endif
			m7450_dbg("State = %s : Interrupt is detected. %s.\n",
						m7450_state_name(pm_state),
						m7450_has_evt(priv, BIT(CWR_RISE)) ? "CRW_RISE" : "INVALID_CASE. ERROR !!!");

			if (m7450_has_evt(priv, BIT(TERM))) {
				pdata->awr_set(priv->pdev, false);
				pdata->arr_set(priv->pdev, false);
				return -EBUSY;
			}
			break;

		case STATE_L3_CA_WAKEUP:
			pdata->awr_set(priv->pdev, true);
		case STATE_L2_CA_WAKEUP:
			pdata->arr_set(priv->pdev, true);
			break;

		default:
			m7450_err("Invalid state (%s) on USB resume.\n",
					m7450_state_name(pm_state));
			m7450_notify_evt(priv, BIT(evt));
			return -EBUSY;
	}

	m7450_spam("USB1 pre-resume\n");
	if (evt == USB_BUS_RESUME)
		pdata->power(priv->pdev, M7450_HSIC_POWER_AUTO);

#if defined(CONFIG_ODIN_DWC_USB_HOST)
	/* Workaround to fix USB disconnect issue while port is resumed */
	if (evt == USB_PORT_RESUME)
		msleep(100);
#endif
	starttime_usb = ktime_get();
	ret = usb_resume(udev, message);
	m7450_pm_show_time(starttime_usb, "usb1_resume");
	m7450_spam("USB1 post-resume\n");

	if (ret) {
		m7450_err("Wakeup from %s: "
				"USB resume failed: %d, fall back to L3\n",
				m7450_state_name(pm_state),
				ret);
		m7450_notify_evt(priv, BIT(CWR_RISE));
		m7450_notify_evt(priv, BIT(evt));
		return ret;
	}

	spin_lock_irq(&priv->lock);

	m7450_state_change(priv, pm_state);
	m7450_dbg("USB resume success: fall back to %s\n",
				m7450_state_name(pm_state));

	__m7450_notify_evt(priv, BIT(evt));
	m7450_run_sm(priv, false);
	spin_unlock_irq(&priv->lock);

	m7450_pm_show_time(starttime, "usb_resume");
	return ret;
}

#if defined(CONFIG_ODIN_DWC_USB_HOST)
extern void generic_pm_hook(
	int (*suspend)(struct usb_device *udev, pm_message_t message),
	int (*resume)(struct usb_device *udev, pm_message_t message));
extern int generic_suspend_temp(struct usb_device *udev, pm_message_t msg);
extern int generic_resume_temp(struct usb_device *udev, pm_message_t msg);
#endif

static int m7450_hsic_usb_notify(struct notifier_block *nfb,
		unsigned long action, void *arg)
{
	struct m7450_hsic *priv =
		container_of(nfb, struct m7450_hsic, usb_nfb);
	struct usb_device *udev = arg;
	struct usb_device_driver *usb_driver =
		to_usb_device_driver(udev->dev.driver);
	const struct usb_device_descriptor *desc = &udev->descriptor;
	unsigned long flags;

	switch (action) {
		case USB_DEVICE_ADD:
			if (m7450_hsic_known_device(priv, desc) &&
					(desc->idProduct == STE_USB_PID_IPC)) {
				m7450_dbg("USB notify:0x%.4X:0x%.4X\n",
						desc->idVendor,desc->idProduct);

				spin_lock_irqsave(&priv->lock, flags);
				priv->usb_dev = udev;

#if defined(CONFIG_ODIN_DWC_USB_HOST)
				udev->force_set_port_susp = true;

				usb_suspend = generic_suspend_temp;
				usb_resume = generic_resume_temp;
				if (usb_driver->suspend && usb_driver->resume)
					generic_pm_hook(m7450_hsic_usb_suspend,
							m7450_hsic_usb_resume);
#else
				usb_suspend = usb_driver->suspend;
				usb_resume = usb_driver->resume;
				if (usb_driver->suspend)
					usb_driver->suspend = m7450_hsic_usb_suspend;
				if (usb_driver->resume)
					usb_driver->resume = m7450_hsic_usb_resume;
#endif
				spin_unlock_irqrestore(&priv->lock, flags);

				m7450_dbg("USB attach, bit %ld\n",BIT(USB_ATTACH));
				m7450_notify_evt(priv, BIT(USB_ATTACH));
			}
			break;
		case USB_DEVICE_REMOVE:
			if (m7450_hsic_known_device(priv, desc) &&
					(desc->idProduct == STE_USB_PID_IPC)) {
				m7450_dbg("USB notify: removed 0x%.4X:0x%.4X\n",
						desc->idVendor, desc->idProduct);

				spin_lock_irqsave(&priv->lock, flags);
#if defined(CONFIG_ODIN_DWC_USB_HOST)
				generic_pm_hook(NULL, NULL);
#else
				usb_driver->suspend = usb_suspend;
				usb_driver->resume = usb_resume;
#endif
				priv->usb_dev = NULL;
				spin_unlock_irqrestore(&priv->lock, flags);

				m7450_notify_evt(priv, BIT(USB_DETACH));
			}
			break;
	}

	return 0;
}

static int m7450_hsic_suspend(struct device *dev)
{
	struct m7450_hsic *priv = dev_get_drvdata(dev);
	struct m7450_platform_data *pdata = dev->platform_data;
	m7450_spam("System suspend, hsic_suspend\n");
	if (pdata->suspend && pdata->suspend(priv->pdev))
		return -EBUSY;
	return 0;
}

static int m7450_hsic_resume(struct device *dev)
{
	struct m7450_hsic *priv = dev_get_drvdata(dev);
	struct m7450_platform_data *pdata = dev->platform_data;
	m7450_spam("System resume, hsic_resume\n");
	if (pdata->resume && pdata->resume(priv->pdev))
		return -EIO;
	return 0;
}

static int m7450_hsic_suspend_late(struct device *dev)
{
	struct m7450_hsic *priv = dev_get_drvdata(dev);
	m7450_spam("Suspend late\n");

	switch (priv->pm_state) {
		case STATE_DETACHED:
		case STATE_L3:
			break;
		default:
			m7450_dbg("Late suspend aborted in %s state.\n",
					m7450_state_name(priv->pm_state));
			return -EBUSY;
	}

	if (priv->bits) {
		m7450_dbg("State has changed on the way to suspend. "
				"Bits: 0x%.8lX\n",
				priv->bits);
		return -EBUSY;
	}

	return 0;
}

static ssize_t m7450_onsw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	int enable;

	if(sscanf(buf, "%d", &enable) != 1)
		return -EINVAL;

	gpio_set_value(m7450_resources[M7450_RESOURCE_ONSW].start, enable);
	return n;
}
static DEVICE_ATTR(m7450_onsw, 0220, NULL, m7450_onsw_store);

static ssize_t m7450_awr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	int enable;

	if(sscanf(buf, "%d", &enable) != 1)
		return -EINVAL;

	gpio_set_value(m7450_resources[M7450_RESOURCE_AWR].start, enable);
	return n;
}
static DEVICE_ATTR(m7450_awr, 0220, NULL, m7450_awr_store);

static ssize_t m7450_arr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	int enable;

	if(sscanf(buf, "%d", &enable) != 1)
		return -EINVAL;

	gpio_set_value(m7450_resources[M7450_RESOURCE_ARR].start, enable);
	return n;
}
static DEVICE_ATTR(m7450_arr, 0220, NULL, m7450_arr_store);

static ssize_t m7450_hsic_power_set(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct m7450_hsic *priv = dev_get_drvdata(dev);
	struct usb_device *usb_dev;
	int power_on;

	if (sscanf(buf, "%d", &power_on) != 1)
		return -EINVAL;

	/* only run "off" */
	if (!power_on) {
		m7450_dbg("Forced power off.\n");
		spin_lock_irq(&priv->lock);
		usb_dev = usb_get_dev(priv->usb_dev);
		spin_unlock_irq(&priv->lock);
		if (usb_dev) {
			pm_runtime_get_sync(&usb_dev->dev);
			usb_put_dev(usb_dev);
		}
		m7450_notify_evt(priv, BIT(FAST_DETACH));
	}
	return count;
}
static DEVICE_ATTR(power_on, 0220, NULL, m7450_hsic_power_set);

static int m7450_dev_cwr_get(struct platform_device *pdev)
{
	const struct resource *r = pdev->resource;
	int cwr = r[M7450_RESOURCE_CWR].start;
	return !!gpio_get_value(cwr);
}

static void m7450_dev_awr_set(struct platform_device *pdev, bool on)
{
	const struct resource *r = pdev->resource;
	int awr = r[M7450_RESOURCE_AWR].start;
	gpio_set_value(awr, on);
	m7450_dbg("AWR set %d\n", on);
}

static void m7450_dev_arr_set(struct platform_device *pdev, bool on)
{
	const struct resource *r = pdev->resource;
	int arr = r[M7450_RESOURCE_ARR].start;
	gpio_set_value(arr, on);
	m7450_dbg("ARR set %d\n", on);
}

static irqreturn_t m7450_hsic_cwr(int irq, void *data)
{
#if defined(USE_DEFAULT_ISR_INSTEAD_OF_WAITQUEUE)
	struct platform_device *pdev = data;
	struct m7450_platform_data *pdata = pdev->dev.platform_data;
	struct m7450_hsic *priv = dev_get_drvdata(&pdev->dev);
	int val=0;

	val = pdata->cwr_get(pdev);

	if (val == priv->cwr_state) {
		m7450_err("Glitch on CWR %s\n", val ? "high" : "low");
		return IRQ_NONE;
	}

	m7450_dbg("Interrupt CWR %s . pm_state = %s.\n"
				, val ? "high" : "low"
				, m7450_state_name(priv->pm_state));

#if defined(PREVENT_SUSPEND_DURING_RESUME)
	if (val) {
		if ( (priv->pm_state != STATE_DETACHED)
			  && (priv->pm_state != STATE_ATTACH)
			  && (priv->pm_state != STATE_L2_AC_WAKEUP)
			  && (priv->pm_state != STATE_L3_AC_WAKEUP)
			  ){
			m7450_dbg("During suspend. Need wake_lock.\n");
			m7450_wake_lock();
		}
	}
#endif

	priv->cwr_state = val;
	m7450_notify_evt(priv, val ? BIT(CWR_RISE) : BIT(CWR_FALL));
#else
	printk("****** CWR Interrupt Handler *******\n");
	printk("schedule_work. rtn = %d. \n", schedule_work(&m7450_wq_irq.irq_work));
#endif

	return IRQ_HANDLED;
}

static void m7450_gpio_irq_work(struct work_struct *data)
{
	struct m7450_wq_gpio_irq *qw = container_of(data, struct m7450_wq_gpio_irq, irq_work);
	struct platform_device *pdev = qw->pdev;

	struct m7450_platform_data *pdata = pdev->dev.platform_data;
	struct m7450_hsic *priv = dev_get_drvdata(&pdev->dev);
	int val=0;

	val = pdata->cwr_get(pdev);

	if (val == priv->cwr_state) {
		m7450_err("Glitch on CWR %s\n", val ? "high" : "low");
	}
	else {
		m7450_dbg("Interrupt CWR %s !!!!!!!!!!\n", val ? "high" : "low");
		priv->cwr_state = val;
		m7450_notify_evt(priv, val ? BIT(CWR_RISE) : BIT(CWR_FALL));
	}
}

static int m7450_dev_cwr_subscribe(struct platform_device *pdev, irqreturn_t
		(*cwr_cb)(int irq, void *data))
{
	int ret = 0;
	struct m7450_platform_data *pdata = pdev->dev.platform_data;
	const struct resource *r = pdev->resource;

	int cwr_irq = gpio_to_irq(r[M7450_RESOURCE_CWR].start);
	if(cwr_irq < 0) {
		ret = -EINVAL;
		return ret;
	}

	if (pdata->cwr_cb) {
		disable_irq_wake(cwr_irq);
		free_irq(cwr_irq, pdev);
	}

	pdata->cwr_cb = cwr_cb;
	if (cwr_cb) {
		ret = odin_gpio_sms_request_irq(cwr_irq, pdata->cwr_cb,
				 IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING ,
				 r[M7450_RESOURCE_CWR].name, pdev);
		if (ret < 0)
			pdata->cwr_cb = NULL;
		else
			enable_irq_wake(cwr_irq);
	}
	return ret;
}


/* output, export, wake_capable */
static const struct m7450_gpio_descriptor m7450_gpios[M7450_RESOURCE_COUNT] = {
	[M7450_RESOURCE_ONSW] = {true, true, false},
	[M7450_RESOURCE_PST] = {false, true, false},
	[M7450_RESOURCE_RO2] = {false, true, true},
	[M7450_RESOURCE_CWR] = {false, false, true},
	[M7450_RESOURCE_AWR] = {true, false, false},
	[M7450_RESOURCE_ARR] = {true, false, false},
};

static int m7450_dev_init(struct platform_device *pdev)
{
	struct m7450_platform_data *pdata = pdev->dev.platform_data;
	const struct resource *r = pdev->resource;
	int i;
	int ret = 0;

	for (i = 0; i < M7450_RESOURCE_COUNT; ++i) {
		if (r[i].name) {
			ret = gpio_request(r[i].start, r[i].name);
			if (ret) {
				m7450_dbg("gpio %d request fail \n", (int)r[i].start);
				goto fail_gpio;
			}
			if (m7450_gpios[i].output)
				gpio_direction_output(r[i].start, 0);
			else
				gpio_direction_input(r[i].start);
			if (m7450_gpios[i].export)
				gpio_export(r[i].start, true);
			if (m7450_gpios[i].wake_capable){
				enable_irq_wake(gpio_to_irq(r[i].start));
				/*--------------------------------------------------
				* ret = enable_irq_wake(gpio_to_irq(r[i].start));
				* if(ret < 0)
				* 	m7450_dbg("%d enable_irq_wake request \
				* 			fail,return %d \
				* 			\n",(int)r[i].start,ret);
				*--------------------------------------------------*/
			}
		}
	}
	pdata->power_mode = M7450_HSIC_POWER_OFF;

	return 0;

fail_gpio:
	gpio_free(r[i].start);
	return ret;

}

static void m7450_dev_deinit(struct platform_device *pdev)
{
	const struct resource *r = pdev->resource;
	int i;

	m7450_dev_cwr_subscribe(pdev, NULL);
	m7450_dev_awr_set(pdev, false);
	m7450_dev_arr_set(pdev, false);
	m7450_dev_power(pdev, false);

	for (i = 0; i < M7450_RESOURCE_COUNT; ++i) {
		if (r[i].name) {
			if (m7450_gpios[i].output)
				gpio_set_value(r[i].start, 0);
			if (m7450_gpios[i].wake_capable)
				disable_irq_wake(gpio_to_irq(r[i].start));
			if (m7450_gpios[i].export)
				gpio_unexport(r[i].start);
			gpio_free(r[i].start);
		}
	}

}

static int m7450_get_gpio_num(struct platform_device *pdev, const struct
		resource *r)
{
	int i;
	int ret = 0;

	for (i = 0; i < M7450_RESOURCE_COUNT; ++i) {
		ret = of_property_read_u32(pdev->dev.of_node, r[i].name,
				&r[i].start);
		if (ret)
			goto fail_gpio;

		m7450_dbg("%s %d\n", r[i].name, (int)r[i].start);
	}

	return 0;

fail_gpio:
	m7450_dbg("%s: no gpio number in device tree\n", __func__);
	return ret;

}

static const struct m7450_hsic_id m7450_hsic_id[] = {
	{ .vid = 0x04cc, .pid = 0x0500 },
	{ .vid = 0x04cc, .pid = 0x230f }, /* ipc */
	{ .vid = 0x0bdb, .pid = 0x100e }, /* dumper */
};

static struct m7450_platform_data lge_m7450_data = {
	.num_ids	= ARRAY_SIZE(m7450_hsic_id),
	.ids		= m7450_hsic_id,
	.init		= m7450_dev_init,
	.deinit		= m7450_dev_deinit,
	.power		= m7450_dev_power,
	.awr_set	= m7450_dev_awr_set,
	.arr_set	= m7450_dev_arr_set,
	.cwr_get	= m7450_dev_cwr_get,
	.cwr_subscribe_irq = m7450_dev_cwr_subscribe,
	.regulator 	= "+1.2V_VDDIO_USBHSIC",
};

#ifdef CONFIG_OF
static struct of_device_id m7450_dev_match[] = {
	{
		.compatible     = "ste,m7450-hsic",
	},
	{},
};
#endif

static int m7450_hsic_probe(struct platform_device *pdev)
{
	int ret = 0;

	struct m7450_platform_data *pdata;
	struct m7450_hsic *priv;

	m7450_dbg("m7450_hsic probe start\n");

	platform_device_add_data(pdev, &lge_m7450_data, (sizeof(lge_m7450_data)) );

	ret = m7450_get_gpio_num(pdev, m7450_resources);
	if (ret)
		goto err_nogpio;

	platform_device_add_resources(pdev, m7450_resources,
			ARRAY_SIZE(m7450_resources));

	pdata = pdev->dev.platform_data;

	ret = pdata->init(pdev);
	if (ret)
		return ret;

	m7450_dbg("GPIO request set ok\n");

	priv = kzalloc(sizeof(struct m7450_hsic), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err_nomem;
	}
	m7450_state_change(priv, STATE_DETACHED);

	priv->pdev = pdev;
	dev_set_drvdata(&pdev->dev, priv);

	ret = device_create_file(&pdev->dev, &dev_attr_m7450_onsw);
	if (ret)
		goto err_nofile;
	ret = device_create_file(&pdev->dev, &dev_attr_m7450_awr);
	if (ret)
		goto err_nofile;
	ret = device_create_file(&pdev->dev, &dev_attr_m7450_arr);
	if (ret)
		goto err_nofile;

	ret = device_create_file(&pdev->dev, &dev_attr_power_on);
	if (ret)
		goto err_nofile;

	modem = class_create(THIS_MODULE, "modem");
	m7450_sysfs = device_create(modem, NULL, 0, NULL, "m7450_sysfs");

	ret = sysfs_create_link(&m7450_sysfs->kobj, &pdev->dev.kobj, "m7450");
	if (ret)
		goto err_nofile;

	m7450_dbg("GPIO sysfs ok\n");

	ret = pdata->cwr_subscribe_irq(pdev, m7450_hsic_cwr);
	if (WARN_ON(ret < 0))
		goto err_noirq;
	m7450_dbg("interrupt request ok\n");

	priv->workqueue = create_singlethread_workqueue("m7450_wq");
	if (!priv->workqueue) {
		ret = -ENOMEM;
		goto err_nowq;
	}

	m7450_wq_irq.pdev = pdev;
	INIT_WORK(&m7450_wq_irq.irq_work, m7450_gpio_irq_work);

	INIT_WORK(&priv->work, m7450_hsic_work);
	init_waitqueue_head(&priv->wait_evt);
	init_waitqueue_head(&priv->wait_state);
	spin_lock_init(&priv->lock);

	spin_lock_bh(&m7450_hsic_devices_lock);
	list_add(&priv->link, &m7450_hsic_devices);
	spin_unlock_bh(&m7450_hsic_devices_lock);
	m7450_dbg("workqueue and wait queue ok\n");

	priv->usb_nfb.notifier_call = m7450_hsic_usb_notify;
	usb_register_notify(&priv->usb_nfb);
	m7450_dbg("usb notify register ok\n");


#if defined(PREVENT_SUSPEND_DURING_RESUME)
	wake_lock_init(&m7450_wl, WAKE_LOCK_SUSPEND, "m7450_wake");
#endif

	pm_runtime_enable(&pdev->dev);

	return ret;

err_nogpio:
	m7450_err("failed to get gpio from tree\n");
	return ret;

err_nomem:
	m7450_err("failed to allocate mem\n");
	pdata->deinit(pdev);
	return ret;

err_nowq:
	m7450_err("failed to create workqueue\n");
	destroy_workqueue(priv->workqueue);
	priv->workqueue = NULL;
	return ret;

err_nofile:
	m7450_err("failed to create sysfs link\n");
	dev_set_drvdata(&pdev->dev, NULL);
	kfree(priv);
	return ret;

err_noirq:
	m7450_err("failed to initialize irq\n");
	return ret;

}

static int m7450_hsic_remove(struct platform_device *pdev)
{
	struct m7450_platform_data *pdata = pdev->dev.platform_data;
	struct m7450_hsic *priv = dev_get_drvdata(&pdev->dev);
	struct usb_device_driver *usb_driver;
	struct usb_device *usb_dev;

	m7450_dbg("Remove\n");

	spin_lock_irq(&priv->lock);
	usb_dev = usb_get_dev(priv->usb_dev);
	spin_unlock_irq(&priv->lock);

	pdata->cwr_subscribe_irq(pdev, NULL);

	usb_unregister_notify(&priv->usb_nfb);
	pm_runtime_disable(&pdev->dev);

	if (usb_dev) {
#if defined(CONFIG_ODIN_DWC_USB_HOST)
		generic_pm_hook(NULL, NULL);
#else
		usb_driver = to_usb_device_driver(usb_dev->dev.driver);
		usb_driver->suspend = usb_suspend;
		usb_driver->resume = usb_resume;
#endif
	}

	spin_lock_bh(&m7450_hsic_devices_lock);
	list_del(&priv->link);
	spin_unlock_bh(&m7450_hsic_devices_lock);

	m7450_notify_evt(priv, BIT(TERM));
	flush_workqueue(priv->workqueue);
	if (priv->pm_state == STATE_L0_AWAKE && usb_dev)
		usb_enable_autosuspend(usb_dev);

	usb_put_dev(usb_dev);

	destroy_workqueue(priv->workqueue);
	priv->workqueue = NULL;

	dev_set_drvdata(&pdev->dev, NULL);
	kfree(priv);
	pdata->deinit(pdev);

#if defined(PREVENT_SUSPEND_DURING_RESUME)
	wake_lock_destroy(&m7450_wl);
#endif

	return 0;
}

static const struct dev_pm_ops m7450_hsic_pm_ops = {
	.suspend = m7450_hsic_suspend,
	.resume = m7450_hsic_resume,
	/* TODO: HPM suspend_noirq prevents suspend mode */
	/* .suspend_noirq = m7450_hsic_suspend_late, */
};

static struct platform_driver m7450_hsic_plat_drv = {
	.probe = m7450_hsic_probe,
	.remove = m7450_hsic_remove,
	.driver = {
		.name = "m7450-hsic",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(m7450_dev_match),
		.pm = &m7450_hsic_pm_ops,
	},
};

static int __init m7450_hsic_init(void)
{
	spin_lock_init(&m7450_hsic_devices_lock);
	INIT_LIST_HEAD(&m7450_hsic_devices);

	return platform_driver_probe (&m7450_hsic_plat_drv, m7450_hsic_probe);
}

static void __exit m7450_hsic_exit(void)
{
	platform_driver_unregister(&m7450_hsic_plat_drv);
}

module_init(m7450_hsic_init);
module_exit(m7450_hsic_exit);

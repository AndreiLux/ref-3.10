/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */





#include <linux/ioport.h>
#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_defs.h>
#include <kbase/mali_kbase_config.h>
#ifdef CONFIG_UMP
#include <linux/ump-common.h>
#endif				/* CONFIG_UMP */

#include <linux/opp.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_REPORT_VSYNC
#include <linux/export.h>
#endif

/* Versatile Express (VE) configuration defaults shared between config_attributes[]
 * and config_attributes_hw_issue_8408[]. Settings are not shared for
 * JS_HARD_STOP_TICKS_SS and JS_RESET_TICKS_SS.
 */
#define KBASE_VE_MEMORY_PER_PROCESS_LIMIT       (1024 * 1024 * 1024UL)	/* 512MB */
#define KBASE_VE_MEMORY_OS_SHARED_MAX           (1280 * 1024 * 1024UL)	/* 768MB */
#define KBASE_VE_MEMORY_OS_SHARED_PERF_GPU      KBASE_MEM_PERF_SLOW
#define KBASE_VE_GPU_FREQ_KHZ_MAX               5000
#define KBASE_VE_GPU_FREQ_KHZ_MIN               5000
#ifdef CONFIG_UMP
#define KBASE_VE_UMP_DEVICE                     UMP_DEVICE_Z_SHIFT
#endif				/* CONFIG_UMP */

#define KBASE_VE_JS_SCHEDULING_TICK_NS_DEBUG    15000000u      /* 15ms, an agressive tick for testing purposes. This will reduce performance significantly */
#define KBASE_VE_JS_SOFT_STOP_TICKS_DEBUG       1	/* between 15ms and 30ms before soft-stop a job */
#define KBASE_VE_JS_HARD_STOP_TICKS_SS_DEBUG    333	/* 5s before hard-stop */
#define KBASE_VE_JS_HARD_STOP_TICKS_SS_8401_DEBUG 2000	/* 30s before hard-stop, for a certain GLES2 test at 128x128 (bound by combined vertex+tiler job) - for issue 8401 */
#define KBASE_VE_JS_HARD_STOP_TICKS_NSS_DEBUG   100000	/* 1500s (25mins) before NSS hard-stop */
#define KBASE_VE_JS_RESET_TICKS_SS_DEBUG        500	/* 45s before resetting GPU, for a certain GLES2 test at 128x128 (bound by combined vertex+tiler job) */
#define KBASE_VE_JS_RESET_TICKS_SS_8401_DEBUG   3000	/* 7.5s before resetting GPU - for issue 8401 */
#define KBASE_VE_JS_RESET_TICKS_NSS_DEBUG       100166	/* 1502s before resetting GPU */

#define KBASE_VE_JS_SCHEDULING_TICK_NS          2500000000u	/* 2.5s */
#define KBASE_VE_JS_SOFT_STOP_TICKS             1	/* 2.5s before soft-stop a job */
#define KBASE_VE_JS_HARD_STOP_TICKS_SS          2	/* 5s before hard-stop */
#define KBASE_VE_JS_HARD_STOP_TICKS_SS_8401     12	/* 30s before hard-stop, for a certain GLES2 test at 128x128 (bound by combined vertex+tiler job) - for issue 8401 */
#define KBASE_VE_JS_HARD_STOP_TICKS_NSS         600	/* 1500s before NSS hard-stop */
#define KBASE_VE_JS_RESET_TICKS_SS              3	/* 7.5s before resetting GPU */
#define KBASE_VE_JS_RESET_TICKS_SS_8401         18	/* 45s before resetting GPU, for a certain GLES2 test at 128x128 (bound by combined vertex+tiler job) - for issue 8401 */
#define KBASE_VE_JS_RESET_TICKS_NSS             601	/* 1502s before resetting GPU */

#define KBASE_VE_JS_RESET_TIMEOUT_MS            3000	/* 3s before cancelling stuck jobs */
#define KBASE_VE_JS_CTX_TIMESLICE_NS            1000000	/* 1ms - an agressive timeslice for testing purposes (causes lots of scheduling out for >4 ctxs) */
#define KBASE_VE_SECURE_BUT_LOSS_OF_PERFORMANCE	((uintptr_t)MALI_FALSE)	/* By default we prefer performance over security on r0p0-15dev0 and KBASE_CONFIG_ATTR_ earlier */
#define KBASE_VE_POWER_MANAGEMENT_CALLBACKS     ((uintptr_t)&pm_callbacks)
#define KBASE_PLATFORM_CALLBACKS                ((uintptr_t)&platform_funcs)

/* Set this to 1 to enable dedicated memory banks */
#define HARD_RESET_AT_POWER_OFF 0

#ifndef CONFIG_OF
static kbase_io_resources io_resources = {
	.job_irq_number = 68,
	.mmu_irq_number = 69,
	.gpu_irq_number = 70,
	.io_memory_region = {
			     .start = 0xFC010000,
			     .end = 0xFC010000 + (4096 * 5) - 1}
};
#endif

#define KBASE_K3V3_PLATFORM_GPU_REGULATOR_NAME      "g3d"
#define RUNTIME_PM_DELAY_TIME	100
#define DEFAULT_POLLING_MS	100
#define STOP_POLLING		0

#ifdef CONFIG_REPORT_VSYNC
static kbase_device *kbase_dev;
#endif

static inline void kbase_platform_on(struct kbase_device *kbdev)
{
	if (kbdev->regulator) {
		if (unlikely(regulator_enable(kbdev->regulator)))
			KBASE_DEBUG_PRINT_ERROR(KBASE_PM, "Failed to enable regulator");
	}
	KBASE_DEBUG_PRINT_INFO(KBASE_PM, "poweron");
}

static inline void kbase_platform_off(struct kbase_device *kbdev)
{
	if (kbdev->regulator)
		regulator_disable(kbdev->regulator);

	KBASE_DEBUG_PRINT_INFO(KBASE_PM, "poweroff");
}

#ifdef CONFIG_PM_DEVFREQ
static int mali_kbase_devfreq_target(struct device *dev, unsigned long *_freq,
			      u32 flags)
{
	struct kbase_device *kbdev = (struct kbase_device *)dev->platform_data;
	unsigned long old_freq = kbdev->devfreq->previous_freq;
	struct opp *opp = NULL;
	unsigned long freq;

	rcu_read_lock();
	opp = devfreq_recommended_opp(dev, _freq, flags);
	if (IS_ERR(opp)) {
		KBASE_DEBUG_PRINT_ERROR(KBASE_PM,
			"Failed to get Operating Performance Point");
		rcu_read_unlock();
		return PTR_ERR(opp);
	}
	freq = opp_get_freq(opp);
	rcu_read_unlock();

	if (old_freq == freq)
		return 0;

	if (clk_set_rate((kbdev->clk), freq)) {
		KBASE_DEBUG_PRINT_ERROR(KBASE_PM,
			"Failed to set gpu freqency, [%lu->%lu]",
			old_freq, freq);
		return -ENODEV;
	}


	return 0;
}

static int mali_kbase_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	struct kbase_device *kbdev = (struct kbase_device *)dev->platform_data;

	(void)kbase_pm_get_dvfs_action(kbdev);
	stat->busy_time = kbdev->pm.metrics.utilisation;
	stat->total_time = 100;
	stat->private_data = (void *)kbdev->pm.metrics.vsync_hit;
	stat->current_frequency = kbdev->devfreq->previous_freq;

	return 0;
}

static struct devfreq_dev_profile mali_kbase_devfreq_profile = {
	/* it would be abnormal to enable devfreq monitor during initialization. */
	.polling_ms	= STOP_POLLING,
	.target		= mali_kbase_devfreq_target,
	.get_dev_status	= mali_kbase_get_dev_status,
};
#endif

#ifdef CONFIG_REPORT_VSYNC
void mali_kbase_pm_report_vsync(int buffer_updated)
{
	if (kbase_dev)
		kbase_pm_report_vsync(kbase_dev, buffer_updated);
}
EXPORT_SYMBOL(mali_kbase_pm_report_vsync);
#endif

#ifdef CONFIG_MALI_T6XX_DVFS
int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation)
{
	return 1;
}

int kbase_platform_dvfs_enable(kbase_device *kbdev, bool enable, int freq)
{
	unsigned long flags;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	if (enable != kbdev->pm.metrics.timer_active) {
		if (enable) {
			spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);
			kbdev->pm.metrics.timer_active = MALI_TRUE;
			spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);
			hrtimer_start(&kbdev->pm.metrics.timer,
					HR_TIMER_DELAY_MSEC(kbdev->pm.platform_dvfs_frequency),
					HRTIMER_MODE_REL);
		} else {
			spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);
			kbdev->pm.metrics.timer_active = MALI_FALSE;
			spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);
			hrtimer_cancel(&kbdev->pm.metrics.timer);
		}
	}

	return 1;
}
#endif

static mali_bool kbase_platform_init(struct kbase_device *kbdev)
{
	struct kbase_os_device *osdev = &kbdev->osdev;
	osdev->dev->platform_data = kbdev;

#ifdef CONFIG_REPORT_VSYNC
	kbase_dev = kbdev;
#endif

	kbdev->clk = devm_clk_get(osdev->dev, NULL);
	if (IS_ERR(kbdev->clk)) {
		KBASE_DEBUG_PRINT_ERROR(KBASE_PM, "Failed to get clk");
		return MALI_FALSE;
	}

	kbdev->regulator = devm_regulator_get(osdev->dev, KBASE_K3V3_PLATFORM_GPU_REGULATOR_NAME);
	if (IS_ERR(kbdev->regulator)) {
		KBASE_DEBUG_PRINT_ERROR(KBASE_PM, "Failed to get regulator");
		return MALI_FALSE;
	}

#ifdef CONFIG_PM_DEVFREQ
	if (of_init_opp_table(osdev->dev) ||
		opp_init_devfreq_table(osdev->dev,
			&mali_kbase_devfreq_profile.freq_table)) {
		KBASE_DEBUG_PRINT_WARN(KBASE_PM, "Failed to init devfreq_table");
		kbdev->devfreq = NULL;
	} else {
		mali_kbase_devfreq_profile.initial_freq	= clk_get_rate(kbdev->clk);
		rcu_read_lock();
		mali_kbase_devfreq_profile.max_state = opp_get_opp_count(osdev->dev);
		rcu_read_unlock();
		kbdev->devfreq = devfreq_add_device(osdev->dev,
						&mali_kbase_devfreq_profile,
						"mali_ondemand",
						NULL);
	}

	if (IS_ERR(kbdev->devfreq)) {
		KBASE_DEBUG_PRINT_WARN(KBASE_PM, "NULL pointer [kbdev->devFreq]");
		return MALI_FALSE;
	}

	/* make devfreq function */
	mali_kbase_devfreq_profile.polling_ms = DEFAULT_POLLING_MS;
#endif
	return MALI_TRUE;
}

static void kbase_platform_term(struct kbase_device *kbdev)
{
#ifdef CONFIG_PM_DEVFREQ
	devfreq_remove_device(kbdev->devfreq);
#endif
}

kbase_platform_funcs_conf platform_funcs = {
	.platform_init_func = &kbase_platform_init,
	.platform_term_func = &kbase_platform_term,
};

#ifdef CONFIG_MALI_T6XX_RT_PM
static int pm_callback_power_on(kbase_device *kbdev)
{
	int result;
	int ret_val;
	struct kbase_os_device *osdev = &kbdev->osdev;

#if (HARD_RESET_AT_POWER_OFF != 1)
	if (!pm_runtime_status_suspended(osdev->dev))
		ret_val = 0;
	else
#endif
		ret_val = 1;

	if (unlikely(osdev->dev->power.disable_depth > 0)) {
		kbase_platform_on(kbdev);
	} else {
		result = pm_runtime_resume(osdev->dev);
		if (result < 0 && result == -EAGAIN)
			kbase_platform_on(kbdev);
		else if (result < 0)
			KBASE_DEBUG_PRINT_ERROR(KBASE_PM, "pm_runtime_resume failed (%d)\n", result);
	}

	return ret_val;
}

static void pm_callback_power_off(kbase_device *kbdev)
{
	struct kbase_os_device *osdev = &kbdev->osdev;

#if HARD_RESET_AT_POWER_OFF
	/* Cause a GPU hard reset to test whether we have actually idled the GPU
	 * and that we properly reconfigure the GPU on power up.
	 * Usually this would be dangerous, but if the GPU is working correctly it should
	 * be completely safe as the GPU should not be active at this point.
	 * However this is disabled normally because it will most likely interfere with
	 * bus logging etc.
	 */
	KBASE_TRACE_ADD(kbdev, CORE_GPU_HARD_RESET, NULL, NULL, 0u, 0);
	kbase_os_reg_write(kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_HARD_RESET);
#endif

	if (unlikely(osdev->dev->power.disable_depth > 0))
		kbase_platform_off(kbdev);
	else
		pm_schedule_suspend(osdev->dev, RUNTIME_PM_DELAY_TIME);
}

static mali_error pm_callback_runtime_init(kbase_device *kbdev)
{
	pm_suspend_ignore_children(kbdev->osdev.dev, true);
	pm_runtime_enable(kbdev->osdev.dev);
	return MALI_ERROR_NONE;
}

static void pm_callback_runtime_term(kbase_device *kbdev)
{
	pm_runtime_disable(kbdev->osdev.dev);
}

static void pm_callback_runtime_off(kbase_device *kbdev)
{
#ifdef CONFIG_PM_DEVFREQ
	devfreq_suspend_device(kbdev->devfreq);
#elif defined(CONFIG_MALI_T6XX_DVFS)
	kbase_platform_dvfs_enable(kbdev, false, 0);
#endif

	kbase_platform_off(kbdev);
}

static int pm_callback_runtime_on(kbase_device *kbdev)
{
	kbase_platform_on(kbdev);

#ifdef CONFIG_PM_DEVFREQ
	devfreq_resume_device(kbdev->devfreq);
#elif defined(CONFIG_MALI_T6XX_DVFS)
	if (kbase_platform_dvfs_enable(kbdev, true, 0) != MALI_TRUE)
		return -EPERM;
#endif

	return 0;
}

static inline void pm_callback_suspend(struct kbase_device *kbdev)
{
	if (!pm_runtime_status_suspended(kbdev->osdev.dev))
		pm_callback_runtime_off(kbdev);
}

static inline void pm_callback_resume(struct kbase_device *kbdev)
{
	if (!pm_runtime_status_suspended(kbdev->osdev.dev))
		pm_callback_runtime_on(kbdev);
	else
		pm_callback_power_on(kbdev);
}


static kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback = pm_callback_suspend,
	.power_resume_callback = pm_callback_resume,
#ifdef CONFIG_PM_RUNTIME
	.power_runtime_init_callback = pm_callback_runtime_init,
	.power_runtime_term_callback = pm_callback_runtime_term,
	.power_runtime_off_callback = pm_callback_runtime_off,
	.power_runtime_on_callback = pm_callback_runtime_on
#else
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_off_callback = NULL,
	.power_runtime_on_callback = NULL
#endif
};
#endif

/* Please keep table config_attributes in sync with config_attributes_hw_issue_8408 */
static kbase_attribute config_attributes[] = {
	{
	 KBASE_CONFIG_ATTR_MEMORY_PER_PROCESS_LIMIT,
	 KBASE_VE_MEMORY_PER_PROCESS_LIMIT},
#ifdef CONFIG_UMP
	{
	 KBASE_CONFIG_ATTR_UMP_DEVICE,
	 KBASE_VE_UMP_DEVICE},
#endif				/* CONFIG_UMP */
	{
	 KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_MAX,
	 KBASE_VE_MEMORY_OS_SHARED_MAX},

	{
	 KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_PERF_GPU,
	 KBASE_VE_MEMORY_OS_SHARED_PERF_GPU},

	{
	 KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MAX,
	 KBASE_VE_GPU_FREQ_KHZ_MAX},

	{
	 KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MIN,
	 KBASE_VE_GPU_FREQ_KHZ_MIN},

#ifdef CONFIG_MALI_DEBUG
/* Use more aggressive scheduling timeouts in debug builds for testing purposes */
	{
	 KBASE_CONFIG_ATTR_JS_SCHEDULING_TICK_NS,
	 KBASE_VE_JS_SCHEDULING_TICK_NS_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS,
	 KBASE_VE_JS_SOFT_STOP_TICKS_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_SS,
	 KBASE_VE_JS_HARD_STOP_TICKS_SS_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_NSS,
	 KBASE_VE_JS_HARD_STOP_TICKS_NSS_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_RESET_TICKS_SS,
	 KBASE_VE_JS_RESET_TICKS_SS_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_RESET_TICKS_NSS,
	 KBASE_VE_JS_RESET_TICKS_NSS_DEBUG},
#else				/* CONFIG_MALI_DEBUG */
/* In release builds same as the defaults but scaled for 5MHz FPGA */
	{
	 KBASE_CONFIG_ATTR_JS_SCHEDULING_TICK_NS,
	 KBASE_VE_JS_SCHEDULING_TICK_NS},

	{
	 KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS,
	 KBASE_VE_JS_SOFT_STOP_TICKS},

	{
	 KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_SS,
	 KBASE_VE_JS_HARD_STOP_TICKS_SS},

	{
	 KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_NSS,
	 KBASE_VE_JS_HARD_STOP_TICKS_NSS},

	{
	 KBASE_CONFIG_ATTR_JS_RESET_TICKS_SS,
	 KBASE_VE_JS_RESET_TICKS_SS},

	{
	 KBASE_CONFIG_ATTR_JS_RESET_TICKS_NSS,
	 KBASE_VE_JS_RESET_TICKS_NSS},
#endif				/* CONFIG_MALI_DEBUG */
	{
	 KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS,
	 KBASE_VE_JS_RESET_TIMEOUT_MS},

	{
	 KBASE_CONFIG_ATTR_JS_CTX_TIMESLICE_NS,
	 KBASE_VE_JS_CTX_TIMESLICE_NS},

#ifdef CONFIG_MALI_T6XX_RT_PM
	{
	 KBASE_CONFIG_ATTR_POWER_MANAGEMENT_CALLBACKS,
	 KBASE_VE_POWER_MANAGEMENT_CALLBACKS},
#endif
	{
	 KBASE_CONFIG_ATTR_PLATFORM_FUNCS,
	 KBASE_PLATFORM_CALLBACKS},

	{
	 KBASE_CONFIG_ATTR_SECURE_BUT_LOSS_OF_PERFORMANCE,
	 KBASE_VE_SECURE_BUT_LOSS_OF_PERFORMANCE},

	{
	 KBASE_CONFIG_ATTR_GPU_IRQ_THROTTLE_TIME_US,
	 20},

	{
	 KBASE_CONFIG_ATTR_END,
	 0}
};

static kbase_platform_config k3v3_platform_config = {
	.attributes = config_attributes,
#ifndef CONFIG_OF
	.io_resources = &io_resources
#endif
};

kbase_platform_config *kbase_get_platform_config(void)
{
	return &k3v3_platform_config;
}

int kbase_platform_early_init(void)
{
	/* Nothing needed at this stage */
	return 0;
}

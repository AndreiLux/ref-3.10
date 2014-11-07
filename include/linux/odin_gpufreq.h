#ifndef __ODIN_GPUFREQ_H__
#define __ODIN_GPUFREQ_H__

#define ODIN_GPUFREQ_PRECHANGE  (0)
#define ODIN_GPUFREQ_POSTCHANGE (1)

struct odin_gpufreq_freqs {
        unsigned int max;
        unsigned int old;
        unsigned int new;
        unsigned int util;
};

extern int odin_gpufreq_register_notifier(struct notifier_block *nb);
extern int odin_gpufreq_unregister_notifier(struct notifier_block *nb);

#endif

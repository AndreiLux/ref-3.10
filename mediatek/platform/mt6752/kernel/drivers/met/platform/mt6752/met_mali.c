#include <linux/module.h>

#include "core/met_drv.h"
#include "core/trace.h"

#include "gator/gator_events_mali_t6xx_hw.c"

static void met_mali_start(void)
{
#ifdef	CONFIG_MET_GATOR
	if (gator_events_mali_t6xx_interface.start() != 0)
		gator_events_mali_t6xx_interface.stop();
#endif
}

static void met_mali_stop(void)
{
#ifdef	CONFIG_MET_GATOR
	gator_events_mali_t6xx_interface.stop();
#endif
}

static void met_mali_polling(unsigned long long stamp, int cpu)
{
#ifdef	CONFIG_MET_GATOR
	int	*buffer, len;

	len = gator_events_mali_t6xx_interface.read(&buffer);
#endif
}

static int mali_print_help(char *buf, int len)
{
	return 0;
}

static int mali_print_header(char *buf, int len)
{
	return 0;
}
static int mali_process_argument(const char *arg, int len)
{
	return 0;
}

static int met_mali_create(struct kobject *parent)
{
	if (gator_events_mali_t6xx_hw_init() != 0)
		return -1;

	return 0;
}

static void met_mali_delete(void)
{
}

struct metdevice met_mali = {
	.name = "mali",
	.owner = THIS_MODULE,
	.type = MET_TYPE_PMU,
	.create_subfs = met_mali_create,
	.delete_subfs = met_mali_delete,
	.cpu_related = 0,
	.start = met_mali_start,
	.stop = met_mali_stop,
	.polling_interval = 0,
	.timed_polling = met_mali_polling,
	.print_help = mali_print_help,
	.print_header = mali_print_header,
	.process_argument = mali_process_argument,
};

#include <linux/module.h>
#include <linux/slab.h>

#include "mmc_user_control.h"

/* START: MMC shutdown related info. */
struct mmc_shutdown_data mmc_shutdown_config = {
	.enabled = 0,
	.pattern_buff = NULL,
	.pattern_len = 0,
	.delay_ms = 0,
};
EXPORT_SYMBOL(mmc_shutdown_config);

/* Must already hold mmc_shutdown_data write_lock */
static void mmc_shutdown_data_clear(struct mmc_shutdown_data *data)
{
	data->enabled = 0;
	kfree(data->pattern_buff);
	data->pattern_buff = NULL;
	data->pattern_len = 0;
}

/* Must already hold mmc_shutdown_data write_lock */
static
void mmc_shutdown_data_set(struct mmc_shutdown_data *data, const char *cmd)
{
	char *pattern;
	int delay_ms;

	if (data == NULL)
		return;
	mmc_shutdown_data_clear(data);

	if (cmd == NULL)
		return;
	pattern = kmalloc(strlen(cmd), GFP_ATOMIC);
	if (sscanf(cmd, "r: %d %s", &delay_ms, pattern) == 2) {
		data->delay_ms = delay_ms;
		data->pattern_buff = pattern;
		data->pattern_len = strlen(pattern);
		data->enabled = 1;
	} else {
		kfree(pattern);
	}
}
/* END: MMC shutdown related info. */

/*
 * TODO: Add support for multiple commands simultaneously.
 */
static
int mmc_user_control_cmd_set(const char *cmd, const struct kernel_param *kp)
{
	char cmd_type;

	/* Clear any mmc user command data to ensure stateless framework. */
	write_lock(&mmc_shutdown_config.root_lock);
	mmc_shutdown_data_clear(&mmc_shutdown_config);
	write_unlock(&mmc_shutdown_config.root_lock);

	if (sscanf(cmd, "%c:", &cmd_type) != 1)
		return 0;

	/* Set any mmc user command data. */
	switch (cmd_type) {
	case 'r':
		write_lock(&mmc_shutdown_config.root_lock);
		mmc_shutdown_data_set(&mmc_shutdown_config, cmd);
		write_unlock(&mmc_shutdown_config.root_lock);
		break;
	default:
		pr_info("Unknown mmc user command: %s\n", cmd);
	}
	return 0;
}

static const struct kernel_param_ops mmc_user_control_cmd_ops = {
	.set = mmc_user_control_cmd_set,
};

/*
 * The following module param is used to control the mmc from user space.
 */
module_param_cb(mmc_user_control_cmd, &mmc_user_control_cmd_ops, NULL, 0600);
MODULE_PARM_DESC(
	mmc_user_control_cmd,
	"The specific command to send to the mmc (see Kconfig for ENABLE_MMC_USER_CONTROL).");


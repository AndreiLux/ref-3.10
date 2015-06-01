#include <linux/module.h>

#include "core/met_drv.h"
#include "core/trace.h"

#include "gator/gator.h"

static LIST_HEAD(gator_events);

int gator_events_get_key(void)
{
	static int key = 3;
	const int ret = key;

	key += 2;
	return ret;
}

int gator_events_install(struct gator_interface *interface)
{
	list_add_tail(&interface->list, &gator_events);

	return 0;
}

static void gator_op_create_files(struct super_block *sb, struct dentry *root)
{
	struct dentry *dir;
	struct gator_interface *gi;

	dir = gatorfs_mkdir(sb, root, "events");
	list_for_each_entry(gi, &gator_events, list)
		if (gi->create_files)
			gi->create_files(sb, dir);
}

#include "gator/gator_fs.c"

static int met_gator_create(struct kobject *parent)
{
	if (gatorfs_register()) {
		return -1;
	}

	return 0;
}

static void met_gator_delete(void)
{
	gatorfs_unregister();
}

struct metdevice met_gator = {
	.name = "gator",
	.owner = THIS_MODULE,
	.type = MET_TYPE_PMU,
	.create_subfs = met_gator_create,
	.delete_subfs = met_gator_delete,
	.cpu_related = 0,
	.polling_interval = 0,
};

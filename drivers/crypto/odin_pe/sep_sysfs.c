/*******************************************************************
* (c) Copyright 2011-2012 Discretix Technologies Ltd.              *
* This software is protected by copyright, international           *
* treaties and patents, and distributed under multiple licenses.   *
* Any use of this Software as part of the Discretix CryptoCell or  *
* Packet Engine products requires a commercial license.            *
* Copies of this Software that are distributed with the Discretix  *
* CryptoCell or Packet Engine product drivers, may be used in      *
* accordance with a commercial license, or at the user's option,   *
* used and redistributed under the terms and conditions of the GNU *
* General Public License ("GPL") version 2, as published by the    *
* Free Software Foundation.                                        *
* This program is distributed in the hope that it will be useful,  *
* but WITHOUT ANY LIABILITY AND WARRANTY; without even the implied *
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. *
* See the GNU General Public License version 2 for more details.   *
* You should have received a copy of the GNU General Public        *
* License version 2 along with this Software; if not, please write *
* to the Free Software Foundation, Inc., 59 Temple Place - Suite   *
* 330, Boston, MA 02111-1307, USA.                                 *
* Any copy or reproduction of this Software, as permitted under    *
* the GNU General Public License version 2, must include this      *
* Copyright Notice as well as any other notices provided under     *
* the said license.                                                *
********************************************************************/



#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>

#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_SYSFS

#include "dx_driver.h"
#include "dx_driver_abi.h"
#include "desc_mgr.h"
#include "sep_log.h"
#include "sep_sysfs.h"

#define MAX_QUEUE_NAME_LEN 50

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct sep_stats {
	spinlock_t stat_lock;
	unsigned long samples_cnt; /* Total number of samples */
	unsigned long long accu_time; /* Accum. samples time (for avg. calc.) */
	unsigned long long min_time;
	unsigned long long max_time;
	/* all times in nano-sec. */
};

#define DESC_TYPE_NUM (1<<SEP_SW_DESC_TYPE_BIT_SIZE)
static const char *desc_names[DESC_TYPE_NUM] = {
	"NULL",    /*SEP_SW_DESC_TYPE_NULL*/
	"CRYPTO",  /*SEP_SW_DESC_TYPE_CRYPTO_OP*/
	"MSG",     /*SEP_SW_DESC_TYPE_MSG_OP*/
	/* Next 12 types are invalid */
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"DEBUG"    /*SEP_SW_DESC_TYPE_DEBUG*/
};

struct kobj_attribute *queue_size[SEP_MAX_NUM_OF_DESC_Q];
struct attribute *queue_attrs[SEP_MAX_NUM_OF_DESC_Q];

struct sep_stats drv_lat_stats[SEP_MAX_NUM_OF_DESC_Q][DXDI_IOC_NR_MAX+1];
struct sep_stats sep_lat_stats[SEP_MAX_NUM_OF_DESC_Q][DESC_TYPE_NUM];

#ifdef PE_IPSEC_QUEUES
/* IPsec queues drop counter - one for inbound and one for outbound */
atomic_t ipsec_drop_cnt[2][IPSEC_DROP_CAUSE_NUM];
#endif

/*
 * Structure used to create a directory and its attributes in sysfs
 */
struct sys_dir {
	struct kobject *sys_dir_kobj;
	struct attribute_group sys_dir_attr_group;
	struct attribute **sys_dir_attr_list;
	int num_of_attrs;
	struct sep_drvdata *drvdata; /* Associated driver context */
};

/* directory initialization*/
static int sys_init_dir(struct sys_dir *sys_dir,  struct sep_drvdata *drvdata,
		struct kobject *parent_dir_kobj,
		const char *dir_name,
		struct kobj_attribute *attrs,
		int num_of_attrs);

/* directory deinitialization */
static void sys_free_dir(struct sys_dir *sys_dir);

/* top level directory structure */
struct sys_dir sys_top_dir;

/* queue level directory structures array */
struct sys_dir sys_queue_dirs[SEP_MAX_NUM_OF_DESC_Q];

#ifdef PE_IPSEC_QUEUES
struct sys_dir sys_ipsec_qs_dir;
#endif

/********************************************************
 *		SYSFS objects				*
 ********************************************************/

/* TOP LEVEL ATTRIBUTES */
static ssize_t sys_fw_ver_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static struct kobj_attribute sys_top_level_attrs[] = {
	__ATTR(fw_ver, 0444, sys_fw_ver_show, NULL)
};

/* SW queues attributes */
static ssize_t sys_queue_size_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t sys_queue_dump_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t sys_queue_stats_drv_lat_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t sys_queue_stats_sep_lat_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

struct kobj_attribute sys_queue_level_attrs[] = {

	__ATTR(size, 0444, sys_queue_size_show, NULL),
	__ATTR(dump, 0444, sys_queue_dump_show, NULL),
	__ATTR(drv_lat, 0444, sys_queue_stats_drv_lat_show, NULL),
	__ATTR(sep_lat, 0444, sys_queue_stats_sep_lat_show, NULL)
};

#ifdef PE_IPSEC_QUEUES
/* IPsec packet queues attributes */
static ssize_t sys_ipsec_qs_drop_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

struct kobj_attribute sys_ipsec_qs_attrs[] = {
	__ATTR(in_drop_defrag, 0444, sys_ipsec_qs_drop_show, NULL),
	__ATTR(in_drop_skb_mem, 0444, sys_ipsec_qs_drop_show, NULL),
	__ATTR(in_drop_q_full , 0444, sys_ipsec_qs_drop_show, NULL),
	__ATTR(in_drop_dma_map, 0444, sys_ipsec_qs_drop_show, NULL),
	__ATTR(out_drop_defrag, 0444, sys_ipsec_qs_drop_show, NULL),
	__ATTR(out_drop_skb_mem, 0444, sys_ipsec_qs_drop_show, NULL),
	__ATTR(out_drop_q_full , 0444, sys_ipsec_qs_drop_show, NULL),
	__ATTR(out_drop_dma_map, 0444, sys_ipsec_qs_drop_show, NULL),
};
#endif

/**************************************
 * Statistics functions section       *
 **************************************/

#ifdef PE_IPSEC_QUEUES
void sysfs_update_ipsec_drop_count(
	int is_outbound, enum ipsec_drop_cause cause)
{
	int qid;

	if ((cause < 0) || (cause >= IPSEC_DROP_CAUSE_NUM)) {
		SEP_LOG_ERR("Invalid cause %d\n", cause);
		return;
	}
	if (is_outbound)
		qid = 1;
	else
		qid = 0;
	atomic_inc(&ipsec_drop_cnt[qid][cause]);
}
#endif


static void update_stats(struct sep_stats *stats,
	unsigned long long start_ns, unsigned long long end_ns)
{
	unsigned long long delta;
	unsigned long flags;

	spin_lock_irqsave(&(stats->stat_lock), flags);

	delta = end_ns - start_ns;
	stats->samples_cnt++;
	stats->accu_time += delta;
	stats->min_time = MIN(delta, stats->min_time);
	stats->max_time = MAX(delta, stats->max_time);

	spin_unlock_irqrestore(&(stats->stat_lock), flags);
}

void sysfs_update_drv_stats(unsigned int qid, unsigned int ioctl_cmd_type,
	unsigned long long start_ns, unsigned long long end_ns)
{
	if ((qid >= SEP_MAX_NUM_OF_DESC_Q) ||
	    (ioctl_cmd_type > DXDI_IOC_NR_MAX)) {
		SEP_LOG_ERR("IDs out of range: qid=%d , ioctl_cmd=%d\n",
			    qid, ioctl_cmd_type);
		return;
	}

	update_stats(&(drv_lat_stats[qid][ioctl_cmd_type]), start_ns, end_ns);
}

void sysfs_update_sep_stats(unsigned int qid, enum sep_sw_desc_type desc_type,
	unsigned long long start_ns, unsigned long long end_ns)
{
	if ((qid >= SEP_MAX_NUM_OF_DESC_Q) ||
	    (desc_type >= DESC_TYPE_NUM)) {
		SEP_LOG_ERR("IDs out of range: qid=%d , descriptor_type=%d\n",
			    qid, desc_type);
		return;
	}
	update_stats(&(sep_lat_stats[qid][desc_type]), start_ns, end_ns);
}


/* compute queue number based by kobject passed to attribute show function */
static int sys_get_queue_num(struct kobject *kobj, struct sys_dir *dirs)
{
	int i;

	for (i = 0; i < SEP_MAX_NUM_OF_DESC_Q; ++i) {
		if (dirs[i].sys_dir_kobj == kobj)
			break;
	}

	return i;
}

static struct sep_drvdata *sys_get_drvdata(struct kobject *kobj)
{
	/* TODO: supporting multiple SeP devices would require avoiding
	 * global "top_dir" and finding associated "top_dir" by traversing
	 * up the tree to the kobject which matches one of the top_dir's */
	return sys_top_dir.drvdata;
}

/**************************************
 * Attributes show functions section  *
 **************************************/

static ssize_t sys_fw_ver_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct sep_drvdata *drvdata = sys_get_drvdata(kobj);
	return sprintf(buf,
		"ROM_VER=0x%08X\nFW_VER=0x%08X\n",
		drvdata->rom_ver, drvdata->fw_ver);
}

static ssize_t sys_queue_size_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{

	return sprintf(buf, "<not supported>\n");
}

static ssize_t sys_queue_dump_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i;

	i = sys_get_queue_num(kobj, (struct sys_dir *) &sys_queue_dirs);

	return sprintf(buf, "DescQ dump not supported, yet.\n");
}

/* time from write to read is measured */
static ssize_t sys_queue_stats_drv_lat_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	uint64_t min_usec, max_usec, avg_usec;
	int qid, i;
	char *cur_buf_pos = buf;

	qid = sys_get_queue_num(kobj, (struct sys_dir *)&sys_queue_dirs);

	cur_buf_pos += sprintf(cur_buf_pos,
			"ioctl#\tmin[us]\tavg[us]\tmax[us]\t#samples\n");

	for (i = 0; i < DXDI_IOC_NR_MAX+1; i++) {
		/* Because we are doing 64 bit (long long) division we
		   need to explicitly invoke do_div() */
		if (drv_lat_stats[qid][i].samples_cnt > 0) {
			min_usec = drv_lat_stats[qid][i].min_time;
			do_div(min_usec , 1000); /* result goes into dividend */
			max_usec = drv_lat_stats[qid][i].max_time;
			do_div(max_usec , 1000);
			avg_usec = drv_lat_stats[qid][i].accu_time;
			do_div(avg_usec, drv_lat_stats[qid][i].samples_cnt);
			do_div(avg_usec , 1000);
		} else {
			min_usec = max_usec = avg_usec = 0;
		}

		cur_buf_pos += sprintf(cur_buf_pos,
			"%u:\t%6llu\t%6llu\t%6llu\t%7lu\n", i,
			min_usec, avg_usec, max_usec,
			drv_lat_stats[qid][i].samples_cnt);
	}
	return cur_buf_pos - buf;
}

/* time from descriptor enqueue to interrupt is measured */
static ssize_t sys_queue_stats_sep_lat_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	uint64_t min_usec, max_usec, avg_usec;
	int qid, i, buf_len;
	char line[256];

	buf_len = sprintf(buf,
		"desc-type\tmin[us]\tavg[us]\tmax[us]\t#samples\n");

	qid = sys_get_queue_num(kobj, (struct sys_dir *)&sys_queue_dirs);

	for (i = 0; i < DESC_TYPE_NUM; ++i) {
		if (desc_names[i] != NULL) { /*Only if valid desc. type*/
			/* Because we are doing 64 bit (long long) division we*
			   need to explicitly invoke do_div() */
			if (sep_lat_stats[qid][i].samples_cnt > 0) {
				min_usec = sep_lat_stats[qid][i].min_time;
				/* result goes into dividend */
				do_div(min_usec , 1000);
				max_usec = sep_lat_stats[qid][i].max_time;
				do_div(max_usec , 1000);
				avg_usec = sep_lat_stats[qid][i].accu_time;
				do_div(avg_usec,
					sep_lat_stats[qid][i].samples_cnt);
				do_div(avg_usec , 1000);
			} else {
				min_usec = max_usec = avg_usec = 0;
			}

			buf_len += sprintf(line,
				"%s\t\t%6llu\t%6llu\t%6llu\t%7lu\n",
				desc_names[i], min_usec, avg_usec, max_usec,
				sep_lat_stats[qid][i].samples_cnt);
			strcat(buf, line);
		}
	}
	return buf_len;
}

#ifdef PE_IPSEC_QUEUES
static ssize_t sys_ipsec_qs_drop_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	/* Pointer arithmetic to get the attr. index */
	const int attr_index = attr - sys_ipsec_qs_attrs;
	int qid;
	enum ipsec_drop_cause cause;

	if (attr_index >= (2 * IPSEC_DROP_CAUSE_NUM)) {
		SEP_LOG_ERR("Invalid attribute index %d (%s)\n",
			attr_index, attr->attr.name);
		return -ENOENT;
	}
	qid = attr_index / IPSEC_DROP_CAUSE_NUM;
	cause = attr_index % IPSEC_DROP_CAUSE_NUM;
	SEP_LOG_DEBUG("%s[%d]=%d\n", qid == 0 ? "IN" : "OUT", cause,
		atomic_read(&ipsec_drop_cnt[qid][cause]));
	return sprintf(buf, "%d\n", atomic_read(&ipsec_drop_cnt[qid][cause]));
}
#endif /*PE_IPSEC_QUEUES*/

int sys_init_dir(struct sys_dir *sys_dir, struct sep_drvdata *drvdata,
		 struct kobject *parent_dir_kobj, const char *dir_name,
		 struct kobj_attribute *attrs, int num_of_attrs)
{
	int i;

	memset(sys_dir, 0, sizeof(struct sys_dir));

	sys_dir->drvdata = drvdata;

	/* initialize directory kobject */
	sys_dir->sys_dir_kobj =
		kobject_create_and_add(dir_name, parent_dir_kobj);

	if (!(sys_dir->sys_dir_kobj))
		return -ENOMEM;
	/* allocate memory for directory's attributes list */
	sys_dir->sys_dir_attr_list =
		kzalloc(sizeof(struct attribute *) * (num_of_attrs + 1),
				GFP_KERNEL);

	if (!(sys_dir->sys_dir_attr_list)) {
		kobject_put(sys_dir->sys_dir_kobj);
		return -ENOMEM;
	}

	sys_dir->num_of_attrs = num_of_attrs;

	/* initialize attributes list */
	for (i = 0; i < num_of_attrs; ++i)
		sys_dir->sys_dir_attr_list[i] = &(attrs[i].attr);

	/* last list entry should be NULL */
	sys_dir->sys_dir_attr_list[num_of_attrs] = NULL;

	sys_dir->sys_dir_attr_group.attrs = sys_dir->sys_dir_attr_list;

	return sysfs_create_group(sys_dir->sys_dir_kobj,
			&(sys_dir->sys_dir_attr_group));
}

void sys_free_dir(struct sys_dir *sys_dir)
{
	if (!sys_dir)
		return;

	kfree(sys_dir->sys_dir_attr_list);

	if (sys_dir->sys_dir_kobj)
		kobject_put(sys_dir->sys_dir_kobj);
}

/* free sysfs directory structures */
void sep_free_sysfs(void)
{
	int j;

#ifdef PE_IPSEC_QUEUES
	sys_free_dir(&sys_ipsec_qs_dir);
#endif
	for (j = 0; (j < SEP_MAX_NUM_OF_DESC_Q) &&
		(sys_queue_dirs[j].sys_dir_kobj != NULL); ++j) {
		sys_free_dir(&(sys_queue_dirs[j]));
	}

	if (sys_top_dir.sys_dir_kobj != NULL)
		sys_free_dir(&sys_top_dir);

}

/* initialize sysfs directories structures */
int sep_setup_sysfs(struct kobject *sys_dev_kobj, struct sep_drvdata *drvdata)
{
	int retval = 0, i, j;
	char queue_name[MAX_QUEUE_NAME_LEN];

	SEP_LOG_DEBUG("setup sysfs under %s\n",
			sys_dev_kobj->name);
	/* reset statistics */
	memset(drv_lat_stats, 0, sizeof(drv_lat_stats));
	memset(sep_lat_stats, 0, sizeof(sep_lat_stats));

	for (i = 0; i < SEP_MAX_NUM_OF_DESC_Q; i++) {
		for (j = 0; j < DXDI_IOC_NR_MAX+1; j++) {
			spin_lock_init(&drv_lat_stats[i][j].stat_lock);
			/* set min_time to largest ULL value so first sample
			   becomes the minimum. */
			drv_lat_stats[i][j].min_time = (unsigned long long)-1;
		}
		for (j = 0; j < DESC_TYPE_NUM; j++) {
			spin_lock_init(&sep_lat_stats[i][j].stat_lock);
			/* set min_time to largest ULL value so first sample
			   becomes the minimum. */
			sep_lat_stats[i][j].min_time = (unsigned long long)-1;
		}
	}

#ifdef PE_IPSEC_QUEUES
	for (i = 0; i < 2; i++) /* Per queue */
		for (j = 0; j < IPSEC_DROP_CAUSE_NUM; j++) /* per cause */
		atomic_set(&ipsec_drop_cnt[i][j], 0);
#endif

	/* zero all directories structures */
	memset(&sys_top_dir, 0, sizeof(struct sys_dir));
	memset(&sys_queue_dirs, 0,
		sizeof(struct sys_dir) * SEP_MAX_NUM_OF_DESC_Q);
#ifdef PE_IPSEC_QUEUES
	memset(&sys_ipsec_qs_dir, 0, sizeof(struct sys_dir));
#endif

	/* initialize the top directory */
	retval =
		sys_init_dir(&sys_top_dir, drvdata, sys_dev_kobj,
				"sep_info", sys_top_level_attrs,
				sizeof(sys_top_level_attrs)/
					sizeof(struct kobj_attribute));

	if (retval)
		return -retval;

	/* initialize decriptor queues directories structures */
	for (i = 0; i < SEP_MAX_NUM_OF_DESC_Q; ++i) {

		sprintf(queue_name, "queue%d", i);

		retval = sys_init_dir(&(sys_queue_dirs[i]), drvdata,
				sys_top_dir.sys_dir_kobj, queue_name,
				sys_queue_level_attrs,
				sizeof(sys_queue_level_attrs)/
					sizeof(struct kobj_attribute));

		if (retval)
			break;

	}

#ifdef PE_IPSEC_QUEUES
	if (likely(retval == 0)) {
		retval = sys_init_dir(&sys_ipsec_qs_dir, drvdata,
			sys_top_dir.sys_dir_kobj, "ipsec_qs",
			sys_ipsec_qs_attrs,
			sizeof(sys_ipsec_qs_attrs) /
				sizeof(struct kobj_attribute));
	}
#endif
	if (unlikely(retval != 0))
		sep_free_sysfs();

	return -retval;
}

#ifdef SEP_SYSFS_UNIT_TEST

static int __init sep_init(void)
{
	int retval;

	retval = sep_setup_sysfs(kernel_kobj);

	return retval;
}

static void __exit sep_exit(void)
{
	sep_free_sysfs();
}

module_init(sep_init);
module_exit(sep_exit);

#endif

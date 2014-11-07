/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * Younghyun Jo <younghyun.jo@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/printk.h>
#include <linux/vmalloc.h>

struct ama_debugfs
{
	struct dentry *dir;
	struct dentry *file;
	int (*cb_show)(struct seq_file *seq, void *cb_arg);
	void *cb_arg;
};
static struct ama_debugfs *ama_debugfs = NULL;

static int ama_debugfs_open(struct inode *inode, struct file *file)
{
	if (ama_debugfs->cb_show == NULL) {
		pr_err("ama_debugfs->cb_show is NULL\n");
		return -1;
	}

	return single_open(file, ama_debugfs->cb_show, ama_debugfs->cb_arg);
}

static ssize_t ama_debugfs_write(struct file *file, const char __user *buf,
		size_t size, loff_t *offset)
{
	return 0;
}

static const struct file_operations ama_debugfs_ops = {
	.open = ama_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = ama_debugfs_write,
	.release = single_release,
};

void ama_debugfs_init(int (*show)(struct seq_file *seq, void *arg), void *arg)
{
	if (ama_debugfs) {
		pr_warn("[AMA_DEBUGFS] ama_debufs is already initialized\n");
		return;
	}

	if (show == NULL) {
		pr_err("[AMA_DEBUGFS] show callback function is NULL\n");
		return;
	}

	ama_debugfs = vzalloc(sizeof(struct ama_debugfs));
	if (ama_debugfs == NULL) {
		pr_err("vzalloc failed. size:%d\n", sizeof(struct ama_debugfs));
		return;
	}

	ama_debugfs->dir = debugfs_create_dir("ama", NULL);
	if (IS_ERR_OR_NULL(ama_debugfs->dir)) {
		pr_err("debugfs_create_dir failed\n");
		goto err_debugfs_create_dir;
	}

	ama_debugfs->file = debugfs_create_file("status", 0664, ama_debugfs->dir,
			NULL, &ama_debugfs_ops);
	if (IS_ERR_OR_NULL(ama_debugfs->file)) {
		pr_err("debugfs_create_file failed\n");
		goto err_debugfs_create_file;
	}

	ama_debugfs->cb_show = show;
	ama_debugfs->cb_arg = arg;

	return;

err_debugfs_create_file:
	if (ama_debugfs->dir)
		debugfs_remove(ama_debugfs->dir);

err_debugfs_create_dir:
	if (ama_debugfs)
		vfree(ama_debugfs);

	ama_debugfs = NULL;
}

void ama_debugfs_cleanup(void)
{
	if (ama_debugfs == NULL)
		return;

	if (ama_debugfs->file)
		debugfs_remove(ama_debugfs->file);

	if (ama_debugfs->dir)
		debugfs_remove(ama_debugfs->dir);

	vfree(ama_debugfs);

	ama_debugfs = NULL;
}

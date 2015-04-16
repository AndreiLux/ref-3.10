/*
 *  drivers/huawei/device/log_jank.c
 *
 *  Copyright (C) 2014 Huawei Technologies Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/aio.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/log_jank.h>

#define   MAX_WORK_COUNT   10
#define   MAX_TAG_SIZE     128
#define   MAX_MSG_SIZE     256
struct my_work_struct{
    int   bWaiting;
    int   nPrio;
    char  tag[MAX_TAG_SIZE];
    char  msg[MAX_MSG_SIZE];
    struct work_struct writelog;
};

static int              nwork_index = 0;
struct my_work_struct   janklog_work[MAX_WORK_COUNT]; 
struct workqueue_struct *janklog_workqueue = NULL;

static int __write_jank_init(char*, int ,char*);
static int __write_jank_kernel(char*, int ,char*);

static int (*write_jank)(char*,int, char*) = __write_jank_init;
static struct file* filp = NULL;

static DEFINE_MUTEX(janklogfile_mutex);
static DEFINE_RAW_SPINLOCK(janklog_spinlock);

void janklog_write(struct work_struct *p_work)
{
    struct my_work_struct *p_janklog_work = container_of(p_work, struct my_work_struct, writelog);
    write_jank(p_janklog_work->tag, p_janklog_work->nPrio,p_janklog_work->msg);
    p_janklog_work->bWaiting = 0;
}

static int __init janklog_workqueue_init(void)
{
    int   i;

    janklog_workqueue = create_singlethread_workqueue("janklog_workqueue");
    if (!janklog_workqueue)
    {
        pr_err("jank_log: failed to create janklog_workqueue\n");
        return 1;
    }
    nwork_index = 0;
    for (i = 0;i <MAX_WORK_COUNT;i++)
    {
        INIT_WORK(&(janklog_work[i].writelog), janklog_write);
        janklog_work[i].bWaiting = 0;
    }

    return 0;
}

static void __exit janklog_workqueue_destory(void)
{
    if (janklog_workqueue)
    {
        destroy_workqueue(janklog_workqueue);
        janklog_workqueue = NULL;
    }
    if (0 == IS_ERR(filp))
    {
        filp_close(filp, NULL);
        filp = NULL;
    }
}

static int __write_jank_init(char* tag, int prio, char* msg)
{
    mm_segment_t oldfs;

    mutex_lock(&janklogfile_mutex);
    if (__write_jank_init == write_jank)
    {
        oldfs = get_fs();
        set_fs(get_ds());

        filp = filp_open(LOG_JANK_FS, O_WRONLY, 0);

        set_fs(oldfs);

        if (IS_ERR(filp)) {
            pr_err("jank_log: failed to open %s: %ld, msg:%s \n", LOG_JANK_FS, PTR_ERR(filp), msg);
            mutex_unlock(&janklogfile_mutex);
            return 0;
    		//write_jank = __write_jank_null;
        } else {
            write_jank = __write_jank_kernel;
        }
    }
    mutex_unlock(&janklogfile_mutex);

    return write_jank(tag,prio, msg);
}
/*
static int __write_jank_null(char* tag, char* msg)
{
    return -1;
}
*/
static int __write_jank_kernel(char* tag,int prio, char* msg)
{
    struct iovec vec[3];

    mm_segment_t oldfs;
    int ret;

    if (unlikely(!tag || !msg)) {
        pr_err("jank_log: invalid arguments\n");
        return -1;
    }

    if (IS_ERR(filp)) {
        pr_err("jank_log: file descriptor to %s is invalid: %ld\n", LOG_JANK_FS, PTR_ERR(filp));
        return -1;
    }

    // format: <priority:1><tag:N>\0<message:N>\0
    vec[0].iov_base = &prio;
    vec[0].iov_len  = 1;
    vec[1].iov_base = tag;
    vec[1].iov_len  = strlen(tag) + 1;
    vec[2].iov_base = msg;
    vec[2].iov_len  = strlen(msg) + 1;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_writev(filp, &vec[0], 3, &filp->f_pos);

    set_fs(oldfs);

    if (unlikely(ret < 0)) {
        pr_err("jank_log: failed to write %s\n", LOG_JANK_FS);
    }
    return ret;
}
#if defined (CONFIG_LOG_JANK)
int log_to_jank(char* tag, int prio, const char* fmt, ...)
{
    va_list args;
    struct my_work_struct   *pWork = NULL;

    raw_spin_lock(&janklog_spinlock);
    pWork = &janklog_work[nwork_index];
    if (0 == pWork->bWaiting)
    {
        pWork->bWaiting = 1;
        nwork_index++;
        if (nwork_index >= MAX_WORK_COUNT)
            nwork_index = 0;
    }
    else
    {
        pWork = NULL;
    }
    raw_spin_unlock(&janklog_spinlock);

    if (NULL == pWork)
    {
        pr_err("jank_log: janklog_workqueue is full, pr_jank failed.\n");
        return -1;
    }
    strncpy(pWork->tag, tag, sizeof(pWork->tag));
    pWork->tag[sizeof(pWork->tag) - 1] = '\0';
    pWork->nPrio = prio;
    va_start(args, fmt);
    vscnprintf(pWork->msg, sizeof(pWork->msg), fmt, args);
    va_end(args);

    queue_work(janklog_workqueue, &(pWork->writelog));

    return 0;
}
#endif

module_init(janklog_workqueue_init);
module_exit(janklog_workqueue_destory);


/*
 *  drivers/misc/inputhub/inputhub_route.c
 *  Sensor Hub Channel driver
 *
 *  Copyright (C) 2012 Huawei, Inc.
 *  Author: qindiwen <inputhub@huawei.com>
 *
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/sensorhub.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include "protocol.h"
#include "inputhub_route.h"
#include "inputhub_bridge.h"
#include "sensor_info.h"
#include "sensor_sys_info.h"
/* < DTS2014010800416 ligang/00183889 20140108 begin */
#include <linux/motionhub.h>
/*  DTS2014010800416 ligang/00183889 20140108 end > */
#include <linux/suspend.h>
#include <linux/fb.h>
#include <linux/timer.h>
#include <linux/rtc.h>
#include <linux/log_exception.h>
#include <linux/wakelock.h>
#include <linux/huawei/dsm_pub.h>

#define LINK_PACKAGE
#define ROUTE_BUFFER_MAX_SIZE (1024)
#ifdef TIMESTAMP_SIZE
#undef TIMESTAMP_SIZE
#define TIMESTAMP_SIZE (8)
#endif
#define LENGTH_SIZE sizeof(unsigned long)
#define HEAD_SIZE (LENGTH_SIZE+TIMESTAMP_SIZE)

typedef struct {
    int for_alignment;
    union {
        char effect_addr[sizeof(int)];
        int pkg_length;
    };
    int64_t timestamp;
} t_head;

static struct wake_lock wlock;
extern int loglevel_val;
static u16 apr_count = 2;
int (*api_mculog_process)(const char * buf,unsigned long length/*buf length*/)=0;
extern void iom3_log_store(int level, u32 ts_nsec, const char *text, u16 text_len);
extern int hall_first_report(bool enable);
extern struct dsm_client *shb_dclient;

struct iom3_log_work{
    void *log_p;
    struct delayed_work log_work;
};

struct inputhub_buffer_pos {
    char *pos;
    unsigned long buffer_size;
};

/*
 *Every route item can be used by one reader and one writer.
 */
struct inputhub_route_table {
    unsigned short port;
    struct inputhub_buffer_pos phead; //point to the head of buffer
    struct inputhub_buffer_pos pRead; //point to the read position of buffer
    struct inputhub_buffer_pos pWrite; //point to the write position of buffer
    wait_queue_head_t read_wait;//to block read when no data in buffer
    atomic_t data_ready;
    spinlock_t buffer_spin_lock;//for read write buffer
};

static struct inputhub_route_table  package_route_tbl[] =
{
    {ROUTE_SHB_PORT, {NULL,0},{NULL,0},{NULL,0}, __WAIT_QUEUE_HEAD_INITIALIZER(package_route_tbl[0].read_wait)},
    /* < DTS2014010800416 ligang/00183889 20140108 begin */
    {ROUTE_MOTION_PORT, {NULL,0},{NULL,0},{NULL,0},__WAIT_QUEUE_HEAD_INITIALIZER(package_route_tbl[1].read_wait)},
    /*  DTS2014010800416 ligang/00183889 20140108 end */
};

static void get_time_stamp(char* timestamp_buf,  unsigned int len)
{
   struct timeval tv;
   struct rtc_time tm;
   if(NULL == timestamp_buf)
   {
       pr_err("timestamp is NULL\n");
       return;
   }
   memset(&tv, 0, sizeof(struct timeval));
   memset(&tm, 0, sizeof(struct rtc_time));
   do_gettimeofday(&tv);
   tv.tv_sec -= sys_tz.tz_minuteswest * 60; /* 一分钟=60秒 */
   rtc_time_to_tm(tv.tv_sec, &tm);
   snprintf(timestamp_buf,len,"%04d%02d%02d%02d%02d%02d",tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

/*******************************************************************************************
Function:       inputhub_route_item
Description:    根据端口号从路由表package_route_tbl中获取路由信息的读写指针
Data Accessed:  package_route_tbl
Data Updated:   无
Input:          unsigned short port, 端口号，用于区分使用的哪个缓冲区
Output:         struct inputhub_route_table **route_item,
Return:         成功或失败信息
*******************************************************************************************/
static int inputhub_route_item(unsigned short port, struct inputhub_route_table **route_item)
{
    int i;
    for (i = 0; i < sizeof(package_route_tbl) / sizeof(package_route_tbl[0]); ++i) {
        if (port == package_route_tbl[i].port) {
            *route_item = &package_route_tbl[i];
            return 0;
        }
    }

    hwlog_err("unknown port: %d in %s.\n", port, __func__);
    return -EINVAL;

}

/*******************************************************************************************
Function:       inputhub_route_open
Description:    根据端口号创建本端口号的读写缓冲区并初始化读写指针
Data Accessed:  package_route_tbl
Data Updated:   package_route_tbl
Input:          unsigned short port, 端口号，用于区分创建哪个端口的缓冲区
Output:         无
Return:         成功或失败信息
*******************************************************************************************/
int inputhub_route_open(unsigned short port)
{
    int ret;
    char *pos;
    struct inputhub_route_table *route_item;

    hwlog_info("%s\n", __func__);
    ret = inputhub_route_item(port, &route_item);
    if (ret < 0) {
        return -EINVAL;
    }

    if (route_item->phead.pos) {
        hwlog_err("port:%d was already opened in %s.\n", port, __func__);
        return -EINVAL;
    }

    pos = kzalloc(ROUTE_BUFFER_MAX_SIZE, GFP_KERNEL);
    if (!pos) {
        return -ENOMEM;
    }
    route_item->phead.pos = pos;
    route_item->pWrite.pos = pos;
    route_item->pRead.pos = pos;
    route_item->phead.buffer_size = ROUTE_BUFFER_MAX_SIZE;
    route_item->pWrite.buffer_size = ROUTE_BUFFER_MAX_SIZE;
    route_item->pRead.buffer_size = 0;

    return 0;
}
EXPORT_SYMBOL_GPL(inputhub_route_open);

/*******************************************************************************************
Function:       inputhub_route_close
Description:    根据端口号释放本端口号申请的缓冲区的动态内存并清除读写指针信息
Data Accessed:  package_route_tbl
Data Updated:   package_route_tbl
Input:          unsigned short port, 端口号，用于区分创建哪个端口的缓冲区
Output:         无
Return:         无
*******************************************************************************************/
void inputhub_route_close(unsigned short port)
{
    int ret;
    struct inputhub_route_table *route_item;

    hwlog_info( "%s\n", __func__);
    ret = inputhub_route_item(port, &route_item);
    if (ret < 0) {
        return ;
    }

    if (route_item->phead.pos) {
        kfree(route_item->phead.pos);
    }
    route_item->phead.pos =NULL;
    route_item->pWrite.pos =NULL;
    route_item->pRead.pos =NULL;
}
EXPORT_SYMBOL_GPL(inputhub_route_close);

/*******************************************************************************************
Function:       inputhub_route_read
Description:    根据端口号从该端口号的读写缓冲区中读取事件拷贝到指定的用户空间的缓冲区中
Data Accessed:  package_route_tbl
Data Updated:   package_route_tbl
Input:          unsigned short port, 端口号，用于区分读取哪个端口的缓冲区中的数据，char __user *buf, size_t count，HAL层读缓冲区的地址和长度信息
Output:         无
Return:         实际读取数据的长度
*******************************************************************************************/
ssize_t inputhub_route_read(unsigned short port, char __user *buf, size_t count)
{
    struct inputhub_route_table *route_item;
    struct inputhub_buffer_pos reader;
    char *buffer_end;
    int full_pkg_length;
    int tail_half_len;
    unsigned long flags = 0;

    if (inputhub_route_item(port, &route_item) != 0) {
        hwlog_err("inputhub_route_item failed in %s\n", __func__);
        return 0;
    }

    buffer_end = route_item->phead.pos + route_item->phead.buffer_size;

    spin_lock_irqsave(&route_item->buffer_spin_lock, flags);
    reader = route_item->pRead;
    spin_unlock_irqrestore(&route_item->buffer_spin_lock, flags);

    if (reader.buffer_size > ROUTE_BUFFER_MAX_SIZE) {
        hwlog_err("error reader.buffer_size = %d in port %d!\n", (int)reader.buffer_size, (int)port);
        goto clean_buffer;
    }

    if (0 == reader.buffer_size) {
        //block here
        atomic_set(&route_item->data_ready, 0);
        wait_event_interruptible(route_item->read_wait, atomic_read(&route_item->data_ready));
        hwlog_debug("unblocked from %s in port %d\n", __func__, (int)port);
    }

    spin_lock_irqsave(&route_item->buffer_spin_lock, flags);
    reader = route_item->pRead;
    spin_unlock_irqrestore(&route_item->buffer_spin_lock, flags);

    //woke up by signal
    if (0 == reader.buffer_size) {
        return 0;
    }

    if (buffer_end - reader.pos >= LENGTH_SIZE) {
        full_pkg_length = *((int *)reader.pos);
        reader.pos += LENGTH_SIZE;
        if (reader.pos == buffer_end) {
            reader.pos = route_item->phead.pos;
        }
    } else {
        tail_half_len = buffer_end - reader.pos;
        memcpy(&full_pkg_length, reader.pos, tail_half_len);
        memcpy((char *)&full_pkg_length + tail_half_len, route_item->phead.pos, LENGTH_SIZE - tail_half_len);
        reader.pos = route_item->phead.pos + (LENGTH_SIZE - tail_half_len);
    }

    if (full_pkg_length + LENGTH_SIZE > reader.buffer_size || full_pkg_length > count) {
        hwlog_err("full_pkg_length = %d is too large in port %d!\n", full_pkg_length, (int)port);
        goto clean_buffer;
    }

    if (buffer_end - reader.pos >= full_pkg_length) {
        if (0 == copy_to_user(buf, reader.pos, full_pkg_length)) {
            reader.pos += full_pkg_length;
            if (reader.pos == buffer_end) {
                reader.pos = route_item->phead.pos;
            }
        } else {
            hwlog_err( "copy to user failed\n");
            return 0;
        }
    } else {
        tail_half_len = buffer_end - reader.pos;
        if ((0 == copy_to_user(buf, reader.pos, tail_half_len)) &&
            (0 == copy_to_user(buf + tail_half_len, route_item->phead.pos, (full_pkg_length - tail_half_len)))) {
            reader.pos = route_item->phead.pos + (full_pkg_length - tail_half_len);
        } else {
            hwlog_err( "copy to user failed\n");
            return 0;
        }
    }
    spin_lock_irqsave(&route_item->buffer_spin_lock, flags);
    route_item->pRead.pos = reader.pos;
    route_item->pRead.buffer_size -= (full_pkg_length + LENGTH_SIZE);
    route_item->pWrite.buffer_size += (full_pkg_length + LENGTH_SIZE);
    spin_unlock_irqrestore(&route_item->buffer_spin_lock, flags);
    hwlog_debug("read a package with size of %d in port %d\n", full_pkg_length, (int)port);

    return full_pkg_length;

clean_buffer:
    hwlog_err( "now we will clear the receive buffer in port %d!\n", (int)port);
    spin_lock_irqsave(&route_item->buffer_spin_lock, flags);
    route_item->pRead.pos = route_item->pWrite.pos;
    route_item->pWrite.buffer_size = ROUTE_BUFFER_MAX_SIZE;
    route_item->pRead.buffer_size = 0;
    spin_unlock_irqrestore(&route_item->buffer_spin_lock, flags);
    return 0;
}
EXPORT_SYMBOL_GPL(inputhub_route_read);

static int64_t getTimestamp(void) {
    struct timespec ts;

    ktime_get_ts(&ts);
    //timevalToNano
    return ts.tv_sec*1000000000LL + ts.tv_nsec;
}

/*******************************************************************************************
Function:       write_to_fifo
Description:    将以buf为起始地址的count字节数据写到以pwriter指向的写位置为起始地址的地方，当写到读写缓冲区的末尾时返回到读写缓冲区的起始地址继续写
Data Accessed:  package_route_tbl
Data Updated:   package_route_tbl
Input:          struct inputhub_buffer_pos *pwriter, char *const buffer_begin, char *const buffer_end, char *buf, int count，数据拷贝时的dst空间和src空间信息
Output:         无
Return:         无
*******************************************************************************************/
static inline void write_to_fifo(struct inputhub_buffer_pos *pwriter, char *const buffer_begin, char *const buffer_end, char *buf, int count)
{
    int cur_to_tail_len = buffer_end - pwriter->pos;

    if (cur_to_tail_len >= count) {
        memcpy(pwriter->pos, buf, count);
        pwriter->pos += count;
        if (buffer_end == pwriter->pos) {
            pwriter->pos = buffer_begin;
        }
    } else {
        memcpy(pwriter->pos, buf, cur_to_tail_len);
        memcpy(buffer_begin, buf + cur_to_tail_len, count - cur_to_tail_len);
        pwriter->pos = buffer_begin + (count - cur_to_tail_len);
    }
}

int ap_hall_report(int value)
{
    struct sensor_data event;
    event.type = TAG_HALL;
    event.length = sizeof(event.value[0]);
    event.value[0] = (int)value;
    return inputhub_route_write(ROUTE_SHB_PORT, (char *)&event, event.length + OFFSET_OF_END_MEM(struct sensor_data, length));
}

/*******************************************************************************************
Function:       inputhub_route_write
Description:    将以buf为起始地址的count字节数据写到port端口对应的事件缓冲区中
Data Accessed:  package_route_tbl
Data Updated:   package_route_tbl
Input:          unsigned short port用于区分往哪个缓冲区中写数据，char  *buf, size_t count，是写数据时src的地址和长度信息
Output:         无
Return:         实际写入到缓冲区中的数据的长度
*******************************************************************************************/
ssize_t inputhub_route_write(unsigned short port, char  *buf, size_t count)
{
    struct inputhub_route_table *route_item;
    struct inputhub_buffer_pos writer;
    char *buffer_begin, *buffer_end;
    t_head header;
    unsigned long flags = 0;

    if (inputhub_route_item(port, &route_item) != 0) {
        hwlog_err("inputhub_route_item failed in %s port = %d!\n", __func__, (int)port);
        return 0;
    }

    spin_lock_irqsave(&route_item->buffer_spin_lock, flags);
    writer = route_item->pWrite;
    spin_unlock_irqrestore(&route_item->buffer_spin_lock, flags);

    if (writer.buffer_size < count + HEAD_SIZE) {
        hwlog_debug("There is no space to write in the buffer, infor: remain_size = %d, need_size = %d\n", (int)(writer.buffer_size), (int)(count + HEAD_SIZE));
        return 0;
    }

    buffer_begin = route_item->phead.pos;
    buffer_end = route_item->phead.pos + route_item->phead.buffer_size;
    header.pkg_length = count + sizeof(int64_t);
    header.timestamp = getTimestamp();
    write_to_fifo(&writer, buffer_begin, buffer_end, header.effect_addr, HEAD_SIZE);
    write_to_fifo(&writer, buffer_begin, buffer_end, buf, count);

    spin_lock_irqsave(&route_item->buffer_spin_lock, flags);
    route_item->pWrite.pos = writer.pos;
    route_item->pWrite.buffer_size -= (count + HEAD_SIZE);
    route_item->pRead.buffer_size += (count + HEAD_SIZE);
    spin_unlock_irqrestore(&route_item->buffer_spin_lock, flags);
    hwlog_debug("write a package to fifo with size of %d in port %d\n", (int)(count + HEAD_SIZE), (int)port);
    atomic_set(&route_item->data_ready, 1);
    wake_up_interruptible(&route_item->read_wait);

    return (count + HEAD_SIZE);
}
EXPORT_SYMBOL_GPL(inputhub_route_write);

/**begin**/
#ifdef LINK_PACKAGE
#define MAX_SEND_LEN (32)
struct link_package {
    int partial_order;
    char link_buffer[MAX_PKT_LENGTH_AP];
    int total_pkg_len;
    int offset;
};
const pkt_header_t *pack(const char *buf, unsigned long length)
{
    const pkt_header_t *head = (const pkt_header_t *)buf;
    static struct link_package linker = {-1};//init partial_order to -1 to aviod lost first pkt

    //try to judge which pkt it is
    if ((TAG_BEGIN <= head->tag && head->tag < TAG_END) && (head->length <= (MAX_PKT_LENGTH_AP - sizeof(pkt_header_t)))) {
        linker.total_pkg_len = head->length + OFFSET_OF_END_MEM(pkt_header_t, length);
        if (linker.total_pkg_len > (int)length) {//need link other partial packages
            linker.partial_order = head->partial_order;//init partial_order
            if (length <= sizeof(linker.link_buffer)) {
                memcpy(linker.link_buffer, buf, length);//save first partial package
                linker.offset = length;
            } else {
                goto error;
            }
            goto receive_next;//receive next partial package
        } else {
            return head;//full pkt
        }
    } else if (TAG_END == head->tag) {//check if partial_order effective
        pkt_part_header_t *partial = (pkt_part_header_t *)buf;
        if (partial->partial_order == (uint8_t)(linker.partial_order + 1)) {//type must keep same with partial->partial_order, because integer promote
            int partial_pkt_data_length = length - sizeof(pkt_part_header_t);
            if (linker.offset + partial_pkt_data_length <= sizeof(linker.link_buffer)) {
                ++linker.partial_order;
                memcpy(linker.link_buffer + linker.offset, buf + sizeof(pkt_part_header_t), partial_pkt_data_length);
                linker.offset += partial_pkt_data_length;
                if (linker.offset >= linker.total_pkg_len) {//link finished
                    return (pkt_header_t *)linker.link_buffer;
                } else {
                    goto receive_next;//receive next partial package
                }
            }
        }
    }

error://clear linker info when error
    linker.partial_order = -1;
    linker.total_pkg_len = 0;
    linker.offset = 0;
receive_next:
    return NULL;
}

int unpack(const void *buf, unsigned long length)
{
    int ret = 0;
    static int partial_order = 0;

    ((pkt_header_t *)buf)->partial_order = partial_order++;//inc partial_order in header
    if (length <= MAX_SEND_LEN) {
        return inputhub_mcu_send((const char *)buf, length);
    } else {
        char send_partial_buf[MAX_SEND_LEN];
        int send_cnt = 0;

        //send head
        if ((ret = inputhub_mcu_send((const char *)buf, MAX_SEND_LEN)) != 0) {
            return ret;
        }

        ((pkt_part_header_t *)send_partial_buf)->tag = TAG_END;
        for (send_cnt = MAX_SEND_LEN; send_cnt < (int)length; send_cnt += (MAX_SEND_LEN - sizeof(pkt_part_header_t))) {
            ((pkt_part_header_t *)send_partial_buf)->partial_order = partial_order++;//inc partial_order in partial pkt
            memcpy(send_partial_buf + sizeof(pkt_part_header_t), (const char *)buf + send_cnt, MAX_SEND_LEN - sizeof(pkt_part_header_t));
            if ((ret = inputhub_mcu_send(send_partial_buf, MAX_SEND_LEN)) != 0) {
                return ret;
            }
        }
    }

    return 0;
}
#endif
/**end**/
static uint16_t tranid = 0;
/*******************************************************************************************
Function:       inputhub_mcu_write_cmd
Description:    将以buf为起始地址的length字节的命令数据发送给MCU
Data Accessed:  无
Data Updated:   无
Input:          const void *buf, unsigned long length发送数据的地址和长度信息
Output:         无
Return:         成功或者失败信息
*******************************************************************************************/
static struct mutex mutex_write_cmd;
static int inputhub_mcu_write_cmd(const void *buf, unsigned long length)
{
    int ret;

    if (length > MAX_PKT_LENGTH) {
        hwlog_err("length = %d is too large in %s\n", (int)length, __func__);
        return -EINVAL;
    }
    mutex_lock(&mutex_write_cmd);
    ((pkt_header_t *)buf)->tranid = tranid++;
#ifdef LINK_PACKAGE//在包较大分开发送时这里必须加锁，否则MCU侧组包会失败
    ret = unpack(buf, length);
#else
    ret = inputhub_mcu_send((const char *)buf, length);
#endif
    mutex_unlock(&mutex_write_cmd);

    if ((0 == ret) && (TAG_SENSOR_BEGIN <= ((pkt_header_t *)buf)->tag && ((pkt_header_t *)buf)->tag < TAG_SENSOR_END)) {
        update_sensor_info(((const pkt_header_t *)buf));
    }

    return ret;
}

/* < DTS2014010800416 ligang/00183889 20140108 begin */
struct motions_cmd_map {
    int         mhb_ioctl_app_cmd;
    int         motion_type;
    int         tag;
    obj_cmd_t cmd;
};
static const struct motions_cmd_map motions_cmd_map_tab[] = {
    {MHB_IOCTL_MOTION_START, -1,  TAG_MOTION, CMD_MOTION_OPEN_REQ},
    {MHB_IOCTL_MOTION_STOP,   -1,  TAG_MOTION, CMD_MOTION_CLOSE_REQ},
    {MHB_IOCTL_MOTION_ATTR_START,-1, TAG_MOTION, CMD_MOTION_ATTR_ENABLE_REQ},
    {MHB_IOCTL_MOTION_ATTR_STOP,  -1, TAG_MOTION, CMD_MOTION_ATTR_DISABLE_REQ},
    {MHB_IOCTL_MOTION_INTERVAL_SET, -1, TAG_MOTION, CMD_MOTION_INTERVAL_REQ},
};
/*******************************************************************************************
Function:       is_motion_data_report
Description:   judge whether it is data of motion
Data Accessed:  no
Data Updated:    no
Input:          const pkt_header_t *head, packake head of data from MCU
Output:        no
Return:        true if motiion data, esle false
*******************************************************************************************/
static bool is_motion_data_report(const pkt_header_t *head)
{
    //all sensors report data with command CMD_PRIVATE
    return (head->tag == TAG_MOTION) && (CMD_MOTION_REPORT_REQ== head->cmd);
}

/*******************************************************************************************
Function:       send_motion_cmd
Description:   translate command and parameter base on protocol, then tranfer to MCU
Data Accessed:  motions_cmd_map_tab
Data Updated:   no
Input:          unsigned int cmd, unsigned long arg
                   command code and parameter
Output:        void
Return:        result of function, 0 successful, -EINVAL failed
*******************************************************************************************/
int send_motion_cmd(unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    int argvalue = 0;
    int i;

    for (i = 0; i < sizeof(motions_cmd_map_tab) / sizeof(motions_cmd_map_tab[0]); ++i) {
        if (motions_cmd_map_tab[i].mhb_ioctl_app_cmd == cmd) {
            break;
        }
    }

    if (sizeof(motions_cmd_map_tab) / sizeof(motions_cmd_map_tab[0]) == i) {
        hwlog_err("send_motion_cmd unknown cmd %d in parse_motion_cmd!\n", cmd);
        return -EFAULT;
    }

    if (copy_from_user(&argvalue, argp, sizeof(argvalue))) {
        return -EFAULT;
    }

    if (CMD_MOTION_OPEN_REQ == motions_cmd_map_tab[i].cmd || CMD_MOTION_CLOSE_REQ == motions_cmd_map_tab[i].cmd) {
        pkt_motion_open_req_t pkt_motion;

        pkt_motion.motion_type = argvalue;

        pkt_motion.hd.tag = motions_cmd_map_tab[i].tag;
        pkt_motion.hd.cmd = motions_cmd_map_tab[i].cmd;
        pkt_motion.hd.resp = NO_RESP;
        pkt_motion.hd.length = sizeof(pkt_motion.motion_type);

        hwlog_info("send_motion_cmd  cmd:%d  motion: %d !", motions_cmd_map_tab[i].cmd, pkt_motion.motion_type);
        return inputhub_mcu_write_cmd(&pkt_motion, sizeof(pkt_motion));
    } else if (CMD_MOTION_ATTR_ENABLE_REQ == motions_cmd_map_tab[i].cmd ||CMD_MOTION_ATTR_DISABLE_REQ == motions_cmd_map_tab[i].cmd) {
        hwlog_err("send_motion_cmd  cmd:%d   !", motions_cmd_map_tab[i].cmd);
    } else if (CMD_MOTION_INTERVAL_REQ == motions_cmd_map_tab[i].cmd) {
        hwlog_err("send_motion_cmd  cmd:%d   !", motions_cmd_map_tab[i].cmd);
    } else {
        hwlog_err("send_motion_cmd unknown cmd!\n");
        return -EINVAL;
    }

    return 0;
}
/*  DTS2014010800416 ligang/00183889 20140108 end > */

int send_apr_log(void)
{
    char cmd[256];
    char time_buf[16];
    int ret = 0;

    get_time_stamp(time_buf,16);
    snprintf(cmd, 256, "%s%s%s",
        "archive -i /data/android_logs/kmsgcat-log -i /data/android_logs/kmsgcat-log.1 -i /data/android_logs/applogcat-log "
                 "-i /data/android_logs/applogcat-log.1 -i /data/rdr/dump_00.bin -i /data/rdr/dump_01.bin -i /data/rdr/dump_02.bin -o ",
                 time_buf, "_sensorhubErr -z zip");
    ret = log_to_exception("sensorhub", cmd);
    if(ret < 0 )
        hwlog_err("logexception sysfs err.\n");
    return ret;
}
void iom3_log_handle(struct work_struct  *work)
{
    struct iom3_log_work *iom3_log = container_of(work, struct iom3_log_work, log_work.work);
    pkt_log_report_req_t *pkt = (pkt_log_report_req_t *)iom3_log->log_p;
    if(api_mculog_process){
        api_mculog_process((char*)pkt,pkt->log_length);
    }
    if(pkt->level <= loglevel_val){
        iom3_log_store(pkt->level, pkt->log_time, pkt->log, pkt->log_length);
    }
    if(pkt->level == LOG_LEVEL_FATAL){
        hwlog_err("[iom3][%10d]%s",pkt->log_time,pkt->log);
        if(apr_count > 0)
        {
            if(!dsm_client_ocuppy(shb_dclient))
            {
                dsm_client_record(shb_dclient, "[iom3][%10d]%s", pkt->log_time, pkt->log);
                dsm_client_notify(shb_dclient, DSM_ERR_SENSORHUB_LOG_FATAL);
            }
            apr_count--;
        }
        //printk(KERN_EMERG "[iom3][%10d]%s",pkt->log_time,pkt->log);//???how to convert to hwlog level???
    }
    else if(pkt->level == LOG_LEVEL_ERR){
        hwlog_err("[iom3][%10d]%s",pkt->log_time,pkt->log);
    }
    else if(pkt->level == LOG_LEVEL_WARNING){
        hwlog_warn("[iom3][%10d]%s",pkt->log_time,pkt->log);
    }
    else if(pkt->level == LOG_LEVEL_INFO){
        hwlog_info("[iom3][%10d]%s",pkt->log_time,pkt->log);
    }
    else if(pkt->level == LOG_LEVEL_DEBUG){
        hwlog_debug("[iom3][%10d]%s",pkt->log_time,pkt->log);
    }
    kfree(iom3_log->log_p);
    kfree(iom3_log);
}



struct sensors_cmd_map {
    int         shb_ioctl_app_cmd;
    int         hal_sensor_type;
    int         tag;
    obj_cmd_t   cmd;
};
/* < DTS2014011302734 fumahong/00186348 20140126 begin
 * add sensors of android4.4(MAGNETIC_FIELD_UNCALIBRATED~GEOMAGNETIC_ROTATION_VECTOR) */
static const struct sensors_cmd_map sensors_cmd_map_tab[] = {
    {SHB_IOCTL_APP_ENABLE_SENSOR,               -1,                                 -1,                     CMD_CMN_OPEN_REQ},
    {SHB_IOCTL_APP_DISABLE_SENSOR,              -1,                                 -1,                     CMD_CMN_CLOSE_REQ},
    {SHB_IOCTL_APP_DELAY_ACCEL,                 SENSORHUB_TYPE_ACCELEROMETER,       TAG_ACCEL,              CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_LIGHT,                 SENSORHUB_TYPE_LIGHT,               TAG_ALS,                CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_PROXI,                 SENSORHUB_TYPE_PROXIMITY,           TAG_PS,                 CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_GYRO,                  SENSORHUB_TYPE_GYROSCOPE,           TAG_GYRO,               CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_GRAVITY,               SENSORHUB_TYPE_GRAVITY,             TAG_GRAVITY,            CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_MAGNETIC,              SENSORHUB_TYPE_MAGNETIC,            TAG_MAG,                CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_ROTATESCREEN,          SENSORHUB_TYPE_ROTATESCREEN,        TAG_SCREEN_ROTATE,      CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_LINEARACCELERATE,      SENSORHUB_TYPE_LINEARACCELERATE,    TAG_LINEAR_ACCEL,       CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_ORIENTATION,           SENSORHUB_TYPE_ORIENTATION,         TAG_ORIENTATION,        CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_ROTATEVECTOR,          SENSORHUB_TYPE_ROTATEVECTOR,        TAG_ROTATION_VECTORS,   CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_PRESSURE,              SENSORHUB_TYPE_PRESSURE,            TAG_PRESSURE,           CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_TEMPERATURE,           SENSORHUB_TYPE_TEMPERATURE,         TAG_TEMP,               CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_RELATIVE_HUMIDITY,     SENSORHUB_TYPE_RELATIVE_HUMIDITY,   TAG_HUMIDITY,           CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_AMBIENT_TEMPERATURE,   SENSORHUB_TYPE_AMBIENT_TEMPERATURE, TAG_AMBIENT_TEMP,       CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_MCU_LABC,              SENSORHUB_TYPE_MCU_LABC,            TAG_LABC,               CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_HALL,                  SENSORHUB_TYPE_HALL,                TAG_HALL,               CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_MAGNETIC_FIELD_UNCALIBRATED,   SENSORHUB_TYPE_MAGNETIC_FIELD_UNCALIBRATED,     TAG_MAG_UNCALIBRATED,       CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_GAME_ROTATION_VECTOR,          SENSORHUB_TYPE_GAME_ROTATION_VECTOR,            TAG_GAME_RV,                CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_GYROSCOPE_UNCALIBRATED,        SENSORHUB_TYPE_GYROSCOPE_UNCALIBRATED,          TAG_GYRO_UNCALIBRATED,      CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_SIGNIFICANT_MOTION,            SENSORHUB_TYPE_SIGNIFICANT_MOTION,              TAG_SIGNIFICANT_MOTION,     CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_STEP_DETECTOR,                 SENSORHUB_TYPE_STEP_DETECTOR,                   TAG_STEP_DETECTOR,          CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_STEP_COUNTER,                  SENSORHUB_TYPE_STEP_COUNTER,                    TAG_STEP_COUNTER,           CMD_CMN_INTERVAL_REQ},
    {SHB_IOCTL_APP_DELAY_GEOMAGNETIC_ROTATION_VECTOR,   SENSORHUB_TYPE_GEOMAGNETIC_ROTATION_VECTOR,     TAG_GEOMAGNETIC_RV,         CMD_CMN_INTERVAL_REQ}
};
/*   DTS2014011302734 fumahong/00186348 20140126 end > */

int inputhub_sensor_enable(int tag, bool enable)
{
    if (TAG_HALL == tag)
        return hall_first_report(enable);

    if (TAG_SENSOR_BEGIN <= tag && tag < TAG_SENSOR_END) {
        pkt_header_t pkt = {
            .tag = tag,
            .cmd = enable ? CMD_CMN_OPEN_REQ : CMD_CMN_CLOSE_REQ,
            .resp = NO_RESP,
            .length = 0
        };


        hwlog_info("%s sensor %d!\n", enable ? "open" : "close", pkt.tag);
        return inputhub_mcu_write_cmd(&pkt, sizeof(pkt));
    } else {
        hwlog_err("unknown sensor type in %s\n", __func__);
        return -EINVAL;
    }
}

int inputhub_sensor_setdelay(int tag, int delay_ms)
{
    if (TAG_HALL == tag)
        return 0;

    if (TAG_SENSOR_BEGIN <= tag && tag < TAG_SENSOR_END) {
        pkt_cmn_interval_req_t pkt;
        pkt.hd.tag = tag;
        pkt.hd.cmd = CMD_CMN_INTERVAL_REQ;
        pkt.hd.resp = NO_RESP;
        pkt.hd.length = sizeof(pkt.interval);
        pkt.interval = delay_ms;

        hwlog_info("set sensor %d delay %d ms!\n", pkt.hd.tag, pkt.interval);
        return inputhub_mcu_write_cmd(&pkt, sizeof(pkt));
    } else {
        hwlog_err("unknown sensor type in %s\n", __func__);
        return -EINVAL;
    }
}

/*******************************************************************************************
Function:       send_sensor_cmd
Description:    将命令码和参数信息转换成通信协议格式发送给MCU
Data Accessed:  sensors_cmd_map_tab
Data Updated:   无
Input:          unsigned int cmd, unsigned long arg，命令码和参数信息
Output:         无
Return:         成功或者失败信息
*******************************************************************************************/
static int send_sensor_cmd(unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    int argvalue = 0;//values of TLV are defined in type of int in communication protocols
    int i;

    for (i = 0; i < sizeof(sensors_cmd_map_tab) / sizeof(sensors_cmd_map_tab[0]); ++i) {
        if (sensors_cmd_map_tab[i].shb_ioctl_app_cmd == cmd) {
            break;
        }
    }

    if (sizeof(sensors_cmd_map_tab) / sizeof(sensors_cmd_map_tab[0]) == i) {
        hwlog_err("unknown cmd %d in %s!\n", cmd, __func__);
        return -EINVAL;
    }

    if (copy_from_user(&argvalue, argp, sizeof(argvalue))) {
        return -EFAULT;
    }

    if (CMD_CMN_OPEN_REQ == sensors_cmd_map_tab[i].cmd || CMD_CMN_CLOSE_REQ == sensors_cmd_map_tab[i].cmd) {
        int j;
        for (j = 0; j < sizeof(sensors_cmd_map_tab) / sizeof(sensors_cmd_map_tab[0]); ++j) {
            if (sensors_cmd_map_tab[j].hal_sensor_type == argvalue) {
                break;
            }
        }
        if (sizeof(sensors_cmd_map_tab) / sizeof(sensors_cmd_map_tab[0]) == j) {
            hwlog_err("unknown hal_sensor_type %d in %s!\n", argvalue, __func__);
            return -EINVAL;
        }

        return inputhub_sensor_enable(sensors_cmd_map_tab[j].tag, CMD_CMN_OPEN_REQ == sensors_cmd_map_tab[i].cmd);
    } else if (CMD_CMN_INTERVAL_REQ == sensors_cmd_map_tab[i].cmd) {
        return inputhub_sensor_setdelay(sensors_cmd_map_tab[i].tag, argvalue);
    } else {
        hwlog_err("unknown cmd!\n");
        return -EINVAL;
    }

    return 0;
}
struct type_record {
    const pkt_header_t *pkt_info;
    struct read_info *rd;
    struct completion resp_complete;//用于同步
    struct mutex lock_mutex;//用于互斥
    spinlock_t lock_spin;//用于保护临界区访问
};

static struct type_record type_record;

/*******************************************************************************************
Function:       cmd_match
Description:    判断resp命令是否是req命令的回复命令
Data Accessed:  无
Data Updated:   无
Input:          int req发送命令 int resp回复命令
Output:         无
Return:         若resp命令是req命令的回复命令返回true，否则返回false
*******************************************************************************************/
static bool cmd_match(int req, int resp)
{
    return (req + 1) == resp;
}

/*******************************************************************************************
Function:       write_customize_cmd
Description:    器件参数配置及查询器件信息
Data Accessed:  无
Data Updated:   无
Input:          const struct write_info *wr, struct read_info *rd，参数wr指定向MCU发送的命令数据，参数rd用于向需要返回数据的命令返回数据，不需要返回时要指定为NULL
Output:         无
Return:         成功或者失败信息: 0->成功, -ETIME->mcu resp timeou
*******************************************************************************************/
int write_customize_cmd(const struct write_info *wr, struct read_info *rd)
{
    char buf[MAX_PKT_LENGTH];
    int ret = 0;

    if (NULL == wr) {
        hwlog_err("NULL pointer in %s\n", __func__);
        return -EINVAL;
    }
    //[TAG_BEGIN, TAG_END)
    if (wr->tag < TAG_BEGIN || wr->tag >= TAG_END) {
        hwlog_err("tag = %d error in %s\n", wr->tag, __func__);
        return -EINVAL;
    }
    if (wr->wr_len + sizeof(pkt_header_t) > MAX_PKT_LENGTH) {
        hwlog_err("-----------> wr_len = %d is too large in %s\n", wr->wr_len, __func__);
        return -EINVAL;
    }

    //转换成MCU需要的协议格式
    ((pkt_header_t *)buf)->tag = wr->tag;
    ((pkt_header_t *)buf)->cmd = wr->cmd;
    ((pkt_header_t *)buf)->resp = ((rd != NULL) ? (RESP) : (NO_RESP));
    ((pkt_header_t *)buf)->length = wr->wr_len;
    if (wr->wr_buf != NULL) {
        memcpy(buf + sizeof(pkt_header_t), wr->wr_buf, wr->wr_len);
    }
    if (NULL == rd) {//tag cmd need not resp
        return inputhub_mcu_write_cmd(buf, sizeof(pkt_header_t) + wr->wr_len);
    } else {//tag cmd need resp
        unsigned long flags = 0;
        mutex_lock(&type_record.lock_mutex);
        spin_lock_irqsave(&type_record.lock_spin, flags);
        type_record.pkt_info = ((pkt_header_t *)buf);
        type_record.rd = rd;
        spin_unlock_irqrestore(&type_record.lock_spin, flags);

        //send data to mcu
        INIT_COMPLETION(type_record.resp_complete);
        if ((ret = inputhub_mcu_write_cmd(buf, sizeof(pkt_header_t) + wr->wr_len)) != 0) {
            hwlog_err("send cmd to mcu failed in %s\n", __func__);
            goto clear_info;
        }

        //wait for resp or timeout
        if (0 == wait_for_completion_timeout(&type_record.resp_complete, msecs_to_jiffies(2000))) {
            hwlog_err("completion timeout in %s!\n", __func__);
            ret =  -ETIME;
            if(!dsm_client_ocuppy(shb_dclient)){
                 dsm_client_record(shb_dclient, "Command tag - %d, cmd - %d\n",wr->tag, wr->cmd);
                 dsm_client_notify(shb_dclient, DSM_ERR_SENSORHUB_IPC_TIMEOUT);
            }
        }

clear_info:
        //clear infor
        spin_lock_irqsave(&type_record.lock_spin, flags);
        type_record.pkt_info = NULL;
        type_record.rd = NULL;
        spin_unlock_irqrestore(&type_record.lock_spin, flags);
        mutex_unlock(&type_record.lock_mutex);
    }

    return ret;
}

/*******************************************************************************************
Function:       is_sensor_data_report
Description:    根据从MCU接收到的数据包中的tag和cmd信息判断是不是传感器数据
Data Accessed:  无
Data Updated:   无
Input:          const pkt_header_t *head，从MCU接收到的数据的包头信息
Output:         无
Return:         当是传感器的数据时返回true，否则返回false
*******************************************************************************************/
static bool is_sensor_data_report(const pkt_header_t *head)
{
    //all sensors report data with command CMD_PRIVATE
    return (TAG_SENSOR_BEGIN <= head->tag && head->tag < TAG_SENSOR_END) && (CMD_PRIVATE == head->cmd);
}


static bool is_log_data_report(const pkt_header_t *head)
{
    //all sensors report data with command CMD_PRIVATE
    return (TAG_LOG == head->tag) && (CMD_LOG_REPORT_REQ == head->cmd);
}



/*******************************************************************************************
Function:       is_requirement_resp
Description:    根据从MCU接收到的数据包中的tag和cmd信息判断是不是MCU的回复信息
Data Accessed:  无
Data Updated:   无
Input:          const pkt_header_t *head，从MCU接收到的数据的包头信息
Output:         无
Return:         当是回复信息时返回true，否则返回false
*******************************************************************************************/
static bool is_requirement_resp(const pkt_header_t *head)//命令码是偶数必定是返回数据
{
    return (0 == (head->cmd & 1));
}

/*******************************************************************************************
Function:       report_resp_data
Description:    将从MCU接收到的命令回复数据传回到回复数据的接收者
Data Accessed:  无
Data Updated:   无
Input:          const pkt_header_resp_t *head，从MCU接收到的回复数据的包头信息
Output:         无
Return:         成功或者失败信息
*******************************************************************************************/
static int report_resp_data(const pkt_header_resp_t *head)
{
    int ret = 0;
    unsigned long flags = 0;

    spin_lock_irqsave(&type_record.lock_spin, flags);
    if (type_record.rd != NULL && type_record.pkt_info != NULL//check record info
        && (cmd_match(type_record.pkt_info->cmd, head->cmd))
        && (type_record.pkt_info->tranid == head->tranid)) {//rcv resp from mcu
        if (head->length <= (MAX_PKT_LENGTH + sizeof(head->errno))) {//data length ok
            type_record.rd->errno = head->errno;//fill errno to app
            type_record.rd->data_length = (head->length - sizeof(head->errno));//fill data_length to app, data_length means data lenght below
            memcpy(type_record.rd->data, (char *)head + sizeof(pkt_header_resp_t), type_record.rd->data_length);//fill resp data to app
        } else {//resp data too large
            type_record.rd->errno = -EINVAL;
            type_record.rd->data_length = 0;
            hwlog_err("data too large from mcu in %s\n", __func__);
        }
        complete(&type_record.resp_complete);
    }
    spin_unlock_irqrestore(&type_record.lock_spin, flags);

    return ret;
}

struct mcu_notifier_work
{
    struct work_struct worker;
    void *data;
};

struct mcu_notifier_node
{
    int tag;
    int cmd;
    int (*notify)(const pkt_header_t *data);
    struct list_head entry;
};

struct mcu_notifier
{
    struct list_head head;
    spinlock_t lock;
    struct workqueue_struct *mcu_notifier_wq;
};

static struct mcu_notifier mcu_notifier = {LIST_HEAD_INIT(mcu_notifier.head)};
static void init_mcu_notifier_list(void)
{
    INIT_LIST_HEAD(&mcu_notifier.head);
    spin_lock_init(&mcu_notifier.lock);
    mcu_notifier.mcu_notifier_wq = create_singlethread_workqueue("mcu_event_notifier");
}

static void mcu_notifier_handler(struct work_struct *work)
{
    //find data according work
    struct mcu_notifier_work *p = container_of(work, struct mcu_notifier_work, worker);

    //search mcu_notifier, call all call_backs
    struct mcu_notifier_node *pos, *n;
    list_for_each_entry_safe(pos, n, &mcu_notifier.head, entry) {
        if ((pos->tag == ((const pkt_header_t *)p->data)->tag) && (pos->cmd == ((const pkt_header_t *)p->data)->cmd)) {
            if (pos->notify != NULL) {
                pos->notify((const pkt_header_t *)p->data);
            }
        }
    }

    kfree(p->data);
    kfree(p);
}

static const void *avoid_coverity_p_work;
static const void *avoid_coverity_p_void;
static void mcu_notifier_queue_work(const pkt_header_t *head)
{
    struct mcu_notifier_work *notifier_work = kmalloc(sizeof(struct mcu_notifier_work), GFP_ATOMIC);
    if (NULL == notifier_work) {
        return;
    }
    notifier_work->data = kmalloc(head->length + sizeof(pkt_header_t), GFP_ATOMIC);
    if (NULL == notifier_work->data) {
        kfree(notifier_work);
        return;
    }

    memcpy(notifier_work->data, head, sizeof(pkt_header_t) + head->length);
    INIT_WORK(&notifier_work->worker, mcu_notifier_handler);
    queue_work(mcu_notifier.mcu_notifier_wq, &notifier_work->worker);

    /**begin: save alloc ptr, just to solve coverity warning**/
    avoid_coverity_p_work = notifier_work;
    avoid_coverity_p_void = notifier_work->data;
    /**end: save alloc ptr, just to solve coverity warning**/
}

static bool is_mcu_notifier(const pkt_header_t *head)
{
    struct mcu_notifier_node *pos, *n;
    list_for_each_entry_safe(pos, n, &mcu_notifier.head, entry) {
        if ((pos->tag == head->tag) && (pos->cmd == head->cmd)) {
            return true;
        }
    }

    return false;
}

int register_mcu_event_notifier(int tag, int cmd, int (*notify)(const pkt_header_t *head))
{
    struct mcu_notifier_node *pnode, *n;
    int ret = 0;
    unsigned long flags = 0;

    if ((!(TAG_BEGIN <= tag && tag < TAG_END)) || (NULL == notify)) {
        return -EINVAL;
    }

    spin_lock_irqsave(&mcu_notifier.lock, flags);
    //avoid regist more than once
    list_for_each_entry_safe(pnode, n, &mcu_notifier.head, entry) {
        if ((tag == pnode->tag) && (cmd == pnode->cmd) && (notify == pnode->notify)) {
            hwlog_warn("tag = %d, cmd = %d, notify = %pf has already registed in %s\n!", tag, cmd, notify, __func__);
            goto out;//return when already registed
        }
    }

    //make mcu_notifier_node
    pnode = kmalloc(sizeof(struct mcu_notifier_node), GFP_ATOMIC);
    if (NULL == pnode) {
        ret = -ENOMEM;
        goto out;
    }
    pnode->tag = tag;
    pnode->cmd = cmd;
    pnode->notify = notify;

    //add to list
    list_add(&pnode->entry, &mcu_notifier.head);
out:
    spin_unlock_irqrestore(&mcu_notifier.lock, flags);
    /**begin: save alloc ptr, just to solve coverity warning**/
    avoid_coverity_p_void = pnode;
    /**end: save alloc ptr, just to solve coverity warning**/

    return ret;
}

int unregister_mcu_event_notifier(int tag, int cmd, int (*notify)(const pkt_header_t *head))
{
    struct mcu_notifier_node *pos, *n;
    unsigned long flags = 0;

    if ((!(TAG_BEGIN <= tag && tag < TAG_END)) || (NULL == notify)) {
        return -EINVAL;
    }

    spin_lock_irqsave(&mcu_notifier.lock, flags);
    list_for_each_entry_safe(pos, n, &mcu_notifier.head, entry) {
        if ((tag == pos->tag) && (cmd == pos->cmd) && (notify == pos->notify)) {
            list_del(&pos->entry);
            kfree(pos);
            break;
        }
    }
    spin_unlock_irqrestore(&mcu_notifier.lock, flags);

    return 0;
}

/*******************************************************************************************
Function:       inputhub_route_recv_mcu_data
Description:    根据从MCU接收到的数据包中的tag和cmd做不同的分发和处理
Data Accessed:  无
Data Updated:   无
Input:          const char *buf, unsigned long length，从MCU接收到数据的地址和长度信息
Output:         无
Return:         成功或者失败信息
*******************************************************************************************/
int inputhub_route_recv_mcu_data(const char *buf, unsigned long length)
{
    const pkt_header_t *head = (const pkt_header_t *)buf;
#ifdef LINK_PACKAGE
    if (NULL == (head = pack(buf, length))) {
            return 0;//receive next partial package.
    }

#else
    if (head->length + OFFSET_OF_END_MEM(pkt_header_t, length) > length ) {
        hwlog_err("-------------->receive a partial package from mcu with head->length = %d, length = %d, in %s\n", (int)head->length, (int)length, __func__);
        return -EINVAL;
    }
#endif
    if (head->tag < TAG_BEGIN || head->tag >= TAG_END) {
        hwlog_err("---------------------->head value : tag=%#.2x, cmd=%#.2x, length=%#.2x in %s\n", head->tag, head->cmd, head->length, __func__);
        return -EINVAL;
    }

    if (is_sensor_data_report(head)) {
        //这里转成之前的格式(HAL层需要的格式)
        struct sensor_data event;
        event.type = head->tag;
        event.length = head->length;
        memcpy(event.value, buf + sizeof(pkt_header_t), event.length);
        if(head->tag == TAG_PS && event.value[0] != 0) //recieve proximity far
        {
            wake_lock_timeout(&wlock, HZ);
            hwlog_info("Kernel get far event!\n");
        }else if(head->tag == TAG_PS && event.value[0] == 0)
        {
            hwlog_info("Kernel get near event!!!!\n");
        }
        return inputhub_route_write(ROUTE_SHB_PORT, (char *)&event, event.length + OFFSET_OF_END_MEM(struct sensor_data, length));
    } else if (is_log_data_report(head)) {
        struct iom3_log_work *work = kmalloc(sizeof(struct iom3_log_work), GFP_ATOMIC);
        if(!work){
            return -ENOMEM;
        }
        work->log_p = kmalloc(head->length + sizeof(pkt_header_t),GFP_ATOMIC);
        if(!work->log_p){
            kfree(work);
            return -ENOMEM;
        }
        memcpy(work->log_p, head, head->length + sizeof(pkt_header_t));
        INIT_DELAYED_WORK(&(work->log_work), iom3_log_handle);
        schedule_delayed_work(&(work->log_work), msecs_to_jiffies(250));
        /**begin: save alloc ptr, just to solve coverity warning**/
        avoid_coverity_p_work = work;
        avoid_coverity_p_void = work->log_p;
        /**end: save alloc ptr, just to solve coverity warning**/
    } else if (is_requirement_resp(head)) {
        return report_resp_data((const pkt_header_resp_t *)head);
    /* < DTS2014010800416 ligang/00183889 20140108 begin */
    } else if(is_motion_data_report(head)) {
        char motion_data[head->length];
        memcpy(motion_data, buf + sizeof(pkt_header_t), head->length);
        return inputhub_route_write(ROUTE_MOTION_PORT, motion_data, head->length);
    /*  DTS2014010800416 ligang/00183889 20140108 end > */
    } else if (is_mcu_notifier(head)) {
        mcu_notifier_queue_work(head);
    } else {
        hwlog_err("--------->tag = %d, cmd = %d is not implement!\n", head->tag, head->cmd);
        return -EINVAL;
    }

    return 0;
}

/*******************************************************************************************
Function:       inputhub_route_cmd
Description:    根据端口号port对不同的命令做分发处理
Data Accessed:  无
Data Updated:   无
Input:          unsigned short port, unsigned int cmd, unsigned long arg，命令码和参数信息
Output:         无
Return:         成功或者失败信息
*******************************************************************************************/
// handle user command as quickly as possible.
int inputhub_route_cmd(unsigned short port, unsigned int cmd, unsigned long arg)
{
    switch (port) {
        case ROUTE_SHB_PORT:
        {
            return send_sensor_cmd(cmd, arg);
        }
        break;
        /* < DTS2014010800416 ligang/00183889 20140108 begin */
        case ROUTE_MOTION_PORT:
        {
            return send_motion_cmd(cmd, arg);
        }
        break;
        /*  DTS2014010800416 ligang/00183889 20140108 end >*/

        default:
            hwlog_err("unimplement port : %d in %s\n", port, __func__);
            return -EINVAL;
            break;
    }
    return 0;
}
EXPORT_SYMBOL_GPL(inputhub_route_cmd);

/*******************************************************************************************
Function:       set_backlight_brightness
Description:    设置背光的亮度
Data Accessed:  无
Data Updated:   无
Input:          int brightness，背光的亮度值
Output:         无
Return:         成功或者失败信息
*******************************************************************************************/
int set_backlight_brightness(int brightness)
{
    pkt_baklight_level_req_t pkt;
    pkt.hd.tag = TAG_BACKLIGHT;
    pkt.hd.cmd = CMD_BACKLIGHT_LEVEL_REQ;
    pkt.hd.resp = NO_RESP;
    pkt.hd.length = sizeof(pkt.level);
    pkt.level = brightness;

    return inputhub_mcu_write_cmd(&pkt, sizeof(pkt));
}

int set_log_config(uint8_t type, uint8_t val)
{
    pkt_log_config_req_t pkt;
    pkt.hd.tag = TAG_LOG;
    pkt.hd.cmd = CMD_LOG_CONFIG_REQ;
    pkt.hd.resp = NO_RESP;
    pkt.hd.length = sizeof(pkt_log_config_req_t) - sizeof(pkt_header_t);
    pkt.type = type;
    pkt.value = val;
    return inputhub_mcu_write_cmd(&pkt, sizeof(pkt));
}

/*******************************************************************************************
Function:       shb_set_light_leds
Description:    将命令信息转换成通信协议格式发送给MCU设置三色灯的工作模式
Data Accessed:  无
Data Updated:   无
Input:          const struct pkg_from_devsensorhub *cmdbuf，上层设置三色灯的命令的数据包的起始地址
Output:         无
Return:         成功或者失败信息
*******************************************************************************************/
static int shb_set_light_leds(const struct pkg_from_devsensorhub *cmdbuf)
{
    switch (cmdbuf->work_mode) {
        case WORK_FLASH:
        {//rgb light flash
            pkt_light_flicker_req_t pkt;
            pkt.hd.tag = TAG_RGBLIGHT;
            pkt.hd.cmd = CMD_LIGHT_FLICKER_REQ;
            pkt.hd.resp = NO_RESP;
            pkt.hd.length = sizeof(pkt) - sizeof(pkt.hd);//rgb, cmdbuf->on_ms, cmdbuf->off_ms
            pkt.rgb = cmdbuf->color_argb;
            pkt.ontime = cmdbuf->on_ms;
            pkt.offtime = cmdbuf->off_ms;

            hwlog_info("set rgb light in flash mode with color_argb = %#.8x, on_ms = %d, off_ms = %d!\n", cmdbuf->color_argb, cmdbuf->on_ms, cmdbuf->off_ms);
            return inputhub_mcu_write_cmd(&pkt, sizeof(pkt));
        }
        break;

        case WORK_ON:
        {//rgb light on
            pkt_light_bright_req_t pkt;
            pkt.hd.tag = TAG_RGBLIGHT;
            pkt.hd.cmd = CMD_LIGHT_BRIGHT_REQ;
            pkt.hd.resp = NO_RESP;
            pkt.hd.length = sizeof(pkt.rgb);
            pkt.rgb = cmdbuf->color_argb;

            hwlog_info("set rgb light on with color_argb = %#.8x!\n", cmdbuf->color_argb);
            return inputhub_mcu_write_cmd(&pkt, sizeof(pkt));
        }
        break;

        case WORK_OFF:
        {//rgb light off
            pkt_header_t pkt;
            pkt.tag = TAG_RGBLIGHT;
            pkt.cmd = CMD_CMN_CLOSE_REQ;
            pkt.resp = NO_RESP;
            pkt.length = 0;

            hwlog_info("rgb light off!\n");
            return inputhub_mcu_write_cmd(&pkt, sizeof(pkt));
        }
        break;

        default:
            hwlog_err("unknown work mode in %s!\n", __func__);
            return -EINVAL;
            break;
    }
}

/*******************************************************************************************
Function:       shb_set_light_buttons
Description:    将命令信息转换成通信协议格式发送给MCU设置button灯的工作模式
Data Accessed:  无
Data Updated:   无
Input:          const struct pkg_from_devsensorhub *cmdbuf，上层设置button灯的命令的数据包的起始地址
Output:         无
Return:         成功或者失败信息
*******************************************************************************************/
static int shb_set_light_buttons(const struct pkg_from_devsensorhub *cmdbuf)
{
    switch (cmdbuf->work_mode) {
        case WORK_ON:
        {//button light on
            pkt_light_bright_req_t pkt;
            pkt.hd.tag = TAG_BUTTONLIGHT;
            pkt.hd.cmd = CMD_LIGHT_BRIGHT_REQ;
            pkt.hd.resp = NO_RESP;
            pkt.hd.length = sizeof(pkt.rgb);
            pkt.rgb = RGB_BUTTON_LIGHT_ON;//

            hwlog_info("button light on!\n");
            return inputhub_mcu_write_cmd(&pkt, sizeof(pkt));
        }
        break;

        case WORK_OFF:
        {//button light off
            pkt_header_t pkt;
            pkt.tag = TAG_BUTTONLIGHT;
            pkt.cmd = CMD_CMN_CLOSE_REQ;
            pkt.resp = NO_RESP;
            pkt.length = 0;

            hwlog_info("button light off!\n");
            return inputhub_mcu_write_cmd(&pkt, sizeof(pkt));
        }
        break;

        default:
            hwlog_err("unknown work mode in %s!\n", __func__);
            return -EINVAL;
            break;
    }
}

/*******************************************************************************************
Function:       shb_set_vibrator
Description:    将命令信息转换成通信协议格式发送给MCU设置马达的工作模式
Data Accessed:  无
Data Updated:   无
Input:          const struct pkg_from_devsensorhub *cmdbuf，上层设置马达的命令的数据包的起始地址
Output:         无
Return:         成功或者失败信息
*******************************************************************************************/
static int shb_set_vibrator(const struct pkg_from_devsensorhub *cmdbuf)
{
    if (NULL == cmdbuf) {
        hwlog_err("p == NULL in %s!\n", __func__);
        return -EINVAL;
    }

    switch (cmdbuf->work_mode) {
        case WORK_OFF:
        {
            pkt_header_t pkt;
            pkt.tag = TAG_VIBRATOR;
            pkt.cmd = CMD_CMN_CLOSE_REQ;
            pkt.resp = NO_RESP;
            pkt.length = 0;

            hwlog_info("vibrator off!\n");
            return inputhub_mcu_write_cmd(&pkt, sizeof(pkt));
        }
        break;

        case WORK_MS:
        {
            pkt_vibrator_single_req_t pkt;
            pkt.hd.tag = TAG_VIBRATOR;
            pkt.hd.cmd = CMD_VIBRATOR_SINGLE_REQ;
            pkt.hd.resp = NO_RESP;
            pkt.hd.length = sizeof(pkt.duration);
            pkt.duration = cmdbuf->vibrate_ms;

            hwlog_info("vibrator run %d ms!\n", pkt.duration);
            return inputhub_mcu_write_cmd(&pkt, sizeof(pkt));
        }
        break;

        case WORK_PATTERN:
        {
            pkt_vibrator_repeat_req_t pkt;
            pkt.hd.tag = TAG_VIBRATOR;
            pkt.hd.cmd = CMD_VIBRATOR_REPEAT_REQ;
            pkt.hd.resp = NO_RESP;
            pkt.hd.length = OFFSET(pkt_vibrator_repeat_req_t, pattern) - OFFSET_OF_END_MEM(pkt_vibrator_repeat_req_t, hd.length) + cmdbuf->pattern_size * sizeof(pkt.pattern[0]);
            pkt.index = cmdbuf->repeat_index;
            pkt.size = cmdbuf->pattern_size;

            if (pkt.size > MAX_PATTERN_SIZE) {
                hwlog_err("pkt.size = %d is too large in %s!\n", pkt.size, __func__);
                return -EINVAL;
            } else {
                memcpy(&pkt.pattern, &cmdbuf->pattern, pkt.size * sizeof(pkt.pattern[0]));
            }

            hwlog_info("vibrator run in pattern mode!\n");
            return inputhub_mcu_write_cmd(&pkt, pkt.hd.length + sizeof(pkt.hd));
        }
        break;

        default :
            hwlog_err("unknown cmd in %s!\n", __func__);
            return -EINVAL;
    }
}

/*******************************************************************************************
Function:       parse_write_cmds_from_devsensorhub
Description:    解析向/dev/sensorhub写命令的数据包并分发
Data Accessed:  无
Data Updated:   无
Input:          const struct pkg_from_devsensorhub *cmdbuf，上层设置马达的命令的数据包的起始地址，size_t len保留
Output:         无
Return:         成功或者失败信息
*******************************************************************************************/
int parse_write_cmds_from_devsensorhub(const struct pkg_from_devsensorhub *cmdbuf, size_t len)
{
    if (NULL == cmdbuf) {
        return -EINVAL;
    }

    switch (cmdbuf->type) {
        case TYPE_BUTTONLIGHT:
            return shb_set_light_buttons(cmdbuf);
        case TYPE_LEDS:
            return shb_set_light_leds(cmdbuf);
        case TYPE_VIBRATOR:
            return shb_set_vibrator(cmdbuf);

        default:
            hwlog_err("unknown type in %s\n", __func__);
            return -EINVAL;
    }

    return -EINVAL;
}

/*******************************************************************************************
Function:       tell_ap_status_to_mcu
Description:    向MCU通知AP的状态如AP初始化完成、亮灭屏等状态
Data Accessed:  无
Data Updated:   无
Input:          int ap_st，AP当前的状态
Output:         无
Return:         成功或者失败信息
*******************************************************************************************/
int tell_ap_status_to_mcu(int ap_st)
{
    if ((ST_BEGIN <= ap_st) && (ap_st < ST_END)) {
        pkt_sys_statuschange_req_t pkt;
        pkt.hd.tag = TAG_SYS;
        pkt.hd.cmd = CMD_SYS_STATUSCHANGE_REQ;
        pkt.hd.resp = NO_RESP;
        pkt.hd.length = sizeof(pkt) - sizeof(pkt.hd);
        pkt.status = ap_st;
        hwlog_info("------------>tell mcu ap in status %d\n", ap_st);

        return inputhub_mcu_write_cmd(&pkt, sizeof(pkt));
    } else {
        hwlog_err("error status %d in %s\n", ap_st, __func__);
        return -EINVAL;
    }
}

static int sensorhub_fb_notifier(struct notifier_block *nb,
                 unsigned long action, void *data)
{
    switch (action) {
    case FB_EVENT_BLANK://change finished
    {
        struct fb_event *event = data;
        int *blank = event->data;
        switch (*blank) {
        case FB_BLANK_UNBLANK:/* screen: unblanked, hsync: on,  vsync: on */
            tell_ap_status_to_mcu(ST_SCREENON);
            break;

        case FB_BLANK_VSYNC_SUSPEND:/* screen: blanked,   hsync: on,  vsync: off */
        case FB_BLANK_HSYNC_SUSPEND:/* screen: blanked,   hsync: off, vsync: on */
        case FB_BLANK_POWERDOWN:/* screen: blanked,   hsync: off, vsync: off */
            tell_ap_status_to_mcu(ST_SCREENOFF);
            break;

        default:
            hwlog_err("unknown---> lcd unknown in %s\n", __func__);
            break;
        }
    }
        break;

    default:
        break;
    }

    return NOTIFY_OK;
}

static int sensorhub_pm_notify(struct notifier_block *nb,
                   unsigned long mode, void *_unused)
{
    switch (mode) {
    case PM_SUSPEND_PREPARE:/* Going to suspend the system */
        disable_sensors_when_suspend();
        tell_ap_status_to_mcu(ST_SLEEP);
        break;

    case PM_POST_SUSPEND:/* resume from suspend */
        tell_ap_status_to_mcu(ST_WAKEUP);
        enable_sensors_when_resume();
        break;

    case PM_HIBERNATION_PREPARE:/* Going to hibernate */
    case PM_POST_HIBERNATION:/* Hibernation finished */
    case PM_RESTORE_PREPARE:/* Going to restore a saved image */
    case PM_POST_RESTORE:/* Restore failed */
    default:
        break;
    }

    return 0;
}

static struct notifier_block fb_notify = {
    .notifier_call = sensorhub_fb_notifier,
};

static int send_current_to_mcu(int current_now)
{
    pkt_current_data_req_t pkt;
    pkt.hd.tag = TAG_CURRENT;
    pkt.hd.cmd = CMD_CURRENT_DATA_REQ;
    pkt.hd.resp = 0;
    pkt.hd.length = sizeof(pkt.current_now);
    pkt.current_now = current_now;
    hwlog_debug("send current val = %d in %s\n", current_now, __func__);
    return inputhub_mcu_write_cmd(&pkt, sizeof(pkt));
}

static int mcu_get_current(const pkt_header_t *head)
{
    hwlog_info("iom3 request current in %s\n", __func__);
    return open_send_current(send_current_to_mcu);
}

static int mcu_unget_current(const pkt_header_t *head)
{
    hwlog_info("iom3 unrequest current in %s\n", __func__);
    return close_send_current();
}

extern int mcu_sys_ready_callback(const pkt_header_t *head);
static void set_notifier(void)
{
    /**register mcu event notifier**/
    init_mcu_notifier_list();
    register_mcu_event_notifier(TAG_SYS, CMD_SYS_STATUSCHANGE_REQ, mcu_sys_ready_callback);
    register_mcu_event_notifier(TAG_CURRENT, CMD_CURRENT_GET_REQ, mcu_get_current);
    register_mcu_event_notifier(TAG_CURRENT, CMD_CURRENT_UNGET_REQ, mcu_unget_current);

    /**register to pm**/
    pm_notifier(sensorhub_pm_notify, 0);

    /**register to framebuffer**/
    if (fb_register_client(&fb_notify)) {
        hwlog_err("register fb client failed in %s\n", __func__);
    }
}
/*******************************************************************************************
Function:       init_locks
Description:    初始化本文件中用到的互斥锁
Data Accessed:  无
Data Updated:   无
Input:          无
Output:         无
Return:         无
*******************************************************************************************/
static void init_locks(void)
{
    int i;
    for (i = 0; i < sizeof(package_route_tbl) / sizeof(package_route_tbl[0]); ++i) {
        spin_lock_init(&package_route_tbl[i].buffer_spin_lock);
    }
    mutex_init(&mutex_write_cmd);
    init_completion(&type_record.resp_complete);
    mutex_init(&type_record.lock_mutex);
    spin_lock_init(&type_record.lock_spin);
    /* Initialize wakelock*/
    wake_lock_init(&wlock, WAKE_LOCK_SUSPEND, "sensorhub");
}

/*******************************************************************************************
Function:       inputhub_route_init
Description:    本文件的初始化函数
Data Accessed:  无
Data Updated:   无
Input:          无
Output:         无
Return:         成功
*******************************************************************************************/
int inputhub_route_init(void)
{
    init_locks();
    set_notifier();
    return 0;
}
EXPORT_SYMBOL_GPL(inputhub_route_init);

/*******************************************************************************************
Function:       close_all_ports
Description:    关闭所有端口，释放所有端口申请的动态内存，并清除读写信息
Data Accessed:  package_route_tbl
Data Updated:   package_route_tbl
Input:          无
Output:         无
Return:         无
*******************************************************************************************/
void close_all_ports(void)
{
    int i;
    for (i = 0; i < sizeof(package_route_tbl) / sizeof(package_route_tbl[0]); ++i) {
        inputhub_route_close(package_route_tbl[i].port);
    }
}

/*******************************************************************************************
Function:       inputhub_route_exit
Description:    调用inputhub_route_exit关闭所有的端口
Data Accessed:  无
Data Updated:   无
Input:          无
Output:         无
Return:         成功
*******************************************************************************************/
void inputhub_route_exit(void)
{
    //close all ports
    close_all_ports();
    wake_lock_destroy(&wlock);
}
EXPORT_SYMBOL_GPL(inputhub_route_exit);

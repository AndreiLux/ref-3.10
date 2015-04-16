/*
 *  HIFI MISC TRANS MSG
 *  Copyright (c) 1994-2013 by HISI
 *
 *   This program is distributed in the hope that we can send msg and
 *   data to hifi
 *
 */

#ifndef _AUDIO_HIFI_H
#define _AUDIO_HIFI_H

#include <linux/types.h>
typedef enum {
    HIFI_CHN_SYNC_CMD = 0,
    HIFI_CHN_READNOTICE_CMD,
    HIFI_CHN_INVAILD_CMD
} HIFI_CHN_CMD_TYPE;

typedef struct HIFI_CHN_CMD_STRUCT
{
    HIFI_CHN_CMD_TYPE cmd_type;
    unsigned int           sn;
} HIFI_CHN_CMD;

/*
    入参，透传给HIFI的参数
    出参，HIFI返回的，透传给AP的参数
*/
struct misc_io_async_param {
	void *                  para_in;        /*入参buffer*/
	unsigned int            para_size_in;   /*入参buffer长度*/
};

/* misc_io_sync_cmd */
struct misc_io_sync_param {
	void *                  para_in;            /*入参buffer*/
	unsigned int            para_size_in;       /*入参buffer长度*/
	void *                  para_out;           /*出参buffer*/
	unsigned int            para_size_out;      /*出参buffer长度*/
};

/* misc_io_senddata_cmd */
struct misc_io_senddata_async_param {
	void *                  para_in;            /*入参buffer*/
	unsigned int            para_size_in;       /*入参buffer长度*/
	void *                  data_src;           /*大数据源地址*/
	unsigned int            data_src_size;      /*大数据源长度*/
};

/* misc_io_send_recv_data_sync_cmd */
struct misc_io_senddata_sync_param {
	void *                  para_in;            /*入参buffer*/
	unsigned int            para_size_in;       /*入参buffer长度*/
	void *                  src;                /*数据源地址*/
	unsigned int            src_size;           /*数据源长度*/
	void *                  dst;                /*地址*/
	unsigned int            dst_size;           /*长度*/
	void *                  para_out;           /*出参buffer*/
	unsigned int            para_size_out;      /*出参buffer长度*/
};

struct misc_io_get_phys_param {
	unsigned int            flag;               /**/
	unsigned long           phys_addr;          /*获取的物理地址*/
};

struct misc_io_dump_buf_param {
    void *                  user_buf;           /*用户空间分配的内存地址*/
    unsigned int            buf_size;           /*用户空间分配的内存长度*/
};

//下面是AP发给HiFi Misc设备的ioctl命令，需要HiFi Misc设备进行响应
#define HIFI_MISC_IOCTL_ASYNCMSG        _IOWR('A', 0x70, struct misc_io_async_param)          //AP向HiFi传递异步消息
#define HIFI_MISC_IOCTL_SYNCMSG         _IOW('A', 0x71, struct misc_io_sync_param)            //AP向HiFi传递同步消息
#define HIFI_MISC_IOCTL_SENDDATA_SYNC   _IOW('A', 0x72, struct misc_io_senddata_sync_param)    //AP向HiFi同步传递数据
#define HIFI_MISC_IOCTL_GET_PHYS        _IOWR('A', 0x73, struct misc_io_get_phys_param)        //AP获取物理地址
#define HIFI_MISC_IOCTL_TEST            _IOWR('A', 0x74, struct misc_io_senddata_sync_param)   //AP测试消息
#define HIFI_MISC_IOCTL_WRITE_PARAMS    _IOWR('A', 0x75, struct misc_io_sync_param)            //写算法参数到HIFI

#define HIFI_MISC_IOCTL_DUMP_HIFI       _IOWR('A', 0x76, struct misc_io_dump_buf_param)        //读取HIFI在DDR上的数据并传递至用户空间
#define HIFI_MISC_IOCTL_DUMP_CODEC      _IOWR('A', 0x77, struct misc_io_dump_buf_param)        //读取CODEC寄存器并传递至用户空间
#define HIFI_MISC_IOCTL_DUMP_MP3_DATA   _IOWR('A', 0x78, unsigned long)        //保存DDR上的数据到data目录下
#define HIFI_MISC_IOCTL_WAKEUP_THREAD   _IOW('A',  0x79, unsigned long)        //唤醒read线程,正常退出
#endif // _AUDIO_HIFI_H


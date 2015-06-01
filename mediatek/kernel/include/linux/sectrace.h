#ifndef _SECTRACE_H
#define _SECTRACE_H


#include <linux/ioctl.h>


enum interface_type {
	if_tci,
	if_dci,
};

enum usage_type {
	usage_tl,
	usage_dr,
	usage_tl_dr,
};

typedef int (*callback_tl_map)(void *va, size_t size);
typedef int (*callback_tl_unmap)(void *va, size_t size);
typedef int (*callback_tl_transact)(void);

typedef int (*callback_dr_map)(unsigned long pa, size_t size);
typedef int (*callback_dr_unmap)(unsigned long pa, size_t size);
typedef int (*callback_dr_transact)(void);

struct callback_tl_func {
	callback_tl_map map;
	callback_tl_unmap unmap;
	callback_tl_transact transact;
};

struct callback_dr_func {
	callback_dr_map map;
	callback_dr_unmap unmap;
	callback_dr_transact transact;
};

union callback_func {
	struct callback_tl_func tl;
	struct callback_dr_func dr;
};


#define CMD_TL_NONE		(0)
#define CMD_TL_START	(1)
#define CMD_TL_STOP		(2)
#define CMD_TL_MAP_PA	(3)
#define CMD_TL_UNMAP_PA	(4)

#define CMD_DR_NONE		(0)
#define CMD_DR_START	(1)
#define CMD_DR_STOP		(2)


#define SIZE_LOG_ITEM	(32)

#define NUM_TRANSACTION_ITEM	(2)
#define NUM_HEADER_ITEM	(1)
#define BEGIN_LOG_ITEM	(NUM_TRANSACTION_ITEM + NUM_HEADER_ITEM)


#define SECTRACE_BUF_SIZE_MIN	(1) /* KB */
#define KB2BYTE(x)				((x) << 10)
#define BYTE2KB(x)				((x) >> 10)


enum sectrace_log_type {
	sectrace_begin = 0,
	sectrace_end = 1,
};

struct sectrace_log_item {
	uint64_t timestamp;
	char type;
	char name[SIZE_LOG_ITEM-sizeof(uint64_t)-sizeof(char)];
};

struct sectrace_log_header {
	unsigned long curr_item;
	char pad[SIZE_LOG_ITEM-sizeof(unsigned long)];
};

struct sectrace_transaction {
	uint32_t cmd;
	uint32_t ret;
	unsigned long param0;
	unsigned long param1;
	char pad[SIZE_LOG_ITEM-sizeof(uint32_t)*2-sizeof(unsigned long)*2];
};


/* TLC */
struct sectrace_tlc_buf_alloc {
	unsigned long pa;
	size_t size;
};

#define MTK_SECTRACE_TLC_BUF_IOCTL_MAGIC	'S'
#define MTK_SECTRACE_TLC_BUF_ALLOC			_IOR(MTK_SECTRACE_TLC_BUF_IOCTL_MAGIC, 0, struct sectrace_tlc_buf_alloc)
#define MTK_SECTRACE_TLC_BUF_FREE			_IO(MTK_SECTRACE_TLC_BUF_IOCTL_MAGIC, 1)

enum sectrace_tlc_event {
	event_enable = 0,
	event_disable,
	event_start,
	event_pause,
	event_stop,
};


extern int init_sectrace(const char *name, enum interface_type ci, enum usage_type usage, size_t size, union callback_func *callback);
extern int deinit_sectrace(const char *name);


#endif /* _SECTRACE_H */

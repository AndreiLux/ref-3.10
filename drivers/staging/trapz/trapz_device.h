/*
 * trapz_device.h
 *
 * TRAPZ (TRAcing and Profiling for Zpeed) Device header file
 *
 * Copyright (C) Amazon Technologies Inc. All rights reserved.
 * Andy Prunicki (prunicki@lab126.com)
 * Martin Unsal (munsal@lab126.com)
 * TODO: Add additional contributor's names.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * NOTE:
 * Copy this header file into any app that needs to read from the trapz
 * device file or make an ioctl call.
 */

#include <linux/ioctl.h>

#ifndef _LINUX_TRAPZ_DEVICE_H
#define _LINUX_TRAPZ_DEVICE_H

#define TRAPZ_DEV_NAME            "trapz"

typedef struct {
	int bufferSize; /* the buffer will hold this many entries */
	int count;      /* the buffer contains this many entries */
	int total;      /* count of entries added since the last reset */
} trapz_config;

typedef struct {
	union {
		struct {
			unsigned char ctrl;
			unsigned char counter;
			unsigned char extra_1;
			unsigned char extra_2;
			unsigned char cpu;
			unsigned char comp_trace_id[3];
			unsigned short pid;
			unsigned short tid;
			struct timespec ts;         /* time stamp */
		};
		struct {
			unsigned char ctrl_e;
			unsigned char counter_e;
			unsigned char format;
			unsigned char unused;
			unsigned int extras[4];
		};
	};
} trapz_entry_t;

/* ======================================== */
/* ============== Triggers ================ */
/* = Only available on configured kernels = */
/* ======================================== */
typedef struct {
	int start_trace_point;
	int end_trace_point;
	int trigger_id;
	int single_shot;
} trapz_trigger_t;

typedef struct {
	trapz_trigger_t trigger;
	struct timespec start_ts;
	struct timespec end_ts;
	int trigger_active;
} trapz_trigger_event_t;
/* ======================================== */

/* ctrl field output masks */
#define TRAPZ_LEVEL_MASK_OUT      (TRAPZ_LEVEL_MASK << TRAPZ_LEVEL_OFFSET)
#define TRAPZ_FLAGS_MASK_OUT      (TRAPZ_FLAGS_MASK << TRAPZ_FLAGS_OFFSET)
#define TRAPZ_CAT_ID_MASK_OUT     (TRAPZ_CAT_ID_MASK << TRAPZ_CAT_ID_OFFSET)
#define TRAPZ_COMP_ID_MASK_OUT    (TRAPZ_COMP_ID_MASK << TRAPZ_COMP_ID_OFFSET)
#define TRAPZ_TRACE_ID_MASK_OUT   (TRAPZ_TRACE_ID_MASK << TRAPZ_TRACE_ID_OFFSET)

/* ctrl field shift out */
#define TRAPZ_LEVEL_OUT(x) ((x & TRAPZ_LEVEL_MASK_OUT) >> TRAPZ_LEVEL_OFFSET)
#define TRAPZ_FLAGS_OUT(x) ((x & TRAPZ_FLAGS_MASK_OUT) >> TRAPZ_FLAGS_OFFSET)
#define TRAPZ_CAT_ID_OUT(x) ((x & TRAPZ_CAT_ID_MASK_OUT) >> TRAPZ_CAT_ID_OFFSET)
#define TRAPZ_COMP_ID_OUT(x) \
	((x & TRAPZ_COMP_ID_MASK_OUT) >> TRAPZ_COMP_ID_OFFSET)
#define TRAPZ_TRACE_ID_OUT(x) \
	((x & TRAPZ_TRACE_ID_MASK_OUT) >> TRAPZ_TRACE_ID_OFFSET)

/* buffer masks */
#define TRAPZ_BUFF_REC_TYPE_MASK      0x80
#define TRAPZ_BUFF_COMPLETE_MASK      0x40
#define TRAPZ_BUFF_CAT_ID_MASK        0x30
#define TRAPZ_BUFF_EXTRA_COUNT_MASK   0x0f

/* trigger masks */
#define TRAPZ_TRIGGER_MASK            \
	((TRAPZ_CAT_ID_MASK << TRAPZ_CAT_ID_OFFSET) | \
	(TRAPZ_COMP_ID_MASK << TRAPZ_COMP_ID_OFFSET) | \
	(TRAPZ_TRACE_ID_MASK << TRAPZ_TRACE_ID_OFFSET))

/* Circular buffer mask 'n shifters */
#define TRAPZ_BUFF_REC_TYPE(x) (x & TRAPZ_BUFF_REC_TYPE_MASK)
#define TRAPZ_BUFF_REC_COMPLETE(x) (x & TRAPZ_BUFF_COMPLETE_MASK)
#define TRAPZ_BUFF_CAT_ID(x) ((x & TRAPZ_BUFF_CAT_ID_MASK) >> 4)
#define TRAPZ_BUFF_EXTRA_COUNT(x) (x & TRAPZ_BUFF_EXTRA_COUNT_MASK)
#define TRAPZ_BUFF_COMP_ID(x, y) (x << 4 | ((y & 0xf0) >> 4))
#define TRAPZ_BUFF_TRACE_ID(x, y) (((x & 0x0f) << 4) | y)

#define TRAPZ_EXTRA_FORMAT_INT 1
#define TRAPZ_EXTRA_FORMAT_STRING 2

/* IOCTL codes */
#define __TRAPZIO	0xAF
/* enable/disable the trapzdriver */
#define TRAPZ_ENABLE_DRIVER      _IO(__TRAPZIO, 1)
/* get buffer capacity and fill size */
#define TRAPZ_GET_CONFIG         _IO(__TRAPZIO, 2)
/* resize the buffer */
#define TRAPZ_SET_BUFFER_SIZE    _IO(__TRAPZIO, 3)
/* reset to defaults - log level, filters, and triggers */
#define TRAPZ_RESET_DEFAULTS     _IO(__TRAPZIO, 4)
#define TRAPZ_CLEAR_BUFFER       _IO(__TRAPZIO, 5)  /* clear buffer */
#define TRAPZ_GET_VERSION        _IO(__TRAPZIO, 6)  /* get TRAPZ version */
#define TRAPZ_SET_LOG_LEVEL      _IO(__TRAPZIO, 7)  /* set minimum log level */
#define TRAPZ_CHK_LOG_LEVEL      _IO(__TRAPZIO, 8)  /* check log level */
#define TRAPZ_ADD_TRIGGER        _IO(__TRAPZIO, 9)  /* add trigger */
#define TRAPZ_DEL_TRIGGER        _IO(__TRAPZIO, 10) /* delete trigger */
#define TRAPZ_CLR_TRIGGERS       _IO(__TRAPZIO, 11) /* clear all triggers */
#define TRAPZ_CNT_TRIGGERS       _IO(__TRAPZIO, 12) /* count triggers */

#endif  /* _LINUX_TRAPZ_DEVICE_H */

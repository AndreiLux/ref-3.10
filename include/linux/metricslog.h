/*
 * Copyright (C) 2011 Amazon Technologies, Inc.
 * Portions Copyright (C) 2007-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_METRICSLOG_H
#define _LINUX_METRICSLOG_H

#ifdef CONFIG_AMAZON_METRICS_LOG

#include <linux/xlog.h>

typedef enum {
	VITALS_NORMAL = 0,
	VITALS_FGTRACKING,
	VITALS_TIME_BUCKET,
} vitals_type;

void log_to_metrics(enum android_log_priority priority,
	const char *domain, char *logmsg);

void log_counter_to_vitals(enum android_log_priority priority,
	const char *domain, const char *program,
	const char *source, const char *key,
	long counter_value, const char *unit, vitals_type type);
void log_timer_to_vitals(enum android_log_priority priority,
	const char *domain, const char *program,
	const char *source, const char *key,
	long timer_value, const char *unit, vitals_type type);
#endif

#endif /* _LINUX_METRICSLOG_H */


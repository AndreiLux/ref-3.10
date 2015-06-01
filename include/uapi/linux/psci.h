/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *	Copyright (C) 2014, Ashwin Chaugule <ashwin.chaugule@linaro.org>
 */

#ifndef _UAPI_LINUX_PSCI_H
#define _UAPI_LINUX_PSCI_H

/* PSCI Function IDs as per PSCI spec v0.2 */
#define PSCI_ID_VERSION					0x84000000
#define PSCI_ID_CPU_SUSPEND_32			0x84000001
#define PSCI_ID_CPU_SUSPEND_64			0xc4000001
#define PSCI_ID_CPU_OFF					0x84000002
#define PSCI_ID_CPU_ON_32				0x84000003
#define PSCI_ID_CPU_ON_64				0xC4000003
#define PSCI_ID_AFFINITY_INFO_32		0x84000004
#define PSCI_ID_AFFINITY_INFO_64		0xC4000004
#define PSCI_ID_CPU_MIGRATE_32			0x84000005
#define PSCI_ID_CPU_MIGRATE_64			0xc4000005
#define PSCI_ID_MIGRATE_INFO_TYPE		0x84000006
#define PSCI_ID_MIGRATE_INFO_UP_CPU_32	0x84000007
#define PSCI_ID_MIGRATE_INFO_UP_CPU_64	0xc4000007
#define PSCI_ID_SYSTEM_OFF				0x84000008
#define PSCI_ID_SYSTEM_RESET			0x84000009

#define PSCI_RET_SUCCESS		0
#define PSCI_RET_EOPNOTSUPP		-1
#define PSCI_RET_EINVAL			-2
#define PSCI_RET_EPERM			-3

/* Return values from the PSCI_ID_AFFINITY_INFO Function. */
#define PSCI_AFFINITY_INFO_RET_ON			0
#define PSCI_AFFINITY_INFO_RET_OFF			1
#define PSCI_AFFINITY_INFO_RET_ON_PENDING	2

#endif /* _UAPI_LINUX_PSCI_H */

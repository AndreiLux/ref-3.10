/*
 * Register map access API
 *
 * Copyright 2013 LG Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/device.h>

#define IPC_NAME_SIZE   20

struct ipc_client {
	unsigned short flags;
	unsigned int addr;
	unsigned int slot;
	char name[IPC_NAME_SIZE];
	struct device *dev;
};

#define to_ipc_client(d) container_of(d, struct ipc_client, dev)

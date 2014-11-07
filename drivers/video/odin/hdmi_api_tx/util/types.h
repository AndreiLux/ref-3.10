/*
 * types.h
 *
 *  Created on: Jun 25, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef TYPES_H_
#define TYPES_H_

/*
 * @file: types
 * Define basic type optimised for use in the API so that it can be
 * platform-independent.
 */
#include <linux/types.h>
#include <linux/bitops.h>

typedef void* mutex_t;

typedef void (*handler_t)(void *);
typedef void* (*thread_t)(void *);

#define TRUE  1
#define FALSE 0

typedef struct
{
	u8 *buffer;
	unsigned int size;
}buffer_t;

#endif /* TYPES_H_ */

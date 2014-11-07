/*
 * log.c
 *
 *  Created on: Jun 25, 2010
 *
 * Synopsys Inc.
 * SG DWC PT02
 */

#include "log.h"
#include <linux/kernel.h>

static verbose_t log_verbose = VERBOSE_NOTICE;
static numeric_t log_numeric = NUMERIC_HEX;
static unsigned log_verbosewrite = 0;

void log_setverbose(verbose_t verbose)
{
	log_verbose = verbose;
}

void log_setnumeric(numeric_t numeric)
{
	log_numeric = numeric;
}

void log_setverbosewrite(unsigned state)
{
	log_verbosewrite = state;
}

void log_printwrite(unsigned a, unsigned b)
{
	if (log_verbosewrite == 1)
	{
		if (log_numeric == NUMERIC_DEC)
		{
			printk("%d, %d\n", a, b);
		}
		else
		{
			printk("0x%x, 0x%x\n", a, b);
		}
	}
}

void log_print0(verbose_t verbose, const char* functionName)
{
	if (verbose == VERBOSE_ERROR)
	{
		printk("ERROR: ");
	}
	if (verbose == VERBOSE_WARN)
	{
		printk("WARNING: ");
	}
	if (verbose <= log_verbose)
	{
		printk("%s\n", functionName);
	}
}

void log_print1(verbose_t verbose, const char* functionName, const char* a)
{
	if (verbose == VERBOSE_ERROR)
	{
		printk("ERROR: ");
	}
	if (verbose == VERBOSE_WARN)
	{
		printk("WARNING: ");
	}
	if (verbose <= log_verbose)
	{
		printk("%s: %s\n", functionName, a);
	}
}

void log_print2(verbose_t verbose, const char* functionName, const char* a,
		unsigned b)
{
	if (verbose == VERBOSE_ERROR)
	{
		printk("ERROR: ");
	}
	if (verbose == VERBOSE_WARN)
	{
		printk("WARNING: ");
	}
	if (verbose <= log_verbose)
	{
		if (log_numeric == NUMERIC_DEC)
		{
			printk("%s: %s, %d\n", functionName, a, b);
		}
		else
		{
			printk("%s: %s, 0x%x\n", functionName, a, b);
		}
	}
}

void log_print3(verbose_t verbose, const char* functionName, const char* a,
		unsigned b, unsigned c)
{
	if (verbose == VERBOSE_ERROR)
	{
		printk("ERROR: ");
	}
	if (verbose == VERBOSE_WARN)
	{
		printk("WARNING: ");
	}
	if (verbose <= log_verbose)
	{
		if (log_numeric == NUMERIC_DEC)
		{
			printk("%s: %s, %d, %d\n", functionName, a, b, c);
		}
		else
		{
			printk("%s: %s, 0x%x, 0x%x\n", functionName, a, b, c);
		}
	}
}

void log_printint(verbose_t verbose, const char* functionName, unsigned a)
{
	if (verbose <= log_verbose)
	{
		if (log_numeric == NUMERIC_DEC)
		{
			printk("%s: %d\n", functionName, a);
		}
		else
		{
			printk("%s: 0x%x\n", functionName, a);
		}
	}
}

void log_printint2(verbose_t verbose, const char* functionName, unsigned a,
		unsigned b)
{
	if (verbose <= log_verbose)
	{
		if (log_numeric == NUMERIC_DEC)
		{
			printk("%s: %d, %d\n", functionName, a, b);
		}
		else
		{
			printk("%s: 0x%x, 0x%x\n", functionName, a, b);
		}
	}
}

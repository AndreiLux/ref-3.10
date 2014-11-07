
/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/mm.h>
#include <linux/iommu.h>
#include <linux/odin_iommu.h>
#include <linux/ion.h>

#include <asm/page.h>

#include "odin_mem_debug.h"


/* print sorted and merged ion buffer's PA ===================== */

static ssize_t show_ion_used_buffer_sorted(
										struct device *dev,
										struct device_attribute *attr,
										char *buf )
{

	odin_print_ion_used_buffer_sorted();

	return 0;

}

static ssize_t store_ion_used_buffer_sorted(
										struct device *dev,
										struct device_attribute *attr,
										const char *buf, size_t count )
{

	return count;

}

static DEVICE_ATTR( ion_used_buffer_sorted, 0664,
							show_ion_used_buffer_sorted,
							store_ion_used_buffer_sorted );

/* print every ion buffer's PA ================================ */

static ssize_t show_ion_used_buffer( struct device *dev,
										struct device_attribute *attr,
										char *buf )
{

	odin_print_ion_used_buffer();

	return 0;

}

static ssize_t store_ion_used_buffer( struct device *dev,
										struct device_attribute *attr,
										const char *buf, size_t count )
{

	return count;

}

static DEVICE_ATTR( ion_used_buffer, 0664,
							show_ion_used_buffer,
							store_ion_used_buffer );

/* ION's page pool ======================================== */

static ssize_t show_ion_unused_buffer( struct device *dev,
										struct device_attribute *attr,
										char *buf )
{

	odin_print_ion_unused_buffer();

	return 0;

}

static ssize_t store_ion_unused_buffer( struct device *dev,
										struct device_attribute *attr,
										const char *buf, size_t count )
{

	return count;

}

static DEVICE_ATTR( ion_unused_buffer, 0664,
							show_ion_unused_buffer,
							store_ion_unused_buffer );

/* View memory : buffer, handle information ===================== */

static bool odin_mem_debug_bh_message_onoff = false;

bool get_odin_debug_bh_message( void )
{

	return odin_mem_debug_bh_message_onoff;

}
EXPORT_SYMBOL(get_odin_debug_bh_message);

void set_odin_debug_bh_message( bool val )
{

	odin_mem_debug_bh_message_onoff = val;

}
EXPORT_SYMBOL(set_odin_debug_bh_message);

static ssize_t show_debug_bh_message( struct device *dev,
										struct device_attribute *attr,
										char *buf )
{

	if ( get_odin_debug_bh_message() == false )
		printk( "odin memory buffer, handle message : off\n"   );
	else if ( get_odin_debug_bh_message() == true )
		printk( "odin memory buffer, handle message : on\n"   );

	return 0;

}

static ssize_t store_debug_bh_message( struct device *dev,
										struct device_attribute *attr,
										const char *buf, size_t count )
{

	if ( (*(buf+0)=='o') && (*(buf+1)=='n') ){
		set_odin_debug_bh_message( true );
		printk( "odin memory buffer, handle log is enabled\n" );
	}else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') ){
		set_odin_debug_bh_message( false );
		printk( "odin memory buffer, handle log is disabled\n" );
	}else{
		printk( "what?" );
	}

	return count;

}

static DEVICE_ATTR( bh_msg, 0664,
							show_debug_bh_message,
							store_debug_bh_message );

/* View memory ========================================== */

static bool odin_mem_debug_view_message_onoff = false;

bool get_odin_debug_v_message( void )
{

	return odin_mem_debug_view_message_onoff;

}
EXPORT_SYMBOL(get_odin_debug_v_message);

void set_odin_debug_v_message( bool val )
{

	odin_mem_debug_view_message_onoff = val;

}
EXPORT_SYMBOL(set_odin_debug_v_message);

static ssize_t show_debug_v_message( struct device *dev,
										struct device_attribute *attr,
										char *buf )
{

	if ( get_odin_debug_v_message() == false )
		printk( "odin memory view message : off\n"   );
	else if ( get_odin_debug_v_message() == true )
		printk( "odin memory view message : on\n"   );

	return 0;

}

static ssize_t store_debug_v_message( struct device *dev,
										struct device_attribute *attr,
										const char *buf, size_t count )
{

	if ( (*(buf+0)=='o') && (*(buf+1)=='n') ){
		set_odin_debug_v_message( true );
		printk( "odin memory view log is enabled\n" );
	}else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') ){
		set_odin_debug_v_message( false );
		printk( "odin memory view log is disabled\n" );
	}else{
		printk( "what?" );
	}

	return count;

}

static DEVICE_ATTR( v_msg, 0664,
							show_debug_v_message,
							store_debug_v_message );

/* nen-View memory ====================================== */

static bool odin_mem_debug_non_view_message_onoff = false;


bool get_odin_debug_nv_message( void )
{

	return odin_mem_debug_non_view_message_onoff;

}
EXPORT_SYMBOL(get_odin_debug_nv_message);

void set_odin_debug_nv_message( bool val )
{

	odin_mem_debug_non_view_message_onoff = val;

}
EXPORT_SYMBOL(set_odin_debug_nv_message);

static ssize_t show_debug_nv_message( struct device *dev,
										struct device_attribute *attr,
										char *buf )
{

	if ( get_odin_debug_nv_message() == false )
		printk( "odin memory non view message : off\n"   );
	else if ( get_odin_debug_nv_message() == true )
		printk( "odin memory non view message : on\n"   );

	return 0;

}

static ssize_t store_debug_nv_message( struct device *dev,
										struct device_attribute *attr,
										const char *buf, size_t count )
{

	if ( (*(buf+0)=='o') && (*(buf+1)=='n') ){
		set_odin_debug_nv_message( true );
		printk( "odin memory non view log is enabled\n" );
	}else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') ){
		set_odin_debug_nv_message( false );
		printk( "odin memory non view log is disabled\n" );
	}else{
		printk( "what?" );
	}

	return count;

}

static DEVICE_ATTR( nv_msg, 0664,
							show_debug_nv_message,
							store_debug_nv_message );

/* init / exit functions ===================================== */

static int __init odin_mem_debug_init(void)
{

	struct class *cls;
	struct device *dev;
	int ret;


	cls = class_create( THIS_MODULE, "odin_mem" );
	if ( IS_ERR( cls ) ){
		return PTR_ERR( cls );
	}

	dev = device_create( cls, NULL, 0, NULL, "dbg" );
	if ( IS_ERR( dev ) ){
		class_destroy( cls );
		return PTR_ERR( dev );
	}

	ret = device_create_file( dev, &dev_attr_bh_msg );
	if ( ret != 0 ){
		/*device_destroy( cls, dev ); change with MKDEV() */
		class_destroy( cls );
		return -ENODEV;
	}

	ret = device_create_file( dev, &dev_attr_v_msg );
	if ( ret != 0 ){
		/*device_destroy( cls, dev ); change with MKDEV() */
		class_destroy( cls );
		return -ENODEV;
	}

	ret = device_create_file( dev, &dev_attr_nv_msg );
	if ( ret != 0 ){
		/*device_destroy( cls, dev ); change with MKDEV() */
		class_destroy( cls );
		return -ENODEV;
	}

	ret = device_create_file( dev, &dev_attr_ion_unused_buffer );
	if ( ret != 0 ){
		/*device_destroy( cls, dev ); change with MKDEV() */
		class_destroy( cls );
		return -ENODEV;
	}

	ret = device_create_file( dev, &dev_attr_ion_used_buffer );
	if ( ret != 0 ){
		/*device_destroy( cls, dev ); change with MKDEV() */
		class_destroy( cls );
		return -ENODEV;
	}

	ret = device_create_file( dev, &dev_attr_ion_used_buffer_sorted );
	if ( ret != 0 ){
		/*device_destroy( cls, dev ); change with MKDEV() */
		class_destroy( cls );
		return -ENODEV;
	}

	return 0;

}

static void __exit odin_mem_debug_exit(void)
{

	/*
	Which major number I can use??
	device_destroy( cls, MKDEV(,) );
	class_destroy( cls );
	*/

}

module_init( odin_mem_debug_init );
module_exit( odin_mem_debug_exit );

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hyunsun Hong <hyunsun.hong@lge.com>");



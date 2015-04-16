/*
 * u_serial.h - interface to USB gadget "serial port"/TTY utilities
 *
 * Copyright (C) 2008 David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 *
 * This software is distributed under the terms of the GNU General
 * Public License ("GPL") as published by the Free Software Foundation,
 * either version 2 of that License or (at your option) any later version.
 */

#ifndef __U_SERIAL_H
#define __U_SERIAL_H

#include <linux/usb/composite.h>
#include <linux/usb/cdc.h>
//balong usb+++
#include "usb_vendor.h"
#define U_ACM_CTRL_DTR	(1 << 0)
#define U_ACM_CTRL_RTS	(1 << 1)
#define U_ACM_CTRL_RING (1 << 3)
//balong usb---

struct f_serial_opts {
	struct usb_function_instance func_inst;
	u8 port_num;
};

/*
 * One non-multiplexed "serial" I/O port ... there can be several of these
 * on any given USB peripheral device, if it provides enough endpoints.
 *
 * The "u_serial" utility component exists to do one thing:  manage TTY
 * style I/O using the USB peripheral endpoints listed here, including
 * hookups to sysfs and /dev for each logical "tty" device.
 *
 * REVISIT at least ACM could support tiocmget() if needed.
 *
 * REVISIT someday, allow multiplexing several TTYs over these endpoints.
 */
struct gserial {
	struct usb_function		func;

	/* port is managed by gserial_{connect,disconnect} */
#ifdef CONFIG_K3V3_BALONG_MODEM
	void                    *ioport;
#else
	struct gs_port			*ioport;
#endif

	struct usb_ep			*in;
	struct usb_ep			*out;

	/* REVISIT avoid this CDC-ACM support harder ... */
	struct usb_cdc_line_coding port_line_coding;	/* 9600-8-N-1 etc */

	/* notification callbacks */
	void (*connect)(struct gserial *p);
	void (*disconnect)(struct gserial *p);
#ifdef CONFIG_K3V3_BALONG_MODEM
	void (*notify_state)(struct gserial *p, u16 state);
	void (*flow_control)(struct gserial *p, u32 rx_is_on, u32 tx_is_on);
#endif
	int (*send_break)(struct gserial *p, int duration);
};
//balong usb+++

struct acm_name_type_tbl {
	char *name;
	USB_PID_UNIFY_IF_PROT_T type;
};

enum acm_class_type {
	acm_class_cdev,
	acm_class_tty,
	acm_class_modem,
	acm_class_unknown		/* last item */
};
//balong usb---
/* utilities to allocate/free request and buffer */
struct usb_request *gs_alloc_req(struct usb_ep *ep, unsigned len, gfp_t flags);
void gs_free_req(struct usb_ep *, struct usb_request *req);

/* management of individual TTY ports */
int gserial_alloc_line(unsigned char *port_line);
void gserial_free_line(unsigned char port_line);

/* connect/disconnect is handled by individual functions */
int gserial_connect(struct gserial *, u8 port_num);
void gserial_disconnect(struct gserial *);

/* functions are bound to configurations by a config or gadget driver */
#ifdef CONFIG_K3V3_BALONG_MODEM
int acm_bind_config(struct usb_configuration *c, u32 port_num, enum acm_class_type type);
#else
int acm_bind_config(struct usb_configuration *c, u8 port_num);
#endif
int gser_bind_config(struct usb_configuration *c, u8 port_num);
int obex_bind_config(struct usb_configuration *c, u8 port_num);

#ifdef CONFIG_K3V3_BALONG_MODEM
/* -------------- cdev for acm --------------- */
int gacm_cdev_setup(struct usb_gadget *g, unsigned n_ports);
void gacm_cdev_cleanup(void);

int gacm_cdev_connect(struct gserial *, u8 port_num);
void gacm_cdev_disconnect(struct gserial *);

int gacm_cdev_line_state(struct gserial *, u8 port_num, u32 state);

/* -------------- modem for acm --------------- */
int gacm_modem_line_state(struct gserial *gser, u8 port_num, u32 state);
void gacm_modem_disconnect(struct gserial *gser);
int gacm_modem_connect(struct gserial *gser, u8 port_num);

/* -------------- acm support cap --------------- */
/* max dev driver support */
#define ACM_CDEV_COUNT      9
#define MAX_U_SERIAL_PORTS       7
#define ACM_MDM_COUNT       2

/* dev count we used (the value must < ACM_XXX_COUNT) */
/* modify to 5 for cshell */
#define ACM_CDEV_USED_COUNT      7
#define ACM_TTY_USED_COUNT       1
#define ACM_MDM_USED_COUNT       1

#if ACM_CDEV_USED_COUNT >= ACM_CDEV_COUNT
    #error "ACM_CDEV_USED_COUNT error"
#endif

#if ACM_TTY_USED_COUNT >= MAX_U_SERIAL_PORTS
    #error "ACM_TTY_USED_COUNT error"
#endif

#if ACM_MDM_USED_COUNT >= ACM_MDM_COUNT
    #error "ACM_MDM_COUNT error"
#endif

/* acm config ... */
#define ACM_IS_SINGLE_INTF          1
/* acm cdev config ... */
#define ACM_CDEV_SUPPORT_NOTIFY     0
/* acm tty config ... */
#define ACM_CONSOLE_IDX     0
#define ACM_CONSOLE_NAME    "uw_tty"
#define ACM_TTY_SUPPORT_NOTIFY      0

/* we can change the console enable checker here */
#define ACM_IS_CONSOLE_ENABLE() gs_acm_is_console_enable()



/* acm modem config ... */
#define ACM_MODEM_SG_LIST_NUM       64
#define ACM_MODEM_SG_LIST_ITEM_NUM  64
#define ACM_MODEM_SUPPORT_NOTIFY    1

/* --- acm evt managment --- */
struct gs_acm_evt_manage {
     void* port_evt_array[ACM_CDEV_COUNT + 1];
     int port_evt_pos;
     char* name;
     spinlock_t evt_lock;
};


static inline void gs_acm_evt_init(struct gs_acm_evt_manage* evt, char* name)
{
    spin_lock_init(&evt->evt_lock);
    evt->port_evt_pos = 0;
    evt->name = name;
    memset(evt->port_evt_array, 0, sizeof(evt->port_evt_array));
}

#define gs_acm_evt_push(port, evt) __gs_acm_evt_push((void*)port, evt)
static inline void __gs_acm_evt_push(void* port, struct gs_acm_evt_manage* evt)
{
    unsigned long flags;
    int add_new = 1;
    int i;

    spin_lock_irqsave(&evt->evt_lock, flags);
    for (i = 0; i <= evt->port_evt_pos; i++) {
        if (evt->port_evt_array[i] == port) {
            add_new = 0;
            break;
        }
    }
    if (add_new) {
        evt->port_evt_array[evt->port_evt_pos] =  port;
        evt->port_evt_pos++;
    }
    spin_unlock_irqrestore(&evt->evt_lock, flags);
}

static inline void* gs_acm_evt_get(struct gs_acm_evt_manage* evt)
{
    unsigned long flags;
    struct gs_acm_cdev_port* ret_port = NULL;

    spin_lock_irqsave(&evt->evt_lock, flags);
    if (evt->port_evt_pos > 0) {
        ret_port = evt->port_evt_array[evt->port_evt_pos-1];
        evt->port_evt_array[evt->port_evt_pos-1] = NULL;
        evt->port_evt_pos--;
    }
    spin_unlock_irqrestore(&evt->evt_lock, flags);

    return ret_port;
}

static inline void gs_acm_evt_dump_info(struct gs_acm_evt_manage* evt)
{
    int i;
    struct gs_acm_cdev_port* port;

    pr_emerg("--- dump evt_pos:%d, name:%s ---\n",
        evt->port_evt_pos, (evt->name) ? (evt->name) : ("NULL"));
    for (i = 0; i <= evt->port_evt_pos; i++) {
        port = evt->port_evt_array[i];
        if (port) {
            pr_emerg("port[%d]_0x%x : %s\n", (u32)port,
                evt->port_evt_pos, (evt->name) ? (evt->name) : ("NULL"));
        }
    }
}

#endif
#endif /* __U_SERIAL_H */

/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * Author: Wonsuk Chang <wonsuk.chang@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>
#include <linux/usb/composite.h>
#include <linux/usb/odin_usb3.h>
#ifdef CONFIG_CPU_IDLE_ODIN_WORKAROUND
#include <linux/odin_cpuidle.h>
#endif
#ifdef CONFIG_USB_G_LGE_ANDROID
#include <linux/board_lge.h>
#endif

#include "odin_udc.h"

static char *odin_udc_ep0_state[] = {
	[EP0_IDLE] = "EP0_IDLE",
	[EP0_IN_DATA_PHASE]	= "EP0_IN_DATA_PHASE",
	[EP0_OUT_DATA_PHASE] = "EP0_OUT_DATA_PHASE",
	[EP0_IN_WAIT_GADGET] = "EP0_IN_WAIT_GADGET",
	[EP0_OUT_WAIT_GADGET] = "EP0_OUT_WAIT_GADGET",
	[EP0_IN_WAIT_NRDY]= "EP0_IN_WAIT_NRDY",
	[EP0_OUT_WAIT_NRDY]= "EP0_OUT_WAIT_NRDY",
	[EP0_IN_STATUS_PHASE] = "EP0_IN_STATUS_PHASE",
	[EP0_OUT_STATUS_PHASE] = "EP0_OUT_STATUS_PHASE",
	[EP0_STALL] = "EP0_STALL",
};

#ifdef CONFIG_USB_G_LGE_ANDROID
static void odin_udc_disconnect_intr(struct odin_udc *udc);
#endif

struct odin_usb3_req *odin_udc_get_request(struct odin_udc *udc,
						struct odin_udc_ep *odin_ep)
{
	struct odin_usb3_req *req;

	if (list_empty(&odin_ep->queue)) {
		ug_dbg("[%s] ep->dwc_ep.queue empty!\n", __func__);
		return NULL;
	}

	req = list_first_entry(&odin_ep->queue, struct odin_usb3_req, entry);
	return req;
}

static void odin_udc_connect(struct odin_udc *udc, int connect)
{
	static int connect_save = 0;

	ud_udc("[%s] ==> %s\n", __func__, connect?"Connect":"Disconnect");
	if (connect_save != connect) {
		if (connect) {
			ud_udc("Wake-lock\n");
			wake_lock(&udc->wake_lock);
		} else {
			ud_udc("Wake-lock timeout\n");
			wake_lock_timeout(&udc->wake_lock, HZ / 2);
		}
		connect_save = connect;
	}
}

static void odin_udc_ep_rst_flags(struct odin_udc_ep *odin_ep)
{
	odin_ep->stopped = 1;
	odin_ep->send_zlp = 0;
	odin_ep->three_stage = 0;
	odin_ep->xfer_started = 0;
	odin_ep->active = 0;
	odin_ep->stream_capable = 0;
	odin_ep->pending_req = 0;
	odin_ep->wedge = 0;
}

static void odin_udc_test_mode(struct odin_udc *udc)
{
	u32 dctl = readl(udc->regs + ODIN_USB3_DCTL);
	dctl &= ~ODIN_USB3_DCTL_TSTCTL_MASK;
	dctl |= ODIN_USB3_DCTL_TSTCTL(udc->test_mode_seletor);
	writel(dctl, udc->regs + ODIN_USB3_DCTL);
}

void odin_udc_start_next_request(struct odin_udc *udc,
						struct odin_udc_ep *odin_ep)
{
	struct odin_usb3_req *req = NULL;
	struct list_head *list_item;

	ug_dbg("%s(%p)\n", __func__, odin_ep);

	if (list_empty(&odin_ep->queue)) {
		ud_udc("start_next EP%d-%s: queue empty\n",
			odin_ep->num, odin_ep->is_in ? "IN" : "OUT");

		if (odin_ep->type == USB_ENDPOINT_XFER_ISOC) {
			ud_iso("[%s] queue empty - flag \n", __func__);
			odin_ep->pending_req = 1;
		}
		return;
	}

	list_for_each (list_item, &odin_ep->queue) {
		req = list_entry(list_item, struct odin_usb3_req, entry);
		if (!(req->flags & ODIN_USB3_REQ_STARTED)) {
			if (odin_ep->desc_avail <= 0) {
				return;
			}

			ud_udc("start_next EP%d %s: OK\n",
						odin_ep->num,
						odin_ep->is_in ? "IN" : "OUT");
			odin_udc_fill_trbs(udc, odin_ep, req);
			odin_udc_ep_start_transfer(udc, odin_ep, req, 0);
			return;
		}
	}
}

void odin_udc_request_complete(struct odin_udc *udc,
	struct odin_udc_ep *odin_ep, struct odin_usb3_req *req, int status)
{
	unsigned stopped = odin_ep->stopped;
	struct odin_udc_trb *trb;
	struct usb_ep *usb_ep = &odin_ep->ep;
	struct usb_request *usb_req;
	int *req_flags;

	if (!req) {
		dev_warn(udc->dev, "no request\n");
		return;
	}

	req_flags = &req->flags;
	trb = req->trb;
	usb_req = &req->usb_req;

	if (odin_ep != udc->ep0) {
		req->flags &= ~ODIN_USB3_REQ_STARTED;
		if (trb) {
			trb->control &= ~ODIN_USB3_DSCCTL_HWO;
			wmb();
			odin_ep->desc_avail++;
			trb = NULL;
			req->trb = NULL;
		}
	}

	/* don't modify queue heads during completion callback */
	odin_ep->stopped = 1;

	list_del_init(&req->entry);

	if (*req_flags & ODIN_USB3_REQ_MAP_DMA) {
		ug_dbg("DMA unmap req %p\n", usb_req);
		if (*req_flags & ODIN_USB3_REQ_IN)
			usb_gadget_unmap_request(&udc->gadget, usb_req, 1);
		else
			usb_gadget_unmap_request(&udc->gadget, usb_req, 0);

		*req_flags &= ~(ODIN_USB3_REQ_MAP_DMA);
		usb_req->dma = ODIN_USB3_DMA_ADDR_INVALID;
	}

	odin_ep->pending_req = 0;

	if (usb_req->complete) {
		usb_req->status = status;
		usb_req->actual = req->actual;
		spin_unlock(&udc->lock);
		usb_req->complete(usb_ep, usb_req);
		spin_lock(&udc->lock);
	}

	odin_ep->stopped = stopped;
}

void odin_udc_request_nuke(struct odin_udc *udc,
					struct odin_udc_ep *odin_ep)
{
	struct odin_usb3_req *req;

	odin_ep->stopped = 1;

	/* called with irqs blocked?? */
	while (!list_empty(&odin_ep->queue)) {
		req = list_first_entry(&odin_ep->queue, struct odin_usb3_req,
				       entry);
		odin_udc_request_complete(udc, odin_ep, req, -ESHUTDOWN);
	}

	if (odin_ep != udc->ep0) {
		odin_ep->desc_idx = 0;
	}
}

static int odin_udc_ep_enable(struct usb_ep *usb_ep,
		     const struct usb_endpoint_descriptor *ep_desc)
{
	struct odin_udc_ep *odin_ep;
	struct odin_udc *udc;
	struct odin_udc_trb *trbs, *cur_trb;
	dma_addr_t trbs_dma;
	int num_trbs, type, size, i;
	unsigned long flags;
	int ret = 0;
	void *mem;
#ifdef CONFIG_CPU_IDLE_ODIN_WORKAROUND
	cpumask_var_t cpu_val;
#endif

	if (!usb_ep || !ep_desc ||
	    ep_desc->bDescriptorType != USB_DT_ENDPOINT) {
		usb_err("%s, bad ep or descriptor %p %p %d!\n",
		       __func__, usb_ep, ep_desc,
		       ep_desc ? ep_desc->bDescriptorType : 0);
		return -EINVAL;
	}

	ud_core("[%s]%s(%p,%p)\n", __func__, usb_ep->name, usb_ep, ep_desc);

	if (!ep_desc->wMaxPacketSize) {
		usb_err("%s, zero %s wMaxPacketSize!\n",
		       __func__, usb_ep->name);
		return -ERANGE;
	}

	odin_ep = odin_usb3_get_ep(usb_ep);
	udc = odin_ep->udc;

	if (!udc->driver ||
	    udc->gadget.speed == USB_SPEED_UNKNOWN) {
		ud_core("%s, bogus device state!\n", __func__);
		return -ESHUTDOWN;
	}

	/* Free any existing TRB allocation for this EP */
	odin_udc_trb_free(odin_ep);

	ud_core("[%s]ep%d-%s=%p phys=%d odin_ep=%p\n",
		  __func__, odin_ep->num, odin_ep->is_in ? "IN" : "OUT",
		  usb_ep, odin_ep->phys, odin_ep);

	spin_lock_irqsave(&udc->lock, flags);

	if (odin_ep->usb_ep_desc) {
		spin_unlock_irqrestore(&udc->lock, flags);
		usb_err("%s, bad ep or descriptor!\n", __func__);
		return -EINVAL;
	}

	type = usb_endpoint_type(ep_desc);
	usb_ep->maxpacket = usb_endpoint_maxp(ep_desc);

	if (type == USB_ENDPOINT_XFER_ISOC) {
		num_trbs = ODIN_USB3_NUM_ISOC_TRBS;
		size = (num_trbs + 1) * 16;
	} else {
		num_trbs = 1;
		size = 16;
	}

	mem = dma_alloc_coherent(NULL, size, &trbs_dma,
				 GFP_ATOMIC | GFP_DMA32);
	if (!mem)
		return -ENOMEM;

	memset(mem, 0, size);
	trbs = cur_trb = mem;

	switch (type) {
	case USB_ENDPOINT_XFER_ISOC:
		#ifdef CONFIG_CPU_IDLE_ODIN_WORKAROUND
		odin_idle_disable(USB_CPU_AFFINITY, 1);
		cpumask_clear(cpu_val);
		cpumask_set_cpu(USB_CPU_AFFINITY, cpu_val);
		irq_set_affinity(udc->irq, cpu_val);
		udc->cpu_handle = 1;
		#endif

		odin_ep->intvl = ep_desc->bInterval - 1;

		for (i = 0; i < ODIN_USB3_NUM_ISOC_TRBS; i++, cur_trb++) {
			odin_udc_fill_trb(cur_trb, 0, 0,
					ODIN_USB3_DSCCTL_TRBCTL_ISOC_1ST,
					ODIN_USB3_DSCCTL_IOC |
					ODIN_USB3_DSCCTL_IMI |
					ODIN_USB3_DSCCTL_CSP, 0);
		}

		odin_udc_fill_trb(cur_trb, trbs_dma, 0,
					ODIN_USB3_DSCCTL_TRBCTL_LINK, 0, 1);
		break;

	case USB_ENDPOINT_XFER_INT:
		odin_ep->intvl = ep_desc->bInterval - 1;
		break;

	default:
		odin_ep->intvl = 0;
		break;
	}

	odin_ep->dma_desc = trbs;
	odin_ep->dma_desc_dma = trbs_dma;
	odin_ep->desc_size = size;
	odin_ep->num_desc = num_trbs;
	odin_ep->desc_avail = num_trbs;
	odin_ep->desc_idx = 0;
	odin_ep->usb_ep_desc = ep_desc;
	odin_ep->stopped = 0;
	odin_ep->type = type;
	odin_ep->maxpacket = usb_ep->maxpacket & 0x7ff;
	odin_ep->xfer_started = 0;

	ret = odin_udc_ep_activate(udc, odin_ep, usb_ep->comp_desc);
	if (ret) {
		dev_err(udc->dev, "EP%d-%s active err = %d\n",
			odin_ep->num, (odin_ep->is_in ? "IN" : "OUT"), ret);
		goto error;
	}
	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;

error:
	odin_udc_trb_free(odin_ep);
	spin_unlock_irqrestore(&udc->lock, flags);
	return ret;
}

static int odin_udc_ep_disable(struct usb_ep *usb_ep)
{
	struct odin_udc_ep *odin_ep;
	struct odin_udc *udc;
	u8 tri;
	u32 ep_index_num, dalepena;
	unsigned long flags;
	int ret = 0;

	if (!usb_ep) {
		usb_err("EP not enabled!\n");
		return -EINVAL;
	}
	odin_ep = odin_usb3_get_ep(usb_ep);
	udc = odin_ep->udc;

	spin_lock_irqsave(&udc->lock, flags);

	if (!odin_ep->usb_ep_desc) {
		spin_unlock_irqrestore(&udc->lock, flags);
		usb_err("%s, bad ep!\n", __func__);
		return -EINVAL;
	}

	ud_core("%s() EP%d-%s\n", __func__, odin_ep->num,
		   (odin_ep->is_in ? "IN" : "OUT"));

	if (odin_ep->is_in) {
		tri = odin_ep->tri_in;
		odin_ep->tri_in = 0;
		ep_index_num = odin_ep->num * 2 + 1;
	} else {
		tri = odin_ep->tri_out;
		odin_ep->tri_out = 0;
		ep_index_num = odin_ep->num * 2;
	}

	ud_udc("end: DWC EP=%lx tri=%d\n", (unsigned long)odin_ep, tri);

	if (tri) {
		odin_udc_epcmd_clr_stall(udc, odin_ep);
		odin_udc_epcmd_end_xfer(udc, odin_ep,
				tri - 1, ODIN_USB3_ENDXFER_FORCE);
	}

	dalepena = readl(udc->regs + ODIN_USB3_DALEPENA);
	if (dalepena & 1 << ep_index_num) {
		dalepena &= ~(1 << ep_index_num);
		writel(dalepena, udc->regs + ODIN_USB3_DALEPENA);
	}

	odin_udc_request_nuke(udc, odin_ep);
	odin_ep->usb_ep_desc = NULL;

	odin_udc_ep_rst_flags(odin_ep);

	spin_unlock_irqrestore(&udc->lock, flags);

	odin_udc_trb_free(odin_ep);

	return ret;
}

static void odin_udc_complete_request(struct odin_udc *udc,
	struct odin_udc_ep *odin_ep, u32 event)
{
	struct odin_usb3_req *req;
	struct odin_udc_trb *trb;
	u32 byte_count;

	req = odin_udc_get_request(udc, odin_ep);
	if (!req) {
		dev_warn(udc->dev, "%s(%lx), ep->dwc_ep.queue empty!\n",
			   __func__, (unsigned long)odin_ep);
		return;
	}
	ud_udc("[%s] req = %lx, pending_req = %d\n", __func__,
					(unsigned long)req, odin_ep->pending_req);

	odin_ep->send_zlp = 0;
	trb = req->trb;

	if (!trb) {
		dev_err(udc->dev, "[%s] EP%d-%s request TRB is NULL!\n",
			     __func__, odin_ep->num, odin_ep->is_in ?
			     "IN" : "OUT");
		return;
	}

	if (!(req->flags & ODIN_USB3_REQ_STARTED)) {
		if (odin_ep->type == USB_ENDPOINT_XFER_ISOC &&
		    (event & ODIN_USB3_DEPEVT_EVENT_MASK) ==
				ODIN_USB3_DEPEVT_XFER_IN_PROG &&
		    odin_ep->xfer_started == 0) {
			goto done;
		}
		dev_err(udc->dev, "[%s] EP%d-%s request not started!\n",
			     __func__, odin_ep->num, odin_ep->is_in ?
			     "IN" : "OUT");
		return;
	}

	if (trb->control & ODIN_USB3_DSCCTL_HWO) {
		dev_err(udc->dev, "[%s] EP%d-%s HWO bit set!\n",
			     __func__, odin_ep->num, odin_ep->is_in ?
			     "IN" : "OUT");
	}

	if ((odin_ep->type == USB_ENDPOINT_XFER_ISOC) &&
		ODIN_USB3_DSCSTS_LENGTH(trb->status) &&
		(ODIN_USB3_DSCSTS_TRBRSP(trb->status) &
			ODIN_USB3_TRBRSP_MISSED_ISOC_IN)) {
		iso_dbg("Missed ISO!");
		req->actual = 0;
		goto done;
	}

	if (odin_ep->is_in) {
		if (ODIN_USB3_DSCSTS_LENGTH(trb->status) == 0) {
			req->actual += req->length;
		}
	} else {
		byte_count = req->length -
				ODIN_USB3_DSCSTS_LENGTH(trb->status);
		req->actual += byte_count;
	}

	if (req->usb_req.zero &&
			odin_ep->is_in &&
			req->length == req->actual &&
			req->length &&
			!(req->length % odin_ep->maxpacket)) {
		ud_core("[%s] usb ep%d-%s is needed zlp?\n",
			__func__, odin_ep->num, odin_ep->is_in ? "IN" : "OUT");
	}

done:
	if ((event & ODIN_USB3_DEPEVT_EVENT_MASK) ==
				ODIN_USB3_DEPEVT_XFER_CMPL) {
		if (odin_ep->is_in)
			odin_ep->tri_in = 0;
		else
			odin_ep->tri_out = 0;
	}

	odin_udc_request_complete(udc, odin_ep, req, 0);
	if (odin_ep->type != USB_ENDPOINT_XFER_ISOC ||
					odin_ep->xfer_started)
		/* If there is a request in the queue start it. */
		odin_udc_start_next_request(udc, odin_ep);
}

int odin_udc_ep0_submit_req(struct odin_udc *udc,
		struct odin_udc_ep *odin_ep, struct odin_usb3_req *req, int req_flags)
{
	switch (udc->ep0state) {
	case EP0_IN_DATA_PHASE:
		ud_udc("[%s] ep0: EP0_IN_DATA_PHASE\n", __func__);
		break;

	case EP0_OUT_DATA_PHASE:
		ud_udc("[%s] ep0: EP0_OUT_DATA_PHASE\n", __func__);
		if (udc->request_config) {
			/* Complete STATUS PHASE */
			odin_ep->is_in = 1;
			udc->ep0state = EP0_IN_WAIT_NRDY;
			return 1;
		}
		break;

	case EP0_IN_WAIT_GADGET:
		ud_udc("[%s] ep0: EP0_IN_WAIT_GADGET\n", __func__);
		udc->ep0state = EP0_IN_WAIT_NRDY;
		return 2;

	case EP0_OUT_WAIT_GADGET:
		ud_udc("[%s] ep0: EP0_OUT_WAIT_GADGET\n", __func__);
		udc->ep0state = EP0_OUT_WAIT_NRDY;
		return 3;

	case EP0_IN_WAIT_NRDY:
		ud_udc("[%s] ep0: EP0_IN_WAIT_NRDY\n", __func__);
		udc->ep0state = EP0_IN_STATUS_PHASE;
		break;

	case EP0_OUT_WAIT_NRDY:
		ud_udc("%s ep0: EP0_OUT_WAIT_NRDY\n", __func__);
		udc->ep0state = EP0_OUT_STATUS_PHASE;
		break;

	default:
		usb_err("%s ep0: odd state %d!\n", __func__, udc->ep0state);
		return -ESHUTDOWN;
	}

	odin_ep->send_zlp = 0;

	if ((req_flags & ODIN_USB3_REQ_ZERO) &&
		req->length &&
		!(req->length % odin_ep->maxpacket)) {
		odin_ep->send_zlp = 1;
		ud_core("[%s]send_zlp SET!\n", __func__);
	}

	odin_udc_ep0_start_transfer(udc, req);
	return 0;
}

static struct usb_request *odin_udc_alloc_request
				(struct usb_ep *usb_ep, gfp_t gfp_flags)
{
	struct odin_usb3_req *req;

	if (!usb_ep) {
		usb_err("%s() - Invalid EP!\n", __func__);
		return NULL;
	}

	req = kmalloc(sizeof(struct odin_usb3_req), gfp_flags);
	if (!req) {
		usb_err("%s() udc request allocation failed!\n", __func__);
		return NULL;
	}

	memset(req, 0, sizeof(struct odin_usb3_req));
	req->usb_req.dma = ODIN_USB3_DMA_ADDR_INVALID;

	return &req->usb_req;
}

static void odin_udc_free_request
			(struct usb_ep *usb_ep, struct usb_request *usb_req)
{
	struct odin_usb3_req *req;

	if (!usb_ep || !usb_req) {
		usb_err("%s()Invalid ep or req argument!\n", __func__);
		return;
	}

	req = odin_usb3_get_req(usb_req);
	kfree(req);
}

static int odin_udc_ep_queue(struct usb_ep *usb_ep,
				struct usb_request *usb_req, gfp_t gfp_flags)
{
	struct odin_udc *udc;
	struct odin_udc_ep *odin_ep;
	struct odin_usb3_req *req;
	unsigned long flags;
	struct list_head *list_item;
	int ret = 0, req_flags = 0;

	if (!usb_req || !usb_req->complete || !usb_req->buf || !usb_ep) {
		usb_err("%s, bad params\n", __func__);
		return -EINVAL;
	}

	iso_dbg("EP Queue");
	req = odin_usb3_get_req(usb_req);
	odin_ep = odin_usb3_get_ep(usb_ep);
	udc = odin_ep->udc;

	if (!udc->driver || udc->gadget.speed == USB_SPEED_UNKNOWN) {
		ud_core("%s, bogus device state!\n", __func__);
		return -ESHUTDOWN;
	}

	usb_req->status = -EINPROGRESS;
	usb_req->actual = 0;

	if (usb_req->zero) {
		ud_core("[%s] usb ep%d-%s zero request \n",
			__func__, odin_ep->num, odin_ep->is_in ? "IN" : "OUT");
		req_flags |= ODIN_USB3_REQ_ZERO;
	}

	if (usb_req->length != 0 && usb_req->dma == ODIN_USB3_DMA_ADDR_INVALID) {
		usb_gadget_map_request(&udc->gadget, usb_req, odin_ep->is_in);
		req_flags |= ODIN_USB3_REQ_MAP_DMA;
		if (odin_ep->is_in)
			req_flags|= ODIN_USB3_REQ_IN;
	}

	spin_lock_irqsave(&udc->lock, flags);

	if (odin_ep->num != 0 && !odin_ep->usb_ep_desc) {
		spin_unlock_irqrestore(&udc->lock, flags);
		usb_err("%s, bad ep!\n", __func__);
		return -EINVAL;
	}

	ud_udc("%s: EP%d-%s %p stream %d req %p\n",
		  __func__, odin_ep->num, odin_ep->is_in ? "IN" : "OUT",
		  odin_ep, usb_req->stream_id, req);

	INIT_LIST_HEAD(&req->entry);

	req->length = 0;
	req->buf = usb_req->buf;
	req->bufdma = usb_req->dma;
	req->buflen = usb_req->length;
	req->length += usb_req->length;
	req->actual = 0;
	req->flags = req_flags;

	if (odin_ep == udc->ep0 && odin_ep->is_in) {
		/* TODO : Fix me
		req->flags |= ODIN_USB3_REQ_ZERO;*/
	}

	if (!list_empty(&odin_ep->queue)) {
		ud_udc(" q not empty \n");

		if (!odin_ep->stopped &&
		    odin_ep->type == USB_ENDPOINT_XFER_ISOC &&
		    odin_ep->desc_avail > 0 &&
		    odin_ep->xfer_started)
			goto do_start;

	} else if (odin_ep->type != USB_ENDPOINT_XFER_ISOC &&
			(odin_ep->stopped ||
			(odin_ep != udc->ep0 && odin_ep->desc_avail <= 0))) {
		ud_udc("[%s] pend queue \n", __func__);
	} else if (odin_ep->type == USB_ENDPOINT_XFER_ISOC &&
						odin_ep->pending_req) {
		list_add_tail(&req->entry, &odin_ep->queue);

		if (odin_ep->xfer_started) {
			ud_iso("[%s] ISOC pending req -started -> STOP \n", __func__);
			odin_udc_iso_xfer_stop(udc, odin_ep);
			odin_ep->desc_avail = ODIN_USB3_NUM_ISOC_TRBS;

		} else {
			ud_iso("[%s] ISOC pending req -not started -> START \n", __func__);
			list_for_each (list_item, &odin_ep->queue) {
				req = list_entry(list_item, struct odin_usb3_req, entry);
				if (req->flags & ODIN_USB3_REQ_STARTED)
					req = NULL;
				else
					break;
			}

			if (!req) {
				spin_unlock_irqrestore(&udc->lock, flags);
				return 0;
			}

			iso_dbg("**Queue Start**");
			odin_udc_fill_trbs(udc, odin_ep, req);
			odin_udc_ep_start_transfer(udc, odin_ep,
									req, odin_ep->last_event);
		}

		odin_ep->pending_req = 0;
		spin_unlock_irqrestore(&udc->lock, flags);
		return 0;
	} else {
do_start:
		odin_udc_fill_trbs(udc, odin_ep, req);
		if (odin_ep == udc->ep0)
			ret = odin_udc_ep0_submit_req(udc, odin_ep, req,
						    req_flags);
		else
			odin_udc_ep_start_transfer(udc, odin_ep, req, 0);
	}

	if (!ret) {
		list_add_tail(&req->entry, &odin_ep->queue);
		odin_ep->pending_req = 1;
	}

	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;
}

static int odin_udc_ep_dequeue(struct usb_ep *usb_ep,
					struct usb_request *usb_req)
{
	struct odin_usb3_req *req = NULL;
	struct odin_udc_ep *odin_ep;
	struct odin_udc *udc;
	struct list_head *list_item;
	unsigned long flags;
	bool match = false;

	if (!usb_ep || !usb_req) {
		usb_err("%s, bad argument!\n", __func__);
		return -EINVAL;
	}

	req = odin_usb3_get_req(usb_req);
	odin_ep = odin_usb3_get_ep(usb_ep);
	udc = odin_ep->udc;

	if (!udc->driver ||
	    udc->gadget.speed == USB_SPEED_UNKNOWN) {
		ud_core("%s, bogus device state!\n", __func__);
		return -ESHUTDOWN;
	}

	spin_lock_irqsave(&udc->lock, flags);

	if (odin_ep->num != 0 && !odin_ep->usb_ep_desc) {
		spin_unlock_irqrestore(&udc->lock, flags);
		usb_err("%s, bad odin_ep!\n", __func__);
		return -EINVAL;
	}

	if (!req) {
		spin_unlock_irqrestore(&udc->lock, flags);
		usb_err("%s, no request in queue!\n", __func__);
		return 0;
	}

	/* make sure it's actually queued on this EP */
	list_for_each (list_item, &odin_ep->queue) {
		req = list_entry(list_item, struct odin_usb3_req, entry);
		if (&req->usb_req == usb_req) {
			match = true;
			break;
		}
	}

	/* cancel an I/O request from an EP */
	odin_udc_stop_xfer(udc, odin_ep);

	odin_ep->xfer_started = 0;

	if(match) {
		odin_udc_request_complete(udc, odin_ep, req, -ECONNRESET);
	}

	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;
}

static int odin_udc_ep_set_halt(struct usb_ep *usb_ep, int value)
{
	struct odin_udc_ep *odin_ep;
	struct odin_udc *udc;
	unsigned long flags;

	if (!usb_ep) {
		usb_err("%s, bad usb_ep!\n", __func__);
		return -EINVAL;
	}

	odin_ep = odin_usb3_get_ep(usb_ep);
	udc = odin_ep->udc;

	spin_lock_irqsave(&udc->lock, flags);

	ud_core("epnum=%d is_in=%d\n",
		  odin_ep->num, odin_ep->is_in);

	if ((!odin_ep->usb_ep_desc && odin_ep->num != 0) ||
	    (odin_ep->usb_ep_desc && odin_ep->type
							== USB_ENDPOINT_XFER_ISOC)) {
		spin_unlock_irqrestore(&udc->lock, flags);
		usb_err("%s, bad odin_ep!\n", __func__);
		return -EINVAL;
	}

	if (!list_empty(&odin_ep->queue)) {
		spin_unlock_irqrestore(&udc->lock, flags);
		usb_err("%s, Xfer in process!\n", __func__);
		return -EAGAIN;
	}

	switch (value) {
	case 0:	/* clear_halt */
		odin_udc_epcmd_clr_stall(udc, odin_ep);

		if (odin_ep->stopped) {
			odin_ep->stopped = 0;

			/* If there is a request in the EP queue start it */
			if (odin_ep != udc->ep0 && odin_ep->is_in)
				odin_udc_start_next_request(udc, odin_ep);
		}
		odin_ep->wedge = 0;
		break;

	case 1:	/* set_halt */
		if (odin_ep == udc->ep0) {
			odin_ep->is_in = 0;
			odin_udc_epcmd_set_stall(udc, odin_ep);
			udc->ep0state = EP0_STALL;
		} else {
			odin_udc_epcmd_set_stall(udc, odin_ep);
		}
		odin_ep->stopped = 1;
		break;

	default:
		dev_warn(udc->dev, "[%s] Other case (%d)\n", __func__, value);
		break;
	}

	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;
}

static int odin_udc_ep_set_wedge(struct usb_ep *usb_ep)
{
	struct odin_udc_ep *odin_ep;
	struct odin_udc *udc;
	unsigned long flags;

	if (!usb_ep) {
		usb_err("%s, bad usb_ep!\n", __func__);
		return -EINVAL;
	}

	odin_ep = odin_usb3_get_ep(usb_ep);
	udc = odin_ep->udc;

	spin_lock_irqsave(&udc->lock, flags);
	odin_ep->wedge = 1;
	spin_unlock_irqrestore(&udc->lock, flags);

	return odin_udc_ep_set_halt(usb_ep, 1);
}

static struct usb_ep_ops odin_udc_ep_ops = {
	.enable		= odin_udc_ep_enable,
	.disable		= odin_udc_ep_disable,
	.alloc_request	= odin_udc_alloc_request,
	.free_request	= odin_udc_free_request,
	.queue		= odin_udc_ep_queue,
	.dequeue		= odin_udc_ep_dequeue,
	.set_halt		= odin_udc_ep_set_halt,
	.set_wedge	= odin_udc_ep_set_wedge,
};

int odin_udc_gadget_setup(struct odin_udc *udc,
								struct usb_ctrlrequest *ctrl)
{
	int ret = -EOPNOTSUPP;
	ug_dbg("[%s]\n", __func__);

	if (udc->driver && udc->driver->setup) {
		spin_unlock(&udc->lock);
		ret = udc->driver->setup(&udc->gadget, ctrl);
		spin_lock(&udc->lock);
		if (ret < 0) {
#ifdef CONFIG_USB_G_LGE_ANDROID
		if(!lge_get_factory_boot())
#endif
			usb_err("gadget setup error : ret = %d \n", ret);
			ud_core("SETUP %02x.%02x v%04x i%04x l%04x\n",
				   ctrl->bRequestType, ctrl->bRequest,
				   le16_to_cpu(ctrl->wValue),
				   le16_to_cpu(ctrl->wIndex),
				   le16_to_cpu(ctrl->wLength));
			return ret;
		}
	} else {
		usb_err("[%s]No gadget driver!\n", __func__);
	}

	/* check this delayed status again */
	if (ret == USB_GADGET_DELAYED_STATUS)
		udc->request_config = 1;

	return 0;
}

static int odin_udc_gadget_disconnect(struct odin_udc *udc)
{
	if (!udc) {
		usb_err("%s no udc!\n", __func__);
		return -EINVAL;
	}

	if (udc->driver && udc->driver->disconnect) {
		spin_unlock(&udc->lock);
		udc->driver->disconnect(&udc->gadget);
		spin_lock(&udc->lock);
	}
	return 0;
}

static int odin_udc_init(struct odin_udc *udc)
{
	u32 temp;
#ifdef CONFIG_USB_G_LGE_ANDROID
	lge_boot_mode_type boot_mode;
#endif

	ud_udc("[%s]\n", __func__);

	/* Soft-reset the core */
	temp = readl(udc->regs + ODIN_USB3_DCTL);
#ifdef CONFIG_USB_G_LGE_ANDROID
	/* Fix factory cable port open fail (dwc3/dwgadget.c from QCT)
	    Clear Keep-Connect bit instead of Run/Stop bit in odin_udc_init  */
	temp &= ~ODIN_USB3_DCTL_KEEP_CONNECT;
#else
	temp &= ~ODIN_USB3_DCTL_RUN_STOP;
#endif
	temp |= ODIN_USB3_DCTL_CSFT_RST;
	writel(temp, udc->regs + ODIN_USB3_DCTL);

	/* Wait for core to come out of reset */
	do {
		mdelay(1);
		temp = readl(udc->regs + ODIN_USB3_DCTL);
	} while (temp & ODIN_USB3_DCTL_CSFT_RST);

	/* Wait for at least 3 PHY clocks */
	mdelay(1);

	udc->link_state = ODIN_USB3_DSTS_LINK_STATE_U0;

	/* Initialize the Event Buffer registers */
	odin_drd_init_eventbuf(udc->dev->parent, udc->event_buf_dma);
	udc->event_ptr = udc->event_buf;

	temp = readl(udc->regs + ODIN_USB3_DCFG);
	temp &= ~(ODIN_USB3_DCFG_DEVSPD_MASK);

#ifdef CONFIG_USB_G_LGE_ANDROID
	boot_mode = lge_get_boot_mode();
	/* Set speed to Full speed with 130K factory cable */
	if(boot_mode == LGE_BOOT_MODE_FACTORY || boot_mode == LGE_BOOT_MODE_PIFBOOT) {
		temp |= ODIN_USB3_DCFG_DEVSPD(ODIN_USB3_SPEED_FS_30OR60);
	}
	else
#endif
	{
		if (udc->speed_mode == USB3_SPEED_MODE_SS) {
			temp |= ODIN_USB3_DCFG_DEVSPD(ODIN_USB3_SPEED_SS_125OR250);
		} else if (udc->speed_mode == USB3_SPEED_MODE_HS) {
			temp |= ODIN_USB3_DCFG_DEVSPD(ODIN_USB3_SPEED_HS_30OR60);
		} else {
			dev_err(udc->dev, "unsupported speed (%d) \n", udc->speed_mode);
			temp |= ODIN_USB3_DCFG_DEVSPD(ODIN_USB3_SPEED_SS_125OR250);
		}
	}

	/* Set LPMCap bit */
	temp |= ODIN_USB3_DCFG_LPM_CAP;

	/* Set NUMP */
	temp &= ~ODIN_USB3_DCFG_NUM_RCV_BUF_MASK;
	/* TODO : FIX NUMP */
	temp |= 16 << ODIN_USB3_DCFG_NUM_RCV_BUF_SHIFT;
	writel(temp, udc->regs + ODIN_USB3_DCFG);


	/* Enable LPM Cap and Automatic phy suspend will be supported (after 1.94a) */

	/* Start 9000498951: When connected at 2.0 speed (High speed),
	    a suspend or disconnect may cause endpoint commands to hang
	    Workaroud: Set ODIN_USB3_GUSB2PHYCFG_SUS_PHY to 0 at all times */
	if (udc->speed_mode == USB3_SPEED_MODE_HS)
		odin_drd_usb2_phy_suspend(udc->dev->parent, 0);
	else /* USB_SPEED_SUPER */
		odin_drd_usb2_phy_suspend(udc->dev->parent, 1);

	odin_drd_usb3_phy_suspend(udc->dev->parent, 1);

	odin_udc_ep0_activate(udc);

	/* Start EP0 to receive SETUP packets */
	udc->ep0state = EP0_IDLE;
	udc->ep0->is_in = 0;
	odin_udc_ep0_out_start(udc);

	/* Enable EP0-OUT/IN in DALEPENA register */
	writel(3, udc->regs + ODIN_USB3_DALEPENA);

	/* Enable Device mode interrupts */
	odin_udc_enable_device_interrupts(udc);

	/* Set Run/Stop bit */
	temp = readl(udc->regs + ODIN_USB3_DCTL);
	temp |= ODIN_USB3_DCTL_RUN_STOP;
	writel(temp, udc->regs + ODIN_USB3_DCTL);

#ifdef CONFIG_USB_G_LGE_ANDROID
	/* Poll DevCtrlHlt is 0 because when Halted = 1,
	    the core does not generate Device events */
	if(odin_udc_check_regs(udc->regs + ODIN_USB3_DSTS, false,
					ODIN_USB3_DSTS_DEV_CTRL_HLT, 500)) {
		usb_err("[%s] odin_udc_check_regs fails\n", __func__);
	}
#endif

	udc->eps_enabled = 0;

#ifdef CONFIG_USB_G_LGE_ANDROID
	if(lge_get_factory_boot())
		usb_print("[%s] done\n", __func__);
#endif

	return 0;
}

static void odin_udc_exit(struct odin_udc *udc)
{
	struct odin_udc_ep *odin_ep;
	int i;

	ud_udc("[%s]\n", __func__);

	/* kill any outstanding requests, prevent new request submissions */
	for (i = 1; i < udc->num_eps; i++) {
		odin_ep = &udc->eps[i - 1];
		odin_udc_stop_xfer(udc, odin_ep);
		odin_udc_request_nuke(udc, odin_ep);
		odin_udc_ep_rst_flags(odin_ep);
	}

#ifdef CONFIG_USB_G_LGE_ANDROID
	/* wake_lock is not unlocked (odin_udc_disconnect_intr is not called)
	because MAX77809 charger safeout disconnect is later than MAX77809 charger interrupt */
	odin_udc_disconnect_intr(udc);
#else
	if (udc->state != USB_STATE_NOTATTACHED) {
		udc->state = USB_STATE_NOTATTACHED;
		odin_udc_gadget_disconnect(udc);
	}
#endif
}


static int odin_udc_get_frame(struct usb_gadget *gadget)
{
	struct odin_udc *udc = container_of(gadget,
					struct odin_udc, gadget);
	unsigned long	flags;
	u32 dsts;
	int ret = 0;

	if (!gadget)
		return -ENODEV;

	spin_lock_irqsave(&udc->lock, flags);
	dsts = readl(udc->regs + ODIN_USB3_DSTS);
	ret = ODIN_USB3_DSTS_SOF_FN(dsts);
	spin_unlock_irqrestore(&udc->lock, flags);

	return ret;
}

static int odin_udc_wakeup(struct usb_gadget *gadget)
{
	struct odin_udc *udc = container_of(gadget,
					struct odin_udc, gadget);
	unsigned long	flags;
	u32 dsts;
	u8 state;
	u8 speed;
	int ret = 0;

	spin_lock_irqsave(&udc->lock, flags);

	dsts = readl(udc->regs + ODIN_USB3_DSTS);

	speed = dsts & ODIN_USB3_DSTS_CONNSPD;
	if (speed == ODIN_USB3_SPEED_SS_125OR250) {
		ud_core("no wakeup on SuperSpeed\n");
		ret = -EINVAL;
		goto out;
	}

	state = ODIN_USB3_DSTS_LINK_STATE(dsts);
	switch (state) {
	case ODIN_USB3_DSTS_LINK_STATE_RX_DET:
	case ODIN_USB3_DSTS_LINK_STATE_U3:
		break;
	default:
		ud_core("can't wakeup from link state %d\n", state);
		ret = -EINVAL;
		goto out;
	}

	ret = odin_udc_set_link_state(udc,
					ODIN_USB3_DSTS_LINK_STATE_RECOV);
	if (ret < 0) {
		dev_err(udc->dev, "failed to set link\n");
		goto out;
	}

	odin_udc_handshake(udc->regs + ODIN_USB3_DSTS,
					ODIN_USB3_DSTS_LINK_STATE_U0);

	if (ODIN_USB3_DSTS_LINK_STATE(dsts) !=
				ODIN_USB3_DSTS_LINK_STATE_U0) {
		dev_err(udc->dev, "failed to send remote wakeup\n");
		ret = -EINVAL;
	}

out:
	spin_unlock_irqrestore(&udc->lock, flags);
	return ret;
}

static int odin_udc_vbus_session(struct usb_gadget *gadget, int is_active)
{
	struct odin_udc *udc = container_of(gadget,
					struct odin_udc, gadget);
	unsigned long flags;
	u32 temp;
	int count = 0;

#ifdef CONFIG_USB_G_LGE_ANDROID
	if(lge_get_factory_boot())
		usb_print("[%s] :: is_active = %d (%d)\n", __func__, is_active, udc->pullup);
	else
#endif
	ud_core("[%s] :: is_active = %d (%d)\n", __func__, is_active, udc->pullup);

	spin_lock_irqsave(&udc->lock, flags);

	if (is_active) {
		if (udc->udc_enabled) {
			ud_core("UDC is already enabled \n");
			spin_unlock_irqrestore(&udc->lock, flags);
			return 0;
		}

		if (udc->pullup) {
			odin_udc_init(udc);
		}
		udc->udc_enabled = 1;
		spin_unlock_irqrestore(&udc->lock, flags);
	}else {
		if (!udc->udc_enabled) {
			spin_unlock_irqrestore(&udc->lock, flags);
			ud_core("UDC is already disabled \n");
			return 0;
		}

		odin_udc_exit(udc);

		if (udc->pullup) {
			/* Clear Run/Stop bit */
			temp = readl(udc->regs + ODIN_USB3_DCTL);
			temp &= ~ODIN_USB3_DCTL_RUN_STOP;
			writel(temp, udc->regs + ODIN_USB3_DCTL);
		}

		/* Disable device interrupts */
		writel(0, udc->regs + ODIN_USB3_DEVTEN);
		odin_drd_dis_eventbuf(udc->dev->parent);
		odin_drd_flush_eventbuf(udc->dev->parent);
		odin_udc_flush_all_fifo(udc);

		udc->udc_enabled = 0;
		spin_unlock_irqrestore(&udc->lock, flags);

#ifdef CONFIG_CPU_IDLE_ODIN_WORKAROUND
		if (udc->cpu_handle) {
			odin_idle_disable(USB_CPU_AFFINITY, 0);
			udc->cpu_handle = 0;
		}
#endif
		/* Wait for core stopped */
		do {
			msleep(1);
			temp = readl(udc->regs + ODIN_USB3_DSTS);
		} while (!(temp & ODIN_USB3_DSTS_DEV_CTRL_HLT) && (++count < 30));
		msleep(10);
	}
	return 0;
}

static int odin_udc_vbus_draw(struct usb_gadget *gadget, unsigned mA)
{
	struct odin_udc *udc = container_of(gadget,
					struct odin_udc, gadget);

	if (!IS_ERR_OR_NULL(&udc->odin_otg->phy))
		return usb_phy_set_power(&udc->odin_otg->phy, mA);
	return -EOPNOTSUPP;
}

static int odin_udc_pullup(struct usb_gadget *gadget, int is_on)
{
	struct odin_udc *udc = container_of(gadget,
					struct odin_udc, gadget);
	u32 temp;
	unsigned long flags;

	ud_core("[%s] :: is_on = %d \n", __func__, is_on);
	spin_lock_irqsave(&udc->lock, flags);
	udc->pullup = is_on;

	if (!udc->udc_enabled) {
		spin_unlock_irqrestore(&udc->lock, flags);
		ud_core("UDC is not enabled \n");
		return 0;
	}

	if (is_on) {
		odin_udc_init(udc);
	} else {
		temp = readl(udc->regs + ODIN_USB3_DCTL);
#ifdef CONFIG_USB_G_LGE_ANDROID
		/* Fix factory cable port open fail (dwc3/dwgadget.c from QCT)
		    Clear Keep-Connect bit in odin_udc_init */
		temp &= ~ODIN_USB3_DCTL_RUN_STOP;
#else
		/* Clear the Run/Stop and Keep-Connect bits */
		temp &= ~(ODIN_USB3_DCTL_RUN_STOP |
					ODIN_USB3_DCTL_KEEP_CONNECT);
#endif
		writel(temp, udc->regs + ODIN_USB3_DCTL);

		/* Disable device interrupts */
		writel(0, udc->regs + ODIN_USB3_DEVTEN);

		odin_drd_dis_eventbuf(udc->dev->parent);
		odin_udc_flush_all_fifo(udc);

#ifdef CONFIG_USB_G_LGE_ANDROID
		/* Wait for core stopped */
		if(odin_udc_check_regs(udc->regs + ODIN_USB3_DSTS, true,
					ODIN_USB3_DSTS_DEV_CTRL_HLT, 500)) {
			usb_err("[%s] odin_udc_check_regs fails\n", __func__);
		}
#endif
	}
	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;
}

static int odin_udc_start(struct usb_gadget *gadget,
				struct usb_gadget_driver *driver)
{
	struct odin_udc *udc = container_of(gadget,
					struct odin_udc, gadget);
	int ret = 0;
	ud_core("[%s] \n", __func__);

	udc->driver = driver;
	udc->gadget.dev.driver = &driver->driver;
	return ret;
}

static int odin_udc_stop(struct usb_gadget *gadget,
			       struct usb_gadget_driver *driver)
{
	struct odin_udc *udc = container_of(gadget,
					struct odin_udc, gadget);
	udc->driver = NULL;
	return 0;
}

static const struct usb_gadget_ops odin_gadget_ops = {
	.get_frame	= odin_udc_get_frame,
	.wakeup		= odin_udc_wakeup,
	.vbus_session	= odin_udc_vbus_session,
	.vbus_draw	= odin_udc_vbus_draw,
	.pullup		= odin_udc_pullup,
	.udc_start	= odin_udc_start,
	.udc_stop		= odin_udc_stop,
};

struct odin_udc_ep *odin_udc_get_ep_by_addr
							(struct odin_udc *udc, u16 index)
{
	ud_udc("[%s] index = %x \n", __func__, index);
	u32 epnum = (index & USB_ENDPOINT_NUMBER_MASK) << 1;
	if ((index & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN)
		epnum |= 1;

	if (epnum == 0)
		epnum = 1;

	ud_udc("[%s] epnum = %x \n", __func__, epnum);
	return &udc->eps[epnum-1];
}

static void odin_udc_ep0_data_stage(struct odin_udc *udc, int length)
{
	udc->ep0_req.buf = udc->ep0_status_buf;
	udc->ep0_req.bufdma = udc->ep0_status_buf_dma;
	udc->ep0_req.length = length;
	udc->ep0_req.actual = 0;
	udc->ep0_status_pending = 1;
	udc->ep0->send_zlp = 0;
	odin_udc_ep0_start_transfer(udc, &udc->ep0_req);
}

static void odin_udc_setup_status_phase_zlp(struct odin_udc *udc, void *buf,
				  dma_addr_t dma)
{
	ud_udc("%s()\n", __func__);

	if (udc->ep0state == EP0_STALL) {
		ud_udc("EP0 STALLED\n");
		return;
	}

	if (udc->ep0->is_in)
		udc->ep0state = EP0_IN_STATUS_PHASE;
	else
		udc->ep0state = EP0_OUT_STATUS_PHASE;

	udc->ep0_req.buf = buf;
	udc->ep0_req.bufdma = dma;
	udc->ep0_req.length = 0;
	udc->ep0_req.actual = 0;
	odin_udc_ep0_start_transfer(udc, &udc->ep0_req);
}

static void odin_udc_ep0_stall(struct odin_udc *udc, int err_val)
{
	struct usb_ctrlrequest *ctrl = udc->ctrl_req;

#ifdef CONFIG_USB_G_LGE_ANDROID
	if(!lge_get_factory_boot())
#endif
	dev_err(udc->dev, "req %02x.%02x protocol STALL; err %d\n",
		   ctrl->bRequestType, ctrl->bRequest, err_val);
	udc->ep0->is_in = 0;
	odin_udc_epcmd_set_stall(udc, udc->ep0);
	udc->ep0->stopped = 1;
	udc->ep0state = EP0_IDLE;
	odin_udc_ep0_out_start(udc);
}

static int odin_udc_set_address(struct odin_udc *udc)
{
	struct usb_ctrlrequest *ctrl = udc->ctrl_req;
	u32 dcfg, addr;

	if (ctrl->bRequestType == USB_RECIP_DEVICE) {
		addr = le16_to_cpu(ctrl->wValue);
		if (addr > 127) {
			dev_err(udc->dev, "invalid device address %x\n", addr);
			return -EINVAL;
		}

		ud_udc("[%s] address = %d \n", __func__, addr);

		dcfg = readl(udc->regs + ODIN_USB3_DCFG);
		dcfg &= ~ODIN_USB3_DCFG_DEVADDR_MASK;
		dcfg |= ODIN_USB3_DCFG_DEVADDR(addr);
		writel(dcfg, udc->regs + ODIN_USB3_DCFG);

		udc->ep0->is_in = 1;
		udc->ep0state = EP0_IN_WAIT_NRDY;
		if (ctrl->wValue)
			udc->state = USB_STATE_ADDRESS;
		else
			udc->state = USB_STATE_DEFAULT;
	}
	return 0;
}

static void odin_udc_get_status(struct odin_udc *udc)
{
	struct usb_ctrlrequest *ctrl = udc->ctrl_req;
	u8 *status = udc->ep0_status_buf;
	struct odin_udc_ep *odin_ep;
	int length;
	u32 dctl;

	if (le16_to_cpu(ctrl->wLength) != 2) {
		odin_udc_ep0_stall(udc, -EOPNOTSUPP);
		return;
	}

	switch (ctrl->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		*status = 1; /* Self powered */

		if (udc->speed == USB_SPEED_SUPER) {
			if (udc->state == USB_STATE_CONFIGURED) {
				dctl = readl(udc->regs + ODIN_USB3_DCTL);
				if (dctl & ODIN_USB3_DCTL_INIT_U1_EN)
					*status |= 1 << 2;
				if (dctl & ODIN_USB3_DCTL_INIT_U2_EN)
					*status |= 1 << 3;
				*status |= udc->ltm_enable << 4;
			}
		} else {
			*status |= udc->remote_wakeup_enable << 1;
		}

		ud_udc("GET_STATUS(Device)=%02x\n", *status);
		*(status + 1) = 0;
		break;

	case USB_RECIP_INTERFACE:
		*status = 0;
#if 1 /* Enable Wakeup */
		*status |= 1;
#endif
		*status |= udc->remote_wakeup_enable << 1;
		ud_udc("GET_STATUS(Interface %d)=%02x\n",
			   le16_to_cpu(ctrl->wIndex), *status);
		*(status + 1) = 0;
		break;

	case USB_RECIP_ENDPOINT:
		odin_ep = odin_udc_get_ep_by_addr(udc, le16_to_cpu(ctrl->wIndex));

		/* @todo check for EP stall */
		*status = odin_ep->stopped;
		ud_udc("GET_STATUS(Endpoint %d)=%02x\n",
			   le16_to_cpu(ctrl->wIndex), *status);
		*(status + 1) = 0;
		break;

	default:
		odin_udc_ep0_stall(udc, -EOPNOTSUPP);
		return;
	}

	length = 2;
	odin_udc_ep0_data_stage(udc, length);
}

static void odin_udc_set_feature(struct odin_udc *udc)
{
	struct usb_ctrlrequest *ctrl = udc->ctrl_req;
	struct odin_udc_ep *odin_ep;
	int ret;
	u32 dctl;
	u16 wvalue = le16_to_cpu(ctrl->wValue);
	u16 windex = le16_to_cpu(ctrl->wIndex);


	switch (ctrl->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		switch (wvalue) {
		case USB_DEVICE_REMOTE_WAKEUP:
			udc->remote_wakeup_enable = 1;
			break;

		case USB_DEVICE_B_HNP_ENABLE:
			ud_udc("SET_FEATURE: USB_DEVICE_B_HNP_ENABLE\n");
			break;

		case USB_DEVICE_A_HNP_SUPPORT:
			ud_udc("SET_FEATURE: USB_DEVICE_A_HNP_SUPPORT\n");
			break;

		case USB_DEVICE_A_ALT_HNP_SUPPORT:
			ud_udc("SET_FEATURE: USB_DEVICE_A_ALT_HNP_SUPPORT\n");
			break;

		case USB_DEVICE_U1_ENABLE:
			ud_udc("SET_FEATURE: USB_DEVICE_U1_ENABLE\n");
			if (udc->speed != USB_SPEED_SUPER ||
			    udc->state != USB_STATE_CONFIGURED) {
				ud_udc("Fail to U1 Enable\n");
				odin_udc_ep0_stall(udc, -EOPNOTSUPP);
				return;
			}
			/* Enable U1 */
			dctl = readl(udc->regs + ODIN_USB3_DCTL);
			dctl |= ODIN_USB3_DCTL_INIT_U1_EN;
			writel(dctl, udc->regs + ODIN_USB3_DCTL);
			break;

		case USB_DEVICE_U2_ENABLE:
			ud_udc("SET_FEATURE: UF_U2_ENABLE\n");
			if (udc->speed != USB_SPEED_SUPER ||
			    udc->state != USB_STATE_CONFIGURED) {
				ud_udc("Fail to U2 Enable\n");
				odin_udc_ep0_stall(udc, -EOPNOTSUPP);
				return;
			}
			/* Enable U2 */
			dctl = readl(udc->regs + ODIN_USB3_DCTL);
			dctl |= ODIN_USB3_DCTL_INIT_U2_EN;
			writel(dctl, udc->regs + ODIN_USB3_DCTL);
			break;

		case USB_DEVICE_LTM_ENABLE:
			ud_udc("SET_FEATURE: UF_LTM_ENABLE\n");
			if (udc->speed != USB_SPEED_SUPER ||
			    udc->state != USB_STATE_CONFIGURED ||
			    windex != 0) {
				odin_udc_ep0_stall(udc, -EOPNOTSUPP);
				return;
			}

			udc->ltm_enable = 1;
			udc->send_lpm = 1;
			break;

		case USB_DEVICE_TEST_MODE:
			usb_dbg("SET_FEATURE: UF_TEST_MODE\n");
			if (windex & 0xff)
				return;

			switch (windex >> 8) {
			case TEST_J:
			case TEST_K:
			case TEST_SE0_NAK:
			case TEST_PACKET:
			case TEST_FORCE_EN:
				udc->test_mode_seletor = windex >> 8;
				udc->test_mode = 1;
				break;
			default:
				return;
			}

			break;

		default:
			odin_udc_ep0_stall(udc, -EOPNOTSUPP);
			return;
		}
		break;

	case USB_RECIP_INTERFACE:
		switch (wvalue) {
		case USB_INTRF_FUNC_SUSPEND:
			if (windex & USB_INTRF_FUNC_SUSPEND_RW)
				udc->remote_wakeup_enable = 1;

			if (windex & USB_INTRF_FUNC_SUSPEND_LP)
			/* TODO */
			break;
		default:
			ret = odin_udc_gadget_setup(udc, ctrl);
			if (ret < 0)
				odin_udc_ep0_stall(udc, ret);
			return;
		}
		break;

	case USB_RECIP_ENDPOINT:
		switch (wvalue) {
		case USB_ENDPOINT_HALT:
			odin_ep = odin_udc_get_ep_by_addr(udc, windex);
			odin_ep->stopped = 1;
			odin_udc_epcmd_set_stall(udc, odin_ep);
			break;
		default:
			odin_udc_ep0_stall(udc, -EOPNOTSUPP);
			return;
		}
		break;
	}

	udc->ep0->is_in = 1;
	udc->ep0state = EP0_IN_WAIT_NRDY;
}

static void odin_udc_clear_feature(struct odin_udc *udc)
{
	struct usb_ctrlrequest *ctrl = udc->ctrl_req;
	struct odin_udc_ep *odin_ep;
	u32 dctl;
	u16 wvalue = le16_to_cpu(ctrl->wValue);
	u16 windex = le16_to_cpu(ctrl->wIndex);

	switch (ctrl->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		switch (wvalue) {
		case USB_DEVICE_REMOTE_WAKEUP:
			udc->remote_wakeup_enable = 0;
			break;

		case USB_DEVICE_TEST_MODE:
			/* @todo Add CLEAR_FEATURE for TEST modes. */
			break;

		case USB_DEVICE_U1_ENABLE:
			ud_udc("CLEAR_FEATURE: USB_DEVICE_U1_ENABLE\n");
			if (udc->speed != USB_SPEED_SUPER ||
			    udc->state != USB_STATE_CONFIGURED) {
				odin_udc_ep0_stall(udc, -EOPNOTSUPP);
				return;
			}

			/* Disable U1 */
			dctl = readl(udc->regs + ODIN_USB3_DCTL);
			dctl &= ~ODIN_USB3_DCTL_INIT_U1_EN;
			writel(dctl, udc->regs + ODIN_USB3_DCTL);
			break;

		case USB_DEVICE_U2_ENABLE:
			ud_udc("CLEAR_FEATURE: USB_DEVICE_U2_ENABLE\n");
			if (udc->speed != USB_SPEED_SUPER ||
			    udc->state != USB_STATE_CONFIGURED) {
				odin_udc_ep0_stall(udc, -EOPNOTSUPP);
				return;
			}

			/* Disable U2 */
			dctl = readl(udc->regs + ODIN_USB3_DCTL);
			dctl &= ~ODIN_USB3_DCTL_INIT_U2_EN;
			writel(dctl, udc->regs + ODIN_USB3_DCTL);
			break;

		case USB_DEVICE_LTM_ENABLE:
			ud_udc("CLEAR_FEATURE: USB_DEVICE_LTM_ENABLE\n");
			if (udc->speed != USB_SPEED_SUPER ||
			    udc->state != USB_STATE_CONFIGURED ||
			    windex != 0) {
				odin_udc_ep0_stall(udc, -EOPNOTSUPP);
				return;
			}

			udc->ltm_enable = 0;
			udc->send_lpm = 1;
			break;

		default:
			odin_udc_ep0_stall(udc, -EOPNOTSUPP);
			return;
		}
		break;

	case USB_RECIP_INTERFACE:
		switch (wvalue) {
		case USB_INTRF_FUNC_SUSPEND:
			if (windex & USB_INTRF_FUNC_SUSPEND_RW)
				udc->remote_wakeup_enable = 0;

			if (windex & USB_INTRF_FUNC_SUSPEND_LP)
			/* TODO */
			break;
		default:
			odin_udc_ep0_stall(udc, -EOPNOTSUPP);
			return;
		}
		break;

	case USB_RECIP_ENDPOINT:
		switch (wvalue) {
		case USB_ENDPOINT_HALT:
			odin_ep = odin_udc_get_ep_by_addr(udc, windex);

			if (odin_ep->wedge)
				break;

			odin_udc_epcmd_clr_stall(udc, odin_ep);

			if (odin_ep->stopped) {
				odin_ep->stopped = 0;

				/* If there is a request in the EP queue start it */
				if (odin_ep != udc->ep0 && odin_ep->is_in)
					odin_udc_start_next_request(udc, odin_ep);
			}
			break;
		default:
			odin_udc_ep0_stall(udc, -EOPNOTSUPP);
			return;
		}
		break;
	}

	udc->ep0->is_in = 1;
	udc->ep0state = EP0_IN_WAIT_NRDY;
}

static void odin_udc_do_setup(struct odin_udc *udc)
{
	struct usb_ctrlrequest *ctrl = udc->ctrl_req;
	u16 wvalue, wlength;
	int ret;
	u32 dctl;

	wvalue = le16_to_cpu(ctrl->wValue);
	wlength = le16_to_cpu(ctrl->wLength);

	ud_udc("\n");
	ud_udc("SETUP %02x.%02x v%04x i%04x l%04x\n",
		   ctrl->bRequestType, ctrl->bRequest, wvalue,
		   le16_to_cpu(ctrl->wIndex), wlength);

	/* Clean up the request queue */
	odin_udc_request_nuke(udc, udc->ep0);
	udc->ep0->stopped = 0;
	udc->ep0->three_stage = 1;

	if (ctrl->bRequestType & USB_DIR_IN) {
		udc->ep0->is_in = 1;
		udc->ep0state = EP0_IN_DATA_PHASE;
	} else {
		udc->ep0->is_in = 0;
		udc->ep0state = EP0_OUT_DATA_PHASE;
	}

	if (wlength == 0) {
		udc->ep0->is_in = 1;
		udc->ep0state = EP0_IN_WAIT_GADGET;
		udc->ep0->three_stage = 0;
	}

	if ((ctrl->bRequestType & USB_TYPE_MASK) != USB_TYPE_STANDARD) {
		/* Non-standard (class/vendor) Requests */
		ud_udc("Non-standrd Requests\n");
		ret = odin_udc_gadget_setup(udc, ctrl);
		if (ret < 0){
			ud_udc("%s odin_udc_gadget_setup fail \n", __func__);
			odin_udc_ep0_stall(udc, ret);
		}
		return;
	}

	/* Standard Requests */
	switch (ctrl->bRequest) {
	case USB_REQ_GET_STATUS:
		ud_udc("USB_REQ_GET_STATUS\n");
		odin_udc_get_status(udc);
		break;

	case USB_REQ_CLEAR_FEATURE:
		ud_udc("USB_REQ_CLEAR_FEATURE\n");
		odin_udc_clear_feature(udc);
		break;

	case USB_REQ_SET_FEATURE:
		ud_udc("USB_REQ_SET_FEATURE\n");
		odin_udc_set_feature(udc);
		break;

	case USB_REQ_SET_ADDRESS:
		ud_udc("USB_REQ_SET_ADDRESS\n");
		odin_udc_set_address(udc);
		break;

	case USB_REQ_GET_DESCRIPTOR:
		ud_udc("USB_REQ_GET_DESCRIPTOR\n");
		ret = odin_udc_gadget_setup(udc, ctrl);
		if (ret < 0) {
			odin_udc_ep0_stall(udc, ret);
			return;
		}
		break;

	case USB_REQ_SET_CONFIGURATION:
		ud_udc("USB_REQ_SET_CONFIGURATION\n");
		if (udc->ltm_enable)
			udc->send_lpm = 1;

		ret = odin_udc_gadget_setup(udc, ctrl);
		if (ret >= 0) {
			if (wvalue != 0)
				udc->state = USB_STATE_CONFIGURED;
			else
				udc->state = USB_STATE_ADDRESS;
		} else {
			odin_udc_ep0_stall(udc, ret);
			return;
		}

		/* Use U1 & U2 */
		dctl = readl(udc->regs + ODIN_USB3_DCTL);
		dctl |= ODIN_USB3_DCTL_ACCEPT_U1_EN | ODIN_USB3_DCTL_ACCEPT_U2_EN;
		writel(dctl, udc->regs + ODIN_USB3_DCTL);

		udc->ltm_enable = 0;
		break;

	case USB_REQ_SET_INTERFACE:
		ud_udc("USB_REQ_SET_INTERFACE\n");
		ret = odin_udc_gadget_setup(udc, ctrl);
		if (ret < 0) {
			odin_udc_ep0_stall(udc, ret);
			return;
		}
		break;

	case USB_REQ_SET_SEL:
		ud_udc("USB_REQ_SET_SEL\n");
		/* For now this is a no-op */
		odin_udc_ep0_data_stage(udc, ODIN_USB3_STATUS_BUF_SIZE < wlength ?
					    ODIN_USB3_STATUS_BUF_SIZE : wlength);
		break;

	case USB_REQ_SET_ISOCH_DELAY:
		ud_udc("USB_REQ_SET_ISOCH_DELAY\n");
		/* For now this is a no-op */
		udc->ep0->is_in = 1;
		udc->ep0state = EP0_IN_WAIT_NRDY;
		break;

	default:
		/* Call the Gadget driver's setup routine */
		ud_udc("Default -> Call Gadget Setup \n");
		ret = odin_udc_gadget_setup(udc, ctrl);
		if (ret < 0)
			odin_udc_ep0_stall(udc, ret);
		break;
	}
	ud_udc("\n");
}

static int odin_udc_ep0_complete_request(struct odin_udc *udc,
		struct odin_usb3_req *req, struct odin_udc_trb *trb, int status)
{
	int is_last = 0;

	if (udc->ep0_status_pending && !req) {
		if (udc->ep0->is_in) {
			udc->ep0->is_in = 0;
			udc->ep0state = EP0_OUT_WAIT_NRDY;
		} else {
			udc->ep0->is_in = 1;
			udc->ep0state = EP0_IN_WAIT_NRDY;
		}

		udc->ep0_status_pending = 0;
		return 1;
	}

	if (!req)
		return 0;

	if (udc->ep0state == EP0_OUT_STATUS_PHASE ||
	    udc->ep0state == EP0_IN_STATUS_PHASE) {
		is_last = 1;

	} else if (udc->ep0->is_in) {
		ud_udc("IN len=%d actual=%d xfrcnt=%d trbrsp=0x%02x\n",
			   req->length, req->actual,
			   ODIN_USB3_DSCSTS_LENGTH(trb->status),
			   ODIN_USB3_DSCSTS_TRBRSP(trb->status));

		if (ODIN_USB3_DSCSTS_LENGTH(trb->status) == 0) {
			if (req->flags & ODIN_USB3_REQ_ZERO) {
				ud_core("[%s]IN/ODIN_USB3_REQ_ZERO removed \n",
						__func__);
				req->flags &= ~ODIN_USB3_REQ_ZERO;
			}

			udc->ep0->is_in = 0;
			udc->ep0state = EP0_OUT_WAIT_NRDY;
		}
	} else {
		ud_udc("OUT len=%d actual=%d xfrcnt=%d trbrsp=0x%02x\n",
			   req->length, req->actual,
			   ODIN_USB3_DSCSTS_LENGTH(trb->status),
			   ODIN_USB3_DSCSTS_TRBRSP(trb->status));

		udc->ep0->is_in = 1;
		udc->ep0state = EP0_IN_WAIT_NRDY;
	}

	if (is_last) {
		ud_udc("is_last len=%d actual=%d\n",
			   req->length, req->actual);
		odin_udc_request_complete(udc, udc->ep0, req, status);
		return 1;
	}

	return 0;
}

static void odin_udc_handle_depevt_epcmd_cmpl(struct odin_udc *udc,
						struct odin_udc_ep *odin_ep, u32 event)
{
	u32 cmpl;
	struct odin_usb3_req *req;
	struct list_head *list_item;

	cmpl = (event & ODIN_USB3_DEPEVT_EVT_STATUS_MASK);
	switch (cmpl) {
	case ODIN_USB3_DEPEVT_CMDTYPE_DEPSTARTXFER:
		ud_udc("DEPEVT CMDTYPE = DEPSTARTXFER\n");

		if ((odin_ep->type == USB_ENDPOINT_XFER_ISOC)
				&& (event & ODIN_USB3_DEPEVT_EVT_STATUS_UF_PASSED)){
			ud_core("uF has already passed! \n");
			list_for_each (list_item, &odin_ep->queue) {
				req = list_entry(list_item, struct odin_usb3_req, entry);
				if (req->flags & ODIN_USB3_REQ_STARTED)
					req->flags &= ~ODIN_USB3_REQ_STARTED;
			}
			odin_udc_iso_xfer_stop(udc, odin_ep);
			odin_ep->desc_avail++;
		}
		break;

	case ODIN_USB3_DEPEVT_CMDTYPE_DEPENDXFER:
		ud_udc("DEPEVT CMDTYPE = DEPENDXFER\n");
		/* TODO */
		break;

	default:
		usb_print("Unregisted cmd complete depevt = %x \n", cmpl);
		break;
	}
}

void odin_udc_handle_ep0(struct odin_udc *udc, struct odin_usb3_req *req,
			 u32 event)
{
	struct odin_udc_trb *trb;
	u32 byte_count, len;
	int status;
	ud_udc("[%s] S: ep0state = %s \n", __func__,
					odin_udc_ep0_state[udc->ep0state]);

	switch (udc->ep0state) {
	case EP0_IN_DATA_PHASE:
		if (!req)
			req = &udc->ep0_req;
		trb = udc->ep0_in_trb;
		if (trb->control & ODIN_USB3_DSCCTL_HWO) {
			usb_print("[%s] EP%d-%s HWO bit set 1!\n",
				__func__, udc->ep0->num, udc->ep0->is_in ?
				"IN" : "OUT");
		}

		status = ODIN_USB3_DSCSTS_TRBRSP(trb->status);
		if (status & ODIN_USB3_TRBRSP_SETUP_PEND) {
			/* Start of a new Control transfer */
			ud_udc("IN SETUP PENDING\n");
			trb->status = 0;
		}

		byte_count =
			req->length - ODIN_USB3_DSCSTS_LENGTH(trb->status);
		req->actual += byte_count;
		req->buf += byte_count;
		req->bufdma += byte_count;
		ud_udc("length=%d byte_count=%d actual=%d\n",
			req->length, byte_count, req->actual);

		if (req->actual < req->length) {
			ud_udc("IN CONTINUE, Stall EP0\n");
			udc->ep0->is_in = 0;
			odin_udc_epcmd_set_stall(udc, udc->ep0);
			udc->ep0->stopped = 1;
			udc->ep0state = EP0_IDLE;
			odin_udc_ep0_out_start(udc);

		} else if (udc->ep0->send_zlp) {
			ud_core("IN Data Phase ZLP\n");
			odin_udc_send_zlp(udc, req);
			udc->ep0->send_zlp = 0;
		} else {
			ud_udc("IN COMPLETE\n");
			/* This sets ep0state = EP0_IN/OUT_WAIT_NRDY */
			odin_udc_ep0_complete_request(udc, req, trb, 0);
			ud_udc("COMPLETE TRANSFER\n");
		}

		break;

	case EP0_OUT_DATA_PHASE:
		if (!req)
			req = &udc->ep0_req;
		trb = udc->ep0_out_trb;
		ud_udc("req=%lx\n", (unsigned long)req);
#ifdef DEBUG_EP0
		ud_udc("DATA_OUT EP%d-%s: type=%d mps=%d trb.status=0x%08x\n",
			udc->ep0->num, (udc->ep0->is_in ? "IN" : "OUT"),
			udc->ep0->type, udc->ep0->maxpacket, trb->status);
#endif
		if (trb->control & ODIN_USB3_DSCCTL_HWO) {
			ud_udc("[%s] EP%d-%s HWO bit set 2! \n",
				__func__, udc->ep0->num, udc->ep0->is_in?
				"IN" : "OUT");
		}

		status = ODIN_USB3_DSCSTS_TRBRSP(trb->status);
		if (status & ODIN_USB3_TRBRSP_SETUP_PEND) {
			/* Start of a new Control transfer */
			ud_udc("OUT SETUP PENDING\n");
		}

		len = (req->length + udc->ep0->maxpacket - 1) &
			~(udc->ep0->maxpacket - 1);
		byte_count = len - ODIN_USB3_DSCSTS_LENGTH(trb->status);
		req->actual += byte_count;
		req->buf += byte_count;
		req->bufdma += byte_count;
		ud_udc("length=%d byte_count=%d actual=%d\n",
			req->length, byte_count, req->actual);

		 if (udc->ep0->send_zlp) {
			ud_core("OUT Data Phase ZLP\n");
			odin_udc_send_zlp(udc, req);
			udc->ep0->send_zlp = 0;
		} else {
			ud_udc("OUT COMPLETE\n");
			/* This sets ep0state = EP0_IN/OUT_WAIT_NRDY */
			odin_udc_ep0_complete_request(udc, req, trb, 0);
			ud_udc("COMPLETE TRANSFER\n");
		}

		break;

	case EP0_IN_WAIT_GADGET:
		udc->ep0state = EP0_IN_WAIT_NRDY;
		break;

	case EP0_OUT_WAIT_GADGET:
		udc->ep0state = EP0_OUT_WAIT_NRDY;
		break;

	case EP0_IN_WAIT_NRDY:
	case EP0_OUT_WAIT_NRDY:
		odin_udc_setup_status_phase_zlp(udc, udc->ctrl_req,
					       udc->ctrl_req_dma);
		break;

	case EP0_IN_STATUS_PHASE:
	case EP0_OUT_STATUS_PHASE:
		if (udc->ep0->is_in)
			trb = udc->ep0_in_trb;
		else
			trb = udc->ep0_out_trb;
#ifdef DEBUG_EP0
		ud_udc("STATUS EP%d-%s\n", udc->ep0->num,
			   (udc->ep0->is_in ? "IN" : "OUT"));
#endif
		odin_udc_ep0_complete_request(udc, req, trb, 0);
		udc->ep0state = EP0_IDLE;
		udc->ep0->stopped = 1;
		udc->ep0->is_in = 0;	/* OUT for next SETUP */

		if (udc->send_lpm) {
			udc->send_lpm = 0;
		}

		if (udc->test_mode)
			odin_udc_test_mode(udc);

		/* Prepare for more SETUP Packets */
		odin_udc_ep0_out_start(udc);
		break;

	case EP0_STALL:
		usb_err("EP0 STALLed, should not get here\n");
		break;

	case EP0_IDLE:
		usb_err("EP0 IDLE, should not get here\n");
		break;
	}
out:
	ud_udc("[%s] E: ep0state = %s \n", __func__,
					odin_udc_ep0_state[udc->ep0state]);
	return;
}

static void odin_udc_handle_ep0_xfer(struct odin_udc *udc, u32 event)
{
	struct odin_usb3_req *req = NULL;

	ud_udc("%s()\n", __func__);

	req = odin_udc_get_request(udc, udc->ep0);

	if (udc->ep0state == EP0_IDLE) {
		udc->request_config = 0;
		odin_udc_do_setup(udc);
	} else {
		odin_udc_handle_ep0(udc, req, event);
	}
}

static void odin_udc_usb_reset_intr(struct odin_udc *udc)
{
	struct odin_udc_ep *odin_ep;
	u32 dcfg, dctl;
	int i;

#ifdef CONFIG_USB_G_ANDROID
	if (udc->speed != USB_SPEED_UNKNOWN)
		odin_udc_gadget_disconnect(udc);
#endif

	for (i = 1; i < udc->num_eps; i++) {
		odin_ep = &udc->eps[i - 1];

		if (i > 1) {	/* Non-EP0 */
			odin_udc_stop_xfer(udc, odin_ep);
			odin_udc_request_nuke(udc, odin_ep);
			odin_ep->xfer_started = 0;
		}

		if (odin_ep->stopped)
			odin_udc_epcmd_clr_stall(udc, odin_ep);
	}

	dctl = readl(udc->regs + ODIN_USB3_DCTL);
	dctl &= ~ODIN_USB3_DCTL_TSTCTL_MASK;
	writel(dctl, udc->regs + ODIN_USB3_DCTL);
	udc->test_mode = 0;

	udc->eps_enabled = 0;
	udc->ep0state = EP0_IDLE;

	dcfg = readl(udc->regs + ODIN_USB3_DCFG);
	dcfg &= ~ODIN_USB3_DCFG_DEVADDR_MASK;
	writel(dcfg, udc->regs + ODIN_USB3_DCFG);

	udc->remote_wakeup_enable = 0;
	udc->ltm_enable = 0;
}

static void odin_udc_connect_done_intr(struct odin_udc *udc)
{
	u32 diepcfg0, doepcfg0, diepcfg1, doepcfg1;
	int speed;

	odin_udc_connect(udc, 1);

	udc->ep0->stopped = 0;
	speed = odin_udc_get_device_speed(udc);
	udc->speed = speed;

	switch (speed) {
	case USB_SPEED_SUPER:
		udc->ep0->maxpacket = 512;
		break;

	case USB_SPEED_HIGH:
	case USB_SPEED_FULL:
		udc->ep0->maxpacket = 64;
		break;

	case USB_SPEED_LOW:
		udc->ep0->maxpacket = 8;
		break;
	}

	diepcfg0 = ODIN_USB3_EPCFG0_EP_TYPE_CONTROL;
	diepcfg0 |= ODIN_USB3_EPCFG0_ACTION_MODIFY;
	diepcfg1 = ODIN_USB3_EPCFG1_XFER_CMPL_EN | ODIN_USB3_EPCFG1_XFER_IN_PROG_EN |
		   ODIN_USB3_EPCFG1_XFER_NRDY_EN | ODIN_USB3_EPCFG1_EP_DIR;

	doepcfg0 = ODIN_USB3_EPCFG0_EP_TYPE_CONTROL;
	doepcfg0 |= ODIN_USB3_EPCFG0_ACTION_MODIFY;
	doepcfg1 = ODIN_USB3_EPCFG1_XFER_CMPL_EN | ODIN_USB3_EPCFG1_XFER_IN_PROG_EN |
		   ODIN_USB3_EPCFG1_XFER_NRDY_EN;

	diepcfg0 |= ODIN_USB3_EPCFG0_MPS(udc->ep0->maxpacket);
	doepcfg0 |= ODIN_USB3_EPCFG0_MPS(udc->ep0->maxpacket);
	diepcfg0 |= ODIN_USB3_EPCFG0_TXFNUM(udc->ep0->tx_fifo_num);

	/* EP0-OUT */
	odin_udc_epcmd_set_cfg(udc, 0, doepcfg0, doepcfg1, 0);
	/* EP0-IN */
	odin_udc_epcmd_set_cfg(udc, 1, diepcfg0, diepcfg1, 0);

	if (udc->state == USB_STATE_NOTATTACHED)
		udc->state = USB_STATE_DEFAULT;

	udc->gadget.speed = speed;
	udc->ep0->ep.maxpacket = udc->ep0->maxpacket;
}

static void odin_udc_disconnect_intr(struct odin_udc *udc)
{
#ifdef CONFIG_USB_G_LGE_ANDROID
	if (udc->state != USB_STATE_NOTATTACHED) {
		odin_udc_connect(udc, 0);
		udc->state = USB_STATE_NOTATTACHED;
		udc->speed = USB_SPEED_UNKNOWN;
		odin_udc_gadget_disconnect(udc);
	}
#else
	odin_udc_connect(udc, 0);
	udc->state = USB_STATE_NOTATTACHED;
	udc->speed = USB_SPEED_UNKNOWN;
	odin_udc_gadget_disconnect(udc);
#endif
}

static void odin_udc_u3_l2l1_susp_intr(struct odin_udc *udc)
{
	u32 dsts, state;

	if (udc->snpsid < 0x5533230a)
		return;

	dsts = readl(udc->regs + ODIN_USB3_DSTS);
	state = ODIN_USB3_DSTS_LINK_STATE(dsts);

	ud_core("[%s]LINK state=%d\n", __func__, state);

	switch (state) {
	case ODIN_USB3_DSTS_LINK_STATE_U0:
		if (udc->link_state == ODIN_USB3_DSTS_LINK_STATE_U3) {
			ud_core("USB state U3 -> U0\n");
		}
		udc->link_state = state;
		break;

	case ODIN_USB3_DSTS_LINK_STATE_U3:
		if (udc->link_state != ODIN_USB3_DSTS_LINK_STATE_U3) {
			if (udc->driver && udc->driver->suspend)
				udc->driver->suspend(&udc->gadget);
		}
		udc->link_state = state;
		break;

	default:
		udc->link_state = state;
		break;
	}
}

static void odin_udc_erratic_error_intr(struct odin_udc *udc)
{
	odin_drd_dump_global_regs_reset(udc->dev->parent);
}

void odin_udc_ep_intr_handler(struct odin_udc *udc, int physep, u32 event)
{
	struct odin_udc_ep *odin_ep;
	int epnum, is_in;
	char *dir;

	is_in = physep & 1;
	epnum = physep >> 1 & 0xf;

	if (epnum != 0)
		epnum = physep - 1;

	ud_udc("[%s] physep = %x / epnum = %d\n", __func__, physep, epnum);
	odin_ep = &udc->eps[epnum];

	if (is_in)
		dir = "IN";
	else
		dir = "OUT";

	if (!odin_ep->active) {
		usb_err("[%s] EP%d-%s is not active \n", __func__, odin_ep->num, dir);
		return;
	}

	switch (event & ODIN_USB3_DEPEVT_EVENT_MASK) {
	case ODIN_USB3_DEPEVT_XFER_CMPL:
		ud_udc("[EP%d] %s xfer complete\n", epnum, dir);

		odin_ep->xfer_started = 0;

		if (odin_ep->type != USB_ENDPOINT_XFER_ISOC) {
			if (epnum == 0)
				odin_udc_handle_ep0_xfer(udc, event);
			else
				odin_udc_complete_request(udc, odin_ep, event);
		} else {
			ud_iso("[EP%d] %s xfer complete for ISOC EP!\n",
				   epnum, dir);
		}
		break;

	case ODIN_USB3_DEPEVT_XFER_IN_PROG:
		ud_udc("[EP%d] %s xfer in progress\n", epnum, dir);

		if (odin_ep->type == USB_ENDPOINT_XFER_ISOC) {
			iso_dbg("Complete trans");
			odin_udc_complete_request(udc, odin_ep, event);
		} else {
			if (epnum == 0)
				odin_udc_handle_ep0_xfer(udc, event);
			else
				odin_udc_complete_request(udc, odin_ep, event);
		}
		break;

	case ODIN_USB3_DEPEVT_XFER_NRDY:
		ud_udc("[EP%d] %s xfer not ready\n", epnum, dir);
		if (odin_ep->type == USB_ENDPOINT_XFER_ISOC) {
			odin_udc_isoc_xfer_start(udc, odin_ep, event);
		} else {
			if (epnum == 0) {
				switch (udc->ep0state) {
				case EP0_IN_WAIT_GADGET:
				case EP0_IN_WAIT_NRDY:
					if (is_in)
						odin_udc_handle_ep0_xfer(udc, event);
					else {
						if(lge_get_factory_boot())
							usb_err("[EP%d] %s xfer not ready(ep0state %d)\n",
									epnum, dir, udc->ep0state);
					}
					break;
				case EP0_OUT_WAIT_GADGET:
				case EP0_OUT_WAIT_NRDY:
					if (!is_in)
						odin_udc_handle_ep0_xfer(udc, event);
					else {
						if(lge_get_factory_boot())
							usb_err("[EP%d] %s xfer not ready(ep0state %d)\n",
									epnum, dir, udc->ep0state);
					}
					break;
				default:
					break;
				}
			}
		}
		break;

	case ODIN_USB3_DEPEVT_FIFOXRUN:
		usb_err("[EP%d] %s FIFO Underrun Error!\n", epnum, dir);
		break;

	case ODIN_USB3_DEPEVT_EPCMD_CMPL:
		ud_udc("[EP%d] %s Command Complete!\n", epnum, dir);
		odin_udc_handle_depevt_epcmd_cmpl(udc, odin_ep, event);
		break;

	default:
		usb_print("DEPEVT Unknown event![EP%d] %s \n", epnum, dir);
		break;
	}
}

void odin_udc_dev_intr_handler(struct odin_udc *udc, u32 event)
{
	switch (event & ODIN_USB3_DEVT_EVENT_MASK) {
	case ODIN_USB3_DEVT_DISCONN:
		usb_print("DISCONNECT\n");
		odin_udc_disconnect_intr(udc);
		break;

	case ODIN_USB3_DEVT_USBRESET:
		usb_print("USB RESET\n");
		odin_udc_usb_reset_intr(udc);
		break;

	case ODIN_USB3_DEVT_CONNDONE:
		usb_print("CONNECT\n");
		odin_udc_connect_done_intr(udc);
		break;

	case ODIN_USB3_DEVT_U3_L2L1_SUSP:
		usb_print("U3/L2-L1 SUSPEND\n");
		odin_udc_u3_l2l1_susp_intr(udc);
		break;

	case ODIN_USB3_DEVT_ERRATICERR:
		usb_print("ERRATIC ERROR\n");
		odin_udc_erratic_error_intr(udc);
		break;

	default:
		usb_print("DEVT Unknown event!\n");
		break;
	}
}

static irqreturn_t odin_udc_irq(int irq, void *dev)
{
	struct odin_udc *udc = dev;
	u32 event;
	int count, intr, physep, i;

	iso_dbg("IRQ");

	spin_lock(&udc->lock);

	count = odin_drd_get_eventbuf(udc->dev->parent);

	for (i = 0; i < count; i += 4) {
		/*usb_dbg("Event addr 0x%08lx\n",
			   (unsigned long)dev->event_ptr[0]);*/

		event = *udc->event_ptr++;
		if (udc->event_ptr >=
			udc->event_buf + ODIN_USB3_EVENT_BUF_SIZE)
			udc->event_ptr = udc->event_buf;

		odin_drd_set_eventbuf(udc->dev->parent, 4);

		if (event == 0) {
			usb_dbg("Null event\n");
			continue;
		}

		/*usb_dbg("Interrupt event 0x%08x\n", event);*/
		if (event & ODIN_USB3_EVENT_NON_EP_MASK) {
			intr = (event & ODIN_USB3_EVENT_MASK)
						>> ODIN_USB3_EVENT_SHIFT;
			switch (intr) {
				case ODIN_USB3_EVENT_DEV_INT:
					/*usb_dbg("*NONEP* DEV INT (0x%08x)\n", event);*/
					odin_udc_dev_intr_handler(udc, event);
				break;

				case ODIN_USB3_EVENT_OTG_INT:
					usb_dbg("*NONEP* OTG INT (0x%08x)\n", event);
					/* TODO */
				break;

				case ODIN_USB3_EVENT_CARKIT_INT:
				case ODIN_USB3_EVENT_I2C_INT:
					usb_dbg("*NONEP* CORE INT (0x%08x)\n", event);
					/* TODO */
				break;
			}
		} else {
			physep = ODIN_USB3_DEPEVT_EPNUM(event);
			/*usb_dbg("*DEPEVT* [EP%d] %s : 0x%08x \n", physep >> 1 & 0xf,
				   physep & 1 ? "IN" : "OUT", event);*/
			odin_udc_ep_intr_handler(udc, physep, event);
		}
	}
	spin_unlock(&udc->lock);

	return IRQ_HANDLED;
}

static void odin_udc_ep_free(struct odin_udc *udc)
{
	struct odin_udc_ep *odin_ep;
	int epnum;

	for (epnum = 1; epnum < udc->num_eps; epnum++) {
		odin_ep = &udc->eps[epnum - 1];
		ud_core("EP FREE num%d = %p\n", epnum, odin_ep);
		odin_udc_trb_free(odin_ep);
		list_del(&odin_ep->ep.ep_list);
	}
}

static int odin_udc_ep_init(struct odin_udc *udc)
{
	struct odin_udc_ep *odin_ep;
	struct usb_ep *ep0, *ep;
	int epnum, ep_in = 0, ep_out = 0;

	ug_dbg("[%s]\n", __func__);

	INIT_LIST_HEAD(&udc->gadget.ep_list);

	for (epnum = 1; epnum < udc->num_eps; epnum++) {
		if (epnum == 1) {
			snprintf(udc->ep_names[epnum], sizeof(udc->ep_names[epnum]),
					"ep%d", epnum >> 1);
			ud_core("EP%d name=%s\n", epnum, udc->ep_names[epnum]);

			odin_ep = &udc->eps[0];
			ep0 = &odin_ep->ep;
			udc->gadget.ep0 = ep0;
			udc->ep0 = odin_ep;

			odin_ep->is_in = 0;
			odin_ep->phys = 0;
			odin_ep->num = 0;
			odin_ep->tx_fifo_num = 0;

			INIT_LIST_HEAD(&udc->gadget.ep0->ep_list);
			INIT_LIST_HEAD(&odin_ep->queue);

			ep0->name = udc->ep_names[0];
			ep0->ops = (struct usb_ep_ops *)&odin_udc_ep_ops;
			ep0->maxpacket = ODIN_USB3_MAX_EP0_SIZE;
		} else {
#if CONFIG_ARCH_ODIN_REVISION==2
			snprintf(udc->ep_names[epnum], sizeof(udc->ep_names[epnum]),
					"ep%d%s", epnum >> 1, (epnum & 1) ? "in" : "out");
			ud_core("EP%d name=%s\n", epnum, udc->ep_names[epnum]);
			odin_ep = &udc->eps[epnum - 1];

			if (epnum & 1)
#else
			if (epnum > 3)
#endif
			{
				ep_in++;
#if CONFIG_ARCH_ODIN_REVISION!=2
				snprintf(udc->ep_names[epnum], sizeof(udc->ep_names[epnum]),
						"ep%d%s", ep_in, "in");
#endif
				ud_core("ep%d-IN name=%s phys=%d odin_ep=%p\n",
					ep_in, udc->ep_names[epnum], ep_in, odin_ep);
				odin_ep->num = ep_in;
				odin_ep->is_in = 1;
				odin_ep->phys = ep_in << 1 | 1;
				odin_ep->tx_fifo_num = ep_in;
			} else {
				ep_out++;
#if CONFIG_ARCH_ODIN_REVISION!=2
				snprintf(udc->ep_names[epnum], sizeof(udc->ep_names[epnum]),
						"ep%d%s", ep_out, "out");
#endif
				ud_core("ep%d-OUT name=%s phys=%d odin_ep=%p\n",
					  ep_out, udc->ep_names[epnum], ep_out, odin_ep);
				odin_ep->num = ep_out;
				odin_ep->is_in = 0;
				odin_ep->phys = ep_out << 1;
				odin_ep->tx_fifo_num = 0;
			}

			INIT_LIST_HEAD(&odin_ep->queue);
			ep = &odin_ep->ep;
			ep->name = udc->ep_names[epnum];
			ep->ops = (struct usb_ep_ops *)&odin_udc_ep_ops;
			ep->maxpacket = ODIN_USB3_MAX_PACKET_SIZE;
			ep->max_streams = 15;

			list_add_tail(&ep->ep_list, &udc->gadget.ep_list);
		}

		odin_ep->udc = udc;
		odin_ep->active = 0;
		odin_ep->dma_desc = NULL;
		odin_ep->dma_desc_dma = 0;
		odin_ep->usb_ep_desc = NULL;
		odin_ep->stopped = 1;
		odin_ep->type = USB_ENDPOINT_XFER_CONTROL;
		odin_ep->maxpacket = ODIN_USB3_MAX_EP0_SIZE;
		odin_ep->send_zlp = 0;
		odin_ep->stream_capable = 0;
		odin_ep->pending_req = 0;
		odin_ep->wedge = 0;
	}
	udc->ep0state = EP0_IDLE;

	udc->ep0_req.buf = NULL;
	udc->ep0_req.bufdma = 0;
	udc->ep0_req.buflen = 0;
	udc->ep0_req.length = 0;
	udc->ep0_req.actual = 0;
	return 0;
}

int odin_udc_param_init(struct odin_udc *udc)
{
	u32 hwparams3;
	hwparams3 = readl(udc->gregs + ODIN_USB3_HWPARAMS3);

	udc->num_eps = ODIN_USB3_HWP3_NUM_EPS(hwparams3);

	if(udc->num_eps != ODIN_USB3_DEFAULT_EP_NUM) {
		usb_err("[%s] num_eps(%d) is not default\n", __func__, udc->num_eps);
		return -EINVAL;
	}

	udc->num_in_eps =
			ODIN_USB3_HWP3_NUM_IN_EPS(hwparams3) - 1;
	udc->num_out_eps = udc->num_eps -udc->num_in_eps -2;
	usb_print("num_eps = %d, num_in_eps =%d num_out_eps = %d\n",
		udc->num_eps, udc->num_in_eps, udc->num_out_eps);

	if (of_property_read_u32(udc->dev->parent->of_node,
				"speed_mode", &udc->speed_mode) != 0)
		dev_err(udc->dev, "Can't read of property!\n");
	usb_dbg("%s: speed_mode = %d\n", __func__, udc->speed_mode);

	udc->speed = USB_SPEED_UNKNOWN;

	udc->snpsid = readl(udc->gregs + ODIN_USB3_SNPSID);

	return 0;
}

static int odin_udc_probe(struct platform_device *pdev)
{
	struct odin_udc *udc;
	struct device *dev = &pdev->dev;
	struct resource *res_mem;
	int irq, ret = 0;

	ud_core("ODIN USB SS UDC \n");

	udc = devm_kzalloc(dev, sizeof(struct odin_udc), GFP_KERNEL);
	if (!udc) {
		dev_err(&pdev->dev, "Memory allocation failed for Odin UDC \n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, udc);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Get irq failed!");
		return irq;
	}
	udc->dev = dev;
	udc->irq = irq;
	udc->udc_enabled = 0;

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem) {
		dev_err(dev, "Get platform resource failed!");
		return -ENXIO;
	}

	if (!devm_request_mem_region(dev, res_mem->start, resource_size(res_mem),
				"odin-udc")) {
		dev_err(dev, "request_mem_region() failed!\n");
		return -ENOENT;
	}

	udc->regs = devm_ioremap_nocache(dev, res_mem->start,
									resource_size(res_mem));
	if (!udc->regs) {
		dev_err(dev, "ioremap_nocache() failed!\n");
		return -ENOMEM;
	}

	udc->gregs = (volatile u32 __iomem *)(udc->regs - 0x600);
	pm_runtime_get_sync(dev->parent);

	ret = devm_request_irq(dev, udc->irq, odin_udc_irq,
			     IRQF_SHARED, "odn_udc", udc);
	if (ret < 0) {
		dev_err(dev, "request of irq%d failed!\n", udc->irq);
		return ret;
	}

	udc->event_buf = dma_alloc_coherent(NULL, ODIN_USB3_EVENT_BUF_SIZE,
						&udc->event_buf_dma,
						GFP_KERNEL | GFP_DMA32);
	if (!udc->event_buf) {
		dev_err(dev, "Memory allocation of event_buf failed!\n");
		return -ENOMEM;
	}

	ret = odin_udc_memory_alloc(udc);
	if (ret){
		dev_err(dev, "DMA memory allocation failed! \n");
		goto out1;
	}

	ret = odin_udc_param_init(udc);
	if (ret) {
		dev_err(dev, "parameter initialization failed!\n");
		goto out2;
	}

	spin_lock_init(&udc->lock);
	wake_lock_init(&udc->wake_lock, WAKE_LOCK_SUSPEND, "odin-usb");

	ret = odin_udc_ep_init(udc);
	if (ret) {
		dev_err(dev, "endpoints initialization failed!\n");
		goto out2;
	}

	ret = usb_add_gadget_udc(dev, &udc->gadget);
	if (ret) {
		dev_err(dev, "failed to register udc\n");
		goto out3;
	}
	udc->gadget.ops = &odin_gadget_ops;
	udc->gadget.speed = USB_SPEED_UNKNOWN;
	if (udc->speed_mode == USB3_SPEED_MODE_HS)
		udc->gadget.max_speed = USB_SPEED_HIGH;
	else
		udc->gadget.max_speed = USB_SPEED_SUPER;
	udc->gadget.is_otg = 1;
	udc->gadget.name = "odin_udc";

	udc->odin_otg = odin_usb3_get_otg(pdev);
	ret = otg_set_peripheral(&udc->odin_otg->otg, &udc->gadget);
	if (ret) {
		dev_err(dev, "failed to set peripheral\n");
		goto out4;
	}

	pm_runtime_put_sync(dev->parent);

	printk(KERN_INFO"[%s] Done!! \n", __func__);
	return 0;

out4:
	usb_del_gadget_udc(&udc->gadget);
out3:
	odin_udc_ep_free(udc);
	wake_lock_destroy(&udc->wake_lock);
out2:
	odin_udc_memory_free(udc);
out1:
	dma_free_coherent(NULL, ODIN_USB3_EVENT_BUF_SIZE, udc->event_buf,
			  udc->event_buf_dma);
	return ret;
}

static int odin_udc_remove(struct platform_device *pdev)
{
	struct odin_udc *udc = platform_get_drvdata(pdev);
	ud_core("%s(%p)\n", __func__, pdev);

	otg_set_peripheral(&udc->odin_otg->otg, NULL);

	usb_del_gadget_udc(&udc->gadget);

	odin_udc_ep_free(udc);

	wake_lock_destroy(&udc->wake_lock);

	odin_udc_memory_free(udc);

	dma_free_coherent(NULL, ODIN_USB3_EVENT_BUF_SIZE, udc->event_buf,
			  udc->event_buf_dma);

	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM
 static int odin_udc_suspend(struct platform_device *pdev,
				 pm_message_t state)
{
	struct odin_udc *udc =  platform_get_drvdata(pdev);

	if (udc->driver && udc->driver->suspend)
		udc->driver->suspend(&udc->gadget);

	return 0;
}

static int odin_udc_resume(struct platform_device *pdev)
{
	struct odin_udc *udc =  platform_get_drvdata(pdev);

	if (udc->driver && udc->driver->suspend)
		udc->driver->resume(&udc->gadget);

	return 0;
}
#else
#define odin_udc_suspend NULL
#define odin_udc_resume NULL
#endif /* CONFIG_PM */

static const struct platform_device_id odin_udc_platform_ids[] = {
	{ "odin-udc", 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, odin_udc_platform_ids);

static struct platform_driver odin_udc_platform_driver = {
	.id_table	= odin_udc_platform_ids,
	.probe		= odin_udc_probe,
	.remove		= odin_udc_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "odin-udc",
	},
	.suspend = odin_udc_suspend,
	.resume = odin_udc_resume,
};

module_platform_driver(odin_udc_platform_driver);

MODULE_AUTHOR("Wonsuk Chang <wonsuk.chang@lge.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LGE USB3.0 UDC");

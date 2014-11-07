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
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/irq.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>
#include <linux/usb/composite.h>
#include <linux/usb/odin_usb3.h>
#include "odin_udc.h"

#ifdef ODIN_USB_ISO_TRANSFER_DEBUG
#define ISODBGBUF 10000
static char iso_dbg_msg[ISODBGBUF][30];
static int n_iso = 0;
void odin_udc_dbg_iso(char *str)
{
	int i;
	char temp[12];
	unsigned long long t;
	int this_cpu = smp_processor_id();

	t = cpu_clock(this_cpu);
	sprintf(temp, "[%5lu]--", (unsigned long)t);
	strncpy(&iso_dbg_msg[n_iso][0], temp, 13);
	strncpy(&iso_dbg_msg[n_iso][13], str, 17);
	n_iso++;

	if (n_iso > ISODBGBUF) {
		usb_print("\n-------------[START]-------------\n");
		for (i = 0; i < (ISODBGBUF+1); i++)
			usb_print("%s \n", iso_dbg_msg[i]);
		usb_print("-------------[END]-------------\n");
	}
}
#endif

void odin_udc_dump_dev_regs(struct odin_udc *udc)
{
	volatile u32 __iomem *addr;

	usb_print("[[Dump Device Registers]]\n");
	addr = udc->regs + ODIN_USB3_DCFG;
	usb_print("DCFG		@0x%08lx : 0x%08x\n",
		   (unsigned long)addr, readl(addr));
	addr = udc->regs + ODIN_USB3_DCTL;
	usb_print("DCTL		@0x%08lx : 0x%08x\n",
		   (unsigned long)addr, readl(addr));
	addr = udc->regs + ODIN_USB3_DSTS;
	usb_print("DSTS		@0x%08lx : 0x%08x\n",
		   (unsigned long)addr, readl(addr));
}

int odin_udc_handshake(void __iomem *ptr, u32 val)
{
	u32 timeout = ODIN_UDC_HANDSHAKE_TIMEOUT;
	int val_ret = 0;

	do {
		val_ret = readl(ptr);
		if ((val_ret & val) == 0) {
			return 0;
		}

		udelay(1);
		timeout -= 1;
	} while (timeout > 0);

	ud_core("[%s] timeout\n", __func__);

	return -ETIME;
}

#ifdef CONFIG_USB_G_LGE_ANDROID
int odin_udc_check_regs(void __iomem *ptr, bool exist,
							u32 val, u32 time_us)
{
	int val_ret = 0;
	u32 timeout = time_us;

	do {
		val_ret = readl(ptr);
		if(exist) {
			if (val_ret & val) {
				return 0;
			}
		}
		else {
			if (!(val_ret & val)) {
				return 0;
			}
		}

		udelay(1);
		timeout -= 1;
	} while (timeout > 0);

	ud_core("[%s] timeout\n", __func__);

	return -ETIME;
}
#endif

unsigned odin_udc_ep_to_epnum(struct odin_udc_ep *odin_ep)
{
	unsigned epnum;

	if (odin_ep->num == 0) {
		if (odin_ep->is_in)
			epnum = 1;
		else
			epnum = 0;
	} else {
		epnum = odin_ep->phys;
	}
	return epnum;
}

void odin_udc_fill_trb(struct odin_udc_trb *trb, dma_addr_t dma_addr,
			u32 dma_len, u32 type, u32 ctrlbits, int own)
{
	trb->bptl = (u32)(dma_addr & 0xffffffffU);
#ifdef CONFIG_ODIN_USB3_64_BIT_ARCH
	trb->bpth = (u32)(dma_addr >> 32U & 0xffffffffU);
#else
	trb->bpth = 0;
#endif
	trb->status = ODIN_USB3_DSCSTS_LENGTH(dma_len);

	/* Note: If type is 0, leave original control bits intact (for isoc) */
	if (type)
		trb->control = ODIN_USB3_DSCCTL_TRBCTL(type);

	trb->control |= ctrlbits;

	/* Must do this last! */
	if (own) {
		wmb();
		trb->control |= ODIN_USB3_DSCCTL_HWO;
	}
}

void odin_udc_epcmd_act(struct odin_udc *udc, unsigned epnum,
		struct odin_usb3_ep_cmd_params *params, unsigned cmd)
{
	writel(params->param0,
			udc->regs + ODIN_USB3_DEPCMDPAR0(epnum));
	writel(params->param1,
			udc->regs + ODIN_USB3_DEPCMDPAR1(epnum));
	writel(params->param2,
			udc->regs + ODIN_USB3_DEPCMDPAR2(epnum));
	writel(cmd | ODIN_USB3_EPCMD_ACT,
			udc->regs + ODIN_USB3_DEPCMD(epnum));

	odin_udc_handshake(udc->regs + ODIN_USB3_DEPCMD(epnum),
			ODIN_USB3_EPCMD_ACT);
}

void odin_udc_epcmd_set_cfg(struct odin_udc *udc,
		unsigned epnum, u32 depcfg0, u32 depcfg1, u32 depcfg2)
{
	struct odin_usb3_ep_cmd_params params;
	memset(&params, 0x00, sizeof(params));

	params.param0 = depcfg0;
	params.param1 = depcfg1;
	params.param2 = depcfg2;

	return odin_udc_epcmd_act(udc, epnum, &params,
			ODIN_USB3_EPCMD_SET_EP_CFG);
}

void odin_udc_epcmd_set_xfercfg(struct odin_udc *udc,
			unsigned epnum, u32 depstrmcfg)
{
	struct odin_usb3_ep_cmd_params params;
	memset(&params, 0x00, sizeof(params));

	params.param0 = depstrmcfg;

	return odin_udc_epcmd_act(udc, epnum, &params,
			ODIN_USB3_EPCMD_SET_XFER_CFG);
}

int odin_udc_epcmd_start_xfer(struct odin_udc *udc,
			struct odin_udc_ep *odin_ep,
			dma_addr_t dma_addr, u32 stream_or_uf)
{
	struct odin_usb3_ep_cmd_params params;
	unsigned epnum = odin_udc_ep_to_epnum(odin_ep);
	u32 depcmd;
	memset(&params, 0x00, sizeof(params));

#ifdef CONFIG_ODIN_USB3_64_BIT_ARCH
	params.param0 = dma_addr >> 32U & 0xffffffffU;
	ud_udc("TDADDRHI=%08lx\n",
		   (unsigned long)(dma_addr >> 32U & 0xffffffffU));
#else
	params.param0 = 0;
#endif
	params.param1 = dma_addr & 0xffffffffU;
	ud_udc("TDADDRLO=%08lx\n",
		   (unsigned long)(dma_addr & 0xffffffffU));

	depcmd = ODIN_USB3_EPCMD_STR_NUM_OR_UF(stream_or_uf) |
			ODIN_USB3_EPCMD_START_XFER | ODIN_USB3_EPCMD_ACT;

	if (odin_ep->type == USB_ENDPOINT_XFER_ISOC)
		depcmd |= ODIN_USB3_EPCMD_IOC | ODIN_USB3_EPCMD_HP_FRM;

	odin_udc_epcmd_act(udc, epnum, &params, depcmd);

	depcmd = readl(udc->regs + ODIN_USB3_DEPCMD(epnum));
	return ODIN_USB3_EPCMD_XFER_RSRC(depcmd);
}

void odin_udc_epcmd_update_xfer(struct odin_udc *udc,
			unsigned epnum, u32 tri)
{
	struct odin_usb3_ep_cmd_params params;
	memset(&params, 0x00, sizeof(params));

	return odin_udc_epcmd_act(udc, epnum, &params,
			ODIN_USB3_EPCMD_XFER_RSRC_IDX(tri) |
			ODIN_USB3_EPCMD_UPDATE_XFER);
}

void odin_udc_epcmd_end_xfer(struct odin_udc *udc,
			struct odin_udc_ep *odin_ep, u32 tri, int flags)
{
	struct odin_usb3_ep_cmd_params params;
	unsigned epnum = odin_udc_ep_to_epnum(odin_ep);
	u32 depcmd;
	memset(&params, 0x00, sizeof(params));

	depcmd = ODIN_USB3_EPCMD_XFER_RSRC_IDX(tri) | ODIN_USB3_EPCMD_END_XFER;
	depcmd |= ODIN_USB3_EPCMD_ACT;
	if (flags & ODIN_USB3_ENDXFER_FORCE)
		depcmd |= ODIN_USB3_EPCMD_HP_FRM;
	if (flags & ODIN_USB3_ENDXFER_ISO)
		depcmd |= ODIN_USB3_EPCMD_IOC;

	odin_udc_epcmd_act(udc, epnum, &params, depcmd);

	if (!(flags & ODIN_USB3_ENDXFER_NODELAY))
		udelay(100);
}

void odin_udc_epcmd_set_stall(struct odin_udc *udc,
						struct odin_udc_ep *odin_ep)
{
	struct odin_usb3_ep_cmd_params params;
	memset(&params, 0x00, sizeof(params));

	return odin_udc_epcmd_act(udc,
			odin_udc_ep_to_epnum(odin_ep), &params,
			ODIN_USB3_EPCMD_SET_STALL);
}

void odin_udc_epcmd_clr_stall(struct odin_udc *udc,
						struct odin_udc_ep *odin_ep)
{
	struct odin_usb3_ep_cmd_params params;
	memset(&params, 0x00, sizeof(params));

	return odin_udc_epcmd_act(udc,
			odin_udc_ep_to_epnum(odin_ep), &params,
			ODIN_USB3_EPCMD_CLR_STALL);
}

void odin_udc_epcmd_start_newcfg(struct odin_udc *udc,
			unsigned epnum, u32 rsrcidx)
{
	struct odin_usb3_ep_cmd_params params;
	memset(&params, 0x00, sizeof(params));

	return odin_udc_epcmd_act(udc, epnum, &params,
			ODIN_USB3_EPCMD_XFER_RSRC_IDX(rsrcidx) |
			ODIN_USB3_EPCMD_START_NEW_CFG);
}

int odin_udc_flush_all_fifo(struct odin_udc *udc)
{
	writel(ODIN_USB3_DGCMD_ALL_FIFO_FLUSH |
			ODIN_USB3_DGCMD_CMDACT,
			udc->regs + ODIN_USB3_DGCMD);
	odin_udc_handshake(udc->regs + ODIN_USB3_DGCMD,
			ODIN_USB3_DGCMD_CMDACT);
	return 0;
}

int odin_udc_get_device_speed(struct odin_udc *udc)
{
	u32 dsts;
	int speed = USB_SPEED_UNKNOWN;

	dsts = readl(udc->regs + ODIN_USB3_DSTS);

	switch (dsts & ODIN_USB3_DSTS_CONNSPD) {
	case ODIN_USB3_SPEED_HS_30OR60:
		ud_udc("HIGH SPEED\n");
		speed = USB_SPEED_HIGH;
		break;

	case ODIN_USB3_SPEED_FS_30OR60:
	case ODIN_USB3_SPEED_FS_48:
		ud_udc("FULL SPEED\n");
		speed = USB_SPEED_FULL;
		break;

	case ODIN_USB3_SPEED_LS_6:
		ud_udc("LOW SPEED\n");
		speed = USB_SPEED_LOW;
		break;

	case ODIN_USB3_SPEED_SS_125OR250:
		ud_udc("SUPER SPEED\n");
		speed = USB_SPEED_SUPER;
		break;
	}
	return speed;
}

int odin_udc_set_link_state(struct odin_udc *udc, int state)
{
	u32 dctl;

	if (odin_udc_handshake(udc->regs + ODIN_USB3_DSTS,
					ODIN_USB3_DSTS_DCNRD))
		return -ETIME;

	dctl = readl(udc->regs + ODIN_USB3_DCTL);
	dctl &= ~ODIN_USB3_DCTL_ULST_CHNG_REQ_MASK;
	dctl |= ODIN_USB3_DCTL_ULST_CHNG_REQ(state);
	writel(dctl, udc->regs + ODIN_USB3_DCTL);
	return 0;
}

void odin_udc_fill_trbs(struct odin_udc *udc,
		struct odin_udc_ep *odin_ep, struct odin_usb3_req *req)
{
	struct odin_udc_trb *trb;
	dma_addr_t trb_dma;
	u32 len, tlen, pkts, ctrl;

	if (odin_ep == udc->ep0)
		return;

	/* Get the next DMA Descriptor (TRB) for this EP */
	trb = odin_ep->dma_desc + odin_ep->desc_idx;
	trb_dma = (dma_addr_t)((unsigned long)odin_ep->dma_desc_dma +
		(unsigned long)odin_ep->desc_idx * 16);

	if (++odin_ep->desc_idx >= odin_ep->num_desc)
		odin_ep->desc_idx = 0;
	odin_ep->desc_avail--;

	req->trb = trb;
	req->trbdma = trb_dma;

	pkts = 0;

	if (odin_ep->is_in) {
		/* For IN, TRB length is just xfer length */
		len = req->length;

		if (odin_ep->type == USB_ENDPOINT_XFER_ISOC &&
				udc->speed == USB_SPEED_HIGH) {
			pkts = (len + odin_ep->maxpacket - 1)
					/ odin_ep->maxpacket;
			if (pkts)
				pkts--;
		}
	} else {
		/* For OUT, TRB length must be multiple of maxpacket */
		if ((odin_ep->type == USB_ENDPOINT_XFER_ISOC ||
				odin_ep->type == USB_ENDPOINT_XFER_INT) &&
						odin_ep->maxpacket != 1024)
			/* Might not be power of 2, so use (expensive?)
			 * divide/multiply
			 */
			len = ((req->length + odin_ep->maxpacket - 1)
			       / odin_ep->maxpacket) * odin_ep->maxpacket;
		else
			/* Must be power of 2, use cheap AND */
			len = (req->length + odin_ep->maxpacket - 1)
			      & ~(odin_ep->maxpacket - 1);

		req->length = len;
	}

	/* DMA Descriptor Setup */
	if (odin_ep->type == USB_ENDPOINT_XFER_ISOC) {
		tlen = len;
		tlen |= ODIN_USB3__DSCSTS_PCM1(pkts);
		odin_udc_fill_trb(trb, req->bufdma, tlen,
				   0, 0, 1);
	} else {
		ctrl = ODIN_USB3_DSCCTL_LST;
		if (odin_ep->type == USB_ENDPOINT_XFER_BULK &&
				odin_ep->stream_capable == 1) {
			ctrl |= ODIN_USB3_DSCCTL_STRMID_SOFN(req->usb_req.stream_id);
		}
		tlen = len;
		odin_udc_fill_trb(trb, req->bufdma, tlen,
				ODIN_USB3_DSCCTL_TRBCTL_NORMAL, ctrl, 1);
	}
}


void odin_udc_trb_free(struct odin_udc_ep *odin_ep)
{
	struct odin_udc_trb *trb;
	dma_addr_t trb_dma;
	int size;

	if (odin_ep->dma_desc) {
		trb = odin_ep->dma_desc;
		trb_dma = odin_ep->dma_desc_dma;
		size = odin_ep->desc_size;
		odin_ep->dma_desc = NULL;
		odin_ep->dma_desc_dma = 0;

		dma_free_coherent(NULL, size, (void *)trb, trb_dma);
	}
}

void odin_udc_ep0_activate(struct odin_udc *udc)
{
	u32 diepcfg0, doepcfg0, diepcfg1, doepcfg1;
	struct odin_udc_ep *odin_ep =  udc->ep0;

	diepcfg0 = ODIN_USB3_EPCFG0_EP_TYPE_CONTROL;
	diepcfg1 = ODIN_USB3_EPCFG1_XFER_CMPL_EN |
			ODIN_USB3_EPCFG1_XFER_IN_PROG_EN |
			ODIN_USB3_EPCFG1_XFER_NRDY_EN |
			ODIN_USB3_EPCFG1_EP_DIR;

	doepcfg0 = ODIN_USB3_EPCFG0_EP_TYPE_CONTROL;
	doepcfg1 = ODIN_USB3_EPCFG1_XFER_CMPL_EN |
			ODIN_USB3_EPCFG1_XFER_IN_PROG_EN |
			ODIN_USB3_EPCFG1_XFER_NRDY_EN;

	/* Default to MPS of 512 (will reconfigure after ConnectDone event) */
	diepcfg0 |= ODIN_USB3_EPCFG0_MPS(512);
	doepcfg0 |= ODIN_USB3_EPCFG0_MPS(512);

	diepcfg0 |= ODIN_USB3_EPCFG0_TXFNUM(odin_ep->tx_fifo_num);

	if ((udc->snpsid & 0xffff) >= 0x109a) {
		/* EP0-OUT */
		odin_udc_epcmd_start_newcfg(udc, 0, 0);
	}

	/* EP0-OUT */
	odin_udc_epcmd_set_cfg(udc, 0, doepcfg0, doepcfg1, 0);
	odin_udc_epcmd_set_xfercfg(udc, 0, 1);

	/* EP0-IN */
	odin_udc_epcmd_set_cfg(udc, 1, diepcfg0, diepcfg1, 0);
	odin_udc_epcmd_set_xfercfg(udc, 1, 1);
	odin_ep->active = 1;
}

int odin_udc_ep_activate(struct odin_udc *udc, struct odin_udc_ep *odin_ep,
		const struct usb_ss_ep_comp_descriptor *comp_desc)
{
	u32 depcfg0, depcfg1, depcfg2 = 0;
	u32 ep_index_num, dalepena;
	int maxburst;

	if (!udc->eps_enabled) {
		udc->eps_enabled = 1;

		depcfg0 = ODIN_USB3_EPCFG0_EP_TYPE_CONTROL;
		depcfg0 |= ODIN_USB3_EPCFG0_ACTION_MODIFY;
		depcfg1 = ODIN_USB3_EPCFG1_XFER_CMPL_EN |
				ODIN_USB3_EPCFG1_XFER_IN_PROG_EN |
				ODIN_USB3_EPCFG1_XFER_NRDY_EN |
				ODIN_USB3_EPCFG1_EP_DIR;

		switch (udc->speed) {
		case USB_SPEED_SUPER:
			depcfg0 |= ODIN_USB3_EPCFG0_MPS(512);
			break;

		case USB_SPEED_HIGH:
		case USB_SPEED_FULL:
			depcfg0 |= ODIN_USB3_EPCFG0_MPS(64);
			break;

		case USB_SPEED_LOW:
			depcfg0 |= ODIN_USB3_EPCFG0_MPS(8);
			break;
		}

		/* EP0-IN */
		odin_udc_epcmd_set_cfg(udc, 1, depcfg0, depcfg1, 0);

		if ((udc->snpsid & 0xffff) >= 0x109a) {
			/* EP0-OUT */
			odin_udc_epcmd_start_newcfg(udc, 0, 2);
		}
	}

	depcfg0 = ODIN_USB3_EPCFG0_EP_TYPE(odin_ep->type);
	depcfg0 |= ODIN_USB3_EPCFG0_MPS(odin_ep->maxpacket);
	if (udc->speed == USB_SPEED_SUPER) {
		maxburst = odin_ep->ep.maxburst -1;
		usb_dbg("SS Setting - maxburst = %d(%d), maxstreams  = %d\n",
			maxburst, comp_desc->bMaxBurst, comp_desc->bmAttributes);
		depcfg0 |= ODIN_USB3_EPCFG0_BRSTSIZ(maxburst);
	}
	if (odin_ep->is_in)
		depcfg0 |= ODIN_USB3_EPCFG0_TXFNUM(odin_ep->tx_fifo_num);

	depcfg1 = ODIN_USB3_EPCFG0_DSNUM(odin_ep->num);
	if (odin_ep->is_in)
		depcfg1 |= ODIN_USB3_EPCFG1_EP_DIR;
	depcfg1 |= ODIN_USB3_EPCFG1_XFER_CMPL_EN |
			ODIN_USB3_EPCFG1_XFER_IN_PROG_EN |
			ODIN_USB3_EPCFG1_XFER_NRDY_EN;

	ud_iso("Setting bInterval-1 to %u\n", odin_ep->intvl);
	depcfg1 |= ODIN_USB3_EPCFG1_BINTERVAL_M1(odin_ep->intvl);

	if (usb_ss_max_streams(comp_desc) &&
			odin_ep->type == USB_ENDPOINT_XFER_BULK) {
		ud_core("SS - Setting stream-capable bit\n");
		depcfg1 |= ODIN_USB3_EPCFG1_STRM_CAP_EN |
		/* TODO - Check Again */
		ODIN_USB3_EPCFG1_STREAM_EVENT_EN;
		odin_ep->stream_capable = 1;
	}

	odin_udc_epcmd_set_cfg(udc, odin_ep->phys, depcfg0, depcfg1, depcfg2);

	if (!odin_ep->active) {
		odin_udc_epcmd_set_xfercfg(udc, odin_ep->phys, 1);

		ep_index_num = odin_ep->num * 2;
		if (odin_ep->is_in)
			ep_index_num += 1;

		dalepena = readl(udc->regs + ODIN_USB3_DALEPENA);
		ud_udc("[%s]dalepena = %x (ep_index_num = %d) \n",
				__func__, dalepena, ep_index_num);
		if (dalepena & 1 << ep_index_num) {
			dev_err(udc->dev, "ep%d-%s have already enabled \n",
				odin_ep->num, odin_ep->is_in ? "IN" : "OUT");
			return -EBUSY;
		}

		dalepena |= 1 << ep_index_num;
		writel(dalepena, udc->regs + ODIN_USB3_DALEPENA);

		odin_ep->active = 1;
	}

	return 0;
}


void odin_udc_ep0_out_start(struct odin_udc *udc)
{
	struct odin_udc_trb *trb;
	dma_addr_t trb_dma;
	u8 tri;

	trb = udc->ep0_setup_trb;
	trb_dma = udc->ep0_setup_trb_dma;

	odin_udc_fill_trb(trb, udc->ctrl_req_dma, udc->ep0->maxpacket,
			   ODIN_USB3_DSCCTL_TRBCTL_SETUP, ODIN_USB3_DSCCTL_LST, 1);
	ud_udc("%s() desc=0x%08lx xfercnt=%u bptr=0x%08x:%08x\n",
		   __func__, (unsigned long)trb, ODIN_USB3_DSCSTS_LENGTH(trb->status),
		   trb->bpth, trb->bptl);
#ifdef ODIN_USB_VERBOSE
	usb_print("0x%08x 0x%08x 0x%08x 0x%08x\n",
		   *((unsigned *)trb), *((unsigned *)trb + 1),
		   *((unsigned *)trb + 2), *((unsigned *)trb + 3));*/
#endif

	tri = odin_udc_epcmd_start_xfer(udc, udc->ep0, trb_dma, 0);
	udc->ep0->tri_out = tri + 1;
}

void odin_udc_ep0_start_transfer(struct odin_udc *udc,
				     struct odin_usb3_req *req)
{
	struct odin_udc_trb *trb;
	dma_addr_t trb_dma;
	u32 desc_type, len;
	u8 tri;

	ud_udc("%s(): ep%d-%s req=%lx xfer_len=%d xfer_cnt=%d xfer_buf=%lx\n",
		__func__, udc->ep0->num, (udc->ep0->is_in ? "IN" : "OUT"),
		(unsigned long)req, req->length, req->actual,
		(unsigned long)req->buf);

	if (udc->ep0->is_in) {
		req->trb = udc->ep0_in_trb;
		req->trbdma = udc->ep0_in_trb_dma;
	} else {
		req->trb = udc->ep0_out_trb;
		req->trbdma = udc->ep0_out_trb_dma;
	}

	trb = req->trb;
	trb_dma = req->trbdma;

	if (udc->ep0->is_in) {
		len = req->length;

		if (udc->ep0state == EP0_IN_STATUS_PHASE) {
			if (udc->ep0->three_stage)
				desc_type = ODIN_USB3_DSCCTL_TRBCTL_STATUS_3;
			else
				desc_type = ODIN_USB3_DSCCTL_TRBCTL_STATUS_2;
		} else {
			desc_type = ODIN_USB3_DSCCTL_TRBCTL_CTLDATA_1ST;
		}

		odin_udc_fill_trb(trb, req->bufdma,
				   len, desc_type, ODIN_USB3_DSCCTL_LST, 1);
#ifdef ODIN_USB_VERBOSE
		usb_print("0x%08x 0x%08x 0x%08x 0x%08x\n",
			   *((unsigned *)trb), *((unsigned *)trb + 1),
			   *((unsigned *)trb + 2), *((unsigned *)trb + 3));
#endif
		tri = odin_udc_epcmd_start_xfer(udc, udc->ep0, trb_dma, 0);
		udc->ep0->tri_in = tri + 1;
	} else {	/* OUT */
		len = (req->length + udc->ep0->maxpacket - 1) &
			~(udc->ep0->maxpacket - 1);

		if (udc->ep0state == EP0_OUT_STATUS_PHASE) {
			if (udc->ep0->three_stage)
				desc_type = ODIN_USB3_DSCCTL_TRBCTL_STATUS_3;
			else
				desc_type = ODIN_USB3_DSCCTL_TRBCTL_STATUS_2;
		} else {
			desc_type = ODIN_USB3_DSCCTL_TRBCTL_CTLDATA_1ST;
		}

		odin_udc_fill_trb(trb, req->bufdma,
				   len, desc_type, ODIN_USB3_DSCCTL_LST, 1);
#ifdef ODIN_USB_VERBOSE
		usb_print("0x%08x 0x%08x 0x%08x 0x%08x\n",
			   *((unsigned *)trb), *((unsigned *)trb + 1),
			   *((unsigned *)trb + 2), *((unsigned *)trb + 3));
#endif
		tri = odin_udc_epcmd_start_xfer(udc, udc->ep0, trb_dma, 0);
		udc->ep0->tri_out = tri + 1;
	}
}

void odin_udc_ep_start_transfer(struct odin_udc *udc,
		struct odin_udc_ep *odin_ep, struct odin_usb3_req *req, u32 event)
{
	struct odin_udc_trb *trb;
	dma_addr_t trb_dma;
	u32 dsts;
	u16 current_uf, intvl, mask, now, target_uf = 0;
	u8 tri;

	ud_udc("%s(): ep%d-%s (%d phys) %lx max_pkt=%d req=%lx"
		    " xfer_len=%d xfer_cnt=%d xfer_buf=%lx\n",
		    __func__, odin_ep->num, (odin_ep->is_in ? "IN" : "OUT"),
		    odin_ep->phys, (unsigned long)odin_ep, odin_ep->maxpacket,
		    (unsigned long)req, req->length,
		    req->actual, (unsigned long)req->buf);

	if (event) {
		current_uf = ODIN_USB3_DEPEVT_ISOC_UFRAME_NUM(event);

		intvl = 1 << odin_ep->intvl;
		mask = ~(intvl - 1);

		dsts = readl(udc->regs + ODIN_USB3_DSTS);
		now = ODIN_USB3_DSTS_SOF_FN(dsts);
		if (now < (current_uf & 0x3fff))
			now += 0x4000;
		now += current_uf & 0xc000;
		target_uf = current_uf & mask;
		ud_core("tgt:%1x now:%1x tgt-now:%1x now-tgt:%1x\n",
			     target_uf, now, target_uf - now, now - target_uf);
		target_uf += (now - target_uf + 0x100);
	}

	odin_ep->send_zlp = 0;
	req->flags |= ODIN_USB3_REQ_STARTED;
	trb = req->trb;
	trb_dma = req->trbdma;

#ifdef ODIN_USB_VERBOSE
	usb_print("%08x %08x %08x %08x (%08x)\n",
		   *((unsigned *)trb), *((unsigned *)trb + 1),
		   *((unsigned *)trb + 2), *((unsigned *)trb + 3),
		   (unsigned)trb_dma);
#endif

	if (odin_ep->is_in) {
		if (odin_ep->xfer_started) {
			ud_iso("[%s] update %p / %08x %08x %08x (%08x) \n",
					__func__, &req->usb_req, *((unsigned *)trb),
					*((unsigned *)trb + 2), *((unsigned *)trb + 3), (unsigned)trb_dma);
			odin_udc_epcmd_update_xfer(udc, odin_ep->phys,
						odin_ep->tri_in - 1);
		} else {
			if (odin_ep->type == USB_ENDPOINT_XFER_ISOC) {
			ud_iso("[%s] start %p / %08x %08x %08x (%08x) \n",
					__func__, &req->usb_req, *((unsigned *)trb),
					*((unsigned *)trb + 2), *((unsigned *)trb + 3), (unsigned)trb_dma);
				odin_udc_epcmd_set_xfercfg(udc, odin_ep->phys, 1);
				tri = odin_udc_epcmd_start_xfer(udc, odin_ep,
						trb_dma, target_uf);
			} else {
				tri = odin_udc_epcmd_start_xfer(udc, odin_ep,
						trb_dma, req->usb_req.stream_id);
			}
			odin_ep->tri_in = tri + 1;
			odin_ep->xfer_started = 1;
		}
	} else { /*OUT */
		if (odin_ep->xfer_started) {
			odin_udc_epcmd_update_xfer(udc, odin_ep->phys,
						odin_ep->tri_out - 1);
		} else {
			if (odin_ep->type == USB_ENDPOINT_XFER_ISOC) {
				/* TODO : FIX IT  */
				odin_udc_epcmd_set_xfercfg(udc, odin_ep->phys, 1);
				tri = odin_udc_epcmd_start_xfer(udc, odin_ep,
						trb_dma, target_uf);
			} else {
				tri = odin_udc_epcmd_start_xfer(udc, odin_ep,
						trb_dma, req->usb_req.stream_id);
			}
			odin_ep->tri_out = tri + 1;
			odin_ep->xfer_started = 1;
		}
	}
}

void odin_udc_stop_xfer(struct odin_udc *udc,
						struct odin_udc_ep *odin_ep)
{
	if (odin_ep->is_in) {
		if (odin_ep->active && odin_ep->tri_in) {
			odin_udc_epcmd_end_xfer(udc, odin_ep,
					odin_ep->tri_in - 1, ODIN_USB3_ENDXFER_FORCE);
			odin_ep->tri_in = 0;
		}
	} else {
		if (odin_ep->active && odin_ep->tri_out) {
			odin_udc_epcmd_end_xfer(udc, odin_ep,
					odin_ep->tri_out - 1, ODIN_USB3_ENDXFER_FORCE);
			odin_ep->tri_out = 0;
		}
	}
}

void odin_udc_isoc_xfer_start(struct odin_udc *udc,
			struct odin_udc_ep *odin_ep, u32 event)
{
	struct odin_usb3_req *req = NULL;
	struct list_head *list_item;

	odin_ep->last_event = event;

	if (list_empty(&odin_ep->queue)) {
		odin_ep->pending_req = 1;
		ud_core("%s(%p) queue empty!\n",
			  __func__, odin_ep);
		return;
	}

	if (odin_ep->desc_avail <= 0) {
		usb_err("EP%d-%s: no TRB avail!\n",
			  odin_ep->num, odin_ep->is_in ? "IN" : "OUT");
		return;
	}

	list_for_each (list_item, &odin_ep->queue) {
		req = list_entry(list_item, struct odin_usb3_req, entry);
		if (req->flags & ODIN_USB3_REQ_STARTED)
			req = NULL;
		else
			break;
	}

	if (!req) {
		usb_err("EP%d-%s: no requests to start!\n",
			  odin_ep->num, odin_ep->is_in ? "IN" : "OUT");
		return;
	}

	odin_udc_fill_trbs(udc, odin_ep, req);
	odin_udc_ep_start_transfer(udc, odin_ep, req, event);

	/*
	 * Now start any remaining queued transfers
	 */
	while (!list_is_last(list_item, &odin_ep->queue)) {
		list_item = list_item->next;
		req = list_entry(list_item, struct odin_usb3_req, entry);
		if (!(req->flags & ODIN_USB3_REQ_STARTED)) {
			if (odin_ep->desc_avail <= 0) {
				usb_err("start_next EP%d-%s: no TRB avail!\n",
					  odin_ep->num, odin_ep->is_in ?
					  "IN" : "OUT");
				return;
			}

			odin_udc_fill_trbs(udc, odin_ep, req);
			odin_udc_ep_start_transfer(udc, odin_ep, req, 0);
		}
	}
}

void odin_udc_iso_xfer_stop(struct odin_udc *udc,
						struct odin_udc_ep *odin_ep)
{
	if (odin_ep->is_in)
		odin_ep->tri_in = 0;
	else
		odin_ep->tri_out = 0;

	odin_udc_epcmd_end_xfer(udc, odin_ep, 0,
			ODIN_USB3_ENDXFER_FORCE |ODIN_USB3_ENDXFER_ISO);
	odin_ep->xfer_started = 0;
}

void odin_udc_enable_device_interrupts(struct odin_udc *udc)
{
	u32 devten;

	odin_drd_dis_eventbuf(udc->dev->parent);
	odin_drd_flush_eventbuf(udc->dev->parent);
	odin_drd_ena_eventbuf(udc->dev->parent);

	devten = ODIN_USB3_DEVTEN_DISCONN |
				ODIN_USB3_DEVTEN_USBRESET |
				ODIN_USB3_DEVTEN_CONNDONE |
				ODIN_USB3_DEVTEN_U3_L2L1_SUSP |
				ODIN_USB3_DEVTEN_ERRATICERR;

	writel(devten, udc->regs + ODIN_USB3_DEVTEN);
}

void odin_udc_send_zlp(struct odin_udc *udc, struct odin_usb3_req *req)
{
	struct odin_udc_trb *trb;
	dma_addr_t trb_dma;
	u8 tri;

	/* This routine is called to send a 0-length packet after the end of a transfer */
	if (udc->ep0->is_in) {
		trb = udc->ep0_in_trb;
		trb_dma = udc->ep0_in_trb_dma;

		odin_udc_fill_trb(trb, req->bufdma, 0,
			ODIN_USB3_DSCCTL_TRBCTL_NORMAL, ODIN_USB3_DSCCTL_LST, 1);

		tri = odin_udc_epcmd_start_xfer(udc, udc->ep0, trb_dma, 0);
		udc->ep0->tri_in = tri + 1;
	}
}

int odin_udc_memory_alloc(struct odin_udc* udc)
{
	int ret = -ENOMEM;

	/* Allocate the EP0 packet buffers */
	udc->ctrl_req = dma_alloc_coherent(NULL,
			sizeof(struct usb_ctrlrequest) * 5,
			&udc->ctrl_req_dma, GFP_KERNEL | GFP_DMA32);
	if (!udc->ctrl_req)
		goto out1;

	udc->ep0_status_buf = dma_alloc_coherent(NULL,
			ODIN_USB3_STATUS_BUF_SIZE,
			 &udc->ep0_status_buf_dma, GFP_KERNEL | GFP_DMA32);
	if (!udc->ep0_status_buf)
		goto out2;

	/* Allocate the EP0 DMA descriptors */
	udc->ep0_setup_trb = dma_alloc_coherent(NULL,
			 sizeof(struct odin_udc_trb), &udc->ep0_setup_trb_dma,
			 GFP_KERNEL | GFP_DMA32);
	if (!udc->ep0_setup_trb)
		goto out3;

	udc->ep0_in_trb = dma_alloc_coherent(NULL,
			sizeof(struct odin_udc_trb), &udc->ep0_in_trb_dma,
			GFP_KERNEL | GFP_DMA32);
	if (!udc->ep0_in_trb)
		goto out4;

	udc->ep0_out_trb = dma_alloc_coherent(NULL,
			sizeof(struct odin_udc_trb), &udc->ep0_out_trb_dma,
			GFP_KERNEL | GFP_DMA32);
	if (!udc->ep0_out_trb)
		goto out5;

	return 0;

out5:
	dma_free_coherent(NULL, sizeof(struct odin_udc_trb), udc->ep0_in_trb,
			  udc->ep0_in_trb_dma);
out4:
	dma_free_coherent(NULL, sizeof(struct odin_udc_trb),
			  udc->ep0_setup_trb, udc->ep0_setup_trb_dma);
out3:
	dma_free_coherent(NULL, ODIN_USB3_STATUS_BUF_SIZE,
			udc->ep0_status_buf,
			udc->ep0_status_buf_dma);
out2:
	dma_free_coherent(NULL, sizeof(struct usb_ctrlrequest) * 5,
			  udc->ctrl_req, udc->ctrl_req_dma);
out1:
	return ret;
}

void odin_udc_memory_free(struct odin_udc* udc)
{
	dma_free_coherent(NULL, sizeof(struct odin_udc_trb), udc->ep0_out_trb,
			  udc->ep0_out_trb_dma);
	dma_free_coherent(NULL, sizeof(struct odin_udc_trb), udc->ep0_in_trb,
			  udc->ep0_in_trb_dma);
	dma_free_coherent(NULL, sizeof(struct odin_udc_trb), udc->ep0_setup_trb,
			  udc->ep0_setup_trb_dma);
	dma_free_coherent(NULL, ODIN_USB3_STATUS_BUF_SIZE, udc->ep0_status_buf,
			  udc->ep0_status_buf_dma);
	dma_free_coherent(NULL, sizeof(struct usb_ctrlrequest) * 5,
			  udc->ctrl_req, udc->ctrl_req_dma);
}

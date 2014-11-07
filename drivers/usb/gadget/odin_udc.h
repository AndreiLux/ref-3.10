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
 #ifndef __DRIVERS_USB_DWC3_ODIN_UDC__H
#define __DRIVERS_USB_DWC3_ODIN_UDC__H

#include <linux/wakelock.h>

#define ODIN_UDC_HANDSHAKE_TIMEOUT	2000 /* 2ms */

#ifdef ODIN_USB_GADGET_DEBUG
#define ug_dbg(x...)	printk(ODIN_USB x )
#else
#define ug_dbg(x...)	do {} while (0)
#endif

#ifdef ODIN_USB_ISO_TRANSFER_DEBUG
void odin_udc_dbg_iso(char *str);
#define iso_dbg(x)	odin_udc_dbg_iso(x)
#else
#define iso_dbg(x)	do {} while (0)
#endif

#define ODIN_USB3_DEFAULT_EP_NUM	22

#define ODIN_USB3_NUM_ISOC_TRBS	64
#define ODIN_USB3_STATUS_BUF_SIZE 512
#define ODIN_USB3_MAX_PACKET_SIZE	1024
#define ODIN_USB3_MAX_EP0_SIZE	512

/* Will be moved to DRD */
#define ODIN_USB3_SNPSID	0x20

#define ODIN_USB3_HWPARAMS0	0x40
#define ODIN_USB3_HWP0_MDWIDTH_MASK	(0xff << 8)
#define ODIN_USB3_HWP0_MDWIDTH(n)	\
			(((n) & ODIN_USB3_HWP0_MDWIDTH_MASK) >> 8)

#define ODIN_USB3_HWPARAMS3		0x4C
#define ODIN_USB3_HWP3_NUM_EPS_MASK (0x3f << 12)
#define ODIN_USB3_HWP3_NUM_EPS(n) \
			(((n) & ODIN_USB3_HWP3_NUM_EPS_MASK) >> 12)
#define ODIN_USB3_HWP3_NUM_IN_EPS_MASK (0x1f << 18)
#define ODIN_USB3_HWP3_NUM_IN_EPS(n)	\
			(((n) & ODIN_USB3_HWP3_NUM_IN_EPS_MASK) >> 18)

#define ODIN_USB3_GTXFIFOSIZE(a)	(0x200 + ((a) * 0x04))
#define ODIN_USB3_FIFOSZ_DEPTH_SHIFT	0
#define ODIN_USB3_FIFOSZ_STARTADDR_SHIFT	16
#define ODIN_USB3_FIFOSZ_STARTADDR_MASK	(0xffff << 16)

/** Device Configuration Register (DCFG) **/
#define ODIN_USB3_DCFG	0x0
#define ODIN_USB3_DCFG_DEVSPD_MASK	(7 << 0)
#define ODIN_USB3_DCFG_DEVSPD(n)	(n << 0)
#define ODIN_USB3_DCFG_DEVADDR_MASK	(0x7f << 3)
#define ODIN_USB3_DCFG_DEVADDR(n)	(n << 3)
#define ODIN_USB3_DCFG_NUM_RCV_BUF_MASK	(0x1f << 17)
#define ODIN_USB3_DCFG_NUM_RCV_BUF_SHIFT	17
#define ODIN_USB3_DCFG_LPM_CAP	(1 << 22)

enum odin_usb3_speed {
	ODIN_USB3_SPEED_HS_30OR60 = 0,
	ODIN_USB3_SPEED_FS_30OR60,
	ODIN_USB3_SPEED_LS_6,
	ODIN_USB3_SPEED_FS_48,
	ODIN_USB3_SPEED_SS_125OR250,
};

/** Device Control Register (DCTL) **/
#define ODIN_USB3_DCTL	0x04
#define ODIN_USB3_DCTL_TSTCTL_MASK		(0xf << 1)
#define ODIN_USB3_DCTL_TSTCTL(n)			((n) << 1)
#define ODIN_USB3_DCTL_ULST_CHNG_REQ_MASK	(0x0f << 5)
#define ODIN_USB3_DCTL_ULST_CHNG_REQ(n)	\
			(((n) << 5) & ODIN_USB3_DCTL_ULST_CHNG_REQ_MASK)
#define ODIN_USB3_DCTL_ACCEPT_U1_EN	(1 << 9)
#define ODIN_USB3_DCTL_INIT_U1_EN	(1 << 10)
#define ODIN_USB3_DCTL_ACCEPT_U2_EN	(1 << 11)
#define ODIN_USB3_DCTL_INIT_U2_EN	(1 << 12)
#define ODIN_USB3_DCTL_L1_HBIER_EN	(1 << 18)
#define ODIN_USB3_DCTL_KEEP_CONNECT	(1 << 19)
#define ODIN_USB3_DCTL_HIRD_THRES(n)	(((n) & 0x1f) << 24)
#define ODIN_USB3_DCTL_LSFT_RST	(1 << 29)
#define ODIN_USB3_DCTL_CSFT_RST	(1 << 30)
#define ODIN_USB3_DCTL_RUN_STOP	(1 << 31)

/** Device Event Enable Register (DEVTEN) **/
#define ODIN_USB3_DEVTEN	0x08
#define ODIN_USB3_DEVTEN_DISCONN	0x0001
#define ODIN_USB3_DEVTEN_USBRESET	0x0002
#define ODIN_USB3_DEVTEN_CONNDONE	0x0004
#define ODIN_USB3_DEVTEN_ULST_CHNG	0x0008
#define ODIN_USB3_DEVTEN_WKUP	0x0010
#define ODIN_USB3_DEVTEN_HIBER_REQ	0x0020
#define ODIN_USB3_DEVTEN_U3_L2L1_SUSP	0x0040
#define ODIN_USB3_DEVTEN_SOF	0x0080
#define ODIN_USB3_DEVTEN_ERRATICERR	0x0200
#define ODIN_USB3_DEVTEN_INACT_TIMEOUT	0x2000

/** Device Status Register (DSTS) **/
#define ODIN_USB3_DSTS	0x0C
#define ODIN_USB3_DSTS_CONNSPD	(7 << 0)
#define ODIN_USB3_DSTS_SOF_FN_MASK	(0x3fff << 3)
#define ODIN_USB3_DSTS_SOF_FN(n)	\
			(((n) & ODIN_USB3_DSTS_SOF_FN_MASK) >> 3)
#define ODIN_USB3_DSTS_LINK_STATE_MASK	(0x0f << 18)
#define ODIN_USB3_DSTS_LINK_STATE(n)	\
			(((n) & ODIN_USB3_DSTS_LINK_STATE_MASK) >> 18)
#define ODIN_USB3_DSTS_DEV_CTRL_HLT	(1 << 22)
#define ODIN_USB3_DSTS_CORE_IDLE_BIT	(1 << 23)
#define ODIN_USB3_DSTS_SSS_BIT	(1 << 24)
#define ODIN_USB3_DSTS_RSS_BIT	(1 << 25)
#define ODIN_USB3_DSTS_SRE_BIT	(1 << 28)
#define ODIN_USB3_DSTS_DCNRD	(1 << 29)

/** Device Generic Command Parameter Register (DGCMDPAR) **/
#define ODIN_USB3_DGCMDPAR	0x10

/** Device Generic Command Register (DGCMDn) **/
#define ODIN_USB3_DGCMD	0x14
#define ODIN_USB3_DGCMD_SELECTED_FIFO_FLUSH	0x09
#define ODIN_USB3_DGCMD_ALL_FIFO_FLUSH	0x0a
#define ODIN_USB3_DGCMD_SET_ENDPOINT_NRDY	0x0c
#define ODIN_USB3_DGCMD_RUN_SOC_BUS_LOOPBACK	0x10

#define ODIN_USB3_DGCMD_STATUS(n)		(((n) >> 15) & 1)
#define ODIN_USB3_DGCMD_CMDACT		(1 << 10)
#define ODIN_USB3_DGCMD_CMDIOC		(1 << 8)

/** Device Generic Command Parameter Register (DGCMDPARn) **/
#define ODIN_USB3_DGCMDPAR_FORCE_LINKPM_ACCEPT	(1 << 0)
#define ODIN_USB3_DGCMDPAR_FIFO_NUM(n)		((n) << 0)
#define ODIN_USB3_DGCMDPAR_RX_FIFO			(0 << 5)
#define ODIN_USB3_DGCMDPAR_TX_FIFO			(1 << 5)
#define ODIN_USB3_DGCMDPAR_LOOPBACK_DIS		(0 << 0)
#define ODIN_USB3_DGCMDPAR_LOOPBACK_ENA		(1 << 0)

#define ODIN_USB3_DALEPENA	0x20
#define ODIN_USB3_DEPCMDPAR2(n)	(0x100 + (n * 0x10))
#define ODIN_USB3_DEPCMDPAR1(n)	(0x104 + (n * 0x10))
#define ODIN_USB3_DEPCMDPAR0(n)	(0x108 + (n * 0x10))
#define ODIN_USB3_DEPCMD(n)		(0x10c + (n * 0x10))

/** Content of Event Buffers **/
#define ODIN_USB3_EVENT_NON_EP_MASK	0x01
#define ODIN_USB3_EVENT_MASK	0xfe
#define ODIN_USB3_EVENT_SHIFT	1
/* Non-Endpoint Specific Event Type values */
#define ODIN_USB3_EVENT_DEV_INT		0
#define ODIN_USB3_EVENT_OTG_INT		1
#define ODIN_USB3_EVENT_CARKIT_INT	3
#define ODIN_USB3_EVENT_I2C_INT		4

/** Event Buffer Content for Device Endpoint-Specific Events (DEPEVT) **/
#define ODIN_USB3_DEPEVT_EPNUM_MASK	(0x1f << 1)
#define ODIN_USB3_DEPEVT_EPNUM(n)	\
				(((n) & ODIN_USB3_DEPEVT_EPNUM_MASK) >> 1)
#define ODIN_USB3_DEPEVT_EVENT_MASK	(0x0f << 6)
#define ODIN_USB3_DEPEVT_EVENT_SHIFT	6
/* Endpoint Event Type values */
#define ODIN_USB3_DEPEVT_XFER_CMPL		(1 << 6)
#define ODIN_USB3_DEPEVT_XFER_IN_PROG	(2 << 6)
#define ODIN_USB3_DEPEVT_XFER_NRDY		(3 << 6)
#define ODIN_USB3_DEPEVT_FIFOXRUN		(4 << 6)
#define ODIN_USB3_DEPEVT_STRM_EVT		(6 << 6)
#define ODIN_USB3_DEPEVT_EPCMD_CMPL		(7 << 6)

#define ODIN_USB3_DEPEVT_CTRL_MASK	(0x03 << 12)
#define ODIN_USB3_DEPEVT_CTRL_SHIFT	12
/* Xfer Not Ready Event Status values */
#define ODIN_USB3_DEPEVT_CTRL_SETUP		(0 << 12)
#define ODIN_USB3_DEPEVT_CTRL_DATA		(1 << 12)
#define ODIN_USB3_DEPEVT_CTRL_STATUS		(2 << 12)

#define ODIN_USB3_DEPEVT_ISOC_UFRAME_NUM_MASK (0xffff << 16)
#define ODIN_USB3_DEPEVT_ISOC_UFRAME_NUM(n)	\
			(((n) & ODIN_USB3_DEPEVT_ISOC_UFRAME_NUM_MASK) >> 16)
#define ODIN_USB3_DEPEVT_CMDTYPE_DEPSTARTXFER	(6 << 24)
#define ODIN_USB3_DEPEVT_CMDTYPE_DEPENDXFER	(8 << 24)
#define ODIN_USB3_DEPEVT_EVT_STATUS_MASK	(0xf << 24)
#define ODIN_USB3_DEPEVT_EVT_STATUS_UF_PASSED	(1 << 13)

/** Event Buffer Content for Device-Specific Events (DEVT) **/
#define ODIN_USB3_DEVT_EVENT_MASK	(0x0f << 8)
#define ODIN_USB3_DEVT_EVENT_SHIFT	8
/* Device Specific Event Type values */
#define ODIN_USB3_DEVT_DISCONN				(0 << 8)
#define ODIN_USB3_DEVT_USBRESET				(1 << 8)
#define ODIN_USB3_DEVT_CONNDONE				(2 << 8)
#define ODIN_USB3_DEVT_ULST_CHNG				(3 << 8)
#define ODIN_USB3_DEVT_WKUP					(4 << 8)
#define ODIN_USB3_DEVT_HIBER_REQ				(5 << 8)
#define ODIN_USB3_DEVT_U3_L2L1_SUSP			(6 << 8)
#define ODIN_USB3_DEVT_SOF					(7 << 8)
#define ODIN_USB3_DEVT_ERRATICERR				(9 << 8)
#define ODIN_USB3_DEVT_CMD_CMPL				(10 << 8)
#define ODIN_USB3_DEVT_OVERFLOW				(11 << 8)
#define ODIN_USB3_DEVT_VNDR_DEV_TST_RCVD	(12 << 8)
#define ODIN_USB3_DEVT_INACT_TIMEOUT_RCVD	(13 << 8)

enum odin_usb3_link_state {
	ODIN_USB3_DSTS_LINK_STATE_U0 = 0x00, /*HS - Normal */
	ODIN_USB3_DSTS_LINK_STATE_U1 = 0x01,
	ODIN_USB3_DSTS_LINK_STATE_U2 = 0x02, /* HS - Sleep */
	ODIN_USB3_DSTS_LINK_STATE_U3 = 0x03,	/*HS - Suspend */
	ODIN_USB3_DSTS_LINK_STATE_SS_DIS = 0x04,
	ODIN_USB3_DSTS_LINK_STATE_RX_DET = 0x05,
	ODIN_USB3_DSTS_LINK_STATE_SS_INACT = 0x06,
	ODIN_USB3_DSTS_LINK_STATE_POLL = 0x07,
	ODIN_USB3_DSTS_LINK_STATE_RECOV = 0x08,
	ODIN_USB3_DSTS_LINK_STATE_HRESET = 0x09,
	ODIN_USB3_DSTS_LINK_STATE_CMPLY = 0x0a,
	ODIN_USB3_DSTS_LINK_STATE_LPBK = 0x0b,
	ODIN_USB3_DSTS_LINK_STATE_RESET	= 0x0e,
	ODIN_USB3_DSTS_LINK_STATE_RESUME = 0x0f,
};

/** Device Endpoint Command Parameter 1 Register (DEPCMDPAR1n) **/
#define ODIN_USB3_EPCFG1_INTRNUM(n)	((n) << 0)
#define ODIN_USB3_EPCFG1_XFER_CMPL_EN	(1 << 8)
#define ODIN_USB3_EPCFG1_XFER_IN_PROG_EN	(1 << 9)
#define ODIN_USB3_EPCFG1_XFER_NRDY_EN	(1 << 10)
#define ODIN_USB3_EPCFG1_FIFOXRUN_EN	(1 << 11)
#define ODIN_USB3_EPCFG1_STREAM_EVENT_EN	(1 << 13)
#define ODIN_USB3_EPCFG1_BINTERVAL_M1(n)	((n)	<< 16)
#define ODIN_USB3_EPCFG1_STRM_CAP_EN		(1 << 24)
#define ODIN_USB3_EPCFG1_EP_DIR	(1 << 25)
#define ODIN_USB3_EPCFG1_EP_NUM(n)	((n) << 26)
#define ODIN_USB3_EPCFG1_BULK_BASED	(1 << 30)
#define ODIN_USB3_EPCFG1_FIFO_BASED	(1 << 31)

/** Device Endpoint Command Parameter 0 Register (DEPCMDPAR0n) **/
#define ODIN_USB3_EPCFG0_EP_TYPE(n)	((n) << 1)
#define ODIN_USB3_EPCFG0_EP_TYPE_CONTROL	(0 << 1)
#define ODIN_USB3_EPCFG0_EP_TYPE_ISOC		(1 << 1)
#define ODIN_USB3_EPCFG0_EP_TYPE_BULK		(2 << 1)
#define ODIN_USB3_EPCFG0_EP_TYPE_INTR		(3 << 1)
#define ODIN_USB3_EPCFG0_MPS(n)	((n) << 3)
#define ODIN_USB3_EPCFG0_TXFNUM(n)	((n) << 17)
#define ODIN_USB3_EPCFG0_BRSTSIZ(n)	((n) << 22)
#define ODIN_USB3_EPCFG0_DSNUM(n)	((n)	<< 26)
#define ODIN_USB3_EPCFG0_ACTION_INIT			(0 << 30)
#define ODIN_USB3_EPCFG0_ACTION_RESTORE		(1 << 30)
#define ODIN_USB3_EPCFG0_ACTION_MODIFY		(2 << 30)

/** Device Endpoint Command Register (DEPCMDn) **/
#define ODIN_USB3_EPCMD_SET_EP_CFG		(1 << 0)
#define ODIN_USB3_EPCMD_SET_XFER_CFG		(2 << 0)
#define ODIN_USB3_EPCMD_GET_EP_STATE		(3 << 0)
#define ODIN_USB3_EPCMD_SET_STALL		(4 << 0)
#define ODIN_USB3_EPCMD_CLR_STALL			(5 << 0)
#define ODIN_USB3_EPCMD_START_XFER		(6 << 0)
#define ODIN_USB3_EPCMD_UPDATE_XFER		(7 << 0)
#define ODIN_USB3_EPCMD_END_XFER			(8 << 0)
#define ODIN_USB3_ENDXFER_FORCE		1
#define ODIN_USB3_ENDXFER_NODELAY	2
#define ODIN_USB3_ENDXFER_ISO			4
#define ODIN_USB3_EPCMD_START_NEW_CFG	(9 << 0)

#define ODIN_USB3_EPCMD_IOC	(1 << 8)
#define ODIN_USB3_EPCMD_ACT	(1 << 10)
#define ODIN_USB3_EPCMD_HP_FRM	(1 << 11)
#define ODIN_USB3_EPCMD_STR_NUM_OR_UF(n)	(n << 16)
#define ODIN_USB3_EPCMD_XFER_RSRC_IDX(n)	(n << 16)
#define ODIN_USB3_EPCMD_XFER_RSRC_MASK	(0x7f << 16)
#define ODIN_USB3_EPCMD_XFER_RSRC(n)	\
			(((n) & ODIN_USB3_EPCMD_XFER_RSRC_MASK) >> 16)

/** TRB Descriptor **/
#define ODIN_USB3_DSCCTL_HWO	(1 << 0)
#define ODIN_USB3_DSCCTL_LST	(1 << 1)
#define ODIN_USB3_DSCCTL_CHN	(1 << 2)
#define ODIN_USB3_DSCCTL_CSP	(1 << 3)
#define ODIN_USB3_DSCCTL_TRBCTL(n)		(((n) & 0x3f) << 4)
#define ODIN_USB3_DSCCTL_ISP	(1 << 10)
#define ODIN_USB3_DSCCTL_IMI	ODIN_USB3_DSCCTL_ISP
#define ODIN_USB3_DSCCTL_IOC	(1 << 11)
#define ODIN_USB3_DSCCTL_STRMID_SOFN(n) (((n) & 0xffff) << 14)

enum odin_usb3_trb_contol_type {
	ODIN_USB3_DSCCTL_TRBCTL_NORMAL = 1,
	ODIN_USB3_DSCCTL_TRBCTL_SETUP = 2,
	ODIN_USB3_DSCCTL_TRBCTL_STATUS_2 = 3,
	ODIN_USB3_DSCCTL_TRBCTL_STATUS_3 = 4,
	ODIN_USB3_DSCCTL_TRBCTL_CTLDATA_1ST= 5,
	ODIN_USB3_DSCCTL_TRBCTL_ISOC_1ST = 6,
	ODIN_USB3_DSCCTL_TRBCTL_ISOC = 7,
	ODIN_USB3_DSCCTL_TRBCTL_LINK = 8,
};

#define ODIN_USB3_DSCSTS_MASK	(0x00ffffff)
#define ODIN_USB3_DSCSTS_LENGTH(n)	((n) & ODIN_USB3_DSCSTS_MASK)
#define ODIN_USB3__DSCSTS_PCM1(n)	(n << 24)
#define ODIN_USB3_DSCSTS_TRBRSP(n)	(((n) & (0x0f << 28)) >> 28)

enum odin_usb3_dscsts_trbrsp {
	ODIN_USB3_TRBRSP_MISSED_ISOC_IN	= 1,
	ODIN_USB3_TRBRSP_SETUP_PEND		= 2,
	ODIN_USB3_TRBRSP_XFER_IN_PROG	= 4,
};

enum odin_usb3_ep0_state {
	EP0_IDLE,
	EP0_IN_DATA_PHASE,
	EP0_OUT_DATA_PHASE,
	EP0_IN_WAIT_GADGET,
	EP0_OUT_WAIT_GADGET,
	EP0_IN_WAIT_NRDY,
	EP0_OUT_WAIT_NRDY,
	EP0_IN_STATUS_PHASE,
	EP0_OUT_STATUS_PHASE,
	EP0_STALL,
};

struct odin_usb3_ep_cmd_params {
	u32	param2;
	u32	param1;
	u32	param0;
};

#define odin_usb3_get_ep(usb_ep) \
	container_of((usb_ep), struct odin_udc_ep, ep)
#define odin_usb3_get_req(usbreq) \
	container_of((usbreq), struct odin_usb3_req, usb_req)

#define ODIN_USB3_DMA_ADDR_INVALID	~(dma_addr_t)0

#define ODIN_USB3_REQ_ZERO		0x001
#define ODIN_USB3_REQ_STARTED		0x002
#define ODIN_USB3_REQ_IN			0x010
#define ODIN_USB3_REQ_MAP_DMA	0x100


struct odin_usb3_req {
	struct list_head entry;
	struct usb_request usb_req;
	struct odin_udc_trb *trb;
	dma_addr_t trbdma;

	u32 length;
	u32 actual;

	int flags;

	char *buf;
	dma_addr_t bufdma;
	u32 buflen;
};

struct odin_udc_ep {
	struct usb_ep	ep;
	struct odin_udc *udc;

	u8 phys;
	u8 num;
	u8 type;
	u8 intvl;
	u16 maxpacket;
	u8 tx_fifo_num;

	u8 tri_out;
	u8 tri_in;

	u32 uframe;
	u32 last_event;

	unsigned int stopped		: 1;
	unsigned int send_zlp		: 1;
	unsigned int three_stage	: 1;
	unsigned int xfer_started	: 1;
	unsigned int is_in			: 1;
	unsigned int active			: 1;
	unsigned int stream_capable	: 1;
	unsigned int pending_req	: 1;
	unsigned int wedge			: 1;

	const struct usb_endpoint_descriptor *usb_ep_desc;

	struct odin_udc_trb *dma_desc;
	dma_addr_t dma_desc_dma;
	int desc_size;
	int num_desc;
	int desc_avail;
	int desc_idx;

	struct list_head queue;
};

enum odin_usb3_speed_mode {
	USB3_SPEED_MODE_SS = 1,
	USB3_SPEED_MODE_HS,
};

struct odin_udc {
	struct odin_usb3_otg 	*odin_otg;

	struct device	*dev;
	struct usb_gadget		gadget;
	struct usb_gadget_driver	*driver;

	struct odin_usb3_req	ep0_req;

	char		ep_names[31][16];
	struct odin_udc_ep eps[31];
	struct odin_udc_ep *ep0;

	u32 *event_ptr;
	u32 *event_buf;
	dma_addr_t event_buf_dma;

	struct odin_udc_trb *ep0_setup_trb;
	dma_addr_t ep0_setup_trb_dma;
	struct odin_udc_trb *ep0_out_trb;
	dma_addr_t ep0_out_trb_dma;
	struct odin_udc_trb *ep0_in_trb;
	dma_addr_t ep0_in_trb_dma;

	u8 *ep0_status_buf;
	dma_addr_t ep0_status_buf_dma;

	struct usb_ctrlrequest *ctrl_req;
	dma_addr_t ctrl_req_dma;

	void __iomem		*regs;
	void __iomem		*gregs;

	u8 num_eps;
	u8 num_out_eps;
	u8 num_in_eps;
	u32 snpsid;

	struct wake_lock	wake_lock;
	int	irq;

	enum usb_device_state	state;
	enum odin_usb3_link_state link_state;

	enum odin_usb3_ep0_state ep0state;

	unsigned int ep0_status_pending		: 1;
	unsigned int request_config			: 1;
	unsigned int remote_wakeup_enable	: 1;
	 /* Latency Tolerance Messaging enable*/
	unsigned int ltm_enable				: 1;
	 /* True if we should send an LPM notification after the status stage */
	unsigned int send_lpm					: 1;
	unsigned int eps_enabled				: 1;
	unsigned int udc_enabled				: 1;
	unsigned int pullup						: 1;

	u8 speed;
	spinlock_t lock;

	enum odin_usb3_speed_mode speed_mode;

#ifdef CONFIG_CPU_IDLE_ODIN_WORKAROUND
	int cpu_handle;
#define USB_CPU_AFFINITY 3
#endif

	unsigned int test_mode				: 1;
	u8 test_mode_seletor;
};

struct odin_udc_trb {
	u32		bptl;
	u32		bpth;
	u32		status;
	u32		control;
} __packed;

int odin_udc_handshake(void __iomem *ptr, u32 val);
#ifdef CONFIG_USB_G_LGE_ANDROID
int odin_udc_check_regs(void __iomem *ptr, bool exist,
							u32 val, u32 time_us);
#endif
void odin_udc_fill_trb(struct odin_udc_trb *trb, dma_addr_t dma_addr,
			u32 dma_len, u32 type, u32 ctrlbits, int own);
void odin_udc_epcmd_set_cfg(struct odin_udc *udc,
			unsigned epnum, u32 depcfg0, u32 depcfg1, u32 depcfg2);
int odin_udc_epcmd_start_xfer(struct odin_udc *udc,
			struct odin_udc_ep *odin_ep,
			dma_addr_t dma_addr, u32 stream_or_uf);
void odin_udc_epcmd_end_xfer(struct odin_udc *udc,
			struct odin_udc_ep *odin_ep, u32 tri, int flags);
void odin_udc_epcmd_set_stall(struct odin_udc *udc,
						struct odin_udc_ep *odin_ep);
void odin_udc_epcmd_clr_stall(struct odin_udc *udc,
						struct odin_udc_ep *odin_ep);
int odin_udc_flush_all_fifo(struct odin_udc *udc);
int odin_udc_get_device_speed(struct odin_udc *udc);
int odin_udc_set_link_state(struct odin_udc *udc, int state);
void odin_udc_unmap_buffers(struct device *dev,
				struct usb_request *usb_req,
				struct odin_udc_ep *odin_ep, int *req_flags);
void odin_udc_fill_trbs(struct odin_udc *udc,
		struct odin_udc_ep *odin_ep, struct odin_usb3_req *req);
void odin_udc_trb_free(struct odin_udc_ep *odin_ep);
void odin_udc_ep0_activate(struct odin_udc *udc);
int odin_udc_ep_activate(struct odin_udc *udc, struct odin_udc_ep *odin_ep,
		const struct usb_ss_ep_comp_descriptor *comp_desc);
void odin_udc_ep0_out_start(struct odin_udc *udc);
void odin_udc_ep0_start_transfer(struct odin_udc *udc,
				     struct odin_usb3_req *req);
void odin_udc_ep_start_transfer(struct odin_udc *udc,
		struct odin_udc_ep *odin_ep, struct odin_usb3_req *req, u32 event);
void odin_udc_stop_xfer(struct odin_udc *udc,
						struct odin_udc_ep *odin_ep);
void odin_udc_isoc_xfer_start(struct odin_udc *udc,
			struct odin_udc_ep *odin_ep, u32 event);
void odin_udc_iso_xfer_stop(struct odin_udc *udc,
						struct odin_udc_ep *odin_ep);
void odin_udc_enable_device_interrupts(struct odin_udc *udc);
void odin_udc_send_zlp(struct odin_udc *udc, struct odin_usb3_req *req);
int odin_udc_memory_alloc(struct odin_udc* udc);
void odin_udc_memory_free(struct odin_udc* udc);
#endif /*__DRIVERS_USB_DWC3_ODIN_UDC__H */

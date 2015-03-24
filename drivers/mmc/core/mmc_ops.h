/*
 *  linux/drivers/mmc/core/mmc_ops.h
 *
 *  Copyright 2006-2007 Pierre Ossman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#ifndef _MMC_MMC_OPS_H
#define _MMC_MMC_OPS_H

#define TOSHIBA_EMMC_MFID		0x11

struct tsb_wear_info {
	unsigned char sub_cmd_no;		/* sub command no. */
	unsigned char reserved1;		/* reserved */
	unsigned char reserved2;		/* reserved */
	unsigned char status;			/* status */
	unsigned int mlc_wr_max;		/* mlc write erase maximum */
	unsigned int mlc_wr_avg;		/* slc write erase avg */
	unsigned int slc_wr_max;		/* mlc write erase maximum */
	unsigned int slc_wr_avg;		/* slc write erase avg */
};

struct tsb_cmd_format {
	unsigned char sub_cmd_no;		/* sub command no. */
	unsigned char reserved1;		/* reserved */
	unsigned char reserved2;		/* reserved */
	unsigned char reserved3;		/* reserved */
	unsigned char pwd1;			/* reserved */
	unsigned char pwd2;			/* reserved */
	unsigned char pwd3;			/* reserved */
	unsigned char pwd4;			/* reserved */
};

int mmc_select_card(struct mmc_card *card);
int mmc_deselect_cards(struct mmc_host *host);
int mmc_go_idle(struct mmc_host *host);
int mmc_send_op_cond(struct mmc_host *host, u32 ocr, u32 *rocr);
int mmc_all_send_cid(struct mmc_host *host, u32 *cid);
int mmc_set_relative_addr(struct mmc_card *card);
int mmc_send_csd(struct mmc_card *card, u32 *csd);
int mmc_send_ext_csd(struct mmc_card *card, u8 *ext_csd);
int mmc_send_status(struct mmc_card *card, u32 *status);
int mmc_send_cid(struct mmc_host *host, u32 *cid);
int mmc_spi_read_ocr(struct mmc_host *host, int highcap, u32 *ocrp);
int mmc_spi_set_crc(struct mmc_host *host, int use_crc);
int mmc_bus_test(struct mmc_card *card, u8 bus_width);
int mmc_send_hpi_cmd(struct mmc_card *card, u32 *status);

#endif


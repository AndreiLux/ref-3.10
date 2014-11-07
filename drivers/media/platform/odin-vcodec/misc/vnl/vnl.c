/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
 * Younghyun Jo <younghyun.jo@lge.com>
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

#include <linux/netlink.h>
#include <linux/net.h>

#include <net/netlink.h>
#include <net/net_namespace.h>

#include "vnl.h"

static struct sock *vdec_nl_sk = NULL;

static void vnl_receive(struct sk_buff *sk)
{
	pr_info("[vnl] %s \n", __func__);
	return ;
};

static struct sock* _vnl_init(void)
{
	struct netlink_kernel_cfg cfg;
	struct sock *nl_sk;

	memset(&cfg, 0, sizeof(cfg));

	cfg.input = vnl_receive;
	cfg.flags = NL_CFG_F_NONROOT_RECV|NL_CFG_F_NONROOT_SEND;

	nl_sk = netlink_kernel_create(&init_net, NETLINK_VCODEC, &cfg);
	if (nl_sk == NULL) {
		pr_err("[vnl] netlink_kernel_create() failed\n");
	}

	return nl_sk;
}

static void _vnl_cleanup(struct sock *nl_sk)
{
	netlink_kernel_release(nl_sk);
}

static int _vnl_send(struct sock *nl_sk, int dst_port, void *payload, int payload_len)
{
	int errno = 0;
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh;

	/* pr_info("[vnl] %s start\n", __func__); */

	skb = nlmsg_new(payload_len, GFP_KERNEL);
	if (skb == NULL) {
		pr_err("[vnl] nlmsg_new() failed\n"); 
		return -ENOMEM;
	}

	nlh = nlmsg_put(skb, dst_port, 0, 0, payload_len, 0);
	if (nlh == NULL) {
		errno = -ENOBUFS;
		pr_err("[vnl] nlmsg_put() failed\n"); 
		goto err_nlmsg_put;;
	}

	NETLINK_CB(skb).dst_group = 1;

	memcpy(nlmsg_data(nlh), payload, payload_len);

	errno = nlmsg_unicast(nl_sk, skb, dst_port);
	if (errno < 0) {
		pr_err("[vnl] nlmsg_unicast failed errno:%d \n", errno); 
		goto err_nlmsg_unicast;
	}

	/* pr_info("[vnl] %s success\n", __func__); */

	return 0;

err_nlmsg_unicast:
err_nlmsg_put:
	if (skb)
		nlmsg_free(skb);

	return errno;
}

int vnl_init(void)
{
	pr_info("[vnl] %s start\n", __func__);

	vdec_nl_sk = _vnl_init();
	if (IS_ERR_OR_NULL(vdec_nl_sk))
		return -1;

	pr_info("[vnl] %s success\n", __func__);

	return 0;
}

void vnl_cleanup(void)
{
	_vnl_cleanup(vdec_nl_sk);
}

int vnl_send(int dst_port, void *payload, int payload_len)
{
	if (vdec_nl_sk == NULL) {
		pr_err("[vnl] vdec_nl_sk == NULL\n"); 
		return -EINVAL;
	}

	return _vnl_send(vdec_nl_sk, dst_port, payload, payload_len);
}

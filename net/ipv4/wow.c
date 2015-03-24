/* wow.c  -- Wake-on-Wireless packet sniffer/detector
 *
 * Copyright 2014 Lab126, Inc.
 *
 * Author: Sandeep Patil <sandep@lab126.com>
 * Author: Wentian Cui <wentianc@lab126.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/types.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/if_packet.h>
#include <net/ip.h>
#include <net/inet_hashtables.h>
#include <net/tcp.h>
#include <linux/errno.h>
#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/in.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/device.h>

#include <net/wow.h>

static struct wow_sniffer *priv;
static unsigned int total_wow_pkts;
static LIST_HEAD(wow_pkt_list);
static DEFINE_MUTEX(wow_lock);

static struct dentry *wow_pkt_stats_dentry;

#define dump_skb(s)	do {} while (0)

static inline void ip_dot(char *str, size_t str_size, __u32 addr)
{
	unsigned char *ip = (unsigned char *)&addr;
	snprintf(str, str_size, "%u.%u.%u.%u",
			(unsigned)ip[3], (unsigned)ip[2],
			(unsigned)ip[1], (unsigned)ip[0]);
}

static bool match_wow_pkt(struct wow_pkt_info *l,
				struct wow_pkt_info *r)
{
	if (l->l3_proto == r->l3_proto &&
		l->saddr == r->saddr &&
		l->l4_proto == r->l4_proto &&
		l->sport == r->sport)
		return true;

	return false;
}

static void add_wow_pkt(struct wow_pkt_info *info)
{
	struct wow_pkt_info *w;

	rcu_read_lock();
	list_for_each_entry_rcu(w, &wow_pkt_list, link) {
		if (match_wow_pkt(w, info)) {
			atomic_inc(&w->count);
			kfree(info);
			info = NULL;
			break;
		}
	}
	rcu_read_unlock();

	if (info) {
		mutex_lock(&wow_lock);
		atomic_inc(&info->count);
		list_add_rcu(&info->link, &wow_pkt_list);
		mutex_unlock(&wow_lock);
	}

	total_wow_pkts++;
}

static void wow_put_sk(struct sock *sk)
{
	if (sk->sk_state == TCP_TIME_WAIT)
		inet_twsk_put(inet_twsk(sk));
	else
		sock_put(sk);
}

static void unwrap_tcp(struct sk_buff *skb, struct iphdr *iph,
			struct wow_pkt_info *info)
{
	struct tcphdr *th;
	__u16 sport, dport;
	size_t hdr_size;
	struct sock *sk;
	bool got_sk = false;

	hdr_size = sizeof(struct tcphdr);
	if (skb->len < hdr_size) {
		pr_warn("wow: no tcp hdr\n");
		return;
	}

	th = tcp_hdr(skb);
	dump_skb(skb);
	if (th->doff < (hdr_size/4)) {
		pr_warn("wow: bad tcp hdr len\n");
		return;
	}

	if (skb->len < (th->doff * 4)) {
		pr_warn("wow: partial tcp hdr\n");
		return;
	}

	sport = ntohs(th->source);
	dport = ntohs(th->dest);
	info->sport = sport;

	sk = skb->sk;
	if (sk && sk->sk_state == TCP_TIME_WAIT)
		sk = NULL;

	if (sk == NULL) {
		sk = inet_lookup(dev_net(skb->dev), &tcp_hashinfo, iph->saddr,
				th->source, iph->daddr, th->dest,
				skb->dev->ifindex);
		if (!sk) {
			pr_warn("wow: tcp no socket for port (%u)\n",
					(unsigned)dport);
			goto put_sock;
		}

		got_sk = sk;

		pr_emerg("wow: %p->sk_proto=%u ->sk_state=%d\n",
			sk, sk->sk_protocol, sk->sk_state);

		/* When in TCP_TIME_WAIT the sk is not a "struct sock" but
		 * "struct inet_timewait_sock" which is missing fields.
		 *
		 * Main reason why we would run into crashes from
		 * wow_detect_work()
		 */

		if (sk->sk_state == TCP_TIME_WAIT) {
			pr_info("wow: skipping over time_wait socket\n");
			goto put_sock;
		}
	}

	lock_sock(sk);
#ifdef CONFIG_DEBUG_SPINLOCK
	if (sk->sk_callback_lock.magic != RWLOCK_MAGIC) {
		pr_emerg("wow: processing sk(%p) with refcnt(%d)\n",
				sk, atomic_read(&sk->sk_refcnt));
		pr_emerg("wow_error: sk(%p) lock(%p) magic(%u)\n",
				sk, &sk->sk_callback_lock,
				sk->sk_callback_lock.magic);
		BUG();
	}
#endif
	info->uid = info->gid = sock_i_uid(sk);
	release_sock(sk);

put_sock:
	if (got_sk)
		wow_put_sk(sk);
}

static void unwrap_ip(struct sk_buff *skb, struct wow_pkt_info *info)
{
	struct iphdr *iph;
	int len;

	if (unlikely(sizeof(struct iphdr) > skb->len))
		goto out;

	iph = ip_hdr(skb);
	if (unlikely((iph->ihl*4) > skb->len))
		goto out;

	len = ntohs(iph->tot_len);
	if ((skb->len < len) ||
			(len < iph->ihl*4))
		goto out;

	info->saddr = ntohl(iph->saddr);
	info->daddr = ntohl(iph->daddr);

	if (unlikely(ipv4_is_multicast(iph->daddr)))
		goto out;

	if (unlikely(ipv4_is_local_multicast(iph->daddr)))
		goto out;

	if (unlikely(ipv4_is_lbcast(iph->daddr)))
		goto out;

	/* if there are more fragments to come or we are at an offset
	 * just log the source and destination ip and get out
	 */
	if (ip_is_fragment(iph)) {
		info->frag_off = (iph->frag_off &
				htons(IP_MF | IP_OFFSET));
		goto out;
	}

	info->l4_proto = iph->protocol;
	/* move pointers to next proto header */
	skb_pull(skb, ip_hdrlen(skb));
	skb_reset_transport_header(skb);

	switch (iph->protocol) {
	/* tcp */
	case IPPROTO_IP:
	case IPPROTO_TCP:
		/* set uid/gid to invalid first */
		info->uid = info->gid = UINT_MAX;
		unwrap_tcp(skb, iph, info);
		break;
	case IPPROTO_UDP:
		/* placeholder for udp unwrap */
		break;
	default:
		/* Nothing to do */
		break;
	};

out:
	return;
}

static void wow_detect_work(struct work_struct *work)
{
	struct wow_sniffer *p =
		container_of(work, struct wow_sniffer, wow_work);
	struct sk_buff *skb;
	struct wow_pkt_info *info;

	/* we come here only when we know we have an ipv4 packet */
	while (skb_queue_len(&p->q)) {

		skb = skb_dequeue(&p->q);
		BUG_ON(!skb);

		info = kzalloc(sizeof(*info), GFP_KERNEL);
		if (!info) {
			pr_warn("%s: skipping packet\n", __func__);
			kfree_skb(skb);
			continue;
		}

		info->pkt_type = skb->pkt_type;
		info->l3_proto = ntohs(skb->protocol);

		if (info->l3_proto == ETH_P_IP)
			unwrap_ip(skb, info);

		add_wow_pkt(info);

		/* release skb */
		kfree_skb(skb);
	}

}

/**
 * tag_wow_skb - tag socket buffer with a patter so the sniffer can detect it
 *		 when it gets pushed up the network stack
 *
 * @skb - socket buffer to tag
 *
 * The function uses first 4 bytes of skb->cb[48] and writes them with a
 * pattern. This is hardly unique and can/should be changed in future.
 *
 * Fix Me: @ssp
 */
void tag_wow_skb(struct sk_buff *skb)
{
	struct wow_skb_parm *parm;

	if (!skb)
		return;

	parm = WOWCB(skb);

	memset(&parm->magic[0], 0xa5, 4);
}
EXPORT_SYMBOL(tag_wow_skb);

static inline bool is_wow_packet(struct sk_buff *skb)
{
	struct wow_skb_parm *parm = WOWCB(skb);
	if (unlikely(parm->magic[0] == 0xa5 &&
		parm->magic[1] == 0xa5 &&
		parm->magic[2] == 0xa5 &&
		parm->magic[3] == 0xa5))
		return true;
	return false;
}

static bool is_wow_resume(void)
{
	ktime_t ts, now;
	wakeup_event_t ev;

	ev = pm_get_resume_ev(&ts);
	if ((ev != WEV_WIFI) && (ev != WEV_WAN))
		return false;

	/* Make sure the resume happened during the last couple
	 * of seconds. This is very dangerous approach but if and when
	 * we do end up tracking additional lpm wakeups, we will
	 * be easily able to ignore them based on the count of wakeup events
	 * seen on Wifi/Wan interrupts.
	 *
	 * Fix Me: find better way to differentiate between *real wakeups* and
	 * wifi lpm wakeup packets
	 */

	now = ns_to_ktime(sched_clock());

	if ((s64)(2*NSEC_PER_SEC) < ktime_to_ns(ktime_sub(now, ts)))
		return false;

	return true;
}

static int wow_rcv(struct sk_buff *skb, struct net_device *dev,
		struct packet_type *pt, struct net_device *orig_dev)
{
	struct wow_sniffer *s = (struct wow_sniffer *)pt->af_packet_priv;
	struct sk_buff *clone;

	if (likely(skb->pkt_type == PACKET_OUTGOING ||
			skb->pkt_type == PACKET_LOOPBACK ||
			!is_wow_packet(skb)))
		goto drop;

	if (is_wow_resume()) {
#ifdef DUMP_WOW_PACKET
		print_hex_dump(KERN_INFO, "wow_rcv ",
				DUMP_PREFIX_OFFSET, 16, 2, skb->data,
				skb->len, true);

		pr_info("-----------------------------\n");
		print_hex_dump(KERN_INFO, "wow_rcv ",
				DUMP_PREFIX_OFFSET, 16, 4, skb->data,
				skb->len, true);
#endif
		/* new skb with dataref incremented */
		clone = skb_clone(skb, GFP_ATOMIC);
		if (clone) {
			skb_queue_tail(&s->q, clone);
			schedule_work(&s->wow_work);
		}
	}
drop:
	consume_skb(skb);
	return NET_RX_SUCCESS;
}

struct packet_type wow_packet = {
	.type = cpu_to_be16(ETH_P_ALL),
	.func = wow_rcv,
};

/**
 * show_wow_pkt - Print one wow pkt details.
 * @m: seq_file to print the statistics into.
 * @v: wow pkt info object.
 */
static int show_wow_pkt(struct seq_file *m, void *v)
{
	char saddr[16] = {0,};
	char l3[10] = {0,};
	char l4[10] = {0,};
	struct wow_pkt_info *w = (struct wow_pkt_info *)v;

	ip_dot(saddr, sizeof(saddr), w->saddr);

	switch (w->l3_proto) {
	case ETH_P_IP:
		strlcpy(l3, "ip", sizeof(l3));
		break;
	case ETH_P_ARP:
		strlcpy(l3, "arp", sizeof(l3));
		break;
	default:
		snprintf(l3, sizeof(l3), "%04x", w->l3_proto);
	}

	switch (w->l4_proto) {
	/* tcp */
	case IPPROTO_IP:
	case IPPROTO_TCP:
		strlcpy(l4, "tcp", sizeof(l4));
		break;
	case IPPROTO_UDP:
		strlcpy(l4, "udp", sizeof(l4));
		break;
	case IPPROTO_ICMP:
		strlcpy(l4, "icmp", sizeof(l4));
		break;
	default:
		snprintf(l4, sizeof(l4), "%u", (unsigned int)w->l4_proto);
	};

	seq_printf(m, "%-18s%-8u%-8s%-8s%-8u%-8d%-8d\n",
			saddr, atomic_read(&w->count), l3, l4, w->sport, (int)w->uid,
			(int)w->gid);

	return 0;
}

/**
 * wow_pkt_seq_start - setup dumping the wow_pkt_list
 * @m: seq_file to print the statistics into.
 * @pos: index into the wow_pkt_list
 */
static void *wow_seq_start(struct seq_file *m, loff_t *pos)
{
	struct wow_pkt_info *w;
	loff_t index = *pos, i;

	if (index >= total_wow_pkts)
		return NULL;

	if (index == 0) {
		seq_printf(m, "%-18s%-8s%-8s%-8s%-8s%-8s%-8s\n",
			"source", "count", "l3proto", "l4proto", "s-port",
			"uid", "gid");
	}

	i = -1LL;
	rcu_read_lock();
	list_for_each_entry_rcu(w, &wow_pkt_list, link) {
		i++;
		if (i == index) {
			rcu_read_unlock();
			return (void *)w;
		}
	}
	rcu_read_unlock();

	return NULL;
}

static void *wow_seq_next(struct seq_file *f, void *v, loff_t *pos)
{
	struct wow_pkt_info *w = (struct wow_pkt_info *)v;
	struct wow_pkt_info *next = NULL;
	struct list_head *__next;

	(*pos)++;

	rcu_read_lock();
	/* to avoid race, we may read 'next' only once. */
	__next = ACCESS_ONCE(w->link.next);
	if (__next != &wow_pkt_list)
		next = list_entry_rcu(__next, typeof(*w), link);
	rcu_read_unlock();

	return (void *)next;
}

static void wow_seq_stop(struct seq_file *f, void *v)
{
	/* Do nothing */
}

static const struct seq_operations wow_pkt_seq_ops = {
	.start = wow_seq_start,
	.next  = wow_seq_next,
	.stop  = wow_seq_stop,
	.show  = show_wow_pkt,
};

static int wow_pkt_stats_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &wow_pkt_seq_ops);
}

static const struct file_operations wow_pkt_stats_fops = {
	.owner = THIS_MODULE,
	.open = wow_pkt_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int __init wow_init(void)
{
	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		pr_err("%s: not enough memory!\n", __func__);
		return -ENOMEM;
	}

	skb_queue_head_init(&priv->q);
	INIT_WORK(&priv->wow_work, wow_detect_work);
	wow_packet.af_packet_priv = priv;

	wow_pkt_stats_dentry = debugfs_create_file("wow_events",
			S_IRUGO, NULL, NULL, &wow_pkt_stats_fops);

	dev_add_pack(&wow_packet);

	pr_info("wow_sniffer registered\n");

	return 0;
}

module_init(wow_init);

MODULE_DESCRIPTION("WoW detector");
MODULE_AUTHOR("Lab126");
MODULE_LICENSE("GPL");

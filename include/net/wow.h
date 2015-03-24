#ifndef __NET_WOW_H__
#define __NET_WOW_H__

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/sched.h>

struct wow_skb_parm {
	char magic[4];
	char pad[44];
};

#define WOWCB(skb) ((struct wow_skb_parm *)((skb)->cb))

struct wow_sniffer {
	struct work_struct wow_work;
	struct sk_buff_head q;
};

struct wow_pkt_info {
	uid_t uid;
	gid_t gid;
	u8 pkt_type;
	u16 l3_proto;
	u32 saddr;
	u32 daddr;
	u16 frag_off;
	u8 l4_proto;
	u16 sport;
	atomic_t count;
	struct list_head link;
};

extern void tag_wow_skb(struct sk_buff *skb);

#endif /* __NET_WOW_H__ */

/* mailbox_debugfs.h */

#ifndef MAILBOX_DEBUGFS_H
#define MAILBOX_DEBUGFS_H

#ifdef CONFIG_HISI_MAILBOX_DEBUGFS
extern void mbox_debugfs_register(struct hisi_mbox_device **list);
extern void mbox_debugfs_unregister(void);
#else
static inline void mbox_debugfs_register(struct hisi_mbox_device **list)
{
	return;
}

static inline void mbox_debugfs_unregister(void)
{
	return;
}
#endif /* CONFIG_HISI_MAILBOX_DEBUGFS */

#endif /* mailbox_debugfs.h */

#ifndef __MTK_SELINUX_WARNING_LIST_H__
#define __MTK_SELINUX_WARNING_LIST_H__

#ifdef CONFIG_SECURITY_SELINUX_DEVELOP
extern int selinux_enforcing;
#else
#define selinux_enforcing 1
#endif

#define AEE_FILTER_NUM 10
extern const char *aee_filter_list[AEE_FILTER_NUM];
extern void mtk_audit_hook(char *data);

#endif

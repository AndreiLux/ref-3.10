#ifndef SEC_MOD_CORE_H
#define SEC_MOD_CORE_H

/******************************************************************************
 *  EXPORT FUNCTION
 ******************************************************************************/
extern long sec_core_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
extern void sec_core_init(void);
extern void sec_core_exit(void);
extern int sec_get_random_id(unsigned int *rid);
extern void sec_update_lks(unsigned char tr, unsigned char dn, unsigned char fb_ulk);

#endif				/* SEC_MOD_CORE_H */

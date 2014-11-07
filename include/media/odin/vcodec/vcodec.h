#ifndef _VCODEC_H_
#define _VCODEC_H_

extern void* vcodec_open(void);
extern long vcodec_unlocked_ioctl(void *_vcodec_ch, unsigned int cmd, unsigned long arg);
extern int vcodec_release(void *_vcodec_ch);

#endif /* #ifndef _VCODEC_H_ */

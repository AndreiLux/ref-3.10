#ifndef DEV_H
#define DEV_H

/******************************************************************************
 *  EXPORT VARIABLE
 ******************************************************************************/
extern uint32 secro_img_off;
extern uint32 secro_img_mtd_num;
extern uint32 secro_v3_off;

extern bool bSecroExist;
extern bool bSecroV5Exist;
extern bool bSecroIntergiy;
extern bool bSecroV5Intergiy;

/******************************************************************************
 *  EXPORT FUNCTION
 ******************************************************************************/
extern void sec_dev_dump_part(void);
extern void sec_dev_find_parts(void);
extern int sec_dev_read_rom_info(void);
extern int sec_dev_read_secroimg(void);
extern int sec_dev_read_secroimg_v5(unsigned int index);
extern unsigned int sec_dev_read_image(char *part_name, char *buf, u64 off,
					 unsigned int sz, unsigned int image_type);
#endif	/* DEV_H */

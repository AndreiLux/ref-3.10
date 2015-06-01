#ifndef _PCA9575_
#define _PCA9575_
//#include <mach/mt_typedefs.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned char ubool;

#define GPIO_IN0_REG 0x00
#define GPIO_IN1_REG 0x01 
#define GPIO_INVERSION0_REG 0x02
#define GPIO_INVERSION1_REG 0x03 
#define GPIO_PULL_ENABLE0_REG 0x04
#define GPIO_PULL_ENABLE1_REG 0x05 
#define GPIO_BUSHOLD_ENABLE0_REG  0x04
#define GPIO_BUSHOLD_ENABLE1_REG  0x05
#define GPIO_PULLUP0_REG 0x06
#define GPIO_PULLUP1_REG 0x07
#define GPIO_DIR0_REG 0x08
#define GPIO_DIR1_REG 0x09
#define GPIO_OUTPUT0_REG 0x0a
#define GPIO_OUTPUT1_REG  0x0b
#define GPIO_INT_MASK0_REG 0x0c
#define GPIO_INT_MASK1_REG  0xd0
#define GPIO_INT_STATUS0_REG 0x0e
#define GPIO_INT_STATUS1_REG  0x0f

#define GPIO_REG_LEN 0x08

#define GPIO_P0_0 0x0
#define GPIO_P0_1 0x1
#define GPIO_P0_2 0x2
#define GPIO_P0_3 0x3
#define GPIO_P0_4 0x4
#define GPIO_P0_5 0x5
#define GPIO_P0_6 0x6
#define GPIO_P0_7 0x7

#define GPIO_P1_0 0x8
#define GPIO_P1_1 0x9
#define GPIO_P1_2 0xa
#define GPIO_P1_3 0xb
#define GPIO_P1_4 0xc
#define GPIO_P1_5 0xd
#define GPIO_P1_6 0xe
#define GPIO_P1_7 0xf

#define GPIO_OUT_DIR 0
#define GPIO_IN_DIR 1

#define GPIO_OUTPUT_ZERO 0
#define GPIO_OUTPUT_ONE 1

#define GPIO_EXT_ERR(fmt, args...)    printk(KERN_ERR "[pca9575]""%s: line:%d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GPIO_EXT_DBG(fmt, args...)    printk(KERN_ERR "[pca9575]" fmt, ##args)


static int i2c_write_reg( u16 addr, u8 data );

static int i2c_read_reg( u16 addr);

static int i2c_get_reg_bit(u8 reg, u8 bit);
 
static int i2c_get_gpio_function_status(u8 function_reg_base, u8 gpionum);



extern int pca9575_get_gpio_in(u16 gpionum);
 
extern int pca9575_get_gpio_inversion(u16 gpionum);
 
extern int pca9575_set_gpio_inversion(u16 gpionum, ubool isInversion);
 
extern int pca9575_gpio_port0_pull_enable(ubool enable);
extern int pca9575_gpio_port1_pull_enable(ubool enable);

 
extern int pca9575_get_gpio_port_bushold_enable_status( u8 port );
extern int pca9575_get_gpio_port_pull_enable_status( u8 port );
 
extern int pca9575_gpio_port0_bushold_enable(ubool enable);
extern int pca9575_gpio_port1_bushold_enable(ubool enable);



 

extern int pca9575_set_gpio_pullup(u16 gpionum, ubool isPullup);
 
extern int pca9575_get_gpio_pullup(u16 gpionum);

extern int pca9575_set_gpio_dir(u16 gpionum, ubool isInput);
  
extern int pca9575_get_gpio_dir(u16 gpionum);
    
extern int pca9575_set_gpio_output(u16 gpionum, ubool value);

extern int pca9575_mask_gpio_int(u16 gpionum, ubool isMask);
  
extern int pca9575_get_gpio_int_mask_status(u16 gpionum);
 
extern int pca9575_get_gpio_int_status(u16 gpionum);
extern int pca9575_get_gpio_output(u16 gpionum);

#endif /* _CUST_LEDS_DEF_H */

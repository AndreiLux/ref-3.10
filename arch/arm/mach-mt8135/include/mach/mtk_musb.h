#ifndef __MUSB_MTK_MUSB_H__
#define __MUSB_MTK_MUSB_H__
#include <mach/mt_reg_base.h>

#include <linux/wakelock.h>
#include <mach/mt_gpio.h>
#include <mach/pmic_mt6329_sw.h>
/* #include <mach/upmu_common.h> */
#include <mach/irqs.h>

#define USBPHY_READ8(offset)          __raw_readb(USB_SIF_BASE+0x800+offset)
#define USBPHY_WRITE8(offset, value)  __raw_writeb(value, USB_SIF_BASE+0x800+offset)
#define USBPHY_SET8(offset, mask)     USBPHY_WRITE8(offset, USBPHY_READ8(offset) | mask)
#define USBPHY_CLR8(offset, mask)     USBPHY_WRITE8(offset, USBPHY_READ8(offset) & ~mask)

#define USBPHY_READ16(offset)          __raw_readw(USB_SIF_BASE+0x800+offset)
#define USBPHY_WRITE16(offset, value)  __raw_writew(value, USB_SIF_BASE+0x800+offset)
#define USBPHY_SET16(offset, mask)     USBPHY_WRITE16(offset, USBPHY_READ16(offset) | mask)
#define USBPHY_CLR16(offset, mask)     USBPHY_WRITE16(offset, USBPHY_READ16(offset) & ~mask)

#define USBPHY_READ32(offset)          __raw_readl(USB_SIF_BASE+0x800+offset)
#define USBPHY_WRITE32(offset, value)  __raw_writel(value, USB_SIF_BASE+0x800+offset)
#define USBPHY_SET32(offset, mask)     USBPHY_WRITE32(offset, USBPHY_READ32(offset) | mask)
#define USBPHY_CLR32(offset, mask)     USBPHY_WRITE32(offset, USBPHY_READ32(offset) & ~mask)

#define REG_SET8(addr, mask)          __raw_writeb(__raw_readb(addr) | mask, addr)
#define REG_CLR8(addr, mask)          __raw_writeb(__raw_readb(addr) & ~mask, addr)

#define REG_SET16(addr, mask)          __raw_writew(__raw_readw(addr) | mask, addr)
#define REG_CLR16(addr, mask)          __raw_writew(__raw_readw(addr) & ~mask, addr)

#define REG_SET32(addr, mask)          __raw_writel(__raw_readl(addr) | mask, addr)
#define REG_CLR32(addr, mask)          __raw_writel(__raw_readl(addr) & ~mask, addr)

/*
MTK Software reset reg
*/
#define MUSB_SWRST 0x74
#define MUSB_SWRST_PHY_RST         (1<<7)
#define MUSB_SWRST_PHYSIG_GATE_HS  (1<<6)
#define MUSB_SWRST_PHYSIG_GATE_EN  (1<<5)
#define MUSB_SWRST_REDUCE_DLY      (1<<4)
#define MUSB_SWRST_UNDO_SRPFIX     (1<<3)
#define MUSB_SWRST_FRC_VBUSVALID   (1<<2)
#define MUSB_SWRST_SWRST           (1<<1)
#define MUSB_SWRST_DISUSBRESET     (1<<0)

#define USB_L1INTS (0x00a0)	/* USB level 1 interrupt status register */
#define USB_L1INTM (0x00a4)	/* USB level 1 interrupt mask register  */
#define USB_L1INTP (0x00a8)	/* USB level 1 interrupt polarity register  */


#define DMA_INTR (USB_BASE + 0x0200)
#define DMA_INTR_UNMASK_CLR_OFFSET (16)
#define DMA_INTR_UNMASK_SET_OFFSET (24)
#define USB_DMA_REALCOUNT(chan) (0x0280+0x10*(chan))


/* ====================== */
/* USB interrupt register */
/* ====================== */

/* word access */
#define TX_INT_STATUS        (1<<0)
#define RX_INT_STATUS        (1<<1)
#define USBCOM_INT_STATUS    (1<<2)
#define DMA_INT_STATUS       (1<<3)
#define PSR_INT_STATUS       (1<<4)
#define QINT_STATUS          (1<<5)
#define QHIF_INT_STATUS      (1<<6)
#define DPDM_INT_STATUS      (1<<7)
#define VBUSVALID_INT_STATUS (1<<8)
#define IDDIG_INT_STATUS     (1<<9)
#define DRVVBUS_INT_STATUS   (1<<10)

#define VBUSVALID_INT_POL    (1<<8)
#define IDDIG_INT_POL        (1<<9)
#define DRVVBUS_INT_POL      (1<<10)

/* usb phy setting */
extern void usb_phy_recover(void);
extern void usb_phy_savecurrent(void);
extern bool usb_enable_clock(bool enable);
extern void usb_phy_poweron(void);

extern bool upmu_is_chr_det(void);
extern bool is_usb_connected(void);

#ifdef CONFIG_MTK_FAN5405_SUPPORT
extern void fan5405_set_opa_mode(kal_uint32 val);
extern void fan5405_set_otg_pl(kal_uint32 val);
extern void fan5405_set_otg_en(kal_uint32 val);
extern kal_uint32 fan5405_config_interface_liao(kal_uint8 RegNum, kal_uint8 val);
#elif defined(CONFIG_MTK_NCP1851_SUPPORT)
extern void tbl_charger_otg_vbus(kal_uint32 mode);
#endif

#endif
/* extern bool mt6573_usb_enable_clock(bool enable) ; */

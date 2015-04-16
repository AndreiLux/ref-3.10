#ifndef _DWC_OTG_HI3630_
#define _DWC_OTG_HI3630_
#include <linux/wakelock.h>
#include <linux/notifier.h>
#include <linux/huawei/usb/hisi_usb.h>
#include <linux/regulator/consumer.h>

/**
 * usb otg ahbif registers definations
 */
union usbotg2_ctrl0 {
	unsigned reg;
	struct {
		unsigned idpullup_sel:1;
		unsigned idpullup:1;
		unsigned acaenb_sel:1;
		unsigned acaenb:1;
		unsigned id_sel:2;
		unsigned id:1;
		unsigned drvvbus_sel:1;
		unsigned drvvbus:1;
		unsigned vbusvalid_sel:1;
		unsigned sessvld_sel:1;
		unsigned sessvld:1;
		unsigned dpdmpulldown_sel:1;
		unsigned dppulldown:1;
		unsigned dmpulldown:1;
		unsigned dbnce_fltr_bypass:1;
		unsigned reserved:16;
	} bits;
};

union usbotg2_ctrl1 {
	unsigned reg;
	struct {
		unsigned scaledown_mode:2;
		unsigned reserved:30;
	} bits;
};

union usbotg2_ctrl2 {
	unsigned reg;
	struct {
		unsigned commononn:1;
		unsigned otgdisable:1;
		unsigned vbusvldsel:1;
		unsigned vbusvldext:1;
		unsigned txbitstuffen:1;
		unsigned txbitstuffenh:1;
		unsigned fsxcvrowner:1;
		unsigned txenablen:1;
		unsigned fsdataext:1;
		unsigned fsse0ext:1;
		unsigned vatestenb:2;
		unsigned reserved:20;
	} bits;
};

union usbotg2_ctrl3 {
	unsigned reg;
	struct {
		unsigned comdistune:3;
		unsigned otgtune:3;
		unsigned sqrxtune:3;
		unsigned txfslstune:4;
		unsigned txpreempamptune:2;
		unsigned txpreemppulsetune:1;
		unsigned txrisetune:2;
		unsigned txvreftune:4;
		unsigned txhsxvtune:2;
		unsigned txrestune:2;
		unsigned reserved:6;
	} bits;
};

union usbotg2_ctrl4 {
	unsigned reg;
	struct {
		unsigned siddq:1;
		unsigned reserved:31;
	} bits;
};

union usbotg2_ctrl5 {
	unsigned reg;
	struct {
		unsigned refclksel:2;
		unsigned fsel:3;
		unsigned reserved:27;
	} bits;
};

union bc_ctrl0 {
	unsigned reg;
	struct {
		unsigned chrg_det_en:1;
		unsigned reserved:31;
	} bits;
};
union bc_ctrl1 {
	unsigned reg;
	struct {
		unsigned chrg_det_int_clr:1;
		unsigned reserved:31;
	} bits;
};
union bc_ctrl2 {
	unsigned reg;
	struct {
		unsigned chrg_det_int_msk:1;
		unsigned reserved:31;
	} bits;
};
union bc_ctrl3 {
	unsigned reg;
	struct {
		unsigned bc_mode:1;
		unsigned reserved:31;
	} bits;
};

union bc_ctrl4 {
	unsigned reg;
	struct {
		unsigned bc_opmode:2;
		unsigned bc_xcvrselect:2;
		unsigned bc_termselect:1;
		unsigned bc_txvalid:1;
		unsigned bc_txvalidh:1;
		unsigned bc_idpullup:1;
		unsigned bc_dppulldown:1;
		unsigned bc_dmpulldown:1;
		unsigned bc_suspendm:1;
		unsigned bc_sleepm:1;
		unsigned reserved:20;
	} bits;
};

union bc_ctrl5 {
	unsigned reg;
	struct {
		unsigned bc_aca_en:1;
		unsigned bc_chrg_sel:1;
		unsigned bc_vdat_src_en:1;
		unsigned bc_vdat_det_en:1;
		unsigned bc_dcd_en:1;
		unsigned reserved:27;
	} bits;
};

union bc_ctrl6 {
	unsigned reg;
	struct {
		unsigned bc_chirp_int_clr:1;
		unsigned reserved:31;
	} bits;
};
union bc_ctrl7 {
	unsigned reg;
	struct {
		unsigned bc_chirp_int_msk:1;
		unsigned reserved:31;
	} bits;
};
union bc_ctrl8 {
	unsigned reg;
	struct {
		unsigned filter_len;
	} bits;
};
union bc_sts0 {
	unsigned reg;
	struct {
		unsigned chrg_det_rawint:1;
		unsigned reserved:31;
	} bits;
};

union bc_sts1 {
	unsigned reg;
	struct {
		unsigned chrg_det_mskint:1;
		unsigned reserved:31;
	} bits;
};

union bc_sts2 {
	unsigned reg;
	struct {
		unsigned bc_vbus_valid:1;
		unsigned bc_sess_valid:1;
		unsigned bc_fs_vplus:1;
		unsigned bc_fs_vminus:1;
		unsigned bc_chg_det:1;
		unsigned bc_iddig:1;
		unsigned bc_rid_float:1;
		unsigned bc_rid_gnd:1;
		unsigned bc_rid_a:1;
		unsigned bc_rid_b:1;
		unsigned bc_rid_c:1;
		unsigned bc_chirp_on:1;
		unsigned bc_linestate:2;
		unsigned reserved:18;
	} bits;
};

union bc_sts3 {
	unsigned reg;
	struct {
		unsigned bc_rawint:1;
		unsigned reserved:31;
	} bits;
};
union bc_sts4 {
	unsigned reg;
	struct {
		unsigned bc_mskint:1;
		unsigned reserved:31;
	} bits;
};

struct usb_ahbif_registers {
	union usbotg2_ctrl0     usbotg2_ctrl0;
	union usbotg2_ctrl1     usbotg2_ctrl1;
	union usbotg2_ctrl2     usbotg2_ctrl2;
	union usbotg2_ctrl3     usbotg2_ctrl3;
	union usbotg2_ctrl4     usbotg2_ctrl4;
	union usbotg2_ctrl5     usbotg2_ctrl5;
	union bc_ctrl0          bc_ctrl0;
	union bc_ctrl1          bc_ctrl1;
	union bc_ctrl2          bc_ctrl2;
	union bc_ctrl3          bc_ctrl3;
	union bc_ctrl4          bc_ctrl4;
	union bc_ctrl5          bc_ctrl5;
	union bc_ctrl6          bc_ctrl6;
	union bc_ctrl7          bc_ctrl7;
	union bc_ctrl8          bc_ctrl8;
	union bc_sts0           bc_sts0;
	union bc_sts1           bc_sts1;
	union bc_sts2           bc_sts2;
	union bc_sts3           bc_sts3;
	union bc_sts4           bc_sts4;
};

/**
 * USB related register in PERICRG
 */
#define PERI_CRG_CLK_EN4			(0x40)
#define PERI_CRG_CLK_DIS4			(0x44)
#define PERI_CRG_CLK_CLKEN4			(0x48)
#define PERI_CRG_CLK_STAT4			(0x4C)
#define BIT_CLK_USB2OTG_REF			(1 << 8)
#define BIT_HCLK_USB2OTG			(1 << 1)
#define BIT_HCLK_USB2OTG_PMU			(1 << 0)

#define PERI_CRG_RSTEN4				(0x90)
#define PERI_CRG_RSTDIS4			(0x94)
#define PERI_CRG_RSTSTAT4			(0x98)
#define BIT_RST_USB2OTG_MUX			(1<<30)
#define BIT_RST_USB2OTG_AHBIF			(1<<29)
#define BIT_RST_USB2OTG_PHY_N			(1<<8)
#define BIT_RST_USB2OTG_ADP			(1<<7)
#define BIT_RST_USB2OTG_32K			(1<<6)
#define BIT_RST_USB2OTG				(1<<5)
#define BIT_RST_USB2OTGPHY			(1<<4)
#define BIT_RST_USB2OTGPHYPOR			(1<<3)

/**
 * USB related registers in PRRICRG
 */
#define REG_PMU_SSI_VERSION		0
#define REG_PMU_SSI_STATUS0		(0x01 << 2)
# define VBAT3P2_2D			(1 << 4)
# define VBUS5P5_D10R			(1 << 3)
# define VBUS4P3_D10			(1 << 2)
# define VBUS_COMP_VBAT_D20		(1 << 1)
#define REG_PMU_SSI_STATUS1		(0x02 << 2)
#define REG_PMU_SSI_IRQ0		(0x120 << 2)
#define REG_PMU_SSI_IRQ1		(0x121 << 2)
# define VBUS_COMP_VBAT_F		(1 << 4)
# define VBUS_COMP_VBAT_R		(1 << 3)
#define REG_PMU_SSI_IRQ_MASK_0		(0x102 << 2)
#define REG_PMU_SSI_IRQ_MASK_1		(0x103 << 2)


/**
 * USB ahbif registers
 */
#define USBPHY_ENTER_SIDDQ			(0x1)
#define USBPHY_QUIT_SIDDQ				(0x0)

#define usb_dbg(format, arg...)    \
	do {                 \
		printk(KERN_INFO "[USB][%s]"format, __func__, ##arg); \
	} while (0)
#define usb_err(format, arg...)    \
	do {                 \
		printk(KERN_ERR "[USB]"format, ##arg); \
	} while (0)

enum otg_dev_status {
	OTG_DEV_OFF = 0,
	OTG_DEV_DEVICE,
	OTG_DEV_HOST,
	OTG_DEV_SUSPEND
};

enum otg_hcd_status {
	HCD_OFF = 0,
	HCD_ON,
};

struct otg_dev {
	enum otg_dev_status status;
	enum otg_hcd_status hcd_status;
	enum otg_dev_event_type event;
	enum hisi_charger_type charger_type;

	unsigned gadget_initialized;
	bool dwc_otg_irq_enabled;

	struct lm_device    *lm_dev;
	struct platform_device *pdev;
	struct delayed_work event_work;
	struct wake_lock wake_lock;
	struct mutex lock;
	spinlock_t event_lock;

	struct atomic_notifier_head charger_type_notifier;

	void __iomem *pericrg_base;
	void __iomem *pmu_reg_base;
	void __iomem *usb_ahbif_base;

	unsigned int is_regu_on;
	struct regulator_bulk_data otgdebugsubsys_regu;
};

void dwc_otg_hi3630_wake_lock(void);
void dwc_otg_hi3630_wake_unlock(void);
int dwc_otg_hi3630_probe_stage2(void);
int dwc_otg_hi3630_is_power_on(void);
void update_charger_type(void);
int dwc_otg_hcd_setup(void);
void dwc_otg_hcd_shutdown(void);

#endif

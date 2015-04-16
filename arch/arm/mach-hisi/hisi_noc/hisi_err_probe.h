#ifndef __HISI_ERR_PROBE
#define __HISI_ERR_PROBE

/* register offset */
#define ERR_PORBE_ID_COREID_OFFSET		0x0
#define ERR_PORBE_ID_REVISIONID_OFFSET		0x4
#define ERR_PORBE_FAULTEN_OFFSET		0x8
#define ERR_PORBE_ERRVLD_OFFSET			0xC
#define ERR_PORBE_ERRCLR_OFFSET			0x10
#define ERR_PORBE_ERRLOG0_OFFSET		0x14
#define ERR_PORBE_ERRLOG1_OFFSET		0x18
#define ERR_PORBE_ERRLOG3_OFFSET		0x20
#define ERR_PORBE_ERRLOG5_OFFSET		0x28
#define ERR_PORBE_ERRLOG7_OFFSET		0x30

/* mask bits */
#define ERR_LOG0_ERRCODE_SHIFT	(8)
#define ERR_LOG0_OPC_SHIFT	(1)

#define ERR_LOG0_ERRCODE_MASK	((0x7) << ERR_LOG0_ERRCODE_SHIFT)
#define ERR_LOG0_OPC_MASK	((0xf) << ERR_LOG0_OPC_SHIFT)

/* declare functions */
void noc_err_probe_hanlder(void __iomem *base, unsigned int idx);
void enable_err_probe(void __iomem *base);
void disable_err_probe(void __iomem *base);
#endif

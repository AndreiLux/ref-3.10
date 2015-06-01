#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>

#define MET_USER_EVENT_SUPPORT
#include "core/met_drv.h"
#include "core/trace.h"

#include "mt_typedefs.h"
#include "mt_emi_bm.h"
#include "plf_trace.h"

//#define MET_EMI_DEBUG

#define DEF_BM_RW_TYPE	(BM_BOTH_READ_WRITE)
#define NBCNT		1
#define NWSCT		4
#define NLATENCY	8
#define NTRANS		8
#define NALL		3
#define NTTYPE		5
#define NIDX_EMI	(NBCNT + NWSCT + NLATENCY + NTRANS + NALL + NTTYPE)
#define NCNT		4
#define NCH		1
#define NIDX_DRAMC	(NCNT * NCH)
#define NIDX		(NIDX_EMI + NIDX_DRAMC)
#define NCLK		1
#define NARB		8
#define NIDX_BL 	(NCLK + NARB)

/* Global variables */
extern struct metdevice met_emi;
static struct kobject *kobj_emi = NULL;
static volatile int rwtype = BM_BOTH_READ_WRITE;
static int is_first;
static int countdown;
/* BW Limiter */
static int bw_limiter_enable = BM_BW_LIMITER_ENABLE;
/* TTYPE counter */
static int ttype_enable = BM_TTYPE_DISABLE;
static int ttype_master = BM_MASTER_AP_MCU;
static int ttype_busid = 0x01; /* MSDC0 */
static int ttype1_nbeat = BM_TRANS_TYPE_1BEAT;
static int ttype1_nbyte = BM_TRANS_TYPE_8Byte;
static int ttype1_burst = BM_TRANS_TYPE_BURST_INCR;
static int ttype2_nbeat = BM_TRANS_TYPE_2BEAT;
static int ttype2_nbyte = BM_TRANS_TYPE_8Byte;
static int ttype2_burst = BM_TRANS_TYPE_BURST_INCR;
static int ttype3_nbeat = BM_TRANS_TYPE_4BEAT;
static int ttype3_nbyte = BM_TRANS_TYPE_8Byte;
static int ttype3_burst = BM_TRANS_TYPE_BURST_INCR;
static int ttype4_nbeat = BM_TRANS_TYPE_8BEAT;
static int ttype4_nbyte = BM_TRANS_TYPE_8Byte;
static int ttype4_burst = BM_TRANS_TYPE_BURST_INCR;
static int ttype5_nbeat = BM_TRANS_TYPE_16BEAT;
static int ttype5_nbyte = BM_TRANS_TYPE_8Byte;
static int ttype5_burst = BM_TRANS_TYPE_BURST_INCR;
/* TEST EMI */
static int times;
static unsigned int *src_addr_v;
static dma_addr_t src_addr_p;
static unsigned int *dst_addr_v;
static dma_addr_t dst_addr_p;

static ssize_t rwtype_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t rwtype_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t emi_clock_rate_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t dcm_ctrl_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t dcm_ctrl_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t test_cqdma_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t test_cqdma_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t test_apmcu_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t test_apmcu_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype_enable_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype_enable_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t bw_limiter_enable_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t bw_limiter_enable_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype_master_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype_master_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype_busid_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype_busid_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype1_nbeat_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype1_nbeat_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype1_nbyte_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype1_nbyte_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype1_burst_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype1_burst_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype2_nbeat_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype2_nbeat_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype2_nbyte_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype2_nbyte_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype2_burst_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype2_burst_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype3_nbeat_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype3_nbeat_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype3_nbyte_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype3_nbyte_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype3_burst_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype3_burst_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype4_nbeat_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype4_nbeat_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype4_nbyte_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype4_nbyte_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype4_burst_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype4_burst_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype5_nbeat_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype5_nbeat_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype5_nbyte_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype5_nbyte_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);
static ssize_t ttype5_burst_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf);
static ssize_t ttype5_burst_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n);

static struct kobj_attribute rwtype_attr = __ATTR(rwtype, 0664, rwtype_show, rwtype_store);
static struct kobj_attribute emi_clock_rate_attr = __ATTR_RO(emi_clock_rate);
static struct kobj_attribute emi_dcm_ctrl_attr = __ATTR(emi_dcm_ctrl,
						0664,
						dcm_ctrl_show,
						dcm_ctrl_store);
static struct kobj_attribute test_cqdma_attr = __ATTR(test_cqdma,
						0664,
						test_cqdma_show,
						test_cqdma_store);
static struct kobj_attribute test_apmcu_attr = __ATTR(test_apmcu,
						0664,
						test_apmcu_show,
						test_apmcu_store);
static struct kobj_attribute ttype_enable_attr = __ATTR(ttype_enable,
						0664,
						ttype_enable_show,
						ttype_enable_store);
static struct kobj_attribute bw_limiter_enable_attr = __ATTR(bw_limiter_enable,
						0644,
						bw_limiter_enable_show,
						bw_limiter_enable_store);
static struct kobj_attribute ttype_master_attr = __ATTR(ttype_master,
						0664,
						ttype_master_show,
						ttype_master_store);
static struct kobj_attribute ttype_busid_attr = __ATTR(ttype_busid,
						0664,
						ttype_busid_show,
						ttype_busid_store);
static struct kobj_attribute ttype1_nbeat_attr = __ATTR(ttype1_nbeat,
						0664,
						ttype1_nbeat_show,
						ttype1_nbeat_store);
static struct kobj_attribute ttype1_nbyte_attr = __ATTR(ttype1_nbyte,
						0664,
						ttype1_nbyte_show,
						ttype1_nbyte_store);
static struct kobj_attribute ttype1_burst_attr = __ATTR(ttype1_burst,
						0664,
						ttype1_burst_show,
						ttype1_burst_store);
static struct kobj_attribute ttype2_nbeat_attr = __ATTR(ttype2_nbeat,
						0664,
						ttype2_nbeat_show,
						ttype2_nbeat_store);
static struct kobj_attribute ttype2_nbyte_attr = __ATTR(ttype2_nbyte,
						0664,
						ttype2_nbyte_show,
						ttype2_nbyte_store);
static struct kobj_attribute ttype2_burst_attr = __ATTR(ttype2_burst,
						0664,
						ttype2_burst_show,
						ttype2_burst_store);
static struct kobj_attribute ttype3_nbeat_attr = __ATTR(ttype3_nbeat,
						0664,
						ttype3_nbeat_show,
						ttype3_nbeat_store);
static struct kobj_attribute ttype3_nbyte_attr = __ATTR(ttype3_nbyte,
						0664,
						ttype3_nbyte_show,
						ttype3_nbyte_store);
static struct kobj_attribute ttype3_burst_attr = __ATTR(ttype3_burst,
						0664,
						ttype3_burst_show,
						ttype3_burst_store);
static struct kobj_attribute ttype4_nbeat_attr = __ATTR(ttype4_nbeat,
						0664,
						ttype4_nbeat_show,
						ttype4_nbeat_store);
static struct kobj_attribute ttype4_nbyte_attr = __ATTR(ttype4_nbyte,
						0664,
						ttype4_nbyte_show,
						ttype4_nbyte_store);
static struct kobj_attribute ttype4_burst_attr = __ATTR(ttype4_burst,
						0664,
						ttype4_burst_show,
						ttype4_burst_store);
static struct kobj_attribute ttype5_nbeat_attr = __ATTR(ttype5_nbeat,
						0664,
						ttype5_nbeat_show,
						ttype5_nbeat_store);
static struct kobj_attribute ttype5_nbyte_attr = __ATTR(ttype5_nbyte,
						0664,
						ttype5_nbyte_show,
						ttype5_nbyte_store);
static struct kobj_attribute ttype5_burst_attr = __ATTR(ttype5_burst,
						0664,
						ttype5_burst_show,
						ttype5_burst_store);

extern unsigned int mt_get_emi_freq(void);
static ssize_t emi_clock_rate_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i;
	i = snprintf(buf, PAGE_SIZE, "%d\n", mt_get_emi_freq());
	return i;
}

static ssize_t rwtype_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i;
	i = snprintf(buf, PAGE_SIZE, "%d\n", rwtype);
	return i;
}

static ssize_t rwtype_store(struct kobject *kobj,
			    struct kobj_attribute *attr,
			    const char *buf,
			    size_t n)
{
	int value;

	if ((n == 0) || (buf == NULL)) {
		return -EINVAL;
	}
	if (sscanf(buf, "%d", &value) != 1) {
		return -EINVAL;
	}
	if (value < 0 && value > BM_BOTH_READ_WRITE) {
		return -EINVAL;
	}
	rwtype = value;
	return n;
}

static ssize_t dcm_ctrl_show(struct kobject *kobj,
			     struct kobj_attribute *attr,
			     char *buf)
{
	return snprintf(buf, PAGE_SIZE, "EMI DCM is %s\n",
		MET_BM_GetEmiDcm() ? "OFF" : "ON");
}

static ssize_t dcm_ctrl_store(struct kobject *kobj,
			      struct kobj_attribute *attr,
			      const char *buf,
			      size_t n)
{
	if (!strncmp(buf, "OFF", strlen("OFF"))) {
		MET_BM_SetEmiDcm(0xff);
	} else if (!strncmp(buf, "ON", strlen("ON"))) {
		MET_BM_SetEmiDcm(0x0);
	} else {
		printk("invalid event\n");
	}

	return n;
}

static ssize_t test_cqdma_show(struct kobject *kobj,
			       struct kobj_attribute *attr,
			       char *buf)
{
	int i;
	i = snprintf(buf, PAGE_SIZE, "%d\n", times);
	return i;
}

static ssize_t test_cqdma_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	int i;

	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (sscanf(buf, "%d", &times) != 1)
		return -EINVAL;
	if (times < 0)
		return -EINVAL;

	if (times > 5000) //Less than 20M
		return -EINVAL;

	/* dma_alloc */
	src_addr_v = dma_alloc_coherent(NULL,
					PAGE_SIZE,
					&src_addr_p,
					GFP_KERNEL);
	if (src_addr_v == NULL)
		return -ENOMEM;

	dst_addr_v = dma_alloc_coherent(NULL,
					PAGE_SIZE,
					&dst_addr_p,
					GFP_KERNEL);
	if (dst_addr_v == NULL)
		return -ENOMEM;

	/* testing */
	met_tag_start(0, "TEST_EMI_CQDMA");
	for (i=0; i<times; i++) {
		MET_APDMA_SetSrcAddr(src_addr_p);
		MET_APDMA_SetDstAddr(dst_addr_p);
		MET_APDMA_SetLen(PAGE_SIZE);
		MET_APDMA_Enable(1);
		met_tag_oneshot(0, "TEST_EMI_CQDMA", PAGE_SIZE);
		/* waiting for DMA done */
		while(MET_APDMA_GetEna());
	}
	met_tag_end(0, "TEST_EMI_CQDMA");
	/* dma_free */
	if (src_addr_v != NULL)
		dma_free_coherent(NULL,
				PAGE_SIZE,
				src_addr_v,
				src_addr_p);
	if (dst_addr_v != NULL)
		dma_free_coherent(NULL,
				PAGE_SIZE,
				dst_addr_v,
				dst_addr_p);
	return n;
}

static ssize_t test_apmcu_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	i = snprintf(buf, PAGE_SIZE, "%d\n", times);
	return i;
}

static ssize_t test_apmcu_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	unsigned long	flags;
	int		i;

	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (sscanf(buf, "%d", &times) != 1)
		return -EINVAL;
	if (times < 0)
		return -EINVAL;

	if (times > 5000) //Less than 20MB
		return -EINVAL;

	/* dma_alloc */
	src_addr_v = dma_alloc_coherent(NULL,
					PAGE_SIZE,
					&src_addr_p,
					GFP_KERNEL);
	if (src_addr_v == NULL)
		return -ENOMEM;

	/* testing */
	local_irq_save(flags);
	met_tag_start(0, "TEST_EMI_APMCU");
	for (i=0; i<times; i++) {
		memset(src_addr_v, 2*i, PAGE_SIZE);
		met_tag_oneshot(0, "TEST_EMI_APMCU", PAGE_SIZE);
	}
	met_tag_end(0, "TEST_EMI_APMCU");
	local_irq_restore(flags);
	/* dma_free */
	if (src_addr_v != NULL)
		dma_free_coherent(NULL,
				PAGE_SIZE,
				src_addr_v,
				src_addr_p);
	return n;
}

static ssize_t ttype_enable_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	if (ttype_enable == BM_TTYPE_DISABLE)
		return snprintf(buf, PAGE_SIZE, "Disable\n");
	else
		return snprintf(buf, PAGE_SIZE, "Enable\n");
}

static ssize_t ttype_enable_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (!strncasecmp(buf, "Disable", strlen("Disable")))
		ttype_enable = BM_TTYPE_DISABLE;
	else if (!strncasecmp(buf, "Enable", strlen("Enable")))
		ttype_enable = BM_TTYPE_ENABLE;
	else
		return -EINVAL;
	return n;
}

static ssize_t bw_limiter_enable_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	if (bw_limiter_enable == BM_BW_LIMITER_DISABLE)
		return snprintf(buf, PAGE_SIZE, "Disable\n");
	else
		return snprintf(buf, PAGE_SIZE, "Enable\n");
}

static ssize_t bw_limiter_enable_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (!strncasecmp(buf, "Disable", strlen("Disable")))
		bw_limiter_enable = BM_BW_LIMITER_DISABLE;
	else if (!strncasecmp(buf, "Enable", strlen("Enable")))
		bw_limiter_enable = BM_BW_LIMITER_ENABLE;
	else
		return -EINVAL;
	return n;
}

static ssize_t ttype_master_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int	i;

	switch (ttype_master) {
	case BM_MASTER_AP_MCU:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "AP_MCU");
		break;
	case BM_MASTER_PRISYS:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "PRISYS");
		break;
	case BM_MASTER_CONNSYS:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "CONNSYS:");
		break;
	case BM_MASTER_MD_MCU:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "MD_MCU");
		break;
	case BM_MASTER_MD_HW:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "MD_HW");
		break;
	case BM_MASTER_MM:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "MM");
		break;
	case BM_MASTER_GPU:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "GPU");
		break;
	case BM_MASTER_MDLITE_MCU:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "MDLITE_MCU");
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%d\n", -1);
		break;
	}
	return i;
}

static ssize_t ttype_master_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (!strncasecmp(buf, "AP_MCU", strlen("AP_MCU")))
		ttype_master = BM_MASTER_AP_MCU;
	else if (!strncasecmp(buf, "PRISYS", strlen("PRISYS")))
		ttype_master = BM_MASTER_PRISYS;
	else if (!strncasecmp(buf, "CONNSYS", strlen("CONNSYS")))
		ttype_master = BM_MASTER_CONNSYS;
	else if (!strncasecmp(buf, "MD_MCU", strlen("MD_MCU")))
		ttype_master = BM_MASTER_MD_MCU;
	else if (!strncasecmp(buf, "MD_HW", strlen("MD_HW")))
		ttype_master = BM_MASTER_MD_HW;
	else if (!strncasecmp(buf, "MM", strlen("MM")))
		ttype_master = BM_MASTER_MM;
	else if (!strncasecmp(buf, "GPU", strlen("GPU")))
		ttype_master = BM_MASTER_GPU;
	else if (!strncasecmp(buf, "MDLITE_MCU", strlen("MDLITE_MCU")))
		ttype_master = BM_MASTER_MDLITE_MCU;
	else
		return -EINVAL;
	return n;
}

static ssize_t ttype_busid_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	i = snprintf(buf, PAGE_SIZE, "0x%08x\n", ttype_busid);
	return i;
}

static ssize_t ttype_busid_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (!strncmp(buf, "0x", strlen("0x"))) {
		if (sscanf(buf, "0x%x", &ttype_busid) != 1)
			return -EINVAL;
	} else {
		if (sscanf(buf, "%d", &ttype_busid) != 1)
			return -EINVAL;
	}
	return n;
}

static ssize_t ttype1_nbeat_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype1_nbeat) {
	case BM_TRANS_TYPE_1BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 1);
		break;
	case BM_TRANS_TYPE_2BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 2);
		break;
	case BM_TRANS_TYPE_3BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 3);
		break;
	case BM_TRANS_TYPE_4BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 4);
		break;
	case BM_TRANS_TYPE_5BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 5);
		break;
	case BM_TRANS_TYPE_6BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 6);
		break;
	case BM_TRANS_TYPE_7BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 7);
		break;
	case BM_TRANS_TYPE_8BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 8);
		break;
	case BM_TRANS_TYPE_9BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 9);
		break;
	case BM_TRANS_TYPE_10BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 10);
		break;
	case BM_TRANS_TYPE_11BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 11);
		break;
	case BM_TRANS_TYPE_12BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 12);
		break;
	case BM_TRANS_TYPE_13BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 13);
		break;
	case BM_TRANS_TYPE_14BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 14);
		break;
	case BM_TRANS_TYPE_15BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 15);
		break;
	case BM_TRANS_TYPE_16BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 16);
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%d\n", -1);
		break;
	}
	return i;
}

static ssize_t ttype1_nbeat_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	int value;
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
	/* 1Beat/2Beat/.../16Beat */
	if (value < 1 || value > 16)
		return -EINVAL;
	switch (value) {
	case 1:
		ttype1_nbeat = BM_TRANS_TYPE_1BEAT;
		break;
	case 2:
		ttype1_nbeat = BM_TRANS_TYPE_2BEAT;
		break;
	case 3:
		ttype1_nbeat = BM_TRANS_TYPE_3BEAT;
		break;
	case 4:
		ttype1_nbeat = BM_TRANS_TYPE_4BEAT;
		break;
	case 5:
		ttype1_nbeat = BM_TRANS_TYPE_5BEAT;
		break;
	case 6:
		ttype1_nbeat = BM_TRANS_TYPE_6BEAT;
		break;
	case 7:
		ttype1_nbeat = BM_TRANS_TYPE_7BEAT;
		break;
	case 8:
		ttype1_nbeat = BM_TRANS_TYPE_8BEAT;
		break;
	case 9:
		ttype1_nbeat = BM_TRANS_TYPE_9BEAT;
		break;
	case 10:
		ttype1_nbeat = BM_TRANS_TYPE_10BEAT;
		break;
	case 11:
		ttype1_nbeat = BM_TRANS_TYPE_11BEAT;
		break;
	case 12:
		ttype1_nbeat = BM_TRANS_TYPE_12BEAT;
		break;
	case 13:
		ttype1_nbeat = BM_TRANS_TYPE_13BEAT;
		break;
	case 14:
		ttype1_nbeat = BM_TRANS_TYPE_14BEAT;
		break;
	case 15:
		ttype1_nbeat = BM_TRANS_TYPE_15BEAT;
		break;
	case 16:
		ttype1_nbeat = BM_TRANS_TYPE_16BEAT;
		break;
	default:
		return -EINVAL;
	}
	return n;
}

static ssize_t ttype1_nbyte_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype1_nbyte) {
	case BM_TRANS_TYPE_1Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 1);
		break;
	case BM_TRANS_TYPE_2Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 2);
		break;
	case BM_TRANS_TYPE_4Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 4);
		break;
	case BM_TRANS_TYPE_8Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 8);
		break;
	case BM_TRANS_TYPE_16Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 16);
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%d\n", -1);
		break;
	}
	return i;
}

static ssize_t ttype1_nbyte_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	int value;
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
	/* 1Byte/2Byte/4Byte/8Byte/16Byte */
	switch (value) {
	case 1:
		ttype1_nbyte = BM_TRANS_TYPE_1Byte;
		break;
	case 2:
		ttype1_nbyte = BM_TRANS_TYPE_2Byte;
		break;
	case 4:
		ttype1_nbyte = BM_TRANS_TYPE_4Byte;
		break;
	case 8:
		ttype1_nbyte = BM_TRANS_TYPE_8Byte;
		break;
	case 16:
		ttype1_nbyte = BM_TRANS_TYPE_16Byte;
		break;
	default:
		return -EINVAL;
	}
	return n;
}

static ssize_t ttype1_burst_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype1_burst) {
	case BM_TRANS_TYPE_BURST_INCR:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "INCR");
		break;
	case BM_TRANS_TYPE_BURST_WRAP:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "WRAP");
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "ERR");
		break;
	}
	return i;
}

static ssize_t ttype1_burst_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (!strncasecmp(buf, "INCR", strlen("INCR")))
		ttype1_burst = BM_TRANS_TYPE_BURST_INCR;
	else if (!strncasecmp(buf, "WRAP", strlen("WRAP")))
		ttype1_burst = BM_TRANS_TYPE_BURST_WRAP;
	else
		return -EINVAL;
	return n;
}

static ssize_t ttype2_nbeat_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype2_nbeat) {
	case BM_TRANS_TYPE_1BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 1);
		break;
	case BM_TRANS_TYPE_2BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 2);
		break;
	case BM_TRANS_TYPE_3BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 3);
		break;
	case BM_TRANS_TYPE_4BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 4);
		break;
	case BM_TRANS_TYPE_5BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 5);
		break;
	case BM_TRANS_TYPE_6BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 6);
		break;
	case BM_TRANS_TYPE_7BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 7);
		break;
	case BM_TRANS_TYPE_8BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 8);
		break;
	case BM_TRANS_TYPE_9BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 9);
		break;
	case BM_TRANS_TYPE_10BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 10);
		break;
	case BM_TRANS_TYPE_11BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 11);
		break;
	case BM_TRANS_TYPE_12BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 12);
		break;
	case BM_TRANS_TYPE_13BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 13);
		break;
	case BM_TRANS_TYPE_14BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 14);
		break;
	case BM_TRANS_TYPE_15BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 15);
		break;
	case BM_TRANS_TYPE_16BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 16);
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%d\n", -1);
		break;
	}
	return i;
}

static ssize_t ttype2_nbeat_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	int value;
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
	/* 1Beat/2Beat/.../16Beat */
	if (value < 1 || value > 16)
		return -EINVAL;
	switch (value) {
	case 1:
		ttype2_nbeat = BM_TRANS_TYPE_1BEAT;
		break;
	case 2:
		ttype2_nbeat = BM_TRANS_TYPE_2BEAT;
		break;
	case 3:
		ttype2_nbeat = BM_TRANS_TYPE_3BEAT;
		break;
	case 4:
		ttype2_nbeat = BM_TRANS_TYPE_4BEAT;
		break;
	case 5:
		ttype2_nbeat = BM_TRANS_TYPE_5BEAT;
		break;
	case 6:
		ttype2_nbeat = BM_TRANS_TYPE_6BEAT;
		break;
	case 7:
		ttype2_nbeat = BM_TRANS_TYPE_7BEAT;
		break;
	case 8:
		ttype2_nbeat = BM_TRANS_TYPE_8BEAT;
		break;
	case 9:
		ttype2_nbeat = BM_TRANS_TYPE_9BEAT;
		break;
	case 10:
		ttype2_nbeat = BM_TRANS_TYPE_10BEAT;
		break;
	case 11:
		ttype2_nbeat = BM_TRANS_TYPE_11BEAT;
		break;
	case 12:
		ttype2_nbeat = BM_TRANS_TYPE_12BEAT;
		break;
	case 13:
		ttype2_nbeat = BM_TRANS_TYPE_13BEAT;
		break;
	case 14:
		ttype2_nbeat = BM_TRANS_TYPE_14BEAT;
		break;
	case 15:
		ttype2_nbeat = BM_TRANS_TYPE_15BEAT;
		break;
	case 16:
		ttype2_nbeat = BM_TRANS_TYPE_16BEAT;
		break;
	default:
		return -EINVAL;
	}
	return n;
}

static ssize_t ttype2_nbyte_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype2_nbyte) {
	case BM_TRANS_TYPE_1Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 1);
		break;
	case BM_TRANS_TYPE_2Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 2);
		break;
	case BM_TRANS_TYPE_4Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 4);
		break;
	case BM_TRANS_TYPE_8Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 8);
		break;
	case BM_TRANS_TYPE_16Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 16);
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%d\n", -1);
		break;
	}
	return i;
}

static ssize_t ttype2_nbyte_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	int value;
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
	/* 1Byte/2Byte/4Byte/8Byte/16Byte */
	switch (value) {
	case 1:
		ttype2_nbyte = BM_TRANS_TYPE_1Byte;
		break;
	case 2:
		ttype2_nbyte = BM_TRANS_TYPE_2Byte;
		break;
	case 4:
		ttype2_nbyte = BM_TRANS_TYPE_4Byte;
		break;
	case 8:
		ttype2_nbyte = BM_TRANS_TYPE_8Byte;
		break;
	case 16:
		ttype2_nbyte = BM_TRANS_TYPE_16Byte;
		break;
	default:
		return -EINVAL;
	}
	return n;
}

static ssize_t ttype2_burst_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype2_burst) {
	case BM_TRANS_TYPE_BURST_INCR:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "INCR");
		break;
	case BM_TRANS_TYPE_BURST_WRAP:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "WRAP");
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "ERR");
		break;
	}
	return i;
}

static ssize_t ttype2_burst_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (!strncasecmp(buf, "INCR", strlen("INCR")))
		ttype2_burst = BM_TRANS_TYPE_BURST_INCR;
	else if (!strncasecmp(buf, "WRAP", strlen("WRAP")))
		ttype2_burst = BM_TRANS_TYPE_BURST_WRAP;
	else
		return -EINVAL;
	return n;
}
static ssize_t ttype3_nbeat_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype3_nbeat) {
	case BM_TRANS_TYPE_1BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 1);
		break;
	case BM_TRANS_TYPE_2BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 2);
		break;
	case BM_TRANS_TYPE_3BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 3);
		break;
	case BM_TRANS_TYPE_4BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 4);
		break;
	case BM_TRANS_TYPE_5BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 5);
		break;
	case BM_TRANS_TYPE_6BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 6);
		break;
	case BM_TRANS_TYPE_7BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 7);
		break;
	case BM_TRANS_TYPE_8BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 8);
		break;
	case BM_TRANS_TYPE_9BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 9);
		break;
	case BM_TRANS_TYPE_10BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 10);
		break;
	case BM_TRANS_TYPE_11BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 11);
		break;
	case BM_TRANS_TYPE_12BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 12);
		break;
	case BM_TRANS_TYPE_13BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 13);
		break;
	case BM_TRANS_TYPE_14BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 14);
		break;
	case BM_TRANS_TYPE_15BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 15);
		break;
	case BM_TRANS_TYPE_16BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 16);
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%d\n", -1);
		break;
	}
	return i;
}

static ssize_t ttype3_nbeat_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	int value;
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
	/* 1Beat/2Beat/.../16Beat */
	if (value < 1 || value > 16)
		return -EINVAL;
	switch (value) {
	case 1:
		ttype3_nbeat = BM_TRANS_TYPE_1BEAT;
		break;
	case 2:
		ttype3_nbeat = BM_TRANS_TYPE_2BEAT;
		break;
	case 3:
		ttype3_nbeat = BM_TRANS_TYPE_3BEAT;
		break;
	case 4:
		ttype3_nbeat = BM_TRANS_TYPE_4BEAT;
		break;
	case 5:
		ttype3_nbeat = BM_TRANS_TYPE_5BEAT;
		break;
	case 6:
		ttype3_nbeat = BM_TRANS_TYPE_6BEAT;
		break;
	case 7:
		ttype3_nbeat = BM_TRANS_TYPE_7BEAT;
		break;
	case 8:
		ttype3_nbeat = BM_TRANS_TYPE_8BEAT;
		break;
	case 9:
		ttype3_nbeat = BM_TRANS_TYPE_9BEAT;
		break;
	case 10:
		ttype3_nbeat = BM_TRANS_TYPE_10BEAT;
		break;
	case 11:
		ttype3_nbeat = BM_TRANS_TYPE_11BEAT;
		break;
	case 12:
		ttype3_nbeat = BM_TRANS_TYPE_12BEAT;
		break;
	case 13:
		ttype3_nbeat = BM_TRANS_TYPE_13BEAT;
		break;
	case 14:
		ttype3_nbeat = BM_TRANS_TYPE_14BEAT;
		break;
	case 15:
		ttype3_nbeat = BM_TRANS_TYPE_15BEAT;
		break;
	case 16:
		ttype3_nbeat = BM_TRANS_TYPE_16BEAT;
		break;
	default:
		return -EINVAL;
	}
	return n;
}

static ssize_t ttype3_nbyte_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype3_nbyte) {
	case BM_TRANS_TYPE_1Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 1);
		break;
	case BM_TRANS_TYPE_2Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 2);
		break;
	case BM_TRANS_TYPE_4Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 4);
		break;
	case BM_TRANS_TYPE_8Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 8);
		break;
	case BM_TRANS_TYPE_16Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 16);
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%d\n", -1);
		break;
	}
	return i;
}

static ssize_t ttype3_nbyte_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	int value;
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
	/* 1Byte/2Byte/4Byte/8Byte/16Byte */
	switch (value) {
	case 1:
		ttype3_nbyte = BM_TRANS_TYPE_1Byte;
		break;
	case 2:
		ttype3_nbyte = BM_TRANS_TYPE_2Byte;
		break;
	case 4:
		ttype3_nbyte = BM_TRANS_TYPE_4Byte;
		break;
	case 8:
		ttype3_nbyte = BM_TRANS_TYPE_8Byte;
		break;
	case 16:
		ttype3_nbyte = BM_TRANS_TYPE_16Byte;
		break;
	default:
		return -EINVAL;
	}
	return n;
}

static ssize_t ttype3_burst_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype3_burst) {
	case BM_TRANS_TYPE_BURST_INCR:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "INCR");
		break;
	case BM_TRANS_TYPE_BURST_WRAP:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "WRAP");
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "ERR");
		break;
	}
	return i;
}

static ssize_t ttype3_burst_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (!strncasecmp(buf, "INCR", strlen("INCR")))
		ttype3_burst = BM_TRANS_TYPE_BURST_INCR;
	else if (!strncasecmp(buf, "WRAP", strlen("WRAP")))
		ttype3_burst = BM_TRANS_TYPE_BURST_WRAP;
	else
		return -EINVAL;
	return n;
}
static ssize_t ttype4_nbeat_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype4_nbeat) {
	case BM_TRANS_TYPE_1BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 1);
		break;
	case BM_TRANS_TYPE_2BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 2);
		break;
	case BM_TRANS_TYPE_3BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 3);
		break;
	case BM_TRANS_TYPE_4BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 4);
		break;
	case BM_TRANS_TYPE_5BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 5);
		break;
	case BM_TRANS_TYPE_6BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 6);
		break;
	case BM_TRANS_TYPE_7BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 7);
		break;
	case BM_TRANS_TYPE_8BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 8);
		break;
	case BM_TRANS_TYPE_9BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 9);
		break;
	case BM_TRANS_TYPE_10BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 10);
		break;
	case BM_TRANS_TYPE_11BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 11);
		break;
	case BM_TRANS_TYPE_12BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 12);
		break;
	case BM_TRANS_TYPE_13BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 13);
		break;
	case BM_TRANS_TYPE_14BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 14);
		break;
	case BM_TRANS_TYPE_15BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 15);
		break;
	case BM_TRANS_TYPE_16BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 16);
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%d\n", -1);
		break;
	}
	return i;
}

static ssize_t ttype4_nbeat_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	int value;
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
	/* 1Beat/2Beat/.../16Beat */
	if (value < 1 || value > 16)
		return -EINVAL;
	switch (value) {
	case 1:
		ttype4_nbeat = BM_TRANS_TYPE_1BEAT;
		break;
	case 2:
		ttype4_nbeat = BM_TRANS_TYPE_2BEAT;
		break;
	case 3:
		ttype4_nbeat = BM_TRANS_TYPE_3BEAT;
		break;
	case 4:
		ttype4_nbeat = BM_TRANS_TYPE_4BEAT;
		break;
	case 5:
		ttype4_nbeat = BM_TRANS_TYPE_5BEAT;
		break;
	case 6:
		ttype4_nbeat = BM_TRANS_TYPE_6BEAT;
		break;
	case 7:
		ttype4_nbeat = BM_TRANS_TYPE_7BEAT;
		break;
	case 8:
		ttype4_nbeat = BM_TRANS_TYPE_8BEAT;
		break;
	case 9:
		ttype4_nbeat = BM_TRANS_TYPE_9BEAT;
		break;
	case 10:
		ttype4_nbeat = BM_TRANS_TYPE_10BEAT;
		break;
	case 11:
		ttype4_nbeat = BM_TRANS_TYPE_11BEAT;
		break;
	case 12:
		ttype4_nbeat = BM_TRANS_TYPE_12BEAT;
		break;
	case 13:
		ttype4_nbeat = BM_TRANS_TYPE_13BEAT;
		break;
	case 14:
		ttype4_nbeat = BM_TRANS_TYPE_14BEAT;
		break;
	case 15:
		ttype4_nbeat = BM_TRANS_TYPE_15BEAT;
		break;
	case 16:
		ttype4_nbeat = BM_TRANS_TYPE_16BEAT;
		break;
	default:
		return -EINVAL;
	}
	return n;
}

static ssize_t ttype4_nbyte_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype4_nbyte) {
	case BM_TRANS_TYPE_1Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 1);
		break;
	case BM_TRANS_TYPE_2Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 2);
		break;
	case BM_TRANS_TYPE_4Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 4);
		break;
	case BM_TRANS_TYPE_8Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 8);
		break;
	case BM_TRANS_TYPE_16Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 16);
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%d\n", -1);
		break;
	}
	return i;
}

static ssize_t ttype4_nbyte_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	int value;
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
	/* 1Byte/2Byte/4Byte/8Byte/16Byte */
	switch (value) {
	case 1:
		ttype4_nbyte = BM_TRANS_TYPE_1Byte;
		break;
	case 2:
		ttype4_nbyte = BM_TRANS_TYPE_2Byte;
		break;
	case 4:
		ttype4_nbyte = BM_TRANS_TYPE_4Byte;
		break;
	case 8:
		ttype4_nbyte = BM_TRANS_TYPE_8Byte;
		break;
	case 16:
		ttype4_nbyte = BM_TRANS_TYPE_16Byte;
		break;
	default:
		return -EINVAL;
	}
	return n;
}

static ssize_t ttype4_burst_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype4_burst) {
	case BM_TRANS_TYPE_BURST_INCR:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "INCR");
		break;
	case BM_TRANS_TYPE_BURST_WRAP:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "WRAP");
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "ERR");
		break;
	}
	return i;
}

static ssize_t ttype4_burst_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (!strncasecmp(buf, "INCR", strlen("INCR")))
		ttype4_burst = BM_TRANS_TYPE_BURST_INCR;
	else if (!strncasecmp(buf, "WRAP", strlen("WRAP")))
		ttype4_burst = BM_TRANS_TYPE_BURST_WRAP;
	else
		return -EINVAL;
	return n;
}
static ssize_t ttype5_nbeat_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype5_nbeat) {
	case BM_TRANS_TYPE_1BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 1);
		break;
	case BM_TRANS_TYPE_2BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 2);
		break;
	case BM_TRANS_TYPE_3BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 3);
		break;
	case BM_TRANS_TYPE_4BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 4);
		break;
	case BM_TRANS_TYPE_5BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 5);
		break;
	case BM_TRANS_TYPE_6BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 6);
		break;
	case BM_TRANS_TYPE_7BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 7);
		break;
	case BM_TRANS_TYPE_8BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 8);
		break;
	case BM_TRANS_TYPE_9BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 9);
		break;
	case BM_TRANS_TYPE_10BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 10);
		break;
	case BM_TRANS_TYPE_11BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 11);
		break;
	case BM_TRANS_TYPE_12BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 12);
		break;
	case BM_TRANS_TYPE_13BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 13);
		break;
	case BM_TRANS_TYPE_14BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 14);
		break;
	case BM_TRANS_TYPE_15BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 15);
		break;
	case BM_TRANS_TYPE_16BEAT:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 16);
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%d\n", -1);
		break;
	}
	return i;
}

static ssize_t ttype5_nbeat_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	int value;
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
	/* 1Beat/2Beat/.../16Beat */
	if (value < 1 || value > 16)
		return -EINVAL;
	switch (value) {
	case 1:
		ttype5_nbeat = BM_TRANS_TYPE_1BEAT;
		break;
	case 2:
		ttype5_nbeat = BM_TRANS_TYPE_2BEAT;
		break;
	case 3:
		ttype5_nbeat = BM_TRANS_TYPE_3BEAT;
		break;
	case 4:
		ttype5_nbeat = BM_TRANS_TYPE_4BEAT;
		break;
	case 5:
		ttype5_nbeat = BM_TRANS_TYPE_5BEAT;
		break;
	case 6:
		ttype5_nbeat = BM_TRANS_TYPE_6BEAT;
		break;
	case 7:
		ttype5_nbeat = BM_TRANS_TYPE_7BEAT;
		break;
	case 8:
		ttype5_nbeat = BM_TRANS_TYPE_8BEAT;
		break;
	case 9:
		ttype5_nbeat = BM_TRANS_TYPE_9BEAT;
		break;
	case 10:
		ttype5_nbeat = BM_TRANS_TYPE_10BEAT;
		break;
	case 11:
		ttype5_nbeat = BM_TRANS_TYPE_11BEAT;
		break;
	case 12:
		ttype5_nbeat = BM_TRANS_TYPE_12BEAT;
		break;
	case 13:
		ttype5_nbeat = BM_TRANS_TYPE_13BEAT;
		break;
	case 14:
		ttype5_nbeat = BM_TRANS_TYPE_14BEAT;
		break;
	case 15:
		ttype5_nbeat = BM_TRANS_TYPE_15BEAT;
		break;
	case 16:
		ttype5_nbeat = BM_TRANS_TYPE_16BEAT;
		break;
	default:
		return -EINVAL;
	}
	return n;
}

static ssize_t ttype5_nbyte_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype5_nbyte) {
	case BM_TRANS_TYPE_1Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 1);
		break;
	case BM_TRANS_TYPE_2Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 2);
		break;
	case BM_TRANS_TYPE_4Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 4);
		break;
	case BM_TRANS_TYPE_8Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 8);
		break;
	case BM_TRANS_TYPE_16Byte:
		i = snprintf(buf, PAGE_SIZE, "%d\n", 16);
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%d\n", -1);
		break;
	}
	return i;
}

static ssize_t ttype5_nbyte_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	int value;
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
	/* 1Byte/2Byte/4Byte/8Byte/16Byte */
	switch (value) {
	case 1:
		ttype5_nbyte = BM_TRANS_TYPE_1Byte;
		break;
	case 2:
		ttype5_nbyte = BM_TRANS_TYPE_2Byte;
		break;
	case 4:
		ttype5_nbyte = BM_TRANS_TYPE_4Byte;
		break;
	case 8:
		ttype5_nbyte = BM_TRANS_TYPE_8Byte;
		break;
	case 16:
		ttype5_nbyte = BM_TRANS_TYPE_16Byte;
		break;
	default:
		return -EINVAL;
	}
	return n;
}

static ssize_t ttype5_burst_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (ttype5_burst) {
	case BM_TRANS_TYPE_BURST_INCR:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "INCR");
		break;
	case BM_TRANS_TYPE_BURST_WRAP:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "WRAP");
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "ERR");
		break;
	}
	return i;
}

static ssize_t ttype5_burst_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (!strncasecmp(buf, "INCR", strlen("INCR")))
		ttype5_burst = BM_TRANS_TYPE_BURST_INCR;
	else if (!strncasecmp(buf, "WRAP", strlen("WRAP")))
		ttype5_burst = BM_TRANS_TYPE_BURST_WRAP;
	else
		return -EINVAL;
	return n;
}

static void emi_init(void)
{
	/* Init. EMI bus monitor */
	MET_BM_SetReadWriteType(rwtype);

	// ALL
	MET_BM_SetMonitorCounter(1,
		BM_MASTER_ALL,
		BM_TRANS_TYPE_4BEAT |
		BM_TRANS_TYPE_8Byte |
		BM_TRANS_TYPE_BURST_WRAP);
	// MM
	MET_BM_SetMonitorCounter(2,
		BM_MASTER_MM,
		BM_TRANS_TYPE_4BEAT |
		BM_TRANS_TYPE_8Byte |
		BM_TRANS_TYPE_BURST_WRAP);
	// MD
	MET_BM_SetMonitorCounter(3,
		BM_MASTER_PRISYS |
		BM_MASTER_CONNSYS |
		BM_MASTER_MD_MCU |
		BM_MASTER_MD_HW |
		BM_MASTER_MDLITE_MCU,
		BM_TRANS_TYPE_4BEAT |
		BM_TRANS_TYPE_8Byte |
		BM_TRANS_TYPE_BURST_WRAP);
	// GPU
	MET_BM_SetMonitorCounter(4,
		BM_MASTER_GPU,
		BM_TRANS_TYPE_4BEAT |
		BM_TRANS_TYPE_8Byte |
		BM_TRANS_TYPE_BURST_WRAP);

	MET_BM_SetLatencyCounter();

	/* TTYPE17~21 */
	if (ttype_enable == BM_TTYPE_ENABLE) {
		/* TTYPE17 */
		MET_BM_SetMonitorCounter(17,
			ttype_master,
			ttype1_nbeat |
			ttype1_nbyte |
			ttype1_burst);
		MET_BM_SetIDSelect(17, ttype_busid, 1);

		/* TTYPE18 */
		MET_BM_SetMonitorCounter(18,
			ttype_master,
			ttype2_nbeat |
			ttype2_nbyte |
			ttype2_burst);
		MET_BM_SetIDSelect(18, ttype_busid, 1);

		/* TTYPE19 */
		MET_BM_SetMonitorCounter(19,
			ttype_master,
			ttype3_nbeat |
			ttype3_nbyte |
			ttype3_burst);
		MET_BM_SetIDSelect(19, ttype_busid, 1);

		/* TTYPE20 */
		MET_BM_SetMonitorCounter(20,
			ttype_master,
			ttype4_nbeat |
			ttype4_nbyte |
			ttype4_burst);
		MET_BM_SetIDSelect(20, ttype_busid, 1);

		/* TTYPE21 */
		MET_BM_SetMonitorCounter(21,
			ttype_master,
			ttype5_nbeat |
			ttype5_nbyte |
			ttype5_burst);
		MET_BM_SetIDSelect(21, ttype_busid, 1);
	}
#ifdef MET_EMI_DEBUG
		printk("===EMI: CONA[0x%x]\n",
			MET_EMI_GetConA());
		printk("===EMI: BMEN[0x%x]\n",
			MET_EMI_GetBMEN());
		printk("===EMI: BMEN2[0x%x]\n",
			MET_EMI_GetBMEN2());
		printk("===EMI: MSEL[0x%x]\n",
			MET_EMI_GetMSEL());
		printk("===EMI: MSEL2[0x%x]\n",
			MET_EMI_GetMSEL2());
		printk("===EMI: MSEL3[0x%x]\n",
			MET_EMI_GetMSEL3());
		printk("===EMI: MSEL4[0x%x]\n",
			MET_EMI_GetMSEL4());
		printk("===EMI: MSEL5[0x%x]\n",
			MET_EMI_GetMSEL5());
		printk("===EMI: MSEL6[0x%x]\n",
			MET_EMI_GetMSEL6());
		printk("===EMI: MSEL7[0x%x]\n",
			MET_EMI_GetMSEL7());
		printk("===EMI: MSEL8[0x%x]\n",
			MET_EMI_GetMSEL8());
		printk("===EMI: MSEL9[0x%x]\n",
			MET_EMI_GetMSEL9());
		printk("===EMI: MSEL10[0x%x]\n",
			MET_EMI_GetMSEL10());
		printk("===EMI: BMID0[0x%x]\n",
			MET_EMI_GetBMID0());
		printk("===EMI: BMID1[0x%x]\n",
			MET_EMI_GetBMID1());
		printk("===EMI: BMID2[0x%x]\n",
			MET_EMI_GetBMID2());
		printk("===EMI: BMID3[0x%x]\n",
			MET_EMI_GetBMID3());
		printk("===EMI: BMID4[0x%x]\n",
			MET_EMI_GetBMID4());
		printk("===EMI: BMID5[0x%x]\n",
			MET_EMI_GetBMID5());
		printk("===EMI: BMID6[0x%x]\n",
			MET_EMI_GetBMID6());
		printk("===EMI: BMID7[0x%x]\n",
			MET_EMI_GetBMID7());
		printk("===EMI: BMID8[0x%x]\n",
			MET_EMI_GetBMID8());
		printk("===EMI: BMID9[0x%x]\n",
			MET_EMI_GetBMID9());
		printk("===EMI: BMID10[0x%x]\n",
			MET_EMI_GetBMID10());
#endif
}

static void emi_start(void)
{
	MET_BM_Enable(1);
}

static void emi_stop(void)
{
	MET_BM_Enable(0);
}

static int do_emi(void)
{
	return met_emi.mode;
}

static unsigned int emi_bw_limiter(unsigned int *array)
{
	int idx = -1;

	// Clk + ARB*
	array[++idx] = mt_get_emi_freq();
        //printk("===freq=%d\n", array[idx]);
	array[++idx] = MET_EMI_GetARBA();
        //printk("===arba=%x\n", array[idx]);
	array[++idx] = MET_EMI_GetARBB();
        //printk("===arbb=%x\n", array[idx]);
	array[++idx] = MET_EMI_GetARBC();
        //printk("===arbc=%x\n", array[idx]);
	array[++idx] = MET_EMI_GetARBD();
        //printk("===arbd=%x\n", array[idx]);
	array[++idx] = MET_EMI_GetARBE();
        //printk("===arbe=%x\n", array[idx]);
	array[++idx] = MET_EMI_GetARBF();
        //printk("===arbf=%x\n", array[idx]);
	array[++idx] = MET_EMI_GetARBG();
        //printk("===arbg=%x\n", array[idx]);
	array[++idx] = MET_EMI_GetARBH();
        //printk("===arbh=%x\n", array[idx]);

	return idx + 1;
}

static unsigned int emi_polling(unsigned int *emi_value)
{
	int i;
	int j=-1;

	MET_BM_Pause();

	//Get Bus Cycle Count
	emi_value[++j] = MET_BM_GetBusCycCount();
	//Get Word Count
	for (i=0; i<4; i++) {
		emi_value[++j] = MET_BM_GetWordCount(i+1);
	}

	emi_value[++j] = MET_BM_GetLatencyCycle(1);
	emi_value[++j] = MET_BM_GetLatencyCycle(2);
	emi_value[++j] = MET_BM_GetLatencyCycle(3);
	emi_value[++j] = MET_BM_GetLatencyCycle(4);
	emi_value[++j] = MET_BM_GetLatencyCycle(5);
	emi_value[++j] = MET_BM_GetLatencyCycle(6);
	emi_value[++j] = MET_BM_GetLatencyCycle(7);
	emi_value[++j] = MET_BM_GetLatencyCycle(8);

	emi_value[++j] = MET_BM_GetLatencyCycle(9);
	emi_value[++j] = MET_BM_GetLatencyCycle(10);
	emi_value[++j] = MET_BM_GetLatencyCycle(11);
	emi_value[++j] = MET_BM_GetLatencyCycle(12);
	emi_value[++j] = MET_BM_GetLatencyCycle(13);
	emi_value[++j] = MET_BM_GetLatencyCycle(14);
	emi_value[++j] = MET_BM_GetLatencyCycle(15);
	emi_value[++j] = MET_BM_GetLatencyCycle(16);

	// Get BACT/BSCT/BCNT
	emi_value[++j] = MET_BM_GetBandwidthWordCount();
	emi_value[++j] = MET_BM_GetOverheadWordCount();
	emi_value[++j] = MET_BM_GetBusCycCount();

	// Get PageHist/PageMiss/InterBank/Idle
	emi_value[++j] = MET_DRAMC_GetPageHitCount(DRAMC_ALL);
	emi_value[++j] = MET_DRAMC_GetPageMissCount(DRAMC_ALL);
	emi_value[++j] = MET_DRAMC_GetInterbankCount(DRAMC_ALL);
	emi_value[++j] = MET_DRAMC_GetIdleCount();

	// TTYPE
	emi_value[++j] = MET_BM_GetLatencyCycle(17);
	emi_value[++j] = MET_BM_GetLatencyCycle(18);
	emi_value[++j] = MET_BM_GetLatencyCycle(19);
	emi_value[++j] = MET_BM_GetLatencyCycle(20);
	emi_value[++j] = MET_BM_GetLatencyCycle(21);

	MET_BM_Enable(0);
	MET_BM_Enable(1);

	return j+1;
}

static void emi_uninit(void)
{
}

static int met_emi_create(struct kobject *parent)
{
	int ret = 0;

	ret = MET_BM_Init();
	if (ret != 0) {
		pr_err("MET_BM_Init failed!!!\n");
		return ret;
	}

	kobj_emi = parent;

	ret = sysfs_create_file(kobj_emi, &rwtype_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create rwtype in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &emi_clock_rate_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create emi_clock_rate in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &emi_dcm_ctrl_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create emi_dcm_ctrl in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &test_cqdma_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create test_cqdma in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &test_apmcu_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create test_apmcu in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype_enable_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype_enable in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &bw_limiter_enable_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create bw_limiter_enable in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype_master_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype_master in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype_busid_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype_busid in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype1_nbeat_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype1_nbeat in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype1_nbyte_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype1_nbyte in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype1_burst_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype1_burst in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype2_nbeat_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype2_nbeat in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype2_nbyte_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype2_nbyte in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype2_burst_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype2_burst in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype3_nbeat_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype3_nbeat in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype3_nbyte_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype3_nbyte in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype3_burst_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype3_burst in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype4_nbeat_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype4_nbeat in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype4_nbyte_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype4_nbyte in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype4_burst_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype4_burst in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype5_nbeat_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype5_nbeat in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype5_nbyte_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype5_nbyte in sysfs\n");
		return ret;
	}
	ret = sysfs_create_file(kobj_emi, &ttype5_burst_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create ttype5_burst in sysfs\n");
		return ret;
	}

	return ret;
}


static void met_emi_delete(void)
{
	sysfs_remove_file(kobj_emi, &rwtype_attr.attr);
	sysfs_remove_file(kobj_emi, &emi_clock_rate_attr.attr);
	sysfs_remove_file(kobj_emi, &emi_dcm_ctrl_attr.attr);
	sysfs_remove_file(kobj_emi, &test_cqdma_attr.attr);
	sysfs_remove_file(kobj_emi, &test_apmcu_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype_enable_attr.attr);
	sysfs_remove_file(kobj_emi, &bw_limiter_enable_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype_master_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype_busid_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype1_nbeat_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype1_nbyte_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype1_burst_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype2_nbeat_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype2_nbyte_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype2_burst_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype3_nbeat_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype3_nbyte_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype3_burst_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype4_nbeat_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype4_nbyte_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype4_burst_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype5_nbeat_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype5_nbyte_attr.attr);
	sysfs_remove_file(kobj_emi, &ttype5_burst_attr.attr);
	kobj_emi = NULL;
	MET_BM_DeInit();
}

static void met_emi_start(void)
{
	/* Initial */
	is_first = 1;
	countdown = 1000;

	if (do_emi()) {
		emi_init();
		emi_stop();
		emi_start();
	}
}

static void met_emi_stop(void)
{
	if (do_emi()) {
		emi_stop();
		emi_uninit();
	}

}

static int emi_show_bw_limiter(void)
{
	unsigned int count;
	unsigned int bw_limiter[NIDX_BL];

	if (do_emi() && (bw_limiter_enable == BM_BW_LIMITER_ENABLE)) {
		count = emi_bw_limiter(bw_limiter);
		if (count)
			ms_bw_limiter(0, NIDX_BL, bw_limiter);
	}

	return 0;
}

static void met_emi_polling(unsigned long long stamp, int cpu)
{
	unsigned char count;
	unsigned int emi_value[NIDX];
	unsigned int bw_limiter[NIDX_BL];

	if (do_emi()) {
		count = emi_polling(emi_value);
		if (count) {
			emi_value[1] = emi_value[1] -
				       emi_value[2] -
				       emi_value[3] -
				       emi_value[4];
			ms_emi(count-NTTYPE, emi_value);
			if (ttype_enable == BM_TTYPE_ENABLE)
				ms_ttype(NTTYPE, emi_value+NIDX-NTTYPE);
			/* BW Limiter */
			if ((bw_limiter_enable == BM_BW_LIMITER_ENABLE) &&
				((is_first) || !(countdown))) {
				is_first = 0;
				count = emi_bw_limiter(bw_limiter);
				if (count)
					ms_bw_limiter(stamp,
							NIDX_BL,
							bw_limiter);
				/* reg. callback function */
				met_reg_bw_limiter(emi_show_bw_limiter);
			}
			/* 1000 times of sampling rate */
			// TODO: need to re-check
			if (countdown > 0)
				countdown--;
			else
				countdown = 1000;
			/* testing */
			//met_show_bw_limiter();
		}
	}
}

static char help[] = "  --emi                                 monitor EMI banwidth\n";
static char header[] =
"met-info [000] 0.0: met_emi_clockrate: %d000\n"
"# ms_emi: BUS_CYCLE,"
"APMCU_WSCT,MM_WSCT,MD_WSCT,GPU_WSCT,"
"APMCU_LATENCY,PRISYS_LATENCY,CONNSYS_LATENCY,MDMCU_LATENCY,"
"MDHW_LATENCY,MM_LATENCY,GPU_LATENCY,MDLITEMCU_LATENCY,"
"APMCU_TRANS,PRISYS_TRANS,CONNSYS_TRANS,MDMCU_TRANS,"
"MDHW_TRANS,MM_TRANS,GPU_TRANS,MDLITEMCU_TRANS,"
"BACT,BSCT,BCNT,"
"PageHit,PageMiss,InterBank,Idle\n"
"met-info [000] 0.0: met_emi_header: BUS_CYCLE,"
"APMCU_WSCT,MM_WSCT,MD_WSCT,GPU_WSCT,"
"APMCU_LATENCY,PRISYS_LATENCY,CONNSYS_LATENCY,MDMCU_LATENCY,"
"MDHW_LATENCY,MM_LATENCY,GPU_LATENCY,MDLITEMCU_LATENCY,"
"APMCU_TRANS,PRISYS_TRANS,CONNSYS_TRANS,MDMCU_TRANS,"
"MDHW_TRANS,MM_TRANS,GPU_TRANS,MDLITEMCU_TRANS,"
"BACT,BSCT,BCNT,"
"PageHit,PageMiss,InterBank,Idle\n";
static char header_ttype[] =
"met-info [000] 0.0: ms_ud_sys_header: ms_ttype,"
"ttyp1,ttyp2,ttyp3,ttyp4,ttyp5,"
"x,x,x,x,x\n";
static char header_bw_limiter[] =
"met-info [000] 0.0: met_bw_limiter_header: CLK,"
"ARBA,ARBB,ARBC,ARBD,ARBE,ARBF,ARBG,ARBH\n";

static int emi_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help);
}

static int emi_print_header(char *buf, int len)
{
	int ret;
	ret = snprintf(buf, PAGE_SIZE, header, mt_get_emi_freq());
	if (ttype_enable == BM_TTYPE_ENABLE)
		ret += snprintf(buf, PAGE_SIZE, "%s%s", buf, header_ttype);
	if (bw_limiter_enable == BM_BW_LIMITER_ENABLE)
		ret += snprintf(buf, PAGE_SIZE, "%s%s", buf, header_bw_limiter);

	return ret;
}

struct metdevice met_emi = {
	.name = "emi",
	.owner = THIS_MODULE,
	.type = MET_TYPE_BUS,
	.create_subfs = met_emi_create,
	.delete_subfs = met_emi_delete,
	.cpu_related = 0,
	.start = met_emi_start,
	.stop = met_emi_stop,
	.timed_polling = met_emi_polling,
	.print_help = emi_print_help,
	.print_header = emi_print_header,
};

#include <linux/platform_device.h>
#include "../dsp_buffer.h"
#include "dsp_reg_odin.h"
#include "dsp_crg_reg_odin.h"


extern int dsp_odin_register_device(struct platform_device *pdev);
extern void dsp_odin_unregister_device(void);

#if !defined(WITH_BINARY)
extern int dsp_odin_download_dram(void *firmwareBuffer);
extern int dsp_odin_download_iram(void *firmwareBuffer);
#else
extern int dsp_odin_download_dram(void);
extern int dsp_odin_download_iram(void);
#endif

extern int dsp_odin_power_on(void);
extern void dsp_odin_power_off(void);

extern irqreturn_t dsp_odin_interrupt(int irq, void *data);

typedef struct DSP_MODE {
	unsigned int is_opened_lpa;
	unsigned int is_opened_pcm;
	unsigned int is_opened_omx;
	unsigned int is_opened_alsa;
} DSP_MODE;

typedef struct WORKQUEUE_FLAG {
	unsigned int lpa_workqueue_flag;
	unsigned int pcm_workqueue_flag;
	unsigned int omx_workqueue_flag;
	unsigned int alsa_workqueue_flag;
	spinlock_t flag_lock;
} WORKQUEUE_FLAG;

enum dsp_odin_mem_resources {
	ODIN_DSP_IOMEM_BASE,
	ODIN_DSP_IOMEM_AUD_SRAM,
	ODIN_DSP_IOMEM_AUD_DRAM0,
	ODIN_DSP_IOMEM_AUD_DRAM1,
	ODIN_DSP_IOMEM_AUD_IRAM0,
	ODIN_DSP_IOMEM_AUD_IRAM1,
	ODIN_DSP_IOMEM_AUD_MAILBOX,
	ODIN_DSP_IOMEM_AUD_INTERRUPT,
	ODIN_DSP_IOMEM_AUD_CTR,
};

typedef struct DSP_ODIN_MEM_CTL {
	unsigned int *dsp_control_register;
	unsigned int dsp_control_register_size;
	unsigned int *dsp_dram0_address;
	unsigned int dsp_dram0_size;
	unsigned int *dsp_dram1_address;
	unsigned int dsp_dram1_size;
	unsigned int *dsp_iram0_address;
	unsigned int dsp_iram0_size;
	unsigned int *dsp_iram1_address;
	unsigned int dsp_iram1_size;
} DSP_ODIN_MEM_CTL;

typedef struct
DSP_WAIT {
	wait_queue_head_t wait;
	spinlock_t dsp_lock;
}DSP_WAIT;

extern DSP_MODE *dsp_mode;

extern DSP_ODIN_MEM_CTL *dsp_odin_mem_ctl;

extern WORKQUEUE_FLAG workqueue_flag;

extern DSP_WAIT *dsp_wait;

extern unsigned int kthread_mailbox_flag;

extern unsigned int received_transaction_number;


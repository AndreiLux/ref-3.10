#include <linux/platform_device.h>
#include "../dsp_buffer.h"
#include "dsp_reg_woden.h"

extern int dsp_woden_register_device(struct platform_device *pdev);
extern void dsp_woden_unregister_device(void);
extern int dsp_woden_download_dram(void *firmwareBuffer);
extern int dsp_woden_download_iram(void *firmwareBuffer);
extern int dsp_woden_power_on(void);
extern void dsp_woden_power_off(void);

extern irqreturn_t dsp_woden_interrupt(int irq, void *data);

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

enum dsp_woden_mem_resources {
	WODEN_DSP_IOMEM_BASE,
	WODEN_DSP_IOMEM_AUD_SRAM,
	WODEN_DSP_IOMEM_AUD_DRAM0,
	WODEN_DSP_IOMEM_AUD_DRAM1,
	WODEN_DSP_IOMEM_AUD_IRAM0,
	WODEN_DSP_IOMEM_AUD_IRAM1,
	WODEN_DSP_IOMEM_AUD_MAILBOX,
	WODEN_DSP_IOMEM_AUD_INTERRUPT,
	WODEN_DSP_IOMEM_AUD_CTR,
};

typedef struct DSP_WODEN_MEM_CTL {
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
} DSP_WODEN_MEM_CTL;

typedef struct
DSP_WAIT {
	wait_queue_head_t wait;
	spinlock_t dsp_lock;
}DSP_WAIT;

extern AUD_CONTROL_WODEN *aud_control_woden;

extern DSP_MODE *dsp_mode;

extern DSP_WODEN_MEM_CTL *dsp_woden_mem_ctl;

extern WORKQUEUE_FLAG workqueue_flag;

extern DSP_WAIT *dsp_wait;

extern unsigned int kthread_mailbox_flag;

extern unsigned int received_transaction_number;

extern struct file_operations gst_dsp_omx_fops;

typedef struct
OMX_AUDIO_INFO {
	unsigned int sampling_freq;
	unsigned int bit_per_sample;
	unsigned int channel_number;
	unsigned int bit_rate;
	unsigned int codec_type;
} OMX_AUDIO_INFO;

typedef struct
{
	unsigned int	cpb1_address;
	unsigned int	cpb1_size;
	unsigned int	cpb2_address;
	unsigned int	cpb2_size;
	unsigned int	dpb1_address;
	unsigned int	dpb1_size;
	unsigned int	dpb2_address;
	unsigned int	dpb2_size;
}
DSP_OMX_MEM_CFG;

extern struct workqueue_struct *omx_workqueue;
extern struct work_struct omx_work;

extern atomic_t received_mailbox_flag;

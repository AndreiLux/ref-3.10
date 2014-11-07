extern struct file_operations gst_dsp_pcm_fops;

typedef struct
EVENT_PCM
{
	unsigned int pcm_readqueue_count;
	unsigned int command;
	unsigned int type;
} EVENT_PCM;

extern struct workqueue_struct *pcm_workqueue;
extern struct work_struct pcm_work;

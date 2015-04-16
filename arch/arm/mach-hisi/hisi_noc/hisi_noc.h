#ifndef __HISI_NOC_H
#define __HISI_NOC_H

#define MAX_NOC_NODE_NAME_LEN	20
#define MAX_NOC_BUSES_NR	3

/* NoC IRQs */
#define NOC_MAX_IRQ_NR		33

/* NoC IRQ status Regs. */
#define PCTRL_NOC_IRQ_STAT1	0x094
#define PCTRL_NOC_IRQ_STAT2	0x098

#ifdef CONFIG_HISI_NOC_ERR_PROBE
struct transcation_configration {
	unsigned int		statperiod;
	unsigned int		statalarmmax;

	unsigned int		trans_f_mode;
	unsigned int		trans_f_addrbase_low;
	unsigned int		trans_f_addrwindowsize;
	unsigned int		trans_f_opcode;
	unsigned int		trans_f_usermask;
	unsigned int		trans_f_securitymask;

	unsigned int		trans_p_mode;
	unsigned int		trans_p_thresholds_0_0;
	unsigned int		trans_p_thresholds_0_1;
	unsigned int		trans_p_thresholds_0_2;
	unsigned int		trans_p_thresholds_0_3;

	unsigned int		trans_m_counters_0_src;
	unsigned int		trans_m_counters_1_src;
	unsigned int		trans_m_counters_2_src;
	unsigned int		trans_m_counters_3_src;

	unsigned int		trans_m_counters_0_alarmmode;
	unsigned int		trans_m_counters_1_alarmmode;
	unsigned int		trans_m_counters_2_alarmmode;
	unsigned int		trans_m_counters_3_alarmmode;
};

struct	packet_configration {
	unsigned int		statperiod;
	unsigned int		statalarmmax;

	unsigned int		packet_counters_0_src;
	unsigned int		packet_counters_1_src;
	unsigned int		packet_counters_0_alarmmode;
	unsigned int		packet_counters_1_alarmmode;

	unsigned int		packet_filterlut;
	unsigned int		packet_f0_routeidbase;
	unsigned int		packet_f0_routeidmask;
	unsigned int		packet_f0_addrbase;
	unsigned int		packet_f0_windowsize;
	unsigned int		packet_f0_securitymask;
	unsigned int		packet_f0_opcode;
	unsigned int		packet_f0_status;
	unsigned int		packet_f0_length;
	unsigned int		packet_f0_urgency;
	unsigned int		packet_f0_usermask;

	unsigned int		packet_f1_routeidbase;
	unsigned int		packet_f1_routeidmask;
	unsigned int		packet_f1_addrbase;
	unsigned int		packet_f1_windowsize;
	unsigned int		packet_f1_securitymask;
	unsigned int		packet_f1_opcode;
	unsigned int		packet_f1_status;
	unsigned int		packet_f1_length;
	unsigned int		packet_f1_urgency;
	unsigned int		packet_f1_usermask;
};

#endif

struct noc_node {
	char			*name;
	void __iomem		*base;
	unsigned int		bus_id;
	unsigned int		pwrack_bit;

	/* err probe */
#ifdef CONFIG_HISI_NOC_ERR_PROBE
	unsigned int		eprobe_offset;
	bool			eprobe_autoenable;
	int			eprobe_hwirq;

	struct	transcation_configration	tran_cfg;
	struct	packet_configration		packet_cfg;
#endif
};

/* keep target route id, initiator flow id etc*/
struct noc_bus_info {
	char *name;
	unsigned int initflow_mask;
	unsigned int initflow_shift;

	unsigned int targetflow_mask;
	unsigned int targetflow_shift;

	unsigned int targ_subrange_mask;
	unsigned int seq_id_mask;

	char **initflow_array;
	unsigned int initflow_array_size;

	char **targetflow_array;
	unsigned int targetflow_array_size;
};

extern struct noc_bus_info noc_buses_info[];
#endif

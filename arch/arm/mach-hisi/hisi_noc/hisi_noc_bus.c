/* noc buses info */
#define CFG_INITFLOW_ARRAY_SIZE		17
#define CFG_TARGETFLOW_ARRAY_SIZE	28

char *cfg_initflow_array[CFG_INITFLOW_ARRAY_SIZE] = {
	"noc_hsic",
	"noc_usb2",
	"noc_mmc1bus_sd3",
	"noc_mmc1bus_sdio0",
	"noc_asp_mst",
	"noc_cci2sysbus",
	"noc_djtag_mst",
	"noc_dmac_mst",
	"noc_emmc",
	"noc_iom3_mst0",
	"noc_iom3_mst1",
	"noc_lpm3_mst",
	"noc_modem_mst",
	"noc_sec_p",
	"noc_sec_s",
	"noc_socp",
	"noc_top_cssys"
};

char *cfg_targetflow_array[CFG_TARGETFLOW_ARRAY_SIZE] = {
	"aspbus_service_target",
	"cfgbus_service_target",
	"dbgbus_service_target",
	"dmabus_service_target",
	"mmc0bus_service_target",
	"mmc1bus_service_target",
	"modembus_service_target",
	"noc_asp_cfg",
	"noc_dmac_cfg",
	"noc_emmc_sdio1_cfg",
	"noc_gic",
	"noc_hkadc_ssi",
	"noc_hsic_cfg",
	"noc_iom3_slv",
	"noc_lpm3_slv",
	"noc_modem_cfg",
	"noc_sd3_sdio0_cfg",
	"noc_sec_p_cfg",
	"noc_sec_s_cfg",
	"noc_socp_cfg",
	"noc_sys2ddrc",
	"noc_sys_peri0_cfg",
	"noc_sys_peri1_cfg",
	"noc_sys_peri2_cfg",
	"noc_sys_peri3_cfg",
	"noc_sysbus2cci",
	"noc_top_cssys_slv",
	"sysbus_service_target"
};

#define VIVO_INITFLOW_ARRAY_SIZE		12
#define VIVO_TARGETFLOW_ARRAY_SIZE		8
char *vivo_initflow_array[VIVO_INITFLOW_ARRAY_SIZE] = {
	"noc_dss0_rd",
	"noc_dss0_wr",
	"noc_dss1_rd",
	"noc_dss1_wr",
	"noc_isp_p1_rd",
	"noc_isp_p1_wr",
	"noc_isp_p2_rd",
	"noc_isp_p2_wr",
	"noc_isp_p3_rd",
	"noc_isp_p3_wr",
	"noc_isp_p4_rd",
	"noc_vivo_cfg"
};

char *vivo_targetflow_array[VIVO_TARGETFLOW_ARRAY_SIZE] = {
	"dss_service_target",
	"isp_service_target",
	"noc_dss_cfg",
	"noc_isp_cfg",
	"noc_vivobus_ddrc0_rd",
	"noc_vivobus_ddrc0_wr",
	"noc_vivobus_ddrc1_rd",
	"noc_vivobus_ddrc1_wr"
};

#define VCODEC_INITFLOW_ARRAY_SIZE		5
#define VCODEC_TARGETFLOW_ARRAY_SIZE		8
char *vcodec_initflow_array[VCODEC_INITFLOW_ARRAY_SIZE] = {
	"noc_jpegcodec",
	"noc_vcodec_cfg",
	"noc_vdec0",
	"noc_venc",
	"noc_vpp"
};

char *vcodec_targetflow_array[VCODEC_TARGETFLOW_ARRAY_SIZE] = {
	"noc_vcodecbus_ddrc",
	"noc_vdec_cfg",
	"noc_venc_cfg",
	"noc_vpp_cfg",
	"vcodec_bus_service_targe",
	"vdec_service_target",
	"venc_service_target",
	"vpp_service_target"
};

struct noc_bus_info noc_buses_info[MAX_NOC_BUSES_NR] = {
	[0] = {
		.name = "cfg_sys_noc_bus",
		.initflow_mask = ((0x1f) << 17),
		.targetflow_mask = ((0x1f) << 12),
		.targ_subrange_mask = 0,
		.seq_id_mask = 0,
		.initflow_array = cfg_initflow_array,
		.initflow_array_size = CFG_INITFLOW_ARRAY_SIZE,
		.targetflow_array = cfg_targetflow_array,
		.targetflow_array_size = CFG_TARGETFLOW_ARRAY_SIZE,
	},
	[1] = {
		.name = "vcodec_bus",
		.initflow_mask = ((0x7) << 9),
		.targetflow_mask = ((0x7) << 6),
		.targ_subrange_mask = ((0x1) << 5),
		.seq_id_mask = (0x1f) ,
		.initflow_array = vcodec_initflow_array,
		.initflow_array_size = VCODEC_INITFLOW_ARRAY_SIZE,
		.targetflow_array = vcodec_targetflow_array,
		.targetflow_array_size = VCODEC_TARGETFLOW_ARRAY_SIZE,
	},
	[2] = {
		.name = "vivo_bus",
		.initflow_mask = ((0xf) << 10),
		.targetflow_mask = ((0x7) << 7),
		.targ_subrange_mask = 0,
		.seq_id_mask = 0,
		.initflow_array = vivo_initflow_array,
		.initflow_array_size = VIVO_INITFLOW_ARRAY_SIZE,
		.targetflow_array = vivo_targetflow_array,
		.targetflow_array_size = VIVO_TARGETFLOW_ARRAY_SIZE,
	}
};

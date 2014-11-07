#ifndef __ODIN_MIPI_DSI_H__
#define __ODIN_MIPI_DSI_H__

#define MIPI_DSI_DCS_CMD_WRITE_MINIMUM_BRIGHTNESS      	(0X5E)
#define MIPI_DSI_DCS_CMD_READ_MINIMUM_BRIGHTNESS       	(0X5F)
#define MIPI_DSI_DCS_CMD_MEMORY_WRITE                  	(0X2C)
#define MIPI_DSI_DCS_CMD_MEMORY_READ                   	(0X2E)
#define MIPI_DSI_DCS_CMD_MEMORY_WRITE_CONTINUE         	(0X3C)
#define MIPI_DSI_DCS_CMD_MEMORY_READ_CONTINUE          	(0X3E)

#define MIPI_DSI_DCS_CMD_SET_COLUMN_ADDRESS		(0x2A)
#define MIPI_DSI_DCS_CMD_SET_PAGE_ADDRESS		(0x2B)

enum odin_mipi_dsi_input_format {
	ODIN_DSI_INFMT_RGB24	= 0,
	ODIN_DSI_INFMT_RGB565,
};

enum odin_mipi_dsi_datapath_mode {
	ODIN_DSI_DATAPATH_COMMAND_MODE	= 0,
	ODIN_DSI_DATAPATH_VIDEO_MODE,
};

enum odin_mipi_dsi_lane_status{
	ODIN_DSI_LANE_ULP = 0,
	ODIN_DSI_LANE_LP  = 1,
	ODIN_DSI_LANE_HS = 2,
};

enum odin_mipi_dsi_lane_num{
	ODIN_DSI_1LANE = 0,
	ODIN_DSI_2LANE = 1,
	ODIN_DSI_3LANE = 2,
	ODIN_DSI_4LANE = 3,
};

enum odin_mipi_dsi_hs_eotp
{
   	ODIN_DSI_HS_EOTP_DISABLE =0,
   	ODIN_DSI_HS_EOTP_ENABLE

};

enum odin_mipi_dsi_hs_modes
{
   	ODIN_DSI_HS_MODE_0 =0, /* Fifo access by ARM*/
   	ODIN_DSI_HS_MODE_1     /* Fifo access by DSSDMA*/
} ;

enum mipi_dsi_src_data_id {
	/* generic short write, no parameter*/
	DSI_SRC_DATA_ID_GEN_SHORT_WRITE_NO_PARA		= 0x03,
	/* generic short write, 1 parameter */
	DSI_SRC_DATA_ID_GEN_SHORT_WRITE_1_PARA		= 0x13,
	/* generic short write, 2 parameter*/
	DSI_SRC_DATA_ID_GEN_SHORT_WRITE_2_PARA		= 0x23,
	/* generic read, , no parameter */
	DSI_SRC_DATA_ID_GEN_SHORT_READ_NO_PARA		= 0x04,
	/* generic read, , 1 parameter*/
	DSI_SRC_DATA_ID_GEN_SHORT_READ_1_PARA		= 0x14,
	/* generic read, , 2 parameter*/
	DSI_SRC_DATA_ID_GEN_SHORT_READ_2_PARA		= 0x24,
	/* Generic Long Write*/
	DSI_SRC_DATA_ID_GEN_LONG_WRITE			= 0x29,
	/* DCS Short Write, no parameter*/
	DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA		= 0x05,
	/* DCS Short Write, 1 parameter*/
	DSI_SRC_DATA_ID_DCS_SHORT_WRITE_1_PARA		= 0x15,
	/* DCS Short Write, 2 parameter*/
	DSI_SRC_DATA_ID_DCS_SHORT_WRITE_2_PARA		= 0x25,
	/* DCS Read, no parameter*/
	DSI_SRC_DATA_ID_DCS_READ_NO_PARA		= 0x06,
	/* DCS Long Write/write_LUT Command Packet*/
	DSI_SRC_DATA_ID_DCS_LONG_WRITE			= 0x39,
	/* Set maximum return packet size*/
	DSI_SRC_DATA_ID_SET_MAX_RETURN_PACKET_SIZE	= 0x37,
	/* loosely packed pixel stream, 20bit YCbCr, 422 format*/
	DSI_SRC_DATA_ID_LOOSELY_PACKED_YUV422_20BIT	= 0x0C,
	/* packed pixel stream, 24bit YCbCr, 422 format*/
	DSI_SRC_DATA_ID_PACKED_YUV422_24BIT		= 0x1C,
	/* packed pixel stream, 16bit YCbCr, 422 format*/
	DSI_SRC_DATA_ID_PACKED_YUV422_16BIT		= 0x2C,
	/* packed pixel stream, 30bit RGB, 10-10-10 format*/
	DSI_SRC_DATA_ID_PACKED_RGB_30BIT		= 0x0D,
	/* packed pixel stream, 36bit RGB, 12-12-12 format*/
	DSI_SRC_DATA_ID_PACKED_RGB_36BIT		= 0x1D,
	/* packed pixel stream, 12bit YCbCr, 420 format*/
	DSI_SRC_DATA_ID_PACKED_YUV420_12BIT		= 0x3D,
	/* packed pixel stream, 16bit RGB, 565 format*/
	DSI_SRC_DATA_ID_PACKED_RGB_16BIT		= 0x0E,
	/* packed pixel stream, 18bit RGB, 666 format*/
	DSI_SRC_DATA_ID_PACKED_RGB_18BIT		= 0x1E,
	/* loosely packed pixel stream, 18bit RGB, 666 format*/
	DSI_SRC_DATA_ID_LOOSELY_PACKED_RGB_18BIT	= 0x2E,
	/* packed pixel stream, 24bit RGB, 888 format*/
	DSI_SRC_DATA_ID_PACKED_RGB_24BIT		= 0x3E,
}; /* Data Types for Processor-sourced Packets*/

enum mipi_dsi_dst_data_id {
	/* Generic Read, 1 parameter*/
	DSI_DST_DATA_ID__GEN_READ_1_BYTE_RETURNED          = 0x11,
	/* Set maximum return packet size*/
	DSI_DST_DATA_ID__DCS_SHORT_READ_1_BYTE_RETURNED    = 0x21,
	/* Generic Read, no parameter*/
	DSI_DST_DATA_ID__ACK_AND_ERR_REPORT                = 0x02,
	/* Generic Read, 2 parameter*/
	DSI_DST_DATA_ID__GEN_READ_2_BYTES_RETURNED         = 0x12,
	/* DCS Long Write/write_LUT Command Packet*/
	DSI_DST_DATA_ID__DCS_SHORT_READ_2_BYTES_RETURNED   = 0x22,
	/* DCS Read, no parameter*/
	DSI_DST_DATA_ID__EOTP                              = 0x08,
	/* DCS Short Write*/
	DSI_DST_DATA_ID__GEN_LONG_READ                     = 0x1A,
	/* Generic Long Write*/
	DSI_DST_DATA_ID__DCS_LONG_READ                     = 0x1C,
}; /* Data Types for Peripheral-sourced Packets*/

struct odin_mipi_dsi_video_timing_info {
	u16 hsa_count;
	u16 hfp_count;
	u16 hbp_count;
	u16 bllp_count;

	u16 num_vsa_lines;
	u16 num_vfp_lines;
	u16 num_vbp_lines;

	u16 num_bytes_lines;
	u16 num_vact_lines;

	u16 full_sync;
};

enum mipi_dsi_packet_type {
	MIPI_DSI_SHORT_PACKET = 0,
	MIPI_DSI_LONG_PACKET,
};

enum mipi_dsi_lane_index {
	MIPI_DSI_INDEX_LANE0 = 0,
	MIPI_DSI_INDEX_LANE1 = 1,
	MIPI_DSI_INDEX_LANE2 = 2,
	MIPI_DSI_INDEX_LANE3 = 3,
	MIPI_DSI_INDEX_CLK = 4,
	MIPI_DSI_INDEX_CLK1 = 5,
};

enum mipi_dsi_hs_access {
	MIPI_DSI_HS_ACCESS_FIFO = 0, 	/* Fifo access by ARM*/
	MIPI_DSI_HS_ACCESS_DU  		/*s Fifo access by DU*/
};


enum mipi_dsi_fifo_flush {
	MIPI_DSI_RX_FIFO_FLUSH0 = 0,
	MIPI_DSI_RX_FIFO_FLUSH1 = 1,
	MIPI_DSI_RX_FIFO_FLUSH2 = 2,
	MIPI_DSI_RX_FIFO_FLUSH3 = 3,
	MIPI_DSI_CFG_FIFO_FLUSH = 4,
	MIPI_DSI_DATA_FIFO_FLUSH = 5,
};

enum mipi_dsi_lane_state {
	MIPI_DSI_CLK_LANE_ULP = 0,
	MIPI_DSI_CLK_LANE_HS = 1,
	MIPI_DSI_DATA_LANE_ULP = 2,
	MIPI_DSI_DATA_LANE_LP = 3,
	MIPI_DSI_DATA_LANE_HS = 4,
};

enum mipi_video_synch_mode {
	MIPI_VIDEO_PARTIAL_SYNCH  = 0,
	MIPI_VIDEO_FULL_SYNCH
};

struct mipi_video_timing2 {
	u16	hfp_count;
	u16	hbp_count;
};

struct mipi_video_config1 {
	enum mipi_video_synch_mode 	full_sync;
	u8				num_vfp_lines;
	u8				num_vbp_lines;
	u8				num_vsa_lines;
};

struct mipi_video_config2 {
	u16				num_bytes_lines;
	u16				num_vact_lines;
};

struct mipi_dsi_info {
	u32				width;
	u32				height;
	u32				lane_num;

	enum mipi_dsi_packet_type 	packet_type;
	enum mipi_dsi_hs_access		hs_access;

	enum odin_mipi_dsi_input_format	infmt;
	enum odin_mipi_dsi_output_format	outfmt;
};


enum mipi_swap_flag {
	MIPI_SWAP_NO_OPERATION = 0,
	MIPI_SWAP_OPERATION,
};

enum mipi_hs_eotp {
	MIPI_HS_EOTP_DISABLE =0,
	MIPI_HS_EOTP_ENABLE
};


enum mipi_cmd_config_wait_fifo {
	MIPI_CMD_CONFIG_DONOT_WAIT_FIFO =0,
	MIPI_CMD_CONFIG_WAIT_FIFO_ENABLE
};

enum mipi_status_phy_direction {
	MIPI_STATUS_PHY_DIR_TX =0,
	MIPI_STATUS_PHY_DIR_RX
};

enum mipi_status_rx_power {
	MIPI_STATUS_RX_POWER_NORMAL =0,
	ODIN_MIPI_STATUS_RX_ULTR_LOWPOWER
};

enum mipi_status_escape_cmd_register {
	MIPI_STATUS_ESCAPE_CMD_REGISTER_STALL =0,
	MIPI_STATUS_ESCAPE_CMD_REGISTER_READY
};

enum mipi_status_hs_data {
	MIPI_STATUS_HS_DATA_STALL =0,
	MIPI_STATUS_HS_DATA_READY
};

enum mipi_status_hs_config {
	MIPI_STATUS_HS_CONFIG_STALL =0,
	MIPI_STATUS_HS_CONFIG_READY
};

enum mipi_status_phy_clk_data {
	MIPI_STATUS_PHY_CLK_DADA_ACTIVE =0,
	MIPI_STATUS_PHY_CLK_DATA_STOP ,
	MIPI_STATUS_PHY_CLK_DATA_ULTR_LOWPOWER
};

enum mipi_fifos {
	MIPI_RX_FIFO =0,
	MIPI_TX_CFG_FIFO ,
	MIPI_TX_DATA_FIFO
};


enum mipi_data_id {
	MIPI_CMDDATA_LONG_DATAID   = 0x10,
	MIPI_CMDONLY_SHORT_DATAID  = 0x20,
	MIPI_DATAONLY_SHORT_DATAID = 0x30,
	MIPI_DATAONLY_LONG_DATAID  = 0x31
};

union mipi_packet_data_id {
	struct {
		volatile u8  dt  :	6;   /* bit0-5*/
		volatile u8  vc  :	2;   /* bit6-7*/
	} af;
	volatile u8 a8;
};

enum mipi_dsi_state {
	MIPI_FRAMEDONE_WAITING 	   = 0x0,
	MIPI_PENDING_PROCESSING    = 0x1,
	MIPI_FIRST_START		   = 0x2,
	MIPI_SECOND_START 		   = 0x3,
	MIPI_COMPLETE			   = 0x4,
};

struct mipi_short_packet_format {
	union mipi_packet_data_id 	data_id;
	u8				data0;
	u8				data1;
};

struct mipi_long_packet_format {
	union mipi_packet_data_id 	data_id;
	u8                		byte_count_lsb;
	u8				byte_count_msb;
	u8				*pdatapayload;
};

struct mipi_configure {
	enum odin_mipi_dsi_datapath_mode 	datapath_mode;
	enum odin_mipi_dsi_output_format	output_fmt;
	enum odin_mipi_dsi_input_format	input_fmt;
};

#define LONG_CMD_MIPI   0
#define SHORT_CMD_MIPI  1
#define END_OF_COMMAND  2
#define DSI_DELAY	3

struct odin_dsi_cmd {
	u8	cmd_type;
	u8	data_id;
	union {
			u16 data_len;
			struct {
						u8 data0;
						u8 data1;
			} sp;
			u16 delay_ms;
	} sp_len_dly;
	u8	*pdata;
};

#define DSI_CMD_SHORT(di, p0, p1)	{ \
				 .cmd_type = SHORT_CMD_MIPI, \
				 .data_id = di, \
				 .sp_len_dly.sp.data0 = p0, \
				 .sp_len_dly.sp.data1 = p1, \
				}

#define DSI_DLY_MS(ms)  		{ \
				.cmd_type = DSI_DELAY, \
				.sp_len_dly.delay_ms = ms, \
				}

#define DSI_CMD_LONG(di, ptr)   	{ \
				.cmd_type = LONG_CMD_MIPI, \
				.data_id = di, \
				.sp_len_dly.data_len = ARRAY_SIZE(ptr), \
				.pdata = ptr, \
				}

u32 odin_mipi_dsi_cmd(
	enum odin_mipi_dsi_index resource_index,
	struct odin_dsi_cmd *dsi_cmd, u32 num_of_cmd);
void odin_mipi_dsi_fifo_flush_all(
	enum odin_mipi_dsi_index resource_index);
u32 odin_mipi_hs_video_mode(
	enum odin_mipi_dsi_index resource_index,
	struct odin_dss_device *dssdev);
void odin_mipi_hs_video_mode_stop(enum odin_mipi_dsi_index resource_index,
	struct odin_dss_device *dssdev);
u32 odin_mipi_hs_cmd_mode_stop(enum odin_mipi_dsi_index resource_index,
	struct odin_dss_device *dssdev);
u32 odin_mipi_hs_cmd_mode(
	enum odin_mipi_dsi_index resource_index,
	struct odin_dss_device *dssdev);
u32 odin_mipi_cmd_loop_set(enum odin_mipi_dsi_index resource_index,
	bool enable, unsigned int loop_cnt);
u32 odin_mipi_hs_command_update(
	enum odin_mipi_dsi_index resource_index, struct odin_dss_device *dssdev);
int mipi_hs_command_pending(struct odin_dss_device *dssdev);
u32 odin_mipi_hs_command_update_loop(enum odin_mipi_dsi_index resource_index,
	struct odin_dss_device *dssdev);
u32 odin_mipi_hs_command_sync(enum odin_mipi_dsi_index resource_index);
int odin_mipi_dsi_check_tx_fifo(
	enum odin_mipi_dsi_index resource_index);
u32 odin_mipi_dsi_get_tx_fifo(enum odin_mipi_dsi_index resource_index);
void odin_mipi_dsi_set_hs_cmd_size(struct odin_dss_device *dssdev);
void odin_mipi_dsi_unregister_irq(struct odin_dss_device *dssdev);

void odin_mipi_dsi_init(struct odin_dss_device *dssdev);
void odin_mipi_dsi_set_lane_config(
	enum odin_mipi_dsi_index resource_index,
	struct odin_dss_device *dssdev,
	enum odin_mipi_dsi_lane_status status);
void odin_mipi_dsi_panel_config(
	enum odin_mipi_dsi_index resource_index,
	struct odin_dss_device *dssdev);

int odin_mipi_dsi_init_platform_driver(void);
void odin_mipi_dsi_uninit_platform_driver(void);

bool mipi_dsi_get_vsync_irq_status(void);
void mipi_dsi_vsync_enable(struct odin_dss_device *dssdev);
void mipi_dsi_vsync_disable(struct odin_dss_device *dssdev);
int mipi_dsi_vsync_ctrl_status(struct odin_dss_device *dssdev);
int odin_mipi_dsi_check_AwER(enum odin_mipi_dsi_index resource_index);

/*struct mipi_hs_packet_config {
	enum mipi_packet_type		packet_type;
	enum mipi_hs_mode		hs_mode;
	enum mipi_hs_eotp		eotp_enable;
	u32				payload;
	union mipi_packet_data_id	data_id;
};*/

#endif	/* __ODIN_MIPI_DSI_H__*/



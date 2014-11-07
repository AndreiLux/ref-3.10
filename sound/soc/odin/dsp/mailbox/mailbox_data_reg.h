/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* For OMX */

typedef union {
	struct {
		unsigned int resource_id			:8;		/* 0:7   */
		unsigned int receiver_object_id		:8;		/* 8:15  */
		unsigned int sender_object_id		:8;		/* 16:23 */
		unsigned int receiver_device_id		:4;		/* 24:27 */
		unsigned int sender_device_id		:4;		/* 28:31 */
	} field;
	unsigned int value;
} MAILBOX_DATA_REGISTER0_OMX;

typedef union {
	struct {
		unsigned int type				:3;		/* 0:2               */
		unsigned int action				:4;		/* 3:6               */
		unsigned int reserved1			:2;		/* 7:8 (reserved)    */
		unsigned int task_group			:4;		/* 9:12              */
		unsigned int reserved2			:3;		/* 13:15 (reserved)  */
		unsigned int transaction_id		:16;	/* 16:31             */
	} field_audio_manager;
	struct {
		unsigned int type				:3;		/* 0:2               */
		unsigned int reserved1			:6;		/* 3:8   (reserved)  */
		unsigned int status				:4;		/* 9:12              */
		unsigned int reserved2			:3;		/* 13:15 (reserved)  */
		unsigned int transaction_id		:16;	/* 16:31             */
	} field_cdaif;
	unsigned int value;
} MAILBOX_DATA_REGISTER1_OMX;

typedef union {
	struct {
		unsigned int sampling_frequency		:5;		/* 0:4               */
		unsigned int bit_per_sample			:4;		/* 5:8               */
		unsigned int channel_number			:4;		/* 9:12              */
		unsigned int bit_rate				:3;		/* 13:15             */
		unsigned int codec_type				:4;		/* 16:19             */
		unsigned int reserved				:12;	/* 20:31 (reserved)  */
	} field;
	unsigned int value;
} MAILBOX_DATA_REGISTER4_OMX;



/* For LPA */

typedef union {
	struct {
		unsigned int command			:12;	/* 0:11  */
		unsigned int type				:4;		/* 12:15 */
		unsigned int transaction_id		:16;	/* 16:31 */
	} field;
	unsigned int value;
} MAILBOX_DATA_REGISTER0_CHANNEL0;

typedef union {
	struct {
		unsigned int command			:12;	/* 0:11  */
		unsigned int type				:4;		/* 12:15 */
		unsigned int transaction_id		:16;	/* 16:31 */
	} field;
	unsigned int value;
} MAILBOX_DATA_REGISTER0_CHANNEL1;

typedef union {
	struct {
		unsigned int command			:12;	/* 0:11  */
		unsigned int type				:4;		/* 12:15 */
		unsigned int transaction_id		:16;	/* 16:31 */
	} field;
	unsigned int value;
} MAILBOX_DATA_REGISTER0_CHANNEL2;

#if 0
typedef union {
	struct {
		unsigned int resource_id		:8;		/* 0:7    */
		unsigned int receiver_object_id	:8;		/* 8:15   */
		unsigned int sender_object_id	:8;		/* 16:23  */
		unsigned int receiver_device_id	:4;		/* 24:27  */
		unsigned int sender_device_id	:4;		/* 28:31  */
	} field;
	unsigned int value;
} MAILBOX_DATA_REGISTER1_COMMAND;
#endif

typedef union {
	struct {
		unsigned int axi_ahb_clock		:16;	/* 0:15   */
		unsigned int dsp_core_clock		:16;	/* 16:31  */
	} field_ready;
	struct {
		unsigned int operation_mode		:28;	/* 0:27	  */
		unsigned int flow_type			:4;		/* 28:31  */
	} field_create;
	struct {
		unsigned int algorithm_type		:16;	/* 0:15	   */
		unsigned int post_processing	:8;		/* 16:23   */
		unsigned int pre_processing		:4;		/* 24:27   */
		unsigned int vr					:4;		/* 28:31   */
	} field_open;
	struct {
		unsigned int sampling_frequency :24;	/* 0:23    */
		unsigned int woofer				:4;		/* 24:27   */
		unsigned int stereo				:4;		/* 28:31   */
	} field_control_init;
	struct {
		unsigned int base_address		:32;	/* 0:31    */
	} field_play;
	struct {
		unsigned int base_address		:32;	/* 0:31    */
	} field_alarm;
	struct {
		int vol_value					:32;	/* 0:31    */
	} field_volume;
	struct {
		int vol_value					:32;	/* 0:31    */
	} field_gain;
	struct {
		unsigned int timestamp_upper	:32;	/* 0:31    */

	} field_timestamp;
	struct {
		unsigned int base_address		:32;	/* 0:31   */
	} field_capture;
	struct {
		unsigned int base_address		:32;	/* 0:31    */
	} field_playback;
#if 0
	struct {
		unsigned int reserved			:6;	/*0:5                                 */
		unsigned int post_processing		:7;	/*6:12                                */
		unsigned int pre_ec				:1;	/*13	echo cancellation           */
		unsigned int pre_ns				:1;	/*14	noise supression            */
		unsigned int pre_nc				:1;	/*15	noise cancellation          */
		unsigned int vr_reserved		:1;	/*16	voice recognition reserved  */
		unsigned int vr_vr				:1;	/*17	                              */
		unsigned int vr_vad				:1;	/*18	                              */
		unsigned int algorithm_type		:4;	/*19:22	                              */
		unsigned int operation_mode		:7;	/*23:29	                              */
		unsigned int flow_type			:2;	/*30:31	                              */
	} field_create_open;
#endif
	struct {
		unsigned int is_normalizer_enable       :32;    /* 0:31 */
	} field_normalizer;
	unsigned int value;
} MAILBOX_DATA_REGISTER1_CHANNEL0;

typedef union {
	struct {
		unsigned int base_address		:32;		/* 0:31 */
	} field_playback;
	struct {
		unsigned int base_address		:32;		/* 0:31 */
	} field_capture;
	unsigned int value;
} MAILBOX_DATA_REGISTER1_CHANNEL2;


typedef union {
	struct {
		unsigned int async_apb_clock		:16;	/* 0:15   */
		unsigned int sync_apb_clock			:16;	/* 16:31  */
	} field_ready;
	struct {
		unsigned int i2s_sample_rate		:32;	/* 0:31   */
	} field_create;
	struct {
		unsigned int bit_rate			:24;	/* 0:23   */
		unsigned int bit_per_sample		:8;		/* 24:31  */
	} field_control_init;
	struct {
		unsigned int length				:32;	/* 0:31	*/
	} field_play;
	struct {
		unsigned int length				:32;	/* 0:31	*/
	} field_alarm;
	struct {
		unsigned int timestamp_lower	:32;	/* 0:31	*/
	} field_timestamp;
	struct {
		unsigned int base_address		:32;	/* 0:31	*/
	} field_capture;
	struct {
		unsigned int base_address		:32;	/* 0:31	*/
	} field_playback;
	unsigned int value;
} MAILBOX_DATA_REGISTER2_CHANNEL0;

typedef union {
	struct {
		unsigned int consumed_buffer_size:32;		/* 0:31	*/
	} field_playback;
	struct {
		unsigned int consumed_buffer_size:32;		/* 0:31	*/
	} field_capture;
	unsigned int value;
} MAILBOX_DATA_REGISTER2_CHANNEL2;

typedef union {
	struct {
		unsigned int sys_table_addr		:32;	/* 0:31	*/
	} field_ready;
	struct {
		unsigned int nearstop_threshold	:32;	/* 0:31	*/
	} field_play;
	struct {
		int volume						:32;	/* 0:31	*/
	} field_alarm;
	struct {
		unsigned int size				:32;	/* 0:31	*/
	} field_playback;
	struct {
		unsigned int decoded_frame_upper:32;	/* 0:31 */
	} field_decoded_frame;
	struct {
		unsigned int timer_wait_ddr_wakeup:32;	/* 0:31 */
	} field_timer_wait;
	unsigned int value;
} MAILBOX_DATA_REGISTER3_CHANNEL0;

typedef union {
	struct {
		unsigned int sys_table_length		:32;	/* 0:31	*/
	} field_ready;
	struct {
		unsigned int length				:32;	/* 0:31	*/
	} field_control_pp;
	struct {
		unsigned int operation			:32;    /* 0:31	*/
	} field_playback;
	struct {
		unsigned int decoded_frame_lower:32;	/* 0:32 */
	} field_decoded_frame;
	struct {
		unsigned int timer_wait_ddr_sleep:32;	/* 0:31 */
	} field_timer_wait;
	unsigned int value;
} MAILBOX_DATA_REGISTER4_CHANNEL0;

typedef union {
	struct {
		unsigned int remain_byte		:32;	/* 0:31	*/
	} field_nearconsumed;
	unsigned int value;
} MAILBOX_DATA_REGISTER4_CHANNEL2;

typedef union {
	struct {
		unsigned int reserved			:16;	/* 0:15	*/
		unsigned int nearstop_threshold	:16;	/* 16:31	*/
	} field_control_pp;
	struct {
		unsigned int timer_wait_arm_wakeup:32;	/* 0:31 */
	} field_timer_wait;
	unsigned int value;
} MAILBOX_DATA_REGISTER5_CHANNEL0;


#if 0
typedef union {
	struct {
		unsigned int reserved			:22;	/* 0:21		*/
		unsigned int command			:7;		/* 22:28    */
		unsigned int type				:3;		/* 29:31    */
	} field;
	unsigned int value;
} MAILBOX_DATA_REGISTER2_COMMAND;


typedef union {
	struct {
		unsigned int param_flag			:1;		/* 0 */
		unsigned int reserved			:31;		/* 1:31 */
	} field;
	unsigned int value;
} MAILBOX_DATA_REGISTER3_COMMAND;


typedef union {
	struct {
		unsigned int sys_table_addr		:32;	/* 0:31	*/
	} field_ready;
	struct {
		unsigned int timestamp_upper	:32;	/* 0:31	*/
	} field_time;
	unsigned int value;
} MAILBOX_DATA_REGISTER0_PARAMETER;

typedef union {
	struct {
		unsigned int dspCoreClk			:32;	/* 0:31	*/
	} field_ready;
	struct {
		unsigned int timestamp_lower	:32;	/* 0:31	*/
	} field_time;
	unsigned int value;
} MAILBOX_DATA_REGISTER1_PARAMETER;

typedef union {
	struct {
		unsigned int reserved			:6;	/*0:5									*/
		unsigned int post_processing		:7;	/*6:12                                  */
		unsigned int pre_ec				:1;	/*13	echo cancellation             */
		unsigned int pre_ns				:1;	/*14	noise supression              */
		unsigned int pre_nc				:1;	/*15	noise cancellation            */
		unsigned int vr_reserved		:1;	/*16	voice recognition reserved    */
		unsigned int vr_vr				:1;	/*17	                                */
		unsigned int vr_vad				:1;	/*18	                                */
		unsigned int algorithm_type		:4;	/*19:22	                                */
		unsigned int operation_mode		:7;	/*23:29	                                */
		unsigned int flow_type			:2;	/*30:31	                                */
	} field;
	unsigned int value;
} MAILBOX_DATA_REGISTER2_PARAMETER;

typedef union {
	struct {
		unsigned int volume				:9;		/*0:8    */
		unsigned int bit_rate			:10;	/*9:18   */
		unsigned int samplingFreq		:5;		/*19:23  */
		unsigned int bit_per_sample		:4;		/*24:27  */
		unsigned int channel_number		:4;		/*28:31  */
	} field;
	unsigned int value;
} MAILBOX_DATA_REGISTER3_PARAMETER;

typedef union {
	struct {
		unsigned int address			:32;	/* 0:31	*/
	} field;
	unsigned int value;
} MAILBOX_DATA_REGISTER4_PARAMETER;

typedef union {
	struct {
		unsigned int length				:32;	/* 0:31	*/
	} field;	
	unsigned int value;
} MAILBOX_DATA_REGISTER5_PARAMETER;

typedef union {
	struct {
		unsigned int pcm_upload_addr	:32;	/* 0:31	*/
	} field;	
	unsigned int value;
} MAILBOX_DATA_REGISTER6_PARAMETER;

/* For OMX */
typedef struct {
	unsigned int
	type				:3,		/* 0:2				*/
	action				:4,		/* 3:6              */
						:2,		/* 7:8 (reserved)   */
	task_group			:4,		/* 9:12             */
						:3,		/* 13:15 (reserved) */
	transaction_id		:16;	/* 16:31            */
} MAILBOX_DATA_REGISTER1_AUDIO_MANAGER;

typedef struct {
	unsigned int
	task_action			:6,		/* 0:5		*/
	algorithm_type		:17,	/* 6:22     */
	task_type			:1,		/* 23       */
	task_group			:5,		/* 24:28    */
	command_id			:3;		/* 29:31    */
} MAILBOX_DATA_REGISTER2;

typedef struct {
	unsigned int
						:9,		/* 0:8 (reserved)	*/
	bit_rate			:10,	/* 9:18             */
	sampling_frequency	:5,		/* 19:23            */
	bit_per_sample		:4,		/* 24:27            */
	channel_number		:4;		/* 28:31            */
} MAILBOX_DATA_REGISTER3;
#endif



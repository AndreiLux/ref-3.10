/*
 * @file:error.h
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef ERROR_H_
#define ERROR_H_
/**
 * @file
 * Define error codes and set error global variable
 */
#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{

	NO_ERROR = 0, /* No error has occurred, or  buffer has been emptied*/
	ERR_NOT_IMPLEMENTED, /*  */

	ERR_INVALID_PARAM_VENDOR_NAME, /*  */
	ERR_INVALID_PARAM_PRODUCT_NAME, /*  */
	ERR_3D_STRUCT_NOT_SUPPORTED, /* No 3D packets are sent - FATAL ERROR */
	ERR_FORCING_VSD_3D_IGNORED, /* Configuring a VSD will cancel the 3D packets */
	ERR_PRODUCT_INFO_NOT_PROVIDED,
	/* Non fatal, SPD (and VSD if no 3D) were not configured */
	ERR_MEM_ALLOCATION,
	/* Dynamic memory allocated for HDCP functionality failed - FATAL ERROR */
	ERR_HDCP_MEM_ACCESS,
	/* Memory where runtime HDCP information are written
	by HDCP engine cannot be accessed - FATAL ERROR */
	ERR_HDCP_FAIL,
	ERR_SRM_INVALID, /* SRM validation will not work */

	ERR_PHY_TEST_CONTROL, /* PHY test control to program PHY- FATAL ERROR*/
	ERR_PHY_NOT_LOCKED,

	ERR_PIXEL_CLOCK_NOT_SUPPORTED, /* Pixel clock not supported - FATAL ERROR*/
	/* Pixel repetition not supported - FATAL ERROR*/
	ERR_PIXEL_REPETITION_NOT_SUPPORTED,
	ERR_COLOR_DEPTH_NOT_SUPPORTED,
	/* Color depth not supported - FATAL ERROR*/
	ERR_DVI_MODE_WITH_PIXEL_REPETITION,
	/* DVI does not support packets so
	it cannot tell if there is pixel repetition - FATAL ERROR */
	ERR_CHROMA_DECIMATION_FILTER_INVALID,
	/* an invalid filter chosen - FATAL ERROR */
	ERR_CHROMA_INTERPOLATION_FILTER_INVALID,
	/* an invalid filter chosen - FATAL ERROR */
	ERR_COLOR_REMAP_SIZE_INVALID,
	/* an invalid YCC422 remap size - FATAL ERROR */
	ERR_INPUT_ENCODING_TYPE_INVALID,
	/* check video parameters for encoding type enumerator - FATAL ERROR*/
	ERR_OUTPUT_ENCODING_TYPE_INVALID,
	/* check video parameters for encoding type enumerator - FATAL ERROR*/
	ERR_AUDIO_CLOCK_NOT_SUPPORTED,
	ERR_SFR_CLOCK_NOT_SUPPORTED,
	ERR_INVALID_I2C_ADDRESS,
	ERR_BLOCK_CHECKSUM_INVALID,
	ERR_HPD_LOST,
	ERR_DTD_BUFFER_FULL,
	ERR_SHORT_AUDIO_DESC_BUFFER_FULL,
	ERR_SHORT_VIDEO_DESC_BUFFER_FULL,
	ERR_DTD_INVALID_CODE,
	ERR_PIXEL_REPETITION_FOR_VIDEO_MODE,
	ERR_OVERFLOW,

	ERR_SINK_DOES_NOT_SUPPORT_AUDIO,
	ERR_SINK_DOES_NOT_SUPPORT_AUDIO_SAMPLING_PREQ,
	ERR_SINK_DOES_NOT_SUPPORT_AUDIO_SAMPLE_SIZE,
	ERR_SINK_DOES_NOT_SUPPORT_ATTRIBUTED_CHANNELS,
	ERR_SINK_DOES_NOT_SUPPORT_AUDIO_TYPE,
	ERR_SINK_DOES_NOT_SUPPORT_30BIT_DC,
	ERR_SINK_DOES_NOT_SUPPORT_36BIT_DC,
	ERR_SINK_DOES_NOT_SUPPORT_48BIT_DC,
	ERR_SINK_DOES_NOT_SUPPORT_YCC444_DC,
	ERR_SINK_DOES_NOT_SUPPORT_TMDS_CLOCK,
	ERR_SINK_DOES_NOT_SUPPORT_HDMI,
	ERR_SINK_DOES_NOT_SUPPORT_YCC444_ENCODING,
	ERR_SINK_DOES_NOT_SUPPORT_YCC422_ENCODING,
	ERR_SINK_DOES_NOT_SUPPORT_XVYCC601_COLORIMETRY,
	ERR_SINK_DOES_NOT_SUPPORT_XVYCC709_COLORIMETRYY,
	ERR_EXTENDED_COLORIMETRY_PARAMS_INVALID,
	ERR_SINK_DOES_NOT_SUPPORT_EXTENDED_COLORIMETRY,
	ERR_DVI_MODE_WITH_YCC_ENCODING,
	ERR_DVI_MODE_WITH_EXTENDED_COLORIMETRY,
	ERR_SINK_DOES_NOT_SUPPORT_SELECTED_DTD,

	ERR_DATA_WIDTH_INVALID,
	ERR_DATA_GREATER_THAN_WIDTH,
	ERR_CANNOT_CREATE_MUTEX,
	ERR_CANNOT_DESTROY_MUTEX,
	ERR_CANNOT_LOCK_MUTEX,
	ERR_CANNOT_UNLOCK_MUTEX,
	ERR_INTERRUPT_NOT_MAPPED,
	ERR_HW_NOT_SUPPORTED,
	ERR_UNDEFINED
} errorType_t;

/**
 * Set the global error variable to the error code
 * @param err the error code
 */
void error_set(errorType_t err);

errorType_t error_get(void);

#ifdef __cplusplus
}
#endif

#endif /*ERROR_H_*/

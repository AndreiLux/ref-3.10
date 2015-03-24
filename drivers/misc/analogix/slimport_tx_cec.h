#ifndef _SP_TX_CEC_H
#define _SP_TX_CEC_H


#define CEC_RECV_BUF_SIZE    96
#define CEC_SEND_BUF_SIZE    64

#define TIME_OUT_THRESHOLD	10
#define LOCAL_REG_TIME_OUT_THRESHOLD	10000

// CEC device logical address
#define CEC_DEVICE_UNKNOWN              15  //not a valid logical address
#define CEC_DEVICE_TV                   0
#define CEC_DEVICE_RECORDING_DEVICE1    1
#define CEC_DEVICE_RECORDING_DEVICE2    2
#define CEC_DEVICE_TUNER1               3
#define CEC_DEVICE_PLAYBACK_DEVICE1     4
#define CEC_DEVICE_AUDIO_SYSTEM         5
#define CEC_DEVICE_TUNER2               6
#define CEC_DEVICE_TUNER3               7
#define CEC_DEVICE_PLAYBACK_DEVICE2     8
#define CEC_DEVICE_RECORDING_DEVICE3    9
#define CEC_DEVICE_TUNER4               10
#define CEC_DEVICE_PLAYBACK_DEVICE3     11
#define CEC_DEVICE_RESERVED1            12
#define CEC_DEVICE_RESERVED2            13
#define CEC_DEVICE_FREEUSE              14
#define CEC_DEVICE_UNREGISTERED         15
#define CEC_DEVICE_BROADCAST            15


// CEC device type
#define CEC_DEVICE_TYPE_TV                  0
#define CEC_DEVICE_TYPE_RECORDING_DEVICE    1
#define CEC_DEVICE_TYPE_RESERVED            2
#define CEC_DEVICE_TYPE_TUNER               3
#define CEC_DEVICE_TYPE_PLAYBACK_DEVICE     4
#define CEC_DEVICE_TYPE_AUDIO_SYSTEM        5


// CEC opcode
#define CEC_OPCODE_ACTIVE_SOURCE                    0x82    // Active Source
#define CEC_OPCODE_IMAGE_VIEW_ON                    0x04    // Image View On
#define CEC_OPCODE_TEXT_VIEW_ON                     0x0D    // Text View On
#define CEC_OPCODE_INACTIVE_SOURCE                  0x9D    // Inactive Source
#define CEC_OPCODE_REQUEST_ACTIVE_SOURCE            0x85    // Request Active Source
#define CEC_OPCODE_ROUTING_CHANGE                   0x80    // Routing Change
#define CEC_OPCODE_ROUTING_INFORMATION              0x81    // Routing Information
#define CEC_OPCODE_SET_STREAM_PATH                  0x86    // Set Stream Path
#define CEC_OPCODE_STANDBY                          0x36    // Standby
#define CEC_OPCODE_RECORD_OFF                       0x0B    // Record Off
#define CEC_OPCODE_RECORD_ON                        0x09    // Record On
#define CEC_OPCODE_RECORD_STATUS                    0x0A    // Record Status
#define CEC_OPCODE_RECORD_TV_SCREEN                 0x0F    // Record TV Screen
#define CEC_OPCODE_CLEAR_ANALOGUE_TIMER             0x33    // Clear Analogue Timer
#define CEC_OPCODE_CLEAR_DIGITAL_TIMER              0x99    // Clear Digital Timer
#define CEC_OPCODE_CLEAR_EXTERNAL_TIMER             0xA1    // Clear External Timer
#define CEC_OPCODE_SET_ANALOGUE_TIMER               0x34    // Set Analogue Timer
#define CEC_OPCODE_SET_DIGITAL_TIMER                0x97    // Set Digital Timer
#define CEC_OPCODE_SET_EXTERNAL_TIMER               0xA2    // Set External Timer
#define CEC_OPCODE_SET_TIMER_PROGRAM_TITLE          0x67    // Set Timer Program Title
#define CEC_OPCODE_TIMER_CLEARED_STATUS             0x43    // Timer Cleared Status
#define CEC_OPCODE_TIMER_STATUS                     0x35    // Timer Status
#define CEC_OPCODE_CEC_VERSION                      0x9E    // CEC Version
#define CEC_OPCODE_GET_CEC_VERSION                  0x9F    // Get CEC Version
#define CEC_OPCODE_GIVE_PHYSICAL_ADDRESS            0x83    // Give Physical Address
#define CEC_OPCODE_GET_MENU_LANGUAGE                0x91    // Get Menu Language
#define CEC_OPCODE_REPORT_PHYSICAL_ADDRESS          0x84    // Report Physical Address
#define CEC_OPCODE_SET_MENU_LANGUAGE                0x32    // Set Menu Language
#define CEC_OPCODE_DECK_CONTROL                     0x42    // Deck Control
#define CEC_OPCODE_DECK_STATUS                      0x1B    // Deck Status
#define CEC_OPCODE_GIVE_DECK_STATUS                 0x1A    // Give Deck Status
#define CEC_OPCODE_PLAY                             0x41    // Play
#define CEC_OPCODE_GIVE_TUNER_DEVICE_STATUS         0x08    // Give Tuner Device Status
#define CEC_OPCODE_SELECT_ANALOGUE_SERVICE          0x92    // Select Analogue Service
#define CEC_OPCODE_SELECT_DIGITAL_SERVICE           0x93    // Select Digital Service
#define CEC_OPCODE_TUNER_DEVICE_STATUS              0x07    // Tuner Device Status
#define CEC_OPCODE_TUNER_STEP_DECREMENT             0x06    // Tuner Step Decrement
#define CEC_OPCODE_TUNER_STEP_INCREMENT             0x05    // Tuner Step Increment
#define CEC_OPCODE_DEVICE_VENDOR_ID                 0x87    // Device Vendor ID
#define CEC_OPCODE_GIVE_DEVICE_VENDOR_ID            0x8C    // Give Device Vendor ID
#define CEC_OPCODE_VENDOR_COMMAND                   0x89    // Vendor Command
#define CEC_OPCODE_VENDOR_COMMAND_WITH_ID           0xA0    // Vendor Command With ID
#define CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN        0x8A    // Vendor Remote Button Down
#define CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP          0x8B    // Vendor Remote Button Up
#define CEC_OPCODE_SET_OSD_STRING                   0x64    // Set OSD String
#define CEC_OPCODE_GIVE_OSD_NAME                    0x46    // Give OSD Name
#define CEC_OPCODE_SET_OSD_NAME                     0x47    // Set OSD Name
#define CEC_OPCODE_MENU_REQUEST                     0x8D    // Menu Request
#define CEC_OPCODE_MENU_STATUS                      0x8E    // Menu Status
#define CEC_OPCODE_USER_CONTROL_PRESSED             0x44    // User Control Pressed
#define CEC_OPCODE_USER_CONTROL_RELEASE             0x45    // User Control Released
#define CEC_OPCODE_GIVE_DEVICE_POWER_STATUS         0x8F    // Give Device Power Status
#define CEC_OPCODE_REPORT_POWER_STATUS              0x90    // Report Power Status
#define CEC_OPCODE_FEATURE_ABORT                    0x00    // Feature Abort
#define CEC_OPCODE_ABORT                            0xFF    // Abort Message
#define CEC_OPCODE_GIVE_AUDIO_STATUS                0x71    // Give Audio Status
#define CEC_OPCODE_GIVE_SYSTEM_AUDIO_MODE_STATUS    0x7D    // Give System Audio Mode Status
#define CEC_OPCODE_REPORT_AUDIO_STATUS              0x7A    // Report Audio Status
#define CEC_OPCODE_REPORT_SHORT_AUDIO_DESCRIPTOR    0xA3    // Report Short Audio Descriptor
#define CEC_OPCODE_REQUEST_SHORT_AUDIO_DESCRIPTOR   0xA4    // Request Short Audio Descriptor
#define CEC_OPCODE_SET_SYSTEM_AUDIO_MODE            0x72    // Set System Audio Mode
#define CEC_OPCODE_SYSTEM_AUDIO_MODE_REQUEST        0x70    // System Audio Mode Request
#define CEC_OPCODE_SYSTEM_AUDIO_MODE_STATUS         0x7E    // System Audio Mode Status
#define CEC_OPCODE_SET_AUDIO_RATE                   0x9A    // Set Audio Rate
#define CEC_OPCODE_INITIATE_ARC                     0xC0    // Initiate ARC
#define CEC_OPCODE_REPORT_ARC_INITIATED             0xC1    // Report ARC Initiated
#define CEC_OPCODE_REPORT_ARC_TERMINATED            0xC2    // Report ARC Terminated
#define CEC_OPCODE_REQUEST_ARC_INITIATION           0xC3    // Request ARC Initiation
#define CEC_OPCODE_REQUEST_ARC_TERMINATION          0xC4    // Request ARC Termination
#define CEC_OPCODE_TERMINATE_ARC                    0xC5    // Terminate ARC
#define CEC_OPCODE_CDC_MESSAGE                      0xF8    // CDC Message


// CEC user control code
#define CEC_UI_SELECT           0x00    // Select
#define CEC_UI_UP               0x01    // Up
#define CEC_UI_DOWN             0x02    // Down
#define CEC_UI_LEFT             0x03    // Left
#define CEC_UI_RIGHT            0x04    // Right

#define CEC_UI_PLAY             0x44    // Play
#define CEC_UI_STOP             0x45    // Stop
#define CEC_UI_PAUSE            0x46    // Pause
#define CEC_UI_FORWARD          0x4B    // Forward
#define CEC_UI_BACKWARD         0x4C    // Backward


typedef enum
{
    CEC_NOT_READY,
    CEC_IS_READY
} CEC_STATUS;

struct tagCECFrameHeader
{
    unchar dest:4;   // Destination Logical Address
    unchar init:4;   // Initiator Logical Address
};

union tagCECFrameData
{
    unchar raw[15];
};

struct tagCECFrame
{
    //unsigned char msglength;
    struct tagCECFrameHeader header;    // header block
    union tagCECFrameData msg;
};

void cec_init(void);
void cec_status_set(CEC_STATUS cStatus);
CEC_STATUS cec_status_get(void);


unchar downStream_hdmi_cec_writemessage(struct tagCECFrame *pFrame, unchar Len);
void downstream_hdmi_cec_readmessage(void);
//void HDMI_CEC_UserControlPressed(void);
//void DownStream_CEC_ParseMessage(void);









//for downstream CEC
//void DownStream_CEC_SetLogicAddr(unsigned char addr);
void downstream_cec_readfifo(void);
unsigned char downstream_cec_checkfullframe(void);
unsigned char downstream_cec_recvframe(unsigned char *pFrameData);
unsigned char downstream_cec_sendframe(unsigned char *pFrameData, unsigned char Len);
//void DownStream_CEC_ParseMessage(void);
//void DownStream_CEC_ParseMessage(struct tagCECFrame *pFrame, unsigned char Len);





//for HDMI RX CEC
unsigned char upstream_hdmi_cec_writemessage(struct tagCECFrame *pFrame, unsigned char Len);

void upstream_hdmi_cec_readmessage(void);
//void UpStream_CEC_ParseMessage(void);

void upstream_cec_readfifo(void);
unsigned char upstream_cec_checkfullframe(void);
unsigned char upstream_cec_recvframe(unsigned char *pFrameData);
unsigned char upstream_cec_sendframe(unsigned char *pFrameData, unsigned char Len);

//bool downstream_cec_is_free(void);

//void HDMI_CEC_BroadcastPhysicalAddress(void);
//void HDMI_CEC_AllocLogicAddr(void);

//void UpStream_CEC_ParseMessage(void);

//void Downstream_CEC_ImageViewOn(void);
//void Downstream_Give_Physical_Addr(void);


//void Uptream_CEC_PING(void);
//void CEC_PING(void);

#ifdef CEC_PHYCISAL_ADDRESS_INSERT
void uptream_cec_parsemessage(void);
void downstream_cec_phy_add_set(unchar addr0,unchar addr1);
void Downstream_Report_Physical_Addr(void);
#endif


#ifdef CEC_DBG_MSG_ENABLED
void trace(const char code *format, ...);
void TraceArray(unsigned char idata array[], unsigned char len);
#define TRACE       trace
#define TRACE_ARRAY TraceArray
#endif

//void DownStream_CEC_ParseMessage(void);


#endif



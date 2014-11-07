#ifndef __HDMI_PANEL_H
#define __HDMI_PANEL_H

/* all video formats defined by EIA CEA 861D */

/* all video formats defined by CEA 861D */
#define HDMI_VFRMT_UNKNOWN              0
#define HDMI_VFRMT_640x480p60_4_3       1
#define HDMI_VFRMT_720x480p60_4_3       2
#define HDMI_VFRMT_720x480p60_16_9      3
#define HDMI_VFRMT_1280x720p60_16_9     4
#define HDMI_VFRMT_1920x1080i60_16_9    5
#define HDMI_VFRMT_720x480i60_4_3       6
#define HDMI_VFRMT_1440x480i60_4_3      HDMI_VFRMT_720x480i60_4_3
#define HDMI_VFRMT_720x480i60_16_9      7
#define HDMI_VFRMT_1440x480i60_16_9     HDMI_VFRMT_720x480i60_16_9
#define HDMI_VFRMT_720x240p60_4_3       8
#define HDMI_VFRMT_1440x240p60_4_3      HDMI_VFRMT_720x240p60_4_3
#define HDMI_VFRMT_720x240p60_16_9      9
#define HDMI_VFRMT_1440x240p60_16_9     HDMI_VFRMT_720x240p60_16_9
#define HDMI_VFRMT_2880x480i60_4_3      10
#define HDMI_VFRMT_2880x480i60_16_9     11
#define HDMI_VFRMT_2880x240p60_4_3      12
#define HDMI_VFRMT_2880x240p60_16_9     13
#define HDMI_VFRMT_1440x480p60_4_3      14
#define HDMI_VFRMT_1440x480p60_16_9     15
#define HDMI_VFRMT_1920x1080p60_16_9    16
#define HDMI_VFRMT_720x576p50_4_3       17
#define HDMI_VFRMT_720x576p50_16_9      18
#define HDMI_VFRMT_1280x720p50_16_9     19
#define HDMI_VFRMT_1920x1080i50_16_9    20
#define HDMI_VFRMT_720x576i50_4_3       21
#define HDMI_VFRMT_1440x576i50_4_3      HDMI_VFRMT_720x576i50_4_3
#define HDMI_VFRMT_720x576i50_16_9      22
#define HDMI_VFRMT_1440x576i50_16_9     HDMI_VFRMT_720x576i50_16_9
#define HDMI_VFRMT_720x288p50_4_3       23
#define HDMI_VFRMT_1440x288p50_4_3      HDMI_VFRMT_720x288p50_4_3
#define HDMI_VFRMT_720x288p50_16_9      24
#define HDMI_VFRMT_1440x288p50_16_9     HDMI_VFRMT_720x288p50_16_9
#define HDMI_VFRMT_2880x576i50_4_3      25
#define HDMI_VFRMT_2880x576i50_16_9     26
#define HDMI_VFRMT_2880x288p50_4_3      27
#define HDMI_VFRMT_2880x288p50_16_9     28
#define HDMI_VFRMT_1440x576p50_4_3      29
#define HDMI_VFRMT_1440x576p50_16_9     30
#define HDMI_VFRMT_1920x1080p50_16_9    31
#define HDMI_VFRMT_1920x1080p24_16_9    32
#define HDMI_VFRMT_1920x1080p25_16_9    33
#define HDMI_VFRMT_1920x1080p30_16_9    34
#define HDMI_VFRMT_2880x480p60_4_3      35
#define HDMI_VFRMT_2880x480p60_16_9     36
#define HDMI_VFRMT_2880x576p50_4_3      37
#define HDMI_VFRMT_2880x576p50_16_9     38
#define HDMI_VFRMT_1920x1250i50_16_9    39
#define HDMI_VFRMT_1920x1080i100_16_9   40
#define HDMI_VFRMT_1280x720p100_16_9    41
#define HDMI_VFRMT_720x576p100_4_3      42
#define HDMI_VFRMT_720x576p100_16_9     43
#define HDMI_VFRMT_720x576i100_4_3      44
#define HDMI_VFRMT_1440x576i100_4_3     HDMI_VFRMT_720x576i100_4_3
#define HDMI_VFRMT_720x576i100_16_9     45
#define HDMI_VFRMT_1440x576i100_16_9    HDMI_VFRMT_720x576i100_16_9
#define HDMI_VFRMT_1920x1080i120_16_9   46
#define HDMI_VFRMT_1280x720p120_16_9    47
#define HDMI_VFRMT_720x480p120_4_3      48
#define HDMI_VFRMT_720x480p120_16_9     49
#define HDMI_VFRMT_720x480i120_4_3      50
#define HDMI_VFRMT_1440x480i120_4_3     HDMI_VFRMT_720x480i120_4_3
#define HDMI_VFRMT_720x480i120_16_9     51
#define HDMI_VFRMT_1440x480i120_16_9    HDMI_VFRMT_720x480i120_16_9
#define HDMI_VFRMT_720x576p200_4_3      52
#define HDMI_VFRMT_720x576p200_16_9     53
#define HDMI_VFRMT_720x576i200_4_3      54
#define HDMI_VFRMT_1440x576i200_4_3     HDMI_VFRMT_720x576i200_4_3
#define HDMI_VFRMT_720x576i200_16_9     55
#define HDMI_VFRMT_1440x576i200_16_9    HDMI_VFRMT_720x576i200_16_9
#define HDMI_VFRMT_720x480p240_4_3      56
#define HDMI_VFRMT_720x480p240_16_9     57
#define HDMI_VFRMT_720x480i240_4_3      58
#define HDMI_VFRMT_1440x480i240_4_3     HDMI_VFRMT_720x480i240_4_3
#define HDMI_VFRMT_720x480i240_16_9     59
#define HDMI_VFRMT_1440x480i240_16_9    HDMI_VFRMT_720x480i240_16_9
#define HDMI_VFRMT_1280x720p24_16_9     60
#define HDMI_VFRMT_1280x720p25_16_9     61
#define HDMI_VFRMT_1280x720p30_16_9     62
#define HDMI_VFRMT_1920x1080p120_16_9   63
#define HDMI_VFRMT_1920x1080p100_16_9   64
/* Video Identification Codes from 65-127 are reserved for the future */
#define HDMI_VFRMT_END                  127

/* extended video formats */
#define HDMI_VFRMT_3840x2160p30_16_9    (HDMI_VFRMT_END + 1)
#define HDMI_VFRMT_3840x2160p25_16_9    (HDMI_VFRMT_END + 2)
#define HDMI_VFRMT_3840x2160p24_16_9    (HDMI_VFRMT_END + 3)
#define HDMI_VFRMT_4096x2160p24_16_9    (HDMI_VFRMT_END + 4)
#define HDMI_EVFRMT_END                 HDMI_VFRMT_4096x2160p24_16_9

/* VESA DMT TIMINGS */
#define HDMI_VFRMT_1024x768p60_4_3      (HDMI_EVFRMT_END + 1)
#define HDMI_VFRMT_1280x1024p60_5_4     (HDMI_EVFRMT_END + 2)
#define HDMI_VFRMT_2560x1600p60_16_9    (HDMI_EVFRMT_END + 3)
#define VESA_DMT_VFRMT_END              HDMI_VFRMT_2560x1600p60_16_9
#define HDMI_VFRMT_MAX                  (VESA_DMT_VFRMT_END + 1)
#define HDMI_VFRMT_FORCE_32BIT          0x7FFFFFFF

extern int hdmi_read_edid_block(int block, u8 *edid_buf);
extern int hdmi_panel_power_on(struct hdmi_panel_config *panel_configs, bool mode);
extern int hdmi_panel_power_off(void);
extern int hdmi_power_off(int powermode);
extern int hdmi_panel_power_on_manual(void);
extern int hdmi_panel_update_panel_config(void);

//kyusuk.lee
extern int get_hdmi_power_status(void);

#endif	/*__HDMI_PANEL_H*/


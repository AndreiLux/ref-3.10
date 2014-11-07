#
# Makefile.in
#
# Makefile.in for HDMI
#
# Copyright (c)2011 LGE Co.,Ltd All rights reserved
#
# Author: Youngmin.Kim
# Revision History:
#    - Sep.01.2011 Created by Youngmin.Kim
#    - Sep.09.2012 Changed to HDMI by Hojung.Han
#
# mandatory files
HDMI_api_objs = api.o
HDMI_bsp_objs =  access.o board.o mutex.o system.o
HDMI_core_objs = audio.o audioParams.o control.o halAudioClock.o halAudioDma.o halAudioGenerator.o halAudioGpa.o halAudioHbr.o halAudioI2s.o halAudioSpdif.o
HDMI_core_objs += halColorSpaceConverter.o halFrameComposerAcp.o halFrameComposerAudio.o halFrameComposerAudioInfo.o halFrameComposerAvi.o halFrameComposerDebug.o
HDMI_core_objs += halFrameComposerGamut.o halFrameComposerGcp.o halFrameComposerIsrc.o halFrameComposerPackets.o halFrameComposerSpd.o halFrameComposerVideo.o
HDMI_core_objs += halFrameComposerVsd.o halIdentification.o halInterrupt.o halMainController.o halVideoGenerator.o halVideoPacketizer.o halVideoSampler.o
HDMI_core_objs += packets.o productParams.o video.o videoParams.o
HDMI_edid_objs = colorimetryDataBlock.o dtd.o edid.o halEdid.o hdmivsdb.o monitorRangeLimits.o shortAudioDesc.o shortVideoDesc.o speakerAllocationDataBlock.o videoCapabilityDataBlock.o
HDMI_hdcp_objs = halHdcp.o hdcp.o hdcpParams.o hdcpVerify.o
HDMI_phy_objs = halI2cMasterPhy.o halSourcePhy.o phy.o
HDMI_util_objs = bitOperation.o error.o log.o
HDMI_driver_objs = cliEdid.o cliParam.o cliRuntime.o command_line.o compliance.o
HDMI_test_objs = hdmi_main.o
HDMI_capture_objs = hcapture_test.o hcapture_util.o hdmi_tx.o i2c_rw.o


# list up objs
objs += $(HDMI_api_objs) $(HDMI_bsp_objs) $(HDMI_core_objs) $(HDMI_edid_objs) $(HDMI_hdcp_objs) $(HDMI_phy_objs) $(HDMI_util_objs) $(HDMI_driver_objs) $(HDMI_test_objs) $(HDMI_capture_objs)
HDMI_INC := __oal __oal/inc HDMI/api HDMI/bsp HDMI/core HDMI/doc HDMI/edid HDMI/hdcp HDMI/phy HDMI/util HDMI/capture
#HDMI_INC += DISPLAY_SUBSYSTEM/DIP DISPLAY_SUBSYSTEM/DIP/drv_displaycommon DISPLAY_SUBSYSTEM/DIP/drv_displaydevice DISPLAY_SUBSYSTEM/__dss_utils DISPLAY_SUBSYSTEM/DU

vpath %.c ../HDMI/api ../HDMI/bsp ../HDMI/core ../HDMI/doc ../HDMI/edid ../HDMI/hdcp ../HDMI/phy ../HDMI/util ../HDMI/capture
#vpath %.h $(top_dir)/__oal $(top_dir)/__oal/inc $(top_dir)//HDMI/api $(top_dir)/HDMI/bsp $(top_dir)/HDMI/core $(top_dir)/HDMI/doc $(top_dir)/HDMI/edid $(top_dir)/HDMI/hdcp $(top_dir)/HDMI/phy $(top_dir)/HDMI/util
#vpath %.h $(foreach dir,$(HDMI_INC),-I$(top_dir)/$(dir))

CFLAGS_HDMI_INC = $(foreach dir,$(HDMI_INC),-I$(top_dir)/$(dir))

CCFLAGS += -DHDMI_ENABLED $(CFLAGS_HDMI_INC)

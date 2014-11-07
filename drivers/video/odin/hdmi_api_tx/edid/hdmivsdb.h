/*
 * hdmivsdb.h
 *
 *  Created on: Jul 21, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HDMIVSDB_H_
#define HDMIVSDB_H_

#include "../util/types.h"

/** For detailed handling of this structure, refer to
documentation of the functions */
typedef struct
{
	u16 mphysicaladdress;

	int msupportsai;

	int mdeepcolor30;

	int mdeepcolor36;

	int mdeepcolor48;

	int mdeepcolory444;

	int mdvidual;

	u8 mmaxtmdsclk;

	u16 mvideolatency;

	u16 maudiolatency;

	u16 minterlacedvideolatency;

	u16 minterlacedaudiolatency;

	u32 mid;

	u8 mlength;

	int mvalid;

} hdmivsdb_t;

void hdmivsdb_reset(hdmivsdb_t *vsdb);

/**
 * Parse an array of data to fill the hdmivsdb_t data strucutre
 * @param *vsdb pointer to the structure to be filled
 * @param *data pointer to the 8-bit data type array to be parsed
 * @return Success, or error code:
 * @return 1 - array pointer invalid
 * @return 2 - Invalid datablock tag
 * @return 3 - Invalid minimum length
 * @return 4 - HDMI IEEE registration identifier not valid
 * @return 5 - Invalid length - latencies are not valid
 * @return 6 - Invalid length - Interlaced latencies are not valid
 */
int hdmivsdb_parse(hdmivsdb_t *vsdb, u8 * data);
/**
 * @param *vsdb pointer to the structure holding the information
 * @return TRUE if sink supports 10bit/pixel deep color mode
 */
int hdmivsdb_getdeepcolor30(hdmivsdb_t *vsdb);
/**
 * @param *vsdb pointer to the structure holding the information
 * @return TRUE if sink supports 12bit/pixel deep color mode
 */
int hdmivsdb_getdeepcolor36(hdmivsdb_t *vsdb);
/**
 * @param *vsdb pointer to the structure holding the information
 * @return TRUE if sink supports 16bit/pixel deep color mode
 */
int hdmivsdb_getdeepcolor48(hdmivsdb_t *vsdb);
/**
 * @param *vsdb pointer to the structure holding the information
 * @return TRUE if sink supports YCC 4:4:4 in deep color modes
 */
int hdmivsdb_getdeepcolory444(hdmivsdb_t *vsdb);

/**
 * @param *vsdb pointer to the structure holding the information
 * @return TRUE if sink supports at least one function that uses
 * information carried by the ACP, ISRC1, or ISRC2 packets
 */
int hdmivsdb_getsupportsai(hdmivsdb_t *vsdb);

/**
 * @param *vsdb pointer to the structure holding the information
 * @return TRUE if Sink supports DVI dual-link operation
 */
int hdmivsdb_getdvidual(hdmivsdb_t *vsdb);
/**
 * @param *vsdb pointer to the structure holding the information
 * @return maximum TMDS clock rate supported [divided by 5MHz]
 */
u8 hdmivsdb_getmaxtmdsclk(hdmivsdb_t *vsdb);
/**
 * @param *vsdb pointer to the structure holding the information
 * @return the amount of video latency when receiving progressive video formats
 */
u16 hdmivsdb_getpvideolatency(hdmivsdb_t *vsdb);
/**
 * @param *vsdb pointer to the structure holding the information
 * @return the amount of audio latency when receiving progressive video formats
 */
u16 hdmivsdb_getpaudiolatency(hdmivsdb_t *vsdb);
/**
 * @param *vsdb pointer to the structure holding the information
 * @return the amount of audio latency when receiving interlaced video formats
 */
u16 hdmivsdb_getiaudiolatency(hdmivsdb_t *vsdb);
/**
 * @param *vsdb pointer to the structure holding the information
 * @return the amount of video latency when receiving interlaced video formats
 */
u16 hdmivsdb_getivideolatency(hdmivsdb_t *vsdb);
/**
 * @param *vsdb pointer to the structure holding the information
 * @return Physical Address extracted from the VSDB
 */
u16 hdmivsdb_getphysicaladdress(hdmivsdb_t *vsdb);
/**
 * @param *vsdb pointer to the structure holding the information
 * @return the 24-bit IEEE Registration Identifier of 0x000C03,
 * a value belonging to HDMI Licensing
 */
u32 hdmivsdb_getid(hdmivsdb_t *vsdb);
/**
 * @param *vsdb pointer to the structure holding the information
 * @return the length of the HDMI VSDB
 * (hdmivsdb_t *vsdb, without counting the first byte in, where the TAG is)
 */
u8 hdmivsdb_getlength(hdmivsdb_t *vsdb);
/**
 * @param *vsdb pointer to the structure to be edited
 * @param id the 24-bit IEEE Registration Identifier of  the VSDB.
 * where 0x000C03 belonging to HDMI Licensing.
 */
void hdmivsdb_setid(hdmivsdb_t *vsdb, u32 id);
/**
 * @param *vsdb pointer to the structure to be edited
 * @param length of the HDMI VSDB datablock
 */
void hdmivsdb_setlength(hdmivsdb_t *vsdb, u8 length);

#endif /* HDMIVSDB_H_ */

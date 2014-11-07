/*
 * dtd.h
 *
 *  Created on: Jul 5, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef DTD_H_
#define DTD_H_

#include "../util/types.h"

/**
 * @file
 * For detailed handling of this structure,
 * refer to documentation of the functions
 */
typedef struct
{
	/** VIC code */
	u8 mcode;

	u16 mpixelrepetitioninput;

	u16 mpixelclock;

	u8 minterlaced;

	u16 mhactive;

	u16 mhblanking;

	u16 mhborder;

	u16 mhimagesize;

	u16 mhsyncoffset;

	u16 mhsyncpulsewidth;

	u8 mhsyncpolarity;

	u16 mvactive;

	u16 mvblanking;

	u16 mvborder;

	u16 mvimagesize;

	u16 mvsyncoffset;

	u16 mvsyncpulsewidth;

	u8 mvsyncpolarity;

} dtd_t;
/**
 * Parses the Detailed Timing Descriptor.
 * Encapsulating the parsing process
 * @param dtd pointer to dtd_t strucutute for the information to be save in
 * @param data a pointer to the 18-byte structure to be parsed.
 * @return TRUE if success
 */
int dtd_parse(dtd_t *dtd, u8 data[18]);

/**
 * @param dtd pointer to dtd_t strucutute for the information to be save in
 * @param code the CEA 861-D video code.
 * @param refreshRate the specified vertical refresh rate.
 * @return TRUE if success
 */
int dtd_fill(dtd_t *dtd, u8 code, u32 refreshRate);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the CEA861-D code if the DTD is listed in the spec
 * @return < 0 when the code has an undefined DTD
 */
u8 dtd_getcode(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the input pixel reptition
 */
u16 dtd_getpixelrepetitioninput(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the pixel clock rate of the DTD
 */
u16 dtd_getpixelclock(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return 1 if the DTD is of an interlaced viedo format
 */
u8 dtd_getinterlaced(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the horizontal active pixels (addressable video)
 */
u16 dtd_gethactive(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the horizontal blanking pixels
 */
u16 dtd_gethblanking(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the horizontal border in pixels
 */
u16 dtd_gethborder(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the horizontal image size in mm
 */
u16 dtd_gethimagesize(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the horizontal sync offset (front porch) from
 * blanking start to start of sync in pixels
 */
u16 dtd_gethsyncoffset(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the horizontal sync polarity
 */
u8 dtd_gethsyncpolarity(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the horizontal sync pulse width in pixels
 */
u16 dtd_gethsyncpulsewidth(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the vertical active pixels (addressable video)
 */
u16 dtd_getvactive(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the vertical border in pixels
 */
u16 dtd_getvblanking(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the horizontal image size
 */
u16 dtd_getvborder(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the vertical image size in mm
 */
u16 dtd_getvimagesize(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the vertical sync offset (front porch) from blanking start to start
 *  of sync in lines
 */
u16 dtd_getvsyncoffset(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the vertical sync polarity
 */
u8 dtd_getvsyncpolarity(const dtd_t *dtd);
/**
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @return the vertical sync pulse width in line
 */
u16 dtd_getvsyncpulsewidth(const dtd_t *dtd);
/**
 * @param dtd1 pointer to dtd_t structure to be compared
 * @param dtd2 pointer to dtd_t structure to be compared to
 * @return TRUE if the two DTDs are identical
 * @note: although DTDs may have different refresh rates, and hence
 * pixel clocks, they can still be identical
 */
int dtd_isequal(const dtd_t *dtd1, const dtd_t *dtd2);
/**
 * Set the desired pixel repetition
 * @param dtd pointer to dtd_t strucutute where the information is held
 * @param value of pixel repetitions
 * @return TRUE if successful
 * @note for CEA video modes, the value has to fall within the
 * defined range, otherwise the method will fail.
 */
int dtd_setpixelrepetitioninput(dtd_t *dtd, u16 value);

#endif /* DTD_H_ */

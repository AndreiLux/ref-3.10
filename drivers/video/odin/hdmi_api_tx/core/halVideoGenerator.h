/*
 * @file:halVideoGenerator.h
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALVIDEOGENERATOR_H_
#define HALVIDEOGENERATOR_H_

#include "../util/types.h"

void halvideogenerator_swreset(u16 baseAddr, u8 bit);

void halvideogenerator_ycc(u16 baseAddr, u8 bit);

void halvideogenerator_ycc422(u16 baseAddr, u8 bit);

void halvideogenerator_vblankosc(u16 baseAddr, u8 bit);

void halvideogenerator_colorincrement(u16 baseAddr, u8 bit);

void halvideogenerator_interlaced(u16 baseAddr, u8 bit);

void halvideogenerator_vsyncpolarity(u16 baseAddr, u8 bit);

void halvideogenerator_hsyncpolarity(u16 baseAddr, u8 bit);

void halvideogenerator_dataenablepolarity(u16 baseAddr, u8 bit);

void halvideogenerator_colorresolution(u16 baseAddr, u8 value);

void halvideogenerator_pixelrepetitioninput(u16 baseAddr, u8 value);

void halvideogenerator_hactive(u16 baseAddr, u16 value);

void halvideogenerator_hblank(u16 baseAddr, u16 value);

void halvideogenerator_hsyncedgedelay(u16 baseAddr, u16 value);

void halvideogenerator_hsyncpulsewidth(u16 baseAddr, u16 value);

void halvideogenerator_vactive(u16 baseAddr, u16 value);

void halvideogenerator_vblank(u16 baseAddr, u16 value);

void halvideogenerator_vsyncedgedelay(u16 baseAddr, u16 value);

void halvideogenerator_vsyncpulsewidth(u16 baseAddr, u16 value);

void halvideogenerator_enable3d(u16 baseAddr, u8 bit);

void halvideogenerator_structure3d(u16 baseAddr, u8 value);

void halvideogenerator_writestart(u16 baseAddr, u16 value);

void halvideogenerator_writedata(u16 baseAddr, u8 value);

u16 halvideogenerator_writecurrentaddr(u16 baseAddr);

u8 halvideogenerator_writecurrentbyte(u16 baseAddr);

#endif /* HALVIDEOGENERATOR_H_ */

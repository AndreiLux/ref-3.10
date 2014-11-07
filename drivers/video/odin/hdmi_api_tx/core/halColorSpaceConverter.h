/*
 * halColorSpaceConverter.h
 *
 *  Created on: Jun 30, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALCOLORSPACECONVERTER_H_
#define HALCOLORSPACECONVERTER_H_

#include "../util/types.h"

void halcolorspaceconverter_interpolation(u16 baseAddr, u8 value);

void halcolorspaceconverter_decimation(u16 baseAddr, u8 value);

void halcolorspaceconverter_colordepth(u16 baseAddr, u8 value);

void halcolorspaceconverter_scalefactor(u16 baseAddr, u8 value);

void halcolorspaceconverter_coefficienta1(u16 baseAddr, u16 value);

void halcolorspaceconverter_coefficienta2(u16 baseAddr, u16 value);

void halcolorspaceconverter_coefficienta3(u16 baseAddr, u16 value);

void halcolorspaceconverter_coefficienta4(u16 baseAddr, u16 value);

void halcolorspaceconverter_coefficientb1(u16 baseAddr, u16 value);

void halcolorspaceconverter_coefficientb2(u16 baseAddr, u16 value);

void halcolorspaceconverter_coefficientb3(u16 baseAddr, u16 value);

void halcolorspaceconverter_coefficientb4(u16 baseAddr, u16 value);

void halcolorspaceconverter_coefficientc1(u16 baseAddr, u16 value);

void halcolorspaceconverter_coefficientc2(u16 baseAddr, u16 value);

void halcolorspaceconverter_coefficientc3(u16 baseAddr, u16 value);

void halcolorspaceconverter_coefficientc4(u16 baseAddr, u16 value);

#endif /* HALCOLORSPACECONVERTER_H_ */

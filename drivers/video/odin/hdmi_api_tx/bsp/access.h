/*
 * access.h
 *
 *  Created on: Jun 25, 2010
 *
 * Synopsys Inc.
 * SG DWC PT02
 */
/**
 * @file 
 *      Access methods mask the the register access
 *      it accesses register using 16-bit addresses
 *      and 8-bit data
 *      @note: at least access_Write and access_Read should be re-implemented
 *      @note functions in this module are critical sections and should not be
 *      	interrupted
 *      @note: this implementation protects critical sections using muteces.
 *     		make sure critical section are protected from race conditions,
 *  	   	particularly when interrupts are enabled.
 */

#ifndef ACCESS_H_
#define ACCESS_H_

#include "../util/types.h"
/**
 *Initialize communications with development board
 *@param baseAddr pointer to the address of the core on the bus
 *@return TRUE  if successful.
 */
int access_initialize(u8 * baseAddr);
/**
 *Close communications with development board and free resources
 *@return TRUE  if successful.
 */
int access_standby(void);
/**
 *Read the contents of a register
 *@param addr of the register
 *@return 8bit byte containing the contents
 */
u8 access_corereadbyte(u16 addr);
/**
 *Read several bits from a register
 *@param addr of the register
 *@param shift of the bit from the beginning
 *@param width or number of bits to read
 *@return the contents of the specified bits
 */
u8 access_coreread(u16 addr, u8 shift, u8 width);
/**
 *Write a byte to a register
 *@param data to be written to the register
 *@param addr of the register
 */
void access_corewritebyte(u8 data, u16 addr);
u8 simualtion_readbyte(u32 addr);
void simualtion_writebyte(u32 addr, u8 data);
/**
 *Write to several bits in a register
 *@param data to be written to the required part
 *@param addr of the register
 *@param shift of the bits from the beginning
 *@param width or number of bits to written to
 */
void access_corewrite(u8 data, u16 addr, u8 shift, u8 width);

u8 access_read(u16 addr);

void access_write(u8 data, u16 addr);

#endif /* ACCESS_H_ */

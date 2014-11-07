/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2014 by LG Electronics Inc.
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

#ifndef __TLODIN_DCI_H__
#define __TLODIN_DCI_H__

typedef uint32_t dciCommandId_t;
typedef uint32_t dciResponseId_t;
typedef uint32_t dciReturnCode_t;

/* Responses have bit 31 set */
#define RSP_ID_MASK (1U << 31)
#define RSP_ID(cmdId) (((uint32_t)(cmdId)) | RSP_ID_MASK)
#define IS_CMD(cmdId) ((((uint32_t)(cmdId)) & RSP_ID_MASK) == 0)
#define IS_RSP(cmdId) ((((uint32_t)(cmdId)) & RSP_ID_MASK) == RSP_ID_MASK)

/**
 * Termination codes
 */
#define EXIT_ERROR  ((uint32_t)(-1))

/**
 * Return codes of driver commands.
 */
#define RET_OK                    0
#define RET_ERR_UNKNOWN_CMD       1
#define RET_ERR_NOT_SUPPORTED     2
#define RET_ERR_INTERNAL_ERROR    3
/* ... add more error codes when needed */


#define FID_DR_ALLOC_SEC_BUF		1000
#define FID_DR_FREE_SEC_BUF		1001
#define FID_DR_MAP_ION_BUF		1002
#define FID_DR_UNMAP_ION_BUF		1003
#define FID_DR_INIT_SEC_PORT		1010
#define FID_DR_FREE_SEC_PORT		1011


/**
 * DCI command header.
 */
typedef struct {
    dciCommandId_t commandId; /**< Command ID */
} dciCommandHeader_t;

/**
 * DCI response header.
 */
typedef struct{
    dciResponseId_t     responseId; /**< Response ID (must be command ID | RSP_ID_MASK )*/
    dciReturnCode_t     returnCode; /**< Return code of command */
} dciResponseHeader_t;

/**
 * command message.
 *
 * @param len Lenght of the data to process.
 * @param data Data to be processed
 */
typedef struct {
    dciCommandHeader_t  header;     /**< Command header */
    uint32_t            len;        /**< Length of data to process */
} cmd_t;


/**
 * Response structure
 */
typedef struct {
    dciResponseHeader_t header;     /**< Response header */
    uint32_t            len;
    uint32_t		returnVal;
} rsp_t;

typedef struct {
	uint32_t	addr;
	uint32_t	len;
	uint32_t	type;
} secure_buf_t;

typedef struct {
	dciCommandHeader_t header;
	uint32_t data_len;
	uint32_t pdata;
} cmdmap_t;

/**
 * DCI message data.
 */
typedef struct {
	union {
		cmd_t command;
		rsp_t response;
		cmdmap_t cmdmap;
	};
} dciMessage_t;

typedef struct {
	dciMessage_t message;	/* DCI message */
} dci_t;


#endif // __TLODIN_DCI_H__

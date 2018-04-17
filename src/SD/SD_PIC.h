/* 
 * File:   SD_PIC.h
 * Author: Tyler Gamvrelis
 *
 * Created on July 21, 2017, 3:52 PM
 */

#ifndef SD_PIC_H
#define	SD_PIC_H

/********************************** Includes **********************************/
#include <xc.h>
#include "../SPI/SPI_PIC.h"

/*********************************** Macros ***********************************/
/* Equivalent to a software reset of the SD card. In this state, only CMD8,
 * ACMD41, CMD58 and CMD59 are valid. */
#define __GO_IDLE_STATE()   while(SD_Command(CMD0, 0) != R1_READY_STATE);

/* Macro for smoothly entering SD card functions post-initialization */
#define __SD_START(){\
    __MSSP_ENABLE();\
    CS_SD = 0;\
}

/* Macro for smoothly exiting SD card functions. */
#define __SD_STOP(){\
    CS_SD = 1;\
    __MSSP_DISABLE();\
}

/*********************************** Defines **********************************/
#define CS_SD               LATEbits.LATE2
#define TRIS_CS_SD          TRISEbits.TRISE2

#define PORT_DAT0           PORTCbits.RC4

#define TYPE_SDSC           0
#define TYPE_SDHC_SDXC      1
#define TYPE_MMC            2

/********************************** Constants *********************************/
const unsigned char CMD0 = 0; // GO_IDLE_STATE
const unsigned char CMD0CRC = 0x95;
const unsigned char CMD1 = 1; // SEND_OP_COND
const unsigned char CMD8 = 8; // SEND_IF_COND
const unsigned char CMD8CRC = 0x87;
const unsigned char CMD9 = 9; // SEND_CSD
const unsigned char CMD10 = 10; // SEND_CID
const unsigned char CMD12 = 12; // STOP_TRANSMISSION
const unsigned char CMD13 = 13; // SEND_STATUS
const unsigned char CMD16 = 16; // SET_BLOCKLEN
const unsigned char CMD17 = 17; // READ_SINGLE_BLOCK (arg: mem addr)
const unsigned char CMD18 = 18; // READ_MULTIPLE_BLOCK (arg: mem addr)
const unsigned char CMD24 = 24; // WRITE_BLOCK (arg: mem addr)
const unsigned char CMD25 = 25; // WRITE_MULTIPLE_BLOCK (arg: mem addr)
const unsigned char CMD32 = 32; // ERASE_WR_BLOCK_START_ADDR (arg: mem addr)
const unsigned char CMD33 = 33; // ERASE_WR_BLOCK_END_ADDR (arg: mem addr)
const unsigned char CMD38 = 38; // ERASE (arg: stuff bits)
const unsigned char CMD55 = 55; // APP_CMD
const unsigned char CMD58 = 58; // READ_OCR
const unsigned char ACMD22 = 22; // SEND_NUM_WR_BLOCKS
const unsigned char ACMD23 = 23; // SET_WR_BLK_ERASE_COUNT (arg[22:0] # blks)
const unsigned char ACMD41 = 41; // SD_SEND_OP_COND
const unsigned char R1_READY_STATE = 0;
const unsigned char R1_IDLE_STATE = 1;
const unsigned char R1_ILLEGAL_COMMAND = 4;
const unsigned char START_BLOCK = 0xFE; // Used for WRITE_BLOCK, READ_SINGLE_BLOCK, READ_MULTIPLE_BLOCK
const unsigned char START_BLOCK_TOKEN = 0xFC; // Used for WRITE_MULTIPLE_BLOCK
const unsigned char STOP_TRAN = 0xFD; // Used to end multiple block writes

/************************************ Types ***********************************/
typedef struct{
    unsigned char SDversion; /* Version of the SD specification the card complies to. */
    unsigned char Type; /* Type of card: SDSC, SDHC/SDXC, MMC. */
    unsigned char MID; /* Manufacturer ID. */
    unsigned short OID; /* OEM/Application ID. */
    unsigned long PHML; /* Product name low bits. */
    unsigned char PHMH; /* Product name high bits. */
    unsigned char PRV; /* Product revision. */
    unsigned long PSN;   /* Product serial number. */
    unsigned short MDT; /* Manufacturing date. */
    unsigned char CRC; /* CRC7 checksum. */
    unsigned short blockSize; /* Size of an addressable data block, in bytes. */
    unsigned long numBlocks; /* Number of block addresses in card. */
    double size; /* Card capacity in MB. */
    
    unsigned char init; // 1 if initialization succeeded, 0 otherwise
    
    struct{
        unsigned long lastBlockWritten; // Updated in all write functions
        unsigned long MBW_startBlock; // For multiple block writes
        unsigned char MBW_flag_first; // For multiple block writes
    }write;
    
    struct{
        unsigned long lastBlockRead; // Updated in all read functions
        unsigned long MBR_startBlock; // For multiple block reads
        unsigned char MBR_flag_first; // For multiple block reads
    }read;
}SDCard_t;

/**************************** External variables ******************************/
SDCard_t SDCard;

/****************************** Public Interfaces *****************************/
/* Low-level protocol. */
void SD_SendDummyBytes(unsigned char numBytes);
unsigned char SD_Command(unsigned char cmd, unsigned long arg);
unsigned char SD_ACMD(unsigned char cmd, unsigned long args);

/* Write functions. */
unsigned char SD_SingleBlockWrite(unsigned long block, unsigned char* arr);

void SD_MBW_Start(unsigned long startBlock, unsigned long numBlocks);
unsigned char SD_MBW_Send(unsigned char* arrWrite);
void SD_MBW_Stop(void);

/* Read functions. */
unsigned char SD_SingleBlockRead(unsigned long block, unsigned char* buf);

unsigned char SD_MBR_Start(unsigned long startBlock);
void SD_MBR_Receive(unsigned char* bufReceive);
void SD_MBR_Stop(void);

/* Erasing. */
void SD_EraseBlocks(unsigned long firstBlock, unsigned long lastBlock);

/* Initialization. */
void initSD(void);

#endif	/* SD_PIC_H */


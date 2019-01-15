/**
 * @file
 * @author Tyler Gamvrelis
 *
 * Created on July 21, 2017, 3:52 PM
 *
 * @defgroup SD
 * @brief SD card driver that uses SPI to transfer bits
 * @{
 */

#ifndef SD_PIC_H
#define SD_PIC_H

/********************************* Includes **********************************/
#include <xc.h>
#include "SPI_PIC.h"

/********************************** Macros ***********************************/
#define CS_SD      LATEbits.LATE2   /**< SD card chip select                 */
#define TRIS_CS_SD TRISEbits.TRISE2 /**< TRIS for the chip select pin        */
#define PORT_DAT0  PORTCbits.RC4    /**< Pin used for receiving SD card data */

/**
 * @brief Equivalent to a software reset of the SD card. In this state, only
 *        CMD8, ACMD41, CMD58 and CMD59 are valid
 */
#define sd_go_idle_state() while(SD_Command(CMD0, 0) != R1_READY_STATE);

/** @brief Smoothly starts SD card usage, post-initialization */
#define sd_start(){\
    mssp_enable();\
    CS_SD = 0;\
}

/** @brief Smoothly stops SD card usage */
#define sd_stop(){\
    CS_SD = 1;\
    mssp_disable();\
}

/******************************** Constants **********************************/
extern const unsigned char CMD0;               /**< GO_IDLE_STATE */
extern const unsigned char CMD0CRC;            /**< CRC for CMD0 -- needed during initialization */
extern const unsigned char CMD1;               /**< SEND_OP_COND */
extern const unsigned char CMD8;               /**< SEND_IF_COND */
extern const unsigned char CMD8CRC;            /**< CRC for CMD0 -- needed during initialization */
extern const unsigned char CMD9;               /**< SEND_CSD */
extern const unsigned char CMD10;              /**< SEND_CID */
extern const unsigned char CMD12;              /**< STOP_TRANSMISSION */
extern const unsigned char CMD13;              /**< SEND_STATUS */
extern const unsigned char CMD16;              /**< SET_BLOCKLEN */
extern const unsigned char CMD17;              /**< READ_SINGLE_BLOCK (arg: mem addr) */
extern const unsigned char CMD18;              /**< READ_MULTIPLE_BLOCK (arg: mem addr) */
extern const unsigned char CMD24;              /**< WRITE_BLOCK (arg: mem addr) */
extern const unsigned char CMD25;              /**< WRITE_MULTIPLE_BLOCK (arg: mem addr) */
extern const unsigned char CMD32;              /**< ERASE_WR_BLOCK_START_ADDR (arg: mem addr) */
extern const unsigned char CMD33;              /**< ERASE_WR_BLOCK_END_ADDR (arg: mem addr) */
extern const unsigned char CMD38;              /**< ERASE (arg: stuff bits) */
extern const unsigned char CMD55;              /**< APP_CMD */
extern const unsigned char CMD58;              /**< READ_OCR */
extern const unsigned char ACMD22;             /**< SEND_NUM_WR_BLOCKS */
extern const unsigned char ACMD23;             /**< SET_WR_BLK_ERASE_COUNT (arg[22:0] # blks) */
extern const unsigned char ACMD41;             /**< SD_SEND_OP_COND */
extern const unsigned char R1_READY_STATE;     /**< R1 response "ready" */
extern const unsigned char R1_IDLE_STATE;      /**< R1 response "idle"  */
extern const unsigned char R1_ILLEGAL_COMMAND; /**< R1 response "illegal command" */
extern const unsigned char START_BLOCK;        /**< Used for WRITE_BLOCK, READ_SINGLE_BLOCK, READ_MULTIPLE_BLOCK */
extern const unsigned char START_BLOCK_TOKEN;  /**< Used for WRITE_MULTIPLE_BLOCK */
extern const unsigned char STOP_TRAN;          /**< Used to end multiple block writes ("stop transfer") */

/********************************** Types ************************************/
/**
 * @brief Possible SD card types. They each require slightly different
 *        treatment for initialization, reading, writing, etc
 */
typedef enum{
    TYPE_SDSC = 0,      /**< Standard capacity */
    TYPE_SDHC_SDXC = 1, /**< eXtra capacity    */
    TYPE_MMC = 2        /**< MultiMediaCard    */
}sd_card_types_e;

/** @brief SD card object */
typedef struct{
    unsigned char SDversion;  /**< Version of the SD specification the card complies to */
    sd_card_types_e Type;     /**< Type of card: SDSC, SDHC/SDXC, MMC */
    unsigned char MID;        /**< Manufacturer ID */
    unsigned short OID;       /**< OEM/Application ID */
    unsigned long PHML;       /**< Product name low bits */
    unsigned char PHMH;       /**< Product name high bits */
    unsigned char PRV;        /**< Product revision */
    unsigned long PSN;        /**< Product serial number */
    unsigned short MDT;       /**< Manufacturing date */
    unsigned char CRC;        /**< CRC7 checksum */
    unsigned short blockSize; /**< Size of an addressable data block, in bytes */
    unsigned long numBlocks;  /**< Number of block addresses in card */
    double size;              /**< Card capacity in MB */
    unsigned char init; /**< 1 if initialization succeeded, 0 otherwise */
    
    /** @brief State information used by write functions */
    struct{
        unsigned long lastBlockWritten; /**< Updated in all write functions */
        unsigned long MBW_startBlock;   /**< For multiple block writes */
        unsigned char MBW_flag_first;   /**< For multiple block writes */
    }write;
    
    /** @brief State information used by read functions */
    struct{
        unsigned long lastBlockRead;  /**< Updated in all read functions */
        unsigned long MBR_startBlock; /**< For multiple block reads */
        unsigned char MBR_flag_first; /**< For multiple block reads */
    }read;
}SDCard_t;

/***************************** Public Variables ******************************/
extern SDCard_t SDCard;

/************************ Public Function Prototypes *************************/
/**
 * @brief Sends dummy bytes of 0xFF to the SD card. Used to update the flash
 *        controller's state machine
 * @param numBytes The number of dummy bytes to send
 */
void SD_SendDummyBytes(unsigned char numBytes);

/**
 * @brief Sends a command to the SD card
 * @param cmd The command code to issue
 * @param arg The command argument (32-bit)
 * @return The SD card's response code
 */
unsigned char SD_Command(unsigned char cmd, unsigned long arg);

/**
 * @brief Sends an application specific command (ACMD) to the SD card
 * @param cmd The command code to issue
 * @param args The command argument (32-bit)
 * @return The SD card's response code
 */
unsigned char SD_ACMD(unsigned char cmd, unsigned long args);

/**
 * @brief Initiates a 512 byte write into the specified block, starting with
 *        arr[0] and ending with arr[511]
 * @param block Block number in the SD card memory to write to
 * @param arr Pointer to the array of bytes to be written
 * @return 1 if successful, 0 otherwise
 */
unsigned char SD_SingleBlockWrite(unsigned long block, unsigned char* arr);

/**
 * @brief Initiates a multiple block write beginning at the block startBlock,
 *        and ending at the block numBlocks later
 * @param startBlock Block number in SD card memory to begin the write
 * @param numBlocks Number of blocks to be written to
 */
void SD_MBW_Start(unsigned long startBlock, unsigned long numBlocks);

/**
 * @brief Sends data as part of a multiple block read
 * @pre The precondition is the multiple block write must have been properly
 *      initialized by calling SD_MBW_Start before this function. Note that
 *      between the time that SD_MBW_Start and this function are called, no
 *      other SD card functions should be called. Otherwise, the pre-erase 
 *      configured in SD_MBW_Start may be cleared
 * @param arrWrite Pointer to the array of bytes to be written
 * @return 1 if successful, 0 otherwise
 */
unsigned char SD_MBW_Send(unsigned char* arrWrite);

/**
 * @brief Stops a multiple block write
 * @pre The precondition is that SD_MBW_Send called properly at least once
 */
void SD_MBW_Stop(void);

/**
 * @brief Initiates a 512 byte read from the specified block, block, into the
 *        array of bytes, buf
 * @param block Block number in SD card memory to begin the read
 * @param buf Pointer to the array of bytes that data read from the card is to
 *        be stored
 * @return 1 if successful, 0 otherwise
 */
unsigned char SD_SingleBlockRead(unsigned long block, unsigned char* buf);

/**
 * @brief Initiates a read of the SD card, starting with the sector at address
 *        startBlock
 * @param startBlock Block number in SD card memory to begin the read
 * @return 1 if successful, 0 otherwise
 */
unsigned char SD_MBR_Start(unsigned long startBlock);

/**
 * @brief Receives data as part of a multiple block read
 * @pre The precondition is that a multiple block read must have been properly
 *      initialized by calling SD_MBR_Start before this function
 * @param bufReceive Pointer to the array that will store the data
 */
void SD_MBR_Receive(unsigned char* bufReceive);

/**
 * @brief Stops a multiple block read
 * @pre The precondition is that SD_MBR_Receive must have been called properly
 *      at least once
 */
void SD_MBR_Stop(void);

/**
 * @brief Erases all the blocks between firstBlock and lastBlock, inclusive.
 *        This may take some time, during which DAT0 will be held low. Thus,
 *        users may choose to poll the SD card's response before attempting
 *        other SD card operations
 * @param firstBlock Address of the first block to be erased
 * @param lastBlock Address of the last block to be erased
 */
void SD_EraseBlocks(unsigned long firstBlock, unsigned long lastBlock);

/**
 * @brief This function performs the length SD card initialization command
 *        sequence
 */
void initSD(void);

/**
 * @}
 */

#endif	/* SD_PIC_H */
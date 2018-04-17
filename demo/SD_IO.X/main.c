/*
 * File:   main.c
 * Author: Tyler Gamvrelis
 * 
 *  Created on July 10, 2017, 10:27 AM
 * 
 * Description: Demonstration of basic SD card I/O capabilities, specifically,
 *              reading/writing single/multiple blocks and erasing.
 * 
 * Preconditions:
 *   1. SD card is properly situated in its socket (indicated by CARD_IN LED)
 *   2. Jumper JP_SD is open (i.e. not shorted)
 */


#include <xc.h>
#include "../../src/PIC18F4620/machineConfig.h"
#include "../../src/PIC18F4620/configBits.h"
#include "../../src/SD/SD_PIC.h"
#include "../../src/CharacterLCD/lcd.h"

/* Declare data buffer. Note that arrays larger than 1 RAM bank (256 bytes) must
 * either be:
 *   1. Global variables
 *   2. Declared with the keyword "static" (if declared within a function)
 * This is done so that the array is placed in general memory instead of the
 * compiled stack (source: XC8 user guide).
 */
unsigned char writeBuffer[512] = {0};

unsigned char average(unsigned char* array, unsigned short numElements){
    /* Computes the average of the first numElements elements in an array.
     *
     * Arguments: array, pointer to the start of the array of bytes whose 
     *              elements are to be used in the computation
     *            numElements, the number of bytes to be used in the computation
     * 
     * Returns: the average of the first numElements elements in the array
     */
    
    unsigned short n = 0;
    unsigned long sum = 0;
    while(n < numElements){
        sum += array[n];
        n++;
    }
    return sum / numElements;
}

void main(void) {
    /* Configure pins, SFRs, and on-board modules. */
    machineConfig();
    
    /* Initialize LCD. */
    initLCD();
    __lcd_display_control(1, 0, 0);
    
    /* Initialize SD card. */
    initSD();
    if(SDCard.init){
        printf("Init success!");
    }
    else{
        printf("Init failed");
        while(1);
    }
    __delay_ms(1000);
    
    /* Initialize read buffer to demonstrate alternate method of large array
     * declaration. Defining a static variable in a function makes it persistent
     * after the function returns. */
    static unsigned char readBuffer[512] = {0};
    
    unsigned long i; // Declare counting variable
    
    // <editor-fold defaultstate="collapsed" desc="WRITING TO SD CARD">
    /***************************************************************************
     *                                                                         *
     *                          SINGLE BLOCK WRITE                             *
     *                                                                         *
     * Description: Writes 512 bytes to the block specified in the SD card.    *
     **************************************************************************/
    /* Erase the first sector (sector 0). */
    __SD_START(); // Start SPI and clear SD card chip select
    SD_EraseBlocks(0, 0);
    while(spiReceive() != 0xFF){  continue;   }
    
    /* Store 0-255 into the array for single block write, twice. */
    for(i = 0; i < 512; i++){
        writeBuffer[i] = i & 0xFF;
    }
    
    /* Single block write into sector 0. */
    while(!SD_SingleBlockWrite(0, writeBuffer));
    __lcd_clear();__lcd_home();
    printf("Single block");
    __lcd_newline();
    printf("write finished");
    __delay_ms(1000);

    /***************************************************************************
     *                                                                         *
     *                        MULTIPLE BLOCK WRITE                             *
     *                                                                         *
     * Description: Writes data to consecutive blocks in the SD card           *
     * Occurs in 3 stages:                                                     *
     *      1. SD_MBR_Start --> Indicates to SD card that a multiple block     *
     *         write is to begin. The starting block is the first argument to  *
     *         this function, and the number of blocks to be written is the    *
     *         second. We indicate the number of blocks to be written so that  *
     *         the SD card can "pre-erase" them, which increases efficiency.   *
     *      2. SD_MBW_Write --> Sends a block (512) bytes from the SD card.    *
     *         This function can be called in a loop to write many blocks, and *
     *         do something else (e.g. acquire data) between each write.       *
     *      3. SD_MBW_Stop --> Stops the multiple block write. The SD card can *
     *         respond to other commands after this.                           *
     **************************************************************************/
    /* Multiple block write (MBW) into sectors 1 through 999. */
    unsigned char firstBlock = 1;
    unsigned short numWrites = 1000;
    
    /* Prepare a different array to write for MBW. */
    for(i = 0; i < 512; i++){
        writeBuffer[i] = 0x34; // Eaach element is 0x34 = d52
    }
    
    __lcd_clear();__lcd_home();
    printf("MBW Start...");
    SD_MBW_Start(firstBlock, numWrites); // Start the MBW

    for(i = 0; i < numWrites; i++){
        /* Send the buffer. */
        if(!SD_MBW_Send(writeBuffer)){
            break; // If SD_MBW_Send returns an error, stop transmission
        }
        if(i % 100 == 0){
            /* Print result to character LCD every 100 times. */
            lcd_set_cursor(0, 1);
            printf("Done: %lu", i);
        }
    }
    SD_MBW_Stop(); // Stop the MBW
    __SD_STOP(); // Stop SPI and deselect SD card
    __lcd_clear();
    __lcd_home();
    printf("Done MBW!");
    __lcd_newline();
    printf("Done %lu", i);
    __delay_ms(1000);
    
    // </editor-fold>
    
    // <editor-fold defaultstate="collapsed" desc="READING FROM SD CARD">    
    __SD_START(); // Start SPI and clear SD card chip select
    
    /***************************************************************************
     *                                                                         *
     *                          SINGLE BLOCK READ                              *
     *                                                                         *
     * Description: Reads 512 bytes from the block specified in the SD card    *
     **************************************************************************/
    __lcd_clear();__lcd_home();
    printf("Reading sector 0");
    __delay_ms(1000);
    
    /* Single block read from sector 0. */
    if(SD_SingleBlockRead(0, readBuffer)){
        __lcd_newline();
        printf("Avg: %d", average(readBuffer, 512));
    }
    else{
        /* If the single block read fails...*/
        __lcd_newline();
        printf("Failure!");
    }
    __delay_ms(1000);
    
    /***************************************************************************
     *                                                                         *
     *                        MULTIPLE BLOCK READ                              *
     *                                                                         *
     * Description: Reads consecutive 512 byte blocks from the SD card.        *
     * Occurs in 3 stages:                                                     *
     *      1. SD_MBR_Start --> Indicates to SD card that a multiple block read*
     *         is to begin. The starting block is the argument to this function*
     *      2. SD_MBR_Receive --> Receives a block (512) bytes from the SD     *
     *         card. This function can be called in a loop to receive many     *
     *         blocks and do something with them between each reception.       *
     *      3. SD_MBR_Stop --> Stops the multiple block read. The SD card can  *
     *         respond to other commands after this.                           *
     **************************************************************************/
    /* Multiple block read from sectors 1-999. In this demonstration, we will
     * find the average value written to blocks 1-999. Since we wrote 0x34 using
     * multiple block write, we expect that the average we will read back will
     * also be 0x34. Note that 0x34 is 52 in decimal. */
    
    /* Declare local counting variable. This will hold the average value of the 
     * data in sectors 1-999. */
    unsigned long avg = 0;
    
    /* Start multiple block read (MBR) from same block address as the MBW was
     * started at. */
    SD_MBR_Start(SDCard.write.MBW_startBlock);
    
    __lcd_clear();__lcd_home();
    printf("Reading sectors");
    __lcd_newline();
    printf("%d-%d...", firstBlock, firstBlock + numWrites - 1);
    
    for(i = 0;
        i < SDCard.write.lastBlockWritten - SDCard.write.MBW_startBlock + 1;
        i++)
    {
        /* Read the sector and store it in readBuffer. */
        SD_MBR_Receive(readBuffer);
        
        /* Do something with the data block received. In this case, compute its
         * average and add it to our average counter. */
        avg += average(readBuffer, 512);
    }
    SD_MBR_Stop();
    
    /* Compute the average of the data received. This should match the value
     * written previously using multiple block write (MBW). */
    avg /= (SDCard.read.lastBlockRead - SDCard.read.MBR_startBlock);
    
    __SD_STOP(); // Stop SPI and deselect SD card
    
    __lcd_clear();__lcd_home();
    printf("Sec %d-%d", (int)firstBlock, (int)(firstBlock + numWrites - 1));
    __lcd_newline();
    printf("Avg: %d", (int)avg);
    // </editor-fold>

    while(1); // Do nothing!
}

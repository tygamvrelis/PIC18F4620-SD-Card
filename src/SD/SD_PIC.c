/* 
 * File:   SD_PIC.c
 * Author: Tyler Gamvrelis
 *
 * Created on July 21, 2017, 3:52 PM
 */

#include "SD_PIC.h"

/**************************** External variables ******************************/
/* Initialize all members to 0. */
SDCard_t SDCard = {0};

/******************************* Public Functions *****************************/
/* Low-level protocol. */
void SD_SendDummyBytes(unsigned char numBytes){
    /* Sends dummy bytes of 0xFF to the SD card. Used to update the flash
     * controller's state machine.
     * 
     * Arguments: numBytes, the number of dummy bytes to send
     * 
     * Returns: none
     */
    
    unsigned char n = numBytes;
    while(n > 0){
        spiSend(0xFF);
        n--;
    }
}

unsigned char SD_Command(unsigned char cmd, unsigned long arg){
    /* Sends a command to the SD card.
     *
     * Arguments: cmd, the command number to be sent
     *            arg, the argument to be sent (32-bit)
     *
     * Returns: SD card response code
     */
    
    /* Declare local variables. */
    unsigned char response;
    unsigned char n = 0;
    
    /* Select SD Card. */
    CS_SD = 0;
    
    /* Poll card until the card is no longer busy. Sending these clocks also
     * ensures the internal state machine of the flash controller will make any
     * pending transitions. */
    while(spiReceive() != 0xFF){continue;}
    
    /* Send command number. ORed with 0x40 because it's a required bit. */
    spiSend(cmd | 0x40U);
    
    /* Iterate through the arguments to send them MSb first. Use a pointer
     * instead of bit manipulation for greater efficiency. */
    unsigned char* ptr = (unsigned char*)&arg + sizeof(unsigned long) - 1;
    while(n < 4){
        spiSend(*ptr);
        ptr--;
        n++;
    }
    
    /* Send CRC. For all cards, CMD0 and CMD8 must have the correct CRC. After
     * these have been sent however, CRC can be disabled and so it becomes
     * unimportant what we send for the CRC after that. */
    if(cmd == CMD8){
        spiSend(CMD8CRC);
    }
    else{
        spiSend(CMD0CRC);
    }
    
    /* Wait for response. Wait at most 8 cycles. */
    n = 0;
    do{
        response = spiReceive();
        n++;
    }while((n < 8) && (response == 0xFF));
    
    /* Deselect SD Card. */
    CS_SD = 1;
    
    return response;
}

unsigned char SD_ACMD(unsigned char cmd, unsigned long arg){
    /* Sends an application specific command to the SD card.
     *
     * Arguments: cmd, the command number to be sent
     *            arg, the argument to be sent (32-bit)
     *
     * Returns: SD card response code
     */
    
    /* All ACMD are preceeded by CMD55, which is a "heads up" to the SD Card
     * that it is about to receive an ACMD. */
    SD_Command(CMD55, 0);
    return SD_Command(cmd, arg);
}

/* Write functions. */
unsigned char SD_SingleBlockWrite(unsigned long block, unsigned char* arr){
    /* Initiates a 512 byte write into the specified block, starting with
     * arr[0] and ending with arr[511]
     * 
     * Arguments: block, the block number in the SD card memory to write to
     *            arr, the pointer to the array of bytes to be written
     *
     * Returns: 1 if successful, 0 otherwise
     */
    
    /* If the SD card is SDHC/SDXC, then it uses the block addressing format
     * that was passed into this function. If the card is SDSC, then it uses
     * byte addressing, thus the address passed into the function has to be
     * converted to bytes (multiplied by 512 bytes per block). */
    if(SDCard.Type == TYPE_SDSC){   block <<= 9;    }
    
    /* Send CMD24 (WRITE_BLOCK) and wait for card ready response. */
    while(SD_Command(CMD24, block) != R1_READY_STATE);
    
    /* Send WRITE_BLOCK Start Block token. */
    CS_SD = 0; // Select card
    spiSend(START_BLOCK);
    
    /* Transfer the array. */
    for(unsigned short i = 0; i < 512; i++){
        spiTransfer(arr[i]);
    }
    
    /* Stuff bits for data block CRC. */
    SD_SendDummyBytes(2);
    
    /* Check data response token to see if write was valid. */
    unsigned char response = (spiReceive() >> 1) & 0x0F;
    CS_SD = 1; // Deselect card
    switch(response){
        case 0b10:
            /* Data accepted. Save the address of the last block written 
             * just in case this needs to be referred to later in the user
             * code. */
            SDCard.write.lastBlockWritten = block;
            
            /* Wait until card is done programming. */
            while(spiReceive() == 0){  continue;   }
            return 1;
        case 0b101:
            /* CRC error. */
            return 0;
        case 0b110:
            /* Write error. */
            return 0;
        default:
            return 0;
    }
}

void SD_MBW_Start(unsigned long startBlock, unsigned long numBlocks){
    /* Initiates a multiple block write beginning at the block startBlock, and 
     * ending at the block numBlocks later.
     *  
     * Arguments: startBlock, the block number in SD card memory to begin the write
     *            numBlocks, the number of blocks to be written to
     *
     * Returns: none
     */
    
    /* If the SD card is SDHC/SDXC, then it uses the block addressing format
     * that was passed into this function. If the card is SDSC, then it uses
     * byte addressing, thus the address passed into the function has to be
     * converted to bytes (multiplied by 512 bytes per block). */
    if(SDCard.Type == TYPE_SDSC){   startBlock <<= 9;    }
    
    /* Specify the number of blocks to be pre-erased. */
    SD_ACMD(ACMD23, numBlocks);
    
    /* Send CMD25 (WRITE_MULTIPLE_BLOCK) and wait for card ready response. */
    while(SD_Command(CMD25, startBlock) != R1_READY_STATE);
    
    /* Update members in SDCard struct. */
    SDCard.write.MBW_startBlock = startBlock;
}

unsigned char SD_MBW_Send(unsigned char* arrWrite){
    /* Sends data as part of a multiple block read.
     * 
     * Precondition: Multiple block write properly initialized by calling
     * SD_MBW_Start before this function. Note that between the time that
     * SD_MBW_Start and this function are called, no other SD card functions
     * should be called. Otherwise, the pre-erase configured in SD_MBW_Start
     * may be cleared.
     *  
     * Arguments: arrWrite, pointer to the array of bytes to be written. Its
     *              size MUST be a multiple of 512.
     *
     * Returns: 1 if successful, 0 otherwise
     */

    /* Initialize local variables. */
    unsigned short i = 0; // Array transfer index
    unsigned char response = 0; // To hold data response token
    unsigned char status = 0; // To hold status register bits 15 through 8
    
    CS_SD = 0; // Select card
    while(spiReceive() != 0xFF); // Poll the DAT0 line until card is not busy
        
    /* Send WRITE_MULTIPLE_BLOCK Start Block token. */
    spiSend(START_BLOCK_TOKEN);

    /* Transfer the array. */
    for(i = 0; i < 512; i++){
        response = spiTransfer(arrWrite[i]);
    }

    /* Stuff bits for data block CRC. */
    SD_SendDummyBytes(2);

    /* Check data response token to see if write was valid. */
    do{
        response = spiReceive() & 0x1F;
    }while(response == 0x1F);
    CS_SD = 1; // Deselect card
    
    switch(response){
        case 0b00101:
            /* Data accepted. Save the address of the last block written 
             * just in case this needs to be referred to later in the user
             * code. */
            if(SDCard.write.MBW_flag_first){
                /* Special case for first block in the multiple block write. */
                SDCard.write.lastBlockWritten = SDCard.write.MBW_startBlock;
                SDCard.write.MBW_flag_first = 0;
            }
            else{
                SDCard.write.lastBlockWritten++;
            }
            
            return 1; // Success
        case 0b01011:
            /* CRC error. */
            SD_Command(CMD12, 0); // End data transmission using CMD12
            return 0;
        case 0b01101:
            /* Write error. */
            SD_Command(CMD12, 0); // End data transmission using CMD12
            return 0;
        default:
            return 0; // Unknown
    }
}

void SD_MBW_Stop(void){
    /* Stops a multiple block write.
     * 
     * Precondition: SD_MBW_Send called properly at least once. 
     * 
     * Arguments: none
     * 
     * Returns: none
     */
    
    CS_SD = 0; // Select card
    while(spiReceive() != 0xFF); // Poll the DAT0 line until card is not busy
    
    /* Send Stop Tran token once card is not busy. */
    spiSend(STOP_TRAN);
    
    /* Wait until card is done programming. */
    while(spiReceive() == 0){  continue;   }
    CS_SD = 1; // Deselect card
    
    /* Resets software fields. */
    SDCard.write.MBW_flag_first = 1;
}

/* Read functions. */
unsigned char SD_SingleBlockRead(unsigned long block, unsigned char* buf){
    /* Initiates a 512 byte read from the specified block, block, into the array
     * of bytes buf.
     * 
     * Arguments: block, the block number in SD card memory to begin the read
     *            buf, the pointer to the array of bytes that data is to be
     *              stored into.
     *
     * Returns: 1 if successful, 0 otherwise
     */
    
    /* If the SD card is SDHC/SDXC, then it uses the block addressing format
     * that was passed into this function. If the card is SDSC, then it uses
     * byte addressing, thus the address passed into the function has to be
     * converted to bytes (multiplied by 512 bytes per block). */
    if(SDCard.Type == TYPE_SDSC){   block <<= 9;    }
    
    /* Poll SD card to see when it stops being busy. Note that the SD card pulls
     * down DAT0 when it's busy, which is why we don't need to assert CS = 0. */
    unsigned char response;
    do{
        response = SD_Command(CMD17, block);
        if((response & 0x0F) != 0){
            /* b0 --> General or unknown error
             * b1 --> Internal card controller (CC) error
             * b2 --> Card ECC failed
             * b3 --> Out of range error.
             */
            return 0;
        }
    }while(response != R1_READY_STATE);
    
    /* Poll card to wait until it's not busy. */
    CS_SD = 0;
    do{
        response = spiReceive();
    }while(response != START_BLOCK);

    /* Store the received data into the array of bytes, buf. */
    for(unsigned short i = 0; i < 512; i++){
        buf[i] = spiReceive();
    }
    
    /* Stuff bits for data block CRC. */
    spiSend(0xFF);
    spiSend(0xFF);
    CS_SD = 1; // Deselect card
    
    /* Store last block read for user. */
    SDCard.read.lastBlockRead = block;
    
    return 1; // Success
}

unsigned char SD_MBR_Start(unsigned long startBlock){
    /* Initiates a read of the SD card, starting with the sector at address
     * startBlock.
     * 
     * Arguments: startBlock, the block number in SD card memory to begin the read
     *
     * Returns: 1 if successful, 0 otherwise
     */
    
    /* Initialize local variables. */
    unsigned char response = 0;
    
    /* If the SD card is SDHC/SDXC, then it uses the block addressing format
     * that was passed into this function. If the card is SDSC, then it uses
     * byte addressing, thus the address passed into the function has to be
     * converted to bytes (multiplied by 512 bytes per block). */
    if(SDCard.Type == TYPE_SDSC){   startBlock <<= 9;    }
    
    /* Send the READ_MULTIPLE_BLOCK command until the SD card indicates it is
     * ready to send data or there is an error. */
    do{
        response = SD_Command(CMD18, startBlock);
        if(response & 0x0F){
            /* b0 --> General or unknown error
             * b1 --> Internal card controller (CC) error
             * b2 --> Card ECC failed
             * b3 --> Out of range error.
             */
            return 0;
        }
    }while(response != R1_READY_STATE);
    
    /* Update members in SDCard struct. */
    SDCard.read.MBR_startBlock = startBlock;
    
    return 1; // Success
}

void SD_MBR_Receive(unsigned char* bufReceive){
    /* Receives data as part of a multiple block read.
     * 
     * Precondition: Multiple block read properly initialized by calling
     * SD_MBR_Start before this function.
     * 
     * Arguments: bufReceive, the pointer to the array that will store the data
     *
     * Returns: none
     */
    
    /* Poll SD card to see when it stops being busy. Note that the SD card pulls
     * down DAT0 when it's busy, which is why we don't need to assert CS = 0. */
    while(spiReceive() == 0x00){    continue;   }
    
    /* Read into buffer. */
    CS_SD = 0; // Select card
    
    /* Wait for 0xFE, the token signifying the start of a data block. */
    while(spiReceive() != START_BLOCK){    continue;   }
    
    /* Receive the data block. */
    for(unsigned short i = 0; i < 512; i++){
        bufReceive[i] = spiReceive();
    }

    /* Stuff bits for data block CRC. */
    spiSend(0xFF);
    spiSend(0xFF);
    CS_SD = 1; // Deselect card

    /* Store last block read for user. */
    if(SDCard.read.MBR_flag_first){
        SDCard.read.lastBlockRead = SDCard.read.MBR_startBlock;
        SDCard.read.MBR_flag_first = 0;
    }
    else{
        SDCard.read.lastBlockRead++;
    }
}

void SD_MBR_Stop(void){
    /* Stops a multiple block read.
     *
     * Precondition: SD_MBR_Receive called properly at least once
     * 
     * Arguments: none
     * 
     * Returns: none
     */
    
    /* Send STOP_TRANSMISSION command. */
    SD_Command(CMD12, 0);
    
    /* Resets software fields. */
    SDCard.read.MBR_flag_first = 1;
}

/* Erasing. */
void SD_EraseBlocks(unsigned long firstBlock, unsigned long lastBlock){
    /* Erases all the blocks between firstBlock and lastBlock, inclusive. This
     * may take some time, during which DAT0 will be held low. Thus, users may 
     * choose to poll the SD card's response before attempting other SD card 
     * operations.
     *
     * Arguments: firstBlock, the address of the first block to be erased
     *            lastBlock, the address of the last block to be erased
     * 
     * Returns: none
     */
    
    /* If the SD card is SDHC/SDXC, then it uses the block addressing format
     * that was passed into this function. If the card is SDSC, then it uses
     * byte addressing, thus the address passed into the function has to be
     * converted to bytes (multiplied by 512 bytes per block). */
    if(SDCard.Type == TYPE_SDSC){   firstBlock <<= 9;  lastBlock <<= 9;  }
    
    /* Specify the block address for erasing to begin. */
    SD_Command(CMD32, firstBlock); // ERASE_WR_BLOCK_START
    
    /* Specify the block address for erasing to end. */
    SD_Command(CMD33, lastBlock); // ERASE_WR_BLOCK_END
    
    /* Erase the specified contiguous block sequence. */
    SD_Command(CMD38, 0); // ERASE
}

/* Initialization. */
void initSD(void){
    /* This function initializes the SPI peripheral and the pin for chip select.
     *
     * Arguments: none
     * 
     * Returns: none
     */
    
    /* Declare local variables. */
    unsigned char last_OSCCON = OSCCON; // Save oscillator state
    unsigned char last_OSCTUNE = OSCTUNE; // Save oscillator state
    unsigned char response;
    unsigned char i;
    unsigned char arr_response[16] = {0};
    
    /* Set oscillator to frequency such that the SPI will clock between 100 kHz 
     * and 400 kHz for SD card initialization. It is also preferred to keep the
     * oscillator frequency sufficiently fast to work through the data sending
     * routines while keeping the SPI clock frequency consistent during 
     * commands, just because this makes communication faster. */
    OSCTUNEbits.TUN = 0b000000; // Run oscillator at calibrated frequency
    OSCCONbits.IRCF = 0b110; // Configure internal oscillator to 4 MHz
    OSCCONbits.SCS = 0b11; // Use internal oscillator
    OSCTUNEbits.PLLEN = 1; // Enable PLL for internal oscillator of 16 MHz

    /* Wait for internal oscillator to stabilize. */
    while(!OSCCONbits.IOFS){   __delay_us(20);   }
    
    /* Initialize SPI */
    spiInit(64); // 250 kHz
    
    /* Wait 20 ms, just in case this function is called before the power supply
     * to the card has stabilized. */
    __delay_ms(20);
    
    /************************** Initialization ritual *************************/
    CS_SD = 1; // Deselect the card
    TRIS_CS_SD = 0; // Set the chip select data direction to output

    /* Send 80 clock pulses. SD card standard specifies a minimum of 74. */
    for(i = 0; i < 10; i++){
        spiSend(0xFF); // 1 byte --> 8 clock pulses (1 for each bit)
    }
    
    CS_SD = 0; // Select the card
    
    /* Send CMD0 with CRC and arguments = 0. CMD0 is the GO_IDLE_STATE command,
     * which is like a software reset of the card. Continue sending this
     * until the response (1 byte) is received. */
    while(SD_Command(CMD0, 0) != R1_IDLE_STATE){  continue;   }
    
    
    /* Send CMD8 with CRC and argument = 0x1AA. This will identify the type of
     * card connected, and the "1" in the argument will check if the voltage
     * range 2.7-3.6 V is supported by the card. The "AA" in the argument is a
     * random check pattern. CMD8 sends back 4 bytes after the regular response,
     * which contain the values in the operation condition register (OCR). */
    while(1){
        response = SD_Command(CMD8, 0x01AA);
        
        /* Read back the next 4 bytes to complete the CMD8 response packet. */
        CS_SD = 0; // Select the card
        for(i = 0; i < 4; i++){ arr_response[i] = spiReceive(); }
        CS_SD = 1; // Deselect the card
        
        if((response & R1_ILLEGAL_COMMAND) == R1_ILLEGAL_COMMAND){
            /* The card is version 2.x and there is a voltage mismatch, or card 
             * is version 1.x, or card is MMC. */
            SDCard.SDversion = 1;
            
            /* Read OCR to check if voltage range is compatible. */
            SD_Command(CMD58, 0);
            CS_SD = 0; // Select card
            for(i = 0; i < 4; i++){ arr_response[i] = spiReceive(); }
            CS_SD = 1; // Deselect card
            
            if(arr_response[2] != 0x01){
                /* Unusable card. */
                return;
            }
            break;
        }
        else if(response == R1_IDLE_STATE){
            if((arr_response[2] == 0x01) && (arr_response[3] == 0xAA)){
                /* Valid response, compatible voltage range, and check pattern
                 * is correct. */
               SDCard.SDversion = 2;
               break;
            }
            else{
                /* Unusable card. */
                return;
            }
        }
    }
    
    /* Send ACMD41 to initialize the card.
     * 
     * The argument depends on the SD version the card is using. If it's using
     * SD version 1, the all the bits in the argument should be cleared.
     * Otherwise, the high capacity support (HCS) bit (bit 33) should be set
     * to indicate to the card that the host (PIC) supports SDHC and SDXC cards.
     */
    unsigned long argument = (SDCard.SDversion == 1) ? 0 : 0x40000000;
    
    do{
        response = SD_ACMD(ACMD41, argument);
    }while((response != R1_READY_STATE)\
       && ((response & R1_ILLEGAL_COMMAND) != R1_ILLEGAL_COMMAND));

    /* If ACMD41 returns illegal command, then the device is not an SD
     * memory card, or initialization has failed. */
    if((response & R1_ILLEGAL_COMMAND) == R1_ILLEGAL_COMMAND){
        if(SDCard.SDversion == 1){
            /* Continue initialization as MMC card. */
            SDCard.Type = TYPE_MMC;
            SD_Command(CMD1, 0);
        }
        else{
            /* Unusable card (initialization failed). */
            SDCard.init = 0;
            return;
        }
    }
    
    if(SDCard.Type != TYPE_MMC){
        /* Read OCR to get card capacity information (CCS) and check if card is
         * SDHC. */
        SD_Command(CMD58, 0);
        
        /* Check CCS flag (bit 30) as well as the power up status (bit 31). */
        CS_SD = 0; // Select card
        SDCard.Type = (unsigned char)((spiReceive() & 0xC0) == 0xC0);
    
        /* Discard remaining OCR bytes, as they simply contain the voltage
         * range and reserved bits. */
        for(i = 0; i < 3; i++){ spiReceive();   }
        CS_SD = 1; // Deselect card
    }
    
    /* Set block length to 512 bytes. Block read/write commands require this. */
    while(SD_Command(CMD16, 512) != R1_READY_STATE){    continue;   }
    SDCard.blockSize = 512;
    
    /* Request the contents of the card-specific data (CSD) register. */
    SD_Command(CMD9, 0);
    CS_SD = 0; // Select card
    while(spiReceive() != START_BLOCK){    continue;   }
    for(i = 0; i < 16; i++){
        arr_response[i] = spiReceive();
    }
    spiReceive(); // Ignore CRC
    spiReceive(); // Ignore CRC
    CS_SD = 1; // Deselect card
    
    if(SDCard.SDversion == 2){
        /* Uses the version 2 (SDHC) capacity calculation (megabytes).
         *      arr_response[9] --> C_SIZE[7:0]
         *      arr_response[8] --> C_SIZE[15:8]
         *      arr_response[7] & 0x3F --> C_SIZE[21:16]
         */
        unsigned long tempSize = arr_response[9] + 1UL;
        tempSize |= (unsigned long)(arr_response[8] << 8);
        tempSize |= (unsigned long)(arr_response[7] & 0x3F) << 16;
        SDCard.size = tempSize * 0.524288; // Number of MB now
        SDCard.numBlocks = (unsigned long)(SDCard.size  * 2048); // Number of sectors/blocks
     }
     else{
        /* Uses the version 1 (SDSC) capacity calculation (bytes).
         *      arr_response[5] --> READ_BL_LEN[3:0]
         *      arr_response[6] --> C_SIZE[11:10]
         *      arr_response[7] --> C_SIZE[9:2]
         *      arr_response[8] & 0xC0 --> C_SIZE[1:0]
         *      arr_response[9] & 0x03 --> C_SIZE_MULT[2:1]
         */
        unsigned long tempSize = (unsigned long)(arr_response[6] & 0x03) << 4;
        tempSize |= (unsigned long)(arr_response[7] << 2);
        tempSize |= (unsigned long)((arr_response[8] & 0xC0) >> 2) + 1;
        tempSize = tempSize << (((unsigned long)\
                 ((arr_response[9] & 0x03) << 1)\
                 | (unsigned long)((arr_response[10] & 0x80) >> 7)) + 2);
        tempSize = tempSize << ((unsigned long)\
                 (arr_response[5] & 0x0F));
        SDCard.size = (unsigned long)tempSize;
        SDCard.numBlocks = (unsigned long)(SDCard.size/SDCard.blockSize);
    }
    
    /* Request the contents of the card identification (CID) register. */
    SD_Command(CMD10, 0);
    
    CS_SD = 0; // Select card
    
    /* Wait for data start token from card. */
    do{
        response = spiReceive();
    }while(response != START_BLOCK);
    
    for(i = 0; i < 16; i++){
        arr_response[i] = spiReceive();
    }
    spiReceive(); // Ignore CRC
    spiReceive(); // Ignore CRC
    CS_SD = 1; // Deselect card
    
    SDCard.MID = arr_response[0];
    SDCard.OID = (unsigned short)(arr_response[1] << 8U) | arr_response[2];
    SDCard.PHMH = arr_response[3];

    /* Explicitly cast for long data type, or else the correct math libraries
     * won't be used and the shifting won't work properly. */
    SDCard.PHML = (unsigned long)arr_response[4] << 24U;
    SDCard.PHML |= (unsigned long)arr_response[5] << 16U;
    SDCard.PHML |= (unsigned long)arr_response[6] << 8U;
    SDCard.PHML |= (unsigned long)arr_response[7];
    
    SDCard.PRV = arr_response[8];

    /* Explicitly cast for long data type, or else the correct math libraries
     * won't be used and the shifting won't work properly. */
    SDCard.PSN = (unsigned long)arr_response[9] << 24U;
    SDCard.PSN |= (unsigned long)arr_response[10] << 16U;
    SDCard.PSN |= (unsigned long)arr_response[11] << 8U;
    SDCard.PSN |= (unsigned long)arr_response[12];

    SDCard.MDT = (unsigned short)(((arr_response[13] & 0x0F) << 8U)) |
                                    (arr_response[14]);
    SDCard.CRC = arr_response[15] & 0xFE;
    
    /* Disable SPI, increase SPI frequency, restore previous oscillator state. */
    __SD_STOP();
    SSPCON1 = 0b00010000; // Max SPI frequency, FOSC/4
    OSCCON = last_OSCCON;
    OSCTUNE = last_OSCTUNE;
    
    /* Wait for internal oscillator to stabilize. */
    while(!OSCCONbits.IOFS){   __delay_us(20);   }
    
    /* Restart SPI. */
    __MSSP_ENABLE();
    
    /* Initialize fields of SDCard struct for use in software. */
    SDCard.write.MBW_flag_first = 1;
    SDCard.write.MBW_startBlock = 0;
    SDCard.write.lastBlockWritten = 0;
    SDCard.read.MBR_flag_first = 1;
    SDCard.read.MBR_startBlock = 0;
    SDCard.read.lastBlockRead = 0;
    
    /* Store that the initialization succeeded. */
    SDCard.init = 1;
}
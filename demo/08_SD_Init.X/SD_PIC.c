/**
 * @file
 * @author Tyler Gamvrelis
 *
 * Created on July 21, 2017, 3:52 PM
 *
 * @ingroup SD
 */

/********************************* Includes **********************************/
#include "SD_PIC.h"

/******************************** Constants **********************************/
const unsigned char CMD0 = 0;
const unsigned char CMD0CRC = 0x95;
const unsigned char CMD1 = 1;
const unsigned char CMD8 = 8;
const unsigned char CMD8CRC = 0x87;
const unsigned char CMD9 = 9;
const unsigned char CMD10 = 10;
const unsigned char CMD12 = 12;
const unsigned char CMD13 = 13;
const unsigned char CMD16 = 16;
const unsigned char CMD17 = 17;
const unsigned char CMD18 = 18;
const unsigned char CMD24 = 24;
const unsigned char CMD25 = 25;
const unsigned char CMD32 = 32;
const unsigned char CMD33 = 33;
const unsigned char CMD38 = 38;
const unsigned char CMD55 = 55;
const unsigned char CMD58 = 58;
const unsigned char ACMD22 = 22;
const unsigned char ACMD23 = 23;
const unsigned char ACMD41 = 41;
const unsigned char R1_READY_STATE = 0;
const unsigned char R1_IDLE_STATE = 1;
const unsigned char R1_ILLEGAL_COMMAND = 4;
const unsigned char START_BLOCK = 0xFE;
const unsigned char START_BLOCK_TOKEN = 0xFC;
const unsigned char STOP_TRAN = 0xFD;

/***************************** Public Variables ******************************/
SDCard_t SDCard = {0};

/***************************** Public Functions ******************************/
void SD_SendDummyBytes(unsigned char numBytes){   
    unsigned char n = numBytes;
    while(n > 0){
        spiSend(0xFF);
        n--;
    }
}

unsigned char SD_Command(unsigned char cmd, unsigned long arg){   
    CS_SD = 0; // Select the SD card
    
    // Poll card until the it is no longer busy. Sending these clocks also
    // ensures the internal state machine of the flash controller will make any
    // pending transitions
    while(spiReceive() != 0xFF){
        continue;
    }
    
    // Send command number. ORed with 0x40 because it's a required bit
    spiSend(cmd | 0x40U);
    
    // Iterate through the arguments to send them MSb first
    spiSend(arg >> 24);
    spiSend((arg >> 16) & 0xFF);
    spiSend((arg >> 8) & 0xFF);
    spiSend(arg & 0xFF);
    
    // Send CRC. For all cards, CMD0 and CMD8 must have the correct CRC. After
    // these have been sent however, CRC can be disabled and so it becomes
    // unimportant what we send for the CRC after that
    if(cmd == CMD8){
        spiSend(CMD8CRC);
    }
    else{
        spiSend(CMD0CRC);
    }
    
    // Wait at most 8 cycles for response
    unsigned char n = 0;
    unsigned char response;
    do{
        response = spiReceive();
        n++;
    }while((n < 8) && (response == 0xFF));
    
    CS_SD = 1; // Deselect SD Card

    return response;
}

unsigned char SD_ACMD(unsigned char cmd, unsigned long arg){    
    // All ACMD are preceeded by CMD55, which is a "heads up" to the SD Card
    // that it is about to receive an ACMD
    SD_Command(CMD55, 0);
    return SD_Command(cmd, arg);
}

unsigned char SD_SingleBlockWrite(unsigned long block, unsigned char* arr){   
    // If the SD card is SDHC/SDXC, then it uses the block addressing format
    // that was passed into this function. If the card is SDSC, then it uses
    // byte addressing, thus the address passed into the function has to be
    // converted to bytes
    if(SDCard.Type == TYPE_SDSC){
        // Multiply by 512 to convert a block address to a byte address
        block <<= 9;
    }
    
    // Send CMD24 (WRITE_BLOCK) and wait for card ready response
    while(SD_Command(CMD24, block) != R1_READY_STATE);
    
    // Send WRITE_BLOCK Start Block token
    CS_SD = 0; // Select card
    spiSend(START_BLOCK);
    
    // Transfer the array
    for(unsigned short i = 0; i < 512; i++){
        spiTransfer(arr[i]);
    }
    
    // Stuff bits for data block CRC
    SD_SendDummyBytes(2);
    
    // Check data response token to see if write was valid
    unsigned char response = (spiReceive() >> 1) & 0x0F;
    CS_SD = 1; // Deselect card
    switch(response){
        case 0b10:
            // Data accepted. Save the address of the last block written 
            // just in case this needs to be referred to later in the user
            // code
            SDCard.write.lastBlockWritten = block;
            
            // Wait until card is done programming
            while(spiReceive() == 0){  continue;   }
            return 1;
        case 0b101:
            // CRC error
            return 0;
        case 0b110:
            // Write error
            return 0;
        default:
            return 0;
    }
}

void SD_MBW_Start(unsigned long startBlock, unsigned long numBlocks){   
    // If the SD card is SDHC/SDXC, then it uses the block addressing format
    // that was passed into this function. If the card is SDSC, then it uses
    // byte addressing, thus the address passed into the function has to be
    // converted to bytes
    if(SDCard.Type == TYPE_SDSC){
        // Multiply by 512 to convert a block address to a byte address
        startBlock <<= 9;
    }
    
    // Specify the number of blocks to be pre-erased
    SD_ACMD(ACMD23, numBlocks);
    
    // Send CMD25 (WRITE_MULTIPLE_BLOCK) and wait for card ready response
    while(SD_Command(CMD25, startBlock) != R1_READY_STATE);
    
    SDCard.write.MBW_startBlock = startBlock;
}

unsigned char SD_MBW_Send(unsigned char* arrWrite){    
    CS_SD = 0; // Select card
    while(spiReceive() != 0xFF); // Poll the DAT0 line until card is not busy
        
    // Send WRITE_MULTIPLE_BLOCK Start Block token
    spiSend(START_BLOCK_TOKEN);

    // Transfer the array
    unsigned char response = 0; // To hold data response token
    for(unsigned short i = 0; i < 512; i++){
        response = spiTransfer(arrWrite[i]);
    }

    // Stuff bits for data block CRC
    SD_SendDummyBytes(2);

    // Check data response token to see if write was valid
    do{
        response = spiReceive() & 0x1F;
    }while(response == 0x1F);
    CS_SD = 1; // Deselect card
    
    switch(response){
        case 0b00101:
            // Data accepted. Save the address of the last block written 
            // just in case this needs to be referred to later in the user
            // code
            if(SDCard.write.MBW_flag_first){
                // Special case for first block in the multiple block write
                SDCard.write.lastBlockWritten = SDCard.write.MBW_startBlock;
                SDCard.write.MBW_flag_first = 0;
            }
            else{
                SDCard.write.lastBlockWritten++;
            }
            
            return 1; // Success
        case 0b01011:
            // CRC error
            SD_Command(CMD12, 0); // End data transmission using CMD12
            return 0;
        case 0b01101:
            // Write error
            SD_Command(CMD12, 0); // End data transmission using CMD12
            return 0;
        default:
            return 0; // Unknown
    }
}

void SD_MBW_Stop(void){    
    CS_SD = 0; // Select card
    while(spiReceive() != 0xFF); // Poll the DAT0 line until card is not busy
    
    // Send Stop Tran token once card is not busy
    spiSend(STOP_TRAN);
    
    // Wait until card is done programming
    while(spiReceive() == 0){
        continue;
    }

    CS_SD = 1; // Deselect card
    
    SDCard.write.MBW_flag_first = 1;
}

unsigned char SD_SingleBlockRead(unsigned long block, unsigned char* buf){   
    // If the SD card is SDHC/SDXC, then it uses the block addressing format
    // that was passed into this function. If the card is SDSC, then it uses
    // byte addressing, thus the address passed into the function has to be
    // converted to bytes
    if(SDCard.Type == TYPE_SDSC){
        // Multiply by 512 to convert a block address to a byte address
        block <<= 9;
    }
    
    // Poll SD card to see when it stops being busy. Note that the SD card pulls
    // down DAT0 when it's busy, which is why we don't need to assert CS = 0
    unsigned char response;
    do{
        response = SD_Command(CMD17, block);
        if((response & 0x0F) != 0){
            // b0 --> General or unknown error
            // b1 --> Internal card controller (CC) error
            // b2 --> Card ECC (error-control code) failed
            // b3 --> Out of range error
            return 0;
        }
    }while(response != R1_READY_STATE);
    
    /// Poll card to wait until it's not busy
    CS_SD = 0;
    do{
        response = spiReceive();
    }while(response != START_BLOCK);

    for(unsigned short i = 0; i < 512; i++){
        buf[i] = spiReceive();
    }
    
    // Stuff bits for data block CRC
    spiSend(0xFF);
    spiSend(0xFF);

    CS_SD = 1; // Deselect card
    
    SDCard.read.lastBlockRead = block;
    
    return 1; // Success
}

unsigned char SD_MBR_Start(unsigned long startBlock){   
    // If the SD card is SDHC/SDXC, then it uses the block addressing format
    // that was passed into this function. If the card is SDSC, then it uses
    // byte addressing, thus the address passed into the function has to be
    // converted to bytes
    if(SDCard.Type == TYPE_SDSC){
        // Multiply by 512 to convert a block address to a byte address
        startBlock <<= 9;
    }
    
    // Send the READ_MULTIPLE_BLOCK command until the SD card indicates it is
    // ready to send data or there is an error
    unsigned char response = 0;
    do{
        response = SD_Command(CMD18, startBlock);
        if(response & 0x0F){
            // b0 --> General or unknown error
            // b1 --> Internal card controller (CC) error
            // b2 --> Card ECC (error-control code) failed
            // b3 --> Out of range error
            return 0;
        }
    }while(response != R1_READY_STATE);
    
    SDCard.read.MBR_startBlock = startBlock;
    
    return 1; // Success
}

void SD_MBR_Receive(unsigned char* bufReceive){    
    // Poll SD card to see when it stops being busy. Note that the SD card pulls
    // down DAT0 when it's busy, which is why we don't need to assert CS = 0
    while(spiReceive() == 0x00){
        continue;
    }
    
    CS_SD = 0; // Select card
    
    // Wait for 0xFE, the token signifying the start of a data block
    while(spiReceive() != START_BLOCK){
        continue;
    }
    
    // Receive the data block
    for(unsigned short i = 0; i < 512; i++){
        bufReceive[i] = spiReceive();
    }

    // Stuff bits for data block CRC
    spiSend(0xFF);
    spiSend(0xFF);

    CS_SD = 1; // Deselect card

    if(SDCard.read.MBR_flag_first){
        SDCard.read.lastBlockRead = SDCard.read.MBR_startBlock;
        SDCard.read.MBR_flag_first = 0;
    }
    else{
        SDCard.read.lastBlockRead++;
    }
}

void SD_MBR_Stop(void){    
    // Send STOP_TRANSMISSION command
    SD_Command(CMD12, 0);
    SDCard.read.MBR_flag_first = 1;
}

void SD_EraseBlocks(unsigned long firstBlock, unsigned long lastBlock){   
    // If the SD card is SDHC/SDXC, then it uses the block addressing format
    // that was passed into this function. If the card is SDSC, then it uses
    // byte addressing, thus the address passed into the function has to be
    // converted to bytes
    if(SDCard.Type == TYPE_SDSC){
        // Multiply by 512 to convert a block address to a byte address
        firstBlock <<= 9;
        lastBlock <<= 9;
    }
    
    // Specify the block address for erasing to begin
    SD_Command(CMD32, firstBlock); // ERASE_WR_BLOCK_START
    
    // Specify the block address for erasing to end
    SD_Command(CMD33, lastBlock); // ERASE_WR_BLOCK_END
    
    // Erase the specified contiguous block sequence
    SD_Command(CMD38, 0); // ERASE
}

void initSD(void){
    const unsigned char last_OSCCON = OSCCON; // Save oscillator state
    const unsigned char last_OSCTUNE = OSCTUNE; // Save oscillator state
    unsigned char response;
    unsigned char arr_response[16] = {0};
    
    // Set oscillator to frequency such that the SPI will clock between 100 kHz 
    // and 400 kHz for SD card initialization
    OSCTUNEbits.TUN = 0b000000; // Run oscillator at calibrated frequency
    OSCCONbits.IRCF = 0b110; // Configure internal oscillator to 4 MHz
    OSCCONbits.SCS = 0b11; // Use internal oscillator

    // Wait for internal oscillator to stabilize
    while(!OSCCONbits.IOFS){
        __delay_us(20);
    }
    
    spiInit(16); // 250 kHz
    
    // Wait 20 ms, just in case this function is called before the power supply
    // to the card has stabilized
    __delay_ms(20);
    
    /************************** Initialization ritual *************************/
    CS_SD = 1; // Deselect the card
    TRIS_CS_SD = 0; // Set the chip select data direction to output

    // Send 80 clock pulses. SD card standard specifies a minimum of 74
    for(unsigned char i = 0; i < 10; i++){
        spiSend(0xFF); // 1 byte --> 8 clock pulses (1 for each bit)
    }
    
    CS_SD = 0; // Select the card
    
    // Send CMD0 with CRC and arguments = 0. CMD0 is the GO_IDLE_STATE command,
    // which is like a software reset of the card. Continue sending this
    // until the response (1 byte) is received
    while(SD_Command(CMD0, 0) != R1_IDLE_STATE){
        continue;
    }
    
    // Send CMD8 with CRC and argument = 0x1AA. This will identify the type of
    // card connected, and the "1" in the argument will check if the voltage
    // range 2.7-3.6 V is supported by the card. The "AA" in the argument is a
    // random check pattern. CMD8 sends back 4 bytes after the regular response,
    // which contain the values in the operation condition register (OCR)
    while(1){
        response = SD_Command(CMD8, 0x01AA);
        
        // Read back the next 4 bytes to complete the CMD8 response packet
        CS_SD = 0; // Select the card
        for(unsigned char i = 0; i < 4; i++){
            arr_response[i] = spiReceive();
        }
        CS_SD = 1; // Deselect the card
        
        if((response & R1_ILLEGAL_COMMAND) == R1_ILLEGAL_COMMAND){
            // The card is version 2.x and there is a voltage mismatch, or card 
            // is version 1.x, or card is MMC
            SDCard.SDversion = 1;
            
            // Read OCR to check if voltage range is compatible
            SD_Command(CMD58, 0);
            CS_SD = 0; // Select card
            for(unsigned char i = 0; i < 4; i++){
                arr_response[i] = spiReceive();
            }
            CS_SD = 1; // Deselect card
            
            if(arr_response[2] != 0x01){
                // Error: Unusable card
                return;
            }
            break;
        }
        else if(response == R1_IDLE_STATE){
            if((arr_response[2] == 0x01) && (arr_response[3] == 0xAA)){
                // Valid response, compatible voltage range, and check pattern
                // is correct
               SDCard.SDversion = 2;
               break;
            }
            else{
                // Error: Unusable card
                return;
            }
        }
    }
    
    // Send ACMD41 to initialize the card.
    // 
    // The argument depends on the SD version the card is using. If it's using
    // SD version 1, the all the bits in the argument should be cleared.
    // Otherwise, the high capacity support (HCS) bit (bit 33) should be set
    // to indicate to the card that the host (PIC) supports SDHC and SDXC cards
    unsigned long argument = (SDCard.SDversion == 1) ? 0 : 0x40000000;
    
    do{
        response = SD_ACMD(ACMD41, argument);
    }while(
        (response != R1_READY_STATE) &&
        ((response & R1_ILLEGAL_COMMAND) != R1_ILLEGAL_COMMAND)
    );

    // If ACMD41 returns illegal command, then the device is not an SD
    // memory card, or initialization has failed
    if((response & R1_ILLEGAL_COMMAND) == R1_ILLEGAL_COMMAND){
        if(SDCard.SDversion == 1){
            // Continue initialization as MMC card
            SDCard.Type = TYPE_MMC;
            SD_Command(CMD1, 0);
        }
        else{
            // Unusable card (initialization failed)
            SDCard.init = 0;
            return;
        }
    }
    
    if(SDCard.Type != TYPE_MMC){
        // Read OCR to get card capacity information (CCS) and check if card is
        // SDHC
        SD_Command(CMD58, 0);
        
        // Check CCS flag (bit 30) as well as the power up status (bit 31)
        CS_SD = 0; // Select card
        if((spiReceive() & 0xC0) == 0xC0){
            SDCard.Type = TYPE_SDHC_SDXC;
        }
        else{
            SDCard.Type = TYPE_SDSC;
        }
    
        // Discard remaining OCR bytes, as they simply contain the voltage
        // range and reserved bits
        for(unsigned char i = 0; i < 3; i++){
            spiReceive();
        }
        CS_SD = 1; // Deselect card
    }
    
    // Set block length to 512 bytes. Block read/write commands require this
    while(SD_Command(CMD16, 512) != R1_READY_STATE){    continue;   }
    SDCard.blockSize = 512;
    
    // Request the contents of the card-specific data (CSD) register
    SD_Command(CMD9, 0);
    CS_SD = 0; // Select card
    while(spiReceive() != START_BLOCK){    continue;   }
    for(unsigned char i = 0; i < 16; i++){
        arr_response[i] = spiReceive();
    }
    spiReceive(); // Ignore CRC
    spiReceive(); // Ignore CRC
    CS_SD = 1; // Deselect card
    
    if(SDCard.SDversion == 2){
        // Uses the version 2 (SDHC) capacity calculation (megabytes).
        //      arr_response[9] --> C_SIZE[7:0]
        //      arr_response[8] --> C_SIZE[15:8]
        //      arr_response[7] & 0x3F --> C_SIZE[21:16]
        unsigned long tempSize = arr_response[9] + 1UL;
        tempSize |= (unsigned long)(arr_response[8] << 8);
        tempSize |= (unsigned long)(arr_response[7] & 0x3F) << 16;
        SDCard.size = tempSize * 0.524288; // Number of MB now
        SDCard.numBlocks = (unsigned long)(SDCard.size  * 2048); // Number of sectors/blocks
     }
     else{
        // Uses the version 1 (SDSC) capacity calculation (bytes).
        //      arr_response[5] --> READ_BL_LEN[3:0]
        //      arr_response[6] --> C_SIZE[11:10]
        //      arr_response[7] --> C_SIZE[9:2]
        //      arr_response[8] & 0xC0 --> C_SIZE[1:0]
        //      arr_response[9] & 0x03 --> C_SIZE_MULT[2:1]
        unsigned long tempSize = (unsigned long)(arr_response[6] & 0x03) << 4;
        tempSize |= (unsigned long)(arr_response[7] << 2);
        tempSize |= (unsigned long)((arr_response[8] & 0xC0) >> 2) + 1;
        tempSize = tempSize << (((unsigned long)
                        ((arr_response[9] & 0x03) << 1) |
                        (unsigned long)((arr_response[10] & 0x80) >> 7)) + 2);
        tempSize = tempSize << (unsigned long)(arr_response[5] & 0x0F);
        SDCard.size = (unsigned long)tempSize;
        SDCard.numBlocks = (unsigned long)(SDCard.size / SDCard.blockSize);
    }
    
    // Request the contents of the card identification (CID) register
    SD_Command(CMD10, 0);
    
    CS_SD = 0; // Select card
    
    // Wait for data start token from card
    do{
        response = spiReceive();
    }while(response != START_BLOCK);
    
    for(unsigned char i = 0; i < 16; i++){
        arr_response[i] = spiReceive();
    }
    spiReceive(); // Ignore CRC
    spiReceive(); // Ignore CRC
    CS_SD = 1; // Deselect card
    
    SDCard.MID = arr_response[0];
    SDCard.OID = (unsigned short)(arr_response[1] << 8U) | arr_response[2];
    SDCard.PHMH = arr_response[3];

    // Explicitly cast for long data type, or else the correct math libraries
    // won't be used and the shifting won't work properly
    SDCard.PHML = (unsigned long)arr_response[4] << 24U;
    SDCard.PHML |= (unsigned long)arr_response[5] << 16U;
    SDCard.PHML |= (unsigned long)arr_response[6] << 8U;
    SDCard.PHML |= (unsigned long)arr_response[7];
    
    SDCard.PRV = arr_response[8];

    // Explicitly cast for long data type, or else the correct math libraries
    // won't be used and the shifting won't work properly
    SDCard.PSN = (unsigned long)arr_response[9] << 24U;
    SDCard.PSN |= (unsigned long)arr_response[10] << 16U;
    SDCard.PSN |= (unsigned long)arr_response[11] << 8U;
    SDCard.PSN |= (unsigned long)arr_response[12];

    SDCard.MDT = (unsigned short)(((arr_response[13] & 0x0F) << 8U)) |
                                    (arr_response[14]);
    SDCard.CRC = arr_response[15] & 0xFE;
    
    // Disable SPI, increase SPI frequency, restore previous oscillator state
    sd_stop();
    OSCCON = last_OSCCON;
    OSCTUNE = last_OSCTUNE;
    
    // Wait for internal oscillator to stabilize
    while(!OSCCONbits.IOFS){   __delay_us(20);   }
    
    // Restart SPI
    spiInit(16);
    
    // Initialize fields of SDCard struct for use in software
    SDCard.write.MBW_flag_first = 1;
    SDCard.write.MBW_startBlock = 0;
    SDCard.write.lastBlockWritten = 0;
    SDCard.read.MBR_flag_first = 1;
    SDCard.read.MBR_startBlock = 0;
    SDCard.read.lastBlockRead = 0;
    
    // Store that the initialization succeeded
    SDCard.init = 1;
}
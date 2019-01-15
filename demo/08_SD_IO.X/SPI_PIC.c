/**
 * @file
 * @author Tyler Gamvrelis
 *
 * Created on July 21, 2017, 11:41 AM
 *
 * @ingroup SPI
 */

/********************************* Includes **********************************/
#include "SPI_PIC.h"    

/***************************** Public Functions ******************************/
unsigned char spiTransfer(unsigned char byteToTransfer){   
    // Write byte to buffer. This byte will be transferred to the shift register
    // and transmitted in hardware. As the bits to be transmitted are shifted 
    // out, the bits to be received are shifted in
    SSPBUF = byteToTransfer; 
    
    // Wait until buffer has received a byte and it is latched into the buffer.
    // This will also indicate that the outgoing byte is sent
    while(!SSPSTATbits.BF | !SSPIF){
        continue;
    }

    return SSPBUF;
}

void spiSend(unsigned char val){
    spiTransfer(val);
}

unsigned char spiReceive(void){
    return spiTransfer(0xFF);
}

void spiInit(unsigned char divider){    
    mssp_disable();
    SSPSTAT = 0x00; // Default, data latched/shifted on rising edge
    
    // Configure SSPCON1. Set clock idle state high, and divider as parameter. 
    // Supposedly, the SD card requires that the clock idle state is high, so
    // be careful if you modify this and plan on using the SD card
    switch(divider){
        case 4:
            SSPCON1 = 0b00010000;
            break;
        case 16:
            SSPCON1 = 0b00010001;
            break;
        case 64:
            SSPCON1 = 0b00010010;
            break;
        default:
            SSPCON1 = 0b00010001; // FOSC/16
    }

    // Enforce correct pin configuration for relevant pins
    TRIS_SDO = 0;
    TRIS_SDI = 1;
    TRIS_SCK = 0;
    
    mssp_enable();
}
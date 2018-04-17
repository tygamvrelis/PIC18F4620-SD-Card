/* 
 * File:   SPI_PIC.c
 * Author: Tyler Gamvrelis
 *
 * Created on July 21, 2017, 11:41 AM
 */

/********************************** Includes **********************************/
#include "SPI_PIC.h"

/****************************** Public Interfaces *****************************/
unsigned char spiTransfer(unsigned char byteToTransfer){
    /* Transfers a byte using the SPI module, and returns the received byte.
     * 
     * Arguments: byteToTransfer, the byte to be transferred
     * 
     * Returns: the byte received
     */
    
    /* Write byte to buffer. This byte will be transferred to the shift register
     * and transmitted in hardware. As the bits to be transmitted are shifted 
     * out, the bits to be received are shifted in. */
    SPI_BUFFER = byteToTransfer; 
    
    /* Wait until buffer has received a byte and it is latched into the buffer.
     * This will also indicate that the outgoing byte is sent. */
    while(!BUFFER_IS_FULL | !SSPIF){    continue;   }

    /* Return the received byte. */
    return SPI_BUFFER;
}

void spiSend(unsigned char val){
    /* Sends a byte using the SPI module.
     * 
     * Arguments: val, the byte to be sent
     * 
     * Returns: none
     */
    
    spiTransfer(val);
}

unsigned char spiReceive(void){
    /* Receives a byte using the SPI module.
     *
     * Arguments: none
     * 
     * Returns: byte received
     */
    return spiTransfer(0xFF);
}

void spiInit(unsigned char divider){
    /* Initializes the MSSP module for SPI mode. All configuration register bits
     * are written to because operating in I2C mode could change them. See
     * section 17 in the PIC18F4620 datasheet for full details.
     * 
     * Arguments: divider, the FOSC divider for the clock (4, 16, or 64)
     * 
     * Returns: none
     */
    
    /* Disable module and configure SSPSTAT register */
    __MSSP_DISABLE();
    SSPSTAT = 0x00; // Default, data latched/shifted on rising edge
    
    /* Configure SSPCON1. Set clock idle state high, and divider as parameter. 
     * Supposedly, the SD card requires that the clock idle state is high, so
     * be careful if you modify this and plan on using the SD card. */
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
            SSPCON1 = 0b00010000;
    }

    /* Enforce correct pin configuration for relevant pins */
    TRIS_SDO = 0;
    TRIS_SDI = 1;
    TRIS_SCK = 0;
    
    /* Enable module */
    __MSSP_ENABLE();
}

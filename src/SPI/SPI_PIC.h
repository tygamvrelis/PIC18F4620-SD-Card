/* 
 * File:   SPI_PIC.h
 * Author: Tyler Gamvrelis
 *
 * Created on July 21, 2017, 11:42 AM
 */

#ifndef SPI_PIC_H
#define	SPI_PIC_H

/********************************** Includes **********************************/
#include <xc.h>
#include "../../src/PIC18F4620/configBits.h"

/*********************************** Macros ***********************************/
#define __MSSP_ENABLE()     SSPCON1bits.SSPEN = 1 // Enables the MSSP module
#define __MSSP_DISABLE()    SSPCON1bits.SSPEN = 0 // Disables the MSSP module

/*********************************** Defines **********************************/
#define TRIS_SDO            TRISCbits.TRISC5
#define TRIS_SDI            TRISCbits.TRISC4
#define TRIS_SCK            TRISCbits.TRISC3

#define SPI_BUFFER          SSPBUF
#define BUFFER_IS_FULL      SSPSTATbits.BF

/****************************** Public Interfaces *****************************/
unsigned char spiTransfer(unsigned char byteToSend);
void spiSend(unsigned char val);
unsigned char spiReceive(void);
void spiInit(unsigned char divider);


#endif	/* SPI_PIC_H */


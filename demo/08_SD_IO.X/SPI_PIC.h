/**
 * @file
 * @author Tyler Gamvrelis
 *
 * Created on July 21, 2017, 11:42 AM
 *
 * @defgroup SPI
 * @brief SPI driver
 * @{
 */

#ifndef SPI_PIC_H
#define SPI_PIC_H

/********************************* Includes **********************************/
#include <xc.h>
#include <configBits.h>

/********************************** Macros ***********************************/
#define TRIS_SDO TRISCbits.TRISC5 /**< Serial data out */
#define TRIS_SDI TRISCbits.TRISC4 /**< Serial data in  */
#define TRIS_SCK TRISCbits.TRISC3 /**< Serial clock    */

/** @brief Enables the MSSP module */
#define mssp_enable() SSPCON1bits.SSPEN = 1

/** @brief Disables the MSSP module */
#define mssp_disable() SSPCON1bits.SSPEN = 0

/************************ Public Function Prototypes *************************/
/**
 * @brief Transfers a byte using the SPI module, and returns the received byte.
 * @param byteToSend The byte to be sent
 * @return The byte received
 */
unsigned char spiTransfer(unsigned char byteToSend);

/**
 * @brief Receives a byte using the SPI module
 * @return The byte received
 */
unsigned char spiReceive(void);

/**
 * @brief Sends a byte using the SPI module
 * @param val The byte to be sent
 */
void spiSend(unsigned char val);

/**
 * @brief Initializes the MSSP module for SPI mode. All configuration register
 *        bits are written to because operating in I2C mode could change them.
 *        See section 17 in the PIC18F4620 datasheet for full details.
 * @param divider The FOSC divider for the MSSP clock (4, 16, or 64)
 */
void spiInit(unsigned char divider);

/**
 * @}
 */

#endif	/* SPI_PIC_H */
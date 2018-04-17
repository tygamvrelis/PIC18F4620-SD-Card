/* 
 * File:   lcd.h
 * Author: Michael Ding
 *
 * Created on August 12, 2016, 4:24 PM
 * Edited by Tyler Gamvrelis, summer 2017
 */

#ifndef LCD_H
#define	LCD_H

/********************************** Includes **********************************/
#include <xc.h>
#include <stdio.h>
#include "../../src/PIC18F4620/configBits.h"

/*********************************** Macros ***********************************/
/* Sets cursor position to start of second line. */
#define __lcd_newline() lcdInst(0xC0);

/* Clears both LCD lines. */
#define __lcd_clear() lcdInst(0x01);__delay_ms(15);

/* Sets cursor position to start of first line. */
#define __lcd_home() lcdInst(0x80);__delay_ms(5);

/* D -> controls display on/off
 * C -> controls cursor on/off
 * B -> controls blinking on/off */
#define __lcd_display_control(D, C, B)  lcdInst(8 | (D << 2) | (C << 1)| B);

/* Used in lcdNibble; pulses the LCD register enable to latch data. Interrupts
 * are disabled during this pulse, and the state of the global interrupt enable
 * (GIE) is restored to the state it was before this macro. */
#define __PULSE_E(){\
    LCDinterruptState = INTCONbits.GIE;\
    di();\
    E = 1;\
    __delay_us(LCD_DELAY);\
    E = 0;\
    __delay_us(100);\
    INTCONbits.GIE = LCDinterruptState;\
}

/*********************************** Defines **********************************/
#define RS          LATDbits.LATD2          
#define E           LATDbits.LATD3
#define	LCD_PORT    LATD   //On LATD[4,7] to be specific
#define LCD_DELAY   25 /* Only needs to be 1 microsecond in theory, but 25 was 
                        * selected experimentally to be safe. */

/********************************** Constants *********************************/
/* Display dimensions addressable by Hitachi controller embedded in the LCD. 
 * The embedded controller can address more characters than are visible on the
 * LCD provided in class so that it can be used in a variety of displays. Note
 * that the Hitachi HD44780 controller can only address 40 x 2 = 80 characters. */
const unsigned char LCD_HORZ_LIMIT = 40; // Number of addressable columns
const unsigned char LCD_VERT_LIMIT = 2; // Number of addressable rows

/* Display dimensions as seen in the real world. */
const unsigned char LCD_SIZE_HORZ = 16; // Number of visible columns
const unsigned char LCD_SIZE_VERT = 2; // Number of visible rows

const unsigned char LCD_RIGHT = 1;
const unsigned char LCD_LEFT = 0;

/****************************** Private variables *****************************/
static unsigned char LCDinterruptState;

/****************************** Public Interfaces *****************************/
void lcdInst(char data);
void putch(char data);
void lcdNibble(char data);
void initLCD(void);
void lcd_set_cursor(unsigned char x, unsigned char y);
void lcd_shift_cursor(unsigned char numChars, unsigned char direction);
void lcd_shift_display(unsigned char numChars, unsigned char direction);

#endif	/* LCD_H */


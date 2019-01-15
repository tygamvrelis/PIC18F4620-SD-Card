/**
 * @file
 * @author Tyler Gamvrelis
 * 
 * Created on July 10, 2017, 10:27 AM
 * 
 * @defgroup SD_Init
 * @brief Initializes the SD card. Information about the card is collected,
 *        during initialization such as its capacity card type, SD spec.
 *        version the card complies to, date of manufacture, etc. These fields
 *        are displayed on the character LCD
 * 
 * Preconditions:
 * @pre SD card is properly situated in its socket (indicated by CARD_IN LED)
 * @pre Jumper JP_SD is open (i.e. not shorted)
 */

#include <xc.h>
#include <configBits.h>
#include "SD_PIC.h"
#include "lcd.h"

void main(void) {
    // RD2 is the character LCD RS
    // RD3 is the character LCD enable (E)
    // RD4-RD7 are character LCD data lines
    LATD = 0x00;
    TRISD = 0x00;
    
    initLCD();
    lcd_display_control(true, false, false);
    
    // Initialize SD card
    initSD();
    
    // Main loop
    while(1){
        if(SDCard.init){
            lcd_clear();
            printf("SD init success!");
            lcd_set_ddram_addr(LCD_LINE2_ADDR);
            switch(SDCard.Type){
                case TYPE_SDHC_SDXC: printf("Type: SDHC/SDXC");
                    break;
                case TYPE_SDSC: printf("Type: SDSC");
                    break;
                case TYPE_MMC: printf("Type: MMC");
                    break;
                default:
                    printf("Type: Unknown");
                    break;
            }     
            __delay_ms(2000);

            lcd_clear();
            printf("BlkSize: %d b", SDCard.blockSize);
            lcd_set_ddram_addr(LCD_LINE2_ADDR);
            printf("#Blks: %lu", SDCard.numBlocks);
            __delay_ms(2000);
            
            lcd_clear();
            printf("SD Version: %u", SDCard.SDversion);
            lcd_set_ddram_addr(LCD_LINE2_ADDR);
            printf("MFG ID: 0x%x", SDCard.MID);
            __delay_ms(2000);
            
            lcd_clear();
            printf("OEM ID: %c%c", SDCard.OID >> 8, SDCard.OID & 0xFF);
            lcd_set_ddram_addr(LCD_LINE2_ADDR);
            unsigned char PNM[5];
            PNM[0] = SDCard.PHML & 0xFF;
            PNM[1] = (SDCard.PHML >> 8) & 0xFF;
            PNM[2] = (SDCard.PHML >> 16) & 0xFF;
            PNM[3] = (SDCard.PHML >> 24) & 0xFF;
            PNM[4] = SDCard.PHMH;
            printf("PNM: %c%c%c%c%c", PNM[4], PNM[3], PNM[2], PNM[1], PNM[0]);
            __delay_ms(2000);
            
            lcd_clear();
            printf("PRV: %u.%u", ((SDCard.PRV >> 4) & 0x0F), (SDCard.PRV & 0x0F));
            lcd_set_ddram_addr(LCD_LINE2_ADDR);
            printf("PSN: 0x%x", (SDCard.PSN >> 16)); // Print upper 16 bits
            printf("%x",SDCard.PSN & 0xFFFF); // Print lower 16 bits
            __delay_ms(2000);
            
            lcd_clear();
            unsigned short year = 2000 + ((SDCard.MDT >> 4) & 0xFF);
            unsigned char month = SDCard.MDT & 0xF;
            printf("MDT: %u/%u", month, year);
            lcd_set_ddram_addr(LCD_LINE2_ADDR);
            printf("CRC7: %u", SDCard.CRC);
            __delay_ms(2000);
            
            lcd_clear();
            printf("Number of MB:");
            lcd_set_ddram_addr(LCD_LINE2_ADDR);
            printf("%.2f ", SDCard.size);
            __delay_ms(2000);
        }
        else{
            printf("SD init failed!");
            while(1);
        }
    }
}
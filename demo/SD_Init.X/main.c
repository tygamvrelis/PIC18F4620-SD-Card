/*
 * File:   main.c
 * Author: Tyler Gamvrelis
 * 
 * Created on July 10, 2017, 10:27 AM
 * 
 * Description: Demonstration of SD card initialization. During initialization,
 *              certain information about the card is collected, such as its
 *              capacity, card type, SD specification version, date of
 *              manufacture, etc. These fields are displayed for users to see.
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

void main(void) {
    /* Configure pins, SFRs, and on-board modules. */
    machineConfig();
    
    /* Initialize LCD. */
    initLCD();
    __lcd_display_control(1, 0, 0);
    
    /* Initialize SD card. */
    initSD();
    
    /* Main loop */
    while(1){
        if(SDCard.init){
            __lcd_clear();__lcd_home();
            printf("SD init success!");
            __lcd_newline();
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

            __lcd_clear();__lcd_home();
            printf("BlkSize: %d b", SDCard.blockSize);
            __lcd_newline();
            printf("#Blks: %lu", SDCard.numBlocks);
            __delay_ms(2000);
            
            __lcd_clear();__lcd_home();
            printf("SD Version: %u", SDCard.SDversion);
            __lcd_newline();
            printf("MFG ID: 0x%x", SDCard.MID);
            __delay_ms(2000);
            
            __lcd_clear();__lcd_home();
            printf("OEM ID: %c%c", SDCard.OID >> 8, SDCard.OID & 0xFF);
            __lcd_newline();
            unsigned char PNM[5];
            PNM[0] = SDCard.PHML & 0xFF;
            PNM[1] = (SDCard.PHML >> 8) & 0xFF;
            PNM[2] = (SDCard.PHML >> 16) & 0xFF;
            PNM[3] = (SDCard.PHML >> 24) & 0xFF;
            PNM[4] = SDCard.PHMH;
            printf("PNM: %c%c%c%c%c", PNM[4], PNM[3], PNM[2], PNM[1], PNM[0]);
            __delay_ms(2000);
            
            __lcd_clear();__lcd_home();
            printf("PRV: %u.%u", ((SDCard.PRV >> 4) & 0x0F), (SDCard.PRV & 0x0F));
            __lcd_newline();
            printf("PSN: 0x%x", (SDCard.PSN >> 16)); // Print upper 16 bits
            printf("%x",SDCard.PSN & 0xFFFF); // Print lower 16 bits
            __delay_ms(2000);
            
            __lcd_clear();__lcd_home();
            unsigned short year = 2000 + ((SDCard.MDT >> 4) & 0xFF);
            unsigned char month = SDCard.MDT & 0xF;
            printf("MDT: %u/%u", month, year);
            __lcd_newline();
            printf("CRC7: %u", SDCard.CRC);
            __delay_ms(2000);
            
            __lcd_clear();__lcd_home();
            printf("Number of MB:");
            __lcd_newline();
            printf("%.2f ", SDCard.size);
            __delay_ms(2000);
        }
        else{
            printf("SD init failed!");
            while(1);
        }
    }
}

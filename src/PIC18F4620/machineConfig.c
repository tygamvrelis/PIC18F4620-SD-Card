
#include "machineConfig.h"

void machineConfig(void) {
    /* This function configures commonly-used registers.
     * 
     * Arguments: none
     * 
     * Returns: none
     */
       
    /********************************* PIN I/O ********************************/
    /* Write outputs to LATx, read inputs from PORTx. Here, all latches (LATx)
     * are being cleared (set low) to ensure a controlled start-up state */
    LATA = 0x00;
    LATB = 0x00;
    LATC = 0x00;
    LATD = 0x00;
    LATE = 0x00;
    
    /*TRIS SETS DATA DIRECTION: 0  = output; 1 = input. Default is  1. */
    TRISA = 0b00000000; // RA7-0 as outputs
    TRISB = 0b11110010; // RB1, RB4, RB5, RB6, RB7 as inputs (for keypad)
    TRISC = 0b10000000; /* RC3 is SCK/SCL (SPI/I2C),
                         * RC4 is SDI/SDA (SPI/I2C),
                         * RC5 is SDO (SPI),
                         * RC6 and RC7 are UART TX and RX, respectively. */
    TRISD = 0b00000001; /* RD0 is the GLCD chip select (tri-stated so that it's
                         * pulled up by default),
                         * RD1 is the GLCD register select,
                         * RD2 is the character LCD RS,
                         * RD3 is the character LCD enable (E),
                         * RD4-RD7 are character LCD data lines. */
    TRISE = 0b00000100; /* RE2 is the SD card chip select (tri-stated so that
                         * it's pulled up by default).
                         * Note that the upper 4 bits of TRISE are special 
                         * control registers. Do not write to them unless you 
                         * read §9.6 in the datasheet */

    nRBPU = 1; // Disable PORTB's pull-up resistors
    
    /************************** A/D Converter Module **************************/
    ADCON0 = 0x00;  //Disable ADC
    ADCON1 = 0b00001111; // Set all A/D ports to digital (pg. 222)
    CVRCON = 0x00; // Disable comparator voltage reference (pg. 239)
    CMCONbits.CIS = 0; // Connect C1 Vin and C2 Vin to RA0 and RA1 (pg. 233)
    ADCON2 = 0b10110001; // Right justify A/D result, 16TAD, FOSC/8 clock
    
    /******************************* INTERRUPTS *******************************/
    INT1IE = 0; // Disable external interrupts on INT1/RB1 for keypad
    PEIE = 1; // Enable peripheral interrupts
    di(); // Disable all interrupts for now
}

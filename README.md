# PIC18F4620-SD-Card

SD cards are capable of storing large amounts of memory in a "non-volatile" fashion, meaning that the information
is preserved even when power is turned off (this relies on the phenomena of floating-gate MOSFETs). An SD card is
a slave device, and so it is communicated with my sending it commands as the master. When a SD card is first
powered up, it will be unresponsive to any commands until it is initialized. Initialization is a "handshake" that
is highly specific; in fact, all commands are sufficiently specific that there is a specification detailing how to
implement them. The implementation in this sample code follows the SD Physical Layer Simplified Specification 
Version 6.00.

## Contents
This project contains source files (in the src folder) for communication with a SD card via SPI using a PIC18F4620. Implementations of initialization, single block read, multiple block read, single block write, multiple block write, and erase are provided.

Two projects to demonstrate the library are provided in the demo folder. These demos make use of printing characters to a HD44780-based character LCD.

## 1. SD_Init
In this sample, initialization is performed, during which time certain card-specific data is retrieved. Some
examples are the storage capacity of the card, the manufacturer ID, the date the card was manufactured, and
the SD version the card controller complies to. These details are printed to the character LCD.

## 2. SD_IO
In this sample, initialization is performed. After this, the first sector (512 bytes) of the card is erased. Then,
0x00 to 0xFF is written into the first sector twice. Then, sectors 1-1000 are written to with the constant 0x34.
Then, the data from sector 1 is read back and averaged. Then, the data from sectors 1-1000 are read back and averaged.
The intermediate results of these exchanges are printed to the character LCD.

Also:

SD cards are organized into 512-byte blocks called "sectors". Each sector is addressable by a 32-bit argument in
read/write/erase commands to the SD card, and each read or write generally must begin at the start of a sector.
For the following explanation, one must understand that SDSC stands for "SD standard
capacity", SDHC stands for "SD high capacity", and SDXC stands for "SD extended capacity".

- For SDHC/SDXC cards, address 0 corresponds to the first sector (i.e begins at byte 0 and ends at byte 511).
  Address 1 corresponds to the second sector (i.e. begins at byte 512 and ends at byte 1023). So on, so forth.

- For SDSC cards, address 0 corresponds to byte 0, address 1 corresponds to byte 1, and so on. These cards can
  get away with byte addressing because they are so small (maximum addressable memory would be 2^32 - 1 bytes,
  which is 4 GB).

There are two ways to read and write to SD cards. The first way is by reading/writing one block at a time; these 
commands are called READ_SINGLE_BLOCK and WRITE_BLOCK in the SD specification. The second way is by continuously
writing data over multiple blocks; these commands are called READ_MULTIPLE_BLOCK and WRITE_MULTIPLE_BLOCK in the SD
specification.

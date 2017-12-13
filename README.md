# EEPROMProgrammer
Simple Arduino code to program an EEPROM for a simple 8-bit breadboard computer project

So far just programs a 7 segment display. Borrowed / copied heavily from Ben Eater's work.

TODO: Next up is code to program the microcode for decoding/running assembly language instructions


# Microcode programmer
Use a 2048 word EEPROM, with 11 address bits (a10...a0).

* Top 4 bits: (a10...a7): unused (hardcoded to zero, so we only program when they are set to zero)
* Next 3 bits (a6...a4): what microcode step we are on (0-7, though really just use 0...4)
* Next 4 bits (a3...a0): what instruction opcode we are (instructions are 8 bits, top 4 are the opcode)

At each address we program 2 EEPROMs, each of which stores 1 byte for that address.
Each bit controls one control line, allowing us 16 control lines total.

Left EEPROM 8 bits represent (MSB on left): TODO...
Right EEPROM 8 bits represent (MSB on left):

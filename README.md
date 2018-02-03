# EEPROMProgrammer
Simple Arduino code to program EEPROMs for a simple 8-bit breadboard computer project

We can program the EEPROM to do a few things:

1. EEPROM can take in an 8-bit number and then render it in decimal by driving a 7 segment display. It can interpret the bits as either a signed or unsigned integer (controllable via another input line). The output can drive 1 of 4 displays, for the leading sign bit place, hundreds place, tens place, and finally ones place. (Which digit is is rendering is also controlled by 2 input lines). This part borrowed / copied heavily from Ben Eater's work.
2. EEPROM can serve as an instructor decoder, taking in an opcode from an assembly language instruction, and outputting what microcode lines should be high or low for that opcode and the selected step for decoding that instruction.
3. EEPROM can serve as a stored program repository, or essentially a "hard-drive". Basically, it stores a copy of what RAM should be before the program actually runs. "Load" mode can be selected on the computer, which basically does a total copy from this EEPROM into the computer's memory. Which RAM snapshot is selected to copy into RAM can be controlled by some input lines. Once "load" mode is finished, the computer can be put in "execute" mode. This allows one to save programs persistently across power outages, and saves all the manual work of setting RAM contents via DIP switches.

In general, see src for documentation.

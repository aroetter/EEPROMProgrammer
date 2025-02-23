# EEPROMProgrammer
Simple Arduino code to program EEPROMs for a simple 8-bit breadboard computer project

More detailed documentation for the actual computer can be found [here](https://docs.google.com/document/d/15yL4C0ukLrMd32EVRmozn8x13IjeZRVXZLeEXENS4ys/edit?usp=sharing).

We can program the EEPROM to do a few things. We control what EEPROM is written by the EEPROMTypeT enum that is defined above setup(), and set to the relevant bolded value below.

1. *EIGHT_BIT_DISPLAY*. EEPROM can take in an 8-bit number and then render it in decimal by driving a 7 segment display. It can interpret the bits as either a signed or unsigned integer (controllable via another input line). The output can drive 1 of 4 displays, for the leading sign bit place, hundreds place, tens place, and finally ones place. (Which digit is is rendering is also controlled by 2 input lines). This part borrowed / copied heavily from Ben Eater's work.
2. *MICROCODE_AND_STORED_PROGRAMS* In this mode the EEPROM actually stores values for two distinct behaviors. (1) EEPROM can serve as an instruction decoder, taking in an opcode from an assembly language instruction, and outputting what microcode lines should be high or low for that opcode and the selected step for decoding that instruction. (2) EEPROM can serve as a stored program repository, or essentially a "hard-drive". Basically, it stores a copy of what RAM should be before the program actually runs. 
3. *FOUR_BIT_DISPLAY*. An EEPROM can drive the displays for a 4-bit adder. Basically, we have the following equation made up of 7 of the 7 segment displays.
 AA +/- BB = CCC. (Each capital letter is one 7-segment display)
 AA and BB are both unsigned 4 bit numbers (0-15 in decimal). CCC is either an unsigned 5 bit result (0-30) or, in subtraction mode, it's a signed result (-15 - 15). One EEPROM can be programmed to drive
 both the operator numbers, and the result number.
4. *READONLY*. Just print out the values of the EEPROM.

In addition, to verify things are working, this repo saves the written values (aka output from READONLY mode), of each of the three different EEPROM contents. See the files EEPROMContents.*.txt


In general, see src for documentation.

# EEPROMProgrammer
Simple Arduino code to program EEPROMs for a simple 8-bit breadboard computer project

More detailed documentation for the actual computer can be found [here](https://docs.google.com/document/d/15yL4C0ukLrMd32EVRmozn8x13IjeZRVXZLeEXENS4ys/edit?usp=sharing).

We can program the EEPROM to do a few things:

1. EEPROM can take in an 8-bit number and then render it in decimal by driving a 7 segment display. It can interpret the bits as either a signed or unsigned integer (controllable via another input line). The output can drive 1 of 4 displays, for the leading sign bit place, hundreds place, tens place, and finally ones place. (Which digit is is rendering is also controlled by 2 input lines). This part borrowed / copied heavily from Ben Eater's work.
2. EEPROM can serve as an instruction decoder, taking in an opcode from an assembly language instruction, and outputting what microcode lines should be high or low for that opcode and the selected step for decoding that instruction.
3. EEPROM can serve as a stored program repository, or essentially a "hard-drive". Basically, it stores a copy of what RAM should be before the program actually runs. "Load" mode can be selected on the computer, which basically does a total copy from this EEPROM into the computer's memory. Which RAM snapshot is selected to copy into RAM can be controlled by some input lines. Once "load" mode is finished, the computer can be put in "execute" mode. This allows one to save programs persistently across power outages, and saves all the manual work of setting RAM contents via DIP switches.
4. An EEPROm can drive the displays for a 4-bit adder. Basically, we have the following equation
made up of 7 of the 7 segment displays.
 AA +/- BB = CCC. (Each capital letter is one 7-segment display)
 AA and BB are both unsigned 4 bit numbers (0-15 in decimal). CCC is either an unsigned 5 bit result (0-30) or, in subtraction mode, it's a signed result (-15 - 15). One EEPROM can be programmed to drive
 both the operator numbers, and the result number.


In general, see src for documentation.

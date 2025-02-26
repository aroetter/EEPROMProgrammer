// See comments at the top of the source for the 8BitComputer program for help troubleshooting
// Mac<->Arduino Serial Post Comms Issues
// https://github.com/aroetter/8BitComputer

// define which arduino pins are used for what
#define SHIFT_DATA    2
#define SHIFT_CLK     3
#define SHIFT_LATCH   4
#define EEPROM_D0     5
#define EEPROM_D7    12
#define WRITE_ENABLE 13

#define EEPROM_NUM_BYTES 2048

// Map a digit to what segments of a display to light up t represent that digit as a decimal char
// e.g. DIGITS[4] = the segments to light up to render a 4.
byte DIGITS[] = { 0x7e, 0x30, 0x6d, 0x79, 0x33, 0x5b, 0x5f, 0x70, 0x7f, 0x7b};

byte DIGIT_NEGATIVE = 0x01;
byte DIGIT_BLANK = 0x00;

// Given a 4-bit number in the range [0...15], return a human readable string of that number in binary
// e.g. passing in 10 returns "1010"
void convert4BitIntToBinaryString(char out[5], byte val) {
  for(int i = 0; i < 4; ++i) {
    out[3-i] = (val & 1) ? '1' : '0';
    val = val >> 1;
  }
  out[4] = '\0';
}

// Set the EEPROM address, and whether we will read to, or write from it.
void setAddress(uint16_t address, bool readMode) {
  // set the top bit to low when readMode is true.
  // that bit controls the output_enable pin on the EEPROM, which is active low.
  // bring it high for write mode.
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, (address >> 8) | (readMode ? 0x00 : 0x80));
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address); // drops upper top bits, shifts lower byte

  // now toggle clock pulse
  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}

// Return the byte found at a specific address
byte readEEPROM(uint16_t address) {
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; ++pin) {
    pinMode(pin, INPUT);
  }
  setAddress(address, /* readMode */ true);
  byte data = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; --pin) {
    data = (data << 1) + digitalRead(pin);
  }
  return data;
}

// Write the given byte to a specific address
void writeEEPROM(uint16_t address, byte data) {
  setAddress(address, /* readMode */ false);
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; ++pin) {
    pinMode(pin, OUTPUT);
  }
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; ++pin) {
    digitalWrite(pin, data & 1);
    data = data >> 1;
  }
  digitalWrite(WRITE_ENABLE, LOW); // active low
  delayMicroseconds(1);
  digitalWrite(WRITE_ENABLE, HIGH);
  delay(5); // msec // works fine for me, but in video needs to be upped to 10.
}

// Print out entire EEPROM contents, 16 bytes per line
void printContents() {
  Serial.println("\n------------------------------------------------------");
  for (int base = 0; base < EEPROM_NUM_BYTES; base += 16) {
    byte data[16];
    for (int offset = 0; offset < 16; ++offset) {
      data[offset] = readEEPROM(base + offset);
    }
    char buf[80];
    sprintf(buf, "%03x: %02x %02x %02x %02x %02x %02x %02x %02x   %02x %02x %02x %02x %02x %02x %02x %02x",
      base, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
      data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
    Serial.println(buf);
  }  
  
}

// Program an EEPROM to drive a 4 digit display to show an 8-bit number (e.g. a register's contents.)
//
// We have 4 digits, arranged from left to right as:
// (sign place), (hundreds place), (tens place), (ones place)
// 
// EEPROM Memory is broken up into 256 address chunks. Each chunk is addressed with an 8-bit number
// from 0-255. The value stored represents how the given decimal digit display should represent the 8-bit
// number used to address it (i.e. what segments of the display need to be turned on)
// e.g. at address 123, we store how to light up the display to make a "3", which is the ones place for 123.
// We store both signed & unsigned representations of the 8-bit index address.
// Memory is laid out as follows:
// [0000...0255]: ones place digit for unsigned decimal representation
// [0256...0511]: tens place digit     ""              ""
// [0512...0767]: hundreds place digit ""              ""
// [0768...1023]: signs place digit    ""              "" (always blank for unsigned numbers)
// [1024...1279]: ones place digit for signed decimal representation
// [1280...1535]: tens place digit     ""              ""
// [1536...1791]: hundreds place digit ""              "" 
// [1792...2047]: signs place digit    ""              "" (either a negative sign or blank)
// 
// Another way to think about memory layout is, for the 11 address pins a10...a0
// a10: high if we are in 2s complement/signed int mode, 0 in unsigned mode
// a9-a8: 2-bit counter value: 00 = 1s digit, 01=10s digit, 10=100s digit, 11= left-most sign digit
// a7-a0: the byte of data to display (the number 0-255)
void write8BitDisplayEEPROM() {

  // program the lower half of the chip, unsigned numbers
  Serial.println("Programming unsigned mode...");
  for (int value = 0; value < 256; ++value) {
    uint16_t ones_addr = value; // ones place address
    uint16_t tens_addr = 256 + value;        // make upper byte 0000 0001
    uint16_t hundreds_addr = 512 + value;    // make upper byte 0000 0010
    uint16_t sign_addr = 768 + value;        // make upper byte 0000 0011

    writeEEPROM(ones_addr, DIGITS[value % 10]);
    writeEEPROM(tens_addr, DIGITS[(value / 10) % 10]);
    writeEEPROM(hundreds_addr, DIGITS[(value / 100) % 10]);
    writeEEPROM(sign_addr, 0);    
  }
  
  // program the upper half of the chip, 2s complement
  Serial.println("Programming signed mode...");
  for(int value = -128; value < 128; ++value) {
    uint16_t ones_addr = 1024 + (byte) value;
    uint16_t tens_addr = 1280 + (byte) value;
    uint16_t hundreds_addr = 1536 + (byte) value;
    uint16_t sign_addr = 1792 + byte (value);

    writeEEPROM(ones_addr, DIGITS[abs(value) % 10]);
    writeEEPROM(tens_addr,  DIGITS[abs(value / 10) % 10]);
    writeEEPROM(hundreds_addr, DIGITS[abs(value / 100) % 10]);
    writeEEPROM(sign_addr, (value < 0) ? DIGIT_NEGATIVE : DIGIT_BLANK);
  }
}

// Program an EEPROM to drive displays for the addition/subtraction of 2 4 bit numbers into a 5 bit result.
// We have 7 7-segment LCD displays to represent an equation as follows:

// AA +/- BB = CCC
// Where each A, B, or C are a 7-segment LCD display.
//
// One EEPROM drives AA and BB (we call it the Operand EEPROM). The other EEPROM drives CCC (Result EEPROM).
// Both AA and BB are unsigned 4 bit numbers, rendered in decimal as 0-15.
// In addition mode, CCC is an unsigned number, between 0-30. (it's impossible to get 31 just given the operands)
// In substraction mode, CCC is a 2s complement number, between [-15 and 15] (impossible to get 16 given the operands)

// EEPROM memory layout, for the 11 address pins a10...a0:
// a10: 1 for the ResultEEPROM, 0 for the operandEEPROM
// a9-a8: the 2-bit counter value telling what digit we are rendering.
//        in OperandEEPROM mode it's:
//           11: AA's tens digit, 10: AA's ones digit, 01: BB's tens digit, 00: BB's ones digit.
//        in ResultEEPROM it's:
//           11: unused (only drives 3 digits), 10: sign prefix, 01: tens digit, 00: ones digit. 
//
// The last bits are then EEPROM dependent:
//
// For the operand EEPROM:
// a7-a4: the four bytes of data that represent a decimal [0-15] in operand A.
// a3-a0: the four bytes of data that represent a decial [0-15] in operand B.
// 
// For the Result EEPROM:
// a7-a6: always 0.
// a5: 1 for "render in 2s complement", 0 for render in unsigned
// a4-a0: the 5-bit number to render
void write4BitDisplayEEPROM() {
  Serial.println("Programming operator EEPROM...");
  // for every possible AABB combination.
  for (uint8_t aa = 0; aa < 16; ++aa) {
    for (uint8_t bb = 0; bb < 16; ++bb) {
      uint8_t aabb = (aa << 4) | bb; // both 4 bit operands AA and BB, concatted together into an 8-bit number.
      uint16_t upper_a_digit_addr = 768 + aabb;   // make upper byte 0000 0011. aka a10 to 0, a9a8 to 11
      uint16_t lower_a_digit_addr = 512 + aabb;   // make upper byte 0000 0010. aka a10 to 0, a9a8 to 10

      uint16_t upper_b_digit_addr = 256 + aabb;   // make upper byte 0000 0001. aka a10 to 0, a9a8 to 01
      uint16_t lower_b_digit_addr = aabb;         // make upper byte 0000 0000. aka a10 to 0, a9a8 to 00

      int aa_tens_digit = aa / 10;
      // if the tens digit is zero, just omit the leading zero (e.g. render 5 as "5", not "05");
      writeEEPROM(upper_a_digit_addr, (aa_tens_digit != 0) ? DIGITS[aa_tens_digit] : DIGIT_BLANK);
      writeEEPROM(lower_a_digit_addr, DIGITS[aa % 10]);
      
      int bb_tens_digit = bb / 10;
      writeEEPROM(upper_b_digit_addr, (bb_tens_digit != 0) ? DIGITS[bb_tens_digit] : DIGIT_BLANK);
      writeEEPROM(lower_b_digit_addr, DIGITS[bb % 10]);
    }
  }
   
  Serial.println("Programming result EEPROM (unsigned)...");
  for(uint8_t value = 0; value < 31; ++value) {
    // address should be 0000 01cc 000x xxxx // (cc is the counter here, xxxxx is the 5-bit unsigned result)
    uint16_t ones_addr = 1024 + value;     // sets upper byte to 0000 0100
    uint16_t tens_addr = 1280 + value;     // sets upper byte to 0000 0101
    uint16_t sign_addr = 1536 + value;     // sets upper byte to 0000 0110
    writeEEPROM(ones_addr, DIGITS[value % 10]);

    int tens_digit = value / 10;
    writeEEPROM(tens_addr, (tens_digit != 0) ? DIGITS[tens_digit] : DIGIT_BLANK);
    writeEEPROM(sign_addr, DIGIT_BLANK);
  }

  Serial.println("Programming result EEPROM (signed)...");
  for(int8_t value = -15; value < 16; ++value) {
    // address should be 0000 01cc 001x xxxx // (cc is the counter here, xxxxx is the 5-bit signed result)
    
    // set the top 3 bits to 0. this leaves a 5-bit signed 2s complement number of the same value.
    // adding in 32 sets a5 to 1.
    uint8_t five_bit_value = ((uint8_t) value) & 0x1f; // TODO mask out the bottom 5 bits only, as in & with 0x1f
    uint16_t ones_addr = 1024 + 32 + five_bit_value; // set upper 12 bits to 0000 0100 0010
    uint16_t tens_addr = 1280 + 32 + five_bit_value; // set upper 12 bits to 0000 0101 0010
    uint16_t sign_addr = 1536 + 32 + five_bit_value; // set upper 12 bits to 0000 0110 0010
        
    writeEEPROM(ones_addr, DIGITS[abs(value) % 10]);
    int tens_digit = abs(value) / 10;
    writeEEPROM(tens_addr, (tens_digit != 0) ? DIGITS[tens_digit] : DIGIT_BLANK);

    // start out by blanking out the far left (this may get overridden later)
    writeEEPROM(sign_addr, DIGIT_BLANK);
    
    // now figure out where minus sign goes, either it's in the far left slot (e.g. "-12"),
    //   or it's in the middle slot (e.g. " -5"), or there isn't one (e.g. " 14")either there isn't one,
    //   or it's in far left (e.g. "-10"), or in middle (e.g. " -4");
    if (value <= -10) {
      writeEEPROM(sign_addr, DIGIT_NEGATIVE);
    } else if (value < 0) {
      writeEEPROM(tens_addr, DIGIT_NEGATIVE);
    } // otherwise number is positive, nothing to do.
  }
}


// Clears an EEPROM, setting all data to zero.
void eraseEEPROM() {
  Serial.print("Erasing EEPROM");
  for (uint16_t addr = 0; addr < EEPROM_NUM_BYTES; ++addr) {
    writeEEPROM(addr, 0);
    if (0 == addr % (EEPROM_NUM_BYTES / 16)) {
      Serial.print(".");
    }
  }
  Serial.println("done.");
}

// Control Line Bits. Top byte here is unused, leaving 24 bits for control lines.
// Left EEPROM control bits (2nd most significant byte)
static uint32_t HALT = 0x00800000; // Clock - Stop the clock
static uint32_t   MI = 0x00400000; // Memory Address Register - Input from Bus
static uint32_t   RI = 0x00200000; // RAM - Input from Bus
static uint32_t   RO = 0x00100000; // RAM - Output to Bus
static uint32_t   IO = 0x00080000; // Instruction Register - Output to Bus
static uint32_t   II = 0x00040000; // Instruction Register - Input from Bus
static uint32_t   AI = 0x00020000; // A Register - Input from Bus
static uint32_t   AO = 0x00010000; // A Register - Output to Bus

// Center EEPROM control bits (3rd most significant byte)
static uint32_t   SO = 0x00008000; // ALU - Output Sum to Bus
static uint32_t   SU = 0x00004000; // ALU - Subtract mode (vs. Add otherwise)
static uint32_t   BI = 0x00002000; // B Register - Input from Bus
static uint32_t   OI = 0x00001000; // Output (LCD) Register - Input from Bus
static uint32_t   CE = 0x00000800; // Program Counter - Counter Enable: PC will increment on each clock
static uint32_t   CO = 0x00000400; // Program Counter - Output to Bus
static uint32_t    J = 0x00000200; // Program Counter - Jump: Input from Bus
static uint32_t   PO = 0x00000100; // Stored Program ("Hard Drive") EEPROM: Output to Bus

// Right EEPROM control bits (least significant byte)

static uint32_t   XO = 0x00000080; // Input Register X - Write to Bus
static uint32_t   YO = 0x00000040; // Input Register Y - Write to Bus
static uint32_t   JC = 0x00000020; // Jump Carry (JC) - Jump only if ALU overflow bit is set
// down to             0x00000001; // last control bit

// This is changable by moving the reset wire on the 3 bit counter on the control
// logic breadboard. This only exists here as a sanity check to enforce consistency
// with other data structures below.
#define NUM_MICROCODE_PER_OPCODE 5

// Every instruction starts with a fetch, so we program that here.
// Way to read this is:
// On 1st clock cycle, a fetch sets the CO & MI control lines
// On 2nd clock cycle, a fetch sets the RO, II, & CE control lines
static uint32_t FETCH_MICROCODE[] = {CO | MI, RO | II | CE };
static int FETCH_MICROCODE_LEN = sizeof(FETCH_MICROCODE) / sizeof(FETCH_MICROCODE[0]);

// must be == (NUM_MICROCODE_PER_OPCODE - FETCH_MICROCODE_LEN).
// will fail initialization checks if not
#define NUM_CUSTOM_MICROCODE_PER_OPCODE 3

// we guarantee, via checking code below, that the following array is this size
#define NUM_OPCODES 16

// Define the symbolic assembly instruction name to binary opcode mapping here.
// These must be consecutive integers starting at zero, and in the same order
// as the OpCodeDefT OPCODE[] array below (this is checked at init time).
enum OpCodeName {
  NOP =   0 << 4, // 0000: Do nothing,
  LDA =   1 << 4, // 0001: Load A (write from RAM into A)
  ADD =   2 << 4, // 0010: Add value from ram with A register, store result back in A
  SUB =   3 << 4, // 0011: Subtract value from ram from A register, store result back in A

  STA =   4 << 4, // 0100: // Store A (write from A -> RAM)
  LDI =   5 << 4, // 0101: Load Immediate (into A)
  JMP =   6 << 4, // 0110: Jump
  STX =   7 << 4, // 0111: Store X (write from X input register -> RAM)

  STY =   8 << 4, // 1000: Store Y (write from Y input register -> RAM)
  JCY =   9 << 4, // 1001: Jump Carry. Jump iff ALU carry bit was set on last operation.
  SUI =  10 << 4, // 1010: Subtract Immediate
  NUL1 = 11 << 4, // 1011: Unused

  NUL2 = 12 << 4, // 1100: Unused
  NUL3 = 13 << 4, // 1101: Unused
  OUT  = 14 << 4, // 1110: Output register A to output register (LCD display)
  HLT  = 15 << 4  // 1111: Halt the computer by stopping the clock
};

typedef struct OpCodeDefT {
  OpCodeName opcode;
  uint32_t microcode[NUM_CUSTOM_MICROCODE_PER_OPCODE];
} OpCodeDefT;


// This defines what each assembly language instruction does, i.e. what microcode
// runs for each opcode. unused steps are set to 0.
static OpCodeDefT OPCODE[] = {
  {NOP,  {0, 0, 0}},
  {LDA,  {IO|MI, RO|AI, 0}},
  {ADD,  {IO|MI, RO|BI, SO|AI}},
  {SUB,  {IO|MI, RO|BI, SO|AI|SU}},

  {STA, {IO|MI, AO|RI, 0}},
  {LDI, {IO|AI, 0, 0}},
  {JMP, {IO|J, 0, 0}},
  {STX, {IO|MI, XO|RI, 0}},

  {STY, {IO|MI, YO|RI, 0}},
  {JCY, {IO|JC, 0, 0}},
  {SUI, {IO|BI, SO|AI|SU, 0}},

  // TODO: Add a "Zero Address" instruction, e.g ZRO 13 will set data at RAM address @13 to
  // zero.
  // Something like: IO|MI, RI. Will only work if, when nothing is on bus, the
  // pull down resistors ensure all data lines are zero. If not, we'd need HW
  // to output all zeroes, and a control line (ZO?) to enable that.
  {NUL1, {0, 0, 0}},
  
  {NUL2, {0, 0, 0}},
  {NUL3, {0, 0, 0}},
  {OUT,  {AO|OI, 0, 0}},
  {HLT,  {HALT, 0, 0}},
};

// Code that is needed regardless of what EEPROM we're programming
void doCommonInit() {
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  digitalWrite(WRITE_ENABLE, HIGH); // active low
  pinMode(WRITE_ENABLE, OUTPUT);
  Serial.begin(57600);

  // Sanity check the static microcode definitions
  // Do we have the right number of microcode definitions?
  if (sizeof(OPCODE) / sizeof(OPCODE[0]) != NUM_OPCODES) {
    Serial.println("Wrong number of opcodes defined!");
    Serial.flush();
    abort();
  }

  // Does each opcode have the right number of microcode steps?
  int per_opcode_microcode_len = sizeof(OPCODE[0].microcode) / sizeof(OPCODE[0].microcode[0]);
  if ((per_opcode_microcode_len + FETCH_MICROCODE_LEN) != NUM_MICROCODE_PER_OPCODE) {
    char buf[100];
    snprintf(buf, 100, "Each opcode must be = %d microcodes. Fix static data structures. Terminating...",
      NUM_MICROCODE_PER_OPCODE);
    Serial.println(buf);
    Serial.flush();
    abort();
  }

  // make sure each opcode is defined in order with a consecutive binary value
  for (int i = 0; i < NUM_OPCODES; ++i) {
    byte expected_opcode = (i << 4);
    if (OPCODE[i].opcode != expected_opcode) {
      Serial.println("Each opcode must have a consecutive and increasing MSB.");
      Serial.flush();
      abort();
    }
  }
}

// Write the given 3-byte data to three different EEPROMs at the specified address
// modify the addresses bits a8 & a7 to specify which of the 3 EEPROMs.
// 2nd most-sig byte goes to LEFT_EEPROM, next byte goes to CENTER_EEPROM,
// least sig byte goes to RIGHT_EEPROM
void write24BitControlWordToEEPROMs(uint16_t addr, uint32_t data) {
  const uint16_t LEFT_EEPROM =   0x000;
  const uint16_t CENTER_EEPROM = 0x080;  
  const uint16_t RIGHT_EEPROM =  0x100;

  // write data for the all 3 EEPROMs. Hard wiring (per physical EEPROM position on the breadboard)
  // will determine which one is read
  writeEEPROM(addr | LEFT_EEPROM, data >> 16);
  writeEEPROM(addr | CENTER_EEPROM, data >> 8);
  writeEEPROM(addr | RIGHT_EEPROM, data);
}


// Write the Microcode EEPROMs. EEPROMs are addressable via 11 address lines [a10...a0]
// Addressing for the microcode EEPROMS is done as follows:
//
// a10: unused, always 0 (1 means we are on the stored EEPROM, see below).
// a09: MC type. 0 for regular microcode to execute programs from RAM,
//               1 for the microcode to load a program from the "hard-drive (stored assembly program)" EEPROM
// a8-a7: which EEPROM. 00 for left, 01 for center, 10 for right EEPROM
//
// Remaining bits depend on the MC type.
//
// When MC type == 0: (Running a program from RAM)
//   a6-a5-a4-a3: 4 bits to represent the opcode
//   a2....a0: 3 bits to represent the microcode step we are on (only 0-4 used per NUM_MICROCODE_PER_OPCODE)
// When MC type == 1: Running the "load pre-canned program from 'hard-drive' EEPROM"
//   a6: unused, always 0
//   a5-a4-a3-a2-a1-a0: the microcode step to run of the "load program from another EEPROM into RAM" program
//                      can be up to 64 steps (2^6)
//     
void writeMicroCodeEEPROM() {  
  // Iterate over every microcode step w/in that opcode
  // Set the fetch instructions, then the custom microcode for each one
  for(uint8_t opcode = 0; opcode < NUM_OPCODES; ++opcode) {
    char buf[100];
    OpCodeDefT mc = OPCODE[opcode];
    char binaryStr[5];
    convert4BitIntToBinaryString(binaryStr, opcode);
    snprintf(buf, 100, "Programming binary opcode = %s.", binaryStr);
    Serial.println(buf);
    for(uint8_t step = 0; step < NUM_MICROCODE_PER_OPCODE; ++step) {
      // generate the lower 7 bits of the address: for opcode and microcode step count
      uint16_t addr = (opcode << 3) | step;
       
      // for the current step: write either the fetch microcode instruction, or the opcode specific microcode
      uint32_t data;
      if (step < FETCH_MICROCODE_LEN) {
        data = FETCH_MICROCODE[step];
      } else {
        data = mc.microcode[step - FETCH_MICROCODE_LEN];
      }
      write24BitControlWordToEEPROMs(addr, data);
    }
  }

  // Now write out the microcode instructions for the "Load program from another EEPROM into RAM"
  Serial.println("Programming microcode to load stored program from another EEPROM into RAM.");
  static int LOAD_PROG_MICROCODE_LEN = 64;
  static uint32_t STEP1 = CO|MI, STEP2 = PO|RI|CE;

  #if 1
  static uint32_t LOAD_PROG_MICROCODE[] = {
    STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, 
    STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, 
    STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, 
    STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, 
    HALT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 
  };
#else
  // Silly microcode that just loops through every control line on, one at a time, one per clock cycle.
  // Makes it easy to debug the microcode board hardware, but otherwise totally useless. Final step
  // is turning them all on.
  static uint32_t LOAD_PROG_MICROCODE[] = {
    HALT, MI, RI, RO, IO, II, AI, AO,
    SO,  SU, BI, OI, CE, CO,  J, PO,
    XO,  YO, JC, 0xffffffff, 0, 0, 0, 0,
    0,    0,  0,  0,  0,  0,  0,  0,
    0,    0,  0,  0,  0,  0,  0,  0,
    0,    0,  0,  0,  0,  0,  0,  0,
    0,    0,  0,  0,  0,  0,  0,  0,
    0,    0,  0,  0,  0,  0,  0,  0
  };
#endif
  
  if ((sizeof(LOAD_PROG_MICROCODE) / sizeof(LOAD_PROG_MICROCODE[0])) != LOAD_PROG_MICROCODE_LEN) {
    Serial.println("Wrong sized 'Load Program from EEPROM' microcode definition!");
    Serial.flush();
    abort();
  }

  uint16_t addr = 0x200; // set a9.
  for (uint16_t i = 0; i < LOAD_PROG_MICROCODE_LEN; ++i) {
    write24BitControlWordToEEPROMs(addr | i, LOAD_PROG_MICROCODE[i]);
  }
}

// Hard-code 8 16 byte programs here.
static byte STORED_PROGRAMS[] = {
  // Program #0 (000): Count by 2, Starting at the X register's value
  LDI | 2, // This is the hardcoded "2" to count by.
  STA | 15,
  STX | 14,
  LDA | 14,
  OUT,
  ADD | 15,
  OUT,
  JMP | 5,
  0, 0, 0, 0, 0, 0,
  0, // @14: Store the X register's value here. It's where we start incrementing from
  0, // @15: Number we are incrementing by

  // Program #1 (001): Fibonacci
  LDI | 0,
  STA | 15,
  LDI | 1,
  OUT,
  STA | 14,
  ADD | 15,
  STA | 15,
  OUT,
  ADD | 14,
  JMP | 3,
  0, 0, 0, 0,
  0, // Used for storage (addr 14)
  0, // Used for storage (addr 15)

  // Program #2 (010): Fibonacci with JC
  LDI | 0,
  STA | 15,
  LDI | 1,
  OUT,
  STA | 14,
  ADD | 15,
  JCY | 0,
  STA | 15,
  OUT,
  ADD | 14,
  JCY | 0,
  JMP | 3,
  0, 0,
  0, // Used for storage (addr 14)
  0, // Used for storage (addr 15)
  
  // Program #3 (011): Compute 43 + 6 - 7 = 42
  LDA | 15,
  ADD | 14,
  SUB | 13,
  OUT,
  HLT,
  0, 0, 0, 0, 0, 0, 0, 0,
  7,  // Data at Memory Address 13
  6,  // Data at Memory Address 14
  43, // Data at Memory Address 15

  // Program #4 (100): Add 2 input registers
  STX | 15,
  STY | 14,
  LDA | 15,
  ADD | 14,
  OUT,
  HLT,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  
  // Program #5 (101): Subtract 2 input registers
  STX | 15,
  STY | 14,
  LDA | 15,
  SUB | 14,
  OUT,
  HLT,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

  // Program #6 (110): Multiply: This works, but is not idempotent as it leaves data
  // in address 13. If we can shrink this by one instruction, and then add a
  // "Zero Out Memory @ Address" assembly language instruction that we call first, then
  // this would be idempotent.
  STX | 14,
  STY | 15,
  JMP |  6,
  LDA | 13, // start of loop, add first input to the result
  ADD | 14,
  STA | 13, // Could do an OUT right after this if we had sufficient memory.
  LDA | 15, // subtract one from the second input
  SUI |  1, // Subtract 1
  STA | 15,
  JCY |  3, // jump if second input was greater than 0
  LDA | 13, // load the result
  OUT,
  HLT,
  0,    // 13: result, initialized to 0.
  0,    // 14: 1st input. This is summed repeatedly with result
  0,    // 15: 2nd input. How many times remaining to sum in 1st input.

  // Program #7 (111): Double.
  STX | 15,
  LDA | 15,
  OUT, // Top of loop. A has current value, as does RAM addr 15
  ADD | 15,
  JCY | 0, // Reset if we overflow
  STA | 15,
  JMP | 2,
  0, 0, 0, 0, 0, 0, 0, 0, 0  
};


// Write out the EEPROM that is used as 'cold-storage' for programs/RAM contents.
// This can be loaded by the above microcode into memory right on power up.
void writeStoredProgramEEPROM() {
  // Note addressing here uses non-overlapping addresses as the microcodeEEPROM,
  // so we can load one EEPROM with all the microcode and these stores programs
  // physical wiring will determine with addresses get read (hence which function the EEPROM serves)
  //
  // the addressing for 11 address bits is
  // a10: 1 (meaning we are in the stored program EEPROM, as opposed to microcode EEPROMs).
  // a9-a8-a7: unused, always 0.
  // a6-a5-a4: with program are we loading into RAM (0-7)
  // a3-a2-a1-a0: which assembly language instruction are we at for this program (0-15)

  Serial.println("Programming stored assembly language programs into ROM.");

  int stored_programs_size = sizeof(STORED_PROGRAMS) / sizeof(STORED_PROGRAMS[0]);  
  if (stored_programs_size != (8 * 16)) {
    Serial.println("Must have 8 stored programs of exactly 16 bytes each.!");
    Serial.flush();
    abort();
  }

  uint16_t base_addr = 0x400; // set a10.
  for (uint16_t i = 0; i < stored_programs_size; ++i) {
    writeEEPROM(base_addr + i, STORED_PROGRAMS[i]);
  }
}

typedef enum { EIGHT_BIT_DISPLAY, MICROCODE_AND_STORED_PROGRAMS, FOUR_BIT_DISPLAY, READONLY, } EEPROMTypeT;

/* Arduino runs this function once after loading the Nano, or after pressing the HW reset button.
 * Think of this like main() */
void setup() {
  doCommonInit();

  /*
   * ******************************************************************
   * MODIFY THE BELOW VARIABLE TO DETERMINE WHAT EEPROM YOU ARE WRITING 
   * ****************************************************************** 
   */

  EEPROMTypeT eepromType = READONLY;
  switch (eepromType) {
    case EIGHT_BIT_DISPLAY:
      eraseEEPROM();
      write8BitDisplayEEPROM();
      break;
    case MICROCODE_AND_STORED_PROGRAMS:
      eraseEEPROM();
      writeMicroCodeEEPROM();
      writeStoredProgramEEPROM();
      break;
    case FOUR_BIT_DISPLAY:
      eraseEEPROM();
      write4BitDisplayEEPROM();
      break;
    case READONLY:
      break;
    default:
      Serial.println("FATAL: Not sure what you want me to do!");
      Serial.flush();
    abort();
  }  
  printContents();
  Serial.println("Done.");
}

void loop() {
  // put your main code here, to run repeatedly:
}

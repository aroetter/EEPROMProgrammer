// define which arduino pins are used for what
#define SHIFT_DATA    2
#define SHIFT_CLK     3
#define SHIFT_LATCH   4
#define EEPROM_D0     5
#define EEPROM_D7    12
#define WRITE_ENABLE 13

#define EEPROM_NUM_BYTES 2048

// Given a string in the range [0...15], return a human readable string of that number in binary
// e.g. passing in 10 returns "1010"
void convert4BitIntToBinaryString(char out[5], byte val) {
  for(int i = 0; i < 4; ++i) {
    out[3-i] = (val & 1) ? '1' : '0';
    val = val >> 1;
  }
  out[4] = '\0';
}

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
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; ++pin) {
    pinMode(pin, OUTPUT);
  }
  setAddress(address, /* readMode */ false);
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; ++pin) {
    digitalWrite(pin, data & 1);
    data = data >> 1;
  }
  digitalWrite(WRITE_ENABLE, LOW); // active low
  delayMicroseconds(1);
  digitalWrite(WRITE_ENABLE, HIGH);
  delay(5); // msec // works fine for me, but in video needs to be upped to 10.
}

void printContents() {
  Serial.println("------------------------------------------------------");
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

// Program an EEPROM to be used to drive a 3 digit display given a register's contents.
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
void write7SegmentDecimalDisplayEEPROM() {
  // represent decimal digits 0 ->10 (the 8 bits for a screen display)
  byte digits[] = { 0x7e, 0x30, 0x6d, 0x79, 0x33, 0x5b, 0x5f, 0x70, 0x7f, 0x7b};

  // program the lower half of the chip, unsigned numbers
  Serial.println("Programming unsigned mode...");
  for (int value = 0; value < 256; ++value) {
    uint16_t zeros_addr = value; // zeroes place address
    uint16_t tens_addr = 256 + value;        // make upper byte 00000001
    uint16_t hundreds_addr = 512 + value;  // make upper byte 00000010
    uint16_t sign_addr = 768 + value; // make upper byte 00000011

    writeEEPROM(zeros_addr, digits[value % 10]);
    writeEEPROM(tens_addr, digits[(value / 10) % 10]);
    writeEEPROM(hundreds_addr, digits[(value / 100) % 10]);
    writeEEPROM(sign_addr, 0);    
  }
  
  // program the upper half of the chip, 2s complement
  Serial.println("Programming signed mode...");
  for(int value = -128; value < 128; ++value) {
    uint16_t zeros_addr = 1024 + (byte) value;
    uint16_t tens_addr = 1280 + (byte) value;
    uint16_t hundreds_addr = 1536 + (byte) value;
    uint16_t sign_addr = 1792 + byte (value);

    writeEEPROM(zeros_addr, digits[abs(value) % 10]);
    writeEEPROM(tens_addr,  digits[abs(value / 10) % 10]);
    writeEEPROM(hundreds_addr, digits[abs(value / 100) % 10]);
    writeEEPROM(sign_addr, (value < 0) ? 0x01 : 0x00); // 0x01 == the negative sign
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
// On 1st clock cycle, a fetch sets the CO & MI conrol lines
// On 2nd clock cycle, a fetch sets the RO, II, & CE control lines
static uint32_t FETCH_MICROCODE[] = {CO | MI, RO | II | CE };
static int FETCH_MICROCODE_LEN = sizeof(FETCH_MICROCODE) / sizeof(FETCH_MICROCODE[0]);

// must be == (NUM_MICROCODE_PER_OPCODE - FETCH_MICROCODE_LEN).
// will fail initialization checks if not
#define NUM_CUSTOM_MICROCODE_PER_OPCODE 3

// we guarantee, via checking code below, that the following array is this size
#define NUM_OPCODES 16

// Define the symbolic assembler instruction name to binary opcode mapping here.
// We use these to write programs. These must be consecutive integers starting
// at zero, and in the same order as the OpCodeDefT OPCODE[] array below
// (this is checked at init time).
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
  JCY =  9 << 4, // 1001: Jump Carry. Jump iff ALU carry bit was set on last operation.
  NUL7 = 10 << 4, // 1010: Unused
  NUL8 = 11 << 4, // 1011: Unused

  NUL9 = 12 << 4, // 1100: Unused
  NULA = 13 << 4, // 1101: Unused
  OUT  = 14 << 4, // 1110: Output register A to output register (LCD display)
  HLT  = 15 << 4  // 1111: Halt the computer by stopping the clock
};

typedef struct OpCodeDefT {
  OpCodeName opcode;
  uint32_t microcode[NUM_CUSTOM_MICROCODE_PER_OPCODE];
} OpCodeDefT;



// This defines what microcode runs for each opcode. unused steps are set to 0.
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
  {JCY, {IO|JC, 0, 0}}, // TODO: test this.
  {NUL7, {0, 0, 0}},
  {NUL8, {0, 0, 0}},
  
  {NUL9, {0, 0, 0}},
  {NULA, {0, 0, 0}},
  {OUT,  {AO|OI, 0, 0}},
  {HLT,  {HALT, 0, 0}},
};

/* Code that is needed regardless of what we're programming */
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
  static uint32_t LOAD_PROG_MICROCODE[] = {
    STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, 
    STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, 
    STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, 
    STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, STEP1, STEP2, 
    HALT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 
  };
  
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
  // Program #0 (000): Dummy just to see if memory is loading properly. Increments each bit in order
  0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 

  // Program #1 (001): Count by 3
  LDI | 3, // Read this as assembly "LDI 3"
  STA | 15,
  LDI | 0,
  ADD | 15,
  OUT,
  JMP | 3,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 

  // Program #2 (010): Fibonacci
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
  0, 0, 0, 0, 0, 0,
  
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

  // Program #6 (110): NAME
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  // Program #7 (111): NAME
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};


// Write out the EEPROM that is used as 'cold-storage' for programs/RAM contents.
// This can be loaded by the above microcode into memory right on power up.
void writeStoredProgramEEPROM() {
  // Note addressing here is mutually exclusive with microcodeEEPROM, meaning we can load a single
  // physical chip with both sets of data
  // the addressing for 11 address bits is
  // a10: 1 (meaning we are in the stored program EEPROM, as opposed to microcode EEPROMs).
  // a9-a8-a7: unused, always 0.
  // a6-a5-a4: with program are we loading into RAM (0-7)
  // a3-a2-a1-a0: which assembly language instruction are we at for this program (0-15)

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

/* Arduino runs this function once after loading the Nano, or after pressing the HW reset button.
 * Think of this like main() */
void setup() {
  doCommonInit();

  eraseEEPROM();
  
  Serial.println("Programming EEPROM...");

  // Usage: Do one of these 2 blocks
#if 0
  // Block 1: LCD displays
  write7SegmentDecimalDisplayEEPROM();
#else
  // Block 2: Microcode & stored programs
  writeMicroCodeEEPROM();
  writeStoredProgramEEPROM();
#endif

  printContents();
  Serial.println("Done.");

}

void loop() {
  // put your main code here, to run repeatedly:
}

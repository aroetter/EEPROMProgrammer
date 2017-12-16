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

// Program an EEPROM to be used to drive a 3 digit display given a register's contents
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
// [0768...1023]: signs place digit (always blank for unsigned numbers)
// [1024...1279]: ones place digit for signed decimal representation
// [1280...1535]: tens place digit     ""              ""
// [1536...1791]: hundreds place digit ""              "" 
// [1792...2047]: signs place digit (either a negative sign or blank)
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
  for (uint16_t addr = 0; addr < EEPROM_NUM_BYTES; ++addr) {
    writeEEPROM(addr, 0);
  }
}

// Control Line Bits. There are 16 bits that can be control lines
// Left EEPROM control bits
static uint16_t HALT = 0x8000;
static uint16_t   MI = 0x4000;
static uint16_t   RI = 0x2000;
static uint16_t   RO = 0x1000;
static uint16_t   IO = 0x0800;
static uint16_t   II = 0x0400;
static uint16_t   AI = 0x0200;
static uint16_t   AO = 0x0100;
// Right EEPROM control bits
static uint16_t   SO = 0x0080;
static uint16_t   SU = 0x0040;
static uint16_t   BI = 0x0020;
static uint16_t   OI = 0x0010;

static uint16_t   CE = 0x0008;
static uint16_t   CO = 0x0004;
static uint16_t    J = 0x0002;
// last line is unused, so no 0x0001 value

// this is changable by moving the reset wire on the 3 bit counter on the control logic breadboard.
// This only exists here as a sanity check, not used.
#define NUM_MICROCODE_PER_OPCODE 5

// Every instruction starts with a fetch, so we program that here.
// Way to read this is:
// On 1st clock cycle, a fetch sets the CO & MI conrol lines
// On 2nd clock cycle, a fetch sets the RO, II, & CE control lines
static uint16_t FETCH_MICROCODE[] = {CO | MI, RO | II | CE };

#define NUM_CUSTOM_MICROCODE_PER_OPCODE 3
typedef struct OpCodeDefT {
  const char* name;
  uint16_t microcode[NUM_CUSTOM_MICROCODE_PER_OPCODE];
} OpCodeDefT;

// we guarantee, via checking code below, that the following array is this size
#define NUM_OPCODES 16 

// This defines what control lines are set, in order, for each opcode. unused steps are set to 0.
static OpCodeDefT OPCODE[] = {
  {"HLT", {HALT, 0, 0}},          // opcode binary = 0000
  {"LDA", {IO|MI, RO|AI, 0}},     // opcode binary = 0001
  {"ADD", {IO|MI, RO|BI, SO|AI}}, // opcode binary = 0010
  {"OUT", {AO|OI, 0, 0}},         // opcode binary = 0011

  {"NUL", {0, 0, 0}}, // opcode binary = 0100
  {"NUL", {0, 0, 0}},    // opcode binary = 0101
  {"NUL", {0, 0, 0}},    // opcode binary = 0110
  {"NUL", {0, 0, 0}},    // opcode binary = 0111
  
  {"NUL", {0, 0, 0}},    // opcode binary = 1000
  {"NUL", {0, 0, 0}},    // opcode binary = 1001
  {"NUL", {0, 0, 0}},    // opcode binary = 1010
  {"NUL", {0, 0, 0}},    // opcode binary = 1011
  
  {"NUL", {0, 0, 0}},    // opcode binary = 1100
  {"NUL", {0, 0, 0}},    // opcode binary = 1101
  {"NUL", {0, 0, 0}},    // opcode binary = 1110
  {"NUL", {0, 0, 0}},    // opcode binary = 1111
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
  if (NUM_OPCODES * sizeof(OpCodeDefT) != sizeof(OPCODE)) {
    char buf[100];
    snprintf(buf, 100, "Must have exactly %d microcodes defined. Instead saw %d. Aborting. Terminating...",
      NUM_OPCODES, sizeof(OPCODE) / sizeof(OpCodeDefT));
    Serial.println(buf);
    Serial.flush();
    abort();
  }

  // Does each opcode have the right number of microcode steps?
  int fetch_microcode_len = sizeof(FETCH_MICROCODE) / sizeof(uint16_t);
  int per_opcode_microcode_len = sizeof(OPCODE[0].microcode) / sizeof(uint16_t);
  if ((per_opcode_microcode_len + fetch_microcode_len) != NUM_MICROCODE_PER_OPCODE) {
    char buf[100];
    snprintf(buf, 100, "Each opcode must be = %d microcodes. Fix static data structures. Terminating...",
      NUM_MICROCODE_PER_OPCODE);
    Serial.println(buf);
    Serial.flush();
    abort();
  }
}


// Set up microcode for out EEPROM, which is addressable via 11 address lines [a10...a0]
//
// Addressing for the microcode is done as follows:
// a10...a7: unused, always 0.
// a6....a3: 4 bits to represent the opcode
// a2....a0: 3 bits to represent the microcode step we are on (only 0-4 used though we could extend to 0-7)

// argument is which EEPROM to write, 8 left bits, or 7 right bits.
void writeMicroCodeEEPROM(bool leftEEPROM) {  
  const int fetch_microcode_len = sizeof(FETCH_MICROCODE) / sizeof(uint16_t);
  
  // Iterate over every microcode step w/in that opcode
  // Set the fetch instructions, then the custom microcode for each one
  for(uint8_t i = 0; i < NUM_OPCODES; ++i) {
    char buf[100];
    OpCodeDefT mc = OPCODE[i];
    char binaryStr[5];
    convert4BitIntToBinaryString(binaryStr, i);
    snprintf(buf, 100, "Programming opcode %s (binary opcode = %s)", mc.name, binaryStr);
    Serial.println(buf);
    for(uint8_t step = 0; step < NUM_MICROCODE_PER_OPCODE; ++step) {
      // generate a 16 bit address of form 00000000 0oooosss
      // where oooo is the 4 bit opcode, and sss is the 3 bit step
      // top 5 bits are unused, EEPROM uses 11 lower bits for address only
      uint16_t addr = (i << 3) | step; 
      // write either the fetch steps, or the custom control logic for the opcode
      uint16_t data;
      if (step < fetch_microcode_len) {
        snprintf(buf, 100, "  Step=%d... writing fetch instruction.", step);
        Serial.println(buf);
        data = FETCH_MICROCODE[step];
      } else {
        int custom_step_to_write = step - fetch_microcode_len;
        snprintf(buf, 1000, "  Step=%d... writing custom control logic step %d.", step, custom_step_to_write);
        Serial.println(buf);
        data = mc.microcode[custom_step_to_write];
      }
      // now write the data
      byte byte_to_write = leftEEPROM ? (data >> 8) : data;
      writeEEPROM(addr, byte_to_write);
    }
  }
}

/* Arduino runs this function once after loading the Nano, or after pressing the HW reset button.
 * Think of this like main() */
void setup() {
  doCommonInit();

  Serial.println("Erasing EEPROM...");
  eraseEEPROM();
  
  Serial.println("Programming EEPROM...");
  
  // Usage: uncomment the single one of these functions you want to run.
  // write7SegmentDecimalDisplayEEPROM();
  writeMicroCodeEEPROM(false); // true == left EEPROM (MSBs), false == right EEPROM (LSBs)
  Serial.println("Done.");

  printContents();
}

void loop() {
  // put your main code here, to run repeatedly:
}

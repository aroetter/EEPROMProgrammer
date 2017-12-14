#define SHIFT_DATA    2
#define SHIFT_CLK     3
#define SHIFT_LATCH   4
#define EEPROM_D0     5
#define EEPROM_D7    12
#define WRITE_ENABLE 13

#define EEPROM_NUM_BYTES 2048

void setAddress(int address, bool outputEnable) {
  // sets top bit to 1 iff outputEnable is true. outputEnable puts EEPROM in write mode (when low), or read mode (when high).
  // it reversed b/c that pin happens to be active low
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, (address >> 8) | (outputEnable ? 0x00 : 0x80));
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address); // drops upper top bits, shifts lower byte

  // now toggle clock pulse
  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}

// Return the byte found at a specific address
byte readEEPROM(int address) {
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; ++pin) {
    pinMode(pin, INPUT);
  }
  setAddress(address, /* outputEnable*/ true); // for reading, b/c outputEnable is active low
  byte data = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; --pin) {
    data = (data << 1) + digitalRead(pin);
  }
  return data;
}

// Write the given byte to a specific address
void writeEEPROM(int address, byte data) {
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; ++pin) {
    pinMode(pin, OUTPUT);
  }
  setAddress(address, /* outputEnable*/ false);
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

/* Code that is needed regardless of what we're programming */
void doCommonInit() {
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  digitalWrite(WRITE_ENABLE, HIGH); // active low
  pinMode(WRITE_ENABLE, OUTPUT);
  Serial.begin(57600);
  
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
    int zeros_addr = value; // zeroes place address
    int tens_addr = 256 + value;        // make upper byte 00000001
    int hundreds_addr = 512 + value;  // make upper byte 00000010
    int sign_addr = 768 + value; // make upper byte 00000011

    writeEEPROM(zeros_addr, digits[value % 10]);
    writeEEPROM(tens_addr, digits[(value / 10) % 10]);
    writeEEPROM(hundreds_addr, digits[(value / 100) % 10]);
    writeEEPROM(sign_addr, 0);    
  }
  
  // program the upper half of the chip, 2s complement
  Serial.println("Programming 2s complement mode...");
  for(int value = -128; value < 128; ++value) {
    int zeros_addr = 1024 + (byte) value;
    int tens_addr = 1280 + (byte) value;
    int hundreds_addr = 1536 + (byte) value;
    int sign_addr = 1792 + byte (value);

    writeEEPROM(zeros_addr, digits[abs(value) % 10]);
    writeEEPROM(tens_addr,  digits[abs(value / 10) % 10]);
    writeEEPROM(hundreds_addr, digits[abs(value / 100) % 10]);
    writeEEPROM(sign_addr, (value < 0) ? 0x01 : 0x00); // 0x01 == the negative sign
  }
}


// Control Line Bits. There are 16 bits that can be control lines

// Clears an EEPROM, setting all data to zero.
void writeBlankEEPROM() {
  for (int addr = 0; addr < EEPROM_NUM_BYTES; ++addr) {
    writeEEPROM(addr, 0);
  }
}


// Must be exactly 16 b/c we use 4 bits for opcodes
enum OPCODES {
  LDA = 0,
  ADD = 1,
  OUT = 2,
  UNUSED3 = 3,
  UNUSED4 = 4,
  UNUSED5 = 5,
  UNUSED6 = 6,
  UNUSED7 = 7,
  UNUSED8 = 8,
  UNUSED9 = 9,
  UNUSED10 = 10,
  UNUSED11 = 11,
  UNUSED12 = 12,
  UNUSED13 = 13,
  UNUSED14 = 14,
  HLT = 15
};

// 


// Set up microcode for out EEPROM, which is addressable via 11 address lines [a10...a0]
//
// Addressing for the microcode is done as follows:
// a10...a7: unused, always 0.
// a6....a4: 3 bits to represent the microcode step we are on (only 0-4 used though we could extend to 0-7)
// a3....a0: 4 bits to represent the opcode

// TODO: for sanity: set all microcode steps 5-7 inclusive to all zeros?
// TODO: for sanity: set all a10-a7 rows with any 1s anywhere in them to all zeros?
// TOOD: for sanity: set all unused opcode rows to all zeros? 
void writeMSBMicroCodeControlLogic() {
  
  Serial.print("Size of int is ");
  Serial.println(sizeof(int), DEC);
}

/* Arduino runs this function once after loading the Nano, or after pressing the HW reset button.
 * Think of this like main() */
void setup() {
  doCommonInit();
  assert(sizeof(int) == 2); // We rely on this elsewhere

  Serial.println("Programming EEPROM...");
  // Usage: uncomment the single one of these functions you want to run.
  
  // write7SegmentDecimalDisplayEEPROM();
  // writeBlankEEPROM();
  writeMSBMicroCodeControlLogic();
  // TODO: writeLSBMicroCodeControlLogic();
  Serial.println("Done.");

  printContents();
}

void loop() {
  // put your main code here, to run repeatedly:
}

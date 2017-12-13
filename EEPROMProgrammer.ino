#define SHIFT_DATA    2
#define SHIFT_CLK     3
#define SHIFT_LATCH   4
#define EEPROM_D0     5
#define EEPROM_D7    12
#define WRITE_ENABLE 13

void setAddress(int address, bool outputEnable) {
  // sets top bit to 1 iff outputEnable is true
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, (address >> 8) | (outputEnable ? 0x00 : 0x80));
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address); // drops upper top bits, shifts lower byte

  // now toggle clock pulse
  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}

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

// only prints lower 256 bytes
void printContents() {
  Serial.println("------------------------------------------------------");
  // TODO: make < 256
  for (int base = 0; base <= 255; base += 16) {
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

void setup() {
  // put your setup code here, to run once:
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  digitalWrite(WRITE_ENABLE, HIGH); // active low
  pinMode(WRITE_ENABLE, OUTPUT);
  Serial.begin(57600);

  Serial.println("Programming EEPROM...");
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
  Serial.println("Done.");
  
  printContents();
}

void loop() {
  // put your main code here, to run repeatedly:
}

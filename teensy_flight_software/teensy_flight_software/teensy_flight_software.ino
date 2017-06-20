/*

PINS
MPU 6050      ARDUINO
GND           GND
VDD           3.3V in
VIO           VDD
SCL           SCL (21)
SDA           SDA (20)


*/

// I2Cdev and MPU6050 must be installed as libraries, or else the .cpp/.h files
// for both classes must be in the include path of your project
#include "I2Cdev.h"
#include "MPU6050.h"
#include <EEPROM.h>
#include <SdFat.h>
#include <string>
#include <cstdio> //for sprintf

#define SERIAL_BAUD_RATE 19200
#define RS232_BAUD_RATE 19200
#define MAX_WRITE_SIZE 245  // Length of a single packet in bytes
#define MAIN_LOOP_PERIOD_MS 250
#define PACKETS_PER_SD_FILE  20 // The number of packets stored in a single SD card file
#define BOOT_COUNTER_EEPROM_ADDRESS 0 // The value at this address in EEPROM is incremented each time the teensy boots
#define TMP102_I2C_ADDRESS 72 /* This is the I2C address for our chip.
This value is correct if you tie the ADD0 pin to ground. See the datasheet for some other values. */

// Used for main loop timing
uint32_t currentTime;
uint32_t lastLoopTime;

String outString; // String for printing output
uint32_t packetID; // Counts packets starting from 0
uint32_t firstPacketIDInCurrentFile;
uint32_t packetsInCurrentFile;

// class default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for InvenSense evaluation board)
// AD0 high = 0x69
MPU6050 accelgyro;

int rawValueADXL;
double convertedValueADXL;
int16_t ax, ay, az;
int16_t gx, gy, gz;

#define LED_PIN 13
bool blinkState = false;

// SD card file and SD object
File SDFile;
char filename[17]; // The filename for SD card files "<timestamp>ms.bin"
char folderName[17]; // The name of the folder that files for a particular boot get written to "Boot<boot number>"
SdFatSdioEX SDCard;

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  Serial1.begin(RS232_BAUD_RATE);
  SDCard.begin();
  Wire.begin();
  delay(1000); // Pause for I/O configuration

  // set up sd card writes
  uint8_t bootNumberMSB = EEPROM.read(BOOT_COUNTER_EEPROM_ADDRESS);
  uint8_t bootNumberLSB = EEPROM.read(BOOT_COUNTER_EEPROM_ADDRESS + 1);
  uint16_t bootNumber = (bootNumberMSB << 8) | bootNumberLSB;
  bootNumber++; // Increment boot counter at each power up
  bootNumberLSB = bootNumber & 0xFF;
  bootNumberMSB = bootNumber >> 8;
  EEPROM.write(BOOT_COUNTER_EEPROM_ADDRESS, bootNumberMSB);   // write the boot number back to EEPROM
  EEPROM.write(BOOT_COUNTER_EEPROM_ADDRESS + 1, bootNumberLSB);
  sprintf(folderName, "Boot%05u", bootNumber);
  SDCard.mkdir(folderName);
  newFile();
  packetID = 0;
  
  // set up ADXL193
  pinMode(A14, INPUT);
  // set up MPU6050
  accelgyro.initialize();
  // configure Teensy LED output
  pinMode(LED_PIN, OUTPUT);
  // set up MPL3115A2
  setModeBarometer(); // Measure pressure in Pascals from 20 to 110 kPa
  setOversampleRate(7); // Set Oversample to the recommended 128
  enableEventFlags(); // Enable all three pressure and temp event flags   <- Necessary?

  // Initialize global variables
  packetID = 0;
  lastLoopTime = 0;
}

void loop() {
  currentTime = millis();
  if(currentTime - lastLoopTime >= MAIN_LOOP_PERIOD_MS){
    lastLoopTime = currentTime;
    // Output housekeeping data over USB serial
    // Format: $,packetID,teensyTime,courseAccel,accelX,accelY,accelZ,gyroX,gyroY,gyroZ,temperature(mpl3115),temperature(tmp102),pressure,<0 Fill><newline>
    outString = "$,"; outString += packetID; outString += ','; outString += millis(); outString += ',';
    packetID++;
    readADXL193();
    readMPU6050();
    readMPL3115Temperature();
    readTemp102();
    readMPL3115Pressure();
    while (outString.length() < MAX_WRITE_SIZE) {
      outString += "0";
    }

    if(packetID >= firstPacketIDInCurrentFile + PACKETS_PER_SD_FILE){
      SDFile.close();
      newFile();
    }

    SDFile.println(outString);
    Serial.println(outString);
    Serial1.println(outString);

    //blink LED to show activity
    digitalWrite(LED_PIN, blinkState);
    blinkState = !blinkState;
  }
}

/*
 * Helper functions for various sensors
 */

/* ----------- ADXL Helpers ----------------------*/
void readADXL193() {
  rawValueADXL = analogRead(A14);
  
  // Convert values if necessary
  convertedValueADXL = (rawValueADXL / 1023.0) * 3.3 / 5.0 * 250 - 125; // 3.3/5 conversion factor because of teensy/sensor voltage mismatch

  // Write to string
  outString += convertedValueADXL; outString += ",";
}

/* ----------- MPU Helpers -----------------------*/
void readMPU6050() {
  accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  outString += ax; outString += ",";
  outString += ay; outString += ",";
  outString += ax; outString += ",";
  outString += gx; outString += ",";
  outString += gy; outString += ",";
  outString += gz; outString += ",";
}

/* ----------- TEMP102 Helpers -------------------*/
void readTemp102(){
  byte firstbyte, secondbyte; //these are the bytes we read from the TMP102 temperature registers
  int val; /* an int is capable of storing two bytes, this is where we "chuck" the two bytes together. */ 
  float convertedtemp; /* We then need to multiply our two bytes by a scaling factor, mentioned in the datasheet. */ 
  float correctedtemp; 
  // The sensor overreads (?) 


  /* Reset the register pointer (by default it is ready to read temperatures)
You can alter it to a writeable register and alter some of the configuration - 
the sensor is capable of alerting you if the temperature is above or below a specified threshold. */

  Wire.beginTransmission(TMP102_I2C_ADDRESS); //Say hi to the sensor. 
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(TMP102_I2C_ADDRESS, 2);
  Wire.endTransmission();


  firstbyte      = (Wire.read()); 
/*read the TMP102 datasheet - here we read one byte from
 each of the temperature registers on the TMP102*/
  secondbyte     = (Wire.read()); 
/*The first byte contains the most significant bits, and 
 the second the less significant */
  val = ((firstbyte) << 4);  
/* MSB */
  val |= (secondbyte >> 4);    
/* LSB is ORed into the second 4 bits of our byte.
   Bitwise maths is a bit funky, but there's a good tutorial on the playground */
  convertedtemp = val*0.0625;
  correctedtemp = convertedtemp - 5; 
  /* See the above note on overreading */

  outString += correctedtemp; outString += ",";
}

/*--------- MPL 3115 Helpers --------------------*/
void readMPL3115Pressure()
{
  float pressure = readPressure();
  outString += pressure; outString += ",";
}

void readMPL3115Temperature()
{
  float temperature = readTemp();
  outString += temperature; outString += ",";
}

void newFile() {
  firstPacketIDInCurrentFile = packetID;
  packetsInCurrentFile = 0;
  unsigned int openTime = millis();
  sprintf(filename, "%s/%010ums.txt", folderName, openTime);
  SDFile = SDCard.open(filename, FILE_WRITE);
}


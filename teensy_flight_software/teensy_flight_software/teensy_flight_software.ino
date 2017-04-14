/*

PINS
MPU 6050      ARDUINO
GND           GND
VDD           3.3V in
VIO           VDD
SCL           SCL (21)
SDA           SDA (20)


*/

#include <stdlib.h>
#include <stdio.h>

#define TMP102_I2C_ADDRESS 72 /* This is the I2C address for our chip.
This value is correct if you tie the ADD0 pin to ground. See the datasheet for some other values. */

// I2Cdev and MPU6050 must be installed as libraries, or else the .cpp/.h files
// for both classes must be in the include path of your project
#include "I2Cdev.h"
#include "MPU6050.h"

// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
//#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
//    #include "Wire.h"
//#endif

// class default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for InvenSense evaluation board)
// AD0 high = 0x69
MPU6050 accelgyro;
//MPU6050 accelgyro(0x69); // <-- use for AD0 high

int rawValueADXL;
double convertedValueADXL;
int16_t ax, ay, az;
int16_t gx, gy, gz;

// uncomment "OUTPUT_READABLE_ACCELGYRO" if you want to see a tab-separated
// list of the accel X/Y/Z and then gyro X/Y/Z values in decimal. Easy to read,
// not so easy to parse, and slow(er) over UART.
#define OUTPUT_READABLE_ACCELGYRO

// uncomment "OUTPUT_BINARY_ACCELGYRO" to send all 6 axes of data as 16-bit
// binary, one right after the other. This is very fast (as fast as possible
// without compression or data loss), and easy to parse, but impossible to read
// for a human.
//#define OUTPUT_BINARY_ACCELGYRO

#define LED_PIN 13
bool blinkState = false;

// Timer global var
unsigned long period;
unsigned long counter;

void setup() {
  delay(2000);
  Serial.begin(9600);
  delay(1000);
  Serial.print("Nothing broken yet");
  // part of set up for MPU6050
  // join I2C bus (I2Cdev library doesn't do this automatically)
//  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
      Wire.begin();
//  #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
//     Fastwire::setup(400, true);
//  #endif  
  
  // set up ADXL193
  pinMode(A14, INPUT);
  
  // set up MPU6050
  accelgyro.initialize();
  // configure Arduino LED for
  pinMode(LED_PIN, OUTPUT);

  // set up MPL3115A2
  setModeBarometer(); // Measure pressure in Pascals from 20 to 110 kPa
  setOversampleRate(7); // Set Oversample to the recommended 128
  enableEventFlags(); // Enable all three pressure and temp event flags   <- Necessary?

  // print initial table format
  Serial.print("CoarseAccel\t\tax\t\tay\t\taz\t\tgx\t\tgy\t\tgz\t\ttemp\tpress\tMPLtemp");

  // Start the timer
  period = 200;
  counter = 0;
}

void loop() {
  if ((millis() - counter) > period)
  {
    // put your main code here, to run repeatedly:
    counter = millis();
    getADXL193();
    getMPU6050();
    getTemp102(); 
    getMPL3115(); 
    Serial.print('\n');
  }
}

/*
 * Helper functions for various sensors
 */

/* ----------- ADXL Helpers ----------------------*/
void getADXL193() {
  rawValueADXL = analogRead(A14);
  
  // Convert values if necessary
  convertedValueADXL = (rawValueADXL / 1023.0) * 250 - 125;
  
  // Output in Gs
  Serial.print(convertedValueADXL); Serial.print("\t\t");
}

/* ----------- MPU Helpers -----------------------*/
void getMPU6050() {
  accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  // these methods (and a few others) are also available
  //accelgyro.getAcceleration(&ax, &ay, &az);
  //accelgyro.getRotation(&gx, &gy, &gz);
  #ifdef OUTPUT_READABLE_ACCELGYRO
    // display tab-separated accel/gyro x/y/z values
    //Serial.print("a/g:\t");
    Serial.print(ax); Serial.print("\t\t");
    Serial.print(ay); Serial.print("\t\t");
    Serial.print(az); Serial.print("\t\t");
    Serial.print(gx); Serial.print("\t\t");
    Serial.print(gy); Serial.print("\t\t");
    Serial.print(gz); Serial.print("\t\t");
  #endif
  #ifdef OUTPUT_BINARY_ACCELGYRO
    Serial.write((uint8_t)(ax >> 8)); Serial.write((uint8_t)(ax & 0xFF));
    Serial.write((uint8_t)(ay >> 8)); Serial.write((uint8_t)(ay & 0xFF));
    Serial.write((uint8_t)(az >> 8)); Serial.write((uint8_t)(az & 0xFF));
    Serial.write((uint8_t)(gx >> 8)); Serial.write((uint8_t)(gx & 0xFF));
    Serial.write((uint8_t)(gy >> 8)); Serial.write((uint8_t)(gy & 0xFF));
    Serial.write((uint8_t)(gz >> 8)); Serial.write((uint8_t)(gz & 0xFF));
  #endif
  // blink LED to indicate activity
  blinkState = !blinkState;
  digitalWrite(LED_PIN, blinkState);
}

/* ----------- TEMP102 Helpers -------------------*/
void getTemp102(){
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

  Serial.print(correctedtemp); Serial.print("\t\t");
}

/*--------- MPL 3115 Helpers --------------------*/
void getMPL3115()
{
  float pressure = readPressure();
  float temperature = readTemp();
  Serial.print(pressure); Serial.print("\t\t");
  Serial.print(temperature); Serial.print("\t\t");
}


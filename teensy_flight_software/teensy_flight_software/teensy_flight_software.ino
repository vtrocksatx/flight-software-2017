/*

PINS
MPU 6050      ARDUINO
GND           GND
VDD           3.3V in
VIO           VDD
SCL           SCL (21)
SDA           SDA (20)


*/

#include <SdFat.h>

#define TMP102_I2C_ADDRESS 72 /* This is the I2C address for our chip.
This value is correct if you tie the ADD0 pin to ground. See the datasheet for some other values. */

// I2Cdev and MPU6050 must be installed as libraries, or else the .cpp/.h files
// for both classes must be in the include path of your project
#include "I2Cdev.h"
#include "MPU6050.h"

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

// Timer global var
unsigned long period;
unsigned long counter;

// SD card file and SD object
File sd_file;
SdFatSdioEX SD;

void setup() {
  delay(2000);
  Serial.begin(9600); // Initialize USB serial (Teensy ignores the specified baud rate and sets it to the USB data rate)
  delay(1000);
  Serial1.begin(9600);  // Initialize RS232 output
  Wire.begin();
  SD.begin(); 

  // set up sd card writes
  sd_file = SD.open("sensorTelemetry.txt", FILE_WRITE);
  
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

  // Start the timer
  period = 200;
  counter = 0;
}

void loop() {
  if ((millis() - counter) > period)
  {
    // put your main code here, to run repeatedly:
    counter = millis();
    sd_file.print("$");
    Serial.print("$");
    getADXL193();
    getMPU6050();
    getTemp102();
    getMPL3115();
    Serial.print("\n");
    sd_file.print("\n");

    Serial1.println("It works!");

    // TODO: REMOVE BEFORE FLIGHT!!!!!!!!!!
    if(millis() > 20000)
    {
      sd_file.close();
      while(1);
    }
  }
}

/*
 * Helper functions for various sensors
 */
/*
void print(//what goes here?) {
  sd_file.print();
  Serial.print();
  //somehow send to usb
}*/

/* ----------- ADXL Helpers ----------------------*/
void getADXL193() {
  rawValueADXL = analogRead(A14);
  
  // Convert values if necessary
  convertedValueADXL = (rawValueADXL / 1023.0) * 250 - 125;

  // Convert to a string
  Serial.print(convertedValueADXL); Serial.print(",");
  sd_file.print(convertedValueADXL); sd_file.print(",");
}

/* ----------- MPU Helpers -----------------------*/
void getMPU6050() {
  accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    Serial.print(ax); Serial.print(",");
    Serial.print(ay); Serial.print(",");
    Serial.print(az); Serial.print(",");
    Serial.print(gx); Serial.print(",");
    Serial.print(gy); Serial.print(",");
    Serial.print(gz); Serial.print(",");
    sd_file.print(ax); sd_file.print(",");
    sd_file.print(ay); sd_file.print(",");
    sd_file.print(az); sd_file.print(",");
    sd_file.print(gx); sd_file.print(",");
    sd_file.print(gy); sd_file.print(",");
    sd_file.print(gz); sd_file.print(",");
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

  Serial.print(correctedtemp); Serial.print(",");
  sd_file.print(correctedtemp); sd_file.print(",");
}

/*--------- MPL 3115 Helpers --------------------*/
void getMPL3115()
{
  float pressure = readPressure();
  float temperature = readTemp();
  Serial.print(pressure); Serial.print(",");
  Serial.print(temperature); Serial.print(",");
  sd_file.print(pressure); sd_file.print(",");
  sd_file.print(temperature); sd_file.print(",");
}


/* Name:            Temperature Logger
 * Developer:       Jeremy Maxey-Vesperman
 * Last Modified:   10/13/2019
 * Description:     Temperature logger with SD shield, RTC, and 7-seg display
 */

/*  Hardware:
 *  - Arduino Uno x1
 *  - DS3231 RTCC module x1
 *  - 4-Digit 7-Seg Display w/ TM1637 driver module x1
 *  - Seeeduino SD Card Logger Rev 3.0 x1
 *  - 192-103LET-A01 Thermistor x1
 *  - 10kOhm Resistor x1
 *  - 1uF Electrolytic Capacitor x1
 *  - 9V battery x1
 *  - 9V to barrel jack connector x1
 *  - Assorted jumpers
 *  - FAT32-formatted SD Card x1
 */

/* Library imports */
#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include <TM1637Display.h>
#include "RTClib.h"
#include "SdFat.h"

/* Compiler definitions */
// Error messages stored in flash.
#define error(msg) sd.errorHalt(F(msg))

#define BAUD_RATE 9600

#define INIT_DELAY_MS 3000

// Pin definitions
#define THERMISTOR_PIN A0

#define SD_CS SS // D10
#define SD_MOSI D11
#define SD_MISO D12
#define SD_SCK D13

#define DISPLAY_CLK 9
#define DISPLAY_DIO 8

// Logger reference constants
#define V_SUPPLY 5.0
#define V_REF 5.0
#define ADC_RES 1024.0

// Temperature sensor circuit constants
#define THERMISTOR_B 3974
#define THERMISTOR_R0 10000
#define THERMISTOR_T0 298.15
#define THERMISTOR_R2 10000

// Temperature conversion constants
#define K_TO_F_MULT 1.8
#define K_TO_F_DIFF 459.67

// File name base for SD logging
#define FILE_BASE_NAME "Temp"

// Logging update configuration
#define UPDATE_FREQ_MS 5000
#define ROLLING_AVG_POINT_SIZE 100

/* Function prototypes */
void setupRTC();
void setupSD();
void setupDisplay();
void setupRollingAvg();
float getThermistorVoltage();
float calcThermR1(float);
float calcTempK(float);
float convertFToK(float);
float getTempF();
void updateDisplay(float);
bool updateRollingAvg(float);
void writeLogHeader();
void writeLogData(DateTime, float);
void logDate(DateTime);
void logTime(DateTime);

/* Global variables */
RTC_DS3231 rtc;
SdFat sd;
SdFile file;
TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);

// Rolling average "FIFO"
float tempFIFO[ROLLING_AVG_POINT_SIZE];
int idxFIFO = 0;
float tempRunningSum = 0;
float tempAvg = 0;

// Characters for 7-seg display
uint8_t displaySegs[4];

/* Default Functions */
void setup() {
  // Open serial port for debugging
  Serial.begin(BAUD_RATE);

  // Delay for opening console
  delay(INIT_DELAY_MS);

  // Setup all components of system
  setupRTC();
  setupSD();
  setupDisplay();
  setupRollingAvg();
}

void loop() {
  // Read temperature from thermistor
  float tempF = getTempF();

  // Update the rolling average with new data point
  bool isAvgReady = updateRollingAvg(tempF);

  // If rolling average is ready, update the display and log the average
  if (isAvgReady) {
    // Get the current time
    DateTime currDateTime = rtc.now();
    
    // Update the user display
    updateDisplay(tempAvg);
    
    // Log the temperature and current time to the SD card
    writeLogData(currDateTime, tempAvg);
    
    // Output the temperature over serial for debugging
    Serial.print("Average: ");
    Serial.println(tempAvg, 6);
  }

  // Loop delay = Desired update rate / number of points for rolling average
  delay(UPDATE_FREQ_MS / ROLLING_AVG_POINT_SIZE);
}

/* Setup Functions */
void setupRTC() {
  // Try to setup the RTC
  if (! rtc.begin()) {
    // Output error if RTC can't be found
    Serial.println("Couldn't find RTC");
    // Halt
    while (1);
  }

  // If RTC lost power
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // Set RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}


void setupSD() {
  const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
  char fileName[13] = FILE_BASE_NAME "00.csv";

  // Initialize at the highest speed supported by the board that is
  // not over 50 MHz. Try a lower speed if SPI errors occur.
  if (!sd.begin(SD_CS, SD_SCK_MHZ(50))) {
    sd.initErrorHalt();
  }

  // Check that base file name is under length threshold
  if (BASE_NAME_SIZE > 6) {
    error("FILE_BASE_NAME too long");
  }

  // Find an unused file name.
  while (sd.exists(fileName)) {
    if (fileName[BASE_NAME_SIZE + 1] != '9') {
      fileName[BASE_NAME_SIZE + 1]++;
    } else if (fileName[BASE_NAME_SIZE] != '9') {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
    } else {
      error("Can't create file name");
    }
  }

  // Create and open the file for writing
  if (!file.open(fileName, O_WRONLY | O_CREAT | O_EXCL)) {
    error("file.open");
  }

  Serial.print(F("Logging to: "));
  Serial.println(fileName);

  // Write the header row to the CSV file
  writeLogHeader();
}

void setupDisplay() {
  // Turn on full brightness
  display.setBrightness(0x0f);

  // Initialize segments array
  // -
  displaySegs[0] = SEG_G;
  // -
  displaySegs[1] = SEG_G;
  // Degree Symbol
  displaySegs[2] = SEG_A | SEG_B | SEG_F | SEG_G;
  // F
  displaySegs[3] = SEG_A | SEG_E | SEG_F | SEG_G;

  // Set the segments
  display.setSegments(displaySegs);
}

void setupRollingAvg() {
  // Init the FIFO to 0
  for (int idx = 0; idx < ROLLING_AVG_POINT_SIZE; idx++) {
    tempFIFO[idx] = 0;
  }
}

/* Temperature Functions */
float calcThermR1(float thermV) {
  float thermR1 = ((V_SUPPLY * THERMISTOR_R2) - (thermV * THERMISTOR_R2)) / thermV;
  return thermR1;
}

float calcTempK(float thermR1) {
  float lnR0R1 = log((THERMISTOR_R0 / thermR1));
  float tempK = ( ( THERMISTOR_T0 * THERMISTOR_B ) / lnR0R1 ) / ( ( THERMISTOR_B / lnR0R1 ) - THERMISTOR_T0 );
  return tempK;
}

// Query the thermistor and calculate temperature in Fahrenheit
float getTempF() {
  float thermV = getThermistorVoltage();
  
  float thermR1 = calcThermR1(thermV);

  // Handle situation that would cause divide by zero error
  float tempK = -1.0;
  if (thermR1 == THERMISTOR_R0) {
    tempK = THERMISTOR_T0;
  } else {
    tempK = calcTempK(thermR1);
  }
  
  float tempF = convertFToK(tempK);
  return tempF;
}

// Read the ADC value for the thermistor and convert to voltage
float getThermistorVoltage() {
  // Read input
  int thermADCVal = analogRead(THERMISTOR_PIN);

  // Vmeas formula
  float thermV = (((float)thermADCVal + 1.0) / ADC_RES) * V_REF;
  
  // Return to caller
  return thermV;
}

float convertFToK(float tempK) {
  float tempF = (tempK * K_TO_F_MULT) - K_TO_F_DIFF;
  return tempF;
}

/* Update Functions */
void updateDisplay(float avgTemp) {
  // Convert floating-point temp to int
  int displayTemp = (int)(avgTemp);

  // Calculate the tens and ones place
  int dig0 = displayTemp / 10;
  int dig1 = displayTemp % 10;

  // If temp is over 99, max out at 99
  if (dig0 > 9) {
    dig0 = 9;
    dig1 = 9;
  }

  // Update the seg array with the segment hex that corresponds to the integer value
  displaySegs[0] = display.encodeDigit(dig0);
  displaySegs[1] = display.encodeDigit(dig1);

  // Refresh the segments
  display.setSegments(displaySegs);
}

bool updateRollingAvg(float newReading) {
  // Remove outgoing value from the running sum
  tempRunningSum -= tempFIFO[idxFIFO];
  // Add new reading to the FIFO
  tempFIFO[idxFIFO] = newReading;
  // Add new reading to the running sum
  tempRunningSum += tempFIFO[idxFIFO];
  // Move to next array position
  idxFIFO++;

  // Determine if we've filled the FIFO
  if (idxFIFO >= ROLLING_AVG_POINT_SIZE) {
    // Reset index to beginning
    idxFIFO = 0;

    // Calculate the new average temperature
    tempAvg = tempRunningSum / (float)ROLLING_AVG_POINT_SIZE;

    // Indicate that rolling average is ready to read
    return true;
  }

  // Indicate that rolling average isn't ready to be read
  return false;
}

/* Logging Functions */
void writeLogHeader() {
  // Create the header for the CSV file
  file.println(F("Date,Time,Average Temperature (F),Number of Points Averaged"));
}

void writeLogData(DateTime currDateTime, float tempF) {
  // Create new entry in the log
  logDate(currDateTime);
  
  file.write(',');

  logTime(currDateTime);

  file.write(',');

  // Average Temperature
  file.print(tempF, 6);

  file.write(',');

  // Number of Points Averaged
  file.print(ROLLING_AVG_POINT_SIZE);

  // End of entry
  file.println();

  // Force data to SD and update the directory entry to avoid data loss.
  if (!file.sync() || file.getWriteError()) {
    error("write error");
  }
}

void logDate(DateTime currDateTime) {
  file.print(currDateTime.month());
  file.write('/');
  file.print(currDateTime.day());
  file.write('/');
  file.print(currDateTime.year());
}

void logTime(DateTime currDateTime) {
  file.print(currDateTime.hour());
  file.write(':');
  file.print(currDateTime.minute());
  file.write(':');
  file.print(currDateTime.second());
}

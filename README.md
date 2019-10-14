# Arduino Uno Temperature Logger
This is an Arduino Uno-based temperature logger. The temperature circuit makes use of a 192-103LET-A01 thermistor and 10kOhm resistor to form a voltage divider. Smoothing is provided by a 1 uF electrolytic capacitor. Temperature is displayed to the user on the 7-segment display and is logged to an SD card in CSV format. An RTCC provides accurate time and date information for the log.


## Hardware Requirements
 *  Arduino Uno *x1*
 *  DS3231 RTCC module *x1*
 *  4-Digit 7-Seg Display w/ TM1637 driver module *x1*
 *  [Seeeduino SD Card Shield Rev 3.0](https://seeeddoc.github.io/SD_Card_Shield_V3.0/) *x1*
 *  [192-103LET-A01 Thermistor](https://sensing.honeywell.com/192-103LET-A01-thermistors) *x1*
 *  10kOhm Resistor *x1*
 *  1uF Electrolytic Capacitor *x1*
 *  9V battery *x1*
 *  9V to barrel jack connector *x1*
 *  Assorted jumpers
 *  FAT32-formatted SD Card *x1*
 
## Software Requirements
This logger leverages a number of external libraries for hardware interface. All necessary libraries have been included as submodules. They include:

* [RTClib](https://github.com/adafruit/RTClib)
* [SdFat](https://github.com/greiman/SdFat)
* [TM1637](https://github.com/avishorp/TM1637)

Please install these into your Arduino IDE prior to compiling.

## Schematic
![Alt text](https://github.com/JeremyV2014/Arduino_Temperature_Logger/blob/master/diagrams/Schematic.svg?sanitize=true "Schematic Diagram of Arduino Temperature Logger")

## Implementation
![Alt text](https://github.com/JeremyV2014/Arduino_Temperature_Logger/blob/master/diagrams/Breadboard_Implementation.jpg "Breadboard Implementation of Arduino Temperature Logger")

#include <MultiFuncShield.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <Wire.h>
#include <EEPROM.h>


SCD30         airSensor;                  // Object to interface with sensor
int           co2Value = 0;               // Sensor read-out value CO2
float         tempValue = 0.0;            // Sensor read-out value Temperature (C)
float         humValue = 0.0;             // Sensor read-out value rel. Hum (%)
short         threshold;                  // co2 Threshold value
byte          temperatureOffset;          // temperature offset of SCD30 sensor
byte          displayMode;                // Display mode
bool          beepMode;                   // co2 beep alarm if over threshold
bool          newReading;                 // only do actions if new sensor reading is ready
byte          altMulti = 1;               // altitude multiplikator for scaling
short         altValue;                   // altitude value


// variables and constants needed for menu
const char    * menuOptions[] = {"disp", "beep", "thre", "tc", "alt", "cal"};
const char    * menuDisp[] = {"co2", "c", "hunn", "all"};
const char    * menuBeep[] = {"off", "on"};
const char    * menuThre[] = {"250", "500", "750", "1000", "1250", "1500", "1750", "2000", "2250", "2500"};
const char    * menuTc[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
const int     menuOptionElements = 6;
const int     menuDispElements = 4;
const int     menuBeepElements = 2;
const int     menuThreElements = 10;
const int     menuTcElements = 11;
bool          menuMode = false;
bool          menuNeedsPrint = false;
byte          menuPage = 0;
char          optionSelected = -1;
byte          cycle = 0;


void setup() {
    // Setup serial port
    int baudrate = 9600;
    Serial.begin(baudrate);

    // Initialize Wire and MFS
    Wire.begin();    
    MFS.initialize();

    // Write TOPTICA to MFS
    MFS.beep();
    MFS.write("top");
    delay(1500);
    MFS.write("tica");
    delay(1500);
    MFS.write("C02");
    delay(2000);
    MFS.beep();

    // Check for errors in the initialisation
    if (airSensor.begin() == false) {
        Serial.println("Error. SCD30 not detected. Please check the Connection."
            "Press RESET to proceed.");
    }
}


void loop() {
    if (airSensor.dataAvailable()) {
        // Get the data from the sensor
        co2Value = airSensor.getCO2();

        // Write it to the MFS and on serial port
        MFS.write(co2Value);
        Serial.println(co2Value);
    }

    if(co2Value >= threshold){
        MFS.beep();
    }

    delay(500);
}

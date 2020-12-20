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

    // show TOPTICA boot animation on display and send to serial
    topticaSplash();
    
    // Check for errors in the initialisation
    if (airSensor.begin() == false) {
        Serial.println("Error. SCD30 not detected. Please check the Connection."
            "Press RESET to proceed.");
        MFS.writeLeds(LED_ALL, ON);
        MFS.blinkLeds(LED_ALL, ON);
        MFS.write("ERR");
        delay(5000);
    }else{
        // SCD30 set measurement interval in s
        airSensor.setMeasurementInterval(2);  
    
        // Check EEPROM against stored crc value, load default if needed and set variables
        eeprom_check();    
        eeprom_load();
    }   
}


void loop() {
    // Initialize MFS buttons
    byte btn = MFS.getButton();

    // Menu Button    
    if (btn == BUTTON_1_PRESSED && !menuMode){
        menuMode = true;
        //Serial.println("Menu activated");
        menuNeedsPrint = true;
    }

    if (btn == BUTTON_1_PRESSED && menuMode){
        switch(menuPage){
            case 0:
                if (optionSelected < menuOptionElements-1){
                    // select next menu item
                    optionSelected++;
                }else{
                    // end of menu array reached. go back to start
                    optionSelected = 0;
                }
                break;
            case 1:
                if (optionSelected < menuDispElements-1){
                    // select next menu item
                    optionSelected++;
                }else{
                    // end of menu array reached. go back to start
                    optionSelected = 0;
                }
                break;
            case 2:
                if (optionSelected < menuBeepElements-1){
                    // select next menu item
                    optionSelected++;
                }else{
                    // end of menu array reached. go back to start
                    optionSelected = 0;
                }
                break;
            case 3:
                if (optionSelected < menuThreElements-1){
                    // select next menu item
                    optionSelected++;
                }else{
                    // end of menu array reached. go back to start
                    optionSelected = 0;
                }
                break;
            case 4:
                if (optionSelected < menuTcElements-1){
                    // select next menu item
                    optionSelected++;
                }else{
                    // end of menu array reached. go back to start
                    optionSelected = 0;
                }
                break;
            case 5:
                if (altMulti <= 10){
                    altMulti++;
                }else{
                    altMulti = 1;
                }
                break;
            default:
                break;
        }
        menuNeedsPrint = true;
    }

    // Select and Apply Action
    if (btn == BUTTON_3_PRESSED){
        if (menuMode && menuPage == 0){
            if (menuOptions[optionSelected] == "disp"){
                // select menuPage 1 and show current value
                menuPage = 1;
                optionSelected = displayMode;
            }else if (menuOptions[optionSelected] == "beep"){
                // select menuPage 1 and show current value
                menuPage = 2;
                optionSelected = beepMode;
            }else if (menuOptions[optionSelected] == "thre"){
                // select menuPage 1 and show current value
                menuPage = 3;
                optionSelected = threshold/250-1;
            }else if (menuOptions[optionSelected] == "tc"){
                // select menuPage 1 and show current value
                menuPage = 4;
                optionSelected = temperatureOffset;
            }else if (menuOptions[optionSelected] == "alt"){
                menuPage = 5;
            }else if (menuOptions[optionSelected] =="cal"){
                MFS.write("set");
                delay(500);
                MFS.write("co2");
                delay(500);
                MFS.write("cal");
                delay(500);
                menuPage = 6;
            }
        menuNeedsPrint = true;
        }else if (menuMode && menuPage != 0){
            if (menuPage == 1){
                displayMode = optionSelected;
            }else if (menuPage == 2){
                beepMode = optionSelected;
            }else if (menuPage == 3){
                threshold = (optionSelected + 1) * 250;
            }else if (menuPage == 4){
                temperatureOffset = optionSelected;
                airSensor.setTemperatureOffset(temperatureOffset);        // Set temperature offset to compensate for self heating
            }else if (menuPage == 5){
                altValue = analogRead(POT_PIN)*altMulti;
                airSensor.setAltitudeCompensation(analogRead(POT_PIN)*altMulti);
            }else if (menuPage == 6){
                // Start forced calibration routine
                forcedCalibration(analogRead(POT_PIN), btn);
            }            
            resetMenu(false, -1);
            loadScreen();
        }
    }
    
    // Save Button
    // use long release here because of non-perfect debouncing in LONG_PRESSED
    if (btn == BUTTON_3_LONG_RELEASE){
            eeprom_update();
            loadScreen();
            saveScreen();
            resetMenu(false, -1);
    }

    // Reset to defaults Button
    // use long release here because of non-perfect debouncing in LONG_PRESSED
    if (btn == BUTTON_2_LONG_RELEASE){
        eeprom_reset();
        loadScreen();
        saveScreen();
    }

    delay(500);
}

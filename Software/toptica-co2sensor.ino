#include <MultiFuncShield.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <Wire.h>
#include <EEPROM.h>


SCD30         airSensor;                  // Object to interface with sensor
short         co2Value = 0;               // Sensor read-out value CO2
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
    short baudrate = 9600;
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
        MFS.blinkDisplay(15, 1);
        MFS.write("ERR");
        delay(5000);
        MFS.blinkDisplay(15, 0);
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
        eeprom_load();
        loadScreen();
        saveScreen();
    }

    // Back Button and exit from menu without saving
    if (btn == BUTTON_2_PRESSED && menuMode && menuPage!=0){
        // reset optionSelected and menuPage
        resetMenu(true, 0);
    }else if (btn == BUTTON_2_PRESSED && menuMode && menuPage==0){
        resetMenu(false, -1);
    }
    
    // Print menuPage element to display
    static uint32_t timer;
    if (menuMode && menuNeedsPrint){
        timer = millis();
        if (menuPage == 0){
            MFS.write(menuOptions[optionSelected]);
        }else if (menuPage == 1){
            MFS.write(menuDisp[optionSelected]);
        }else if (menuPage == 2){
            MFS.write(menuBeep[optionSelected]);
        }else if (menuPage == 3){
            MFS.write(menuThre[optionSelected]);
        }else if (menuPage == 4){
            MFS.write(menuTc[optionSelected]);
        }else if (menuPage == 5){
            MFS.write(analogRead(POT_PIN)*altMulti);
        }else if (menuPage == 6){
            MFS.write(analogRead(POT_PIN));
        }
        menuNeedsPrint = false;
        if (menuPage == 5){
            menuNeedsPrint = true;
            delay(250);
        }else if (menuPage == 6){
            menuNeedsPrint = true;
            delay(250);
        }
    }else if (menuMode && !menuNeedsPrint){
        if ((millis() - timer) > 5000){
            resetMenu(false, -1);  
        }
    }



    // Display Modes
    if (airSensor.dataAvailable() && !menuMode) {
            newReading = true;
            switch(displayMode){
                case 0:
                    co2Value = airSensor.getCO2();
                    MFS.write(co2Value);
                    Serial.println((String)"CO2(ppm): " + co2Value);
                    break;
                case 1:
                    tempValue = airSensor.getTemperature();
                    MFS.write(tempValue);
                    Serial.println((String)"Temperature (C): " + tempValue);
                    break;
                case 2:
                    humValue = airSensor.getHumidity();
                    MFS.write(humValue);
                    Serial.println((String)"rel. Humidity (%): " + humValue);
                    break;
                case 3:
                    // cycle time is given by measurement time
                    co2Value = airSensor.getCO2();
                    tempValue = airSensor.getTemperature();
                    humValue = airSensor.getHumidity();
                    Serial.println((String)"CO2 (ppm): " + co2Value);
                    Serial.println((String)"Temperature (C): " + tempValue);
                    Serial.println((String)"rel. Humidity (%): " + humValue);
                    if ((cycle % 3) == 0){
                        MFS.write(co2Value); 
                    }else if ((cycle % 3) == 1){
                        MFS.write(tempValue);
                    }else if ((cycle % 3) == 2){
                        MFS.write(humValue);
                    }
                    cycle++;
                    break;
                default:
                    break;        
              }
        }else{
            delay(25);
        }
    
    // co2 alarm section
    if(co2Value >= threshold && !menuMode && newReading){
        newReading = false;
        MFS.writeLeds(LED_ALL, ON);
        MFS.blinkLeds(LED_ALL, ON);
        Serial.println((String)"Alarm activated: co2 value above threshold: " + co2Value + " > " + threshold);
        if (beepMode){
            MFS.beep();
        }      
    }else if (co2Value <= threshold && !menuMode && newReading){
        newReading = false;
        MFS.writeLeds(LED_ALL, OFF);
    }
}


unsigned long eeprom_crc(void){
    const unsigned long crc_table[16] = {  
      0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
      0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
      0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
      0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
    };
    unsigned long crc = ~0L;
    
    // Calculate crc only from first X Bytes of EEPROM
    // change the index value if more or less parameters shoudl be saved
    for (int index = 0 ; index < 6  ; ++index) {
      crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
      crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
      crc = ~crc;
    }
    return crc;
}


void resetMenu(bool mMode, byte optSel){
    // reset optionSelected, menuPage and deactivate menuMode
    optionSelected = optSel;
    menuNeedsPrint = true;
    menuMode = mMode;
    menuPage = 0;
    MFS.write("----");
    delay(250);
}


void eeprom_load(){
    // load stored EEPROM values
    displayMode = EEPROM.read(0);
    beepMode = EEPROM.read(1);
    threshold = (EEPROM.read(2) + 1) * 250;
    temperatureOffset = EEPROM.read(3);
    // Set sensor temperature offset to compensate for self heating
    airSensor.setTemperatureOffset(temperatureOffset);
    altMulti = EEPROM.read(4);
    altValue = EEPROM.get(5, altValue);
    // Set sensor altitude compensation
    airSensor.setAltitudeCompensation(altValue);
}


void eeprom_check(){
    // Check for valid crc
    // Reset EEPROM to default configuration values + crc if crc mismatches 
    unsigned long crcTemp; 
    if (EEPROM.get(EEPROM.length()-4, crcTemp) != eeprom_crc()){
        Serial.println("EEPROM not matching last saved crc value - setting defaults");
        // default settings can be changed as default crc is actively calculated
        eeprom_reset();
    }
}


void eeprom_update(void){
    // update all configuration values + crc
    EEPROM.update(0, displayMode);
    EEPROM.update(1, beepMode);
    EEPROM.update(2, threshold/250-1);
    EEPROM.update(3, temperatureOffset);
    EEPROM.update(4, altMulti);
    EEPROM.put(5, altValue);
    EEPROM.put(EEPROM.length()-4, eeprom_crc());
}

void eeprom_reset(void){
    // resetting EEPROM to default configuration values + crc
    Serial.println("Resetting EEPROM to defaults");
    MFS.blinkDisplay(15, 1);
    MFS.write("RST");
    delay(2000);
    MFS.blinkDisplay(15, 0);
    EEPROM.update(0, 0);                                      // set displayMode to co2
    EEPROM.update(1, 1);                                      // set beepMode to on
    EEPROM.update(2, 3);                                      // set co2 threshold to 1000 ppm
    EEPROM.update(3, 0);                                      // set temperature offset to 0
    EEPROM.update(4, 1);                                      // set altMulti to 1
    EEPROM.put(5, 500);                                       // set altValue to 500
    EEPROM.put(EEPROM.length()-4, eeprom_crc());              // put default crc to EEPROM
}


void forcedCalibration(short cal, byte btn){
    // Routine for forced sensor calibration
    // uses analog read of MFS potentiometer to set co2 cal value
    // waits 3 minutes (blinks MFS 7 segment) and then forces calibration
   
    Serial.println("Start sensor forced recalibration routine - 3 Minutes settling time");
    int i;
    for (i=0; i<1440; i++){
        // one cylce takes ~0125.s
        MFS.write("CAL");
        MFS.blinkDisplay(15, 1);    
                
        if (!digitalRead(A2) && menuMode){
        resetMenu(false, 0);
        Serial.println("Sensor recalibration aborted");
        MFS.blinkDisplay(15, 0);
        return;
        }
    }
    // Force recalibration with chosen co2 cal value
    airSensor.setForcedRecalibrationFactor(cal);
    Serial.println("Sensor forced recalibration done");
    MFS.blinkDisplay(15, 0);
}


void topticaSplash(void){
    // Write TOPTICA to MFS
    Serial.println("TOPTICA CO2 Sensor Shield");
    MFS.beep();
    MFS.write("top");
    delay(1500);
    MFS.write("tica");
    delay(1500);
    MFS.write("C02");
    delay(2000);
    MFS.beep();
}


void saveScreen(void){
    MFS.write("SAUE");
    MFS.blinkDisplay(15, 1);
    delay(2000);
    MFS.blinkDisplay(15, 0);
}

void loadScreen(void){
    MFS.write("o---");
    delay(125);
    MFS.write("-o--");
    delay(125);
    MFS.write("--o-");
    delay(125);
    MFS.write("---o");
    delay(125);  
}

#include <MultiFuncShield.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <Wire.h>
#include <EEPROM.h>


SCD30         airSensor;                  // Object to interface with sensor
short         co2Value = 0;               // Sensor read-out value CO2
float         tempValue = 0.0;            // Sensor read-out value Temperature (C)
float         humValue = 0.0;             // Sensor read-out value rel. Hum (%)

// variables and constants needed for menu
const char    * menuOptions[] = {"disp", "beep", "thre", "tc", "alt", "cal", "ser", "ser2"};
const char    * menuDisp[] = {"co2", "c", "rH", "all"};
const char    * menuBeep[] = {"off", "on"};
const int     menuOptionElements = 8;
const int     menuDispElements = 4;
const int     menuBeepElements = 2;
bool          menuMode = false;
bool          menuNeedsPrint = false;
byte          menuPage = 0;
char          menuSelected = -1;
byte          menuCycle = 0;

struct SettingValues {
   byte     altMulti;
   short    altValue;
   bool     beepMode;
   byte     displayMode;
   short    serMode;
   bool     ser2Mode;
   byte     temperatureOffset;
   short    threshold;
} settings;


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
  
    switch (btn){
        case BUTTON_1_PRESSED:
            // Menu Button  
            if (!menuMode){
                menuMode = true;
                menuNeedsPrint = true;
            }
            if (menuMode){
                switch(menuPage){
                    case 0:
                        menuSelected < menuOptionElements-1 ? menuSelected++ : menuSelected = 0;
                        break;
                    case 1:
                    case 7:
                        menuSelected < menuDispElements-1 ? menuSelected++ : menuSelected = 0;
                        break;
                    case 2:
                    case 8:
                        menuSelected < menuBeepElements-1 ? menuSelected++ : menuSelected = 0;
                        break;
                    case 3:
                        menuSelected <= 11 ? menuSelected++ : menuSelected = 1;
                        break;
                    case 4:
                        menuSelected < 15 ? menuSelected++ : menuSelected = 0;
                        break;
                    case 5:
                        settings.altMulti <= 10 ? settings.altMulti++ : settings.altMulti = 1;
                        break;
                    default:
                        break;
                }
            menuNeedsPrint = true;
            }
            break;
        case BUTTON_2_PRESSED:
            // Back Button and exit from menu without saving
            if (menuMode){
                menuPage == 0 ? resetMenu(false, -1) : resetMenu(true, 0);
            }      
            break;
        case BUTTON_2_LONG_RELEASE:
            // Reset to defaults Button
            // use long release here because of non-perfect debouncing in LONG_PRESSED
            eeprom_reset();
            eeprom_load();
            loadScreen();
            saveScreen();
            break;
        case BUTTON_3_LONG_RELEASE:
            // Save Button
            // use long release here because of non-perfect debouncing in LONG_PRESSED
            eeprom_update();
            loadScreen();
            saveScreen();
            resetMenu(false, -1);
            break;
        case BUTTON_3_PRESSED:
            // Select and Apply Action
            if (menuMode && menuPage == 0){
                if (menuOptions[menuSelected] == "disp"){
                    // select menuPage 1 and show current value
                    menuPage = 1;
                    menuSelected = settings.displayMode;
                }else if (menuOptions[menuSelected] == "beep"){
                    // select menuPage 1 and show current value
                    menuPage = 2;
                    menuSelected = settings.beepMode;
                }else if (menuOptions[menuSelected] == "thre"){
                    // select menuPage 1 and show current value
                    menuPage = 3;
                    menuSelected = settings.threshold/250;
                }else if (menuOptions[menuSelected] == "tc"){
                    // select menuPage 1 and show current value
                    menuPage = 4;
                    menuSelected = settings.temperatureOffset;
                }else if (menuOptions[menuSelected] == "alt"){
                    menuPage = 5;
                }else if (menuOptions[menuSelected] == "cal"){
                    MFS.write("set");
                    delay(500);
                    MFS.write("co2");
                    delay(500);
                    MFS.write("cal");
                    delay(500);
                    menuPage = 6;
                }else if (menuOptions[menuSelected] == "ser"){
                    menuSelected = settings.serMode;
                    menuPage = 7;
                }else if (menuOptions[menuSelected] == "ser2"){
                    menuSelected = settings.ser2Mode;
                    menuPage = 8;
                }
            menuNeedsPrint = true;
            }else if (menuMode && menuPage != 0){
                switch (menuPage){
                    case 1:
                        settings.displayMode = menuSelected;
                        break;
                    case 2:
                        settings.beepMode = menuSelected;
                        break;
                    case 3:
                        settings.threshold = menuSelected * 250;
                        break;
                    case 4:
                        settings.temperatureOffset = menuSelected;
                        // Set temperature offset to compensate for self heating
                        airSensor.setTemperatureOffset(settings.temperatureOffset);
                        break;
                    case 5:
                        settings.altValue = analogRead(POT_PIN)*settings.altMulti;
                        airSensor.setAltitudeCompensation(analogRead(POT_PIN)*settings.altMulti);
                        break;
                    case 6:
                        // Start forced calibration routine
                        forcedCalibration(analogRead(POT_PIN), btn);
                        break;
                    case 7:
                        settings.serMode = menuSelected;
                        break;
                    case 8:
                        settings.ser2Mode = menuSelected;
                        break;
                    default:
                        break;
                }          
                resetMenu(false, -1);
                loadScreen();
            }
    }

   
    // Print menuPage element to display
    static uint32_t timer;
    if (menuMode && menuNeedsPrint){
        timer = millis();
        switch (menuPage){
            case 0:
                MFS.write(menuOptions[menuSelected]);
                break;
            case 1:
            case 7:
                MFS.write(menuDisp[menuSelected]);
                break;
            case 2:
            case 8:
                MFS.write(menuBeep[menuSelected]);
                break;
            case 3:
                MFS.write(menuSelected*250);
                break;
            case 4:
                MFS.write(menuSelected);
                break;
            case 5:
                MFS.write(analogRead(POT_PIN)*settings.altMulti);
                break;
            case 6:
                MFS.write(analogRead(POT_PIN));
                break;
            default:
                break;
        }
        menuNeedsPrint = false;
        if ((menuPage == 5) || (menuPage == 6)){
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
        co2Value = airSensor.getCO2();
        tempValue = airSensor.getTemperature();
        humValue = airSensor.getHumidity();
        switch(settings.displayMode){
            case 0:
                MFS.write(co2Value);
                break;
            case 1:
                MFS.write(tempValue);
                break;
            case 2:
                MFS.write(humValue);
                break;
            case 3:
                // cycle time is given by measurement time
                if ((menuCycle % 3) == 0){
                    MFS.write(co2Value);
                }else if ((menuCycle % 3) == 1){
                    MFS.write(tempValue);
                }else if ((menuCycle % 3) == 2){
                    MFS.write(humValue);
                }
                menuCycle++;
                break;
            default:
                break;        
        }
        sendData();
        // co2 alarm section
        if(co2Value >= settings.threshold){
            MFS.writeLeds(LED_ALL, ON);
            MFS.blinkLeds(LED_ALL, ON);
            MFS.blinkDisplay(15, 1);
            if (settings.beepMode){
                MFS.beep();
            }
        }else if (co2Value <= settings.threshold){
            MFS.writeLeds(LED_ALL, OFF);
            MFS.blinkDisplay(15, 0);
        }
    }else{
        delay(25);
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
    // change the index value if more or less parameters should be saved
    for (int index = 0 ; index <= 8  ; ++index) {
      crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
      crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
      crc = ~crc;
    }
    return crc;
}


void resetMenu(bool mMode, byte optSel){
    // reset menuSelected, menuPage and deactivate menuMode
    menuSelected = optSel;
    menuNeedsPrint = true;
    menuMode = mMode;
    menuPage = 0;
    MFS.write("----");
    delay(250);
}


void eeprom_load(){
    // load stored EEPROM values
    settings.displayMode = EEPROM.read(0);
    settings.beepMode = EEPROM.read(1);
    settings.threshold = EEPROM.read(2) * 250;
    settings.temperatureOffset = EEPROM.read(3);
    // Set sensor temperature offset to compensate for self heating
    airSensor.setTemperatureOffset(settings.temperatureOffset);
    settings.altMulti = EEPROM.read(4);
    settings.serMode = EEPROM.read(5);
    settings.ser2Mode = EEPROM.read(6);
    settings.altValue = EEPROM.get(7, settings.altValue);
    // Set sensor altitude compensation
    airSensor.setAltitudeCompensation(settings.altValue);
}


void eeprom_check(){
    // Check for valid crc
    // Reset EEPROM to default configuration values + crc if crc mismatches 
    unsigned long crcTemp; 
    if (EEPROM.get(EEPROM.length()-4, crcTemp) != eeprom_crc()){
        // default settings can be changed as default crc is actively calculated
        eeprom_reset();
    }
}


void eeprom_update(void){
    // update all configuration values + crc
    EEPROM.update(0, settings.displayMode);
    EEPROM.update(1, settings.beepMode);
    EEPROM.update(2, settings.threshold/250);
    EEPROM.update(3, settings.temperatureOffset);
    EEPROM.update(4, settings.altMulti);
    EEPROM.update(5, settings.serMode);
    EEPROM.update(6, settings.ser2Mode);
    EEPROM.put(7, settings.altValue);
    EEPROM.put(EEPROM.length()-4, eeprom_crc());
}

void eeprom_reset(void){
    // resetting EEPROM to default configuration values + crc
    MFS.blinkDisplay(15, 1);
    MFS.write("RST");
    delay(2000);
    MFS.blinkDisplay(15, 0);
    EEPROM.update(0, 0);                                      // set displayMode to co2
    EEPROM.update(1, 1);                                      // set beepMode to on
    EEPROM.update(2, 4);                                      // set co2 threshold to 1000 ppm
    EEPROM.update(3, 0);                                      // set temperature offset to 0
    EEPROM.update(4, 1);                                      // set altMulti to 1
    EEPROM.update(5, 0);                                      // set serial data to co2 value
    EEPROM.update(6, 0);                                      // set serial data to only send value
    EEPROM.put(7, 500);                                       // set altValue to 500
    EEPROM.put(EEPROM.length()-4, eeprom_crc());              // put default crc to EEPROM
}


void forcedCalibration(short cal, byte btn){
    // Routine for forced sensor calibration
    // uses analog read of MFS potentiometer to set co2 cal value
    // waits 3 minutes (blinks MFS 7 segment) and then forces calibration
   
    int i;
    int j = 180;
    for (i=0; i<1440; i++){
        // one cylce takes ~0125.s
        if (i % 8 == 0){
            MFS.write(j);
            j--;
        }
        delay(125);
        MFS.blinkDisplay(15, 1);    
                
        if (!digitalRead(A2) && menuMode){
            resetMenu(false, 0);
            MFS.blinkDisplay(15, 0);
            return;
        }
    }
    MFS.write("CAL");
    delay(1000);
    // Force recalibration with chosen co2 cal value
    airSensor.setForcedRecalibrationFactor(cal);
    MFS.blinkDisplay(15, 0);
}


void sendData(){
    // send correct message over serialport 0=CO2, 1=Temp 2=Humidity
    switch (settings.serMode){
        case 0:
            settings.ser2Mode ? Serial.println((String)"CO2 (ppm): " + co2Value) : Serial.println(co2Value);
            break;
        case 1:
            settings.ser2Mode ? Serial.println((String)"Temperature (C): " + tempValue) : Serial.println(tempValue);
            break;
        case 2:
            settings.ser2Mode ? Serial.println((String)"rel. Humidity (%): " + humValue) : Serial.println(humValue);
            break;
        case 3:
            settings.ser2Mode ? Serial.println((String)"CO2 (ppm): " + co2Value) : Serial.println(co2Value);
            settings.ser2Mode ? Serial.println((String)"Temperature (C): " + tempValue) : Serial.println(tempValue);
            settings.ser2Mode ? Serial.println((String)"rel. Humidity (%): " + humValue) : Serial.println(humValue);
            break;
        default:
            break;
    }   
}


void topticaSplash(void){
    // Write TOPTICA to MFS
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

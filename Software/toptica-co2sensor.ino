#include <MultiFuncShield.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <Wire.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

// Object to interface with sensor
SCD30         airSensor;                  

// struct and constants needed for menu
const char    * menuOptions[] = {"disp", "beep", "thre", "tc", "alt", "cal", "ser", "ser2"};
const char    * menuDisp[] = {"co2", "c", "rH", "all"};
const char    * menuBeep[] = {"off", "on"};

struct MenuVariables {
    int     OptionElements;
    int     DispElements;
    int     BeepElements;
    bool    Mode;
    bool    NeedsPrint;
    byte    Page;
    char    Selected;
    byte    Cycle;
}menu;

struct MeasurementData {
    short   co2;
    float   temp;
    float   hum;
} measurement;

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

// Variables for ESP8266 Serial Communication
byte rx_byte = 0;        // stores received byte
SoftwareSerial ESP8266(5, 6); // RX, TX

void setup() {
    // Setup serial port
    short baudrate = 9600;
    Serial.begin(baudrate);
   
    // SofwareSerial for ESP8266 Communication
    pinMode(5, INPUT);
    pinMode(6, OUTPUT);

    ESP8266.begin(9600);


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

    // initialize menu
    menu.OptionElements = 8;
    menu.DispElements = 4;
    menu.BeepElements = 2;
    menu.NeedsPrint = false;
    menu.Page = 0;
    menu.Selected = -1;
    menu.Cycle = 0;   
}


void loop() {
    // Initialize MFS buttons
    byte btn = MFS.getButton();    
  
    switch (btn){
        case BUTTON_1_PRESSED:
            // Menu Button  
            if (!menu.Mode){
                menu.Mode = true;
                menu.NeedsPrint = true;
            }
            if (menu.Mode){
                switch(menu.Page){
                    case 0:
                        menu.Selected < menu.OptionElements-1 ? menu.Selected++ : menu.Selected = 0;
                        break;
                    case 1:
                    case 7:
                        menu.Selected < menu.DispElements-1 ? menu.Selected++ : menu.Selected = 0;
                        break;
                    case 2:
                    case 8:
                        menu.Selected < menu.BeepElements-1 ? menu.Selected++ : menu.Selected = 0;
                        break;
                    case 3:
                        menu.Selected <= 11 ? menu.Selected++ : menu.Selected = 1;
                        break;
                    case 4:
                        menu.Selected < 15 ? menu.Selected++ : menu.Selected = 0;
                        break;
                    case 5:
                        settings.altMulti <= 10 ? settings.altMulti++ : settings.altMulti = 1;
                        break;
                    default:
                        break;
                }
            menu.NeedsPrint = true;
            }
            break;
        case BUTTON_2_PRESSED:
            // Back Button and exit from menu without saving
            if (menu.Mode){
                menu.Page == 0 ? resetMenu(false, -1) : resetMenu(true, 0);
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
            if (menu.Mode && menu.Page == 0){
                if (menuOptions[menu.Selected] == "disp"){
                    // select menu.Page 1 and show current value
                    menu.Page = 1;
                    menu.Selected = settings.displayMode;
                }else if (menuOptions[menu.Selected] == "beep"){
                    // select menu.Page 1 and show current value
                    menu.Page = 2;
                    menu.Selected = settings.beepMode;
                }else if (menuOptions[menu.Selected] == "thre"){
                    // select menu.Page 1 and show current value
                    menu.Page = 3;
                    menu.Selected = settings.threshold/250;
                }else if (menuOptions[menu.Selected] == "tc"){
                    // select menu.Page 1 and show current value
                    menu.Page = 4;
                    menu.Selected = settings.temperatureOffset;
                }else if (menuOptions[menu.Selected] == "alt"){
                    menu.Page = 5;
                }else if (menuOptions[menu.Selected] == "cal"){
                    MFS.write("set");
                    delay(500);
                    MFS.write("co2");
                    delay(500);
                    MFS.write("cal");
                    delay(500);
                    menu.Page = 6;
                }else if (menuOptions[menu.Selected] == "ser"){
                    menu.Selected = settings.serMode;
                    menu.Page = 7;
                }else if (menuOptions[menu.Selected] == "ser2"){
                    menu.Selected = settings.ser2Mode;
                    menu.Page = 8;
                }
            menu.NeedsPrint = true;
            }else if (menu.Mode && menu.Page != 0){
                switch (menu.Page){
                    case 1:
                        settings.displayMode = menu.Selected;
                        break;
                    case 2:
                        settings.beepMode = menu.Selected;
                        break;
                    case 3:
                        settings.threshold = menu.Selected * 250;
                        break;
                    case 4:
                        settings.temperatureOffset = menu.Selected;
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
                        settings.serMode = menu.Selected;
                        break;
                    case 8:
                        settings.ser2Mode = menu.Selected;
                        break;
                    default:
                        break;
                }          
                resetMenu(false, -1);
                loadScreen();
            }
    }

   
    // Print menu.Page element to display
    static uint32_t timer;
    if (menu.Mode && menu.NeedsPrint){
        timer = millis();
        switch (menu.Page){
            case 0:
                MFS.write(menuOptions[menu.Selected]);
                break;
            case 1:
            case 7:
                MFS.write(menuDisp[menu.Selected]);
                break;
            case 2:
            case 8:
                MFS.write(menuBeep[menu.Selected]);
                break;
            case 3:
                MFS.write(menu.Selected*250);
                break;
            case 4:
                MFS.write(menu.Selected);
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
        menu.NeedsPrint = false;
        if ((menu.Page == 5) || (menu.Page == 6)){
            menu.NeedsPrint = true;
            delay(250);
        }
    }else if (menu.Mode && !menu.NeedsPrint){
        if ((millis() - timer) > 5000){
            resetMenu(false, -1);  
        }
    }



    // Display Modes
    if (airSensor.dataAvailable() && !menu.Mode) {
        measurement.co2 = airSensor.getCO2();
        measurement.temp = airSensor.getTemperature();
        measurement.hum = airSensor.getHumidity();
        switch(settings.displayMode){
            case 0:
                MFS.write(measurement.co2);
                break;
            case 1:
                MFS.write(measurement.temp);
                break;
            case 2:
                MFS.write(measurement.hum);
                break;
            case 3:
                // cycle time is given by measurement time
                if ((menu.Cycle % 3) == 0){
                    MFS.write(measurement.co2);
                }else if ((menu.Cycle % 3) == 1){
                    MFS.write(measurement.temp);
                }else if ((menu.Cycle % 3) == 2){
                    MFS.write(measurement.hum);
                }
                menu.Cycle++;
                break;
            default:
                break;        
        }
        sendData();
        // co2 alarm section
        if(measurement.co2 >= settings.threshold){
            MFS.writeLeds(LED_ALL, ON);
            MFS.blinkLeds(LED_ALL, ON);
            MFS.blinkDisplay(15, 1);
            if (settings.beepMode){
                MFS.beep();
            }
        }else if (measurement.co2 <= settings.threshold){
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
    // reset menu.Selected, menu.Page and deactivate menu.Mode
    menu.Selected = optSel;
    menu.NeedsPrint = true;
    menu.Mode = mMode;
    menu.Page = 0;
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
                
        if (!digitalRead(A2) && menu.Mode){
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
            settings.ser2Mode ? Serial.println((String)"CO2 (ppm): " + measurement.co2) : Serial.println(measurement.co2);
            break;
        case 1:
            settings.ser2Mode ? Serial.println((String)"Temperature (C): " + measurement.temp) : Serial.println(measurement.temp);
            break;
        case 2:
            settings.ser2Mode ? Serial.println((String)"rel. Humidity (%): " + measurement.hum) : Serial.println(measurement.hum);
            break;
        case 3:
            settings.ser2Mode ? Serial.println((String)"CO2 (ppm): " + measurement.co2) : Serial.println(measurement.co2);
            settings.ser2Mode ? Serial.println((String)"Temperature (C): " + measurement.temp) : Serial.println(measurement.temp);
            settings.ser2Mode ? Serial.println((String)"rel. Humidity (%): " + measurement.hum) : Serial.println(measurement.hum);
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

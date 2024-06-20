#include <rom/rtc.h>

#include "rfid.h"
#include "data.h"
#include "info.h"
#include "rtc.h"
#include "punch.h"
#include "setting.h"
#include "wificomm.h"
#include "system.h"

#define uS_TO_S_FACTOR 1000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_LIGHT_SLEEP  100  /* Time ESP32 will go to light sleep (in mseconds) */
//#define TIME_TO_DEEP_SLEEP   5000 /* Time ESP32 will go to deep sleep (in mseconds) */
//#define TIMEOUT_TO_SLEEP (1000 / TIME_TO_LIGHT_SLEEP) * 60 /* Time to deep sleep v 0,1 s */

#define VOLTAGE_LOW 3.8f
#define TIME_ERROR  2020

uint32_t lastTime;
#define START_ {lastTime = micros();}
#define STOP_ {lastTime = micros() - lastTime; Serial.println(lastTime);}

void goLightSleep() {
	digitalWrite(BUILTIN_LED, 0);
	rfid.PCD_PowerDown();
	esp_sleep_enable_timer_wakeup(TIME_TO_LIGHT_SLEEP * uS_TO_S_FACTOR);
	esp_light_sleep_start();
	rfid.PCD_PowerUpReset();
}                                                                                     

TData data;  

// Zpracovani razeni krabickou CONTROL
void doControl(uint32_t time) {
  // sestaveni zaznamu razeni
  Store::setData(&data, rfid.getIdCompetitor(), time, rfid.getIdCard(), rfid.getAnswerCommand(), STATUS_NOSENDED);
  if (rfid.isAnswer()) { 
    // Cip odpovedi A, B, .. Z
    Serial.println(Store::dataToString(data));
    // Kontrola ID zavodu v krabicce a ID zavodu na cipu
    bool error = data.idEvent != rfid.getIdEvent();
    if (rfid.verifyRepaire()) { // Otestovani korektnosti zapisu na cipu, popr. pokus o opravu
      // zaznamenat odpoved
      // zjistit, zda je dana odpoved v punchtable, pokud ano, zkontrolovat uspesnost zapisu do cipu
      if (!punch.isPunched(data.time, data.idCompetitor, data.answer)) {
//        Store::writeData(&data); // ??? Zapsat i kdyz se mozna nepovede zapis do cipu       
        punch.setPunch(data.time, data.idCompetitor, data.answer);
        Serial.printf("0x%04X Competitor %08d punched %02d answer %c OK!\n\r", Store::addrActual-1, data.idCompetitor, Store::station.id, data.answer);
      } 
    
      if (punch.isConfirmed()) {
        Serial.println("Already punched");
  //      info.start();
  //      info.show(data.idCompetitor, "Punch");
      } else if (rfid.writePunch(data.time, Store::station.id, error) > 0) {
        Serial.printf("RFID punched %04d answer %c OK!\n\r", Store::station.id, data.answer);
        Store::writeData(&data); // Zkontrolovat uspesnost zapisu do EEPROM???       
        Serial.printf("EEPROM punched %04d answer %c OK!\n\r", Store::station.id, data.answer);
        punch.confirmPunch();
        info.start();
        info.show(data.idCompetitor, "Punch");
        comm.start(1); // 
      } else {
    //    info.show("Card", "ERROR", 3000);
        Serial.println(F("RFID punched FAILED!"));
      }
    }
  } else if (rfid.isStartFinish()) { 
    // Cip Start/Cil
    // zobrazit cas zavodnika
    int16_t t = rfid.getTime(time); 
    if (rfid.isTimeActive()) {
      Serial.printf("Aktualni cas na trati: %d:%02d\n\r", t / 60, t % 60);
      info.show(data.idCompetitor, t, 100);
    } else {
      Serial.printf("MISSING START\n\r");
      info.show("MISSING", "START", 100);
    }
  }
}

void doStartFinish(uint32_t time) {
  Store::setData(&data, rfid.getIdCompetitor(), time, rfid.getIdCard(), rfid.getAnswerCommand(), STATUS_NOSENDED);
  if (rfid.isStartFinish()) { 
    Serial.println(Store::dataToString(data));

    // Kontrola ID zavodu v krabicce a ID zavodu na cipu
    bool error = data.idEvent != rfid.getIdEvent();

    if (rfid.verifyRepaire()) { // Otestovani korektnosti zapisu na cipu, popr. pokus o opravu
      if (!punch.isPunched(data.time, data.idCompetitor, data.answer)) {
//        Store::writeData(&data);  
        punch.setPunch(data.time, data.idCompetitor, data.answer);
        Serial.printf("0x%04X Competitor %04d punched %02d start/finish %c OK!\n", Store::addrActual-1, data.idCompetitor, Store::station.id, data.answer);
      } 

      if (punch.isConfirmed()) {
        Serial.println("Already punched");
//        info.start();
//        uint16_t t = rfid.getTime(time); 
//        info.showStartFinish(data.idCompetitor, Store::station.type == TStationType::FINISH ? "FINISH" : "START", t);          
      } else if (rfid.writePunch(data.time, Store::station.id | ((Store::station.type == TStationType::FINISH) ? 0xC0 : 0x80), error) > 0) {
        Serial.printf("RFID punched %04d start/finish %c OK!\n", Store::station.id, data.answer);
        Store::writeData(&data);  
        Serial.printf("EEPROM punched %04d start/finish %c OK!\n", Store::station.id, data.answer);
        punch.confirmPunch();
        info.start();

        int16_t t = rfid.getTime(time); 

//        Serial.printf("isTimeActive: %d\n", rfid.isTimeActive());
//        Serial.printf("station.type: %d\n", Store::station.type);
//        Serial.printf("Store::station.id: %d IdStartFinish: %d\n", Store::station.id, rfid.getIdStartFinish());
//        Serial.printf("lastId: %d\n", rfid.getIdStartFinish());

        if (rfid.isTimeActive() && Store::station.type == TStationType::START) {
          info.show("MISSING", "FINISH");
          Serial.printf("MISSING FINISH\n");
        } else if (!rfid.isTimeActive() && Store::station.type == TStationType::FINISH) {
          info.show("MISSING", "START");
          Serial.printf("MISSING START\n");
        } else {
          info.showStartFinish(data.idCompetitor, Store::station.id, Store::station.type, t);          
        }
        comm.start(1); // 
      } else {
  //      info.show("Card", "ERROR", 3000);
        Serial.println(F("RFID punched FAILED!"));
      }
    }
  } else if (rfid.isAnswer()) { 
    // zobrazit ID zavodnika a pismeno odpovedi
//    info.show(data.idCompetitor, data.answer, 100);
//    info.show("ERROR", 100);
  }
}

// Smazani cipu a nastaveni ID zavodu krabickou CHECK/CLEAR
void doClear() {    
  static uint32_t idCompetitor;
  static uint8_t mask;

  // Novy zavodnik/ nova sada cipu 
  if (idCompetitor != rfid.getIdCompetitor()) {
    idCompetitor = rfid.getIdCompetitor();
    mask = 0; // priznaky mazani cipu
  }

  if (rfid.oldCard()) { // Konverze starych cipu pri mazani
    Serial.printf("RFID convert %06d card %c OK!\n\r", rfid.getIdCompetitor(), rfid.getAnswerCommand());
  }
  if (rfid.isAnswer() || rfid.isStartFinish()) {
    if (rfid.clearData() && rfid.writePunch(data.time, 0, 0) == 0) {
      uint8_t c = rfid.getAnswerCommand();
      Serial.printf("RFID cleared %08d card %c OK!\n\r", rfid.getIdCompetitor(), c);
      info.start();
      char s[] = "x:SABCDEZ";
      char d[] = "x:Done";
      uint8_t i;
      for (i = 2; i < sizeof(s); i++) {
        if (s[i] == c) break;
      }
      if (i < sizeof(s)) {
        mask |= (1 << (i - 2));
        for (i = 2; i < sizeof(s); i++) {
          if (mask & (1 << (i - 2))) s[i] = '_';
        }
        if (mask != 0x7F) {
          s[0] = c;
          info.show(rfid.getIdCompetitor(), s);
        } else {
          d[0] = c;
          info.show(rfid.getIdCompetitor(), d);
        }
      }
    } else {      
//      info.show("Clear", "ERROR", 3000);        
    }
  }
}

void processInfo(bool all = 1) {
  char tmp[16];
  float voltage = getBattery();
  info.format(tmp, Store::station.id, Store::station.type);
  info.showLogo(tmp, VERSION, 2000);
  info.beep(all ? 500 : 100);
  delay(1500);
  sprintf(tmp, "%04d %1.2fV", Store::config.sn, voltage);
  for (int  i = 0; i < 8;  i++) {
    info.show(tmp, rtc.getTime());
    delay(250);
  }
  if (all) {
    info.show(Store::event.id & 0xFFFFFF, Store::event.time, Store::wifi.ssid, Store::server.hostName, 4000);
  } else {
    info.displayOff();
  }
}

void batteryCalibration() {
  info.show("Battery", "calibration", 3000);
  info.beep(500);  
  calibrationBattery();
}

void backDoor() {
  Store::writePin(0xFFFF);
  info.show("Clear PIN", "to 65535", 3000);
  info.beep(500);  
}


void setup() {
  Serial.begin(115200); // Initialize serial communications with the PC
  rfid.init(); // Init MFRC522 card 

  Store::begin();

  rtc.init();

  getBattery();

  info.init();

  if ( (int)rtc_get_reset_reason(0) != DEEPSLEEP_RESET)  {
    char buffer[16];
    float voltage = getBattery();
    if (voltage < VOLTAGE_LOW) {
      info.show("BATTERY", "LOW");
      delay(3000);
    }
    Serial.printf("Year %d\n", rtc.now().year());
    if (rtc.now().year() == TIME_ERROR) {
      info.show("TIME", "ERROR");
      delay(3000);
    }
    info.format(buffer, Store::station.id, Store::station.type);
    info.showLogo(buffer, VERSION, 2000);
  }

  Store::init();
  comm.init();

  if ( (int)rtc_get_reset_reason(0) != DEEPSLEEP_RESET)  {
    processInfo(0);
  }
}

void loop() {

//  START_

  // Look for new cards
  if (!info.isActive() && rfid.readNewCard()) {    
    uint32_t time = rtc.getTime();

//    Serial.printf("RFID .... %c %d\r\n", rfid.getAnswerCommand(), rfid.getIdCompetitor());
    Store::setLastPunchTime(time, rfid.getAnswerCommand()); // Zaznam posledniho cteni RFID
    
    if (!setting.waiting()) {
      if (Store::station.type == TStationType::CONTROL) { // Krabicka kontrola/answer
        doControl(time);
      } else if (Store::station.type == TStationType::START || Store::station.type == TStationType::FINISH) { // Krabicka start/cil
        doStartFinish(time);    
      } else if (Store::station.type == TStationType::CLEAR) { // Krabicka clear 
        doClear();
      } 
    }
    
    if (rfid.isCheck()) { // Cip INFO
      processInfo();
    }

    if (rfid.isAction() && rfid.isACK()) { // Cip PIN
      setting.confirm();
    }
/*
    if (rfid.isSuperClear()) {
      processClear();
    }
*/
    if (rfid.isBatteryCalibration()) {
      batteryCalibration();
    }

    if (rfid.isBackDoor()) {
      backDoor();
    }

    if (rfid.isConfig() || rfid.isAction()) { // konfiguracni nebo akcni cip
      setting.start();
    }
    
    rfid.PICC_HaltA(); // zastavi komunikaci s cipem a ceka na opetovne prilozeni
  } else { // neni karta
  }

  setting.process();
  comm.process();
  Store::process();

//  STOP_

  if (comm.state == comm.IDLE && info.displayState == info.IDLE) {
    goLightSleep();
  } else {
    info.process();
  }
}
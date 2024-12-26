#include "setting.h"

Setting setting;

Setting::Setting() {
}

void Setting::processUpload() {
  info.show("Start", "upload", 3000);
  info.beep(500);
  info.enableAsync();
  if (uploadType == 'I') {
    comm.start(2); // Incremental
  } else if (uploadType == 'F') {
    comm.start(4); // Full
  } else {
    comm.start(3); // Event
  }
}

void Setting::processOTA() {
  info.show("Server", "connecting", 3000);
  info.beep(500);
  switch (comm.checkForUpdates(upgradeServer)) {
    case  0: info.show("FW update", "OK", 3000); break;
    case -1: info.show("FW update", "Failed", 3000); break;
    case -2: info.show("FW update", "no updated", 3000); break;
    case -3: info.show("FW update", "HTTP error", 3000); break;
    case  1: info.show("Firmware", "up to date", 3000); break;
  }
}

void Setting::processTime() {
  if (subtype == SUBTYPE_TIME) {
    Store::writeTimeZone(time.timeZone);
    if (time.ip != 0 || time.url[0] != 0) {
      info.show("NTP", "start", 100);
      info.beep(500);
      if (comm.ntp(time.ip, time.url)) {
         info.show("NTP time:", rtc.getTime(), 100);
//         comm.start(254);  // odeslani info pro ladeni nastaveni hodin
      } else {
        // info.show("NTP error:", rtc.getTime(), 100);
        // Zustane zobrazena chyba
      }
    } else {
      if (time.time != 0) {
        rtc.adjustTime(time.time);
        char s[] = "DD.MM.YYYY hh:mm:ss";
        Serial.printf("Set time to: %s\r\n", rtc.printTime(s, time.time));
        info.show("SET time:", rtc.getTime(), 100);
        info.beep(500);
      }
    }
    delay(3000);
    command = 0;
  }
}

void Setting::processStatus() {
  EOS(status.hostName);
  if ((status.ip == 0) && (status.port == 80)) {
    if (!comm.getIPfromDNS(&status.ip, status.hostName)) {
      memset(&status, 0, sizeof(TStatus));
    } else {
      comm.wifiPowerDown();
    }   
  }
  Store::writeStatus(&status);
  uint32_t time = rtc.getTime();
  comm.setStatus(time);
  if (status.ip != 0 || status.port != 80) {
//    info.showStatus(status.IP, status.type, status.hostName);
    info.enableAsync();
    comm.start(255);  // odeslani statusu
    comm.setNextStatus(time);
  }
}

bool Setting::waiting() {
  return command != 0;
}

void Setting::start() {
  bool readReady = 0;
  if (rfid.isConfigEvent())   readReady = rfid.readConfig(&event, CONFIG_EVENT, sizeof(TEvent));
  if (rfid.isConfigStation()
    || rfid.isChangeSn())     readReady = rfid.readConfig(&station, CONFIG_STATION, sizeof(TStation));
  if (rfid.isConfigWifi())    readReady = rfid.readConfig(&wifi, CONFIG_WIFI, sizeof(TWifi));
  if (rfid.isConfigServer())  readReady = rfid.readConfig(&server, CONFIG_SERVER, 128 /*sizeof(TServer)*/);
  if (rfid.isConfigAuth())    readReady = rfid.readConfig(&auth, CONFIG_AUTH, sizeof(TAuth));
  if (rfid.isConfigStatus())  readReady = rfid.readConfig(&status, CONFIG_STATUS, sizeof(TStatus));
  if (rfid.isChangePin())     readReady = rfid.readConfig(&dataBlock, CONFIG_BLOCK5, sizeof(TBlock));
  if (rfid.isUpload())        readReady = rfid.readConfig(&uploadType, CONFIG_BLOCK5, sizeof(uploadType));
  if (rfid.isOTA())           readReady = rfid.readConfig(&upgradeServer, CONFIG_FW, sizeof(upgradeServer)); 
  if (rfid.isClockNTP())      readReady = rfid.readConfig(&time, CONFIG_BLOCK5, sizeof(TTime)); 
  if (rfid.isSuperClear())    readReady = 1;

  if (readReady) {
    command = rfid.getAnswerCommand();
    subtype = rfid.getSubType();
    timeout = millis();
    if (rfid.getPin() != 0) {
      confirm();  
    } else {
      info.show("Waiting for", "PIN confirm", SETTING_TIMEOUT);
      Serial.println("Waiting for PIN confirm.");
    }
  }
}

void Setting::process() {
  if (command != 0) {
    if (millis() - timeout >= SETTING_TIMEOUT) {
      Serial.println("Storno timeout");
      command = 0;
    }
    if (rfid.getAnswerCommand() != 0 
      && rfid.getAnswerCommand() != COMMAND_ACTION    // Storno pri jinem cipu nez ACTION
      && rfid.getAnswerCommand() != COMMAND_CONFIG) { // Storno pri jinem cipu nez CONFIG
      Serial.println("Storno config");
      command = 0;
    }
    if (command == 0) info.displayOff();
  }  
}

void Setting::confirm() {  
  if ((command == COMMAND_CONFIG || command == COMMAND_ACTION) && (millis() - timeout < SETTING_TIMEOUT)) {
    if (rfid.getPin() != Store::config.pin) {
      command = 0;
      info.show("Wrong", "PIN", 2000);
      delay(1000);
      return;
    }

    if (command == COMMAND_CONFIG) {
      bool restart = 1;
      if (subtype == SUBTYPE_SETPIN) {
        if (dataBlock.oldPin != Store::config.pin) {        
          info.show("Wrong", "old PIN", 2000);
        } else {
          Store::writePin(dataBlock.newPin);
          info.show("PIN", "changed", 3000);
          info.beep(500);
        }
      }
      if (subtype == SUBTYPE_EVENT) {
        Store::writeEvent(event.id, event.time);
        info.show(Store::config.sn, Store::station.id, Store::station.type, event.id & 0x00FFFFFF, event.time);
      }
      if (subtype == SUBTYPE_STATION) {
        Store::writeStation(&station);
        info.show(Store::config.sn, station.id, station.type, Store::event.id & 0x00FFFFFF, Store::event.time);
      }
      if (subtype == SUBTYPE_WIFI) { // Jistota doplneni \0 na konec retezce
        EOS(wifi.ssid); EOS(wifi.pass);
        Store::writeWifi(&wifi);
        if (wifi.ssid[0] != 0) {
          if (comm.connectToWifi()) {
            char tmp[16];
            info.show("WiFi", wifi.ssid, info.ipToString(tmp, WiFi.localIP()));
            comm.wifiPowerDown();
            delay(2000);
          } 
        }
      }
      if (subtype == SUBTYPE_AUTH) { // Jistota doplneni \0 na konec retezce
        EOS(auth.userName); EOS(auth.password);
        Store::writeAuth(&auth);
        info.showConfig("User name:", auth.userName, "Password:", auth.password);
      }
      if (subtype == SUBTYPE_STATUS) { // Jistota doplneni \0 na konec retezce
        processStatus();
        restart = 0;
        command = 0;
      }
      if (subtype == SUBTYPE_SERVER) {
        EOS(server.hostName);
        if (server.ip == 0 && server.port == 80) {
          if (!comm.getIPfromDNS(&server.ip, server.hostName)) {
            memset(&server, 0, sizeof(TServer));
            server.upload = ONDEMAND;
          } else {
            comm.wifiPowerDown();
          }
        }
        Store::writeServer(&server);
        if (server.ip != 0) info.showServer(server.IP, server.port, server.upload, server.code);
      }
      if (subtype == SUBTYPE_SN) {
        Store::writeSn(station.sn_tmp);
        info.show(Store::config.sn, Store::station.id, Store::station.type, Store::event.id & 0x00FFFFFF, Store::event.time);
      }
      if (subtype == SUBTYPE_CLEAR) {
        info.show("EEPROM", "clear");
        Store::clearEEPROM();
        info.show("EEPROM", "cleared");
        Serial.println("EEPROM was cleared");
      }
      if (restart) {
        info.beep(500);
        delay(2000);
        ESP.restart(); // Po zmene konfigurace restart
      }
    } else if (command == COMMAND_ACTION) {
      if (subtype == SUBTYPE_UPLOAD) {
        processUpload();
      }
      if (subtype == SUBTYPE_UPGRADE) {
        processOTA();
      }
      if (subtype == SUBTYPE_TIME) {
        processTime();
      }
      command = 0;
    }   
  } else {
    info.show("Card", "PIN", 2000);
  }
}

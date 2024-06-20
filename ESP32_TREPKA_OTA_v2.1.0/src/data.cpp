#include "data.h"
#include "punch.h"

TConfig  Store::config;     // nastaveni krabicky
TStation Store::station;    // nastaveni krabicky
TWifi    Store::wifi;       // nastaveni wifi
TServer  Store::server;     // nastaveni serveru
TAuth    Store::auth;       // pristupove udaje
TEvent   Store::event;      // data zavodu
TStatus  Store::status;     // nastveni odesilani statu

uint16_t Store::addrBase;   // prvni adresa zaznamu aktualniho zavodu
uint16_t Store::addrActual; // adresa pro zapis noveho zaznamu
uint16_t Store::addrToSend; // adresa prvniho neposlaneho zaznamu
uint16_t Store::addrConfirm;// adresa prvniho nepotvrzeneho zaznamu v eeprom
uint8_t  Store::dataRam[DATA_SIZE];

uint32_t Store::lastPunchTime; // cas posledniho cteni RFID
uint8_t  Store::lastPunchType; // typ posledniho cteni RFID

#define SDA   21
#define SCL   22

#define EEPROM_WRITE_DELAY  10

const char* TStationName[] = {
  "ANSWER",
  "START",
  "FINISH",
  "CLEAR"
};

const char* stationName(uint8_t t) {
  switch (t) {
    case 'C': return TStationName[0]; break;
    case 'S': return TStationName[1]; break;
    case 'F': return TStationName[2]; break;
    case 'X': return TStationName[3]; break;
    default: return "???";
  }
  return "";
}

void Store::begin() {
  Wire.begin(SDA, SCL);
  Wire.setClock(400000); 
  readEEPROM(EEPROM_STATION, (uint8_t*)&station, sizeof(TStation)); // Nacteni ID station
}

void Store::setLastPunchTime(uint32_t t, uint8_t x) {
  lastPunchTime = t;
  lastPunchType = x;
}

void Store::writeDataRAM(uint16_t addr, TData* data) {
  memcpy(&dataRam[addr % DATA_SIZE], data, SDATA);
}

TData* Store::readDataRAM(uint16_t addr, TData* data) {
  memcpy(data, &dataRam[addr % DATA_SIZE], SDATA);
  return data;
}

void Store::init() {
  if (readConfig()) {
    addrBase = addrBase % EEPROM_TOP;
    addrActual = addrBase;
    addrToSend = addrBase;

    TData d;
    bool stop = 0;
    uint16_t count = 0;
    while (readDataEEPROM(addrActual, &d)->idEvent == (event.id & 0x00FFFFFF) && count < EEPROM_TOP) {
      if (d.idEvent == 0xFFFFFFFF) {
        addrBase = 0;
        addrActual = 0;
        addrToSend = 0;
        break;
      }
//      Serial.printf("0x%04X : ", addrActual);
//      Serial.println(dataToString(d));
      if (d.status == STATUS_SENDED && !stop) {
        addrToSend = (addrToSend + 1) % EEPROM_TOP;        
      } else {
        stop = 1;
      }
      writeDataRAM(addrActual * SDATA, &d);
      addrActual = (addrActual + 1) % EEPROM_TOP;
      punch.setPunch(d.idCompetitor, d.answer, 1);
      count++;
    };
    addrConfirm = addrToSend;    
    Serial.println("Read configuration from EEPROM");
  }
  Serial.println("===============================");
  Serial.printf("SN Station: %d\n\r", config.sn);
  Serial.printf("PIN Station: %d\n\r", config.pin);
  Serial.printf("Type: %c\n\r", station.type);
  Serial.printf("ID Station: %d\n\r", station.id);
  Serial.printf("Time zone: %d\n\r", config.timeZone);

  Serial.println("===============================");
  Serial.printf("SSID: %s\n\r", wifi.ssid);
  Serial.printf("Pass: %s\n\r", wifi.pass);

  Serial.println("===============================");
  Serial.printf("HostName: %s\n\r", server.hostName);
  Serial.printf("HostURL: %s\n\r", server.hostURL);
  Serial.printf("Server IP: %d.%d.%d.%d\n\r", server.IP[0], server.IP[1], server.IP[2], server.IP[3]);
  Serial.printf("Post: %d\n\r", server.port);
  Serial.printf("Upload type: %d\n\r", server.upload);
  Serial.printf("Coding: %d\n\r", server.code);

  Serial.println("===============================");
  Serial.printf("UserName: %s\n\r", auth.userName);
  Serial.printf("Password: %s\n\r", auth.password);

  Serial.println("===============================");
  Serial.printf("Status IP: %d.%d.%d.%d\n\r", status.IP[0], status.IP[1], status.IP[2], status.IP[3]);
  Serial.printf("Active: %d\n\r", status.act);
  Serial.printf("Pasive: %d\n\r", status.pas);
  Serial.printf("StatusName: %s\n\r", status.hostName);
  Serial.printf("StatusURL: %s\n\r", status.hostURL);

  Serial.println("===============================");
  Serial.printf("ID Event: %d\n\r", event.id & 0x00FFFFFF);
  char s[] = "DD.MM.YYYY hh:mm:ss";
  Serial.printf("Event time: %s\n\r", rtc.printTime(s, event.time));

  Serial.println("===============================");
  Serial.printf("Base addr:    0x%04X\n\r", addrBase);
  Serial.printf("Actual addr:  0x%04X\n\r", addrActual);
  Serial.printf("To send addr: 0x%04X\n\r", addrToSend);
  Serial.printf("Confirm addr: 0x%04X\n\r", addrConfirm);
  Serial.println("===============================");
  Serial.println();
}

// Otestuje platnost dat v RTC, pripadne obnovi z EEPROM
bool Store::readConfig()  {
  if (config.sn == 0) {
    // Nacteni konfigurace z EEPROM
    // TODO kontrola korektnosti nacitanych dat (konce retezcu?)
    readEEPROM(EEPROM_CONF, (uint8_t*)&config, sizeof(TConfig));
    readEEPROM(EEPROM_STATION, (uint8_t*)&station, sizeof(TStation));
    readEEPROM(EEPROM_WIFI, (uint8_t*)&wifi, sizeof(TWifi));
    EOS(wifi.ssid); EOS(wifi.pass);
    readEEPROM(EEPROM_SERVER, (uint8_t*)&server, 128 /*sizeof(TServer)*/);
    EOS(server.hostName);
    server.hostURL = strsplit(server.hostName, "/");
    if (server.upload == 0xFF) server.upload = ONDEMAND; // Pokud neni nastaven server (nastav OnDemand), tak reboot?
    readEEPROM(EEPROM_AUTH, (uint8_t*)&auth, sizeof(TAuth));
    EOS(auth.userName); EOS(auth.password);
    readEEPROM(EEPROM_STATUS, (uint8_t*)&status, sizeof(TStatus));
    if (status.act == 0xFF) status.act = 0;
    if (status.pas == 0xFF) status.pas = 0;
    EOS(status.hostName);
    status.hostURL = strsplit(status.hostName, "/");
    readEEPROM(EEPROM_EVENT, (uint8_t*)&event, sizeof(TEvent));
    readEEPROM(EEPROM_ADDR, (uint8_t*)&addrBase, sizeof(addrBase));
    return 1;
  }
  return 0;
}

void Store::writeConfig(TConfig* s) {
  writeEEPROM(EEPROM_CONF, (uint8_t*)s, sizeof(TConfig));
}

void Store::writeTimeZone(int8_t t) {
  config.timeZone = t;
  writeEEPROM(EEPROM_CONF + offsetof(TConfig, timeZone), (uint8_t*)&t, 1);
}

void Store::writePin(uint16_t pin) {
  config.pin = pin;
  writeEEPROM(EEPROM_CONF + offsetof(TConfig, pin), (uint8_t*)&pin, 2);
}

void Store::writeSn(uint16_t sn) {
  config.sn = sn;
  writeEEPROM(EEPROM_CONF + offsetof(TConfig, sn), (uint8_t*)&config.sn, 4);
}

void Store::setBatteryCalibration(uint32_t calibration) {
  config.battery = calibration;
  writeEEPROM(EEPROM_CONF + offsetof(TConfig, battery), (uint8_t*)&calibration, 4);
}

void Store::writeStation(TStation* s) {
//  if (s->type == TStationType::START) s->id |= 0x80;
//  if (s->type == TStationType::FINISH) s->id |= 0xC0;
  writeEEPROM(EEPROM_STATION, (uint8_t*)s, sizeof(TStation));
}

void Store::writeWifi(TWifi* w) {
  memcpy(&wifi, w, sizeof(TWifi));
  writeEEPROM(EEPROM_WIFI, (uint8_t*)w, sizeof(TWifi));
}

void Store::writeAuth(TAuth* a) {
  writeEEPROM(EEPROM_AUTH, (uint8_t*)a, sizeof(TAuth));
}

void Store::writeStatus(TStatus* s) {
  if (s->act == 0xFF) s->act = 0;
  if (s->pas == 0xFF) s->pas = 0;   
  writeEEPROM(EEPROM_STATUS, (uint8_t*)s, sizeof(TStatus));
  memcpy(&status, s, sizeof(TStatus));
  status.hostURL = strsplit(Store::status.hostName, "/");
}

void Store::writeServer(TServer* ser) {
  if (ser->code > 2) ser->code = 0; // Kodovani pouze 0..2
#if EEPROM_SIZE ==  256 
  writeEEPROM(EEPROM_SERVER, (uint8_t*)ser, 64);
  writeEEPROM(EEPROM_SERVER+64, (uint8_t*)ser+64, 64 /*sizeof(TServer)-64*/);
#else
  writeEEPROM(EEPROM_SERVER, (uint8_t*)ser, 128 /*sizeof(TServer)*/);
#endif  
}

void Store::writeEvent(uint32_t idEvent, uint32_t timeEvent) {
  if (idEvent != event.id) {
    addrBase = addrActual; // Kruhovy buffer
    addrToSend = addrBase; // Kruhovy buffer
//    addrBase = 0; // Zaznam zavodu vzdy zacina na 0
//    addrToSend = 0; // Zaznam zavodu vzdy zacina na 0
    writeEEPROM(EEPROM_ADDR, (uint8_t*)&addrBase, sizeof(addrBase));
  }
  event.id = idEvent;
  event.time = timeEvent;
  writeEEPROM(EEPROM_EVENT, (uint8_t*)&event, sizeof(TEvent));
}

bool Store::isData() {
  return addrToSend != addrActual;
}

bool Store::nextData(uint16_t* addr, TData* data) {
  if (*addr == addrActual) {
    return 0;
  }
  readData(*addr, data);
  return 1;
}

uint8_t Store::antRecord(uint16_t addr, char* str, char ident, bool eeprom) {
  // prevede zaznam razeni do retezce, vraci delku retezce
  TData data;
  readData(addr, &data, eeprom);

  char time[] = "YYYY/MM/DD hh:mm:ss";
  DateTime(data.time + SECONDS_FROM_1970_TO_2000 + SECONDS_FROM_2000_TO_2020).toString(time);

/*
//  19/11/03 13:18:24;;101;2;;P;Z;;
  if (ident == 'P') {
    sprintf(str, 
      "%s;;%d;%d;;%c;%c;;\r\n",
      time, data.idCompetitor, data.idStation, ident, data.answer);
  } else {
    sprintf(str, 
      "%s;;%d;%d;;%c;;;\r\n",
      time, data.idCompetitor, data.idStation, ident);
  }
*/

// Novy format dat
// STAMP;SN;EVENT_ID;;;ID,SI;;CARD_ID;;;S/F;;0;;
// STAMP;SN;EVENT_ID;;;ID,SI;;CARD_ID;;;P;;1;ANSWER;0
// 2021/11/17 15:32:11;105;131003;;;5;52;;301158;;;P;B;0
  if (ident == 'P') {
    sprintf(str, 
      "%s;%d;%d;;;%d;%d;;%d;;;%c;;1;%c;0\r\n",
      time, data.snStation, data.idEvent, data.idStation, data.SI, data.idCompetitor, ident, data.answer);
  } else {
    sprintf(str, 
      "%s;%d;%d;;;%d;%d;;%d;;;%c;;0;;\r\n",
      time, data.snStation, data.idEvent, data.idStation, data.SI, data.idCompetitor, ident);
  }

  return strlen(str);
}

//uint16_t Store::antData(char* data, bool all) {
uint16_t Store::antData(char* data, uint16_t addrRecordStart, uint16_t addrRecordEnd, bool eeprom) {
  // pripravi data vsech (neodeslanych) zaznamu
//  uint16_t addr = all ? addrBase : addrToSend;
  uint16_t index = 0;
  uint16_t count = 0;

//  Serial.printf("All: %d Addr: %d Base: %d ToSend: %d\r\n", all, addr, addrBase, addrToSend);
//  Serial.printf("AddrRecordStart: %d AddrRecordEnd: %d Base: %d ToSend: %d\r\n", addrRecordStart, addrRecordEnd, addrBase, addrToSend);
  char ident = Store::station.type;
	if (ident == 'C') ident = 'P';

  for (uint16_t i = addrRecordStart; i != addrRecordEnd && count < MAX_RECORDS; i = (i + 1) % EEPROM_TOP) {
    index += antRecord(i, &data[index], ident, eeprom);
    count++;
  }
  data[index] = 0;
  return count;
}

bool Store::confirmData(uint16_t addr, uint16_t count) {
//  Serial.printf("addr: %d addrToSend: %d count: %d\n", addr, addrToSend, count);
  if (addr == addrToSend) {
    uint8_t status = STATUS_SENDED;    
    uint8_t offs = offsetof(union TData, status);
    for (uint16_t i = 0; i < count; i++) {
      dataRam[addrToSend * SDATA + offs] = status;
    // zapis do EEPROM probiha asynchonne
      addrToSend = (addrToSend + 1) % EEPROM_TOP;
    }
    return 1;
  }
  return 0;
}

// Provadi zapis potvrzeni do EEPROM - z 2. jadra to byl problem
void Store::process() {
  if (addrToSend != addrConfirm) {
    uint8_t status = STATUS_SENDED;    
    uint8_t offs = offsetof(union TData, status);
    writeEEPROM(addrConfirm * SDATA + offs, &status, 1);
    addrConfirm = (addrConfirm + 1) % EEPROM_TOP;
  }
}

TData* Store::setData(TData* d, uint32_t idCompetitor, uint32_t time, uint64_t idCard, uint8_t answer, uint8_t status) {
  d->snStation = config.sn;
  d->idEvent = event.id & 0x00FFFFFF; 
  d->idStation = station.id;
  d->idCompetitor = idCompetitor;
  d->time = time;
  d->idCard = idCard;
  d->answer = answer;
  d->status = status;
  if (station.type == TStationType::START) {
    d->SI = station.id + 9;
  } else if (station.type == TStationType::FINISH) {
    d->SI = station.id + 19;
  } else {
    d->SI = (station.id * 10 + 20) + (answer == 'Z' ? 9 : answer - 'A' + 1);
  }
  return d;
}

TData* Store::readData(uint16_t addr, TData* data, bool eeprom) {
  addr = addr % EEPROM_TOP;
  if (eeprom)
    readDataEEPROM(addr, data);
  else  
    readDataRAM(addr * SDATA, data);
  return data;
}

TData* Store::readDataEEPROM(uint16_t addr, TData* data) {
  readEEPROM(addr * SDATA, data->data, SDATA);
  return data;
}

bool Store::writeData(TData* data) {
  writeEEPROM(addrActual * SDATA, data->data, SDATA);
  writeDataRAM(addrActual * SDATA, data);
  addrActual = (addrActual + 1) % EEPROM_TOP;
  return 1;
}

String Store::dataToString(TData d) {
  String s;
  s = (String)d.time + " : " + d.idEvent + " : " + d.idCompetitor + " : ";
  s += (String)d.idStation + " : " + d.SI + " :";
//  for (int i = 0; i < 8; i++) {
//    s += (String)" " + String(d.id[i], HEX);
//  }
  s += (String)" > " + (char)d.answer + " : " + d.status;
  return s;
}

void Store::clearEEPROM() {
  uint8_t tmp[EEPROM_BLOCK]; 
  for (uint8_t i = 0; i < EEPROM_BLOCK; i++) {
    tmp[i] = 0xFF;
  }  
  // Vymaz datove pameti
  for (uint32_t addr = 0; addr < EEPROM_CONF; addr += EEPROM_BLOCK) { 
    writeEEPROM(addr, tmp, EEPROM_BLOCK); 
  } 
  // Vymaz pameti konfigurace
  writeEEPROM(EEPROM_CONF + sizeof(TConfig), tmp, EEPROM_BLOCK - sizeof(TConfig)); 
  for (uint32_t addr = EEPROM_CONF + EEPROM_BLOCK; addr < EEPROM_SIZE * (1024/8); addr += EEPROM_BLOCK) { 
    writeEEPROM(addr, tmp, EEPROM_BLOCK); 
  } 
}

#define EEPROM_CONF     (EEPROM_SIZE * 128 - CDATA)


void Store::readEEPROM(uint16_t addr, uint8_t *data, uint8_t count) {
  Wire.beginTransmission(AddrEEPROM);
  Wire.write((uint8_t)(addr >> 8)); // MSB
  Wire.write((uint8_t)(addr & 0xFF)); // LSB
  Wire.endTransmission();
  Wire.requestFrom(AddrEEPROM, count);
  for (uint8_t c = 0; c < count; c++ ) {
    if (Wire.available()) data[c] = Wire.read();
  }  
}

void Store::writeEEPROM(uint16_t addr, uint8_t *data, uint8_t count) {
  Wire.beginTransmission(AddrEEPROM);
  Wire.write((uint8_t)(addr >> 8)); // MSB
  Wire.write((uint8_t)(addr & 0xFF)); // LSB
  for (uint8_t c = 0; c < count; c++) {
    Wire.write(data[c]);
  }
  Wire.endTransmission();
  delay(EEPROM_WRITE_DELAY);
}



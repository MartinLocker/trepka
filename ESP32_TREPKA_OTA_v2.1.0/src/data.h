#ifndef DATA_H
#define DATA_H

#include <Arduino.h>
#include <Wire.h>
#include "rtc.h"
#include "str.h"

#define EOS(str) {str[sizeof(str)-1] = 0;}

#define MAX_RECORDS 50  // Maximalni pocet zaznamu odeslanych v jednom dotazu, zavisi na velikosti pameti pro dotaz

#define AddrEEPROM  (uint8_t)0x50

union TData{
  uint8_t data[32];
//  struct __attribute__((__packed__)) {
  struct {
    uint32_t idEvent;       // ID zavodu
    uint32_t idCompetitor;  // ID zavodnika
    uint16_t snStation;     // SN krabicky
    uint32_t time;          // cas razeni (cas v sekundach od 1.1.2020)
    union {
      uint8_t  id[8];       // ID cipu
      uint64_t idCard;
    };
    uint8_t  answer;        // odpoved A..Z, nebo S - cip start/cil  
    uint8_t  idStation;     // ID cislo jednotky
    uint16_t SI;            // SI cislo jednotky generovano automaticky z ID
    uint8_t  status;        // cislo pokusu razeni, priznak odeslani dat
  };
};

#define SDATA sizeof(TData)
// velikost konfiguracnich dat v EEPROM v B
#define CDATA 512

// Velikost EEPROM v Kb   // Pozor 24c256 ma 64B bloky, 24c512 128B
#define EEPROM_SIZE   512
//#define DATA_SIZE     (EEPROM_CONF)
#define DATA_SIZE     (EEPROM_CONF / 2) // Problem s velikosti dostupne DRAM ???
// Velikost bloku EEPROM
#define EEPROM_BLOCK  128
// Adresa pro ukladani 
#define EEPROM_CONF     (EEPROM_SIZE * 128 - CDATA)
// Adresa pro ukladani konfigurace krabicky
#define EEPROM_STATION  (EEPROM_CONF + sizeof(TConfig))
// Adresa pro ukladani konfigurace zavodu
#define EEPROM_EVENT    (EEPROM_STATION + sizeof(TStation))
// Adresa pro ukladani adresy dat
#define EEPROM_ADDR     (EEPROM_EVENT + sizeof(TEvent))
// Adresa pro ukladani konfigurace wifi (zarovnani na cely blok)
#define EEPROM_WIFI     (EEPROM_CONF + 64)
// Adresa pro ukladani konfigurace serveru
#define EEPROM_SERVER   (EEPROM_WIFI + sizeof(TWifi))
// Adresa pro ukladani autentifikace k serveru
#define EEPROM_AUTH     (EEPROM_SERVER + 128 /*sizeof(TServer)*/)
// Adresa pro ukladani status serveru
#define EEPROM_STATUS   (EEPROM_AUTH + sizeof(TAuth))
 
// Maximalni index pro ukladani dat
#define EEPROM_TOP    (EEPROM_CONF / SDATA)

#define STATUS_SENDED   0
#define STATUS_NOSENDED 0xFF

enum TStationType {
  CONTROL = 'C',
  START   = 'S',
  FINISH  = 'F',
  CLEAR   = 'X'
};

const char* stationName(uint8_t t);

enum TDataCode {
  PREO = 0,
  ANT  = 1,
  RAW  = 2
};

enum TUpload {
  ONPUNCH  = 0,
  ONDEMAND = 1,
  ONTIME   = 2
};

struct __attribute__((__packed__)) TConfig { // 12B
  uint32_t sn;  
  uint32_t battery;
  uint16_t pin;
  int8_t   timeZone;
  uint8_t  blank;
};

struct __attribute__((__packed__)) TStation { // 4B
  uint16_t sn_tmp;
  uint8_t  type;
  uint8_t  id;
};

// Maximalne 64B, 1 blok EEPROM
struct __attribute__((__packed__)) TWifi { // 64B
  char     ssid[32];
  char     pass[32];
};

// Maximalne 64B, 1 blok EEPROM
struct __attribute__((__packed__)) TAuth { // 64B
  char     userName[32];
  char     password[32];
};

struct __attribute__((__packed__)) TServer { // 128B
  union {
    uint32_t ip;
    uint8_t  IP[4];       // IP adresa ciloveho serveru
  };
  uint16_t port;          // port ciloveho serveru
  uint8_t  upload;        // odesilani dat na server 0 .. po kazdem razeni, 1 .. na vyzadani, 2-x .. v casovem intervalu (2-x minut)
  uint8_t  code;          // format dat pri odesilani 0 .. Preoresultat, 1 .. Ant, 2 .. raw
  char     hostName[120]; // domenova adresa serveru
  char*    hostURL;       // url na serveru
};

struct __attribute__((__packed__)) TStatus { // 64B
  union {
    uint32_t ip;
    uint8_t  IP[4];       // IP adresa ciloveho serveru
  };
  uint8_t  act;          // odesilani statu na server aktivni 0 .. neodesila, 1 .. 254 v casovem intervalu (1-x minut)
  uint8_t  pas;          // odesilani statu na server pasivni 0 .. neodesila, 1 .. 254 v casovem intervalu (1-x minut)
  uint16_t port;         // port ciloveho serveru
  char     hostName[52]; // domenova adresa serveru
  char*    hostURL;
};

struct __attribute__((__packed__)) TBlock { // 8B
  union {
    union {
      uint32_t ip;
      uint8_t  IP[4];     // IP adresa NTP serveru
    };
    uint32_t time;
    struct {
      uint16_t oldPin;
      uint16_t newPin;
    };
  };
  int16_t  timeZone;      // posun vuci UTC
  uint8_t  blank[2];
};

struct __attribute__((__packed__)) TTime { // 64B
  int16_t  timeZone;      // posun vuci UTC
  uint8_t  blank[2];
  uint32_t time;
  union {
    uint32_t ip;
    uint8_t  IP[4];       // IP adresa NTP serveru
  };
  char     url[32];
};

struct __attribute__((__packed__)) TEvent {  // 8B
  uint32_t id;   // pouzito 24b, horni byte je nepouzit
  uint32_t time; // cas zacatku zavodu v sekundach od 1.1.2020
};

class Store {
  public:
    static RTC_DATA_ATTR TConfig  config;     // nastaveni krabicky
    static RTC_DATA_ATTR TStation station;    // nastaveni krabicky
    static RTC_DATA_ATTR TWifi    wifi;       // nastaveni wifi
    static RTC_DATA_ATTR TServer  server;     // nastaveni serveru
    static RTC_DATA_ATTR TAuth    auth;       // pristupove udaje
    static RTC_DATA_ATTR TEvent   event;      // data zavodu
    static RTC_DATA_ATTR TStatus  status;     // nastaveni odesilani statusu

    static RTC_DATA_ATTR uint16_t addrBase;   // prvni adresa zaznamu aktualniho zavodu
    static RTC_DATA_ATTR uint16_t addrActual; // adresa pro zapis noveho zaznamu
    static RTC_DATA_ATTR uint16_t addrToSend; // adresa prvniho neposlaneho zaznamu
    static RTC_DATA_ATTR uint16_t addrConfirm; // adresa prvniho nepotvrzeneto zaznamu v eeprom

    static RTC_DATA_ATTR uint32_t lastPunchTime; // cas posledniho cteni RFID
    static RTC_DATA_ATTR uint8_t  lastPunchType; // typ posledniho cteni RFID

    static void   begin();
    static void   init(); 
    static bool   readConfig();
    static void   writeConfig(TConfig* s);
    static void   writeTimeZone(int8_t t);
    static void   writePin(uint16_t pin);
    static void   writeStation(TStation* s);
    static void   writeSn(uint16_t sn);
    static void   writeWifi(TWifi* w);
    static void   writeServer(TServer* server);
    static void   writeAuth(TAuth* a);
    static void   writeEvent(uint32_t idEvent, uint32_t timeEvent);
    static void   writeStatus(TStatus* s);

    static bool     isData();
    static bool     nextData(uint16_t* addr, TData* data);
    static bool     confirmData(uint16_t addr, uint16_t count = 1);
    static void     process();
    static uint8_t  antRecord(uint16_t addr, char* data, char ident, bool);
    static uint16_t antData(char* data, uint16_t, uint16_t, bool);

    static void   clearEEPROM();
    static void   readEEPROM(uint16_t addr, uint8_t *data, uint8_t count);
    static void   writeEEPROM(uint16_t addr, uint8_t *data, uint8_t count);
    static void   setBatteryCalibration(uint32_t);
    static TData* readData(uint16_t, TData*, bool eeprom = 0);
    static bool   writeData(TData*);
    static TData* readDataEEPROM(uint16_t, TData*);
    static TData* readDataRAM(uint16_t, TData*);
    static void   writeDataRAM(uint16_t, TData*);
    static String dataToString(TData);
    static TData* setData(TData*, uint32_t, uint32_t, uint64_t, uint8_t, uint8_t);

    static void   setLastPunchTime(uint32_t, uint8_t);
  private:
    static uint8_t  dataRam[DATA_SIZE];
};


#endif

#ifndef RFID_H
#define RFID_H
 
#include <SPI.h>
#include <MFRC522.h>
#include "rtc.h"

#define RFID_SS   5
#define RFID_RST  4

#define BLOCK_OTP     3
#define BLOCK_ID      4
#define BLOCK_EVENT   5
#define BLOCK_TIME    6
#define BLOCK_BOTTOM  7
#define BLOCK_TOP     40

#define CONFIG_EVENT    5
#define CONFIG_STATION  7
#define CONFIG_SERVER   5
#define CONFIG_AUTH     5
#define CONFIG_STATUS   5
#define CONFIG_WIFI     5
#define CONFIG_FW       5
#define CONFIG_BLOCK5   5

// Verze zaznamu na cip (pro Forsta)
#define VERSION_CARD 2

// Typ cipu
#define COMMAND_ACTION      '!'
#define COMMAND_CONFIG      '&'
#define COMMAND_INFO        '?'

// Tajne funkce
#define COMMAND_BACKDOOR    '~'
#define COMMAND_CALIBRATION 'b'

// Subtyp cipu akce
#define SUBTYPE_UPLOAD      'U'
#define SUBTYPE_UPGRADE     '@'
#define SUBTYPE_ACKPIN      'P'
#define SUBTYPE_TIME        'T'

// Subtyp cipu konfigurace
#define SUBTYPE_EVENT       'E'
#define SUBTYPE_STATION     'B'
#define SUBTYPE_SN          'N'
#define SUBTYPE_WIFI        'W'
#define SUBTYPE_SERVER      'S'
#define SUBTYPE_AUTH        'A'
#define SUBTYPE_STATUS      'H'
#define SUBTYPE_SETPIN      'P'
#define SUBTYPE_CLEAR       'C'

/******************************************************************
 * 
 *  Struktura dat na cipu RFID - odpoved, start/cil
 *    40 bloku x 4B
 *  
 *  Blok        Obsah
 *  0, 1        Id cipu
 *  2           Id cipu, lock bits
 *  3 (OTP)     
 *  4           Id zavodnika (3B), odpoved (1B) - A, B, ...Z, S(start/cil)
 *  5           ID zavodu (3B), pocitadlo zapisu (1B)
 *  6           Cas startu zavodu v minutach od 1.1.2020 (3B), verze formatu cipu (1B)
 *  7 - 39      Zaznam razeni - 33 pozic
 *              relativni cas kontroly v sekundach od 0:00:00 (3B), Id krabicky (1B)
 * 
 ******************************************************************/

/******************************************************************
 * 
 *  Struktura dat na cipu RFID - konfigurace
 *    40 bloku x 4B
 *  
 *  Blok        Obsah
 *  0, 1        Id cipu
 *  2           Id cipu, lock bits
 *  3 (OTP)     
 *  4           ?PIN (2B), podtyp (1B), typ (1B)
 *  5           Data zavodu - ID zavodu (4B) 
 *  6           Data zavodu - cas zacatku zavodu v s od 1.1.2020 (3B), casovy posun vuci UTC (1B)
 *  7           Data krabicky, ID krabicky (2B), typ krabicky (1B - C ... kontrola, S .. start, F .. cil), SI kod (1B)
 *  8           Konfigurace pripojeni - IP adresa serveru (4B)
 *  9           Konfigurace pripojeni - port (2B), 
 *                zpusob odesilani (po kazdem razeni, v intervalu, na vyzadani) (1B), 
 *                typ odesilanych dat (Preo, Ant, ...) (1B) 
 *  10 - 23     Konfigurace pripojeni - host name (56B)
 *  24 - 39     Konfigurace pripojeni - URL (64B)   
 *  24          Wifi pripojeni - zakladni IP adresa krabicky [xx.xx.xx.xx] (4B) - nepouzito, pouziva DHCP
 *  25 - 39     Wifi pripojeni - ssid (30B), password (30B)
 * 
 ******************************************************************/

class RFID : public MFRC522 {
  public: 
    RFID(byte chipSelectPin, byte resetPowerDownPin);
    void  init();
    void  fastReset();
    bool  isNewCardPresent();  // Look for new cards
    bool  readCardSerial();    // Select one of the cards
    bool  readNewCard();   // kombinace isNewCardPresent, readCardSerial, readId
    void  powerDown(); // prechod do rezimu spanku
    void  powerUp();   // probuzeni z rezimu spanku
  
    bool  isAnswer();
    bool  isStartFinish();
    bool  isConfig();
    bool  isAction();
    bool  isCheck();
    bool  isConfigEvent();
    bool  isConfigStation();
    bool  isConfigWifi();
    bool  isConfigServer();
    bool  isConfigAuth();
    bool  isConfigStatus();
    bool  isClockNTP();
    bool  isChangePin();
    bool  isChangeSn();
    bool  isUpload();
    bool  isOTA();
    bool  isSuperClear();
    bool  isBatteryCalibration();
    bool  isBackDoor();
    bool  isACK();

    bool     readBlock(uint8_t addr, uint8_t* buffer);
    bool     readId();
    bool     readAll(uint8_t* buffer);
    bool     readBlocks(uint8_t* buffer, uint8_t start = 0, uint8_t count = 40);
    bool     readConfig(void* data, uint8_t start, uint8_t count);
    bool     writeBlock(uint8_t addr, uint8_t* buffer);
    bool     clearData();
    bool     writeVerifyBlock(uint8_t addr, uint8_t* buffer);
    bool     verifyRepaire();
    int8_t   writePunch(uint32_t time, uint8_t idStation, bool error);
    uint64_t getIdCard();
    uint8_t  getIdCard(uint8_t i);
    uint32_t getIdCompetitor();
    uint8_t  getAnswerCommand();
    uint32_t getIdEvent();
    int16_t  getTime(uint32_t time);
    uint8_t  getIdStartFinish();
    bool     isTimeActive();
    uint16_t getPin();
    uint8_t  getSubType();
    bool     oldCard();
  private:
    bool     idReady;
    uint64_t idCard;

    // format dat na cipu
    union {
      union {
        uint32_t idCompetitor;
        struct {
          uint8_t blank[3];
          uint8_t answer;
        };
      };
      struct {
        uint16_t pin;
        uint8_t  subtype;
        uint8_t  command;
      };
    };

    uint32_t idEvent;
    uint8_t  counter; // rucne se prepisuje do 4. B idEvent
    uint8_t  lastStartFinish; // ID posledniho startu/cile razeneho do cipu S
};

extern RFID rfid;

#endif
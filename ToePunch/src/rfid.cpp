#include "rfid.h"

RFID rfid(RFID_SS, RFID_RST);  // Create MFRC522 instance

RFID::RFID(byte chipSelectPin, byte resetPowerDownPin) : MFRC522(chipSelectPin, resetPowerDownPin) {}

void printBlock(uint8_t addr, uint8_t buffer[])
{
  Serial.printf("%02d: 0x%02X 0x%02X 0x%02X 0x%02X\n\r", addr, buffer[0], buffer[1], buffer[2], buffer[3]);
}

void RFID::fastReset()
{
	pinMode(_chipSelectPin, OUTPUT);
	digitalWrite(_chipSelectPin, HIGH);
	
	
  pinMode(_resetPowerDownPin, OUTPUT);		// Now set the resetPowerDownPin as digital output.
  digitalWrite(_resetPowerDownPin, LOW);		// Make sure we have a clean LOW state.
  delayMicroseconds(2);				// 8.8.1 Reset timing requirements says about 100ns. Let us be generous: 2μsl
  digitalWrite(_resetPowerDownPin, HIGH);		// Exit power down mode. This triggers a hard reset.
  // Section 8.8.2 in the datasheet says the oscillator start-up time is the start up time of the crystal + 37,74μs. Let us be generous: 50ms.
  delay(5);

//	if (!hardReset) { // Perform a soft reset if we haven't triggered a hard reset above.
//		PCD_Reset();
//	}
	
	// Reset baud rates
	PCD_WriteRegister(TxModeReg, 0x00);
	PCD_WriteRegister(RxModeReg, 0x00);
	// Reset ModWidthReg
	PCD_WriteRegister(ModWidthReg, 0x26);

	// When communicating with a PICC we need a timeout if something goes wrong.
	// f_timer = 13.56 MHz / (2*TPreScaler+1) where TPreScaler = [TPrescaler_Hi:TPrescaler_Lo].
	// TPrescaler_Hi are the four low bits in TModeReg. TPrescaler_Lo is TPrescalerReg.
	PCD_WriteRegister(TModeReg, 0x80);			// TAuto=1; timer starts automatically at the end of the transmission in all communication modes at all speeds
	PCD_WriteRegister(TPrescalerReg, 0xA9);		// TPreScaler = TModeReg[3..0]:TPrescalerReg, ie 0x0A9 = 169 => f_timer=40kHz, ie a timer period of 25μs.
	PCD_WriteRegister(TReloadRegH, 0x03);		// Reload timer with 0x3E8 = 1000, ie 25ms before timeout.
	PCD_WriteRegister(TReloadRegL, 0xE8);
	
	PCD_WriteRegister(TxASKReg, 0x40);		// Default 0x00. Force a 100 % ASK modulation independent of the ModGsPReg register setting
	PCD_WriteRegister(ModeReg, 0x3D);		// Default 0x3F. Set the preset value for the CRC coprocessor for the CalcCRC command to 0x6363 (ISO 14443-3 part 6.2.4)
	PCD_AntennaOn();						// Enable the antenna driver pins TX1 and TX2 (they were disabled by the reset)
}

void RFID::init() {
  SPI.begin(); 
  PCD_Init(); 
//  fastReset();
//	PCD_WriteRegister(TPrescalerReg, 0x43);		// 10μs.
//	PCD_WriteRegister(TReloadRegH, 0x00);		// Reload timer with 0x01E = 30, ie 0.3ms before timeout.
//	PCD_WriteRegister(TReloadRegL, 0x1E);

  delay(1);
}

bool RFID::readNewCard() {
  if (rfid.isNewCardPresent()) {  
    // Select one of the cards
    if (rfid.readCardSerial()) {
      // Read data ***************************************************
      Serial.println(F("Reading RFID ... "));
      if (!rfid.readId()) {
        Serial.println(F("ReadID failed!"));
        return 0;
      }
      return 1;
    }
  }
  return 0;
}

// Detekce pritomnosti noveho cipu
bool RFID::isNewCardPresent() {
  return PICC_IsNewCardPresent();
};

// Vyber cipu (aktivace)
bool RFID::readCardSerial() {
  idReady = 0;
  return PICC_ReadCardSerial();
};

void RFID::powerDown() {
	PCD_WriteRegister(CommandReg, PCD_NoCmdChange | 0x10);
}

void RFID::powerUp() {
	PCD_WriteRegister(CommandReg, PCD_NoCmdChange);
	delay(1);
}

bool RFID::isAnswer() {
  return command == 'Z' || (command >= 'A' && command <= 'E');
}

bool RFID::isStartFinish() {
  return command == 'S';
}

bool RFID::isConfig() {
  return command == COMMAND_CONFIG;
}

bool RFID::isAction() {
  return command == COMMAND_ACTION;
}

bool RFID::isConfigEvent() {
  return subtype == SUBTYPE_EVENT;
}

bool RFID::isConfigStation() {
  return subtype == SUBTYPE_STATION;
}

bool RFID::isConfigWifi() {
  return subtype == SUBTYPE_WIFI;
}

bool RFID::isConfigServer() {
  return subtype == SUBTYPE_SERVER;
}

bool RFID::isConfigAuth() {
  return subtype == SUBTYPE_AUTH;
}

bool RFID::isConfigStatus() {
  return subtype == SUBTYPE_STATUS;
}

bool RFID::isClockNTP() {
  return subtype == SUBTYPE_TIME;
}

bool RFID::isChangePin() {
  return subtype == SUBTYPE_SETPIN;
}

bool RFID::isChangeSn() {
  return subtype == SUBTYPE_SN;
}

bool RFID::isCheck() {
  return command == COMMAND_INFO;
}

bool RFID::isBatteryCalibration() {
  return command == COMMAND_CALIBRATION;
}

bool RFID::isBackDoor() {
  return command == COMMAND_BACKDOOR;
}

bool RFID::isUpload() {
  return subtype == SUBTYPE_UPLOAD;
}

bool RFID::isOTA() {
  return subtype == SUBTYPE_UPGRADE;
}

bool RFID::isSuperClear() {
  return subtype == SUBTYPE_CLEAR;
}

bool RFID::isACK() {
  return subtype == SUBTYPE_ACKPIN;
}

// Precteni jednoho 4B bloku z cipu  
bool RFID::readBlock(uint8_t addr, uint8_t* buffer) {
  uint8_t tmpBuffer[18];
  uint8_t size = sizeof(tmpBuffer);
  StatusCode status = (MFRC522::StatusCode) MIFARE_Read(addr, tmpBuffer, &size);
  if (status == MFRC522::STATUS_OK) {
    memcpy(buffer, tmpBuffer, 4);
    return 1;
  } else {
//    Serial.println(GetStatusCodeName(status));
    return 0;
  }  
}

// Cteni identifikace cipu, zavodnika, odpovedi/prikazu z cipu
bool RFID::readId() {
  uint8_t tmpBuffer[18];
  uint8_t size = sizeof(tmpBuffer);
  idReady = 0;
  StatusCode status = (MFRC522::StatusCode) MIFARE_Read(0, tmpBuffer, &size);
  if (status != MFRC522::STATUS_OK) {
    return 0;
  }  
  memcpy(&idCard, tmpBuffer, 8);
  status = (MFRC522::StatusCode) MIFARE_Read(BLOCK_ID, tmpBuffer, &size);
  if (status != MFRC522::STATUS_OK) {
    return 0;
  }
  idCompetitor = 0; // nulovani 4. B 
  memcpy(&idCompetitor, &tmpBuffer[0], 3);
  subtype = tmpBuffer[2];
  command = tmpBuffer[3];
  idEvent = 0; // nulovani 4. B
  memcpy(&idEvent, &tmpBuffer[4], 3);  
  counter = tmpBuffer[7];
  idReady = 1;
  return 1;
}  

// cteni celeho obsahu pameti cipu
bool RFID::readAll(uint8_t* buffer) {
  uint8_t tmpBuffer[18];
  uint8_t size = sizeof(tmpBuffer);
  StatusCode status;
  for (int i = 0; i < 40; i += 4) {
    status = (MFRC522::StatusCode) MIFARE_Read(i, tmpBuffer, &size);
    if (status != MFRC522::STATUS_OK) {
      return 0;
    }  
    memcpy(&buffer[i*4], tmpBuffer, 16);
  }
  return 1;
}

// Cteni bloku pameti cipu
bool RFID::readBlocks(uint8_t* buffer, uint8_t start, uint8_t count) {
  uint8_t tmpBuffer[18];
  uint8_t size = sizeof(tmpBuffer);
  StatusCode status;
  if (count == 0) return 0;
  for (int i = start; i < start + count; i += 4) {
    status = (MFRC522::StatusCode) MIFARE_Read(i, tmpBuffer, &size);
    if (status != MFRC522::STATUS_OK) {
      return 0;
    }  
    memcpy(&buffer[(i-start)*4], tmpBuffer, count-(i-start) < 4 ? (count-(i-start)) * 4 : 16);
  }
  return 1;
}

// Cteni configurace z cipu
bool RFID::readConfig(void* data, uint8_t start, uint8_t count) {
  return readBlocks((uint8_t*)data, start, count / 4);
}

// Zapis jednoho 4B bloku do cipu
bool RFID::writeBlock(uint8_t addr, uint8_t* buffer) {
  StatusCode status = (MFRC522::StatusCode) MIFARE_Ultralight_Write(addr, buffer, 4);
//  Serial.println(GetStatusCodeName(status));
  return (status == MFRC522::STATUS_OK);
}

// Nulovani celeho bloku data na zavodnim cipu
bool RFID::clearData() {
  uint32_t tmp = 0;
  bool status = 1;
  PCD_SetToReadWrite(1); // nastavit ctecku na zapis
  for (uint8_t i = BLOCK_BOTTOM; i < BLOCK_TOP; i++) {
    status &= writeBlock(i, (uint8_t*)&tmp);
//    Serial.printf("Clear block: %d status %d\n\r", i, status);
  }
  return status;
}

// Zapis jednoho 4B bloku do cipu
bool RFID::writeVerifyBlock(uint8_t addr, uint8_t* buffer) {
  bool result = writeBlock(addr, buffer);
//  Serial.printf("%d : %d\r\n", addr, *(uint32_t*)buffer);
  // Bude nutne doplnit cekani na zapis, pri vypisu na Serial funguje, jinak s ?
//  delay(4);
  if (result)  {
    uint8_t tmp[4];
    result = readBlock(addr, tmp);
    if (result) {
//      result = (*((uint32_t*)buffer) == *((uint32_t*)tmp));
      result = memcmp(buffer, tmp, 4) == 0;
    }
  }
  return result;
}

// Zaznam razeni do cipu
// zapise cas razeni (0 - 23:59:59) a ID krabicky
int8_t RFID::writePunch(uint32_t time, uint8_t idStation, bool error) {
  // CHECK krabicka ma idStation = 0
  uint8_t buffer[4];

  if (!idReady) return 0;

  if ((counter >= BLOCK_TOP - BLOCK_BOTTOM) && (idStation != 0) && rfid.getIdCompetitor() != 999999) {
    Serial.println("Card is full");
    return -1; // plny cip
  }

  PCD_SetToReadWrite(1); // nastavit ctecku na zapis
  if (idStation != 0) { // Krabicka START, FINISH, ANSWER
    time = (time % (24 * 3600)) & 0x0001FFFF;   // cas 17b
    memcpy(&buffer[0], &time, 3);
    buffer[2] |= error ? 0x80 : 0;
    buffer[3]  = idStation;
    // cip s IdCompetitor == 9999999 se prepisuje stale dokola
    if (rfid.getIdCompetitor() == 999999) counter = counter % (BLOCK_TOP - BLOCK_BOTTOM);
    if (!writeVerifyBlock(counter + BLOCK_BOTTOM, buffer)) {
      Serial.println("Write error 1");
      return -2;
    }
    counter++;
  } else { // Krabicka CHECK/CLEAR nezapisuje zaznam
    uint32_t t = Store::event.time / 60;
    memcpy(&buffer[0], &t, 3);
    buffer[3]  = VERSION_CARD;
    if (!writeVerifyBlock(BLOCK_TIME, buffer)) {
      Serial.println("Write error 1");
      return -2;
    }
    counter = 0;
    idEvent = Store::event.id;
  }
  
  memcpy(&buffer[0], &idEvent, 3);
  memcpy(&buffer[3], &counter, 1);
  if (!writeVerifyBlock(BLOCK_EVENT, buffer)) {
    Serial.println("Write error 2");
    return -3;
  }
  return counter; 
}

// Kontrola, resp. opraveni dat na zavodnickem cipu po nezdarenem zapisu
bool RFID::verifyRepaire() {
  if (getIdEvent() != 0) {
    return 1;
  }
  // Problem s cipem, nedokonceny zapis, pokusit se obnovit
  // Nalezeni posledniho zaznamu
  const uint8_t count = BLOCK_TOP - BLOCK_BOTTOM;
  uint32_t tmp[count];
  readBlocks((uint8_t*)tmp, BLOCK_BOTTOM, count);
  counter = 0;
  while (tmp[counter] != 0 && counter < count) {
//    Serial.printf("Index: %d time: %d\r\n", counter, tmp[counter]);
    counter++;
  }
  // Zapis bloku 5
  uint8_t buffer[4];
  memcpy(&idEvent, &Store::event.id, 3);
  memcpy(&buffer[0], &idEvent, 3);
  // Posledni zaznam ignorovat
  if (counter > 0) counter--;
  buffer[3] = counter ;
  Serial.printf("Oprava cipu ID event: %d, nalezeno %d zaznamu\r\n", Store::event.id, counter);
  PCD_SetToReadWrite(1); // nastavit ctecku na zapis
  return writeVerifyBlock(BLOCK_EVENT, buffer);
}

// Vraci ID cipu (musi byta nacteno pomoci readId())
uint64_t RFID::getIdCard() {
  return idReady ? idCard : 0;
}

uint8_t RFID::getIdCard(uint8_t i) {
  return idReady ? ((uint8_t*)&idCard)[i] : 0;
}

// Vraci ID zavodnika (musi byta nacteno pomoci readId())
uint32_t RFID::getIdCompetitor() {
  return idReady ? (idCompetitor & 0x00FFFFFF) : 0;
}

// Vraci odpoved/prikaz (musi byta nacteno pomoci readId())
uint8_t  RFID::getAnswerCommand() {
  return idReady ? command : 0;
}

uint32_t RFID::getIdEvent() {
  return idReady ? idEvent : 0;  
}

uint16_t RFID::getPin() {
  return idReady ? pin : 0;
}
 
uint8_t  RFID::getSubType() {
  uint8_t c = 0;
  return idReady ? subtype : c;
}

struct TPunch {
  union {
    uint32_t time;
    struct {
      uint8_t dummy[3];
      uint8_t idStation;
    };
  };
};

// Toto je podezrele, i normalni kontrole to nastavi priznak Start/Cil???
//#define STATION_ID  (Store::station.id | ((Store::station.type == TStationType::FINISH) ? 0xC0 : 0x80))
#define STATION_ID  (Store::station.id | ((Store::station.type == TStationType::FINISH) ? 0xC0 : ((Store::station.type == TStationType::START) ? 0x80 : 0x00)))

// Vypocita aktualni cas na trati z razeni start/cil
int16_t RFID::getTime(uint32_t time) {
  TPunch punch[34];
  int32_t allTime = 0;
  uint8_t last = counter-1;
  bool startFinish = 0;

  lastStartFinish = 0; // Posledni ID razeni ruzne od aktualniho

  if (readBlocks((uint8_t*)&punch, BLOCK_BOTTOM, counter)) {
//    Serial.printf("Last: %d ID: %d PunchTime: %d\r\n", last, punch[last].id, punch[last].time);
//  Serial.printf("STATION_ID: %d\n", STATION_ID);
//    if (STATION_ID != punch[last].idStation && (punch[last].idStation & 0x80) == 0x80) {
//      lastStartFinish = punch[last].idStation;
//    }
    // POZOR nutno upravit, chyba pokud by posledni razeni byla casovka - nemelo by nastat
    if ((punch[last].idStation & 0xC0) == 0x80) { // posledni razeni je start, tak bereme jako konec aktualni cas
      allTime = time % (24 * 3600);
    }
//    Serial.printf("Last: %d AllTime: %d Time: %d S/F: %d\r\n", last, allTime, time, startFinish);
    for (uint8_t i = 0; i <= last; i++) {
      if (STATION_ID != punch[i].idStation && (punch[i].idStation & 0x80) == 0x80) {
        lastStartFinish = punch[i].idStation; // uschova posledniho startu
      }
      if (startFinish == 0 && (punch[i].idStation & 0xC0) == 0x80) { // Razeni startu a prechozi byl cil
        allTime -= punch[i].time & 0x0001FFFF;
        startFinish = 1; // priznak, ze posledni razeni je start, cas bezi
      }
      if (startFinish == 1 && (punch[i].idStation & 0xC0) == 0xC0) { // Razeni cile a prechozi byl start
        allTime += punch[i].time & 0x0001FFFF;
        startFinish = 0; // priznak, ze posledni razeni je cil, nebezi cas
      }
//      Serial.printf("Index: %d ID: %d PunchTime: %d\r\n", i, punch[i].id, punch[i].time);
//      Serial.printf("Index: %d AllTime: %d Time: %d S/F: %d\r\n", i, allTime, time, startFinish);
    }
    return allTime;
  } else {
    return 0;
  }
}

uint8_t  RFID::getIdStartFinish() {
  return lastStartFinish; 
}

bool RFID::isTimeActive() {
  return (lastStartFinish != 0 && (lastStartFinish & 0xC0) == 0x80); // Je alespon jedno razeni a je to start
}

bool RFID::oldCard() {
  uint8_t p = blank[2];
  if ((p == 'S' || p == 'Z' || (p >= 'A' && p <= 'E')) && answer != '&') {
    blank[2] = 0;
    answer = p;
    PCD_SetToReadWrite(1); // nastavit ctecku na zapis
    writeBlock(BLOCK_ID, (uint8_t*)&idCompetitor);
    return 1;
  }
  return 0;
}


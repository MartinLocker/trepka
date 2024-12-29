#include "info.h"

#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"` 

SSD1306Wire  display(0x3c, 21, 22, GEOMETRY_128_64); // SDA, SCL

Info info;

Info::Info() {
  pinMode(LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  buffer1[15] = 0;
  buffer2[15] = 0;
}

char* Info::format(char* s, uint8_t id, uint8_t t) {
  switch (t) {
    case TStationType::START:   sprintf(s, "S%d", id); break;
    case TStationType::FINISH:  sprintf(s, "F%d", id); break;
    case TStationType::CLEAR:   sprintf(s, "X%d", id); break;
    case TStationType::CONTROL: sprintf(s, "%d",  id); break;
    default: sprintf(s, "%s", "xxx"); break;
  }
  return s;
}    

char* Info::ipToString(char* s, IPAddress ip) {
  sprintf(s, "%03d.%03d.%03d.%03d", ip[0], ip[1], ip[2], ip[3]);
  return s;
}


void Info::init() {
  display.init();
  display.flipScreenVertically();
}

void Info::beep(uint16_t x) {
  digitalWrite(LED, x);
  digitalWrite(BUZZER, x);
  if (x > 1) {
    delay(x);
    digitalWrite(LED, 0);
    digitalWrite(BUZZER, 0);
  }
}

void Info::start() {
  time = millis() - TIME_BEEP;
  counter = TIMES_BEEP * 2;  
}

bool Info::isActive() {
  return counter > 0;
}

void Info::displayOff() {
  display.clear();
  display.display();  
  displayState = IDLE;
  asyncEnabled = 0;
}

void Info::showAsync(const char* text1, const char* text2, const char* text3, uint16_t timeOut) {
  asyncData = 1;
  asyncTimeOut = timeOut;
  memcpy(buffer1, text1, 15);
  memcpy(buffer2, text2, 15);
  memcpy(buffer3, text3, 15);
// Proc jsem to sem dal??? Problem, kdyz se nespoji se serverem, tak zustane po razeni nesmazany displej
  if (displayState != ON) displayState = WAIT; 
}

void Info::showAsync(const char* text1, const char* text2, uint16_t timeOut) {
  showAsync(text1, text2, "", timeOut);
}

void Info::enableAsync() {
  asyncEnabled = 1;
}

void Info::process() {
  if (asyncEnabled && asyncData) {
    if (buffer3[0] != 0) {
      info.show(buffer1, buffer2, buffer3, asyncTimeOut);
    } else {
      info.show(buffer1, buffer2, asyncTimeOut);
    }
    asyncData = 0;
  }

  bool x = !(counter & 0x01);
  uint32_t timeOut = x ? TIME_NOBEEP : TIME_BEEP;
  if (counter > 0 && millis() - time >= timeOut) {
    digitalWrite(LED, x);
    digitalWrite(BUZZER, x);
    time += timeOut;
    counter--;
    if (counter == 0)  {
      displayState = ON;
      displayStart = millis();
      displayTime = 1000;
    }
  }

  if (displayState == ON && millis() - displayStart > displayTime) {
    displayState = OFF;
  }

  if (displayState == OFF) {
    displayOff();
  }
}

void Info::showLogo(const char* id, const char* ver, uint16_t timeOut) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_30);  
  display.drawXbm(4, 0, logoWidth, logoHeight, logo); // 
  display.drawString(100, 6, id);
  display.setFont(ArialMT_Plain_16);  
  display.drawString(100, 46, ver);
  display.display();  

  displayState = timeOut == 0 ? PERMANENT : ON;
  displayStart = millis();
  displayTime = timeOut;
}

void Info::show(uint32_t idCompetitor, const char* str, uint16_t timeOut) {
  char buffer[32];  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);  
  sprintf(buffer, "%d", idCompetitor);
  display.drawString(63, 4, buffer);
  if (strlen(str) < 5) {
    display.setFont(ArialMT_Plain_30);  
  }
  display.drawString(63, 34, str);
  display.display();  
  displayState = timeOut == 0 ? PERMANENT : ON;
  displayStart = millis();
  displayTime = timeOut;
}

void Info::showStartFinish(uint32_t idCompetitor, const char* str, uint16_t time) {
  char buffer[32];  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_20);  
  sprintf(buffer, "%d %s", idCompetitor, str);
  display.drawString(63, 6, buffer);
  sprintf(buffer, "%d:%02d", time / 60, time % 60);
  display.drawString(63, 32, buffer);
  display.display();  
  displayState = PERMANENT;
}

void Info::showStartFinish(uint32_t idCompetitor, uint8_t id, uint8_t type, uint16_t time) {
  char tmp[16];  
  format(tmp, id, type);
  showStartFinish(idCompetitor, tmp, time);
}

void Info::show(uint32_t idCompetitor, uint8_t answer, uint16_t timeOut) {
  char buffer[32];  
  sprintf(buffer, "%c", answer);
  show(idCompetitor, buffer, timeOut);
}

//void Info::show(uint32_t idCompetitor, uint16_t time, uint16_t timeOut) {
void Info::show(uint32_t idCompetitor, int16_t time, uint16_t timeOut) {
  char buffer[32];  
  sprintf(buffer, "%d:%02d", time / 60, (time >= 0) ? (time % 60) : -(time % 60));
  show(idCompetitor, buffer, timeOut);
}

void Info::show(const char* str, uint32_t time, uint16_t timeOut) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_20);  
  display.drawString(64, 1, str);
  char t1[] = "DD.MM.YY";
  display.drawString(64, 23, DateTime(time+SECONDS_FROM_1970_TO_2000+SECONDS_FROM_2000_TO_2020).toString(t1));
  char t2[] = "hh:mm:ss";
  display.drawString(64, 45, DateTime(time+SECONDS_FROM_1970_TO_2000+SECONDS_FROM_2000_TO_2020).toString(t2));
  display.display();  
  displayState = timeOut == 0 ? PERMANENT : ON;
  displayStart = millis();
  displayTime = timeOut;
}

void Info::show(const char* txt, const char* str, uint16_t timeOut) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_20);  
  display.drawString(64, 10, txt);
  if (strlen(str) > 10) {
    display.setFont(ArialMT_Plain_16);  
  }
  display.drawString(64, 36, str);
  display.display();  
  displayState = timeOut == 0 ? PERMANENT : ON;
  displayStart = millis();
  displayTime = timeOut;
}

void Info::show(const char* txt, const char* str1, const char* str2, uint16_t timeOut) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_20);  
  display.drawString(64, 4, txt);
  display.setFont(ArialMT_Plain_16);  
  display.drawString(64, 28, str1);
  display.drawString(64, 46, str2);
  display.display();  
  displayState = timeOut == 0 ? PERMANENT : ON;
  displayStart = millis();
  displayTime = timeOut;
}

void Info::show(uint16_t idStation, uint8_t si, uint8_t type, uint32_t idEvent, uint32_t time, uint16_t timeOut) {
  char buffer[32];  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);  
  sprintf(buffer, "Station: #%d", idStation);
  display.drawString(0, 1, buffer);
//  sprintf(buffer, "%s (%d)", type == TStationType::FINISH ? "FINISH" : (type == TStationType::START ? "START" : "CONTROL"), si);
  sprintf(buffer, "%s (%d)", stationName(type), si);
  display.drawString(0, 17, buffer);
  sprintf(buffer, "Race: %d", idEvent);
  display.drawString(0, 33, buffer);
  char tmp[] = "DD.MM.YYYY hh:mm";
  sprintf(buffer, "%s", DateTime(time+SECONDS_FROM_1970_TO_2000+SECONDS_FROM_2000_TO_2020).toString(tmp));
  display.drawString(0, 49, buffer);
  display.display();  
  displayState = timeOut == 0 ? PERMANENT : ON;
  displayStart = millis();
  displayTime = timeOut;
}

void Info::show(uint32_t idEvent, uint32_t time, const char* ssid, const char* hostName, uint16_t timeOut) {
  char buffer[64];  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);  
  sprintf(buffer, "Race: %d", idEvent);
  display.drawString(0, 1, buffer);
  char date[] = "DD.MM.YYYY hh:mm";
  sprintf(buffer, "%s", DateTime(time+SECONDS_FROM_1970_TO_2000+SECONDS_FROM_2000_TO_2020).toString(date));
  display.drawString(0, 17, buffer);
  char tmp[16];
  tmp[15] = 0;
  memcpy(tmp, ssid, 15);
  sprintf(buffer, "SSID: %s", tmp);
  display.drawString(0, 33, buffer);
  memcpy(tmp, hostName, 15);
  sprintf(buffer, "Host: %s", tmp);
  display.drawString(0, 49, buffer);
  display.display();  
  displayState = timeOut == 0 ? PERMANENT : ON;
  displayStart = millis();
  displayTime = timeOut;
}

void Info::showConfig(const char* key, const char* value, const char* key2, const char* value2) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);  
  display.drawString(0, 1, key);
  char tmp[16];
  tmp[15] = 0;
  memcpy(tmp, value, 15);
  display.drawString(0, 17, tmp);
  if (key2 && value2) {
    display.drawString(0, 33, key2);
    memcpy(tmp, value2, 15);
    display.drawString(0, 49, tmp);
  }
  display.display();  
  displayState = PERMANENT;
}

void Info::showServer(IPAddress ip, uint16_t port, uint8_t upload, uint8_t code) {
  char buffer[32];  
  const char* codeName[] = {"Preo", "ANT", "Raw"};
  const char* uploadName[] = {"On punch", "On demand"};
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);  
//  display.drawString(0, 1, "Server:");
  sprintf(buffer, "%03d.%03d.%03d.%03d", ip[0], ip[1], ip[2], ip[3]);
  display.drawString(0, 1, buffer);
  sprintf(buffer, "Port: %d", port);
  display.drawString(0, 17, buffer);
  sprintf(buffer, "Code: %s", codeName[code]);
  display.drawString(0, 33, buffer);
  if (upload < 2)
    sprintf(buffer, "%s", uploadName[upload]);
  else
    sprintf(buffer, "Every %d min", upload);    
  display.drawString(0, 49, buffer);
  display.display();  
  displayState = PERMANENT;
}

void Info::showStatus(IPAddress ip, uint8_t type, char* url) {
  char buffer[32];  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);  
  sprintf(buffer, "%03d.%03d.%03d.%03d", ip[0], ip[1], ip[2], ip[3]);
  display.drawString(0, 1, buffer);
  if (type != 0 && type != 255)
    sprintf(buffer, "Every %d min", type);    
  else
    strcpy(buffer, "Disabled");
  display.drawString(0, 17, buffer);
  display.drawString(0, 33, "Status URL:");
  buffer[15] = 0;
  memcpy(buffer, url, 15);
  display.drawString(0, 49, buffer);
  display.display();  
  displayState = PERMANENT;
}


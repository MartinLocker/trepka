#ifndef INFO_H
#define INFO_H

#include <Arduino.h>
#include <WiFi.h>
#include "data.h"
#include "rtc.h"
#include "logo.h"

#define LED     BUILTIN_LED
#define BUZZER  17  /* TX2 */

#define TIME_BEEP   250
#define TIME_NOBEEP 250
#define TIMES_BEEP  1

class Info {
  public:
    enum DisplayState {
      IDLE,
      ON,
      OFF,
      PERMANENT,
      WAIT
    };
    Info();
    void init();
    char* format(char*, uint8_t, uint8_t);
    char* ipToString(char*, IPAddress);
    void beep(uint16_t);
    void start();
    void process();
    bool isActive();
    void enableAsync();
    void showAsync(const char*, const char*, uint16_t timeOut = 0);
    void showAsync(const char*, const char*, const char*, uint16_t timeOut = 0);
    void showLogo(const char*, const char*, uint16_t timeOut = 0);
    void show(const char*, const char*, uint16_t timeOut = 0);
    void show(const char*, const char*, const char*, uint16_t timeOut = 0);
    void show(uint32_t, uint8_t, uint16_t timeOut = 0);
    void show(uint32_t, int16_t, uint16_t timeOut = 0);
//    void show(uint32_t, uint16_t, uint16_t timeOut = 0);
    void show(uint32_t, const char*, uint16_t timeOut = 0);
    void show(const char*, uint32_t, uint16_t timeOut = 0);
    void show(uint32_t, uint32_t, const char*, const char*, uint16_t timeOut = 0);
    void show(uint16_t, uint8_t, uint8_t, uint32_t, uint32_t, uint16_t timeOut = 0);
    void showStartFinish(uint32_t, const char*, uint16_t);
    void showStartFinish(uint32_t, uint8_t, uint8_t, uint16_t);
    void showConfig(const char*, const char*, const char* = NULL, const char* = NULL);
    void showServer(IPAddress, uint16_t, uint8_t, uint8_t);
    void showStatus(IPAddress, uint8_t, char*);
    void displayOff();

    DisplayState displayState = IDLE;
  private:
    uint32_t time;
    uint32_t displayStart;
    uint16_t displayTime;
    uint8_t  counter;
    uint8_t  asyncData;
    uint8_t  asyncEnabled;
    uint16_t asyncTimeOut;
    char     buffer1[16];
    char     buffer2[16];
    char     buffer3[16];
};

extern Info info;

#endif
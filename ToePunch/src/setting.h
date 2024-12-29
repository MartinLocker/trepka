#ifndef SETTING_H
#define SETTING_H

#include <Arduino.h>

#include "data.h"
#include "rfid.h"
#include "info.h"
#include "wifiComm.h"
#include "str.h"

#define SETTING_TIMEOUT 10000

class Setting {
  public:
    Setting();
    void start();
    void process();
    void confirm();
    bool waiting();
    void processUpload();
    void processOTA();
    void processTime();
    void processStatus();

  private:
    union {
      TEvent   event;
      TStation station;
      TWifi    wifi;
      TServer  server;
      TBlock   dataBlock;
      TTime    time;
      TAuth    auth;
      TStatus  status;
      uint32_t uploadType;
      char     upgradeServer[32];
    };
    uint8_t     command;
    uint8_t     subtype;
    uint32_t    timeout;
};

extern Setting setting;

#endif
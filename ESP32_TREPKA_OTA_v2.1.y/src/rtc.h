#ifndef RTC_H
#define RTC_H
 
#include <RTClib.h>
#include "data.h"
#include <sys/time.h>

#define SECONDS_FROM_2000_TO_2020 DateTime(2020, 1, 1, 0, 0, 0).secondstime()

class RTC : public RTC_DS3231 {
  public:
    RTC();
    void init();
    uint32_t getTime();
    void adjustTime(uint32_t, bool unix = 0);
    char* printTime(char* s, uint32_t time, bool unix = 0);
  private:
    uint32_t getRTCTime();  
};

extern RTC rtc;

#endif
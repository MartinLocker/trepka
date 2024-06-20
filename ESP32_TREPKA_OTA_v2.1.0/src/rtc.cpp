#include "rtc.h"

// Hodiny realneho casu UTC

RTC rtc;

RTC::RTC() : RTC_DS3231() {}

void RTC::init() {
  if (!begin()) {
    while (1);
  }
 
  if (lostPower()) {
    adjust(DateTime(2020, 1, 1, 0, 0, 0));
  }
  
  // nastaveni vnitrniho RTC dle externiho RTC
  uint32_t t = getRTCTime();
  struct timeval tset = {.tv_sec = t};
  settimeofday(&tset, NULL);
}

// Vraci cas v sekundach z interniho RTC vcetne casoveho pasma vuci od 1.1.2020 00:00:00 UTC
uint32_t RTC::getTime() {
  time_t t;
  time(&t);
  return (t + Store::config.timeZone*3600 - SECONDS_FROM_1970_TO_2000 - SECONDS_FROM_2000_TO_2020);
}

// Vraci cas v sekundach z externiho RTC bez casoveho pasma vuci od 1.1.2020 00:00:00 UTC
uint32_t RTC::getRTCTime() {
  return (now().secondstime() + SECONDS_FROM_1970_TO_2000);
}

void RTC::adjustTime(uint32_t t, bool unix) {
  t =  t + (unix ? 0 : SECONDS_FROM_1970_TO_2000 + SECONDS_FROM_2000_TO_2020 - Store::config.timeZone*3600);
  adjust(DateTime(t));
  struct timeval tset = {.tv_sec = t};
  settimeofday(&tset, NULL);  
}

char* RTC::printTime(char* s, uint32_t time, bool unix) {
  DateTime(time + (unix ? 0 : SECONDS_FROM_1970_TO_2000 + SECONDS_FROM_2000_TO_2020)).toString(s);
  return s;
}
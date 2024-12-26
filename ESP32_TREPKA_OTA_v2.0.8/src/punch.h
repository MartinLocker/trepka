#ifndef PUNCH_H
#define PUNCH_H

#include <Arduino.h>

#define PUNCH_TIMEOUT 2

struct TPunch {
  uint32_t time;
  uint32_t idCompetitor;
  uint8_t  answer;
  uint8_t  status;
};
 
class Punch {
  public:
    Punch();
    int16_t  getIndexPunch(uint16_t idCompetitor);
    bool     isPunched(uint32_t eventTime, uint32_t idCompetitor, uint8_t answer);
    uint16_t setPunch(uint32_t eventTime, uint32_t idCompetitor, uint8_t answer, uint8_t confirm = 0);
    void     confirmPunch();
    bool     isConfirmed();
  private:
    TPunch   record;
    uint16_t top;
    int16_t  index;
};

extern Punch punch;

#endif
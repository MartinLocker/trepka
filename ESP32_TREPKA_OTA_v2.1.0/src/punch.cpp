#include "punch.h"

Punch punch;

Punch::Punch() {
  memset(&record, 0, sizeof(TPunch));
}

bool Punch::isPunched(uint32_t eventTime, uint32_t idCompetitor, uint8_t answer) {
//  return ((record.answer == answer) && (eventTime - record.time < PUNCH_TIMEOUT));
  return (eventTime - record.time < PUNCH_TIMEOUT);
}

uint16_t Punch::setPunch(uint32_t eventTime, uint32_t idCompetitor, uint8_t answer, uint8_t confirm) {
  record.time = eventTime;
  record.idCompetitor = idCompetitor;
  record.answer = answer;
  record.status = confirm;
  return 1;
}

void Punch::confirmPunch() {
  record.status = 1;
}

bool Punch::isConfirmed() {
  return record.status == 1;
}


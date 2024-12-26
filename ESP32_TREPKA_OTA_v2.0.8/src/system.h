#ifndef SYSTEM_H
#define SYSTEM_H

#include <Arduino.h>
#include "driver/adc.h"
#include "data.h"

#define ADC 33

float getBattery();
void calibrationBattery();

#endif
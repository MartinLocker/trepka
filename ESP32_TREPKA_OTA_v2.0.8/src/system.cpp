#include "system.h"

#define ADC 33

float getBattery() {
  #define COUNT 4
  adc_power_on();
  analogRead(ADC); // prvni cteni je nanic
  uint32_t calibration = Store::config.battery; 
  if (calibration == 0xFFFFFFFF) calibration = 4095;
  // delic 2:1, referencni napeti 3,6V
  uint32_t voltage_raw = 0;
  for (uint8_t i = 0; i < COUNT; i++) {
    voltage_raw += analogRead(ADC); 
  }
  float voltage = voltage_raw * 7.2 / calibration / COUNT;
  adc_power_off();
  return voltage;
}

void calibrationBattery() {
  #define COUNT_RAW 8
  #define SET_VOLTAGE 3.75
//  #define SET_VOLTAGE 4.02
  adc_power_on();
  uint32_t voltage_raw = 0;
  analogRead(ADC); // prvni cteni je nanic
  for (uint8_t i = 0; i < COUNT_RAW; i++) {
    voltage_raw += analogRead(ADC); 
  }
  uint32_t x = (uint32_t)(voltage_raw * 7.2 / SET_VOLTAGE / COUNT_RAW + 0.5);
  adc_power_off();  

  Store::setBatteryCalibration(x);
}

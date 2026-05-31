#ifndef ADC_H
#define ADC_H

#define GRADIENT_SLOPE (90/(-4095))             // slope of linear function for converting ADC value to temperature
#define INTERCEPT 55                            // intercept of linear function.


#include <stdint.h>
#include "boardSupport.h"
#include "stm32f439xx.h"

float Convert_ADC_to_Temperature(float ADC_Value);
void ADC3_Setup(void);
void Read_Potentiometer(volatile uint16_t *ADC_Value, volatile float *t_value);

#endif

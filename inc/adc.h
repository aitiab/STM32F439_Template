#ifndef ADC_H
#define ADC_H

#define GRADIENT_SLOPE (90.0f/(-4095.0f))             // slope of linear function for converting ADC value to temperature. Add 0.f to cast as float literal otherwise will be assumed as int which will result in 0.
#define INTERCEPT 55.0f                            // intercept of linear function. Add 0.f to cast as float literal 


#include <stdint.h>
#include "boardSupport.h"
#include "stm32f439xx.h"

float Convert_ADC_to_Temperature(float ADC_Value);
void ADC3_Setup(void);
void Read_Potentiometer(volatile uint16_t *ADC_Value, volatile float *t_value);

#endif

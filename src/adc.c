#include "adc.h"

void Read_Potentiometer(volatile uint16_t *ADC_Value, volatile float *t_value)
{
	volatile uint16_t time_out = 1000;
	
	ADC3->CR2 |= ADC_CR2_SWSTART;
	
	while ((ADC3->SR & ADC_SR_EOC) == 0x00 && time_out > 0)   																		// Wait for end of conversion. Conversion time is 492 cycles
	{
		time_out--;
	}
	
	if (time_out > 0)																																							// If time_out is not 0 then ADC successfully converted potentiometer signal
	{
			*ADC_Value = (ADC3->DR & 0x0000FFFF);																	// Read the data. Also resets the SR_EOC
			*t_value = Convert_ADC_to_Temperature(*ADC_Value);
	}
}

//=====================================FUNCTION CONVERT ADC TO TEMPERATURE START================================================================//
// Function: Convert_ADC_to_Temperature
// Description: Maps ADC to temperature using linear function where 55C = 0 ADC Value and -30C = 4095 ADC Value
// Input: ADC Value
// Output: Corresponding temperature value in float?
float Convert_ADC_to_Temperature(float ADC_Value)
{
	// A linear relationship between ADC value between 0 and 4095 and temperature between 55C and -30C
	return (-0.02075702076 * ADC_Value) + 55;
}
//=====================================FUNCTION CONVERT ADC TO TEMPERATURE START================================================================//


// Semi-complete.
// Potentiometer is connected to ADC3_IN8. PF10
void ADC3_Setup(void)
{
	RCC->APB2ENR |= RCC_APB2ENR_ADC3EN;
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;
	
	RCC->APB2RSTR |= RCC_APB2RSTR_ADCRST;
	RCC->AHB1RSTR |= RCC_AHB1RSTR_GPIOFRST;
	
	__asm("nop");		__asm("nop");
	
	RCC->APB2RSTR &= ~(RCC_APB2RSTR_ADCRST);
	RCC->AHB1RSTR &= ~(RCC_AHB1RSTR_GPIOFRST);
	
	__asm("nop");		__asm("nop");
	
	GPIOF->MODER &= ~(GPIO_MODER_MODE10_Msk);
	GPIOF->MODER |= (0b11 << GPIO_MODER_MODE10_Pos);			// 0b11 sets GPIO Pin to analog mode
	GPIOF->PUPDR &= ~(GPIO_PUPDR_PUPD10_Msk);							// 0b00 sets PUPDR to no pull up or down
	
	// ADC3->CR1 |= (ADC_CR1_EOCIE_Msk); 				// 0b1 = Interrupt generated when an regular channel conversion is completed.
	ADC3->CR1 &= ~(ADC_CR1_SCAN_Msk);					// 0b0 = Turn off scan mode. // Single channel. No Scan. 
	ADC3->CR1 &= ~(ADC_CR1_RES_Msk); 					// 0b00 sets ADC at 12-bit resolution. 	4096 steps (0 - 4095)
	
	ADC3->CR2 &= ~(ADC_CR2_CONT_Msk);					// 0b0 sets ADC to run in single sample mode. 
	ADC3->CR2 &= ~(ADC_CR2_ALIGN_Msk);				// 0b0 sets data output to be right-aligned
	
	ADC3->SQR1 &= ~(ADC_SQR1_L_Msk); 					// 0b000 seets total number of conversions in regular channel conversion sequence to 1.
	
	ADC3->SQR3 &= ~(ADC_SQR3_SQ1_Msk);					// Clear channel 1st conversion in regular sequence
	ADC3->SQR3 |= (8 << ADC_SQR3_SQ1_Pos);		// Set first conversion in regular sequence as channel 8
	
	ADC3->SMPR2 |= (0b111 << ADC_SMPR2_SMP8_Pos);		// Cycles for conversion = 480 + 12 = 492 cycles per conversion. Frequency = 42MHz/492 cycles = 85365 Hz

	ADC3->CR2 |= (ADC_CR2_ADON_Msk);					// ADC On.
	ADC3->CR2 &= ~(ADC_CR2_SWSTART_Msk); 			// 0b0 = Disable conversion on the regular channels. Cleared by hardware when conversion starts. Needs ADON to be 1.
	
	
	// Timer to trigger conversion?
	// Sufficiently fast enough to detect changes in potentiometer
}
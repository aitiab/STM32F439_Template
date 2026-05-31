/********************************************
*			STM32F439 Main (C Header File)  			*
*			Developed for the STM32								*
*			Author: Dr. Glenn Matthews						*
*			Header File														*
********************************************/


// Compiler pragmas


#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include "boardSupport.h"
#include "stm32f439xx.h"
#include "control.h"

//================================ CALCULATION DEFINES START ================================//
// Below insert defines for calculations

/* Prescaler calculation. Need to achieve 0.25Hz
	 Source clock is 84MHz. Divide by 8400 provides count clock frequency of 10kHz
	 
	 MAX CNT possible is 2^{16}-1 = 65535. Therefore max time is ~6.5 seconds
*/
#define TIM6_PRESCALER (8399U)
#define EXPECTED_COUNT_CLOCK_TIM6 (84000000U /(TIM6_PRESCALER + 1)) // 10kHz count clock

#define TIM7_PRESCALER (39999)		// TIM7 Prescaler
#define EXPECTED_COUNT_CLOCK_TIM7 (84000000U / (TIM7_PRESCALER + 1))		// 2.1kHz count clock

#define TIM6_TARGET_FREQUENCY (0.25f)
#define TIM6_ONE_QUARTER_HZ_Count (uint16_t)((EXPECTED_COUNT_CLOCK_TIM6/TIM6_TARGET_FREQUENCY))							// Ticks required = Clock frequency / Target frequency

#define TIM7_TARGET_FREQUENCY (0.1f)			// 0.1Hz = 1 tick every 10 seconds
#define TIM7_TEN_SECOND_Count (uint16_t) (EXPECTED_COUNT_CLOCK_TIM7/TIM7_TARGET_FREQUENCY)												// Ticks required = clock frequency / Target frequency

//================================ CALCULATION DEFINES END ================================//

//================================ FLAGS DEFINES START ================================//
// Below are defines useful for setting the TIMERS Flag
// The TIMERS flag is used by TIM7 to determine which 'timers' are running using its 1 second tick.

#define TIMER_FAN_OFF_Pos (1U)
#define TIMER_FAN_OFF_Msk (1 << TIMER_FAN_OFF_Pos)							// 0b0010

#define TIMER_UART_CTRL_Pos (2U)
#define TIMER_UART_CTRL_Msk (1 << TIMER_UART_CTRL_Pos)					// 0b0100

//================================ FLAGS DEFINES END ================================//

#define TIM6_ENABLE   (TIM6->CR1 |= TIM_CR1_CEN_Msk)
#define TIM7_ENABLE   (TIM7->CR1 |= TIM_CR1_CEN_Msk)
#define TIM7_Disable  (TIM7->CR1 &= ~(TIM_CR1_CEN_Msk))





void TIM6_Setup(void);
void TIM7_Setup(void);




#endif
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
#include "switches.h"

//================================ CALCULATION DEFINES START ================================//
// Below insert defines for calculations

/* Prescaler calculation. Need to achieve 0.25Hz
	 Source clock is 84MHz. Divide by 8400 provides count clock frequency of 10kHz
	 
	 MAX CNT possible is 2^{16}-1 = 65535. Therefore max time is ~6.5 seconds
*/
#define TIM_PRESCALER (8399U)
#define EXPECTED_COUNT_CLOCK (84000000U /(TIM_PRESCALER + 1))

#define TIM6_TARGET_FREQUENCY (0.25f)
#define TIM6_ONE_QUARTER_HZ_Count (uint16_t)((EXPECTED_COUNT_CLOCK/TIM6_TARGET_FREQUENCY))							// Ticks required = Clock frequency / Target frequency

#define TIM7_TARGET_FREQUENCY (1U)
#define TIM7_ONE_HZ_Count (uint16_t) (EXPECTED_COUNT_CLOCK/TIM7_TARGET_FREQUENCY)												// Ticks required = clock frequency / Target frequency

//================================ CALCULATION DEFINES END ================================//

//================================ FLAGS DEFINES START ================================//
// Below are defines useful for setting the TIMERS Flag
// The TIMERS flag is used by TIM7 to determine which 'timers' are running using its 1 second tick.

#define TIMER_FAN_OFF_Pos (1U)
#define TIMER_FAN_OFF_Msk (1 << TIMER_FAN_OFF_Pos)							// 0b0010

#define TIMER_UART_CTRL_Pos (2U)
#define TIMER_UART_CTRL_Msk (1 << TIMER_UART_CTRL_Pos)					// 0b0100



//================================ FLAGS PACKET DEFINES END ================================//




// Struct for managing the counts for the different 'timers' that run using the 1 second tick of TIM7
typedef struct {
	uint8_t fan;							// Count for timer for Fan off
	uint8_t UART;							// Count for timer for UART
	uint8_t sw;								// Count for Timer for Switch
} TICK_TIMERS;







void TIM6_Setup(void);
void TIM7_Setup(void);




#endif
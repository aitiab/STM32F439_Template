
/********************************************
*			STM32F439 Main (C Startup File)  			*
*			Developed for the STM32								*
*			Author: Dr. Glenn Matthews						*
*			Source File														*
*     Updated: 04/03/2026 	  							*
********************************************/

#include <stdint.h>
#include "boardSupport.h"
#include "main.h"


// Global variables. Can be accessed without needing to explicitly pass into the function

volatile UART_TX uart_tx = {{0},0,0,0};
volatile UART_RX uart_rx = {{0},0,0, 0};

// Outputs: Heater, Cooling, Fan and Light
// Fan is inital set to 1 (on)
volatile Outputs outputs = {HEATER_INITAL_STATE, COOLING_INITAL_STATE, FAN_INITIAL_STATE, LIGHT_INITAL_STATE}; 				// Initialise Outputs struct. Set default state as off (0).
volatile Sensors sensors = {{0,0,{0}}, 0};				// Initialise Sensors struct. Set default state at 0. The inner {0,0,{0}} is for the temperature struct inside.
// volatile Switches switches = {0, 0}; 				// Initialise Switches struct. Set default state at off (0).

volatile uint8_t COMM_FLAG = 0;

volatile uint8_t CONTROL_MODE_FLAG = CONTROL_AUTO_MODE_Msk;				// Flags for determining control mode. initally set to AUTO MODE.
volatile uint8_t TIM7_RUNNING_FLAG = 0b0000;						// Flags for determining why timer TIM7 is running

//******************************************************************************//
// Function: main()
// Input : None
// Return : None
// Description : Entry point into the application.
// *****************************************************************************//
int main(void)
{
	// Bring up the GPIO for the power regulators.
	//boardSupport_init();
	
	__disable_irq();						// Disable global interrupts until everything is set up
	
	configureRCC();
	configureGPIO();
	
	ADC3_Setup();
	TIM6_Setup();
	TIM7_Setup();
	UART3_Configure();
	configureSysTick();

	// Set priorities of interrupt based on how often they occur.
	NVIC_SetPriority(TIM6_DAC_IRQn, 3);			// Set priority of TIM6 interrupt to 3 (higher than TIM7 as it happens more often 4 sec tick)
	NVIC_SetPriority(TIM7_IRQn, 4);				// Set priority of TIM7 interrupt to 4 (lower than TIM6 as it happens less often 10 sec tick)
	NVIC_SetPriority(USART3_IRQn, 2);			// Set priority of UART interrupt to 2 (higher than TIM6 and TIM7). Not sure why but sense it maybe more important.
	NVIC_SetPriority(SysTick_IRQn, 1);			// Set priority of SysTick interrupt to 1 (highest priority). Needed otherwise will miss the timing.

	NVIC_EnableIRQ(TIM6_DAC_IRQn);				// Enable TIM6 interrupt in NVIC
	NVIC_EnableIRQ(TIM7_IRQn);					// Enable TIM7 interrupt in NVIC
	NVIC_EnableIRQ(USART3_IRQn);				// Enable USART3 interrupt in NVIC
	NVIC_EnableIRQ(SysTick_IRQn);				// Enable SysTick interrupt in NVIC

	__enable_irq();						// Enable global interrupts after setting up everything

	UART_ENABLE;		// Enable UART after setting up interrupts.
	TIM6_ENABLE;		// Enable TIM6 after setting up interrupts. Will tick every 4 seconds.
	
	FAN_SET(FAN_INITIAL_STATE);
	LIGHT_SET(LIGHT_INITAL_STATE);
	
  while (1)
  {
		/*
			Below code is for UART transmitting the updates to the PC
			It checks if the queue is not empty (emptyPos > 0).
			Then tries twice to transmit the whole queue to the UART. However, if it succeeds the first it does not try again.
		*/
		if (uart_tx.emptyPos > 0)
		{
			volatile uint8_t try_count = 2;
			// Try to transmit the uart_tx queue. If transmission fails (due to timeout) try twice.
			while (uart_tx.t_success == 0 && try_count > 0)
			{
				UART_Transmit(&uart_tx);
				try_count --;
			}
			uart_tx.t_success = 0;		// Reset transmission success for next transmission
		}
		
		
		/*
			Below code is reminding the program to generate the packet for communicating the HMS status to the PC.
			The SEND_UPDATE_TO_PC flag is toggled to 1 by TIM6's update event interrupt.
			It will create the packet from the Outputs struct. Therefore it is **important** that when output states are changed, the outputs struct is updated.
			The packet is send the UART_TX's queue. Which will be transmitted in the section above.
		*/
		if ((COMM_FLAG & SEND_UPDATE_TO_PC_Msk) == 1)
		{
			COMM_FLAG &= ~(SEND_UPDATE_TO_PC_Msk);										// Reset flag for next 0.25Hz tick
			Prepare_Msg_To_PC(&outputs, &sensors, &uart_tx);
		}
		
		
		
		/* 
			Below will read the potentiometer which simulates the temperature sensor.
			It will update the ADC_Value and Value components of the Sensors->Temperature var.
		*/
		Read_Potentiometer(&(sensors.temperature.ADC_Value),&(sensors.temperature.Value));
		
		HMS_Poll_Light_Switch(&outputs.Light);
		
		/*
			Below contains the logic for processing the control commands send by PC.
			As a control command will have control over the HMS for 10 seconds, the program will not process any new commands recieved in that period.
			However, data recieved is saved to the buffer. Which circles around itself.
			However when the 10 seconds is over, the program will process the first valid command in its buffer.

			UART CONTROL is allowed for The temperature range 15 < x < 30.
			
			If within the temp range and the commands are valid the code will raise UART CONTROL MODE flag and outputs will contain the target states
			After the code, the program will explicitly commit the outputs to the hardware.
		*/
		
		if ((COMM_FLAG & RECIEVE_CMD_FROM_PC_Msk) != 0 && (CONTROL_MODE_FLAG & CONTROL_UART_MODE_Msk) == 0)
		{
			if (sensors.temperature.Value > 15 && sensors.temperature.Value < 30)
			{
				COMM_FLAG &= ~(RECIEVE_CMD_FROM_PC_Msk);												// Reset the CMD recieved flag as below the buffer is processed.
				if (Process_PC_CMD(&uart_rx, &outputs) != 0)										// If CMD was validated and the desired outputs extracted. Then turn on the timer.
				{
					HMS_UART_Set_Light(&outputs.Light, outputs.Light);
					TIM7_Disable;	// Disable
					TIM7->CNT = 0;	// Reset count. 				
					TIM7_RUNNING_FLAG = (1 << TIMER_UART_CTRL_Pos);								// Indicate that TIM7 is running for the UART CONTROL MODE
					TIM7_ENABLE;																// Start TIM7
					CONTROL_MODE_FLAG = (1 << CONTROL_UART_MODE_Pos);							// Indicate HMS is in UART_CTRL_MODE. Turns off AUTO_MODE
				}
			}
		}
		
		
		if (CONTROL_MODE_FLAG & CONTROL_AUTO_MODE_Msk)											// If in AUTO MODE 
		{
			// The fan is on default unless the fan off timer is on.
			if ((TIM7_RUNNING_FLAG & TIMER_FAN_OFF_Msk) == 0)
			{
				outputs.Fan = 1;
			}
			
			// Check for fan switch.
			if (HMS_Poll_Fan_Switch(&(outputs.Fan), 0) == 1)													// Poll the switch. Told it to not change the hardware
			{
				if (outputs.Fan == 0)																										// If user turned of the fan
				{
					TIM7_RUNNING_FLAG = (1 << TIMER_FAN_OFF_Pos);													// Indicate TIM7 is running for the Fan off period
					TIM7_ENABLE;
				}
				else if (TIM7_RUNNING_FLAG & TIMER_FAN_OFF_Msk)													// If user manually turned fan back on but fan off timer is running
				{
					TIM7_Disable;
					TIM7->CNT = 0;	// Reset count.
					TIM7_RUNNING_FLAG &= ~(TIMER_FAN_OFF_Msk);														// Clear the flag
				}
			}
			
			AUTO_CONTROL(&sensors, &outputs);
		}
		
		
		// AUTO and UART Control mode only set the output status in Outputs struct.
		// Below the func will apply the outputs to hardware. Its a single source of hardware change for temperature control
		Hardware_Temperature_Control(&outputs);
  }
}



void USART3_IRQHandler(void)
{
	if (USART3->SR & USART_SR_RXNE_Msk)						// Recieved data is ready to be read.
	{
		// Read the byte into the buffer. The buffer is circular. The read of DR will clear the RXNE flag.
		uart_rx.buffer[(uart_rx.emptyPos++) % UART_BUFFER_SIZE] = (uint8_t)(USART3->DR & 0xFF);		
		uart_rx.state = 1;
	}
	if (USART3->SR & USART_SR_IDLE_Msk)
	{
		// In case it causes issues
		// https://www.reddit.com/r/C_Programming/comments/1aizjhj/should_you_cast_functions_where_you_dont_use_the/
		// Reset the IDLE Flag by read SR and then DR.
		(void)USART3->SR;																	
		(void)USART3->DR;
		if (uart_rx.state == 1)											// If previously recieving then change to finished recieving
		{
			uart_rx.state = 2;
			COMM_FLAG |= RECIEVE_CMD_FROM_PC_Msk;
		}
		else
		{
			uart_rx.state = 3;												// 3 indicates error.
		}
	}
}


//=====================================FUNCTION TIM7 IRQHANDLER START=====================================//
// Function: TIM7_IRQHandler
// Description: TIM7 Event update interrupt.
// 							TIM7 will provide a 10 second OPM.
// Input: None
// Output: None
void TIM7_IRQHandler(void)
{
	if ((TIM7->SR & TIM_SR_UIF_Msk) != 0)
	{
		TIM7->SR &= ~(TIM_SR_UIF_Msk);														// Clear the Update Event Interrupt flag. Required to allow Timer to continue
		if (TIM7_RUNNING_FLAG & TIMER_FAN_OFF_Msk)								// If timer was running for the fan off period then turn on the fan
		{
			outputs.Fan = 1;
		}
		TIM7_RUNNING_FLAG &= 0x00;																// Clear the flags.
		if (CONTROL_MODE_FLAG & CONTROL_UART_MODE_Msk)
		{
			CONTROL_MODE_FLAG = (CONTROL_AUTO_MODE_Msk);						// Return to AUTO MODE
		}
	}
}
//=====================================FUNCTION TIM7 IRQHANDLER END=====================================//


//=====================================FUNCTION TIM6 DAC IRQHANDLER START=====================================//
// Function: TIM6_DAC_IRQHandler
// Description: TIM6 Event Update Interrupt. Will cause an update to PC.
// Input: None
// Output: None
void TIM6_DAC_IRQHandler(void)
{
	if ((TIM6->SR & TIM_SR_UIF_Msk) != 0)
	{
		TIM6->SR &= ~(TIM_SR_UIF_Msk);								// Clear update interrupt flag. Needed so timer can continue.
		COMM_FLAG |= SEND_UPDATE_TO_PC_Msk;
	}
}
//=====================================FUNCTION TIM6 DAC IRQHANDLER END=====================================//


// Should be called before interrupt are enabled to prevent issues with EGR_UG causing unintended behaviour
void TIM6_Setup(void)
{
	// Enable TIMER 6 on APB1 Bus.
	RCC->APB1ENR |= (RCC_APB1ENR_TIM6EN);
	
	// Reset TIMER 6 on APB1 Bus.
	RCC->APB1RSTR |= (RCC_APB1RSTR_TIM6RST);
	// Two cycle delay
	__asm("nop"); __asm("nop");
	
	// Turn off reset of TIMER 6 on APB1 Bus
	RCC->APB1RSTR &= ~(RCC_APB1RSTR_TIM6RST);
	// Two cycle delay
	__asm("nop"); __asm("nop");
	
	
	// Configure TIMER 6 registers
	TIM6->CR1 &= ~(TIM_CR1_CEN_Msk);					// Ensure timer 6 is off.
	TIM6->PSC &= ~(TIM_PSC_PSC_Msk);  					// Clear Pre-scaler
	TIM6->PSC |= (TIM6_PRESCALER);						// Set prescaler. Remember final prescaler that divides the source clock is PSC + 1
	TIM6->ARR &= ~(TIM_ARR_ARR_Msk);					// Clear the auto reload register
	TIM6->ARR |= TIM6_ONE_QUARTER_HZ_Count;


	TIM6->DIER |= (0b1 << TIM_DIER_UIE_Pos);			// 0b1 enables update event interrupt.

	// When handling the interrupt, the UIF update interrupt flag must be set to 0 by software.
}


// Should be called before interrupt are enabled to prevent issues with EGR_UG causing unintended behaviour
void TIM7_Setup(void)
{
	// Enable TIMER 7 on APB1 Bus.
	RCC->APB1ENR |= (RCC_APB1ENR_TIM7EN);
	
	// Reset TIMER 7 on APB1 Bus.
	RCC->APB1RSTR |= (RCC_APB1RSTR_TIM7RST);
	// Two cycle delay
	__asm("nop"); __asm("nop");
	
	// Turn off reset of TIMER 7 on APB1 Bus
	RCC->APB1RSTR &= ~(RCC_APB1RSTR_TIM7RST);
	// Two cycle delay
	__asm("nop"); __asm("nop");
	
	
	// Configure TIMER 7 registers
	TIM7->CR1 &= ~(TIM_CR1_CEN_Msk);					// Ensure timer 6 is off.
	TIM7->PSC &= ~(TIM_PSC_PSC_Msk);  					// Clear Pre-scaler
	TIM7->PSC |= (TIM7_PRESCALER);						// Set prescaler. Remember final prescaler that divides the source clock is PSC + 1
	TIM7->ARR &= ~(TIM_ARR_ARR_Msk);					// Clear the auto reload register
	TIM7->ARR |= TIM7_TEN_SECOND_Count;


	// Need to enable the interrupt
	TIM7->DIER |= (0b1 << TIM_DIER_UIE_Pos);			// 0b1 enables update event interrupt.
	TIM7->CR1 |= (TIM_CR1_OPM_Msk);						// Set to one-pulse mode

	// When handling the interrupt, the UIF update interrupt flag must be set to 0 by software.
}

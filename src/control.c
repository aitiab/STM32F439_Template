#include "control.h"


#define COMMANDS_FOUND_SIZE 	(11U)



//=====================================FUNCTION UPDATE TO PC START================================================================//
// Function: Prepare_Msg_To_PC
// Description: Creates a packet. Then asks create_HMS_to_PC_Packet to populate the packet. Then asks UART_Transmit to prepare UART to transmit
// Input: The Outputs and Sensors structs are required by create_HMS_to_PC_Packet. 
// Output: None
void Prepare_Msg_To_PC(volatile Outputs *outputs, volatile Sensors *sensors, volatile UART_TX *uart_tx)
{
	volatile char packet[10] = {0};
	ASCII_Extract(&(sensors->temperature.Value), sensors->temperature.ASCII);
	create_HMS_to_PC_Packet(outputs->Light, outputs->Heater, outputs->Fan, outputs->Cooling, sensors->temperature.ASCII, packet);
	
	UART_Prep(uart_tx, packet, 10);
}
//=====================================FUNCTION UPDATE TO PC END================================================================//


uint8_t Process_PC_CMD(volatile UART_RX *uart_rx, volatile Outputs *outputs)
{
	uint8_t commands[COMMANDS_FOUND_SIZE];
	uint8_t idx = 0;
	if (UART_Read_PC_Command(uart_rx, commands, &idx, COMMANDS_FOUND_SIZE) == 0) { return 0; }		// Get all possible commands bytes in rx queue

	
	uint8_t buffer[5];
	uint8_t c_idx = 0;
	uint8_t valid_found = 0;
	while ( (valid_found = validate_CTRL_Packet(commands[c_idx++], buffer)) == 0 && c_idx < idx);
	
	if (valid_found == 0) { return 0; }
	
	outputs->Light = buffer[0];
	outputs->Heater = buffer[1];
	outputs->Cooling = buffer[2];
	outputs->Fan = buffer[3];
	
	return 1;
}



//=====================================FUNCTION AUTO CONTROL START=====================================//
// Function: AUTO_CONTROL
// Description: Does auto temperature control. It sets the cooling and heater. It also attempts to set fan to default on unless the timer for fan off is on.
// Input: None
// Output: None
void AUTO_CONTROL(volatile Sensors *sensors, volatile Outputs *outputs)
{
	//---------------------------------------------------------------------------//
	if (sensors->temperature.Value < 22.0)						// If temperature is less than 22 degrees, then heater is on
	{
		// turn on heater and turn cooling off
		outputs->Cooling = 0;
		outputs->Heater = 1;
	}
	else if(sensors->temperature.Value > 24.0)				// If temperature is greater than 24 degrees
	{
		// then turn off heater and turn cooling on
		outputs->Cooling = 1;
		outputs->Heater = 0;
	}
	else																						// If temperature is within hystersis band then ensure cooling and heater is off
	{
		outputs->Cooling = 0;
		outputs->Heater = 0;
	}
	
	//---------------------------------------------------------------------------//
}
//=====================================FUNCTION AUTO CONTROL END=====================================//



void Configure_Heater_Cooling_GPIO(void)
{
	// Heater Output is on PF8
	// Cooling Output is on PB8
	// Enable RCC Clocks for GPIOF and GPIOB
	RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOFEN_Msk | RCC_AHB1ENR_GPIOBEN_Msk);

	// Reset the GPIO ports
	RCC->AHB1RSTR |= (RCC_AHB1RSTR_GPIOFRST_Msk | RCC_AHB1RSTR_GPIOBRST_Msk);
	__asm("NOP"); __asm("NOP");

	// Clear reset
	RCC->AHB1RSTR &= ~(RCC_AHB1RSTR_GPIOFRST_Msk | RCC_AHB1RSTR_GPIOBRST_Msk);
	__asm("NOP"); __asm("NOP");

	// Configure GPIOF8 and GPIOB8
	GPIOF->MODER &= ~(GPIO_MODER_MODER8_Msk);
	GPIOF->MODER |= (0b01 << GPIO_MODER_MODER8_Pos); 		// 0b01 = Output
	GPIOF->OTYPER &= ~(GPIO_OTYPER_OT8);					// 0b0 = Push-pull. (GND to VDD)
	GPIOF->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED8_Msk);			// 0b00 = Low speed which is fine for LEDs
	GPIOF->PUPDR &= ~(GPIO_PUPDR_PUPD8_Msk); 				// Not needed.
	
	GPIOB->MODER &= ~(GPIO_MODER_MODER8_Msk);
	GPIOB->MODER |= (0b01 << GPIO_MODER_MODER8_Pos); 		// 0b01 = Output
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT8);					// 0b0 = Push-pull. (GND to VDD)
	GPIOB->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED8_Msk);			// 0b00 = Low speed which is fine for LEDs
	GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD8_Msk); 				// Not needed.

	GPIOF->ODR |= (!(HEATER_INITAL_STATE) << GPIO_ODR_OD8_Pos);		    // Outputs are active low. If state = 0 then set ODR to 1 to turn off.
	GPIOB->ODR |= (!(COOLING_INITAL_STATE) << GPIO_ODR_OD8_Pos);		// Outputs are active low. If state = 0 then set ODR to 1 to turn off.
}

void Hardware_Temperature_Control(volatile Outputs *outputs)
{
	if (outputs->Fan == 1)
	{
		FAN_ON();									// Use switches' macro. Avoid making multiple source of truth
	}
	else
	{
		FAN_OFF();								    // Use switches' macro. Avoid making multiple source of truth
	}

	if (outputs->Cooling == 1)
	{
		// Turn on cooling
		GPIOB->ODR &= ~(GPIO_ODR_OD8_Msk);			// Active low output. Set to 0 to turn on.
	}
	else
	{
		// Turn off cooling
		GPIOB->ODR |= GPIO_ODR_OD8_Msk;				// Active low output. Set to 1 to turn off.
	}

	if (outputs->Heater == 1)
	{
		// Turn on heater
		GPIOF->ODR &= ~(GPIO_ODR_OD8_Msk);			// Active low output. Set to 0 to turn on.
	}
	else
	{
		// Turn off heater
		GPIOF->ODR |= GPIO_ODR_OD8_Msk;				// Active low output. Set to 1 to turn off.
	}
}
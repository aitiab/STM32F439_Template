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


void Hardware_Temperature_Control(volatile Outputs *outputs)
{
	FAN_SET(outputs->Fan);							// Keep one source of truth for the hardware state of the fan. The func updates the internal state in switches.c
	LIGHT_SET(outputs->Light);						// Keep one source of truth for the hardware state of the light. The func updates the internal state in switches.c
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

//==============================================================================================================//
/*
	
	Light Switch -> PA10 -> Digital Input
	Light Control Output -> PA9 -> Digital Output
	Light Intensity Sensor -> PA8 -> Digital Input

	USART3 RX -> PB11 -> I/O Alternate Function
	USART3 TX -> PB10 -> I/O Alternate Function
	Cooling Output -> PB8 -> Digital Output
	Fan Control Output -> PB1 -> Digital Output
	Fan Switch -> PB0 -> Digital Input

	Heater Output -> PF8 -> Digital Output
	Temperature Sensor -> PF10 -> Analogue Input

	GPIO A, B, F are used.
*/
void configureRCC(void)
{
	RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOAEN_Msk | RCC_AHB1ENR_GPIOBEN_Msk | RCC_AHB1ENR_GPIOFEN_Msk);

	// Reset the GPIO ports
	RCC->AHB1RSTR |= (RCC_AHB1RSTR_GPIOARST_Msk | RCC_AHB1RSTR_GPIOBRST_Msk | RCC_AHB1RSTR_GPIOFRST_Msk);
	__asm("NOP"); __asm("NOP");

	// Clear reset
	RCC->AHB1RSTR &= ~(RCC_AHB1RSTR_GPIOARST_Msk | RCC_AHB1RSTR_GPIOBRST_Msk | RCC_AHB1RSTR_GPIOFRST_Msk);
	__asm("NOP"); __asm("NOP");
}


// ============================================================
// PA8  input  (SW2 lux sensor)
// PA9  output (LED2 light)
// PA10 input  (SW4 light switch)
// PB0  input  (SW5 fan switch)
// PB1  output (LED5 fan)
// ============================================================
void configureGPIO(void)
{
    // GPIOA
	// Pin 8 and 10 = input. Pin 9 = output.		0b00 = input, 0b01 = output.
    GPIOA->MODER &= ~(GPIO_MODER_MODER10_Msk | GPIO_MODER_MODER9_Msk | GPIO_MODER_MODER8_Msk);	 // Clear
    GPIOA->MODER |=  (0x01 << GPIO_MODER_MODE9_Pos);  // PA9 output. 0b01 = output

	// Only PA9 is output. Set to push-pull (GND to VDD) (0b0). For PA8 and PA10 it does not matter as they are inputs
    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT10_Msk | GPIO_OTYPER_OT9_Msk | GPIO_OTYPER_OT8_Msk );		

	// Only PA9 is output. Set to low speed (0b00). For PA8 and PA10 it does not matter as they are inputs
    GPIOA->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED10_Msk | GPIO_OSPEEDR_OSPEED9_Msk | GPIO_OSPEEDR_OSPEED8_Msk);

    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD10_Msk | GPIO_PUPDR_PUPD9_Msk  | GPIO_PUPDR_PUPD8_Msk);
    GPIOA->PUPDR |=  (0x01 << GPIO_PUPDR_PUPD10_Pos) |  // PA10 pull-up
                     (0x01 << GPIO_PUPDR_PUPD8_Pos);     // PA8  pull-up

    // GPIOB
	// Pin 0 = input. Pin 1, 8 = digital output. Pin 10 and 11 = Alternate Function.
    GPIOB->MODER &= ~(GPIO_MODER_MODER11_Msk | GPIO_MODER_MODER10_Msk | GPIO_MODER_MODER8_Msk |
					 GPIO_MODER_MODER1_Msk | GPIO_MODER_MODER0_Msk);
		 
    GPIOB->MODER |=  (0x01 << GPIO_MODER_MODE1_Pos) | (0x01 << GPIO_MODER_MODE8_Pos);    // PB1 and PB8 output. 0b01 = output

    GPIOB->OTYPER &= ~(GPIO_OTYPER_OT0_Msk | GPIO_OTYPER_OT1_Msk | GPIO_OTYPER_OT8_Msk);		// 0b0 = push-pull ((GND to VDD)) for outputs. Doesnt matter for inputs

    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED0_Msk | GPIO_OSPEEDR_OSPEED1_Msk | GPIO_OSPEEDR_OSPEED8_Msk);	// 0b00 = low speed for outputs. Doesnt matter for inputs
	
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD8_Msk | GPIO_PUPDR_PUPD1_Msk | GPIO_PUPDR_PUPD0_Msk);
    GPIOB->PUPDR |=  (0x01 << GPIO_PUPDR_PUPD0_Pos);    // PB0 pull-up

	// GPIOF
	// Pin 8 = output.
	GPIOF->MODER &= ~(GPIO_MODER_MODER8_Msk);
	GPIOF->MODER |=  (0x01 << GPIO_MODER_MODE8_Pos);    // PF8 output. 0b01 = output
	GPIOF->OTYPER &= ~(GPIO_OTYPER_OT8_Msk);			// 0b0 = push-pull ((GND to VDD)) for outputs. 
	GPIOF->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED8_Msk);	// 0b00 = low speed for outputs. 
	GPIOF->PUPDR &= ~(GPIO_PUPDR_PUPD8_Msk);			// 0b00 = no pull-up or pull-down for outputs.


	GPIOF->ODR |= (!(HEATER_INITAL_STATE) << GPIO_ODR_OD8_Pos);		    // Outputs are active low. If state = 0 then set ODR to 1 to turn off.
	GPIOB->ODR |= (!(COOLING_INITAL_STATE) << GPIO_ODR_OD8_Pos);		// Outputs are active low. If state = 0 then set ODR to 1 to turn off.
}

//==============================================================================================================//
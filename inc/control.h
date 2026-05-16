#ifndef CONTROL_H
#define CONTROL_H


#include <stdint.h>
#include "boardSupport.h"
#include "stm32f439xx.h"
#include "uart.h"
#include "comms.h"
#include "adc.h"

//================================ FLAGS DEFINES START ================================//
// Below are defines useful for setting the CONTROL MODE
// CONTROL MODE Flag is for determining if HMS is auto or UART control mode

#define CONTROL_AUTO_MODE_Pos (0U)
#define CONTROL_AUTO_MODE_Msk (1 << CONTROL_AUTO_MODE_Pos)			// 0b0001

#define CONTROL_UART_MODE_Pos (1U)
#define CONTROL_UART_MODE_Msk (1 << CONTROL_UART_MODE_Pos)			// 0b0010

//================================ FLAGS PACKET DEFINES END ================================//


// Another struct just for temperature. Keeps the other structs more meaningful.
typedef struct {
	uint16_t ADC_Value; 			// The ADC reading of the temperature sensor (Potentiometer)
	float Value;					// Temperature reading, continuous value.
	char ASCII[7];				// The 6 ASCII character version of the temperature
} Temperature;


typedef struct {
	uint8_t Heater; 	// LED outputs therefore either 1 or 0
	uint8_t Cooling;	// LED outputs therefore either 1 or 0
	uint8_t Fan;			// LED outputs therefore either 1 or 0
	uint8_t Light;		// LED outputs therefore either 1 or 0
	
} Outputs;

typedef struct {
	Temperature temperature;
	uint8_t Light_Intensity; 		// Simulated intensity, digital, therefore either 1 or 0.
	
} Sensors;

typedef struct {
	uint8_t Fan;		// Switch state therefore either 1 or 0
	uint8_t Light;	// Switch state therefore either 1 or 0
	
} Switches;


void Prepare_Msg_To_PC(volatile Outputs *outputs, volatile Sensors *sensors, volatile UART_TX *uart_tx);
uint8_t Process_PC_CMD(volatile UART_RX *uart_rx, volatile Outputs *outputs);
void AUTO_CONTROL(volatile Sensors *sensors, volatile Outputs *outputs);


#endif

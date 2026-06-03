/********************************************
*			Comms Subsystem   			
*			Developed for the STM32								
*			Author: Aitazaz					
*			Source File														
*    		Updated: 03/06/2026 	  							
********************************************/

#include "comms.h"

//=====================================FUNCTION CONVERT ADC TO TEMPERATURE START================================================================//
// Function: validate_CTRL_Packet
// Description: Given a command byte it validates it meets the format requirements. It extracts the different bits for each output
// Input: the byte to analyse, and an array passed as reference to store the extracted bits for each output.
// Output: Returns 1 if valid command byte found. 0 if not valid. The array passed as reference is updated with the extracted bits for each output.
uint8_t validate_CTRL_Packet(volatile uint8_t byte, volatile uint8_t buffer[])
{
	if ((byte & 0b11000011) != INPUT_PACKET_FORMAT)
	{
		return 0;
	}
	
	// Extract the bits for each output
	// reverse the shift. Get the first bit
	buffer[0] = (byte >> LIGHT_Bit_Pos) & 1;
	buffer[1] = (byte >> HEATER_Bit_Pos) & 1;
	buffer[2] = (byte >> COOLING_Bit_Pos) & 1;
	buffer[3] = (byte >> FAN_Bit_Pos) & 1;
	
	return 1;
}


//=====================================FUNCTION CONVERT ADC TO TEMPERATURE START================================================================//
// Function: create_HMS_to_PC_Packet
// Description: Generates the packet for HMS to PC communication. Also known as update to PC packet.
// Input: The pointer to the Outputs struct.
// Output: Updates the packet array passed as reference.
void create_HMS_to_PC_Packet(volatile uint8_t o_light, volatile uint8_t o_heater, volatile uint8_t o_fan,
															volatile uint8_t o_cooling, volatile char t_ASCII[], volatile char *packet) 
{
	// The USART can only transmit/recieve 8-bit per message
	// Need an array to split the HMS to PC communication into 8 bits. 
	// header byte + delimiter char + 6 ASCII temperature char + flags (8-bits) = 9 element char array
	packet[0] = '&';
	packet[1] = '~';
	
	// 6 ASCII characters
	packet[2] = t_ASCII[0];
	packet[3]	= t_ASCII[1];
	packet[4]	= t_ASCII[2];
	packet[5] = t_ASCII[3];
	packet[6] = t_ASCII[4];
	packet[7] = t_ASCII[5];
	
	packet[8] = '~';
	
	// Set the last byte of the packet. It holds the status for LIGHT, HEATER, COOLING and FAN.
	// The byte has format: 0b01abcd01 where a, b, c and d are LIGHT, HEATER, COOLING and FAN respectively.
	packet[9] = OUTPUT_PACKET_STATUS_FORMAT | (o_light << LIGHT_Bit_Pos) | (o_heater << HEATER_Bit_Pos);
	packet[9] |= ((o_cooling << COOLING_Bit_Pos) | (o_fan << FAN_Bit_Pos));

	packet[10] = '\r';		// Carriage return
	packet[11] = '\n';		// New line
}
//=====================================FUNCTION CONVERT ADC TO TEMPERATURE START================================================================//

//=====================================FUNCTION ASCII_EXTRACT START================================================================//
// Function: ASCII_Extract
// Description: Parse a provided float value to convert to 6 ASCII characters. e.g. Input: -23.223123, Output = "-23.23"
// Inputs: The sensors struct which contains the struct for temperature. Which includes ADC, Value and ASCII
// Output: No output but does modify the argument temp_ASCII. Which decays to a pointer.
void ASCII_Extract(volatile float *t_Val, volatile char t_ASCII[])
{
	// Convert the signed temperature value to 6 ASCII characters (2 decimal places)
	// <sign><2 digits>.<2 digits>
	// Return the 6 ASCII characters
	
	// https://www.freecodecamp.org/news/c-ternary-operator/
	// temperature_ASCII[0] = (temperature_Value < 0) ? '-' : '+';
	
	volatile float temporary = 0;
	
	if (*t_Val < 0)
	{
		temporary = (*t_Val) * -1;													// Convert to positive otherwise below errors.
		t_ASCII[0] = '-';
	}
	else
	{
		temporary = (*t_Val);
		t_ASCII[0] = '+';
	}
	
	uint8_t twoDigits = (uint8_t)temporary;			// Get the integer part by type casting.
	
	t_ASCII[1] = ((twoDigits / 10) + '0');		// 23 / 10 = 2 (the first digit extracted)
	t_ASCII[2] = ((twoDigits % 10) + '0');		// 23 % 10 = 3 (the second digit extracted)
	
	float temp = temporary - twoDigits;					// Set integer part to 0 to allow the fractional part to be moved up 
	uint8_t twoLastDigits = (uint8_t)(temp * 100);			// Move two fractional digits to the integer position
	
	t_ASCII[3] = '.' ;	
	t_ASCII[4] = ((twoLastDigits / 10) + '0');	
	t_ASCII[5] = ((twoLastDigits % 10) + '0');

	t_ASCII[6] = '\0'; 												// Null terminator
}
//=====================================FUNCTION ASCII_EXTRACT END================================================================//
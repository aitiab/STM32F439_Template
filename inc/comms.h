#ifndef COMMS_H
#define COMMS_H


#include <stdint.h>
#include "boardSupport.h"
#include "stm32f439xx.h"


//================================ COMMUNICATION PROTOCOL PACKET DEFINES START ================================//
/* 
	In the packet structure for comms between HMS and PC. The byte for sending status of outputs 
	has the format 0b01ab cd01 where a, b, c and d are LIGHT, HEATER, COOLING and FAN respectively

	The format for receiving commands has format 0b01abcd00 where a, b, c and d are LIGHT, HEATER, COOLING and FAN respectively
*/
#define OUTPUT_PACKET_STATUS_FORMAT 		(0b01000001)
#define INPUT_PACKET_FORMAT 				(0b01000000)

// Useful for creating the communication protocol bytes
#define LIGHT_Bit_Pos (5U)
#define LIGHT_Bit_Msk (1 << LIGHT_Bit_Pos)			// 0b0000 0000
#define HEATER_Bit_Pos (4U)
#define HEATER_Bit_Msk (1 << HEATER_Bit_Pos)		// 0b0010 0000
#define COOLING_Bit_Pos (3U)
#define COOLING_Bit_Msk (1 << COOLING_Bit_Pos)		// 0b0001 0000
#define FAN_Bit_Pos (2U)
#define FAN_Bit_Msk (1 << FAN_Bit_Pos)						// 0b0000 1000
//================================ COMMUNICATION PROTOCOL PACKET DEFINES END ================================//

#define SEND_UPDATE_TO_PC_Pos (0U)
#define	SEND_UPDATE_TO_PC_Msk (1 << SEND_UPDATE_TO_PC_Pos)						// 0b0001

#define RECIEVE_CMD_FROM_PC_Pos (1U)
#define RECIEVE_CMD_FROM_PC_Msk (1 << RECIEVE_CMD_FROM_PC_Pos)		// 0b0010




void ASCII_Extract(volatile float *t_Val, volatile char t_ASCII[]);

void create_HMS_to_PC_Packet(volatile uint8_t o_light, volatile uint8_t o_heater, volatile uint8_t o_fan,
															volatile uint8_t o_cooling, volatile char temperatASCII[], volatile char *packet);


uint8_t validate_CTRL_Packet(volatile uint8_t byte, volatile uint8_t buffer[]);

#endif

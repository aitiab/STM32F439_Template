#ifndef UART_H
#define UART_H


#include <stdint.h>
#include "boardSupport.h"
#include "stm32f439xx.h"


#define UART_BUFFER_SIZE (20U)
#define UART_QUEUE_SIZE (20U)

// Enables the USART, Transmitter and reciever.
#define UART_ENABLE (USART3->CR1 |= (USART_CR1_UE_Msk | USART_CR1_TE_Msk | USART_CR1_RE));


typedef struct {
	uint8_t queue [UART_QUEUE_SIZE];
	uint8_t curPos;						// Index of next item in queue
	uint8_t emptyPos;					// Index of next empty place in queue
	uint8_t t_success;				// Bool to indicate if UART successfully transmitted all content from queue
} UART_TX;

typedef struct {
	uint8_t	buffer [UART_BUFFER_SIZE];
	uint8_t curPos;
	uint8_t emptyPos;
	uint8_t state;						// 1 = recieving, 2 = finished, 3 = something went wrong. rx in idle despite no recieving.
}	UART_RX;

void UART3_Configure(void);
void UART_Prep(volatile UART_TX *uart_tx, volatile char *packet, uint8_t size);
void UART_Transmit(volatile UART_TX *uart_tx);
uint8_t UART_Read_PC_Command(volatile UART_RX *uart_rx, uint8_t commands[], uint8_t *idx, uint8_t size);

#endif

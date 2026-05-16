
#include "uart.h"
uint8_t UART_Read_PC_Command(volatile UART_RX *uart_rx, uint8_t commands[], uint8_t *idx, uint8_t size)
{	
		if ((uart_rx->emptyPos - uart_rx->curPos) < 2)												// Check in case the next two elements do exist
		{
			return 0;
		}
		
		uint8_t header;
		
		volatile uint8_t found = 0;
		volatile uint8_t idx_incre = UART_BUFFER_SIZE;								// The buffer is circular. Therefore to search from curPos back to curPos (in circle) is the size of buffer	
			
		// Search until a header is found. The next element is assumed to be the commandByte
		while (*idx < size && (idx_incre > 0))																											// idx is the index of the commands buffer. Size is the size of the commands buffer
		{
			idx_incre--;																																							// idx_incre is used to track the circular traversal.
			header = uart_rx->buffer[((uart_rx->curPos)++) % UART_BUFFER_SIZE];												// Get current byte. Possibly the header
			if (header == 0x26)																																				// If byte is header
			{
				found = 1;
				commands[(*idx)++] = uart_rx->buffer[((uart_rx->curPos)++) % UART_BUFFER_SIZE];						// Get the commandByte
				idx_incre--;
			}
		}

		
		// Reset the buffer.
		uart_rx->curPos = 0;
		uart_rx->emptyPos = 0;
		uart_rx->state = 0;
		
		if (found == 0)																								// If header is not valid
		{
				return 0;
		}
		
    return 1U;
}



//=====================================FUNCTION UART PREP START================================================================//
// Functoin: UART_Transmit
// Description: Copies the bytes from the packet to the UART transmission buffer. If the UART is not transmitting bytes the queue (has finished), it begins the transmitting.
// Input: A packet array containing bytes to transfer to the UART. And also its size.
// Output: None.
void UART_Prep(volatile UART_TX *uart_tx, volatile char *packet, uint8_t size)
{
	for (uint8_t i = 0; i < size; i++)
	{
		uart_tx->queue[uart_tx->emptyPos] = packet[i];
		uart_tx->emptyPos += 1;													// This means points to next empty place in queue.
	}
	
	// In case the UART had transferred all bytes from the queue and sat idle (transmitting was 0)
	//if (uart_tx->transmitting == 0)
	//{
		
	//	USART3->DR = uart_tx->queue[uart_tx->curPos];					// Place in data register.
	//	uart_tx->curPos += 1;																// Increment to next item in queue.
	//	uart_tx->transmitting = 1;														// Indicate the uart_tx is transmiting bytes from the buffer
		// USART3->CR1 |= USART_CR1_TXEIE;											// Turn on TXE Interrupt. TXE Interrupt take care of transmitting the rest of the queue elements.
	// }
	
}
//=====================================FUNCTION UART PREP END================================================================//

//=====================================FUNCTION UART TRANSMIT START================================================================//
void UART_Transmit(volatile UART_TX *uart_tx)
{
	// If TXE Interrupt was triggered. Indicates the data inside the data register was transferred ot the shift register
	// Place the next byte into data register
	uart_tx->t_success = 0; 																			// Default is 0. But changes to 1 if transmission is successful
	
	// (9 bits / 57600 bps) = time to transmit char.    char time * 168MHz = cycles to transmit char = 26250 * 1.5 = 39375 
	volatile uint16_t time_out = 39375;
	// Save curPos at start of transmission in case transmission fails. Then queue can be attempted to be transmit from og position again
	const uint8_t save_curPos = uart_tx->curPos;									
	
	while (uart_tx->curPos < uart_tx->emptyPos && time_out > 0)		// Keep transferring from queue until an empty element is reached. or timeout had previously expired.
	{
		while((USART3->SR & USART_SR_TXE_Msk) == 0x00 && time_out > 0) { time_out--; }
		
		if (time_out > 0)
		{
			USART3->DR = (uint8_t)uart_tx->queue[uart_tx->curPos];		// Place current element from queue into data register
			// uart_tx->transmitting = 1;															// Indicate the uart_tx is transmitting bytes from the buffer
				
			uart_tx->curPos += 1;																			// Increment to the next item in queue
			if (uart_tx->curPos >= uart_tx->emptyPos)									// if the position of current element in queue is greater than the end of queue. Then end of queue has been reached. Reset.
			{
				uart_tx->curPos = 0;
				uart_tx->emptyPos = 0;
			}
		}
		
		time_out = 39375; 																					// Reset time_out
		
		while ((USART3->SR & USART_SR_TC) == 0x00 && time_out > 0) { time_out--; }
	}
	
	
	// If timeout had expired then reset the curPos to original position so the queue can be attempted to be transmitted again
	if (time_out <= 0)
	{
		uart_tx->curPos = save_curPos;
	}
	else {	uart_tx->t_success = 1; }															// Indicate last transmission was not successful
	
}
//=====================================FUNCTION UART TRANSMIT END================================================================//


//-------------------------UART3_CONFIGURE FUNCTION START-------------------------//
void UART3_Configure(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_USART3EN; // Enable USART3 clock
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;	// Enable GPIOB clock
	
	RCC->APB1RSTR |= RCC_APB1RSTR_USART3RST;
	RCC->AHB1RSTR |= RCC_AHB1RSTR_GPIOBRST;
	__asm("nop"); __asm("nop");	// Two cycle delay
	
	RCC->APB1RSTR &= ~(RCC_APB1RSTR_USART3RST);
	RCC->AHB1RSTR &= ~(RCC_AHB1RSTR_GPIOBRST);
	__asm("nop"); __asm("nop");
	
	// Assume that GPIOB is enabled
	// Configure PB10, PB11 for Alternate functions.
	GPIOB->MODER &= ~(GPIO_MODER_MODE11_Msk | GPIO_MODER_MODE10_Msk);	// Clear the MODE for PB11 and PB10
	GPIOB->MODER |= ((0b10 << GPIO_MODER_MODE11_Pos) | (0b10 << GPIO_MODER_MODE10_Pos));
	
	// Setup the alternate function - AF7
	// AFR[1] = AFRH
	GPIOB->AFR[1] &= ~(GPIO_AFRH_AFSEL10_Msk | GPIO_AFRH_AFSEL11_Msk);
	GPIOB->AFR[1] |= ((0x07 << GPIO_AFRH_AFSEL11_Pos) | (0x07 << GPIO_AFRH_AFSEL10_Pos));
	
	// Turn on OVER16 (16 time over sampling)
	USART3->CR1 &= ~(USART_CR1_OVER8_Msk); // Set to 0b0 for oversampling by 16
	
	USART3->CR1 |= (USART_CR1_M_Msk); // Set to 0b1 for 1 Start bit, 9 data bits, and n Stop bits
	USART3->CR1 |= (USART_CR1_PCE_Msk); // Set to 0b1 to enable Parity Control.
	
	USART3->CR1 |= (1 << USART_CR1_PS_Pos); // Set to 0b1 for odd parity
	
	USART3->CR2 &= ~(USART_CR2_STOP_Msk); // Set to 0b00 for 1 Stop bit.
	USART3->CR2 &= ~(USART_CR2_CLKEN_Msk | USART_CR2_CPOL_Msk | USART_CR2_CPHA_Msk); // Set CLKEN, CPOL and CPHA to 0b00 to set to async mode of op.
	
	USART3->CR3 &= ~(USART_CR3_CTSE | USART_CR3_RTSE); // Disable hardware flow control
	
	USART3->BRR &= ~(USART_BRR_DIV_Mantissa_Msk | USART_BRR_DIV_Fraction_Msk); // Clear the Baud Rate register first
	USART3->BRR |= ((45U << USART_BRR_DIV_Mantissa_Pos) | (9U << USART_BRR_DIV_Fraction_Pos)); // USARTDIV = 42MHz / (16 * BAUDRATE) = 45.57291667.     16*0.57291667 = 9.166666667
	
	
	USART3->CR1 |= (0b1 << USART_CR1_RXNEIE_Pos); // Enable RXNE interrupt. Will trigger when ORE=1 or RXNE = 1
	// RXNE is set by hardware when recieved data is ready to be read.
	// RXNE is cleared by a read to the USART_DR register. 

	USART3->CR1 |= (0b1 << USART_CR1_IDLEIE_Pos); // Enable IDLE interrupt. Will trigger when IDLE line is detected.
	// IDLE is set to 1 by hardware when idle line bound. Cleared by a read to SR registerister followed by a read to DR register
}
//-------------------------UART3_CONFIGURE FUNCTION END-------------------------//
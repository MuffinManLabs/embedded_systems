#include "stm32f407xx.h"
#include "usart2.h"

void usart2_init(void)
{
    /* Step 1: Enable clocks for GPIOA and USART2 */
	RCC->AHB1ENR |= (1<<0);

	RCC->APB1ENR |= (1<<17);

    /* Step 2: Configure PA2 and PA3 as alternate function mode */
	GPIOA->MODER |= (1<<5);
	GPIOA->MODER |= (1<<7);

    /* Step 3: Set alternate function type to AF7 (USART2) for PA2 and PA3 */
	GPIOA->AFRL |= (0x7<<8);
	GPIOA->AFRL |= (0x7<<12);

    /* Step 4: Configure USART2: baud rate, word length, stop bits */
	USART2->BRR = 0x008B;

    /* Step 5: Enable USART2 transmitter and the USART2 peripheral */
	USART2->CR1 |= (1<<13);
	USART2->CR1 |= (1<<3);

}


/*
 * Sends a single byte over USART2 TX.
 * Blocks (waits in a loop) until the transmit data register is empty,
 * then writes the byte to the data register for transmission.
 */
void usart2_send_byte(uint8_t byte)
{
    /* Wait until TXE (bit 7) in the status register is set.
     * TXE = 1 means the data register is empty and ready for a new byte.
     * TXE = 0 means the previous byte hasn't been moved to the shift
     * register yet, so we must wait. */
    while (!(USART2->SR & (1 << 7)))
    {
        /* Do nothing — just keep checking */
    }

    /* Write the byte to the data register.
     * The hardware automatically moves it to the shift register
     * and begins transmitting out the TX pin (PA2). */
    USART2->DR = byte;
}

/*
 * Sends a null-terminated string over USART2 TX.
 * Loops through each character and sends it one byte at a time.
 * Stops when it reaches the null terminator '\0'.
 */
void usart2_send_string(const char *str)
{
    while (*str != '\0')
    {
        usart2_send_byte((uint8_t)*str);
        str++;
    }
}
